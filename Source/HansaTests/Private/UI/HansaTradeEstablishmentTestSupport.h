#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "World/HansaRuntimeSimulationHost.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Presence/HansaForeignPresenceInitialization.h"
#include "Save/HansaSaveEnvelope.h"
#include "Logistics/HansaLocalLogistics.h"
namespace Hansa::Tests {
// Explicit test-only saved campaign: qualified merchant, first ship empty, second
// ship funded. No production data, provider, authoring schema or runtime seam changes.
inline bool PrepareEstablishment(UHansaRuntimeSimulationHost& Host,FString& Error,int64 Treasury=200000,TFunction<void(Hansa::Simulation::FHansaSimulationInitialization&)> Configure={},bool WithTradeAccess=false) {
 using namespace Hansa::Simulation;
 const auto* R=Host.GetEconomicRegistry();if(!R)return false;
 TArray<uint8> Bytes;if(!Host.CaptureSaveBytes(Bytes,TEXT("TG08 deterministic fixture"),TEXT("2026-09-23T00:00:00Z")))return false;
 FHansaSaveSnapshot Save;int64 Tick=0;if(!Host.InspectSaveBytes(Bytes,Save,Tick))return false;
 const auto InitialContext=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(Save.Scenario.ScenarioId).Value,R->GetRegistryHash(),*R);if(!InitialContext){Error=TEXT("Definition context invalid");return false;}
 const auto Original=Save.State.CreateReadOnlyAccess(InitialContext.Value);
 const auto Context=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(Save.Scenario.ScenarioId).Value,R->GetRegistryHash(),*R,*Original.GetPlacement().GetTopology());if(!Context)return false;
 FHansaSimulationInitialization Init;Init.Placement.Entitlements.Append(Original.GetPlacement().GetEntitlements());Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(1).Value).Value;Init.CampaignSeed=808;
 for(const auto& Player:Save.Players)Init.Houses.Add({Player.HouseId,FHansaMoney::FromRaw(Treasury)});
 const auto Rostock=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;const auto Lubeck=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 for(const auto& C:Original.GetCities())Init.Cities.Add(C);
 const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
 for(int32 I=1;I<=3;++I){FHansaVehicleState V;V.Id=FHansaVehicleId::TryCreate(I).Value;V.DefinitionId=FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;V.OwnerId=I==3?Host.GetRivalHouseId():Host.GetHouseId();V.CargoInventoryId=FHansaInventoryId::TryCreate(I).Value;V.Mode=EHansaRouteMode::Sea;V.Capacity=FHansaQuantity::FromRaw(30000);V.CurrentCityId=I==2?Lubeck:Rostock;Init.Vehicles.Add(V);
 FHansaInventoryInitialization Inv;Inv.Id=V.CargoInventoryId;Inv.OwnerKind=EHansaInventoryOwnerKind::Vehicle;Inv.VehicleId=V.Id;Inv.Capacity=V.Capacity;Inv.AcceptedGoods={Timber,Planks};if(I!=1)Inv.InitialStock={{Timber,FHansaQuantity::FromRaw(12000)},{Planks,FHansaQuantity::FromRaw(6000)}};Init.Inventories.Add(Inv);}
 int32 MarketId=10;for(const auto City:{Lubeck,Rostock}){FHansaInventoryInitialization Inv;Inv.Id=FHansaInventoryId::TryCreate(MarketId++).Value;Inv.OwnerKind=EHansaInventoryOwnerKind::City;Inv.CityId=City;Inv.Capacity=FHansaQuantity::FromRaw(100000);Inv.AcceptedGoods={Timber};Init.Inventories.Add(Inv);FHansaCityMarketInitialization Market;Market.CityId=City;Market.GoodId=Timber;Market.InventoryIds={Inv.Id};Market.InitialPriceMilliMarks=1000;Init.Markets.Add(Market);}
 if(!FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(Init,*R))return false;
 for(auto& Presence:Init.ForeignPresences)if(Presence.HouseId==Host.GetHouseId()&&Presence.CityId==Rostock)Presence.Contributions={50000,3,0,50000,10000,3,3};
 if(WithTradeAccess)for(const auto& B:Original.GetBuildings())if(B.ConstructionState==EHansaConstructionState::Completed&&(B.DefinitionId.ToString()==TEXT("Building.Market")||B.DefinitionId.ToString()==TEXT("Building.Dock")||B.DefinitionId.ToString()==TEXT("Building.Road"))){const auto* Placement=Original.GetPlacement().FindPlacement(B.Id);if(Placement){Init.Buildings.Add(B);Init.Placement.Placements.Add(*Placement);}}
 if(Configure)Configure(Init);
 auto State=FHansaSimulationState::TryCreate(MoveTemp(Init),Context.Value.GetPlacementTopologyShared());if(!State){Error=FString(TEXT("Fixture state invalid: "))+LexToString(State.Error);return false;}
 Save.State=MoveTemp(State.Value);Save.NextCommandId=1;Save.NextBuildingId=WithTradeAccess?Save.NextBuildingId:1;for(const auto& B:Save.State.CreateReadOnlyAccess(Context.Value).GetBuildings())Save.NextBuildingId=FMath::Max(Save.NextBuildingId,B.Id.GetValue()+1);Save.RouteLabels.Reset();Save.PendingCommands.Reset();
 const auto Encoded=FHansaSaveEnvelope::Encode(Save,Context.Value,Bytes);if(!Encoded){Error=Encoded.Message;return false;}
 const auto Restored=Host.RestoreSaveBytes(Bytes);Error=Restored.Message;return Restored.IsSuccess();
}
}
#endif
