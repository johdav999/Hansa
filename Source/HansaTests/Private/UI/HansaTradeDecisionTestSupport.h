#pragma once
#include "HansaTradeSpecializationTestSupport.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests {
inline bool PrepareDecisions(UHansaRuntimeSimulationHost& Host,FString& Error)
{
 using namespace Hansa::Simulation;
 return PrepareSpecialization(Host,Error,[&](auto& Init){
  const auto* Stage=Host.GetEconomicRegistry()->FindPresenceStage(TEXT("PresenceStage.PrivilegedPresence"));
  for(auto& P:Init.ForeignPresences)if(P.HouseId==Host.GetHouseId()){P.CurrentStageId=Stage->StableId;P.GrantedCapabilityIds=Stage->GrantedCapabilityIds;}
  const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value, Hamburg=FHansaCityDefinitionId::TryParse(TEXT("City.Hamburg")).Value;
  FHansaForeignPresenceState H;H.HouseId=Host.GetHouseId();H.CityId=Hamburg;H.CurrentStageId=Stage->StableId;H.GrantedCapabilityIds=Stage->GrantedCapabilityIds;Init.ForeignPresences.Add(H);
  const auto Bread=FHansaGoodId::TryParse(TEXT("Good.Bread")).Value;
  auto* Inv=Init.Inventories.FindByPredicate([&](const auto& X){return X.OwnerKind==EHansaInventoryOwnerKind::City&&X.CityId==City;});
  Inv->AcceptedGoods.Add(Bread);FHansaCityMarketInitialization Market;Market.CityId=City;Market.GoodId=Bread;Market.InventoryIds={Inv->Id};Market.InitialPriceMilliMarks=1000;Init.Markets.Add(Market);
 });
}
}
#endif
