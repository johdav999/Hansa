#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace Hansa::Game::TerrainPlacement
{
    /** Landscape collision or an explicitly tagged Hansa.Terrain surface; excludes roofs and water. */
    HANSA_API bool Trace(const UWorld* World, const FVector& Start, const FVector& End, FHitResult& Hit);
    HANSA_API FQuat RoadRotation(const UWorld* World, const FVector& Position, const FQuat& Heading);
    /** Preserve the authored offset above a local ground datum. Missing terrain preserves legacy placement. */
    HANSA_API FVector Ground(const UWorld* World, const FVector& Position, double DatumZ);
}
