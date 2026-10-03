#pragma once
#include "HansaTradeEstablishmentTestSupport.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests {
inline bool PrepareVisitingTrade(UHansaRuntimeSimulationHost& Host,FString& Error){
 using namespace Hansa::Simulation;
 return PrepareEstablishment(Host,Error,200000,[&](auto& Init){
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value;
 for(auto& M:Init.Markets){M.InitialLastUpdateTick=0;M.InitialReportTick=0;}
 for(auto& V:Init.Vehicles)if(V.Id.GetValue()==2)V.CurrentCityId=City;
 for(auto& I:Init.Inventories)if(I.OwnerKind==EHansaInventoryOwnerKind::City&&I.CityId==City)I.InitialStock={{Timber,FHansaQuantity::FromRaw(20000)}};
 });
}
}
#endif
