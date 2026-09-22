#pragma once

#include "Containers/ArrayView.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Events/HansaDomainEvent.h"
#include "Inventory/HansaInventory.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	class FHansaTradeExecutor final
	{
	public:
		[[nodiscard]] static EHansaRoutePlanError ValidatePlan(
			const FHansaVehicleState& Vehicle,
			FHansaRouteDefinitionId RouteDefinitionId,
			TConstArrayView<FHansaRouteStop> Stops,
			TConstArrayView<FHansaCityState> Cities,
			const FHansaInventoryLedger& Inventories,
			const FHansaEconomicRegistry& Registry,
			TConstArrayView<FHansaForeignPresenceState> Presences = {},
			TConstArrayView<FHansaTradeStationState> Stations = {});

		[[nodiscard]] static int32 FindTravelTicks(
			const FHansaCompiledRouteDefinition& Definition,
			FHansaCityDefinitionId Source,
			FHansaCityDefinitionId Destination);

		static void AdvanceOneTick(
			TArray<FHansaRouteState>& Routes,
			TArray<FHansaVehicleState>& Vehicles,
			TArray<FHansaHouseState>& Houses,
			FHansaInventoryLedger& Inventories,
			const FHansaPlacementState& Placement,
			TConstArrayView<FHansaBuildingState> Buildings,
			TConstArrayView<FHansaCityMarketState> Markets,
			TConstArrayView<FHansaHouseResearchState> Research,
			const FHansaEconomicRegistry& Registry,
			FHansaSimulationTick Tick,
			uint64& InOutPublishedEventCount,
			TArray<FHansaDomainEvent>& OutEvents,
			TConstArrayView<FHansaForeignPresenceState> Presences = {},
			TConstArrayView<FHansaTradeStationState> Stations = {});

	private:
		static void PublishRouteEvent(
			FHansaRouteState& Route,
			FHansaVehicleState& Vehicle,
			EHansaDomainEventType Type,
			FHansaSimulationTick Tick,
			FHansaCityDefinitionId CityId,
			FHansaGoodId GoodId,
			EHansaRouteCargoActionKind Kind,
			int64 Value,
			int64 RelatedValue,
			uint64& InOutPublishedEventCount,
			TArray<FHansaDomainEvent>& OutEvents);
	};
}
