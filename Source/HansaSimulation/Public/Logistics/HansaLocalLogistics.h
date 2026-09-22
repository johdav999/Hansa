#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Simulation
{
	class FHansaInventoryReadOnlyAccess;
	class FHansaPlacementState;
	class FHansaEconomicRegistry;
	struct FHansaBuildingState;

	enum class EHansaLogisticsPriority : uint8
	{
		Low = 0,
		Normal,
		High,
		Critical
	};

	enum class EHansaLogisticsBottleneck : uint8
	{
		None = 0,
		SourceInventoryMissing,
		DestinationInventoryMissing,
		DisconnectedRoad,
		SourceStockUnavailable,
		DestinationFull,
		FleetCapacity
	};

	enum class EHansaLogisticsRequestStatus : uint8
	{
		Pending = 0,
		InProgress,
		Completed
	};

	enum class EHansaLogisticsJobStatus : uint8
	{
		AwaitingPickup = 0,
		InTransit,
		Completed,
		PausedAwaitingPickup,
		PausedInTransit
	};

	/** Causal result for a physical road-and-market access query. */
	enum class EHansaLogisticsRoadPathFailure : uint8
	{
		None = 0,
		SourceInventoryMissing,
		DestinationInventoryMissing,
		SourceEndpointUnavailable,
		DestinationEndpointUnavailable,
		DifferentCities,
		NoCompletedRoad,
		NoOperationalMarket,
		NoMarketRoadAccess,
		SourceNotAdjacentToRoad,
		DestinationNotAdjacentToRoad,
		SourceNotConnectedToMarket,
		DestinationNotConnectedToMarket,
		EndpointsDisconnected,
		MarketNotInRange
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsPriority Priority);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsBottleneck Bottleneck);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsRequestStatus Status);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsJobStatus Status);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsRoadPathFailure Failure);

	/** Fixed-step policy for the intentionally aggregated MVP cart/warehouse abstraction. */
	struct HANSASIMULATION_API FHansaLocalLogisticsSettings final
	{
		FHansaQuantity JobCapacity = FHansaQuantity::FromRaw(10'000);
		int32 PickupDelayTicks = 1;
		int32 TicksPerRoadCell = 1;
		int32 MaximumConcurrentJobs = 4;
	};

	struct HANSASIMULATION_API FHansaLogisticsRequestInitialization final
	{
		FHansaLogisticsRequestId Id;
		FHansaInventoryId SourceInventoryId;
		FHansaInventoryId DestinationInventoryId;
		FHansaGoodId GoodId;
		FHansaQuantity Quantity;
		EHansaLogisticsPriority Priority = EHansaLogisticsPriority::Normal;
	};

	/** Authoritative demand record. RemainingQuantity includes cargo already in flight. */
	struct HANSASIMULATION_API FHansaLogisticsRequestState final
	{
		FHansaLogisticsRequestId Id;
		FHansaInventoryId SourceInventoryId;
		FHansaInventoryId DestinationInventoryId;
		FHansaGoodId GoodId;
		FHansaQuantity RequestedQuantity;
		FHansaQuantity RemainingQuantity;
		FHansaQuantity InFlightQuantity;
		EHansaLogisticsPriority Priority = EHansaLogisticsPriority::Normal;
		EHansaLogisticsRequestStatus Status = EHansaLogisticsRequestStatus::Pending;
		EHansaLogisticsBottleneck Bottleneck = EHansaLogisticsBottleneck::None;
		FHansaSimulationTick CreatedTick;
	};

	/** Authoritative aggregated delivery. Cargo is owned here between pickup and delivery. */
	struct HANSASIMULATION_API FHansaLogisticsJobState final
	{
		FHansaLogisticsJobId Id;
		FHansaLogisticsRequestId RequestId;
		FHansaReservationId SourceReservationId;
		FHansaInventoryId SourceInventoryId;
		FHansaInventoryId DestinationInventoryId;
		FHansaGoodId GoodId;
		FHansaQuantity Quantity;
		FHansaQuantity CargoQuantity;
		FHansaSimulationTick DispatchTick;
		FHansaSimulationTick PickupTick;
		FHansaSimulationTick DeliveryTick;
		int32 RoadDistanceCells = 0;
		FHansaBuildingId SelectedMarketBuildingId;
		TArray<FHansaGridCoordinate> RouteCells;
		int32 ElapsedTravelTicks = 0;
		int32 RemainingTravelTicks = 0;
		EHansaLogisticsRoadPathFailure PauseReason = EHansaLogisticsRoadPathFailure::None;
		EHansaLogisticsJobStatus Status = EHansaLogisticsJobStatus::AwaitingPickup;
	};

	struct HANSASIMULATION_API FHansaLogisticsRoadPathProjection final
	{
		FHansaInventoryId SourceInventoryId;
		FHansaInventoryId DestinationInventoryId;
		FHansaCityDefinitionId CityId;
		bool bConnected = false;
		bool bMarketEligible = false;
		FHansaBuildingId SelectedMarketBuildingId;
		EHansaLogisticsRoadPathFailure Failure = EHansaLogisticsRoadPathFailure::None;
		FName MessageKey;
		FName RemedyKey;
		int32 RoadDistanceCells = 0;
		TArray<FHansaGridCoordinate> RouteCells;
		TArray<FHansaGridCoordinate> SourceAccessCells;
		TArray<FHansaGridCoordinate> DestinationAccessCells;
	};

	struct HANSASIMULATION_API FHansaLogisticsRequestProjection final
	{
		FHansaLogisticsRequestId Id;
		FHansaInventoryId SourceInventoryId;
		FHansaInventoryId DestinationInventoryId;
		FHansaGoodId GoodId;
		FHansaQuantity RequestedQuantity;
		FHansaQuantity RemainingQuantity;
		FHansaQuantity InFlightQuantity;
		EHansaLogisticsPriority Priority = EHansaLogisticsPriority::Normal;
		EHansaLogisticsRequestStatus Status = EHansaLogisticsRequestStatus::Pending;
		EHansaLogisticsBottleneck Bottleneck = EHansaLogisticsBottleneck::None;
		FHansaSimulationTick CreatedTick;
	};

	struct HANSASIMULATION_API FHansaLogisticsJobProjection final
	{
		FHansaLogisticsJobId Id;
		FHansaLogisticsRequestId RequestId;
		FHansaInventoryId SourceInventoryId;
		FHansaInventoryId DestinationInventoryId;
		FHansaGoodId GoodId;
		FHansaQuantity Quantity;
		FHansaQuantity CargoQuantity;
		FHansaSimulationTick DispatchTick;
		FHansaSimulationTick PickupTick;
		FHansaSimulationTick DeliveryTick;
		int32 RoadDistanceCells = 0;
		FHansaBuildingId SelectedMarketBuildingId;
		TArray<FHansaGridCoordinate> RouteCells;
		int32 ElapsedTravelTicks = 0;
		int32 RemainingTravelTicks = 0;
		EHansaLogisticsRoadPathFailure PauseReason = EHansaLogisticsRoadPathFailure::None;
		EHansaLogisticsJobStatus Status = EHansaLogisticsJobStatus::AwaitingPickup;
	};

	class HANSASIMULATION_API FHansaLocalLogisticsSnapshot final
	{
	public:
		[[nodiscard]] const FHansaLocalLogisticsSettings& GetSettings() const { return Settings; }
		[[nodiscard]] TConstArrayView<FHansaLogisticsRequestState> GetRequests() const { return Requests; }
		[[nodiscard]] TConstArrayView<FHansaLogisticsJobState> GetJobs() const { return Jobs; }

	private:
		friend class FHansaSimulationReadOnlyAccess;
		FHansaLocalLogisticsSettings Settings;
		TArray<FHansaLogisticsRequestState> Requests;
		TArray<FHansaLogisticsJobState> Jobs;
	};

	/** Pure typed graph query used by simulation, automation and diagnostics. */
	class HANSASIMULATION_API FHansaLocalLogisticsQueries final
	{
	public:
		/** Tests only whether a completed building has orthogonal access to a completed road. */
		[[nodiscard]] static FHansaLogisticsRoadPathProjection QueryBuildingRoadAccess(
			FHansaBuildingId SourceBuildingId,
			const FHansaPlacementState& Placement,
			TConstArrayView<FHansaBuildingState> Buildings,
            const FHansaEconomicRegistry* Registry = nullptr);

		[[nodiscard]] static FHansaLogisticsRoadPathProjection QueryRoadPath(
			FHansaInventoryId SourceInventoryId,
			FHansaInventoryId DestinationInventoryId,
			const FHansaInventoryReadOnlyAccess& Inventories,
			const FHansaPlacementState& Placement,
			TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaEconomicRegistry* Registry = nullptr);

		/** Tests physical access from a completed building to one spatial market inventory. */
		[[nodiscard]] static FHansaLogisticsRoadPathProjection QueryBuildingMarketAccess(
			FHansaBuildingId SourceBuildingId,
			FHansaInventoryId MarketInventoryId,
			const FHansaInventoryReadOnlyAccess& Inventories,
			const FHansaPlacementState& Placement,
			TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaEconomicRegistry* Registry = nullptr);
	};
}
