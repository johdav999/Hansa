#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeEstablishmentTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Framework/Application/SlateApplication.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeEstablishmentJourney,"Hansa.UI.TradeMap.Establishment.ExplicitSourceAndAuthority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeEstablishmentJourney::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true}){
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error)){AddError(Error);return false;}
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->SetViewerHouse(Host->GetHouseId());if(!Remote)M->BindRuntime(Host.Get());
 FHansaMultiplayerAuthority Authority;bool Delay=false;FHansaClientCommandIntent Pending;int64 Nonce=8100;
 if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;if(!Authority.RegisterAdmittedClient({808,FHansaParticipantId::TryCreate(1808).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
 M->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& I){Pending=I;Pending.ClientSequence=Authority.GetExpectedClientSequence(808);Pending.ClientNonce=++Nonce;FHansaClientCommandFeedback Ack;Ack.State=EHansaClientCommandState::Pending;Ack.ClientSequence=Pending.ClientSequence;Ack.ClientNonce=Pending.ClientNonce;M->ReceiveCommandFeedback(Ack);if(!Delay)M->ReceiveCommandFeedback(Authority.SubmitIntent(808,Pending));return true;});}
 auto Refresh=[&]{if(Remote){FHansaClientProjectionSnapshot Wire;if(Authority.BuildProjection(808,0,true,Wire,Error)){TestTrue(TEXT("Remote projection carries station reviews"),!Wire.StationEstablishments.IsEmpty());TestTrue(TEXT("Remote projection carries owner presence"),Wire.Presences.ContainsByPredicate([](const auto& V){return V.City==TEXT("City.Rostock")&&!V.CurrentStage.IsEmpty();}));for(const auto& E:Wire.StationEstablishments)TestFalse(TEXT("Remote review never exposes rival ship"),E.Sources.ContainsByPredicate([](const auto& S){return S.Id==TEXT("3");}));TArray<uint8> Transport;FMemoryWriter Writer(Transport);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);FHansaClientProjectionSnapshot Received;FMemoryReader Reader(Transport);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Received,nullptr);TestFalse(TEXT("Wire review deserializes"),Reader.IsError());M->ApplyRemoteEstablishment(Received);TestTrue(TEXT("Remote presence summary is hydrated"),!M->GetSnapshot().PresenceStageSummary.IsEmpty());if(const auto* City=M->GetSelectedCityPresentation())TestTrue(TEXT("Remote city uses reviewed coordinates"),City->NormalizedPosition!=FVector2D::ZeroVector);}else AddError(Error);}else M->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());};Refresh();M->Open();M->SelectCityIntent(TEXT("City.Rostock"));
 auto View=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());View->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence"));auto Act=[&](const TCHAR* Id){return View->ActivateSemanticId(Id);};
 TestFalse(TEXT("No implicit site"),M->GetSnapshot().bCanTradeStationAction);TestTrue(TEXT("Choose site through ordinary input"),Act(TEXT("TradeMap.Station.Site")));TestTrue(TEXT("Requirements accessible to controller"),View->GetControllerFocusOrder().Contains(TEXT("TradeMap.Station.Terms")));TestTrue(TEXT("Open exact site terms"),Act(TEXT("TradeMap.Station.Terms")));
 TestTrue(TEXT("Review proposal"),Act(TEXT("TradeMap.Station.Action")));TestEqual(TEXT("Review does not propose"),Host->BuildProjection().Value.GetTradeStations().Num(),0);
 TestTrue(TEXT("Cancel proposal review"),Act(TEXT("TradeMap.Station.Cancel")));TestTrue(TEXT("Reviewed site survives cancellation"),!M->GetSnapshot().Establishment.SiteId.IsEmpty());
 TestTrue(TEXT("Review again"),Act(TEXT("TradeMap.Station.Action")));TestTrue(TEXT("Confirm proposal"),Act(TEXT("TradeMap.Station.Confirm")));Refresh();
 const int64 Station=M->GetSnapshot().TradeStationValue;TestTrue(TEXT("Proposal has stable identity"),Station>0);TestTrue(TEXT("Funding starts unselected"),M->GetSnapshot().Establishment.SourceId.IsEmpty());TestFalse(TEXT("Cannot default-submit first owned vehicle"),Act(TEXT("TradeMap.Station.Action")));
 TestEqual(TEXT("Only two owned ships and own station appear"),M->GetSnapshot().Establishment.Sources.Num(),3);
 TestTrue(TEXT("Select first owned source"),Act(TEXT("TradeMap.Station.Source")));TestFalse(TEXT("Empty ship cannot fund"),M->GetSnapshot().Establishment.bCanFund);TestTrue(TEXT("Shortfall is explained"),M->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("shortfall")));
 TestTrue(TEXT("Select second owned source"),Act(TEXT("TradeMap.Station.Source")));TestEqual(TEXT("Second inventory selected"),M->GetSnapshot().Establishment.SourceId,FString(TEXT("2")));TestTrue(TEXT("Home-located cargo remains legal under existing command"),M->GetSnapshot().Establishment.bCanFund);
 TestTrue(TEXT("Review exact material spending"),Act(TEXT("TradeMap.Station.Action")));const auto Hash=Host->BuildProjection().Value.GetFingerprint().Value;
 TestTrue(TEXT("Review shows remainder"),M->GetSnapshot().Establishment.Confirmation.ToString().Contains(TEXT("afterward")));
 if(Remote){Delay=true;TestTrue(TEXT("Send reviewed funding"),Act(TEXT("TradeMap.Station.Confirm")));TestFalse(TEXT("Duplicate funding disabled while pending"),Act(TEXT("TradeMap.Station.Confirm")));TestFalse(TEXT("Pending station review cannot be replaced by route creation"),M->BeginCreateIntent());FHansaClientCommandFeedback Unrelated;Unrelated.ClientSequence=999;Unrelated.ClientNonce=999;Unrelated.State=EHansaClientCommandState::Rejected;M->ReceiveCommandFeedback(Unrelated);TestTrue(TEXT("Unrelated feedback cannot complete station command"),M->GetSnapshot().Establishment.bPending);Host->AdvanceTicks(65);const auto BeforeReject=Host->BuildProjection().Value.GetFingerprint().Value;const auto Rejected=Authority.SubmitIntent(808,Pending);TestEqual(TEXT("Remote stale tick rejects"),Rejected.Rejection,EHansaClientCommandRejection::StaleProjection);M->ReceiveCommandFeedback(Rejected);TestEqual(TEXT("Rejection mutates nothing"),Host->BuildProjection().Value.GetFingerprint().Value,BeforeReject);Refresh();TestEqual(TEXT("Rejection retains chosen inventory"),M->GetSnapshot().Establishment.SourceId,FString(TEXT("2")));Delay=false;TestTrue(TEXT("Re-review after rejection"),Act(TEXT("TradeMap.Station.Action")));}
 else {TestEqual(TEXT("Review preserves state"),Host->BuildProjection().Value.GetFingerprint().Value,Hash);}
 TestTrue(TEXT("Confirm chosen source spending"),Act(TEXT("TradeMap.Station.Confirm")));Refresh();TestTrue(TEXT("Separate construction state"),M->GetSnapshot().Establishment.bConstructing);TestFalse(TEXT("Cannot spend again during construction"),Act(TEXT("TradeMap.Station.Action")));
 const auto Funded=Host->BuildProjection().Value;const auto* S=Funded.GetTradeStations().FindByPredicate([&](const auto& X){return int64(X.Station.Id.GetValue())==Station;});TestNotNull(TEXT("Station identity retained"),S);if(S)TestEqual(TEXT("Authority spent exactly chosen inventory"),S->Station.FundingInventoryId.GetValue(),uint64(2));
 const auto* Inv=Funded.GetInventories().FindByPredicate([](const auto& X){return X.Id.GetValue()==2;});TestTrue(TEXT("Inventory remainder conserved"),Inv&&Inv->UsedCapacity.GetRawValue()==6000);
 TArray<uint8> Bytes;TestTrue(TEXT("Save constructing station"),Host->CaptureSaveBytes(Bytes,TEXT("TG08"),TEXT("2026-09-23T00:00:00Z")).IsSuccess());TestTrue(TEXT("Reload constructing station"),Host->RestoreSaveBytes(Bytes).IsSuccess());Host->AdvanceTicks(3);Refresh();TestEqual(TEXT("Save/reload preserves identity"),M->GetSnapshot().TradeStationValue,Station);TestTrue(TEXT("Authoritative completion"),M->GetSnapshot().Establishment.bComplete);TestTrue(TEXT("Success opens station operations"),Act(TEXT("TradeMap.Station.Action")));TestEqual(TEXT("Operations ledger"),M->GetSnapshot().ActiveSection,FString(TEXT("Ledger")));
 }
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeEstablishmentReviewSafety,"Hansa.UI.TradeMap.Establishment.ReviewSafetyAndPrivacy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeEstablishmentReviewSafety::RunTest(const FString&) {
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error)){AddError(Error);return false;}
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());M->InitializeDefaults();M->SetViewerHouse(Host->GetHouseId());const auto P=Host->BuildProjection().Value;const auto* R=Host->GetEconomicRegistry();M->ApplyProjection(P,*R);M->Open();M->SelectCityIntent(TEXT("City.Rostock"));
 TestFalse(TEXT("Unknown source IDs rejected"),M->SelectEstablishmentSource(TEXT("999")));TestFalse(TEXT("Rival cargo not selectable"),M->SelectEstablishmentSource(TEXT("3")));
 M->EstablishmentIntent(TEXT("Site"));M->SelectEstablishmentSource(TEXT("2"));TestTrue(TEXT("Review opens"),M->TradeStationActionIntent());
 auto View=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());auto StableSource=View->ResolveSemanticWidget(TEXT("TradeMap.Station.Source"));
 M->ApplyProjection(P,*R);TestTrue(TEXT("Unchanged refresh preserves review"),M->GetSnapshot().Establishment.bReview);TestTrue(TEXT("Unchanged refresh preserves widget"),StableSource==View->ResolveSemanticWidget(TEXT("TradeMap.Station.Source")));
 auto Changed=*R;auto Stages=R->GetPresenceStages();for(auto& S:Stages)if(S.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation")))S.UpgradeCostPfennig+=1000;Changed.SetPresenceDefinitions(R->GetPresenceCapabilities(),Stages,R->GetCityTradePolicies());M->ApplyProjection(P,Changed);
 TestFalse(TEXT("Changed policy invalidates reviewed spending"),M->GetSnapshot().Establishment.bReview);TestFalse(TEXT("Stale confirm cannot submit"),M->EstablishmentIntent(TEXT("Confirm")));TestEqual(TEXT("Stale review retains source"),M->GetSnapshot().Establishment.SourceId,FString(TEXT("2")));TestTrue(TEXT("Stale review gives remedy"),!M->GetSnapshot().Establishment.Feedback.IsEmpty());
 auto Policies=R->GetCityTradePolicies();for(auto& Policy:Policies)if(Policy.CityId==TEXT("City.Rostock"))Policy.DeniedCapabilityIds.Add(TEXT("PresenceCapability.TradeStation"));Changed.SetPresenceDefinitions(R->GetPresenceCapabilities(),R->GetPresenceStages(),Policies);M->ApplyProjection(P,Changed);TestFalse(TEXT("Denied policy disables proposal"),M->GetSnapshot().Establishment.bCanPropose);TestTrue(TEXT("Denied policy explains remedy"),M->GetSnapshot().Establishment.Summary.ToString().Contains(TEXT("Restore the required rights")));
 M->SetViewerHouse(FHansaHouseId());M->ApplyProjection(P,*R);TestTrue(TEXT("Absent viewer sees no private sources"),M->GetSnapshot().Establishment.Sources.IsEmpty());TestFalse(TEXT("Absent viewer cannot propose"),M->GetSnapshot().Establishment.bCanPropose);
 if(!Hansa::Tests::PrepareEstablishment(*Host,Error,1)){AddError(Error);return false;}M->BindRuntime(Host.Get());M->ApplyProjection(Host->BuildProjection().Value,*R);M->EstablishmentIntent(TEXT("Site"));TestTrue(TEXT("Proposal is separate from affordability"),M->TradeStationActionIntent());TestTrue(TEXT("Proposal spends no money"),M->EstablishmentIntent(TEXT("Confirm")));M->SelectEstablishmentSource(TEXT("2"));TestFalse(TEXT("Insufficient treasury disables spending"),M->GetSnapshot().Establishment.bCanFund);TestTrue(TEXT("Money shortfall has a remedy"),M->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("Treasury shortfall")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeOfficeRemoteJourney,"Hansa.UI.TradeMap.Presence.RemoteOfficeJourney",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeOfficeRemoteJourney::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
 const auto House=Host->GetHouseId();const auto* Registry=Host->GetEconomicRegistry();
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 const auto StationId=FHansaTradeStationId::TryCreate(1).Value;const auto InvId=FHansaInventoryId::TryCreate(20).Value;const auto LeaseId=FHansaLeasedPlotId::TryCreate(1).Value;
 if(!Hansa::Tests::PrepareEstablishment(*Host,Error,500000,[&](FHansaSimulationInitialization& Init){
  const auto* TradeStage=Registry->FindPresenceStage(TEXT("PresenceStage.TradeStation"));
  for(auto& P:Init.ForeignPresences)if(P.HouseId==House&&P.CityId==City){P.CurrentStageId=TradeStage->StableId;P.GrantedCapabilityIds=TradeStage->GrantedCapabilityIds;P.GrantedCapabilityIds.Sort();P.StationId=StationId;P.LeasedPlotId=LeaseId;P.Contributions.LawfulTradeVolumeMilliUnits=250000;P.Contributions.CompletedDeliveryCount=12;P.Contributions.InvestedPfennig=25000;P.Contributions.TransactionValuePfennig=250000;P.Contributions.FulfilledShortageMilliUnits=50000;P.Contributions.ReliableOperatingTicks=12;P.Contributions.SolventOperatingTicks=8;}
  FHansaTradeStationState Station;Station.Id=StationId;Station.OwnerId=House;Station.CityId=City;Station.SiteId=TEXT("TradeStationSite.Rostock.Harbor.01");Station.InventoryId=InvId;Station.FactorId=FHansaFactorId::TryCreate(1).Value;Station.LeasedPlotId=LeaseId;Station.Status=EHansaTradeStationStatus::Active;Station.UpkeepPfennigPerTick=25;Init.TradeStations.Add(Station);
  FHansaLeasedPlotState Lease;Lease.Id=LeaseId;Lease.StationId=StationId;Lease.OwnerId=House;Lease.CityId=City;Lease.SiteId=Station.SiteId;Lease.PlotCategory=TEXT("Commercial");Lease.BoundsMin={8,8};Lease.BoundsMax={19,19};Lease.PermittedBuildingCategories={TEXT("Storage"),TEXT("Commercial"),TEXT("Production")};Lease.bActive=true;Lease.bOccupied=true;Init.LeasedPlots.Add(Lease);
  FHansaInventoryInitialization Storage;Storage.Id=InvId;Storage.OwnerKind=EHansaInventoryOwnerKind::TradeStation;Storage.CityId=City;Storage.TradeStationId=StationId;Storage.Capacity=FHansaQuantity::FromRaw(50000);const auto Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value,Tools=FHansaGoodId::TryParse(TEXT("Good.Tools")).Value;Storage.AcceptedGoods={Planks,Tools};Storage.InitialStock={{Planks,FHansaQuantity::FromRaw(30000)},{Tools,FHansaQuantity::FromRaw(7000)}};Init.Inventories.Add(Storage);
 })){AddError(Error);return false;}
 FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;
 if(!Authority.RegisterAdmittedClient({908,FHansaParticipantId::TryCreate(1908).Value,House,EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();
 int64 Nonce=9000;Model->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Command){auto I=Command;I.ClientSequence=Authority.GetExpectedClientSequence(908);I.ClientNonce=++Nonce;FHansaClientCommandFeedback Pending;Pending.State=EHansaClientCommandState::Pending;Pending.ClientSequence=I.ClientSequence;Pending.ClientNonce=I.ClientNonce;Model->ReceiveCommandFeedback(Pending);const auto Result=Authority.SubmitIntent(908,I);Model->ReceiveCommandFeedback(Result);return Result.bAccepted;});
 auto Refresh=[&]{FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(908,0,true,Wire,Error)){AddError(Error);return false;}TestEqual(TEXT("Only owner Presence is replicated"),Wire.Presences.Num(),1);TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);if(Reader.IsError()){AddError(TEXT("Remote Presence wire decode failed"));return false;}Model->ApplyRemoteEstablishment(Copy);return true;};
 if(!Refresh())return false;Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));TestEqual(TEXT("Relevant remote city marker opens Presence"),Model->GetSnapshot().ActiveSection,FString(TEXT("Presence")));auto Ui=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());Ui->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence"));
 TestTrue(TEXT("Remote requirements are labeled"),Model->GetSnapshot().PresenceRequirements.Num()>0);
 TestTrue(TEXT("Remote completed station enters its dedicated Upgrade tab"),Ui->ActivateSemanticId(TEXT("TradeMap.Station.Tab.Upgrade")));
 TestTrue(TEXT("Remote review opens"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
 const auto Before=Host->BuildProjection().Value.GetFingerprint().Value;TestEqual(TEXT("Remote review spends nothing"),Host->BuildProjection().Value.GetFingerprint().Value,Before);
 TestTrue(TEXT("Remote review request reaches authority"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
 if(!Refresh())return false;TestTrue(TEXT("Remote funding requires explicit source"),Model->GetSnapshot().PresenceSourceId.IsEmpty()&&!Model->GetSnapshot().bCanPresenceUpgradeAction);
 TestTrue(TEXT("Remote source is chosen through UI"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Source")));
 TestFalse(TEXT("Empty ship cannot fund office"),Model->GetSnapshot().bCanPresenceUpgradeAction);
 TestTrue(TEXT("Cycle to second owned source"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Source")));
 TestTrue(TEXT("Cycle to funded station source"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Source")));
 TestEqual(TEXT("Remote source is owned station"),Model->GetSnapshot().PresenceSourceId,FString(TEXT("20")));
 TestTrue(TEXT("Remote funding review opens"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
 Host->AdvanceTicks(1);if(!Refresh())return false;
 TestFalse(TEXT("Changed treasury invalidates office review"),Model->GetSnapshot().bPresenceReview);
 TestEqual(TEXT("Stale review retains chosen source"),Model->GetSnapshot().PresenceSourceId,FString(TEXT("20")));
 TestTrue(TEXT("Remote funding can be reviewed again"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
 TestTrue(TEXT("Remote funding reaches authority"),Ui->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
 if(!Refresh())return false;TestEqual(TEXT("Remote construction state"),Model->GetSnapshot().PresenceStatus.ToString(),FString(TEXT("Under construction")));
 TArray<uint8> Save;TestTrue(TEXT("Funded office saves"),Host->CaptureSaveBytes(Save,TEXT("TG11 remote"),TEXT("2026-09-23T00:00:00Z")).IsSuccess());TestTrue(TEXT("Funded office reloads"),Host->RestoreSaveBytes(Save).IsSuccess());
 Host->AdvanceTicks(3);if(!Refresh())return false;TestEqual(TEXT("Remote completed office is active"),Model->GetSnapshot().PresenceStatus.ToString(),FString(TEXT("Active")));TestTrue(TEXT("Remote stage identity is Merchant Office"),Model->GetSnapshot().PresenceStageSummary.ToString().Contains(TEXT("Merchant Office")));
 TestTrue(TEXT("Remote stage consequence is explained"),Model->GetSnapshot().PresenceConsequences.ToString().Contains(TEXT("Civic:")));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstructionCargoRouteWarning,"Hansa.UI.TradeMap.Establishment.ConstructionCargoRouteWarning",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FConstructionCargoRouteWarning::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,200000,[&](FHansaSimulationInitialization& Init){
  const auto& V=Init.Vehicles[1];FHansaRouteState Route;
  Route.Id=FHansaRouteId::TryCreate(1).Value;Route.OwnerId=V.OwnerId;Route.VehicleId=V.Id;
  Route.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;Route.Mode=EHansaRouteMode::Sea;
  FHansaRouteCargoAction Load;Load.GoodId=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value;Load.QuantityLimit=FHansaQuantity::FromRaw(20000);
  auto Unload=Load;Unload.Kind=EHansaRouteCargoActionKind::Unload;
  auto PlanksLoad=Load;PlanksLoad.GoodId=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
  auto PlanksUnload=PlanksLoad;PlanksUnload.Kind=EHansaRouteCargoActionKind::Unload;
  Route.Stops={{V.CurrentCityId,{Load,PlanksLoad}},{FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,{Unload,PlanksUnload}}};Init.Routes.Add(Route);
 })){AddError(Error);return false;}
 const auto P=Host->BuildProjection().Value;
 const auto E=Hansa::UI::BuildTradeEstablishment(P,*Host->GetEconomicRegistry(),Host->GetHouseId(),TEXT("City.Rostock"),TEXT("TradeStationSite.Rostock.Harbor.01"),TEXT("2"));
 const auto* Source=E.Sources.FindByPredicate([](const auto& S){return S.Id==TEXT("2");});
 if(!TestNotNull(TEXT("Owned cargo source"),Source))return false;
 TestTrue(TEXT("Actual timber and planks qualify despite planned export"),Source->bEligible);
 TestTrue(TEXT("Planned loads distinguished from physical cargo"),Source->Detail.ToString().Contains(TEXT("planned route loads do not count as secured")));
 TestTrue(TEXT("Exact scheduled export and city explained"),Source->Detail.ToString().Contains(TEXT("Route unloads up to 20 Timber at Rostock")));
 TestTrue(TEXT("Shared capacity conflict explained"),Source->Detail.ToString().Contains(TEXT("Route requests 40 units at Lübeck for a 30-unit hold")));
 TestTrue(TEXT("Route can continue after funding"),Source->Detail.ToString().Contains(TEXT("The route can keep running")));
 TestFalse(TEXT("Other ship does not inherit route warning"),E.Sources[0].Detail.ToString().Contains(TEXT("Route unloads")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstructionPickupJourney,"Hansa.UI.TradeMap.Establishment.ConstructionPickupJourney",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FConstructionPickupJourney::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true})for(bool Supplied:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,200000,[&](FHansaSimulationInitialization& Init){
   const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
   Init.Inventories[1].InitialStock={{Timber,FHansaQuantity::FromRaw(2000)}};
   auto& CityStock=Init.Inventories[3];CityStock.AcceptedGoods={Timber,Planks};CityStock.InitialStock={{Timber,FHansaQuantity::FromRaw(20000)},{Planks,FHansaQuantity::FromRaw(Supplied?20000:2000)}};
   const auto& V=Init.Vehicles[1];FHansaRouteState Route;Route.Id=FHansaRouteId::TryCreate(1).Value;Route.OwnerId=V.OwnerId;Route.VehicleId=V.Id;Route.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;Route.Mode=EHansaRouteMode::Sea;
   FHansaRouteCargoAction Load;Load.Kind=EHansaRouteCargoActionKind::OwnedCityLoad;Load.GoodId=Timber;Load.QuantityLimit=FHansaQuantity::FromRaw(20000);Load.CargoSlotIndex=0;auto Unload=Load;Unload.Kind=EHansaRouteCargoActionKind::Unload;
   Route.Stops={{V.CurrentCityId,{Load}},{FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,{Unload}}};Route.Lifecycle=EHansaRouteLifecycleState::Inactive;Route.bPendingStopActions=false;Init.Routes.Add(Route);
  },true)){AddError(Error);return false;}
  FHansaTradeStationId Id;TestTrue(TEXT("Reserve station"),Host->ProposeTradeStation(FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,TEXT("TradeStationSite.Rostock.Harbor.01"),Id).IsSuccess());
  const auto E=Hansa::UI::BuildTradeEstablishment(Host->BuildProjection().Value,*Host->GetEconomicRegistry(),Host->GetHouseId(),TEXT("City.Rostock"),TEXT("TradeStationSite.Rostock.Harbor.01"),TEXT("2"));
  TestTrue(TEXT("Missing materials offer pickup"),E.bCanFund);TestTrue(TEXT("Pickup action names city"),E.Action.ToString().Contains(TEXT("Review collection at Lübeck")));TestTrue(TEXT("Pickup stock preview"),E.Confirmation.ToString().Contains(TEXT("city stock now")));
  FHansaMultiplayerAuthority Authority;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;if(!Authority.RegisterAdmittedClient({909,FHansaParticipantId::TryCreate(1909).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}FHansaClientCommandIntent I;I.Type=EHansaClientIntentType::FundTradeStation;I.TradeStationId=Id.GetValue();I.FundingInventoryId=2;I.ExpectedServerTick=E.Tick;I.ClientSequence=Authority.GetExpectedClientSequence(909);I.ClientNonce=1;TestTrue(TEXT("Server accepts reviewed pickup"),Authority.SubmitIntent(909,I).bAccepted);}
  else TestTrue(TEXT("Local accepts reviewed pickup"),Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(2).Value).IsSuccess());
  auto P=Host->BuildProjection().Value;const auto* Station=P.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Id;});
  TestTrue(TEXT("Partial materials are escrowed"),Station&&Station->Station.Status==EHansaTradeStationStatus::Proposed&&Station->Station.SpentGoods.Num()==1&&Station->Station.SpentGoods[0].Quantity.GetRawValue()==2000);
  TestEqual(TEXT("Money paid once"),P.GetHouses()[0].Money.GetRawValue(),int64(150000));
  const auto Before=P.GetFingerprint().Value;TestFalse(TEXT("Repeated funding is rejected"),Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(2).Value).IsSuccess());TestEqual(TEXT("Rejected repeat changes nothing"),Host->BuildProjection().Value.GetFingerprint().Value,Before);
  TArray<uint8> PendingSave;TestTrue(TEXT("Save pending pickup"),Host->CaptureSaveBytes(PendingSave,TEXT("Pickup"),TEXT("2026-09-28T00:00:00Z")).IsSuccess());TestTrue(TEXT("Reload pending pickup"),Host->RestoreSaveBytes(PendingSave).IsSuccess());
  if(Remote){FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(909,0,true,Wire,Error)){AddError(Error);return false;}TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->ApplyRemoteEstablishment(Copy);Model->SelectCityIntent(TEXT("City.Rostock"));TestTrue(TEXT("Remote pending survives transport without local selected source"),Model->GetSnapshot().Establishment.bAwaitingPickup);TestFalse(TEXT("Remote pending cannot double-fund"),Model->GetSnapshot().Establishment.bCanFund);TestTrue(TEXT("Remote secured quantity visible"),Model->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("secured 2 / 8")));}
  TestTrue(TEXT("Resume assigned Cog route"),Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true).IsSuccess());Host->AdvanceTicks(1);P=Host->BuildProjection().Value;Station=P.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Id;});
  if(Supplied){
   TestTrue(TEXT("Pickup starts construction before ordinary loads"),Station&&Station->Station.Status==EHansaTradeStationStatus::UnderConstruction&&Station->Station.SpentGoods.Num()==2);
   const auto* City=P.GetInventories().FindByPredicate([](const auto& I){return I.Id.GetValue()==10;});const auto* Planks=City?City->Stocks.FindByPredicate([](const auto& S){return S.GoodId.ToString()==TEXT("Good.Planks");}):nullptr;TestTrue(TEXT("Construction consumes exact plank pickup"),Planks&&Planks->Stock.GetRawValue()==16000);
   Host->AdvanceTicks(3);P=Host->BuildProjection().Value;Station=P.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Id;});TestTrue(TEXT("Construction completes automatically"),Station&&Station->Station.Status==EHansaTradeStationStatus::Active);TestTrue(TEXT("Completed pickup activates commercial lease immediately"),Station&&Station->Lease.bActive);
  }else{
   TestTrue(TEXT("Short city stock leaves order waiting"),Station&&Station->Station.Status==EHansaTradeStationStatus::Proposed&&Station->Station.CompletionTick.GetValue()==0);
   const auto* Planks=Station?Station->Station.SpentGoods.FindByPredicate([](const auto& G){return G.GoodId.ToString()==TEXT("Good.Planks");}):nullptr;TestTrue(TEXT("Partial pickup remains protected"),Planks&&Planks->Quantity.GetRawValue()==2000);
   TArray<uint8> Partial;TestTrue(TEXT("Partial pickup saves"),Host->CaptureSaveBytes(Partial,TEXT("Partial pickup"),TEXT("2026-09-28T00:00:00Z")).IsSuccess());TestTrue(TEXT("Partial pickup restores"),Host->RestoreSaveBytes(Partial).IsSuccess());
  }
  TestTrue(TEXT("Restore pending order for cancellation"),Host->RestoreSaveBytes(PendingSave).IsSuccess());TestTrue(TEXT("Cancel pickup"),Host->CloseTradeStation(Id).IsSuccess());P=Host->BuildProjection().Value;TestEqual(TEXT("Cancellation uses authored 50 percent money refund"),P.GetHouses()[0].Money.GetRawValue(),int64(175000));const auto* Cargo=P.GetInventories().FindByPredicate([](const auto& I){return I.Id.GetValue()==2;});TestTrue(TEXT("Refund secured goods only"),Cargo&&Cargo->UsedCapacity.GetRawValue()==1000);Host->AdvanceTicks(1);P=Host->BuildProjection().Value;Station=P.GetTradeStations().FindByPredicate([&](const auto& S){return S.Station.Id==Id;});TestTrue(TEXT("Cancelled order never restarts"),Station&&Station->Station.Status==EHansaTradeStationStatus::Closed);
 }
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFundingReviewWhileSailing,"Hansa.UI.TradeMap.Establishment.FundingReviewWhileSailing",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FFundingReviewWhileSailing::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,200000,[&](FHansaSimulationInitialization& Init){
   auto& V=Init.Vehicles[1];V.UpkeepPfennigPerTravelTick=5;
   auto& City=Init.Inventories[3];City.AcceptedGoods.Add(FHansaGoodId::TryParse(TEXT("Good.Planks")).Value);City.InitialStock={{FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,FHansaQuantity::FromRaw(20000)},{FHansaGoodId::TryParse(TEXT("Good.Planks")).Value,FHansaQuantity::FromRaw(20000)}};
   FHansaRouteState Route;Route.Id=FHansaRouteId::TryCreate(1).Value;Route.OwnerId=V.OwnerId;Route.VehicleId=V.Id;Route.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;Route.Mode=EHansaRouteMode::Sea;
   FHansaRouteCargoAction A;A.Kind=EHansaRouteCargoActionKind::OwnedCityLoad;A.GoodId=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value;A.QuantityLimit=FHansaQuantity::FromRaw(20000);auto B=A;B.Kind=EHansaRouteCargoActionKind::Unload;
   Route.Stops={{V.CurrentCityId,{A}},{FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,{B}}};Route.Lifecycle=EHansaRouteLifecycleState::Traveling;Route.TotalTravelTicks=8;Route.RemainingTravelTicks=6;Route.Progress=FHansaRate::TryRatio(2,8).Value;Init.Routes.Add(Route);
  },true)){AddError(Error);return false;}
  FHansaTradeStationId Id;TestTrue(TEXT("Reserve site while Cog sails"),Host->ProposeTradeStation(FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,TEXT("TradeStationSite.Rostock.Harbor.01"),Id).IsSuccess());
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->SetViewerHouse(Host->GetHouseId());if(!Remote)Model->BindRuntime(Host.Get());
  FHansaMultiplayerAuthority Authority;int64 Nonce=100;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;if(!Authority.RegisterAdmittedClient({919,FHansaParticipantId::TryCreate(1919).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}Model->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Draft){auto I=Draft;I.ClientSequence=Authority.GetExpectedClientSequence(919);I.ClientNonce=++Nonce;FHansaClientCommandFeedback Pending;Pending.State=EHansaClientCommandState::Pending;Pending.ClientSequence=I.ClientSequence;Pending.ClientNonce=I.ClientNonce;Model->ReceiveCommandFeedback(Pending);const auto Ack=Authority.SubmitIntent(919,I);Model->ReceiveCommandFeedback(Ack);return Ack.bAccepted;});}
  auto Refresh=[&](){if(Remote){FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(919,0,true,Wire,Error)){AddError(Error);return;}TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);Model->ApplyRemoteEstablishment(Copy);}else Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());};
  Refresh();Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));Model->SelectEstablishmentSource(TEXT("2"));auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());
  TestTrue(TEXT("Action explicitly opens funding review"),Model->GetSnapshot().Establishment.Action.ToString().Contains(TEXT("Review funding")));
  TestTrue(TEXT("Review opened through normal control"),Screen->ActivateSemanticId(TEXT("TradeMap.Station.Action")));
  const auto MoneyBefore=Host->BuildProjection().Value.GetHouses()[0].Money.GetRawValue();const auto Key=Model->GetSnapshot().Establishment.ReviewKey;
  TestTrue(TEXT("Sailing advances while reading review"),Host->AdvanceTicks(12));Refresh();
  TestTrue(TEXT("Route upkeep changed treasury"),Host->BuildProjection().Value.GetHouses()[0].Money.GetRawValue()!=MoneyBefore);
  TestEqual(TEXT("Fixed funding terms survive travel, unloading, cargo and treasury changes"),Model->GetSnapshot().Establishment.ReviewKey,Key);
  TestTrue(TEXT("Confirmation remains open while sailing"),Model->GetSnapshot().Establishment.bReview);
  TestTrue(TEXT("Fund through native confirmation without pausing route"),Screen->ActivateSemanticId(TEXT("TradeMap.Station.Confirm")));Refresh();
  const auto P=Host->BuildProjection().Value;const auto* S=P.GetTradeStations().FindByPredicate([&](const auto& V){return V.Station.Id==Id;});TestTrue(TEXT("Order accepts materials now or queues pickup"),S&&(S->Station.Status==EHansaTradeStationStatus::UnderConstruction||(S->Station.Status==EHansaTradeStationStatus::Proposed&&S->Station.FundingInventoryId.IsValid())));TestTrue(TEXT("Cog keeps sailing"),P.GetRoutes()[0].Lifecycle==EHansaRouteLifecycleState::Traveling);
  TestTrue(TEXT("Run through pickup and completion"),Host->AdvanceTicks(20));const auto Completed=Host->BuildProjection().Value;const auto* Final=Completed.GetTradeStations().FindByPredicate([&](const auto& V){return V.Station.Id==Id;});TestTrue(TEXT("Review through changing cargo results in built station"),Final&&Final->Station.Status==EHansaTradeStationStatus::Active);
 }
 return !HasAnyErrors();
}
#endif
