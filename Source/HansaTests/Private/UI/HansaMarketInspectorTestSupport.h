#pragma once
#include "World/HansaRuntimeSimulationHost.h"

/** Construct the tested market through the normal command gateway, without fixture state mutation. */
inline bool EnsureMarketInspectorTestBuilding(UHansaRuntimeSimulationHost* Host)
{
    using namespace Hansa::Simulation;
    auto HasMarket=[&]{
        const auto P=Host->BuildProjection();
        return P && P.Value.GetBuildingWorldProjections().ContainsByPredicate([](const auto& B){
            return B.Placement.BuildingDefinitionId.ToString()==TEXT("Building.Market") && B.Status!=EHansaBuildingWorldStatus::UnderConstruction;
        });
    };
    if(HasMarket())return true;
    const auto* Map=Host->FindPlacementMap();if(!Map)return false;
    FHansaPlacementSpec Spec;Spec.CityId=Host->GetCityId();Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Market")).Value;
    for(const auto& Cell:Map->Cells)
    {
        Spec.Anchor=Cell.Coordinate;
        auto Valid=Host->ValidatePlacement(Spec);
        if(!Valid.CanPlace() && Valid.GetReasons().Num()==1 && Valid.GetPrimaryFailure()==EHansaPlacementFailure::RoadRequired)
        {
            auto Road=Spec;Road.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
            Road.Anchor.X-=1;
            if(!Host->ValidatePlacement(Road).CanPlace())continue;
            TArray<FHansaPlacementSpec> Roads={Road};
            const auto R=Host->PlaceBuildings(Roads);
            if(!R.IsSuccess()){UE_LOG(LogTemp,Warning,TEXT("Market test road rejected: %s"),LexToString(R.GetError()));return false;}
            Valid=Host->ValidatePlacement(Spec);
        }
        if(!Valid.CanPlace())continue;
        TArray<FHansaPlacementSpec> Specs={Spec};
        const auto Placed=Host->PlaceBuildings(Specs);
        if(!Placed.IsSuccess()){UE_LOG(LogTemp,Warning,TEXT("Market test placement rejected: %s"),LexToString(Placed.GetError()));return false;}
        for(int32 Tick=0;Tick<600;++Tick){Host->AdvanceTicks(1);if(HasMarket())return true;}
        return false;
    }
    return false;
}
