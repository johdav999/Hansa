#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeLedgerTestSupport.h"
#include "UI/HansaTradeLedger.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "InputCoreTypes.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeLedgerReconciliation,"Hansa.UI.TradeMap.Ledger.ReconciliationAndPrivacy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeLedgerReconciliation::RunTest(const FString&){
 using namespace Hansa::Simulation;using namespace Hansa::UI;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error)){AddError(Error);return false;}
 const auto P=Host->BuildProjection().Value;const auto& Registry=*Host->GetEconomicRegistry();const auto L=BuildTradeLedger(P,Registry,Host->GetHouseId(),TEXT("City.Rostock"));TestTrue(TEXT("Owned inventory available"),L.bAvailable);
 int64 Sum=0,Reserved=0;for(const auto& R:L.Rows){Sum+=R.Physical;Reserved+=R.Reserved;TestEqual(TEXT("Physical reconciles with hard reserve and available"),R.Physical,R.Reserved+R.Available);}
 TestEqual(TEXT("Rows reconcile to capacity used"),Sum,L.Used);TestEqual(TEXT("Rows reconcile to hard reservations"),Reserved,L.Reserved);TestEqual(TEXT("Free space reconciles"),L.Free+L.Used,L.Capacity);
 const auto* Timber=L.Rows.FindByPredicate([](const auto& R){return R.Good==TEXT("Good.Timber");});if(!TestNotNull(TEXT("Timber row"),Timber))return false;
 TestEqual(TEXT("Paused order floor remains protected"),Timber->DesiredReserve,int64(5000));TestEqual(TEXT("Floor does not become a hard reservation"),Timber->Reserved,int64(0));TestTrue(TEXT("Overstock requires acquire target"),Timber->Matches(3));TestTrue(TEXT("Reserved filter includes protected policy"),Timber->Matches(1));TestFalse(TEXT("Unrelated stock is not route cargo"),Timber->Matches(4));TestTrue(TEXT("Order cargo filter"),Timber->Matches(5));
 const auto* Planks=L.Rows.FindByPredicate([](const auto& R){return R.Good==TEXT("Good.Planks");});TestTrue(TEXT("Shortage uses authored order target"),Planks&&Planks->Matches(2));TestTrue(TEXT("Unknown attribution explicit"),Timber->Detail.ToString().Contains(TEXT("no order/route attribution")));
 // The isolated owning projection below uses a real inventory ledger reservation,
 // not an invented order/route allocation. The authoritative host is unchanged.
 FHansaInventoryInitialization ReserveInit;ReserveInit.Id=FHansaInventoryId::TryCreate(100).Value;ReserveInit.OwnerKind=EHansaInventoryOwnerKind::TradeStation;ReserveInit.TradeStationId=FHansaTradeStationId::TryCreate(100).Value;ReserveInit.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;ReserveInit.Capacity=FHansaQuantity::FromRaw(L.Capacity);const auto TimberId=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value;ReserveInit.AcceptedGoods={TimberId};ReserveInit.InitialStock={{TimberId,FHansaQuantity::FromRaw(12000)}};
 auto Inventory=FHansaInventoryLedger::TryCreate({ReserveInit});TestTrue(TEXT("Real ledger reservation succeeds"),Inventory.Value.TryReserve(ReserveInit.Id,FHansaReservationId::TryCreate(900).Value,TimberId,FHansaQuantity::FromRaw(2000),P.GetClock().GetTick(),1).IsSuccess());auto ReservedProjection=P;
 auto* TestInventory=const_cast<FHansaInventoryProjection*>(ReservedProjection.GetInventories().FindByPredicate([&](const auto& I){return I.Id==ReserveInit.Id;}));*TestInventory=Inventory.Value.CreateReadOnlyAccess().QueryInventory(ReserveInit.Id).GetValue();
 const auto WithReservation=BuildTradeLedger(ReservedProjection,Registry,Host->GetHouseId(),L.City);const auto* ReservedRow=WithReservation.Rows.FindByPredicate([](const auto& R){return R.Good==TEXT("Good.Timber");});TestTrue(TEXT("Hard reservations excluded exactly once"),ReservedRow&&ReservedRow->Physical==12000&&ReservedRow->Reserved==2000&&ReservedRow->Available==10000&&ReservedRow->DesiredReserve==5000);
 TestTrue(TEXT("Rival cannot see station rows"),BuildTradeLedger(P,Registry,Host->GetRivalHouseId(),L.City).Rows.IsEmpty());TestTrue(TEXT("Invalid viewer cannot see station rows"),BuildTradeLedger(P,Registry,FHansaHouseId(),L.City).Rows.IsEmpty());
 TArray<uint8> Save;TestTrue(TEXT("Save station ledger"),Host->CaptureSaveBytes(Save,TEXT("TG09"),TEXT("2026-09-23T00:00:00Z")).IsSuccess());TestTrue(TEXT("Reload station ledger"),Host->RestoreSaveBytes(Save).IsSuccess());TestEqual(TEXT("Ledger survives save/reload"),BuildTradeLedger(Host->BuildProjection().Value,Registry,Host->GetHouseId(),L.City).Key(),L.Key());
 FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;if(!Authority.RegisterAdmittedClient({909,FHansaParticipantId::TryCreate(1909).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
 FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(909,0,true,Wire,Error)){AddError(Error);return false;}TestEqual(TEXT("Scoped remote ledger"),Wire.StationLedgers.Num(),1);
 TestEqual(TEXT("Owner receives one station order collection"),Wire.StationOrders.Num(),1);
 TestTrue(TEXT("Owner receives retained order terms"),!Wire.StationOrders[0].Orders.IsEmpty());TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);TestEqual(TEXT("Wire ledger preserves complete data"),Copy.StationLedgers[0].Key(),L.Key());
 TestEqual(TEXT("Wire preserves owner order identity"),Copy.StationOrders[0].Orders[0].Id,Wire.StationOrders[0].Orders[0].Id);
 FHansaClientInterest RivalInterest;
 TestTrue(TEXT("Rival admitted"),Authority.RegisterAdmittedClient({910,FHansaParticipantId::TryCreate(1910).Value,Host->GetRivalHouseId(),EHansaAdmissionMode::LanOffline},RivalInterest,Error));
 FHansaClientProjectionSnapshot RivalWire;
 TestTrue(TEXT("Rival projection builds"),Authority.BuildProjection(910,0,true,RivalWire,Error));
 TestTrue(TEXT("Rival cannot read private station orders"),RivalWire.StationOrders.IsEmpty());
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->ApplyRemoteEstablishment(Copy);M->Open();M->SelectCityIntent(L.City);
 TestTrue(TEXT("Remote order list uses scoped projection"),M->GetSnapshot().StationOrderList.ToString().Contains(TEXT("Timber")));
 auto View=SNew(SHansaTradeMap).Model(M.Get());TestTrue(TEXT("Remote ledger ordinary navigation"),View->ActivateSemanticId(TEXT("TradeMap.Navigate.Ledger")));TestEqual(TEXT("Remote rows have exact stock"),M->GetLedgerPresentation().Used,L.Used);TestTrue(TEXT("Filter action"),View->ActivateSemanticId(TEXT("TradeMap.Ledger.Filter")));TestTrue(TEXT("Select good causal detail"),View->ActivateSemanticId(TEXT("TradeMap.Ledger.Good.Good.Timber")));TestTrue(TEXT("Operations action"),View->ActivateSemanticId(TEXT("TradeMap.Ledger.Operations")));TestTrue(TEXT("Ledger retains global new-route controller access"),View->GetControllerFocusOrder().Contains(TEXT("TradeMap.New")));TestTrue(TEXT("Ledger page down uses its own bounded content"),View->OnKeyDown(FGeometry(),FKeyEvent(EKeys::PageDown,FModifierKeysState(),0,false,0,0)).IsEventHandled());TestTrue(TEXT("Controller can reach ledger back"),View->GetControllerFocusOrder().Contains(TEXT("TradeMap.Ledger.Back")));TestTrue(TEXT("Back retains selected city"),View->ActivateSemanticId(TEXT("TradeMap.Ledger.Back")));TestEqual(TEXT("City retained"),M->GetSnapshot().SelectedCityStableId,L.City);
 FHansaClientCommandIntent Forged;Forged.Type=EHansaClientIntentType::ManageStationOrder;
 Forged.ClientSequence=Authority.GetExpectedClientSequence(910);Forged.ClientNonce=91001;
 Forged.TradeStationId=Wire.StationOrders[0].StationId;Forged.StationOrderId=Wire.StationOrders[0].Orders[0].Id;
 Forged.StationOrderAction=static_cast<uint8>(EHansaStationOrderAction::Cancel);
 TestFalse(TEXT("Rival cannot cancel another house's station order"),Authority.SubmitIntent(910,Forged).bAccepted);
 int64 Nonce=90900;
 M->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Unsequenced){
  FHansaClientCommandIntent Intent=Unsequenced;
  Intent.ClientSequence=Authority.GetExpectedClientSequence(909);Intent.ClientNonce=++Nonce;
  return Authority.SubmitIntent(909,Intent).bAccepted;
 });
 TestTrue(TEXT("Remote orders tab opens"),View->ActivateSemanticId(TEXT("TradeMap.Navigate.Orders")));
 TestTrue(TEXT("Remote order selected with native action"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Select")));
 TestTrue(TEXT("Remote resume reaches server authority"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Pause")));
 FHansaClientProjectionSnapshot Refreshed;
 TestTrue(TEXT("Refresh scoped order view"),Authority.BuildProjection(909,0,true,Refreshed,Error));
 M->ApplyRemoteEstablishment(Refreshed);
 TestFalse(TEXT("Remote result agrees with authoritative resume"),Refreshed.StationOrders[0].Orders[0].bPaused);
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Local(NewObject<UHansaTradeMapPresentationModel>());Local->InitializeDefaults();Local->BindRuntime(Host.Get());Local->ApplyProjection(Host->BuildProjection().Value,Registry);Local->Open();Local->SelectCityIntent(L.City);auto LocalView=SNew(SHansaTradeMap).Model(Local.Get());LocalView->ActivateSemanticId(TEXT("TradeMap.Navigate.Ledger"));LocalView->ActivateSemanticId(TEXT("TradeMap.Ledger.Good.Good.Planks"));TestTrue(TEXT("Related owned route opens through ordinary action"),LocalView->ActivateSemanticId(TEXT("TradeMap.Ledger.Route")));TestEqual(TEXT("Related route selected"),Local->GetSnapshot().SelectedRouteValue,int64(100));Local->SelectCityIntent(L.City);LocalView->ActivateSemanticId(TEXT("TradeMap.Navigate.Ledger"));LocalView->ActivateSemanticId(TEXT("TradeMap.Ledger.Good.Good.Timber"));TestTrue(TEXT("Related order opens through ordinary action"),LocalView->ActivateSemanticId(TEXT("TradeMap.Ledger.Orders")));TestEqual(TEXT("Related order section"),Local->GetSnapshot().ActiveSection,FString(TEXT("Orders")));
 FHansaManageStationOrderCommand Resume;Resume.StationId=FHansaTradeStationId::TryCreate(100).Value;Resume.OrderId=1;Resume.Action=EHansaStationOrderAction::Resume;TestTrue(TEXT("Resume factor via authority"),Host->ManageStationOrder(Resume).IsSuccess());TestTrue(TEXT("Execute factor ticks"),Host->AdvanceTicks(10));const auto Updated=BuildTradeLedger(Host->BuildProjection().Value,Registry,Host->GetHouseId(),L.City);const auto* Result=Updated.Rows.FindByPredicate([](const auto& R){return R.Good==TEXT("Good.Timber");});TestTrue(TEXT("Retained authoritative order result shown"),Result&&!Result->LastResult.ToString().Contains(TEXT("No retained")));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeLedgerRecovery,"Hansa.UI.TradeMap.Ledger.RecoveryStates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeLedgerRecovery::RunTest(const FString&){using namespace Hansa::Simulation;TStrongObjectPtr<UHansaRuntimeSimulationHost> H(NewObject<UHansaRuntimeSimulationHost>());FString E;if(!H->InitializeForLubeck(nullptr,E)){AddError(E);return false;}
 for(auto State:{EHansaTradeStationOperationalState::Underfunded,EHansaTradeStationOperationalState::StorageBlocked,EHansaTradeStationOperationalState::OrderSuspended,EHansaTradeStationOperationalState::RightsSuspended,EHansaTradeStationOperationalState::VoluntarilyClosed,EHansaTradeStationOperationalState::Revoked}){if(!Hansa::Tests::PrepareLedger(*H,E,State)){AddError(E);return false;}const auto L=Hansa::UI::BuildTradeLedger(H->BuildProjection().Value,*H->GetEconomicRegistry(),H->GetHouseId(),TEXT("City.Rostock"));TestEqual(TEXT("Recovery preserves cargo"),L.Used,int64(15000));TestFalse(TEXT("State names a blocker/remedy"),L.Blocker.IsEmpty());TestFalse(TEXT("Asset preservation explicit"),L.Preservation.IsEmpty());if(State==EHansaTradeStationOperationalState::Underfunded)TestTrue(TEXT("Arrears explicit"),L.Upkeep.ToString().Contains(TEXT("25")));}
 auto Missing=*H->GetEconomicRegistry();auto Policies=Missing.GetCityTradePolicies();Policies.RemoveAll([](const auto& P){return P.CityId==TEXT("City.Rostock");});Missing.SetPresenceDefinitions(Missing.GetPresenceCapabilities(),Missing.GetPresenceStages(),Policies);const auto Broken=Hansa::UI::BuildTradeLedger(H->BuildProjection().Value,Missing,H->GetHouseId(),TEXT("City.Rostock"));TestTrue(TEXT("Missing definition preserves visible owned cargo"),Broken.bAvailable&&Broken.Used==15000);TestTrue(TEXT("Missing definition gives migration remedy"),Broken.Blocker.ToString().Contains(TEXT("migration")));
 return !HasAnyErrors();}
#endif
