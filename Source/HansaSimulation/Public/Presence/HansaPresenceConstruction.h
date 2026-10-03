#pragma once
#include "Presence/HansaForeignPresence.h"

namespace Hansa::Simulation
{
 /** Native footprint contract for the approved 7.24 x 6.52 m merchant house on four-metre cells. */
 struct HANSASIMULATION_API FHansaPresenceConstructionRules
 {
  static constexpr int32 FootprintCells=2;
  static FHansaPlacementValidationResult ExcludeStations(FHansaPlacementValidationResult Result,FHansaCityDefinitionId City,TConstArrayView<FHansaTradeStationState> Stations)
  {
   for(const auto& S:Stations)if(S.CityId==City&&S.ConstructionSite.bLocalDelivery&&S.Status!=EHansaTradeStationStatus::Closed)
    for(const auto C:Result.GetOccupiedCells())if(C.X>=S.ConstructionSite.Anchor.X&&int64(C.X)<int64(S.ConstructionSite.Anchor.X)+FootprintCells&&C.Y>=S.ConstructionSite.Anchor.Y&&int64(C.Y)<int64(S.ConstructionSite.Anchor.Y)+FootprintCells)return Result.WithOccupiedFailure(C);
   return Result;
  }
  static FString ValidateSite(const FHansaPlacementState& Placement,FHansaCityDefinitionId City,
   FHansaGridCoordinate Min,FHansaGridCoordinate Max,FHansaGridCoordinate Anchor,EHansaGridRotation Rotation,bool bCheckOccupancy=true)
  {
   if(static_cast<uint8>(Rotation)>3||Anchor.X<Min.X||Anchor.Y<Min.Y||int64(Anchor.X)+1>Max.X||int64(Anchor.Y)+1>Max.Y)
    return TEXT("Place the whole trade house inside the commercial lease.");
   for(int32 X=0;X<FootprintCells;++X)for(int32 Y=0;Y<FootprintCells;++Y){
    const FHansaGridCoordinate C{Anchor.X+X,Anchor.Y+Y};const auto* Cell=Placement.FindCell(City,C);
    if(!Cell||Cell->Terrain!=EHansaPlacementTerrain::Land||Cell->bBlocked)return TEXT("Choose clear land away from streets and city buildings.");
    if(bCheckOccupancy)for(const auto& P:Placement.GetPlacements())if(P.Spec.CityId==City&&P.OccupiedCells.Contains(C))return TEXT("Another building occupies this site.");
   }return {};
  }
 };
}
