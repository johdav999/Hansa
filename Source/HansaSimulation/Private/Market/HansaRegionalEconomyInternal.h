#pragma once

#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Market/HansaMarket.h"
#include "Market/HansaRegionalEconomy.h"

namespace Hansa::Simulation
{
	class FHansaRegionalEconomyExecutor final
	{
	public:
		static void AdvanceMarketUpdate(TArray<FHansaRemoteIndustryState>& Industries,
			TArray<FHansaRegionalShipmentState>& Shipments, uint64& NextShipmentSequence,
			TArray<FHansaCityMarketState>& Markets, FHansaInventoryLedger& Inventories,
			const FHansaEconomicRegistry& Registry, FHansaSimulationTick Tick);
	};
}
