#include "Inventory/HansaInventory.h"

namespace Hansa::Simulation
{
	namespace
	{
		int32 FindInventoryIndex(
			const TArray<FHansaInventoryRecord>& Inventories,
			const FHansaInventoryId InventoryId)
		{
			for (int32 Index = 0; Index < Inventories.Num(); ++Index)
			{
				if (Inventories[Index].Id == InventoryId)
				{
					return Index;
				}
				if (InventoryId < Inventories[Index].Id)
				{
					break;
				}
			}
			return INDEX_NONE;
		}

		int32 FindStockIndex(const TArray<FHansaInventoryStockRecord>& Stocks, const FHansaGoodId GoodId)
		{
			for (int32 Index = 0; Index < Stocks.Num(); ++Index)
			{
				if (Stocks[Index].GoodId == GoodId)
				{
					return Index;
				}
				if (GoodId < Stocks[Index].GoodId)
				{
					break;
				}
			}
			return INDEX_NONE;
		}

		int32 FindStockInsertionIndex(const TArray<FHansaInventoryStockRecord>& Stocks, const FHansaGoodId GoodId)
		{
			int32 Index = 0;
			while (Index < Stocks.Num() && Stocks[Index].GoodId < GoodId)
			{
				++Index;
			}
			return Index;
		}

		int32 FindReservationIndex(
			const TArray<FHansaInventoryReservation>& Reservations,
			const FHansaReservationId ReservationId)
		{
			for (int32 Index = 0; Index < Reservations.Num(); ++Index)
			{
				if (Reservations[Index].Id == ReservationId)
				{
					return Index;
				}
				if (ReservationId < Reservations[Index].Id)
				{
					break;
				}
			}
			return INDEX_NONE;
		}

		int32 FindReservationInsertionIndex(
			const TArray<FHansaInventoryReservation>& Reservations,
			const FHansaReservationId ReservationId)
		{
			int32 Index = 0;
			while (Index < Reservations.Num() && Reservations[Index].Id < ReservationId)
			{
				++Index;
			}
			return Index;
		}

		THansaValueResult<FHansaQuantity> SumStock(const TArray<FHansaInventoryStockRecord>& Stocks)
		{
			FHansaQuantity Total;
			for (const FHansaInventoryStockRecord& Stock : Stocks)
			{
				const THansaValueResult<FHansaQuantity> Next = FHansaQuantity::TryAdd(Total, Stock.Quantity);
				if (!Next)
				{
					return THansaValueResult<FHansaQuantity>::Failure(Next.Error);
				}
				Total = Next.Value;
			}
			return THansaValueResult<FHansaQuantity>::Success(Total);
		}

		THansaValueResult<FHansaQuantity> SumReserved(const TArray<FHansaInventoryStockRecord>& Stocks)
		{
			FHansaQuantity Total;
			for (const FHansaInventoryStockRecord& Stock : Stocks)
			{
				const THansaValueResult<FHansaQuantity> Next = FHansaQuantity::TryAdd(Total, Stock.Reserved);
				if (!Next)
				{
					return THansaValueResult<FHansaQuantity>::Failure(Next.Error);
				}
				Total = Next.Value;
			}
			return THansaValueResult<FHansaQuantity>::Success(Total);
		}

		FHansaQuantity StockQuantity(
			const FHansaInventoryRecord& Inventory,
			const FHansaGoodId GoodId)
		{
			const int32 Index = FindStockIndex(Inventory.Stocks, GoodId);
			return Index == INDEX_NONE ? FHansaQuantity() : Inventory.Stocks[Index].Quantity;
		}

		bool AcceptsGood(
			const FHansaInventoryRecord& Inventory,
			const FHansaGoodId GoodId)
		{
			return Inventory.AcceptedGoods.Contains(GoodId);
		}

		FHansaInventoryTransactionResult Failure(
			const EHansaInventoryTransactionError Error,
			const uint64 Sequence,
			const FHansaQuantity Requested)
		{
			FHansaInventoryTransactionResult Result;
			Result.Error = Error;
			Result.Sequence = Sequence;
			Result.RequestedQuantity = Requested;
			return Result;
		}
	}

	const TCHAR* LexToString(const EHansaInventoryTransactionError Error)
	{
		switch (Error)
		{
		case EHansaInventoryTransactionError::None: return TEXT("None");
		case EHansaInventoryTransactionError::InvalidIdentity: return TEXT("InvalidIdentity");
		case EHansaInventoryTransactionError::InvalidQuantity: return TEXT("InvalidQuantity");
		case EHansaInventoryTransactionError::InvalidEndpoint: return TEXT("InvalidEndpoint");
		case EHansaInventoryTransactionError::SameInventory: return TEXT("SameInventory");
		case EHansaInventoryTransactionError::InventoryNotFound: return TEXT("InventoryNotFound");
		case EHansaInventoryTransactionError::GoodNotAccepted: return TEXT("GoodNotAccepted");
		case EHansaInventoryTransactionError::CapacityExceeded: return TEXT("CapacityExceeded");
		case EHansaInventoryTransactionError::InsufficientUnreservedStock: return TEXT("InsufficientUnreservedStock");
		case EHansaInventoryTransactionError::ReservationAlreadyExists: return TEXT("ReservationAlreadyExists");
		case EHansaInventoryTransactionError::ReservationNotFound: return TEXT("ReservationNotFound");
		case EHansaInventoryTransactionError::ReservationMismatch: return TEXT("ReservationMismatch");
		case EHansaInventoryTransactionError::SequenceOutOfOrder: return TEXT("SequenceOutOfOrder");
		case EHansaInventoryTransactionError::ArithmeticOverflow: return TEXT("ArithmeticOverflow");
		default: return TEXT("UnknownInventoryTransactionError");
		}
	}

	FHansaInventoryEndpoint FHansaInventoryEndpoint::Inventory(const FHansaInventoryId InventoryId)
	{
		FHansaInventoryEndpoint Result;
		Result.Kind = EHansaInventoryEndpointKind::Inventory;
		Result.InventoryId = InventoryId;
		return Result;
	}

	FHansaInventoryEndpoint FHansaInventoryEndpoint::Source(const FName SourceId)
	{
		FHansaInventoryEndpoint Result;
		Result.Kind = EHansaInventoryEndpointKind::ExplicitSource;
		Result.ExternalEndpointId = SourceId;
		return Result;
	}

	FHansaInventoryEndpoint FHansaInventoryEndpoint::Sink(const FName SinkId)
	{
		FHansaInventoryEndpoint Result;
		Result.Kind = EHansaInventoryEndpointKind::ExplicitSink;
		Result.ExternalEndpointId = SinkId;
		return Result;
	}

	THansaValueResult<FHansaInventoryLedger> FHansaInventoryLedger::TryCreate(
		TArray<FHansaInventoryInitialization> Initializations,
		const int32 RecentMovementCapacity)
	{
		if (RecentMovementCapacity <= 0)
		{
			return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::OutOfRange);
		}
		Initializations.Sort([](const FHansaInventoryInitialization& Left, const FHansaInventoryInitialization& Right)
		{
			return Left.Id < Right.Id;
		});

		FHansaInventoryLedger Ledger;
		Ledger.MovementCapacity = RecentMovementCapacity;
		Ledger.Inventories.Reserve(Initializations.Num());
		for (int32 InventoryIndex = 0; InventoryIndex < Initializations.Num(); ++InventoryIndex)
		{
			FHansaInventoryInitialization& Initialization = Initializations[InventoryIndex];
			if (!Initialization.Id.IsValid() ||
				Initialization.OwnerKind > EHansaInventoryOwnerKind::Warehouse ||
				Initialization.Capacity.GetRawValue() <= 0 ||
				(InventoryIndex > 0 && Initializations[InventoryIndex - 1].Id == Initialization.Id))
			{
				return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::InvalidFormat);
			}
			const bool bCityOwner = Initialization.OwnerKind == EHansaInventoryOwnerKind::City;
			if ((bCityOwner && (!Initialization.CityId.IsValid() || Initialization.BuildingId.IsValid())) ||
				(!bCityOwner && (!Initialization.BuildingId.IsValid() || Initialization.CityId.IsValid())))
			{
				return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::InvalidFormat);
			}

			Initialization.AcceptedGoods.Sort();
			if (Initialization.AcceptedGoods.IsEmpty())
			{
				return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::InvalidFormat);
			}
			for (int32 GoodIndex = 0; GoodIndex < Initialization.AcceptedGoods.Num(); ++GoodIndex)
			{
				if (!Initialization.AcceptedGoods[GoodIndex].IsValid() ||
					(GoodIndex > 0 && Initialization.AcceptedGoods[GoodIndex - 1] == Initialization.AcceptedGoods[GoodIndex]))
				{
					return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::InvalidFormat);
				}
			}

			Initialization.InitialStock.Sort([](const FHansaInventoryStockInitialization& Left, const FHansaInventoryStockInitialization& Right)
			{
				return Left.GoodId < Right.GoodId;
			});
			FHansaInventoryRecord Record;
			Record.Id = Initialization.Id;
			Record.OwnerKind = Initialization.OwnerKind;
			Record.CityId = Initialization.CityId;
			Record.BuildingId = Initialization.BuildingId;
			Record.Capacity = Initialization.Capacity;
			Record.AcceptedGoods = MoveTemp(Initialization.AcceptedGoods);
			if (Ledger.Inventories.ContainsByPredicate([&Record](const FHansaInventoryRecord& Existing)
			{
				const bool bExistingCity = Existing.OwnerKind == EHansaInventoryOwnerKind::City;
				const bool bRecordCity = Record.OwnerKind == EHansaInventoryOwnerKind::City;
				if (bExistingCity != bRecordCity)
				{
					return false;
				}
				return bRecordCity
					? Existing.CityId == Record.CityId
					: Existing.BuildingId == Record.BuildingId;
			}))
			{
				return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::InvalidFormat);
			}
			for (int32 StockIndex = 0; StockIndex < Initialization.InitialStock.Num(); ++StockIndex)
			{
				const FHansaInventoryStockInitialization& Stock = Initialization.InitialStock[StockIndex];
				if (!Stock.GoodId.IsValid() || Stock.Quantity.GetRawValue() < 0 ||
					!Record.AcceptedGoods.Contains(Stock.GoodId) ||
					(StockIndex > 0 && Initialization.InitialStock[StockIndex - 1].GoodId == Stock.GoodId))
				{
					return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::InvalidFormat);
				}
				if (Stock.Quantity.GetRawValue() > 0)
				{
					Record.Stocks.Add({ Stock.GoodId, Stock.Quantity, FHansaQuantity() });
				}
			}
			const THansaValueResult<FHansaQuantity> Used = SumStock(Record.Stocks);
			if (!Used || Used.Value.GetRawValue() > Record.Capacity.GetRawValue())
			{
				return THansaValueResult<FHansaInventoryLedger>::Failure(EHansaValueError::OutOfRange);
			}
			Ledger.Inventories.Add(MoveTemp(Record));
		}
		Ledger.bInitialized = true;
		return THansaValueResult<FHansaInventoryLedger>::Success(MoveTemp(Ledger));
	}

	FHansaInventoryReadOnlyAccess FHansaInventoryLedger::CreateReadOnlyAccess() const
	{
		check(bInitialized);
		return FHansaInventoryReadOnlyAccess(*this);
	}

	void FHansaInventoryLedger::AddRecentMovement(FHansaInventoryMovement Movement)
	{
		RecentMovements.Add(MoveTemp(Movement));
		if (RecentMovements.Num() > MovementCapacity)
		{
			RecentMovements.RemoveAt(0, RecentMovements.Num() - MovementCapacity, EAllowShrinking::No);
		}
	}

	FHansaInventoryTransactionResult FHansaInventoryLedger::TryTransfer(
		const FHansaInventoryEndpoint& Source,
		const FHansaInventoryEndpoint& Destination,
		const FHansaGoodId GoodId,
		const FHansaQuantity Quantity,
		const FHansaSimulationTick Tick,
		const uint64 Sequence,
		const FHansaReservationId SourceReservationId)
	{
		if (!bInitialized || !GoodId.IsValid())
		{
			return Failure(EHansaInventoryTransactionError::InvalidIdentity, Sequence, Quantity);
		}
		if (Quantity.GetRawValue() <= 0)
		{
			return Failure(EHansaInventoryTransactionError::InvalidQuantity, Sequence, Quantity);
		}
		if (Sequence == 0 || Sequence <= LastMovementSequence)
		{
			return Failure(EHansaInventoryTransactionError::SequenceOutOfOrder, Sequence, Quantity);
		}
		const bool bSourceInventory = Source.Kind == EHansaInventoryEndpointKind::Inventory;
		const bool bDestinationInventory = Destination.Kind == EHansaInventoryEndpointKind::Inventory;
		if ((!bSourceInventory && (Source.Kind != EHansaInventoryEndpointKind::ExplicitSource || Source.ExternalEndpointId.IsNone())) ||
			(!bDestinationInventory && (Destination.Kind != EHansaInventoryEndpointKind::ExplicitSink || Destination.ExternalEndpointId.IsNone())) ||
			(!bSourceInventory && !bDestinationInventory))
		{
			return Failure(EHansaInventoryTransactionError::InvalidEndpoint, Sequence, Quantity);
		}
		if ((bSourceInventory && !Source.InventoryId.IsValid()) ||
			(bDestinationInventory && !Destination.InventoryId.IsValid()))
		{
			return Failure(EHansaInventoryTransactionError::InvalidIdentity, Sequence, Quantity);
		}
		if (bSourceInventory && bDestinationInventory && Source.InventoryId == Destination.InventoryId)
		{
			return Failure(EHansaInventoryTransactionError::SameInventory, Sequence, Quantity);
		}

		const int32 SourceIndex = bSourceInventory ? FindInventoryIndex(Inventories, Source.InventoryId) : INDEX_NONE;
		const int32 DestinationIndex = bDestinationInventory ? FindInventoryIndex(Inventories, Destination.InventoryId) : INDEX_NONE;
		if ((bSourceInventory && SourceIndex == INDEX_NONE) || (bDestinationInventory && DestinationIndex == INDEX_NONE))
		{
			return Failure(EHansaInventoryTransactionError::InventoryNotFound, Sequence, Quantity);
		}
		if (bDestinationInventory && !AcceptsGood(Inventories[DestinationIndex], GoodId))
		{
			return Failure(EHansaInventoryTransactionError::GoodNotAccepted, Sequence, Quantity);
		}

		FHansaInventoryTransactionResult Result;
		Result.Sequence = Sequence;
		Result.RequestedQuantity = Quantity;
		if (bSourceInventory)
		{
			const FHansaInventoryRecord& SourceInventory = Inventories[SourceIndex];
			const int32 SourceStockIndex = FindStockIndex(SourceInventory.Stocks, GoodId);
			if (SourceStockIndex == INDEX_NONE)
			{
				return Failure(EHansaInventoryTransactionError::InsufficientUnreservedStock, Sequence, Quantity);
			}
			const FHansaInventoryStockRecord& Stock = SourceInventory.Stocks[SourceStockIndex];
			Result.SourceQuantityBefore = Stock.Quantity;
			if (SourceReservationId.IsValid())
			{
				const int32 ReservationIndex = FindReservationIndex(Reservations, SourceReservationId);
				if (ReservationIndex == INDEX_NONE)
				{
					return Failure(EHansaInventoryTransactionError::ReservationNotFound, Sequence, Quantity);
				}
				const FHansaInventoryReservation& Reservation = Reservations[ReservationIndex];
				if (Reservation.InventoryId != Source.InventoryId || Reservation.GoodId != GoodId ||
					Reservation.Quantity.GetRawValue() < Quantity.GetRawValue())
				{
					return Failure(EHansaInventoryTransactionError::ReservationMismatch, Sequence, Quantity);
				}
			}
			else
			{
				const THansaValueResult<FHansaQuantity> Available = FHansaQuantity::TrySubtract(Stock.Quantity, Stock.Reserved);
				if (!Available || Available.Value.GetRawValue() < Quantity.GetRawValue())
				{
					return Failure(EHansaInventoryTransactionError::InsufficientUnreservedStock, Sequence, Quantity);
				}
			}
		}
		else if (SourceReservationId.IsValid())
		{
			return Failure(EHansaInventoryTransactionError::ReservationMismatch, Sequence, Quantity);
		}

		if (bDestinationInventory)
		{
			const FHansaInventoryRecord& DestinationInventory = Inventories[DestinationIndex];
			const THansaValueResult<FHansaQuantity> Used = SumStock(DestinationInventory.Stocks);
			const THansaValueResult<FHansaQuantity> UsedAfter = Used
				? FHansaQuantity::TryAdd(Used.Value, Quantity)
				: THansaValueResult<FHansaQuantity>::Failure(EHansaValueError::Overflow);
			if (!UsedAfter)
			{
				return Failure(EHansaInventoryTransactionError::ArithmeticOverflow, Sequence, Quantity);
			}
			if (UsedAfter.Value.GetRawValue() > DestinationInventory.Capacity.GetRawValue())
			{
				return Failure(EHansaInventoryTransactionError::CapacityExceeded, Sequence, Quantity);
			}
			Result.DestinationQuantityBefore = StockQuantity(DestinationInventory, GoodId);
			if (!FHansaQuantity::TryAdd(Result.DestinationQuantityBefore, Quantity))
			{
				return Failure(EHansaInventoryTransactionError::ArithmeticOverflow, Sequence, Quantity);
			}
		}

		FHansaInventoryLedger Candidate = *this;
		if (bSourceInventory)
		{
			FHansaInventoryRecord& SourceInventory = Candidate.Inventories[SourceIndex];
			const int32 SourceStockIndex = FindStockIndex(SourceInventory.Stocks, GoodId);
			FHansaInventoryStockRecord& Stock = SourceInventory.Stocks[SourceStockIndex];
			Stock.Quantity = FHansaQuantity::TrySubtract(Stock.Quantity, Quantity).Value;
			if (SourceReservationId.IsValid())
			{
				const int32 ReservationIndex = FindReservationIndex(Candidate.Reservations, SourceReservationId);
				FHansaInventoryReservation& Reservation = Candidate.Reservations[ReservationIndex];
				Reservation.Quantity = FHansaQuantity::TrySubtract(Reservation.Quantity, Quantity).Value;
				Stock.Reserved = FHansaQuantity::TrySubtract(Stock.Reserved, Quantity).Value;
				if (Reservation.Quantity.GetRawValue() == 0)
				{
					Candidate.Reservations.RemoveAt(ReservationIndex);
				}
			}
			Result.SourceQuantityAfter = Stock.Quantity;
			if (Stock.Quantity.GetRawValue() == 0 && Stock.Reserved.GetRawValue() == 0)
			{
				SourceInventory.Stocks.RemoveAt(SourceStockIndex);
			}
		}
		else
		{
			Result.ExplicitlyCreatedQuantity = Quantity;
		}

		if (bDestinationInventory)
		{
			FHansaInventoryRecord& DestinationInventory = Candidate.Inventories[DestinationIndex];
			int32 DestinationStockIndex = FindStockIndex(DestinationInventory.Stocks, GoodId);
			if (DestinationStockIndex == INDEX_NONE)
			{
				FHansaInventoryStockRecord Stock;
				Stock.GoodId = GoodId;
				DestinationStockIndex = FindStockInsertionIndex(DestinationInventory.Stocks, GoodId);
				DestinationInventory.Stocks.Insert(Stock, DestinationStockIndex);
			}
			FHansaInventoryStockRecord& Stock = DestinationInventory.Stocks[DestinationStockIndex];
			Stock.Quantity = FHansaQuantity::TryAdd(Stock.Quantity, Quantity).Value;
			Result.DestinationQuantityAfter = Stock.Quantity;
		}
		else
		{
			Result.ExplicitlyDestroyedQuantity = Quantity;
		}

		TArray<FHansaInventoryMovement> NewMovements;
		if (bSourceInventory)
		{
			FHansaInventoryMovement Movement;
			Movement.Sequence = Sequence;
			Movement.Tick = Tick;
			Movement.Kind = bDestinationInventory ? EHansaInventoryMovementKind::TransferOut : EHansaInventoryMovementKind::SinkWithdrawal;
			Movement.InventoryId = Source.InventoryId;
			Movement.CounterpartyInventoryId = Destination.InventoryId;
			Movement.ExternalEndpointId = Destination.ExternalEndpointId;
			Movement.GoodId = GoodId;
			Movement.Quantity = Quantity;
			Movement.ReservationId = SourceReservationId;
			NewMovements.Add(Movement);
		}
		if (bDestinationInventory)
		{
			FHansaInventoryMovement Movement;
			Movement.Sequence = Sequence;
			Movement.Tick = Tick;
			Movement.Kind = bSourceInventory ? EHansaInventoryMovementKind::TransferIn : EHansaInventoryMovementKind::SourceDeposit;
			Movement.InventoryId = Destination.InventoryId;
			Movement.CounterpartyInventoryId = Source.InventoryId;
			Movement.ExternalEndpointId = Source.ExternalEndpointId;
			Movement.GoodId = GoodId;
			Movement.Quantity = Quantity;
			Movement.ReservationId = SourceReservationId;
			NewMovements.Add(Movement);
		}
		NewMovements.Sort([](const FHansaInventoryMovement& Left, const FHansaInventoryMovement& Right)
		{
			return Left.InventoryId < Right.InventoryId;
		});
		for (FHansaInventoryMovement& Movement : NewMovements)
		{
				Candidate.AddRecentMovement(MoveTemp(Movement));
		}
		Candidate.LastMovementSequence = Sequence;
		Result.AppliedQuantity = Quantity;
		*this = MoveTemp(Candidate);
		return Result;
	}

	FHansaInventoryTransactionResult FHansaInventoryLedger::TryReserve(
		const FHansaInventoryId InventoryId,
		const FHansaReservationId ReservationId,
		const FHansaGoodId GoodId,
		const FHansaQuantity Quantity,
		const FHansaSimulationTick Tick,
		const uint64 Sequence)
	{
		if (!bInitialized || !InventoryId.IsValid() || !ReservationId.IsValid() || !GoodId.IsValid())
		{
			return Failure(EHansaInventoryTransactionError::InvalidIdentity, Sequence, Quantity);
		}
		if (Quantity.GetRawValue() <= 0)
		{
			return Failure(EHansaInventoryTransactionError::InvalidQuantity, Sequence, Quantity);
		}
		if (Sequence == 0 || Sequence <= LastMovementSequence)
		{
			return Failure(EHansaInventoryTransactionError::SequenceOutOfOrder, Sequence, Quantity);
		}
		if (FindReservationIndex(Reservations, ReservationId) != INDEX_NONE)
		{
			return Failure(EHansaInventoryTransactionError::ReservationAlreadyExists, Sequence, Quantity);
		}
		const int32 InventoryIndex = FindInventoryIndex(Inventories, InventoryId);
		if (InventoryIndex == INDEX_NONE)
		{
			return Failure(EHansaInventoryTransactionError::InventoryNotFound, Sequence, Quantity);
		}
		const int32 StockIndex = FindStockIndex(Inventories[InventoryIndex].Stocks, GoodId);
		if (StockIndex == INDEX_NONE)
		{
			return Failure(EHansaInventoryTransactionError::InsufficientUnreservedStock, Sequence, Quantity);
		}
		const FHansaInventoryStockRecord& Stock = Inventories[InventoryIndex].Stocks[StockIndex];
		const THansaValueResult<FHansaQuantity> Available = FHansaQuantity::TrySubtract(Stock.Quantity, Stock.Reserved);
		if (!Available || Available.Value.GetRawValue() < Quantity.GetRawValue())
		{
			return Failure(EHansaInventoryTransactionError::InsufficientUnreservedStock, Sequence, Quantity);
		}

		FHansaInventoryLedger Candidate = *this;
		FHansaInventoryStockRecord& CandidateStock = Candidate.Inventories[InventoryIndex].Stocks[StockIndex];
		CandidateStock.Reserved = FHansaQuantity::TryAdd(CandidateStock.Reserved, Quantity).Value;
		FHansaInventoryReservation Reservation;
		Reservation.Id = ReservationId;
		Reservation.InventoryId = InventoryId;
		Reservation.GoodId = GoodId;
		Reservation.Quantity = Quantity;
		Candidate.Reservations.Insert(Reservation, FindReservationInsertionIndex(Candidate.Reservations, ReservationId));

		FHansaInventoryMovement Movement;
		Movement.Sequence = Sequence;
		Movement.Tick = Tick;
		Movement.Kind = EHansaInventoryMovementKind::ReservationCreated;
		Movement.InventoryId = InventoryId;
		Movement.GoodId = GoodId;
		Movement.Quantity = Quantity;
		Movement.ReservationId = ReservationId;
		Candidate.AddRecentMovement(MoveTemp(Movement));
		Candidate.LastMovementSequence = Sequence;

		FHansaInventoryTransactionResult Result;
		Result.Sequence = Sequence;
		Result.RequestedQuantity = Quantity;
		Result.AppliedQuantity = Quantity;
		Result.SourceQuantityBefore = Stock.Quantity;
		Result.SourceQuantityAfter = Stock.Quantity;
		*this = MoveTemp(Candidate);
		return Result;
	}

	FHansaInventoryTransactionResult FHansaInventoryLedger::TryReleaseReservation(
		const FHansaReservationId ReservationId,
		const FHansaSimulationTick Tick,
		const uint64 Sequence)
	{
		if (!bInitialized || !ReservationId.IsValid())
		{
			return Failure(EHansaInventoryTransactionError::InvalidIdentity, Sequence, FHansaQuantity());
		}
		if (Sequence == 0 || Sequence <= LastMovementSequence)
		{
			return Failure(EHansaInventoryTransactionError::SequenceOutOfOrder, Sequence, FHansaQuantity());
		}
		const int32 ReservationIndex = FindReservationIndex(Reservations, ReservationId);
		if (ReservationIndex == INDEX_NONE)
		{
			return Failure(EHansaInventoryTransactionError::ReservationNotFound, Sequence, FHansaQuantity());
		}
		const FHansaInventoryReservation Reservation = Reservations[ReservationIndex];
		const int32 InventoryIndex = FindInventoryIndex(Inventories, Reservation.InventoryId);
		const int32 StockIndex = InventoryIndex != INDEX_NONE
			? FindStockIndex(Inventories[InventoryIndex].Stocks, Reservation.GoodId)
			: INDEX_NONE;
		if (StockIndex == INDEX_NONE)
		{
			return Failure(EHansaInventoryTransactionError::ReservationMismatch, Sequence, Reservation.Quantity);
		}

		FHansaInventoryLedger Candidate = *this;
		FHansaInventoryStockRecord& Stock = Candidate.Inventories[InventoryIndex].Stocks[StockIndex];
		Stock.Reserved = FHansaQuantity::TrySubtract(Stock.Reserved, Reservation.Quantity).Value;
		Candidate.Reservations.RemoveAt(ReservationIndex);
		FHansaInventoryMovement Movement;
		Movement.Sequence = Sequence;
		Movement.Tick = Tick;
		Movement.Kind = EHansaInventoryMovementKind::ReservationReleased;
		Movement.InventoryId = Reservation.InventoryId;
		Movement.GoodId = Reservation.GoodId;
		Movement.Quantity = Reservation.Quantity;
		Movement.ReservationId = Reservation.Id;
		Candidate.AddRecentMovement(MoveTemp(Movement));
		Candidate.LastMovementSequence = Sequence;

		FHansaInventoryTransactionResult Result;
		Result.Sequence = Sequence;
		Result.RequestedQuantity = Reservation.Quantity;
		Result.AppliedQuantity = Reservation.Quantity;
		Result.SourceQuantityBefore = Stock.Quantity;
		Result.SourceQuantityAfter = Stock.Quantity;
		*this = MoveTemp(Candidate);
		return Result;
	}

	int32 FHansaInventoryReadOnlyAccess::GetInventoryCount() const
	{
		return Ledger->Inventories.Num();
	}

	uint64 FHansaInventoryReadOnlyAccess::GetLastMovementSequence() const
	{
		return Ledger->LastMovementSequence;
	}

	TOptional<FHansaInventoryProjection> FHansaInventoryReadOnlyAccess::QueryInventory(
		const FHansaInventoryId InventoryId) const
	{
		const int32 InventoryIndex = FindInventoryIndex(Ledger->Inventories, InventoryId);
		if (InventoryIndex == INDEX_NONE)
		{
			return TOptional<FHansaInventoryProjection>();
		}
		const FHansaInventoryRecord& Inventory = Ledger->Inventories[InventoryIndex];
		const THansaValueResult<FHansaQuantity> Used = SumStock(Inventory.Stocks);
		const THansaValueResult<FHansaQuantity> Reserved = SumReserved(Inventory.Stocks);
		check(Used && Reserved);

		FHansaInventoryProjection Projection;
		Projection.Id = Inventory.Id;
		Projection.OwnerKind = Inventory.OwnerKind;
		Projection.CityId = Inventory.CityId;
		Projection.BuildingId = Inventory.BuildingId;
		Projection.Capacity = Inventory.Capacity;
		Projection.UsedCapacity = Used.Value;
		Projection.FreeCapacity = FHansaQuantity::TrySubtract(Inventory.Capacity, Used.Value).Value;
		Projection.Reserved = Reserved.Value;
		Projection.AcceptedGoods = Inventory.AcceptedGoods;
		Projection.Stocks.Reserve(Inventory.Stocks.Num());
		for (const FHansaInventoryStockRecord& Stock : Inventory.Stocks)
		{
			FHansaInventoryStockProjection StockProjection;
			StockProjection.InventoryId = Inventory.Id;
			StockProjection.GoodId = Stock.GoodId;
			StockProjection.Stock = Stock.Quantity;
			StockProjection.Reserved = Stock.Reserved;
			StockProjection.Available = FHansaQuantity::TrySubtract(Stock.Quantity, Stock.Reserved).Value;
			Projection.Stocks.Add(StockProjection);
		}
		return TOptional<FHansaInventoryProjection>(MoveTemp(Projection));
	}

	TOptional<FHansaQuantity> FHansaInventoryReadOnlyAccess::QueryCapacity(
		const FHansaInventoryId InventoryId) const
	{
		const TOptional<FHansaInventoryProjection> Inventory = QueryInventory(InventoryId);
		return Inventory.IsSet()
			? TOptional<FHansaQuantity>(Inventory->Capacity)
			: TOptional<FHansaQuantity>();
	}

	TOptional<FHansaQuantity> FHansaInventoryReadOnlyAccess::QueryReservedAmount(
		const FHansaInventoryId InventoryId) const
	{
		const TOptional<FHansaInventoryProjection> Inventory = QueryInventory(InventoryId);
		return Inventory.IsSet()
			? TOptional<FHansaQuantity>(Inventory->Reserved)
			: TOptional<FHansaQuantity>();
	}

	TOptional<FHansaInventoryStockProjection> FHansaInventoryReadOnlyAccess::QueryStock(
		const FHansaInventoryId InventoryId,
		const FHansaGoodId GoodId) const
	{
		const TOptional<FHansaInventoryProjection> Inventory = QueryInventory(InventoryId);
		if (!Inventory.IsSet() || !GoodId.IsValid())
		{
			return TOptional<FHansaInventoryStockProjection>();
		}
		for (const FHansaInventoryStockProjection& Stock : Inventory->Stocks)
		{
			if (Stock.GoodId == GoodId)
			{
				return TOptional<FHansaInventoryStockProjection>(Stock);
			}
		}
		if (!Inventory->AcceptedGoods.Contains(GoodId))
		{
			return TOptional<FHansaInventoryStockProjection>();
		}
		FHansaInventoryStockProjection Empty;
		Empty.InventoryId = InventoryId;
		Empty.GoodId = GoodId;
		return TOptional<FHansaInventoryStockProjection>(Empty);
	}

	TArray<FHansaInventoryMovement> FHansaInventoryReadOnlyAccess::QueryRecentMovements(
		const FHansaInventoryId InventoryId,
		const int32 MaximumResults) const
	{
		TArray<FHansaInventoryMovement> Result;
		if (!InventoryId.IsValid() || MaximumResults <= 0)
		{
			return Result;
		}
		for (int32 Index = Ledger->RecentMovements.Num() - 1; Index >= 0 && Result.Num() < MaximumResults; --Index)
		{
			if (Ledger->RecentMovements[Index].InventoryId == InventoryId)
			{
				Result.Add(Ledger->RecentMovements[Index]);
			}
		}
		return Result;
	}

	TArray<FHansaInventoryProjection> FHansaInventoryReadOnlyAccess::BuildProjection() const
	{
		TArray<FHansaInventoryProjection> Result;
		Result.Reserve(Ledger->Inventories.Num());
		for (const FHansaInventoryRecord& Inventory : Ledger->Inventories)
		{
			const TOptional<FHansaInventoryProjection> Projection = QueryInventory(Inventory.Id);
			check(Projection.IsSet());
			Result.Add(Projection.GetValue());
		}
		return Result;
	}

	FHansaInventorySnapshot FHansaInventoryReadOnlyAccess::CaptureSnapshot() const
	{
		FHansaInventorySnapshot Snapshot;
		Snapshot.MovementCapacity = Ledger->MovementCapacity;
		Snapshot.LastMovementSequence = Ledger->LastMovementSequence;
		Snapshot.Inventories = Ledger->Inventories;
		Snapshot.Reservations = Ledger->Reservations;
		Snapshot.RecentMovements = Ledger->RecentMovements;
		return Snapshot;
	}
}
