#pragma once

#include "Containers/Array.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	enum class EHansaConstructionState : uint8
	{
		UnderConstruction = 0,
		Completed
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaConstructionState State);

	struct HANSASIMULATION_API FHansaConstructionResourceCostProjection final
	{
		FHansaGoodId GoodId;
		FHansaQuantity Required;
		FHansaQuantity Available;
		FHansaQuantity Missing;
	};

	/** Read-only affordability result used by placement UI, command rejection and automation. */
	struct HANSASIMULATION_API FHansaConstructionCostProjection final
	{
		FHansaHouseId HouseId;
		FHansaCityDefinitionId CityId;
		FHansaBuildingTypeId BuildingDefinitionId;
		FHansaMoney RequiredCurrency;
		FHansaMoney AvailableCurrency;
		FHansaMoney MissingCurrency;
		TArray<FHansaConstructionResourceCostProjection> Resources;

		[[nodiscard]] bool IsAffordable() const;
	};

	/** Owning construction read model; no mutable state or inventory container is exposed. */
	struct HANSASIMULATION_API FHansaConstructionProjection final
	{
		FHansaBuildingId BuildingId;
		FHansaHouseId OwnerId;
		FHansaCityDefinitionId CityId;
		FHansaBuildingTypeId BuildingDefinitionId;
		EHansaConstructionState State = EHansaConstructionState::UnderConstruction;
		FHansaSimulationTick StartedTick;
		int32 ElapsedTicks = 0;
		int32 TotalTicks = 0;
		FHansaRate Progress;
		FHansaMoney PaidCurrency;
		FHansaMoney CancellationCurrencyRefund;
		TArray<FHansaConstructionResourceCostProjection> PaidResources;
		TArray<FHansaConstructionResourceCostProjection> CancellationResourceRefunds;
	};
}
