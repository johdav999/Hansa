#pragma once

#include "Containers/Array.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Simulation
{
	enum class EHansaRouteMode : uint8
	{
		Sea = 0,
		Land
	};

	enum class EHansaRouteCargoActionKind : uint8
	{
		Load = 0, // Legacy city action: foreign cities settle money. Never reinterpret saved values.
		Unload,
		StationLoad,
		StationUnload,
		OwnedCityLoad,
		OwnedCityUnload
	};

	inline bool IsRouteLoad(EHansaRouteCargoActionKind Kind)
	{
		return Kind == EHansaRouteCargoActionKind::Load || Kind == EHansaRouteCargoActionKind::StationLoad || Kind == EHansaRouteCargoActionKind::OwnedCityLoad;
	}
	inline bool IsStationTransfer(EHansaRouteCargoActionKind Kind)
	{
		return Kind == EHansaRouteCargoActionKind::StationLoad || Kind == EHansaRouteCargoActionKind::StationUnload;
	}

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

	/** Explicit ownership-changing commerce. Route load/unload remains a separate contract. */
	enum class EHansaSpotTradeSide : uint8 { BuyFromCity = 0, SellToCity };
	enum class EHansaSpotTradeOutcome : uint8 { None = 0, Completed, Partial, Missed };
	enum class EHansaSpotTradeBlocker : uint8
	{
		None = 0,
		InsufficientFunds,
		InsufficientMarketStock,
		InsufficientShipCapacity,
		InsufficientShipStock,
		InsufficientMarketCapacity
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
	HANSASIMULATION_API const TCHAR* LexToString(EHansaSpotTradeSide Side);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaSpotTradeOutcome Outcome);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaSpotTradeBlocker Blocker);
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
        int64 UnitPriceMilliMarks = 0;
        int64 SettledMoneyRaw = 0; // Positive sale proceeds, negative purchase cost; local transfers remain zero.
	};

	/** Persisted receipt for the most recent explicit quay transaction performed by a ship. */
	struct HANSASIMULATION_API FHansaSpotTradeRecord final
	{
		FHansaCommandId CommandId;
		FHansaSimulationTick Tick;
		FHansaHouseId HouseId;
		FHansaVehicleId VehicleId;
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		EHansaSpotTradeSide Side = EHansaSpotTradeSide::BuyFromCity;
		FHansaQuantity RequestedQuantity;
		FHansaQuantity AppliedQuantity;
		EHansaSpotTradeOutcome Outcome = EHansaSpotTradeOutcome::None;
		EHansaSpotTradeBlocker Blocker = EHansaSpotTradeBlocker::None;
		int64 MarketUpdateTick = -1;
		int64 UnitPriceMilliMarks = 0;
		int64 SettledMoneyRaw = 0;
	};

	struct HANSASIMULATION_API FHansaSpotTradeQuoteProjection final
	{
		FHansaHouseId HouseId;
		FHansaVehicleId VehicleId;
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		EHansaSpotTradeSide Side = EHansaSpotTradeSide::BuyFromCity;
		bool bCanSubmit = false;
		FString Cause;
		FString Remedy;
		FHansaQuantity RequestedQuantity;
		FHansaQuantity EstimatedQuantity;
		int64 ReviewedMarketUpdateTick = -1;
		int64 ReviewedUnitPriceMilliMarks = 0;
		int64 EstimatedSettlementMoneyRaw = 0;
	};
    struct HANSASIMULATION_API FHansaWaterJourney final
    {
        FHansaCityDefinitionId CityId;
        FHansaGridCoordinate Home;
        FHansaGridCoordinate Cell;
        TArray<FHansaGridCoordinate> Path;
        int32 NextIndex = 0;
        bool IsMoving() const { return Path.IsValidIndex(NextIndex); }
        bool IsAtHome() const { return !IsMoving() && Cell == Home; }
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
		FHansaSpotTradeRecord LastSpotTrade;
        FHansaWaterJourney Navigation;
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
		FHansaSpotTradeRecord LastSpotTrade;
        FHansaWaterJourney Navigation;
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
