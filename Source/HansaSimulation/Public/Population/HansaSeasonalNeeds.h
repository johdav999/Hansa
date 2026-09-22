#pragma once

#include "Definitions/HansaEconomicRegistry.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	/** Calendar-derived demand. Season indices: summer, autumn, winter, spring. */
	class HANSASIMULATION_API FHansaSeasonalNeeds final
	{
	public:
		static int32 Multiplier(const FHansaCompiledNeedDefinition& Need, FHansaSimulationTick Tick, uint32 MinutesPerTick);
		static int64 Demand(const FHansaCompiledNeedDefinition& Need, int32 Rate, int32 Residents,
			FHansaSimulationTick Tick, uint32 MinutesPerTick);
	};
}
