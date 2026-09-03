#pragma once

#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Market/HansaMarket.h"
#include "Placement/HansaPlacement.h"
#include "Population/HansaPopulation.h"
#include "Production/HansaProduction.h"

namespace Hansa::Simulation
{
	struct FHansaBuildingState;

	class FHansaPopulationExecutor final
	{
	public:
		static void SynchronizeResidencesAndAssignWorkforce(
			TArray<FHansaPopulationCohortState>& Cohorts,
			TArray<FHansaProductionState>& Productions,
			const TArray<FHansaBuildingState>& Buildings,
			const FHansaPlacementState& Placement,
			const FHansaInventoryLedger& InventoryLedger,
			const FHansaEconomicRegistry& Registry);

		static void AdvanceOneTick(TArray<FHansaPopulationCohortState>& Cohorts,
			FHansaInventoryLedger& InventoryLedger, const TArray<FHansaCityMarketState>& Markets,
			const TArray<FHansaBuildingState>& Buildings, const FHansaEconomicRegistry& Registry,
			FHansaSimulationTick Tick, uint32 MinutesPerTick);
	};
}
