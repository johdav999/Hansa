#pragma once

#include "Logistics/HansaLocalLogistics.h"

namespace Hansa::Simulation
{
	class FHansaInventoryLedger;
	class FHansaPlacementState;
	class FHansaEconomicRegistry;
	struct FHansaBuildingState;
	struct FHansaProductionState;
	struct FHansaCityMarketState;

	class FHansaLocalLogisticsExecutor final
	{
	public:
		static void SynchronizeProductionRequests(
			TArray<FHansaLogisticsRequestState>& Requests,
			TConstArrayView<FHansaProductionState> Productions,
			TConstArrayView<FHansaCityMarketState> Markets,
			const FHansaEconomicRegistry& Registry,
			const FHansaInventoryLedger& InventoryLedger,
			const FHansaPlacementState& Placement,
			TConstArrayView<FHansaBuildingState> Buildings,
			FHansaSimulationTick CurrentTick);

		static void AdvanceOneTick(
			TArray<FHansaLogisticsRequestState>& Requests,
			TArray<FHansaLogisticsJobState>& Jobs,
			uint64& NextJobValue,
			uint64& NextReservationValue,
			const FHansaLocalLogisticsSettings& Settings,
			FHansaInventoryLedger& InventoryLedger,
			const FHansaPlacementState& Placement,
			TConstArrayView<FHansaBuildingState> Buildings,
			FHansaSimulationTick CurrentTick);
	};
}
