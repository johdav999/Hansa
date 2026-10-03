#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaTradeEstablishmentTestSupport.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Presence/HansaPresenceConstruction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPlacedPresence,"Hansa.UI.TradeMap.Establishment.PlacedPresence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaPlacedPresence::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,200000,[](FHansaSimulationInitialization& Init){
  for(auto& Presence:Init.ForeignPresences)Presence.Contributions={};
 })){AddError(Error);return false;}
 {
  TArray<uint8> Current,Legacy;FHansaSaveSnapshot Saved;int64 Tick=0;
  if(!Host->CaptureSaveBytes(Current,TEXT("Legacy migration"),TEXT("2026-09-29T00:00:00Z"))||!Host->InspectSaveBytes(Current,Saved,Tick))return false;
  const auto* Registry=Host->GetEconomicRegistry();const auto Base=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(Saved.Scenario.ScenarioId).Value,Registry->GetRegistryHash(),*Registry);
  const auto Context=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(Saved.Scenario.ScenarioId).Value,Registry->GetRegistryHash(),*Registry,*Saved.State.CreateReadOnlyAccess(Base.Value).GetPlacement().GetTopology());
  TestTrue(TEXT("Encode real previous-format fixture"),FHansaSaveEnvelope::EncodeHistoricalFixtureForTests(Saved,Context.Value,22,34,Legacy).IsSuccess());
  TestTrue(TEXT("Migrate previous-format campaign"),Host->RestoreSaveBytes(Legacy).IsSuccess());
 }
 FHansaPlacementSpec Spec;Spec.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.TradeHouse")).Value;Spec.Rotation=EHansaGridRotation::East;
 bool Found=false;for(int32 X=8;X<19&&!Found;++X)for(int32 Y=8;Y<19;++Y){Spec.Anchor={X,Y};if(Host->TradeHousePlacementError(Spec).IsEmpty()){Found=true;break;}}
 if(!TestTrue(TEXT("A real lease has a clear two-cell site"),Found))return false;
 auto Money=[&](){const auto P=Host->BuildProjection();return P.Value.GetHouses().FindByPredicate([&](const auto& H){return H.Id==Host->GetHouseId();})->Money.GetRawValue();};
 const int64 Before=Money();auto Invalid=Spec;Invalid.Anchor={-999,-999};
 TestFalse(TEXT("Outside lease cannot place"),Host->PlaceBuildings({Invalid}).IsSuccess());TestEqual(TEXT("Invalid placement spends nothing"),Money(),Before);
 FHansaTradeStationId LegacyId;
 const FString SiteId=Host->GetEconomicRegistry()->FindCityTradePolicyForCity(Spec.CityId.ToString())->TradeStationSites[0].SiteId;
 TestFalse(TEXT("Unqualified free reservation stays blocked"),Host->ProposeTradeStation(Spec.CityId,SiteId,LegacyId).IsSuccess());
 TestEqual(TEXT("Rejected free reservation spends nothing"),Money(),Before);
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Menu(NewObject<UHansaBuildMenuPresentationModel>());
 if(!Menu->InitializeForLubeck(nullptr,Host.Get(),Error)){AddError(Error);return false;}
 TestTrue(TEXT("Select trade house without trade qualification"),Menu->SelectBuilding(TEXT("Building.TradeHouse")));
 TestTrue(TEXT("Rotate trade house before placement"),Menu->RotateIntent());
 Menu->TargetGridCell(Spec.Anchor.X,Spec.Anchor.Y);
 TestTrue(TEXT("Unqualified local preview allows placement"),Menu->GetSnapshot().bCanConfirm);
 TestTrue(TEXT("Place house without materials or trade qualification"),Menu->ConfirmIntent());
 auto P=Host->BuildProjection();if(!TestEqual(TEXT("Exactly one station"),P.Value.GetTradeStations().Num(),1))return false;
 const auto Id=P.Value.GetTradeStations()[0].Station.Id;const int64 Paid=Money();TestEqual(TEXT("Charge on placement"),Before-Paid,int64(50000));
 TestFalse(TEXT("Duplicate placement rejected"),Host->PlaceBuildings({Spec}).IsSuccess());TestEqual(TEXT("Duplicate never charges"),Money(),Paid);
 TestTrue(TEXT("Assign loaded ship in another city"),Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(2).Value).IsSuccess());TestEqual(TEXT("Assignment does not charge"),Money(),Paid);
 Host->AdvanceTicks(5);P=Host->BuildProjection();TestEqual(TEXT("Wrong-city cargo does not start construction"),P.Value.GetTradeStations()[0].Station.Status,EHansaTradeStationStatus::Proposed);TestTrue(TEXT("Wrong-city cargo not secured"),P.Value.GetTradeStations()[0].Station.SpentGoods.IsEmpty());
 TArray<uint8> Bytes;TestTrue(TEXT("Save paid waiting site"),Host->CaptureSaveBytes(Bytes,TEXT("Placed presence"),TEXT("2026-09-29T00:00:00Z")).IsSuccess());TestTrue(TEXT("Restore paid waiting site"),Host->RestoreSaveBytes(Bytes).IsSuccess());
 P=Host->BuildProjection();TestEqual(TEXT("Restore exact anchor"),P.Value.GetTradeStations()[0].Station.ConstructionSite.Anchor,Spec.Anchor);TestEqual(TEXT("Restore rotation"),P.Value.GetTradeStations()[0].Station.ConstructionSite.Rotation,Spec.Rotation);TestEqual(TEXT("Restore without second payment"),Money(),Paid);
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(P.Value,*Host->GetEconomicRegistry());
 TestTrue(TEXT("Unfinished site selectable"),Model->OpenWorldStation(TEXT("City.Rostock"),Id.GetValue()));TestTrue(TEXT("Change assigned delivery ship"),Model->SelectEstablishmentSource(TEXT("1")));
 // Save fixture moves the already-owned ship to its destination, preserving cargo and the paid order.
 FHansaSaveSnapshot Save;int64 Tick=0;TestTrue(TEXT("Inspect waiting save"),Host->InspectSaveBytes(Bytes,Save,Tick).IsSuccess());
 // Cancellation must refund the configured percentage, once, without granting an active station.
 TestTrue(TEXT("Cancel paid waiting site"),Host->CloseTradeStation(Id).IsSuccess());const int64 CancelMoney=Money();TestTrue(TEXT("Money refunded"),CancelMoney>Paid);Host->CloseTradeStation(Id);TestEqual(TEXT("No repeated refund"),Money(),CancelMoney);

 Host=TStrongObjectPtr<UHansaRuntimeSimulationHost>(NewObject<UHansaRuntimeSimulationHost>());
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,49999,[](FHansaSimulationInitialization& Init){for(auto& Presence:Init.ForeignPresences)Presence.Contributions={};})){AddError(Error);return false;}
 TestFalse(TEXT("Authority rejects placement without enough money"),Host->PlaceBuildings({Spec}).IsSuccess());
 TestEqual(TEXT("Rejected payment preserves treasury"),Money(),int64(49999));
 TestTrue(TEXT("Rejected payment creates no station"),Host->BuildProjection().Value.GetTradeStations().IsEmpty());

 Host=TStrongObjectPtr<UHansaRuntimeSimulationHost>(NewObject<UHansaRuntimeSimulationHost>());if(!Host->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
 if(!TestTrue(TEXT("Prepare destination cargo"),Hansa::Tests::PrepareEstablishment(*Host,Error,500000,[&](FHansaSimulationInitialization& Init){
  Init.Inventories[0].InitialStock={{FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,FHansaQuantity::FromRaw(8000)},{FHansaGoodId::TryParse(TEXT("Good.Planks")).Value,FHansaQuantity::FromRaw(4000)}};
  for(auto& Presence:Init.ForeignPresences)if(Presence.HouseId==Host->GetHouseId())Presence.Contributions={1000000,100,1000000,1000000,1000000,100,100};
 }))){AddError(Error);return false;}
 TestTrue(TEXT("Place second fixture"),Host->PlaceBuildings({Spec}).IsSuccess());P=Host->BuildProjection();const auto CompleteId=P.Value.GetTradeStations()[0].Station.Id;
 TestTrue(TEXT("Assign berthed cargo"),Host->FundTradeStation(CompleteId,FHansaInventoryId::TryCreate(1).Value).IsSuccess());Host->AdvanceTicks(1);P=Host->BuildProjection();
 TestEqual(TEXT("Local materials start timer"),P.Value.GetTradeStations()[0].Station.Status,EHansaTradeStationStatus::UnderConstruction);
 TestEqual(TEXT("Both required goods secured"),P.Value.GetTradeStations()[0].Station.SpentGoods.Num(),2);
 TestTrue(TEXT("Save in-progress building"),Host->CaptureSaveBytes(Bytes,TEXT("In progress"),TEXT("2026-09-29T00:00:00Z")).IsSuccess());TestTrue(TEXT("Restore in-progress building"),Host->RestoreSaveBytes(Bytes).IsSuccess());Host->AdvanceTicks(4);P=Host->BuildProjection();
 TestEqual(TEXT("Construction completes"),P.Value.GetTradeStations()[0].Station.Status,EHansaTradeStationStatus::Active);
 TestTrue(TEXT("Footprint protected from ordinary warehouse"),!Host->ValidatePlacement({Spec.CityId,FHansaBuildingTypeId::TryParse(TEXT("Building.Warehouse")).Value,Spec.Anchor,EHansaGridRotation::North}).CanPlace());
 const FString Office=TEXT("PresenceStage.MerchantOffice");
 TestTrue(TEXT("Review office upgrade"),Host->RequestPresenceUpgrade({Spec.CityId,Office}).IsSuccess());
 const int64 BeforeUpgrade=Money();TestTrue(TEXT("Pay office without materials"),Host->FundPresenceUpgrade({Spec.CityId,Office,FHansaInventoryId::TryCreate(1).Value}).IsSuccess());
 TestEqual(TEXT("Upgrade payment plus one normal upkeep tick"),BeforeUpgrade-Money(),int64(150000)+P.Value.GetTradeStations()[0].Station.UpkeepPfennigPerTick);Host->AdvanceTicks(2);P=Host->BuildProjection();
 const auto* Presence=P.Value.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Host->GetHouseId()&&X.CityId==Spec.CityId;});
 TestEqual(TEXT("Upgrade waits for materials"),Presence->Upgrade.Status,EHansaPresenceUpgradeStatus::AwaitingMaterials);TestEqual(TEXT("Station stays active during upgrade"),P.Value.GetTradeStations()[0].Station.Status,EHansaTradeStationStatus::Active);
 TestTrue(TEXT("Save waiting upgrade"),Host->CaptureSaveBytes(Bytes,TEXT("Upgrade waiting"),TEXT("2026-09-29T00:00:00Z")).IsSuccess());TestTrue(TEXT("Restore waiting upgrade"),Host->RestoreSaveBytes(Bytes).IsSuccess());
 const int64 BeforeCancelUpgrade=Money();TestTrue(TEXT("Closure cancels waiting upgrade"),Host->CloseTradeStation(CompleteId).IsSuccess());TestEqual(TEXT("Upgrade refund uses site terms"),Money()-BeforeCancelUpgrade,int64(75000));Host->AdvanceTicks(6);P=Host->BuildProjection();
 const auto* ClosedPresence=P.Value.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Host->GetHouseId()&&X.CityId==Spec.CityId;});TestEqual(TEXT("Cancelled upgrade never completes"),ClosedPresence->Upgrade.Status,EHansaPresenceUpgradeStatus::None);

 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPlacedPresenceRoute,"Hansa.UI.TradeMap.Establishment.PlacedPresenceRouteAuthority",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaPlacedPresenceRoute::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,200000,[&](FHansaSimulationInitialization& Init){
  for(auto& Presence:Init.ForeignPresences)Presence.Contributions={};
  const auto& V=Init.Vehicles[1];FHansaRouteState R;R.Id=FHansaRouteId::TryCreate(1).Value;R.OwnerId=V.OwnerId;R.VehicleId=V.Id;R.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;R.Mode=EHansaRouteMode::Sea;
  FHansaRouteCargoAction Sell;Sell.Kind=EHansaRouteCargoActionKind::Unload;Sell.GoodId=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value;Sell.QuantityLimit=FHansaQuantity::FromRaw(12000);Sell.CargoSlotIndex=0;
  R.Stops={{V.CurrentCityId,{}},{FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,{Sell}}};R.CurrentStopIndex=0;R.NextStopIndex=1;R.Lifecycle=EHansaRouteLifecycleState::Traveling;R.RemainingTravelTicks=8;R.TotalTravelTicks=8;Init.Routes.Add(R);
 })){AddError(Error);return false;}
 FHansaPlacementSpec Spec;Spec.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.TradeHouse")).Value;
 bool Found=false;for(int32 X=8;X<19&&!Found;++X)for(int32 Y=8;Y<19;++Y){Spec.Anchor={X,Y};if(Host->TradeHousePlacementError(Spec).IsEmpty()){Found=true;break;}}if(!Found)return false;
 FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;Interest.CityIds.Add(TEXT("City.Rostock"));if(!Authority.RegisterAdmittedClient({947,FHansaParticipantId::TryCreate(1947).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
 FHansaClientProjectionSnapshot Preview;TestTrue(TEXT("Receive permissions before remote placement"),Authority.BuildProjection(947,0,true,Preview,Error));
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> ClientMenu(NewObject<UHansaBuildMenuPresentationModel>());if(!ClientMenu->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
 ClientMenu->RemotePlacementProjection=[&](){return &Preview;};TestTrue(TEXT("Client can select trade house"),ClientMenu->SelectBuilding(TEXT("Building.TradeHouse")));ClientMenu->TargetGridCell(Spec.Anchor.X,Spec.Anchor.Y);TestTrue(TEXT("Unqualified client can preview paid placement"),ClientMenu->GetSnapshot().bCanConfirm);if(!ClientMenu->GetSnapshot().bCanConfirm)AddError(ClientMenu->GetSnapshot().ValidationCause.ToString());
 const int64 RemoteMoney=Preview.OwnerMoneyPfennig;Preview.OwnerMoneyPfennig=0;ClientMenu->TargetGridCell(Spec.Anchor.X,Spec.Anchor.Y);TestFalse(TEXT("Remote preview still requires payment"),ClientMenu->GetSnapshot().bCanConfirm);Preview.OwnerMoneyPfennig=RemoteMoney;
 ClientMenu->TargetGridCell(-999,-999);TestFalse(TEXT("Remote preview still rejects invalid geometry"),ClientMenu->GetSnapshot().bCanConfirm);ClientMenu->TargetGridCell(Spec.Anchor.X,Spec.Anchor.Y);
 FHansaClientCommandIntent Intent;Intent.Type=EHansaClientIntentType::PlaceBuilding;Intent.CityId=TEXT("City.Rostock");Intent.BuildingDefinitionId=TEXT("Building.TradeHouse");Intent.AnchorX=Spec.Anchor.X;Intent.AnchorY=Spec.Anchor.Y;Intent.ExpectedServerTick=Host->BuildProjection().Value.GetClock().GetTick().GetValue();Intent.ClientSequence=Authority.GetExpectedClientSequence(947);Intent.ClientNonce=1;
 TestTrue(TEXT("Remote placement accepted"),Authority.SubmitIntent(947,Intent).bAccepted);
 auto P=Host->BuildProjection();if(P.Value.GetTradeStations().IsEmpty())return false;const auto Id=P.Value.GetTradeStations()[0].Station.Id;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(P.Value,*Host->GetEconomicRegistry());
 TestTrue(TEXT("Open paid site"),Model->OpenWorldStation(TEXT("City.Rostock"),Id.GetValue()));
 TestTrue(TEXT("Select sailing ship for review"),Model->SelectEstablishmentSource(TEXT("2")));
 TestTrue(TEXT("Selection reports unsubmitted assignment"),Model->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("selected for review only")));
 TestTrue(TEXT("Sailing source names its destination"),Model->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("In transit to Rostock")));
 TestTrue(TEXT("Review supply assignment"),Model->EstablishmentIntent(TEXT("Review")));
 TestTrue(TEXT("Cancel review"),Model->EstablishmentIntent(TEXT("Cancel")));
 P=Host->BuildProjection();TestEqual(TEXT("Cancelled review keeps default recovery inventory"),P.Value.GetTradeStations()[0].Station.FundingInventoryId,P.Value.GetTradeStations()[0].Station.InventoryId);
 TestTrue(TEXT("Review supply assignment again"),Model->EstablishmentIntent(TEXT("Review")));
 TestTrue(TEXT("Confirm supply assignment"),Model->EstablishmentIntent(TEXT("Confirm")));
 P=Host->BuildProjection();TestEqual(TEXT("Authority records assigned ship"),P.Value.GetTradeStations()[0].Station.FundingInventoryId.GetValue(),uint64(2));
 TestTrue(TEXT("Assignment reports confirmation"),Model->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("is assigned to the paid site")));
 TestEqual(TEXT("At sea stays waiting"),P.Value.GetTradeStations()[0].Station.Status,EHansaTradeStationStatus::Proposed);
 FHansaClientProjectionSnapshot Wire;TestTrue(TEXT("Owner receives site pose"),Authority.BuildProjection(947,0,true,Wire,Error));const auto* E=Wire.StationEstablishments.FindByPredicate([&](const auto& V){return V.StationId==int64(Id.GetValue());});TestTrue(TEXT("Replicated paid local site"),E&&E->bLocalDelivery&&E->PlacementAnchor==FIntPoint(Spec.Anchor.X,Spec.Anchor.Y));
 Host->AdvanceTicks(12);P=Host->BuildProjection();const auto& S=P.Value.GetTradeStations()[0].Station;
 TestEqual(TEXT("Active route delivers and completes house"),S.Status,EHansaTradeStationStatus::Active);TestEqual(TEXT("Route cannot sell required construction timber"),S.SpentGoods.Num(),2);
 TestTrue(TEXT("Route continues without manual pause"),P.Value.GetRoutes()[0].Lifecycle!=EHansaRouteLifecycleState::Inactive);
 return !HasAnyErrors();
}
#endif
