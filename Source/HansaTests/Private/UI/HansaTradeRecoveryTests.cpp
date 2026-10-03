#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeLedgerTestSupport.h"
#include "HansaTradeSpecializationTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryJourney,"Hansa.UI.TradeMap.Recovery.Journey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryJourney::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error,Hansa::Simulation::EHansaTradeStationOperationalState::Active,true)){AddError(Error);return false;}
  TArray<uint8> InitialSave;Host->CaptureSaveBytes(InitialSave,TEXT("TG16 initial"),TEXT("2026-09-24T00:00:00Z"));
  TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->SetViewerHouse(Host->GetHouseId());if(!Remote)M->BindRuntime(Host.Get());
  FHansaMultiplayerAuthority Authority;FHansaClientCommandIntent Pending;int64 Nonce=16000;bool Delay=false;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;Interest.CityIds.AddUnique(TEXT("City.Rostock"));if(!Authority.RegisterAdmittedClient({916,FHansaParticipantId::TryCreate(1916).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
   M->SetNetworkCommandIntent([&](const auto& I){Pending=I;Pending.ClientSequence=Authority.GetExpectedClientSequence(916);Pending.ClientNonce=++Nonce;FHansaClientCommandFeedback F;F.State=EHansaClientCommandState::Pending;F.ClientSequence=Pending.ClientSequence;F.ClientNonce=Pending.ClientNonce;M->ReceiveCommandFeedback(F);if(!Delay)M->ReceiveCommandFeedback(Authority.SubmitIntent(916,Pending));return true;});
  }
  auto Refresh=[&]{if(Remote){FHansaClientProjectionSnapshot Wire;TestTrue(TEXT("Remote projection"),Authority.BuildProjection(916,0,true,Wire,Error));TArray<uint8> B;FMemoryWriter W(B);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(W,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader R(B);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(R,&Copy,nullptr);TestFalse(TEXT("Recovery serializes"),R.IsError());M->ApplyRemoteEstablishment(Copy);}else M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());};
  auto Hash=[&]{return Host->BuildProjection().Value.GetFingerprint().Value;};
  auto Stock=[&]{return Host->BuildProjection().Value.GetInventories().FindByPredicate([](const auto& I){return I.Id.GetValue()==100;})->UsedCapacity.GetRawValue();};
  Refresh();M->OpenRecovery(TEXT("City.Rostock"),100,TEXT("HUD.AlertStack.Alert.Recovery_100.OpenCause"));auto UI=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
  auto Act=[&](const FString& A){return UI->ActivateSemanticId(TEXT("TradeMap.Recovery.")+A);};
  TestEqual(TEXT("Active state"),M->GetSnapshot().Recovery.Status,FString(TEXT("Active")));TestTrue(TEXT("Dependency route listed"),M->GetSnapshot().Recovery.Items.ContainsByPredicate([](const auto& I){return I.Id==TEXT("Route.100");}));
  TestTrue(TEXT("Private recovery hidden from rival"),Host->BuildTradeRecovery(Host->GetRivalHouseId()).IsEmpty());
  const auto Widget=UI->ResolveSemanticWidget(TEXT("TradeMap.Recovery.Item.Route.100"));Refresh();TestTrue(TEXT("Stable recovery row"),Widget==UI->ResolveSemanticWidget(TEXT("TradeMap.Recovery.Item.Route.100")));
  TestTrue(TEXT("Select order"),Act(TEXT("Item.Order.1")));TestTrue(TEXT("Inspect order"),Act(TEXT("Inspect")));TestEqual(TEXT("Exact order retained"),M->GetSnapshot().SelectedStationOrderId,int64(1));M->SelectSectionIntent(TEXT("Recovery"));
  TestTrue(TEXT("Select dependent route"),Act(TEXT("Item.Route.100")));TestTrue(TEXT("Open recovery route"),Act(TEXT("Inspect")));TestEqual(TEXT("Route identity retained remotely"),M->GetSnapshot().SelectedRouteValue,int64(100));
  TestTrue(TEXT("Edit recovery quantity"),M->AdjustQuantityIntent(1000));TestTrue(TEXT("Remove research-gated reserve for recovery"),M->AdjustMinimumReserveIntent(-1000));const bool EditAccepted=M->CommitIntent();TestTrue(FString(TEXT("Commit through ordinary route intent: "))+M->GetSnapshot().EditorStatus.ToString(),EditAccepted);Refresh();TestEqual(TEXT("Authoritative route edit"),Host->BuildProjection().Value.GetRoutes()[0].Stops[0].Actions[0].QuantityLimit.GetRawValue(),int64(2000));M->SelectSectionIntent(TEXT("Recovery"));
  uint64 Before=Hash();Act(TEXT("Review"));TestEqual(TEXT("Review cannot mutate"),Hash(),Before);TestTrue(TEXT("Preservation dossier"),M->GetSnapshot().Recovery.Terms.Contains(TEXT("PRESERVED")));Act(TEXT("Cancel"));TestFalse(TEXT("No unreviewed close"),Act(TEXT("Confirm")));
  Act(TEXT("Review"));Host->AdvanceTicks(1);Refresh(); // active orders may change dependent stocks, so any changed dossier must invalidate.
  if(!M->GetSnapshot().bRecoveryReview)Act(TEXT("Review"));
  const int64 Cargo=Stock();TestTrue(TEXT("Reviewed closure"),Act(TEXT("Confirm")));Refresh();TestEqual(TEXT("Closure conserves station stock"),Stock(),Cargo);TestTrue(TEXT("Recovery remains outbound"),M->GetSnapshot().Recovery.Status.Contains(TEXT("outbound")));TestFalse(TEXT("Stock blocks finalization"),Act(TEXT("Review")));
  const auto Closed=Host->BuildProjection().Value;TestTrue(TEXT("Order identities retained and paused"),Closed.GetTradeStations()[0].Station.Orders.ContainsByPredicate([](const auto& O){return O.Id==1&&O.bPaused&&!O.bCancelled;}));
  if(Remote){Before=Hash();Authority.SubmitIntent(916,Pending);TestEqual(TEXT("Replay cannot mutate"),Hash(),Before);}
  TArray<uint8> Save;Before=Hash();TestTrue(TEXT("Save interruption"),Host->CaptureSaveBytes(Save,TEXT("TG16"),TEXT("2026-09-24T00:00:00Z")).IsSuccess());TestTrue(TEXT("Restore interruption"),Host->RestoreSaveBytes(Save).IsSuccess());TestEqual(TEXT("Restored conservation"),Hash(),Before);Refresh();
  // Clear order policy floors, then recover both goods via the existing physical route executor.
  for(uint64 Id:{1ull,2ull,3ull}){FHansaManageStationOrderCommand C;C.StationId=FHansaTradeStationId::TryCreate(100).Value;C.OrderId=Id;C.Action=EHansaStationOrderAction::Cancel;TestTrue(TEXT("Cancel preserved order safely"),Host->ManageStationOrder(C).IsSuccess());}
  TArray<FHansaRouteCargoAction> Loads,Unloads;for(const TCHAR* Good:{TEXT("Good.Timber"),TEXT("Good.Planks")}){FHansaRouteCargoAction A;A.Kind=EHansaRouteCargoActionKind::StationLoad;A.GoodId=FHansaGoodId::TryParse(Good).Value;A.QuantityLimit=FHansaQuantity::FromRaw(30000);Loads.Add(A);A.Kind=EHansaRouteCargoActionKind::OwnedCityUnload;Unloads.Add(A);}
  TArray<FHansaRouteStop> Stops={{FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,Loads},{FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value,Unloads}};
  const auto Route=FHansaRouteId::TryCreate(100).Value;
  TestTrue(TEXT("Edit physical recovery route"),Host->EditRoute(Route,Stops).IsSuccess());TestTrue(TEXT("Start outbound recovery"),Host->SetRouteActive(Route,true).IsSuccess());
  for(int32 Tick=0;Tick<80&&Stock()>0;++Tick)TestTrue(TEXT("Recovery tick"),Host->AdvanceTicks(1));Refresh();TestEqual(TEXT("All station stock recovered physically"),Stock(),int64(0));
  TestTrue(TEXT("Empty closure can be reviewed"),Act(TEXT("Review")));TestTrue(TEXT("Finalize through same UI authority"),Act(TEXT("Confirm")));Refresh();TestTrue(TEXT("Closed plot released"),M->GetSnapshot().Recovery.Status.Contains(TEXT("plot released")));TestFalse(TEXT("Cannot finalize twice"),Act(TEXT("Review")));
  Before=Hash();Save.Reset();TestTrue(TEXT("Save finalized state"),Host->CaptureSaveBytes(Save,TEXT("TG16 finalized"),TEXT("2026-09-24T00:00:00Z")).IsSuccess());TestTrue(TEXT("Reload finalized state"),Host->RestoreSaveBytes(Save).IsSuccess());TestEqual(TEXT("Finalized checksum"),Hash(),Before);
  TStrongObjectPtr<UHansaHudPresentationModel> Hud(NewObject<UHansaHudPresentationModel>());auto AlertViews=M->Recoveries;AlertViews[0].bAttention=true;Hud->ApplyRecoveryAlerts(AlertViews);Hud->ApplyRecoveryAlerts(M->Recoveries);TestNotNull(TEXT("Recovery alert exists"),Hud->FindAlert(TEXT("Recovery.100")));
  FName Restored;M->OnFocusRestoreRequested().AddLambda([&](FName Id){Restored=Id;});Act(TEXT("Back"));TestEqual(TEXT("Return exact alert"),Restored,FName(TEXT("HUD.AlertStack.Alert.Recovery_100.OpenCause")));
  if(Remote){ // A concurrent owner operation changes dependency terms between review and arrival.
   if(!Host->RestoreSaveBytes(InitialSave)){AddError(TEXT("Initial save restore failed"));return false;}Refresh();M->OpenRecovery(TEXT("City.Rostock"),100);Act(TEXT("Review"));Delay=true;TestTrue(TEXT("Pending confirmation"),Act(TEXT("Confirm")));TestFalse(TEXT("Pending double submit blocked"),Act(TEXT("Confirm")));
   FHansaManageStationOrderCommand C;C.StationId=FHansaTradeStationId::TryCreate(100).Value;C.OrderId=1;C.Action=EHansaStationOrderAction::Cancel;TestTrue(TEXT("Concurrent order cancellation"),Host->ManageStationOrder(C).IsSuccess());Before=Hash();M->ReceiveCommandFeedback(Authority.SubmitIntent(916,Pending));Refresh();TestEqual(TEXT("Stale closure rolls back"),Hash(),Before);TestFalse(TEXT("Pending cleared"),M->GetSnapshot().bRecoveryPending);TestEqual(TEXT("Stale closure leaves active station"),M->GetSnapshot().Recovery.Status,FString(TEXT("Active")));
  }
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryStateSave,"Hansa.UI.TradeMap.Recovery.InterruptionSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryStateSave::RunTest(const FString&){using namespace Hansa::Simulation;TStrongObjectPtr<UHansaRuntimeSimulationHost> H(NewObject<UHansaRuntimeSimulationHost>());FString E;if(!H->InitializeForLubeck(nullptr,E)){AddError(E);return false;}
 if(!Hansa::Tests::PrepareSpecialization(*H,E,[](auto& Init){auto Extra=Init.LeasedPlots[0];Extra.Id=FHansaLeasedPlotId::TryCreate(2).Value;Extra.BoundsMin={24,8};Extra.BoundsMax={31,19};Init.LeasedPlots.Add(Extra);})){AddError(E);return false;}
 const auto Leases=H->BuildTradeRecovery(H->GetHouseId());TestTrue(TEXT("Every additional station lease inventoried"),Leases.Num()==1&&Leases[0].Items.ContainsByPredicate([](const auto& I){return I.Id==TEXT("Lease.2");}));TestTrue(TEXT("Review explains additional plot retention"),Leases.Num()==1&&Leases[0].Terms.Contains(TEXT("does not release this additional plot")));
 for(auto State:{EHansaTradeStationOperationalState::Active,EHansaTradeStationOperationalState::Underfunded,EHansaTradeStationOperationalState::RightsSuspended,EHansaTradeStationOperationalState::Revoked,EHansaTradeStationOperationalState::VoluntarilyClosed}){
  if(!Hansa::Tests::PrepareLedger(*H,E,State)){AddError(E);return false;}const auto Before=H->BuildTradeRecovery(H->GetHouseId());TArray<uint8> B;const auto Hash=H->BuildProjection().Value.GetFingerprint().Value;TestTrue(TEXT("Save interruption"),H->CaptureSaveBytes(B,TEXT("TG16"),TEXT("2026-09-24T00:00:00Z")).IsSuccess());TestTrue(TEXT("Restore interruption"),H->RestoreSaveBytes(B).IsSuccess());TestEqual(TEXT("State checksum"),H->BuildProjection().Value.GetFingerprint().Value,Hash);TestEqual(TEXT("Recovery dossier stable"),H->BuildTradeRecovery(H->GetHouseId())[0].ReviewKey,Before[0].ReviewKey);
 }
 return !HasAnyErrors();}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStationInspectorRecoveryEntry,"Hansa.UI.TradeMap.Recovery.StationInspectorEntry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStationInspectorRecoveryEntry::RunTest(const FString&){
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error,EHansaTradeStationOperationalState::Underfunded)){AddError(Error);return false;}
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());
 TestTrue(TEXT("Interrupted station opens world inspector"),Model->OpenWorldStation(TEXT("City.Rostock"),100));
 TestTrue(TEXT("Fixture reports upkeep arrears"),Model->GetSnapshot().Establishment.bArrears);
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());
 TestTrue(TEXT("Recovery entry remains in controller order"),Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Station.Action")));
 TestTrue(TEXT("Interrupted inspector opens existing recovery controls"),Screen->ActivateSemanticId(TEXT("TradeMap.Station.Action")));
 TestEqual(TEXT("Recovery opens shared recovery workflow"),Model->GetSnapshot().ActiveSection,FString(TEXT("Recovery")));
 return !HasAnyErrors();
}
#endif
