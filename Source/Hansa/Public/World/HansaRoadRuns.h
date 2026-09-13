#pragma once
#include "CoreMinimal.h"

namespace Hansa::Game
{
    /** Canonical visual paths between endpoints/junctions. Closed loops repeat their first cell. */
    HANSA_API TArray<TArray<FIntPoint>> BuildRoadRuns(const TSet<FIntPoint>& Cells);
    HANSA_API TMap<FIntPoint,uint8> RoadRunNeighborMasks(const TArray<TArray<FIntPoint>>& Runs);
}
