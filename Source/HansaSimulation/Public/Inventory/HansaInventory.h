#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Math/HansaFixedPoint.h"
#include "Misc/Optional.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Model/HansaValueResult.h"

namespace Hansa::Simulation
{
	enum class EHansaInventoryOwnerKind : uint8
	{
		City = 0,
		Building,
		Warehouse
	};

	enum class EHansaInventoryEndpointKind : uint8
	{
		Inventory = 0,
		ExplicitSource,
		ExplicitSink
	};

	enum class EHansaInventoryMovementKind : uint8
	{
		TransferOut = 0,
		TransferIn,
		SourceDeposit,
		SinkWithdrawal,
		ReservationCreated,
		ReservationReleased
	};

	enum class EHansaInventoryTransactionError : uint8
	{
		None = 0,
		InvalidIdentity,
		InvalidQuantity,
		InvalidEndpoint,
		SameInventory,
		InventoryNotFound,
		GoodNotAccepted,
		CapacityExceeded,
		InsufficientUnreservedStock,
		ReservationAlreadyExists,
		ReservationNotFound,
		ReservationMismatch,
		SequenceOutOfOrder,
		ArithmeticOverflow
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaInventoryTransactionError Error);

	struct HANSASIMULATION_API FHansaInventoryEndpoint final
	{
		EHansaInventoryEndpointKind Kind = EHansaInventoryEndpointKind::Inventory;
		FHansaInventoryId InventoryId;
		FName ExternalEndpointId;

		[[nodiscard]] static FHansaInventoryEndpoint Inventory(FHansaInventoryId InventoryId);
		[[nodiscard]] static FHansaInventoryEndpoint Source(FName SourceId);
		[[nodiscard]] static FHansaInventoryEndpoint Sink(FName SinkId);
	};

	struct HANSASIMULATION_API FHansaInventoryStockInitialization final
	{
		FHansaGoodId GoodId;
		FHansaQuantity Quantity;
	};

	struct HANSASIMULATION_API FHansaInventoryInitialization final
	{
		FHansaInventoryId Id;
		EHansaInventoryOwnerKind OwnerKind = EHansaInventoryOwnerKind::City;
		FHansaCityDefinitionId CityId;
		FHansaBuildingId BuildingId;
		FHansaQuantity Capacity;
		TArray<FHansaGoodId> AcceptedGoods;
		TArray<FHansaInventoryStockInitialization> InitialStock;
	};

	struct HANSASIMULATION_API FHansaInventoryStockRecord final
	{
		FHansaGoodId GoodId;
		FHansaQuantity Quantity;
		FHansaQuantity Reserved;
	};

	struct HANSASIMULATION_API FHansaInventoryReservation final
	{
		FHansaReservationId Id;
		FHansaInventoryId InventoryId;
		FHansaGoodId GoodId;
		FHansaQuantity Quantity;
	};

	/** Canonically sorted authoritative record; callers receive it only through read-only owning snapshots. */
	struct HANSASIMULATION_API FHansaInventoryRecord final
	{
		FHansaInventoryId Id;
		EHansaInventoryOwnerKind OwnerKind = EHansaInventoryOwnerKind::City;
		FHansaCityDefinitionId CityId;
		FHansaBuildingId BuildingId;
		FHansaQuantity Capacity;
		TArray<FHansaGoodId> AcceptedGoods;
		TArray<FHansaInventoryStockRecord> Stocks;
	};

	struct HANSASIMULATION_API FHansaInventoryMovement final
	{
		uint64 Sequence = 0;
		FHansaSimulationTick Tick;
		EHansaInventoryMovementKind Kind = EHansaInventoryMovementKind::TransferIn;
		FHansaInventoryId InventoryId;
		FHansaInventoryId CounterpartyInventoryId;
		FName ExternalEndpointId;
		FHansaGoodId GoodId;
		FHansaQuantity Quantity;
		FHansaReservationId ReservationId;
	};

	struct HANSASIMULATION_API FHansaInventoryTransactionResult final
	{
		EHansaInventoryTransactionError Error = EHansaInventoryTransactionError::None;
		uint64 Sequence = 0;
		FHansaQuantity RequestedQuantity;
		FHansaQuantity AppliedQuantity;
		FHansaQuantity SourceQuantityBefore;
		FHansaQuantity SourceQuantityAfter;
		FHansaQuantity DestinationQuantityBefore;
		FHansaQuantity DestinationQuantityAfter;
		FHansaQuantity ExplicitlyCreatedQuantity;
		FHansaQuantity ExplicitlyDestroyedQuantity;

		[[nodiscard]] bool IsSuccess() const { return Error == EHansaInventoryTransactionError::None; }
	};

	struct HANSASIMULATION_API FHansaInventoryStockProjection final
	{
		FHansaInventoryId InventoryId;
		FHansaGoodId GoodId;
		FHansaQuantity Stock;
		FHansaQuantity Reserved;
		FHansaQuantity Available;
	};

	struct HANSASIMULATION_API FHansaInventoryProjection final
	{
		FHansaInventoryId Id;
		EHansaInventoryOwnerKind OwnerKind = EHansaInventoryOwnerKind::City;
		FHansaCityDefinitionId CityId;
		FHansaBuildingId BuildingId;
		FHansaQuantity Capacity;
		FHansaQuantity UsedCapacity;
		FHansaQuantity FreeCapacity;
		FHansaQuantity Reserved;
		TArray<FHansaGoodId> AcceptedGoods;
		TArray<FHansaInventoryStockProjection> Stocks;
	};

	class FHansaInventoryReadOnlyAccess;
	class FHansaStateHasher;

	/** Owning immutable copy used by simulation snapshots, save work, and asynchronous readers. */
	class HANSASIMULATION_API FHansaInventorySnapshot final
	{
	public:
		[[nodiscard]] int32 GetMovementCapacity() const { return MovementCapacity; }
		[[nodiscard]] uint64 GetLastMovementSequence() const { return LastMovementSequence; }
		[[nodiscard]] TConstArrayView<FHansaInventoryRecord> GetInventories() const { return Inventories; }
		[[nodiscard]] TConstArrayView<FHansaInventoryReservation> GetReservations() const { return Reservations; }
		[[nodiscard]] TConstArrayView<FHansaInventoryMovement> GetRecentMovements() const { return RecentMovements; }

	private:
		friend class FHansaInventoryReadOnlyAccess;
		int32 MovementCapacity = 0;
		uint64 LastMovementSequence = 0;
		TArray<FHansaInventoryRecord> Inventories;
		TArray<FHansaInventoryReservation> Reservations;
		TArray<FHansaInventoryMovement> RecentMovements;
	};

	/**
	 * Actor-independent authoritative inventory ledger. All successful mutations are
	 * atomic and deterministic; failures return AppliedQuantity=0 and leave the ledger unchanged.
	 */
	class HANSASIMULATION_API FHansaInventoryLedger final
	{
	public:
		FHansaInventoryLedger() = default;

		[[nodiscard]] static THansaValueResult<FHansaInventoryLedger> TryCreate(
			TArray<FHansaInventoryInitialization> Inventories,
			int32 RecentMovementCapacity = 64);

		[[nodiscard]] bool IsValid() const { return bInitialized; }
		[[nodiscard]] FHansaInventoryReadOnlyAccess CreateReadOnlyAccess() const;

		[[nodiscard]] FHansaInventoryTransactionResult TryTransfer(
			const FHansaInventoryEndpoint& Source,
			const FHansaInventoryEndpoint& Destination,
			FHansaGoodId GoodId,
			FHansaQuantity Quantity,
			FHansaSimulationTick Tick,
			uint64 Sequence,
			FHansaReservationId SourceReservationId = FHansaReservationId());

		[[nodiscard]] FHansaInventoryTransactionResult TryReserve(
			FHansaInventoryId InventoryId,
			FHansaReservationId ReservationId,
			FHansaGoodId GoodId,
			FHansaQuantity Quantity,
			FHansaSimulationTick Tick,
			uint64 Sequence);

		[[nodiscard]] FHansaInventoryTransactionResult TryReleaseReservation(
			FHansaReservationId ReservationId,
			FHansaSimulationTick Tick,
			uint64 Sequence);

	private:
		friend class FHansaInventoryReadOnlyAccess;
		friend class FHansaStateHasher;

		void AddRecentMovement(FHansaInventoryMovement Movement);

		bool bInitialized = false;
		int32 MovementCapacity = 64;
		uint64 LastMovementSequence = 0;
		TArray<FHansaInventoryRecord> Inventories;
		TArray<FHansaInventoryReservation> Reservations;
		TArray<FHansaInventoryMovement> RecentMovements;
	};

	/** Typed read-only inventory queries returning owning projections, never mutable containers. */
	class HANSASIMULATION_API FHansaInventoryReadOnlyAccess final
	{
	public:
		[[nodiscard]] int32 GetInventoryCount() const;
		[[nodiscard]] uint64 GetLastMovementSequence() const;
		[[nodiscard]] TOptional<FHansaInventoryProjection> QueryInventory(FHansaInventoryId InventoryId) const;
		[[nodiscard]] TOptional<FHansaQuantity> QueryCapacity(FHansaInventoryId InventoryId) const;
		[[nodiscard]] TOptional<FHansaQuantity> QueryReservedAmount(FHansaInventoryId InventoryId) const;
		[[nodiscard]] TOptional<FHansaInventoryStockProjection> QueryStock(
			FHansaInventoryId InventoryId,
			FHansaGoodId GoodId) const;
		[[nodiscard]] TArray<FHansaInventoryMovement> QueryRecentMovements(
			FHansaInventoryId InventoryId,
			int32 MaximumResults) const;
		[[nodiscard]] TArray<FHansaInventoryProjection> BuildProjection() const;
		[[nodiscard]] FHansaInventorySnapshot CaptureSnapshot() const;

	private:
		friend class FHansaInventoryLedger;
		explicit FHansaInventoryReadOnlyAccess(const FHansaInventoryLedger& InLedger)
			: Ledger(&InLedger)
		{
		}

		const FHansaInventoryLedger* Ledger = nullptr;
	};
}
