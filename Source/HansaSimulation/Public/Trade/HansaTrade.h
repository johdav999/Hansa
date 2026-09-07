#pragma once

#include "Containers/Array.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	enum class EHansaRouteMode : uint8
	{
		Sea = 0,
		Land
	};

	enum class EHansaRouteCargoActionKind : uint8
	{
		Load = 0,
		Unload
	};

	/** MVP executes only Always. The explicit enum is the version-safe extension point for later trade rules. */
	enum class EHansaRouteCargoCondition : uint8
	{
		Always = 0
	};

	enum class EHansaRouteLifecycleState : uint8
	{
		Inactive = 0,
		AtStop,
		Traveling,
		Cancelled
	};

	enum class EHansaRouteTransferOutcome : uint8
	{
		None = 0,
		Completed,
		Partial,
		Missed
	};

	enum class EHansaRoutePlanError : uint8
	{
		None = 0,
		InvalidIdentity,
		VehicleNotFound,
		DefinitionNotFound,
		ModeMismatch,
		InvalidStops,
		UnreachableLeg,
		InvalidAction,
		VehicleLocationMismatch,
		InvalidCargoInventory
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaRouteMode Mode);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaRouteCargoActionKind Kind);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaRouteLifecycleState State);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaRouteTransferOutcome Outcome);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaRoutePlanError Error);

	struct HANSASIMULATION_API FHansaRouteCargoAction final
	{
		EHansaRouteCargoActionKind Kind = EHansaRouteCargoActionKind::Load;
		EHansaRouteCargoCondition Condition = EHansaRouteCargoCondition::Always;
		FHansaGoodId GoodId;
		FHansaQuantity QuantityLimit;
		FHansaQuantity MinimumSourceReserve;
	};

	struct HANSASIMULATION_API FHansaRouteStop final
	{
		FHansaCityDefinitionId CityId;
		TArray<FHansaRouteCargoAction> Actions;
	};

	struct HANSASIMULATION_API FHansaRouteTransferRecord final
	{
		FHansaSimulationTick Tick;
		int32 StopIndex = INDEX_NONE;
		int32 ActionIndex = INDEX_NONE;
		EHansaRouteCargoActionKind Kind = EHansaRouteCargoActionKind::Load;
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		FHansaQuantity RequestedQuantity;
		FHansaQuantity AppliedQuantity;
		EHansaRouteTransferOutcome Outcome = EHansaRouteTransferOutcome::None;
	};

	struct HANSASIMULATION_API FHansaVehicleState final
	{
		// First four fields preserve the original aggregate-initialization contract.
		FHansaVehicleId Id;
		FHansaVehicleDefinitionId DefinitionId;
		FHansaHouseId OwnerId;
		FHansaQuantity Cargo;
		FHansaInventoryId CargoInventoryId;
		EHansaRouteMode Mode = EHansaRouteMode::Sea;
		FHansaQuantity Capacity;
		FHansaCityDefinitionId CurrentCityId;
		int64 UpkeepPfennigPerTravelTick = 0;
		int64 AccruedUpkeepPfennig = 0;
	};

	struct HANSASIMULATION_API FHansaRouteState final
	{
		// First four fields preserve the original aggregate-initialization contract.
		FHansaRouteId Id;
		FHansaHouseId OwnerId;
		FHansaVehicleId VehicleId;
		FHansaRate Progress;
		FHansaRouteDefinitionId RouteDefinitionId;
		EHansaRouteMode Mode = EHansaRouteMode::Sea;
		TArray<FHansaRouteStop> Stops;
		EHansaRouteLifecycleState Lifecycle = EHansaRouteLifecycleState::Inactive;
		int32 CurrentStopIndex = 0;
		int32 NextStopIndex = 1;
		int32 RemainingTravelTicks = 0;
		int32 TotalTravelTicks = 0;
		bool bPendingStopActions = false;
		int64 CompletedLegCount = 0;
		int64 MissedCargoActionCount = 0;
		FHansaRouteTransferRecord LastTransfer;
	};

	struct HANSASIMULATION_API FHansaVehicleProjection final
	{
		FHansaVehicleId Id;
		FHansaVehicleDefinitionId DefinitionId;
		FHansaHouseId OwnerId;
		FHansaInventoryId CargoInventoryId;
		EHansaRouteMode Mode = EHansaRouteMode::Sea;
		FHansaCityDefinitionId CurrentCityId;
		FHansaQuantity Cargo;
		FHansaQuantity Capacity;
		FHansaQuantity FreeCapacity;
		int64 UpkeepPfennigPerTravelTick = 0;
		int64 AccruedUpkeepPfennig = 0;
	};

	struct HANSASIMULATION_API FHansaRouteProjection final
	{
		FHansaRouteId Id;
		FHansaHouseId OwnerId;
		FHansaVehicleId VehicleId;
		FHansaRouteDefinitionId RouteDefinitionId;
		EHansaRouteMode Mode = EHansaRouteMode::Sea;
		EHansaRouteLifecycleState Lifecycle = EHansaRouteLifecycleState::Inactive;
		TArray<FHansaRouteStop> Stops;
		int32 CurrentStopIndex = 0;
		int32 NextStopIndex = 1;
		int32 RemainingTravelTicks = 0;
		int32 TotalTravelTicks = 0;
		FHansaRate Progress;
		int64 CompletedLegCount = 0;
		int64 MissedCargoActionCount = 0;
		FHansaRouteTransferRecord LastTransfer;
	};
}
