#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaTradeEstablishmentTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Widgets/Text/STextBlock.h"

using namespace Hansa::Simulation;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMerchantOfficeUpgradeJourney,"Hansa.UI.TradeMap.Presence.AutomaticMerchantOfficeUpgrade",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMerchantOfficeUpgradeJourney::RunTest(const FString&)
{
 using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true})for(bool Import:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
  const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value,Tools=FHansaGoodId::TryParse(TEXT("Good.Tools")).Value;
  const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value,Salt=FHansaGoodId::TryParse(TEXT("Good.Salt")).Value;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,1000000,[&](FHansaSimulationInitialization& Init){
   for(auto& P:Init.ForeignPresences)if(P.HouseId==Host->GetHouseId())P.Contributions={1000000,100,1000000,1000000,1000000,100,100};
   for(auto& I:Init.Inventories)I.AcceptedGoods.AddUnique(Tools);
   Init.Inventories[0].InitialStock={{Timber,FHansaQuantity::FromRaw(8000)},{Planks,FHansaQuantity::FromRaw(Import?4000:16000)}};
   if(!Import)Init.Inventories[0].InitialStock.Add({Tools,FHansaQuantity::FromRaw(2000)});
   auto& Cargo=Init.Inventories[1];Cargo.AcceptedGoods.Append({Grain,Salt});Cargo.InitialStock={{Grain,FHansaQuantity::FromRaw(1000)},{Salt,FHansaQuantity::FromRaw(1000)}};
   auto& Home=Init.Inventories[3];Home.AcceptedGoods={Timber,Planks,Tools,Grain};Home.Capacity=FHansaQuantity::FromRaw(1000000);Home.InitialStock={{Planks,FHansaQuantity::FromRaw(12000)},{Tools,FHansaQuantity::FromRaw(2000)},{Grain,FHansaQuantity::FromRaw(500000)}};
   auto& Destination=Init.Inventories[4];Destination.AcceptedGoods.Add(Grain);Destination.Capacity=FHansaQuantity::FromRaw(1000000);
   FHansaCityMarketInitialization M;M.CityId=City;M.GoodId=Grain;M.InventoryIds={Destination.Id};M.InitialPriceMilliMarks=1000;Init.Markets.Add(M);
   FHansaRouteState Route;const auto& Ship=Init.Vehicles[1];Route.Id=FHansaRouteId::TryCreate(1).Value;Route.OwnerId=Ship.OwnerId;Route.VehicleId=Ship.Id;Route.Mode=EHansaRouteMode::Sea;Route.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;Route.Lifecycle=EHansaRouteLifecycleState::Inactive;
   FHansaRouteCargoAction Load;Load.Kind=EHansaRouteCargoActionKind::OwnedCityLoad;Load.GoodId=Grain;Load.QuantityLimit=FHansaQuantity::FromRaw(30000);Load.CargoSlotIndex=0;auto Unload=Load;Unload.Kind=EHansaRouteCargoActionKind::Unload;
   Route.Stops={{Ship.CurrentCityId,{Load}},{City,{Unload}}};Init.Routes.Add(Route);
  },true)){AddError(Error);return false;}
  FHansaPlacementSpec Spec;Spec.CityId=City;Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.TradeHouse")).Value;
  bool Found=false;for(int32 X=8;X<19&&!Found;++X)for(int32 Y=8;Y<19;++Y){Spec.Anchor={X,Y};if(Host->TradeHousePlacementError(Spec).IsEmpty()){Found=true;break;}}
  if(!Found||!Host->PlaceBuildings({Spec})){AddError(TEXT("Could not place office journey station"));return false;}
  const auto StationId=Host->BuildProjection().Value.GetTradeStations()[0].Station.Id;
  if(!Host->FundTradeStation(StationId,FHansaInventoryId::TryCreate(1).Value)||!Host->AdvanceTicks(4)){AddError(TEXT("Could not build office journey station"));return false;}
  auto Presence=[&](){const auto P=Host->BuildProjection().Value;return *P.GetForeignPresences().FindByPredicate([&](const auto& V){return V.CityId==City&&V.HouseId==Host->GetHouseId();});};
  auto Money=[&](){const auto P=Host->BuildProjection().Value;return P.GetHouses().FindByPredicate([&](const auto& H){return H.Id==Host->GetHouseId();})->Money.GetRawValue();};
  TestEqual(TEXT("Fixture is ready trade station"),Presence().CurrentStageId,FString(TEXT("PresenceStage.TradeStation")));
  const auto Hash=Host->BuildProjection().Value.GetFingerprint().Value;
  TestFalse(TEXT("Rival cannot supply a directly funded office"),Host->FundPresenceUpgrade({City,TEXT("PresenceStage.MerchantOffice"),FHansaInventoryId::TryCreate(3).Value}).IsSuccess());
  TestEqual(TEXT("Rejected order leaves no request or deduction"),Host->BuildProjection().Value.GetFingerprint().Value,Hash);
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->SetViewerHouse(Host->GetHouseId());
  FHansaMultiplayerAuthority Authority;int64 Nonce=0;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;
   if(!Authority.RegisterAdmittedClient({939,FHansaParticipantId::TryCreate(1939).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
   Model->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Draft){auto I=Draft;I.ClientSequence=Authority.GetExpectedClientSequence(939);I.ClientNonce=++Nonce;TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientCommandIntent::StaticStruct()->SerializeItem(Writer,&I,nullptr);FHansaClientCommandIntent Copy;FMemoryReader Reader(Bytes);FHansaClientCommandIntent::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);
    FHansaClientCommandFeedback Pending;Pending.State=EHansaClientCommandState::Pending;Pending.ClientSequence=I.ClientSequence;Pending.ClientNonce=I.ClientNonce;Model->ReceiveCommandFeedback(Pending);const auto Ack=Authority.SubmitIntent(939,Copy);Model->ReceiveCommandFeedback(Ack);return Ack.bAccepted;});
  }else Model->BindRuntime(Host.Get());
  auto Refresh=[&](){if(Remote){FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(939,0,true,Wire,Error)){AddError(Error);return false;}TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);if(Reader.IsError())return false;Model->ApplyRemoteEstablishment(Copy);}else Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());return true;};
  if(!Refresh())return false;Model->OpenWorldStation(TEXT("City.Rostock"),StationId.GetValue());auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());
  TestEqual(TEXT("Upgrade action appears in world station details"),Model->GetSnapshot().PresenceUpgradeAction.ToString(),FString(TEXT("Upgrade to Merchant Office")));
  TestFalse(TEXT("An available upgrade is still a trade station"),Model->GetSnapshot().Establishment.bOfficeBuilt);
  TestEqual(TEXT("Suggest a physically suitable owned source"),Model->GetSnapshot().PresenceSourceId,FString(Import?TEXT("2"):TEXT("1")));
  const auto* SelectedSource=Model->GetSnapshot().PresenceSources.FindByPredicate([&](const auto& C){return C.Id==Model->GetSnapshot().PresenceSourceId;});
  if(TestNotNull(TEXT("Inspector source survives owner wire projection"),SelectedSource)){
   TestEqual(TEXT("Inspector has one row for each upgrade good"),SelectedSource->Materials.Num(),2);
   TestTrue(TEXT("Inspector shows authored cost"),SelectedSource->UpgradeCost.ToString().Contains(TEXT("150")));
  }
  TestEqual(TEXT("Station lease shows its six commercial capabilities"),Model->GetSnapshot().Establishment.Rights.Num(),6);
  const int64 BeforeTerms=Money();TestTrue(TEXT("Open separate native terms view"),Screen->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));
  TestTrue(TEXT("Return terms action enters controller order"),Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Station.Return")));
  TestTrue(TEXT("Return to station before reviewing upgrade"),Screen->ActivateSemanticId(TEXT("TradeMap.Station.Return")));TestEqual(TEXT("Viewing terms spends nothing"),Money(),BeforeTerms);
  TestTrue(TEXT("Upgrade opens its dedicated tab"),Screen->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Upgrade")));
  TestTrue(TEXT("First click opens upgrade review"),Screen->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
  const int64 BeforeReview=Money();Host->AdvanceTicks(1);if(!Refresh())return false;
  TestTrue(TEXT("Normal running ticks preserve the reviewed upgrade"),Model->GetSnapshot().bPresenceReview);
  TestEqual(TEXT("Review never submits a separate request"),Presence().Upgrade.Status,EHansaPresenceUpgradeStatus::None);
  TestTrue(TEXT("Only normal upkeep while reviewing"),BeforeReview-Money()<150000);
  if(Import)TestTrue(TEXT("Review discloses priority before ordinary loads"),Model->GetSnapshot().PresenceReview.ToString().Contains(TEXT("Materials load before ordinary trade goods")));
  const int64 BeforePay=Money();const int64 Upkeep=Host->BuildProjection().Value.GetTradeStations()[0].Station.UpkeepPfennigPerTick;
  TestTrue(TEXT("One confirmation funds and arranges upgrade"),Screen->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));if(!Refresh())return false;
  TestEqual(TEXT("Exactly one upgrade deduction and normal upkeep"),BeforePay-Money(),int64(150000)+Upkeep);
  TestFalse(TEXT("Repeated confirm cannot charge twice"),Screen->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
  TestEqual(TEXT("Station continues trading while upgrading"),Host->BuildProjection().Value.GetTradeStations()[0].Station.Status,EHansaTradeStationStatus::Active);
  TestFalse(TEXT("Paid upgrade is not a completed office"),Model->GetSnapshot().Establishment.bOfficeBuilt);
  if(Import){
   TestEqual(TEXT("Upgrade automatically arranges construction priority"),Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryMode,uint8(2));
   TestTrue(TEXT("Resume the existing route without editing loads"),Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true).IsSuccess());
   for(int32 T=0;T<20&&Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty();++T)Host->AdvanceTicks(1);
   TestFalse(TEXT("Missing goods physically reserved aboard"),Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty());
   TestEqual(TEXT("Cargo pickup cannot start destination construction"),Presence().Upgrade.Status,EHansaPresenceUpgradeStatus::AwaitingMaterials);
  }
  TArray<uint8> Saved;TestTrue(TEXT("Pending upgrade saves with its physical reservations"),Host->CaptureSaveBytes(Saved,TEXT("Automatic office test"),TEXT("2026-10-01T00:00:00Z")).IsSuccess());const auto PendingHash=Host->BuildProjection().Value.GetFingerprint().Value;
  TestTrue(TEXT("Upgrade reloads without another payment"),Host->RestoreSaveBytes(Saved).IsSuccess());TestEqual(TEXT("Reload preserves deterministic state"),Host->BuildProjection().Value.GetFingerprint().Value,PendingHash);
  if(Import){TestTrue(TEXT("Closing station cancels shipment"),Host->CloseTradeStation(StationId).IsSuccess());const auto P=Host->BuildProjection().Value;const auto* Cargo=P.GetInventories().FindByPredicate([](const auto& I){return I.Id.GetValue()==2;});TestTrue(TEXT("Cancellation keeps physical cargo and releases protection"),Cargo&&Cargo->Stocks.ContainsByPredicate([&](const auto& S){return S.GoodId==Planks&&S.Stock.GetRawValue()>0&&S.Reserved.GetRawValue()==0;}));TestTrue(TEXT("Restore pending upgrade after cancellation check"),Host->RestoreSaveBytes(Saved).IsSuccess());}
  TestTrue(TEXT("Run automatic transport and construction"),Host->AdvanceTicks(60));if(!Refresh())return false;
  const auto Complete=Host->BuildProjection().Value;
  TestEqual(TEXT("Merchant Office completes without load/unload edits"),Presence().CurrentStageId,FString(TEXT("PresenceStage.MerchantOffice")));
  TestEqual(TEXT("Completed upgrade clears its delivery policy"),Complete.GetTradeStations()[0].Station.DeliveryMode,uint8(0));TestTrue(TEXT("No lingering reservations"),Complete.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty());
  TestEqual(TEXT("Station keeps its identity"),Complete.GetTradeStations()[0].Station.Id,StationId);
  auto CheckOffice=[&](){
   const auto& E=Model->GetSnapshot().Establishment;
   TestTrue(TEXT("Completed owner capability reaches local and serialized remote details"),E.bOfficeBuilt);
   TestTrue(TEXT("Completed office adds its granted commercial right"),E.Rights.ContainsByPredicate([](const auto& R){return R.Id==TEXT("PresenceCapability.MerchantOffice")&&R.bGranted;}));
   TestFalse(TEXT("Completed office has no pending-upgrade refund"),E.bPendingUpgrade);
   TestEqual(TEXT("Office card names the upgraded building"),E.SiteName.ToString(),FString(TEXT("Merchant Office")));
   TestEqual(TEXT("Office displays live upgraded storage"),E.Storage.ToString(),FString(TEXT("100 units storage")));
   TestEqual(TEXT("Office no longer shows original station build duration"),E.BuildDuration.ToString(),FString(TEXT("Upgrade complete")));
   TestTrue(TEXT("Office status identifies the completed building"),Model->GetSnapshot().TradeStationState.ToString().Contains(TEXT("Merchant Office")));
   TestTrue(TEXT("Terms describe granted office capabilities"),E.Summary.ToString().Contains(TEXT("Merchant Office"))&&E.Summary.ToString().Contains(TEXT("4 standing-order slots")));
   for(const TCHAR* Semantic:{TEXT("TradeMap.Overview.Identity"),TEXT("TradeMap.Station.Identity"),TEXT("TradeMap.Station.BuildingName")}){
    const auto W=Screen->ResolveSemanticWidget(Semantic);if(TestTrue(TEXT("Office identity has a native text widget"),W.IsValid()))TestTrue(TEXT("Visible native identity names Merchant Office"),StaticCastSharedPtr<STextBlock>(W)->GetText().ToString().Contains(TEXT("Merchant Office")));
   }
   auto Before=E;Before.bOfficeBuilt=false;TestFalse(TEXT("Office completion invalidates presentation equality"),Before==E);
  };
  CheckOffice();
  Model->CloseIntent();TestTrue(TEXT("Completed office reopens with preserved station identity"),Model->OpenWorldStation(TEXT("City.Rostock"),StationId.GetValue()));CheckOffice();
  TArray<uint8> OfficeSave;TestTrue(TEXT("Completed office saves"),Host->CaptureSaveBytes(OfficeSave,TEXT("Completed office selection test"),TEXT("2026-10-01T00:00:00Z")).IsSuccess());
  Model->CloseIntent();TestTrue(TEXT("Completed office reloads"),Host->RestoreSaveBytes(OfficeSave).IsSuccess());if(!Refresh())return false;
  TestTrue(TEXT("Reloaded office reopens through world selection"),Model->OpenWorldStation(TEXT("City.Rostock"),StationId.GetValue()));CheckOffice();
  TestTrue(TEXT("Office operations keep the shared ledger workflow"),Screen->ActivateSemanticId(TEXT("TradeMap.Station.Action")));TestEqual(TEXT("Office operations open ledger"),Model->GetSnapshot().ActiveSection,FString(TEXT("Ledger")));
  TestEqual(TEXT("Route quantity instructions remain unchanged"),Complete.GetRoutes()[0].Stops[0].Actions[0].QuantityLimit.GetRawValue(),int64(30000));
  if(Import){const auto* Cargo=Complete.GetInventories().FindByPredicate([](const auto& I){return I.Id.GetValue()==2;});TestTrue(TEXT("Existing unrelated cargo is preserved"),Cargo&&Cargo->Stocks.ContainsByPredicate([&](const auto& S){return S.GoodId==Salt&&S.Stock.GetRawValue()==1000;}));}
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMerchantOfficeUpgradeGates,"Hansa.UI.TradeMap.Presence.AutomaticMerchantOfficeGates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMerchantOfficeUpgradeGates::RunTest(const FString&)
{
 for(bool Qualified:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,Qualified?70000:1000000,[&](FHansaSimulationInitialization& Init){
   if(Qualified)for(auto& P:Init.ForeignPresences)if(P.HouseId==Host->GetHouseId())P.Contributions={1000000,100,1000000,1000000,1000000,100,100};
   Init.Inventories[0].InitialStock={{FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,FHansaQuantity::FromRaw(8000)},{FHansaGoodId::TryParse(TEXT("Good.Planks")).Value,FHansaQuantity::FromRaw(4000)}};
   FHansaRouteState R;const auto& V=Init.Vehicles[1];R.Id=FHansaRouteId::TryCreate(1).Value;R.OwnerId=V.OwnerId;R.VehicleId=V.Id;R.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;R.Mode=EHansaRouteMode::Sea;R.Lifecycle=EHansaRouteLifecycleState::Inactive;R.Stops={{V.CurrentCityId,{}},{City,{}}};Init.Routes.Add(R);
  })){AddError(Error);return false;}
  FHansaPlacementSpec Spec;Spec.CityId=City;Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.TradeHouse")).Value;
  bool Found=false;for(int32 X=8;X<19&&!Found;++X)for(int32 Y=8;Y<19;++Y){Spec.Anchor={X,Y};if(Host->TradeHousePlacementError(Spec).IsEmpty()){Found=true;break;}}
  if(!Found||!Host->PlaceBuildings({Spec})){AddError(TEXT("Could not place upgrade gate fixture"));return false;}
  const auto Id=Host->BuildProjection().Value.GetTradeStations()[0].Station.Id;
  if(!Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(1).Value)||!Host->AdvanceTicks(4))return false;
  const auto Before=Host->BuildProjection().Value;
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Before,*Host->GetEconomicRegistry());Model->OpenWorldStation(TEXT("City.Rostock"),Id.GetValue());
  TestTrue(TEXT("Can inspect a candidate delivery source"),Model->SelectPresenceSource(TEXT("2")));
  TestFalse(TEXT("Source choice cannot bypass trading requirements or funds"),Model->GetSnapshot().bCanPresenceUpgradeAction);
  TestFalse(TEXT("Authority rejects ineligible direct upgrade"),Host->FundPresenceUpgrade({City,TEXT("PresenceStage.MerchantOffice"),FHansaInventoryId::TryCreate(2).Value}).IsSuccess());
  TestEqual(TEXT("Rejected upgrade is atomic"),Host->BuildProjection().Value.GetFingerprint().Value,Before.GetFingerprint().Value);
 }
 return !HasAnyErrors();
}
#endif
