#pragma once
#include "Placement/HansaPlacement.h"
#include "Math/Vector.h"
namespace Hansa::Simulation::RostockPlacement
{
 // Quarter coordinates in centimetres. Shared by authority, editor, cursor and overlays.
 inline constexpr double CellSize = 400, OriginX = -4700, OriginY = -8200, WorldOffsetX = 60000;
 inline FVector CellCenter(int32 X, int32 Y, double Z = 100) { return {WorldOffsetX + OriginX + (X+.5)*CellSize, OriginY + (Y+.5)*CellSize, Z}; }
 inline FHansaGridCoordinate WorldToCell(const FVector& P) { return {FMath::FloorToInt32((P.X-WorldOffsetX-OriginX)/CellSize),FMath::FloorToInt32((P.Y-OriginY)/CellSize)}; }
 HANSASIMULATION_API FHansaPlacementMapInitialization CreateMap();
 HANSASIMULATION_API bool IsCanonicalMap(const FHansaPlacementMapInitialization& Map);
}
