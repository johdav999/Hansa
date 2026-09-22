#pragma once

#include "Definitions/HansaEconomicRegistry.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	class HANSASIMULATION_API FHansaForeignPresenceInitialization final
	{
	public:
		/** Adds only explicitly authored initial stages. Existing records are never overwritten. */
		static bool SeedAuthoredInitialPresence(FHansaSimulationInitialization& Initialization,
			const FHansaEconomicRegistry& Registry);
	};
}
