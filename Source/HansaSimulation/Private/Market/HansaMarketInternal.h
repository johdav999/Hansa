#pragma once

#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Market/HansaMarket.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	class FHansaMarketExecutor final
	{
	public:
		static void AdvanceOneTick(TArray<FHansaCityMarketState>& Markets, const FHansaMarketSettings& Settings,
			FHansaInventoryLedger& InventoryLedger, const TArray<FHansaProductionState>& Productions,
			const TArray<FHansaPopulationCohortState>& PopulationCohorts,
			const FHansaEconomicRegistry& Registry, FHansaSimulationTick Tick,
			const TArray<FHansaRouteState>& Routes, const TArray<FHansaVehicleState>& Vehicles,
			TArray<FHansaRemoteIndustryState>& RemoteIndustries,
			TArray<FHansaRegionalShipmentState>& RegionalShipments, uint64& NextRegionalShipmentSequence);
	};
}
