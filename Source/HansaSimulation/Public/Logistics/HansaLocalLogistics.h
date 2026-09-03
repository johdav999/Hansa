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
		Completed
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsPriority Priority);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsBottleneck Bottleneck);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsRequestStatus Status);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaLogisticsJobStatus Status);

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
		EHansaLogisticsJobStatus Status = EHansaLogisticsJobStatus::AwaitingPickup;
	};

	struct HANSASIMULATION_API FHansaLogisticsRoadPathProjection final
	{
		FHansaInventoryId SourceInventoryId;
		FHansaInventoryId DestinationInventoryId;
		FHansaCityDefinitionId CityId;
		bool bConnected = false;
		int32 RoadDistanceCells = 0;
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
		[[nodiscard]] static FHansaLogisticsRoadPathProjection QueryRoadPath(
			FHansaInventoryId SourceInventoryId,
			FHansaInventoryId DestinationInventoryId,
			const FHansaInventoryReadOnlyAccess& Inventories,
			const FHansaPlacementState& Placement,
			TConstArrayView<FHansaBuildingState> Buildings);
	};
}
