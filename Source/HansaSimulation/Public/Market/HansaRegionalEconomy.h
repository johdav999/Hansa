#pragma once

#include "Containers/Array.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	enum class EHansaRemoteIndustryBlocker : uint8
	{
		None,
		MissingInput,
		OutputReserveReached,
		SourceCapacityReached,
		NoInventory,
		Disabled
	};

	struct HANSASIMULATION_API FHansaRemoteIndustryState final
	{
		FHansaCityDefinitionId CityId;
		FString ProductionChainId;
		FString StageKey;
		FString RecipeId;
		int64 CompletedCycles = 0;
		EHansaRemoteIndustryBlocker Blocker = EHansaRemoteIndustryBlocker::None;
		FHansaGoodId BlockingGoodId;
		FHansaQuantity BlockingRequired;
		FHansaQuantity BlockingAvailable;
		FHansaQuantity LastProduced;
		FHansaSimulationTick LastUpdateTick;
	};

	struct HANSASIMULATION_API FHansaRegionalShipmentState final
	{
		uint64 Sequence = 0;
		FString RegionId;
		FHansaCityDefinitionId SourceCityId;
		FHansaCityDefinitionId DestinationCityId;
		FHansaGoodId GoodId;
		FHansaQuantity CommittedQuantity;
		FHansaQuantity DeliverableQuantity;
		FHansaSimulationTick DispatchTick;
		FHansaSimulationTick DeliveryTick;
		int64 TransportCostMilliMarks = 0;
	};

	struct HANSASIMULATION_API FHansaRemoteIndustryProjection final
	{
		FHansaCityDefinitionId CityId;
		FString RegionId;
		FString ProductionChainId;
		FString StageKey;
		FString RecipeId;
		int64 CompletedCycles = 0;
		EHansaRemoteIndustryBlocker Blocker = EHansaRemoteIndustryBlocker::None;
		FHansaGoodId BlockingGoodId;
		FHansaQuantity LastProduced;
	};
}
