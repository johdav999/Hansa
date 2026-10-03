#pragma once
#include "Presence/HansaForeignPresence.h"
#include "Inventory/HansaInventory.h"

namespace Hansa::Simulation
{
 /** One-time hauling uses ledger reservations: cargo remains physical and cannot be sold. */
 struct HANSASIMULATION_API FHansaConstructionDelivery
 {
  static bool Release(FHansaTradeStationState& Station, FHansaInventoryLedger& Ledger, FHansaSimulationTick Tick);
  static void Collect(FHansaTradeStationState& Station, FHansaInventoryLedger& Ledger,
   TConstArrayView<FHansaCompiledPresenceUpgradeGoodCost> Costs, FHansaSimulationTick Tick,
   bool AtDestination, TConstArrayView<FHansaInventoryId> Sources);
  static void Collect(FHansaInventoryId Cargo, TArray<FHansaTradeStationSpentGood>& Delivered,
   TArray<FHansaReservationId>& Reservations, FHansaInventoryLedger& Ledger,
   TConstArrayView<FHansaCompiledPresenceUpgradeGoodCost> Costs, FHansaSimulationTick Tick,
   bool AtDestination, TConstArrayView<FHansaInventoryId> Sources);
 };
}
