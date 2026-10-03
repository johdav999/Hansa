#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeEstablishmentTestSupport.h"
namespace Hansa::Tests {
inline bool PrepareLedger(UHansaRuntimeSimulationHost& Host,FString& Error,Hansa::Simulation::EHansaTradeStationOperationalState Operating=Hansa::Simulation::EHansaTradeStationOperationalState::Active,bool AuthoredShipCapacity=false,bool ReferenceOffice=false,bool ReadyReports=false,TFunction<void(Hansa::Simulation::FHansaSimulationInitialization&)> Configure={}){
 using namespace Hansa::Simulation;
 return PrepareEstablishment(Host,Error,200000,[&](FHansaSimulationInitialization& Init){
  const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;const auto* Registry=Host.GetEconomicRegistry();const auto* Policy=Registry->FindCityTradePolicyForCity(City.ToString());const auto& Site=Policy->TradeStationSites[0];
  FHansaTradeStationState S;S.Id=FHansaTradeStationId::TryCreate(100).Value;S.InventoryId=FHansaInventoryId::TryCreate(100).Value;S.FactorId=FHansaFactorId::TryCreate(100).Value;S.LeasedPlotId=FHansaLeasedPlotId::TryCreate(100).Value;S.OwnerId=Host.GetHouseId();S.CityId=City;S.SiteId=Site.SiteId;S.Status=EHansaTradeStationStatus::Active;S.OperationalState=Operating;S.UpkeepPfennigPerTick=Site.UpkeepPfennigPerTick;
  if(Operating==EHansaTradeStationOperationalState::Underfunded){S.Status=EHansaTradeStationStatus::Suspended;S.OutstandingUpkeepPfennig=25;}
  if(Operating==EHansaTradeStationOperationalState::VoluntarilyClosed)S.Status=EHansaTradeStationStatus::Closed;
  if(Operating==EHansaTradeStationOperationalState::RightsSuspended||Operating==EHansaTradeStationOperationalState::Revoked)S.Status=EHansaTradeStationStatus::Suspended;
  const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
  for(int32 I=1;I<=3;++I){FHansaStationOrderState O;O.Id=I;O.LastCommandId=FHansaCommandId::TryCreate(I).Value;O.Terms.GoodId=I==3?Planks:Timber;O.Terms.Side=I==1?EHansaStationOrderSide::Release:EHansaStationOrderSide::Acquire;O.Terms.TargetOrReserveMilliUnits=I==1?5000:I==2?8000:9000;O.Terms.CapMilliUnits=1000;O.Terms.TotalBudgetPfennig=100000;O.bPaused=I==1;S.Orders.Add(O);}
  FHansaLeasedPlotState Lease;Lease.Id=S.LeasedPlotId;Lease.StationId=S.Id;Lease.OwnerId=S.OwnerId;Lease.CityId=City;Lease.SiteId=Site.SiteId;Lease.PlotCategory=Site.PlotCategory;Lease.BoundsMin=Site.LeaseBoundsMin;Lease.BoundsMax=Site.LeaseBoundsMax;Lease.PermittedBuildingCategories=Site.PermittedBuildingCategories;Lease.bActive=S.Status==EHansaTradeStationStatus::Active;Lease.bOccupied=true;
  FHansaInventoryInitialization Inv;Inv.Id=S.InventoryId;Inv.TradeStationId=S.Id;Inv.OwnerKind=EHansaInventoryOwnerKind::TradeStation;Inv.CityId=City;Inv.Capacity=FHansaQuantity::FromRaw(Site.StorageCapacityMilliUnits);for(const auto& G:Registry->GetGoods())Inv.AcceptedGoods.Add(FHansaGoodId::TryParse(G.StableId).Value);Inv.InitialStock={{Timber,FHansaQuantity::FromRaw(12000)},{Planks,FHansaQuantity::FromRaw(3000)}};
  FHansaRouteState Route;Route.Id=FHansaRouteId::TryCreate(100).Value;Route.OwnerId=S.OwnerId;Route.VehicleId=FHansaVehicleId::TryCreate(1).Value;Route.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;Route.Mode=EHansaRouteMode::Sea;Route.Lifecycle=EHansaRouteLifecycleState::Inactive;
  FHansaRouteCargoAction Load;Load.Kind=EHansaRouteCargoActionKind::StationLoad;Load.GoodId=Planks;Load.QuantityLimit=FHansaQuantity::FromRaw(1000);Load.MinimumSourceReserve=FHansaQuantity::FromRaw(1000);FHansaRouteCargoAction Unload=Load;Unload.Kind=EHansaRouteCargoActionKind::OwnedCityUnload;Unload.MinimumSourceReserve=FHansaQuantity();Route.Stops={{City,{Load}},{FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value,{Unload}}};Init.Routes.Add(Route);
  Init.Inventories.Add(Inv);Init.TradeStations.Add(S);Init.LeasedPlots.Add(Lease);
  if(ReferenceOffice){
   Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(9).Value).Value;
   const auto Charcoal=FHansaGoodId::TryParse(TEXT("Good.Charcoal")).Value;auto* MarketStock=Init.Inventories.FindByPredicate([&](const auto& I){return I.OwnerKind==EHansaInventoryOwnerKind::City&&I.CityId==City;});MarketStock->AcceptedGoods.Add(Charcoal);MarketStock->InitialStock.Add({Charcoal,FHansaQuantity::FromRaw(20000)});
   FHansaCityMarketInitialization Quote;Quote.CityId=City;Quote.GoodId=Charcoal;Quote.InventoryIds={MarketStock->Id};Quote.InitialPriceMilliMarks=1560;Quote.InitialReportTick=1;Quote.InitialLastUpdateTick=1;Init.Markets.Add(Quote);
   for(auto& M:Init.Markets)if(M.InitialReportTick<0)M.InitialReportTick=1;
  }
  if(AuthoredShipCapacity)for(auto& V:Init.Vehicles){V.Capacity=FHansaQuantity::FromRaw(Registry->FindVehicle(V.DefinitionId.ToString())->CargoCapacityMilliUnits);for(auto& I:Init.Inventories)if(I.Id==V.CargoInventoryId)I.Capacity=V.Capacity;}
  if(ReadyReports)for(auto& M:Init.Markets)if(M.InitialReportTick<0)M.InitialReportTick=1;
  for(auto& P:Init.ForeignPresences)if(P.HouseId==S.OwnerId&&P.CityId==City){P.StationId=S.Id;P.LeasedPlotId=S.LeasedPlotId;for(const auto& Stage:Registry->GetPresenceStages())if(Stage.StableId==(ReferenceOffice?TEXT("PresenceStage.MerchantOffice"):TEXT("PresenceStage.TradeStation"))){P.CurrentStageId=Stage.StableId;P.GrantedCapabilityIds=Stage.GrantedCapabilityIds;break;}if(Operating==EHansaTradeStationOperationalState::Revoked)P.Status=EHansaForeignPresenceStatus::Revoked;}
  if(Configure)Configure(Init);
 });
}
}
#endif
