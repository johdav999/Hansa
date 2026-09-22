#pragma once
#include "Inventory/HansaInventory.h"
#include "Logistics/HansaLocalLogistics.h"
#include "Production/HansaProduction.h"
#include "Definitions/HansaEconomicRegistry.h"
namespace Hansa::Simulation
{
class FHansaSpoilageExecutor final
{
public:
 static void Advance(FHansaInventoryLedger& Ledger, TArray<FHansaLogisticsJobState>& Jobs,
  TArray<FHansaLogisticsRequestState>& Requests, TArray<FHansaProductionState>& Productions,
  const FHansaEconomicRegistry& Registry, FHansaSimulationTick Tick, uint32 MinutesPerTick);
};
}
