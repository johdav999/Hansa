#pragma once
#include "HansaTradeEstablishmentTestSupport.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests {
inline bool PrepareSpecialization(UHansaRuntimeSimulationHost& Host,FString& Error,TFunction<void(Hansa::Simulation::FHansaSimulationInitialization&)> Configure={}){
 using namespace Hansa::Simulation;
 const auto* R=Host.GetEconomicRegistry();const auto House=Host.GetHouseId(),Rival=Host.GetRivalHouseId();
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 return PrepareEstablishment(Host,Error,500000,[&](FHansaSimulationInitialization& Init){
  const auto StationId=FHansaTradeStationId::TryCreate(1).Value;const auto Inv=FHansaInventoryId::TryCreate(20).Value;const auto LeaseId=FHansaLeasedPlotId::TryCreate(1).Value;
  const auto* Stage=R->FindPresenceStage(TEXT("PresenceStage.MerchantOffice"));
  for(auto& P:Init.ForeignPresences)if(P.HouseId==House&&P.CityId==City){P.CurrentStageId=Stage->StableId;P.GrantedCapabilityIds=Stage->GrantedCapabilityIds;P.GrantedCapabilityIds.Sort();P.StationId=StationId;P.LeasedPlotId=LeaseId;}
  FHansaTradeStationState S;S.Id=StationId;S.OwnerId=House;S.CityId=City;S.SiteId=TEXT("TradeStationSite.Rostock.Harbor.01");S.InventoryId=Inv;S.FactorId=FHansaFactorId::TryCreate(1).Value;S.LeasedPlotId=LeaseId;S.Status=EHansaTradeStationStatus::Active;S.UpkeepPfennigPerTick=25;Init.TradeStations.Add(S);
  FHansaLeasedPlotState L;L.Id=LeaseId;L.StationId=StationId;L.OwnerId=House;L.CityId=City;L.SiteId=S.SiteId;L.PlotCategory=TEXT("Commercial");L.BoundsMin={8,8};L.BoundsMax={19,19};L.PermittedBuildingCategories={TEXT("Storage"),TEXT("Commercial"),TEXT("Production")};L.bActive=true;L.bOccupied=true;Init.LeasedPlots.Add(L);
  FHansaInventoryInitialization Storage;Storage.Id=Inv;Storage.OwnerKind=EHansaInventoryOwnerKind::TradeStation;Storage.CityId=City;Storage.TradeStationId=StationId;Storage.Capacity=FHansaQuantity::FromRaw(500000);
  for(const auto& B:R->FindCityTradePolicyForCity(City.ToString())->Specializations)for(const auto& G:B.InvestmentGoods){const auto Good=FHansaGoodId::TryParse(G.GoodId).Value;if(!Storage.AcceptedGoods.Contains(Good)){Storage.AcceptedGoods.Add(Good);Storage.InitialStock.Add({Good,FHansaQuantity::FromRaw(100000)});}}
  Init.Inventories.Add(Storage);if(Configure)Configure(Init);
 });
}
}
#endif
