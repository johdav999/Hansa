#pragma once

#include "Construction/HansaConstruction.h"
#include "Events/HansaDomainEvent.h"

namespace Hansa::Simulation
{
	struct FHansaBuildingState;
	struct FHansaHouseState;
	class FHansaEconomicRegistry;
	class FHansaInventoryLedger;
	class FHansaPlacementState;

	class FHansaConstructionExecutor final
	{
	public:
		[[nodiscard]] static FHansaConstructionCostProjection BuildCostProjection(
			const TArray<FHansaHouseState>& Houses,
			const FHansaInventoryLedger& Inventories,
			const FHansaEconomicRegistry& Definitions,
			FHansaHouseId HouseId,
			FHansaCityDefinitionId CityId,
			FHansaBuildingTypeId BuildingDefinitionId);

		[[nodiscard]] static bool TryPayCost(
			TArray<FHansaHouseState>& Houses,
			FHansaInventoryLedger& Inventories,
			const FHansaEconomicRegistry& Definitions,
			FHansaHouseId HouseId,
			FHansaCityDefinitionId CityId,
			FHansaBuildingTypeId BuildingDefinitionId,
			FHansaSimulationTick Tick);

		[[nodiscard]] static bool TryRefundCancellation(
			TArray<FHansaHouseState>& Houses,
			FHansaInventoryLedger& Inventories,
			const FHansaEconomicRegistry& Definitions,
			const FHansaBuildingState& Building,
			FHansaCityDefinitionId CityId,
			FHansaSimulationTick Tick,
			FHansaMoney& OutCurrencyRefund);

		static void AdvanceOneTick(
			TArray<FHansaBuildingState>& Buildings,
			const FHansaEconomicRegistry& Definitions,
			FHansaSimulationTick Tick,
			uint64& InOutPublishedEventCount,
			TArray<FHansaDomainEvent>& OutEvents);
	};
}
