#pragma once

#include "Trade/HansaTrade.h"

namespace Hansa::Simulation
{
    /** Shared editor/runtime navigation over the complete immutable terrain survey.
     * Four-metre cells, one cell per simulation tick, four-metre lateral clearance for the 7.81-metre beam.
     * No actor transforms, streamed collision or camera bounds participate in authority. */
    class HANSASIMULATION_API FHansaWaterNavigation final
    {
    public:
        static bool IsNavigable(const FHansaPlacementMapInitialization& Map, FHansaGridCoordinate Cell);
        static bool FindStart(const FHansaPlacementMapInitialization& Map, FHansaGridCoordinate Near, FHansaGridCoordinate& Out);
        static bool FindPath(const FHansaPlacementMapInitialization& Map, FHansaGridCoordinate From,
            FHansaGridCoordinate To, TArray<FHansaGridCoordinate>& Out);
        static bool Validate(const FHansaVehicleState& Vehicle, const FHansaPlacementState& Placement);
    };
}
