#pragma once

#include "Model/HansaSimulationState.h"
#include "Definitions/HansaEconomicRegistry.h"

namespace Hansa::Simulation
{
	struct HANSASIMULATION_API FHansaHeatingProjection
	{
		int32 ReserveDays = 3;
		bool bOverride = false;
		int32 SeasonMultiplier = 0;
		int64 StockRaw = 0;
		int64 CommittedRaw = 0;
		int64 ProtectedRaw = 0;
		int64 SurplusRaw = 0;
		int64 HouseholdDailyRaw = 0;
		int64 WinterDailyRaw = 0;
        int64 WorkshopDailyRaw = 0; // Nominal demand from enabled recipe cycles, before staffing/logistics constraints.
	};

	class HANSASIMULATION_API FHansaHeating final
	{
	public:
		static void RefreshProtection(FHansaInventoryLedger& Ledger, const TArray<FHansaCityState>& Cities,
			const TArray<FHansaPopulationCohortState>& Cohorts, const FHansaEconomicRegistry& Registry,
			const FHansaSimulationClock& Clock);
		static FHansaHeatingProjection Project(FHansaCityDefinitionId CityId, const FHansaInventoryReadOnlyAccess& Inventories,
			TConstArrayView<FHansaCityState> Cities, TConstArrayView<FHansaPopulationCohortState> Cohorts,
			const FHansaEconomicRegistry& Registry, const FHansaSimulationClock& Clock);
	};
}
