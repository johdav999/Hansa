#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
    struct FHansaPopulationCohortState;

    /** City-wide citizen consumption, in milli-units. Services and industrial demand are excluded. */
    struct FHansaConsumptionTotal
    {
        FHansaCityDefinitionId CityId;
        FHansaGoodId GoodId;
        int64 Required = 0;
        int64 Consumed = 0;
    };

    struct FHansaConsumptionSample
    {
        int64 EndTick = 0;
        TArray<FHansaConsumptionTotal> Goods;
    };

    /** Owning read model shared by game presentation and automation. Zero coverage means pending. */
    struct FHansaConsumptionProjection
    {
        int64 CoveredMinutes = 0;
        bool bFullWindow = false;
        TArray<FHansaConsumptionTotal> Goods;
    };

    class HANSASIMULATION_API FHansaConsumptionHistory final
    {
    public:
        static constexpr int64 WindowMinutes = 30 * 24 * 60;
        // Whole consumption ticks only: never invent fractional consumption at a window boundary.
        static int32 WindowTicks(uint16 MinutesPerTick) { return int32(WindowMinutes / MinutesPerTick); }
        bool Record(TConstArrayView<FHansaPopulationCohortState> Cohorts, const FHansaSimulationClock& Clock);
        bool Validate(const FHansaSimulationClock& Clock) const;
        FHansaConsumptionProjection Project(const FHansaSimulationClock& Clock) const;
        /** Most recent completed ticks, used for migration smoothing; no state mutation. */
        TArray<FHansaConsumptionTotal> RecentTotals(int32 TickCount) const;

    private:
        friend class FHansaSaveCodec;
        friend class FHansaStateHasher;
        TArray<FHansaConsumptionSample> Samples;
    };
}
