#pragma once
#include "Inventory/HansaInventory.h"
#include "Logistics/HansaLocalLogistics.h"
#include "Production/HansaProduction.h"
#include "Trade/HansaTrade.h"

namespace Hansa::UI
{
struct FProductionStockSummary
{
    int64 Building = 0, Markets = 0;
    bool bBuildingKnown = false, bMarketsKnown = false;
};
/** Physical units, including reservations and loaded cargo, without counting queued deliveries twice. */
HANSA_API FProductionStockSummary SummarizeProductionStock(
    const Simulation::FHansaProductionProjection& Production, const Simulation::FHansaGoodId& Good,
    TConstArrayView<Simulation::FHansaInventoryProjection> Inventories,
    TConstArrayView<Simulation::FHansaLogisticsJobProjection> Jobs,
    TConstArrayView<Simulation::FHansaRouteProjection> Routes,
    TConstArrayView<Simulation::FHansaVehicleProjection> Vehicles);
}
