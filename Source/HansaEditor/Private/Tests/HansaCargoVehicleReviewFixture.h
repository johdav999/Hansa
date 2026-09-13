#pragma once
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"

namespace Hansa::Editor::VehicleReview
{
/** A real inventory/road/dispatch fixture; presentation never synthesizes a job projection. */
inline bool FindInTransitJob(Hansa::Simulation::FHansaLogisticsJobProjection& OutJob)
{
    using namespace Hansa::Simulation;
    const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
    const auto House=FHansaHouseId::TryCreate(1).Value;
    FHansaSimulationInitialization Init;
    Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value).Value;
    Init.CampaignSeed=19;Init.Houses={{House,FHansaMoney::FromRaw(100000)}};Init.Cities={{City,{}}};
    FHansaPlacementMapInitialization Map;Map.CityId=City;Map.BoundsMin={0,0};Map.BoundsMax={4,1};
    Map.RoadBuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
    for(int32 X=0;X<=4;++X)for(int32 Y=0;Y<=1;++Y)Map.Cells.Add({{X,Y},EHansaPlacementTerrain::Land,House,false});
    Init.Placement.Maps.Add(Map);
    const auto AddBuilding=[&](uint64 Id,const TCHAR* Name,int32 X,int32 Y)
    {
        FHansaBuildingState Building;Building.Id=FHansaBuildingId::TryCreate(Id).Value;
        Building.OwnerId=House;Building.DefinitionId=FHansaBuildingTypeId::TryParse(Name).Value;
        Building.ConstructionProgress=FHansaRate::TryMakeNormalized(FHansaRate::Scale).Value;
        Building.ConstructionState=EHansaConstructionState::Completed;Init.Buildings.Add(Building);
        FHansaPlacedBuildingRecord Placement;Placement.BuildingId=Building.Id;Placement.OwnerId=House;
        Placement.Spec.CityId=City;Placement.Spec.BuildingDefinitionId=Building.DefinitionId;Placement.Spec.Anchor={X,Y};
        Placement.OccupiedCells={{X,Y}};Init.Placement.Placements.Add(Placement);
    };
    AddBuilding(1,TEXT("Building.Warehouse"),0,1);AddBuilding(2,TEXT("Building.Dock"),4,1);
    for(int32 X=0;X<=4;++X)AddBuilding(10+X,TEXT("Building.Road"),X,0);
    for(uint64 Id:{1ULL,2ULL})
    {
        FHansaInventoryInitialization Inventory;Inventory.Id=FHansaInventoryId::TryCreate(Id).Value;
        Inventory.BuildingId=FHansaBuildingId::TryCreate(Id).Value;Inventory.OwnerKind=EHansaInventoryOwnerKind::Warehouse;
        Inventory.Capacity=FHansaQuantity::FromRaw(10000);Inventory.AcceptedGoods={Grain};
        if(Id==1)Inventory.InitialStock={{Grain,FHansaQuantity::FromRaw(5000)}};
        Init.Inventories.Add(Inventory);
    }
    FHansaLogisticsRequestInitialization Request;Request.Id=FHansaLogisticsRequestId::TryCreate(19).Value;
    Request.SourceInventoryId=Init.Inventories[0].Id;Request.DestinationInventoryId=Init.Inventories[1].Id;
    Request.GoodId=Grain;Request.Quantity=FHansaQuantity::FromRaw(1000);Init.LocalLogisticsRequests={Request};
    FHansaCompiledGoodDefinition Good;Good.StableId=TEXT("Good.Grain");
    const auto Definitions=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.VehicleReview")).Value,19,FHansaEconomicRegistry({Good},{},{},19));
    auto State=FHansaSimulationState::TryCreate(MoveTemp(Init));if(!Definitions||!State)return false;
    FHansaSimulationTransientCache Cache;
    for(int32 Tick=0;Tick<8;++Tick)
    {
        if(!FHansaGameplayCommandGateway::ExecuteTick(State.Value,Definitions.Value,{},Cache).IsSuccess())return false;
        for(const auto& Job:State.Value.CreateReadOnlyAccess(Definitions.Value).BuildLogisticsJobProjection())
            if(Job.Status==EHansaLogisticsJobStatus::InTransit){OutJob=Job;return true;}
    }
    return false;
}
}
