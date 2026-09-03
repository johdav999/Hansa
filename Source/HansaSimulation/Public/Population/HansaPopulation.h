#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"

namespace Hansa::Simulation
{
	enum class EHansaPopulationTrend : uint8
	{
		Declining = 0,
		Stable,
		Growing
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaPopulationTrend Trend);

	struct HANSASIMULATION_API FHansaPopulationCohortInitialization final
	{
		FHansaPopulationCohortId Id;
		FHansaBuildingId ResidenceBuildingId;
		FHansaCityDefinitionId CityId;
		FHansaInventoryId ConsumptionInventoryId;
		FHansaPopulationTierId TierId;
		int32 Residents = 0;
		int32 ResidenceCapacity = 0;
		int32 PurchasingPowerBasisPoints = 10000;
		int32 ServiceAccessBasisPoints = 10000;
		int32 ServiceReliabilityBasisPoints = 10000;
	};

	struct HANSASIMULATION_API FHansaPopulationNeedState final
	{
		FHansaNeedId NeedId;
		FHansaGoodId GoodId;
		FHansaQuantity RequiredLastTick;
		FHansaQuantity ConsumedLastTick;
		int32 AccessBasisPoints = 0;
		int32 AffordabilityBasisPoints = 0;
		int32 ReliabilityBasisPoints = 0;
		int32 SatisfactionBasisPoints = 0;
		int64 ReserveMilliDays = 0;
	};

	struct HANSASIMULATION_API FHansaPopulationCohortState final
	{
		FHansaPopulationCohortId Id;
		FHansaBuildingId ResidenceBuildingId;
		FHansaCityDefinitionId CityId;
		FHansaInventoryId ConsumptionInventoryId;
		FHansaPopulationTierId TierId;
		int32 Residents = 0;
		int32 ResidenceCapacity = 0;
		int32 PurchasingPowerBasisPoints = 10000;
		int32 ServiceAccessBasisPoints = 10000;
		int32 ServiceReliabilityBasisPoints = 10000;
		bool bResidenceOperational = false;
		bool bHasMarketAccess = false;
		int32 AccessBasisPoints = 0;
		int32 AffordabilityBasisPoints = 0;
		int32 ReliabilityBasisPoints = 0;
		int32 SatisfactionBasisPoints = 0;
		int32 WorkforceSupply = 0;
		int32 ConsecutiveGrowthTicks = 0;
		int32 ConsecutiveDeclineTicks = 0;
		int32 ResidentChangeLastTick = 0;
		TArray<FHansaPopulationNeedState> Needs;
	};

	/** Owning, explainable UI/automation view of one residence cohort. */
	struct HANSASIMULATION_API FHansaPopulationCohortProjection final
	{
		FHansaPopulationCohortId Id;
		FHansaBuildingId ResidenceBuildingId;
		FHansaCityDefinitionId CityId;
		FHansaInventoryId ConsumptionInventoryId;
		FHansaPopulationTierId TierId;
		int32 Residents = 0;
		int32 ResidenceCapacity = 0;
		bool bResidenceOperational = false;
		bool bHasMarketAccess = false;
		int32 WorkforceSupply = 0;
		int32 AccessBasisPoints = 0;
		int32 AffordabilityBasisPoints = 0;
		int32 ReliabilityBasisPoints = 0;
		int32 SatisfactionBasisPoints = 0;
		int32 ResidentChangeLastTick = 0;
		TArray<FHansaPopulationNeedState> Needs;
	};

	/** City-level population loop summary consumed directly by HUD, automation and diagnostics. */
	struct HANSASIMULATION_API FHansaCityPopulationProjection final
	{
		FHansaCityDefinitionId CityId;
		int32 TotalResidents = 0;
		int32 ResidentChangeLastTick = 0;
		EHansaPopulationTrend Trend = EHansaPopulationTrend::Stable;
		int32 HousingCapacity = 0;
		int32 LaborerResidents = 0;
		int32 ArtisanResidents = 0;
		int32 LaborerWorkforceSupply = 0;
		int32 LaborerWorkforceAssigned = 0;
		int32 LaborerWorkforceAvailable = 0;
		int32 ArtisanWorkforceSupply = 0;
		int32 ArtisanWorkforceAssigned = 0;
		int32 ArtisanWorkforceAvailable = 0;
		int32 SatisfactionBasisPoints = 0;
		int64 StapleReserveMilliDays = 0;
		bool bHasMarketAccess = false;
	};

	class HANSASIMULATION_API FHansaPopulationSnapshot final
	{
	public:
		[[nodiscard]] TConstArrayView<FHansaPopulationCohortState> GetCohorts() const { return Cohorts; }

	private:
		friend class FHansaSimulationReadOnlyAccess;
		TArray<FHansaPopulationCohortState> Cohorts;
	};
}
