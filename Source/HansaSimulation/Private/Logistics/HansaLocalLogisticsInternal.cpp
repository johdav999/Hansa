#include "Logistics/HansaLocalLogisticsInternal.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Market/HansaMarket.h"
#include "Model/HansaSimulationState.h"
#include "Production/HansaProduction.h"

namespace Hansa::Simulation
{
	namespace
	{
		FHansaSimulationTick TickOffset(const FHansaSimulationTick Tick, const int64 Offset)
		{
			return FHansaSimulationTick::TryCreate(Tick.GetValue() + Offset).Value;
		}

		bool IsCompletedLogisticsBuilding(
			const TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaBuildingId BuildingId)
		{
			for (const FHansaBuildingState& Building : Buildings)
			{
				if (Building.Id == BuildingId)
				{
					return Building.ConstructionState == EHansaConstructionState::Completed;
				}
			}
			return false;
		}

		int32 ActiveJobCount(const TArray<FHansaLogisticsJobState>& Jobs)
		{
			int32 Count = 0;
			for (const FHansaLogisticsJobState& Job : Jobs)
			{
				Count += Job.Status == EHansaLogisticsJobStatus::AwaitingPickup ||
					Job.Status == EHansaLogisticsJobStatus::InTransit ? 1 : 0;
			}
			return Count;
		}

		int32 RouteTravelTicks(const FHansaLocalLogisticsSettings& Settings, const int32 RoadDistanceCells)
		{
			return static_cast<int32>(FMath::Min<int64>(MAX_int32,
				FMath::Max<int64>(1, static_cast<int64>(RoadDistanceCells) * Settings.TicksPerRoadCell)));
		}

		int64 CommittedDestinationQuantity(
			const TArray<FHansaLogisticsJobState>& Jobs,
			const FHansaInventoryId DestinationId)
		{
			int64 Total = 0;
			for (const FHansaLogisticsJobState& Job : Jobs)
			{
				if (Job.Status != EHansaLogisticsJobStatus::Completed &&
					Job.DestinationInventoryId == DestinationId)
				{
					Total += Job.Quantity.GetRawValue();
				}
			}
			return Total;
		}

		FHansaLogisticsRequestState* FindRequest(
			TArray<FHansaLogisticsRequestState>& Requests,
			const FHansaLogisticsRequestId Id)
		{
			return Requests.FindByPredicate([Id](const FHansaLogisticsRequestState& Request)
			{
				return Request.Id == Id;
			});
		}

		FHansaCityDefinitionId InventoryCity(
			const FHansaInventoryProjection& Inventory,
			const FHansaPlacementState& Placement)
		{
			if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City)
			{
				return Inventory.CityId;
			}
			const FHansaPlacedBuildingRecord* Record = Placement.FindPlacement(Inventory.BuildingId);
			return Record != nullptr ? Record->Spec.CityId : FHansaCityDefinitionId();
		}

		int64 OutstandingToDestination(
			const TArray<FHansaLogisticsRequestState>& Requests,
			const FHansaInventoryId DestinationId,
			const FHansaGoodId GoodId)
		{
			int64 Total = 0;
			for (const FHansaLogisticsRequestState& Request : Requests)
			{
				if (Request.Status != EHansaLogisticsRequestStatus::Completed &&
					Request.DestinationInventoryId == DestinationId && Request.GoodId == GoodId)
				{
					Total += Request.RemainingQuantity.GetRawValue();
				}
			}
			return Total;
		}

		int64 UnscheduledFromSource(
			const TArray<FHansaLogisticsRequestState>& Requests,
			const FHansaInventoryId SourceId,
			const FHansaGoodId GoodId)
		{
			int64 Total = 0;
			for (const FHansaLogisticsRequestState& Request : Requests)
			{
				if (Request.Status != EHansaLogisticsRequestStatus::Completed &&
					Request.SourceInventoryId == SourceId && Request.GoodId == GoodId)
				{
					Total += Request.RemainingQuantity.GetRawValue() - Request.InFlightQuantity.GetRawValue();
				}
			}
			return Total;
		}

		uint64 NextRequestValue(const TArray<FHansaLogisticsRequestState>& Requests)
		{
			uint64 Result = 1;
			for (const FHansaLogisticsRequestState& Request : Requests)
			{
				Result = FMath::Max(Result, Request.Id.GetValue() + 1);
			}
			return Result;
		}

		void AddCandidate(TArray<FHansaInventoryId>& Candidates, const FHansaInventoryId Id)
		{
			if (Id.IsValid() && !Candidates.Contains(Id))
			{
				Candidates.Add(Id);
			}
		}
	}

	void FHansaLocalLogisticsExecutor::SynchronizeProductionRequests(
		TArray<FHansaLogisticsRequestState>& Requests,
		const TConstArrayView<FHansaProductionState> Productions,
		const TConstArrayView<FHansaCityMarketState> Markets,
		const FHansaEconomicRegistry& Registry,
		const FHansaInventoryLedger& InventoryLedger,
		const FHansaPlacementState& Placement,
		const TConstArrayView<FHansaBuildingState> Buildings,
		const FHansaSimulationTick CurrentTick)
	{
		const FHansaInventoryReadOnlyAccess InventoryView = InventoryLedger.CreateReadOnlyAccess();
		uint64 NextId = NextRequestValue(Requests);
		for (const FHansaProductionState& Production : Productions)
		{
			if (Production.Kind != EHansaProductionKind::BuildingRecipe || !Production.bActive ||
				!IsCompletedLogisticsBuilding(Buildings, Production.BuildingId))
			{
				continue;
			}
			const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(
                Production.RequestedRecipeId.IsValid() ? Production.RequestedRecipeId.ToString() : Production.RecipeId.ToString());
			const TOptional<FHansaInventoryProjection> InputInventory =
				InventoryView.QueryInventory(Production.InputInventoryId);
			const TOptional<FHansaInventoryProjection> OutputInventory =
				InventoryView.QueryInventory(Production.OutputInventoryId);
			const FHansaPlacedBuildingRecord* ProductionPlacement = Placement.FindPlacement(Production.BuildingId);
			if (Recipe == nullptr || !InputInventory.IsSet() || !OutputInventory.IsSet() ||
				ProductionPlacement == nullptr || InputInventory->OwnerKind == EHansaInventoryOwnerKind::City)
			{
				continue;
			}
			const FHansaCityDefinitionId CityId = ProductionPlacement->Spec.CityId;

			for (const FHansaCompiledGoodAmount& Input : Recipe->Inputs)
			{
				const THansaValueResult<FHansaGoodId> GoodId = FHansaGoodId::TryParse(Input.GoodId);
				if (!GoodId || Input.QuantityMilliUnits <= 0)
				{
					continue;
				}
				const TOptional<FHansaInventoryStockProjection> Current =
					InventoryView.QueryStock(Production.InputInventoryId, GoodId.Value);
				if (!Current.IsSet())
				{
					continue;
				}
				const int64 Need = Input.QuantityMilliUnits - Current->Stock.GetRawValue() -
					OutstandingToDestination(Requests, Production.InputInventoryId, GoodId.Value);
				if (Need <= 0)
				{
					continue;
				}

				TArray<FHansaInventoryId> Candidates;
				for (const FHansaCityMarketState& Market : Markets)
				{
					if (Market.CityId == CityId && Market.GoodId == GoodId.Value)
					{
						for (const FHansaInventoryId Id : Market.InventoryIds) AddCandidate(Candidates, Id);
					}
				}
				for (const FHansaInventoryProjection& Inventory : InventoryView.BuildProjection())
				{
					if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City ||
						Inventory.OwnerKind == EHansaInventoryOwnerKind::Warehouse)
					{
						AddCandidate(Candidates, Inventory.Id);
					}
				}
				for (const FHansaInventoryId CandidateId : Candidates)
				{
					if (CandidateId == Production.InputInventoryId)
					{
						continue;
					}
					const TOptional<FHansaInventoryProjection> Candidate = InventoryView.QueryInventory(CandidateId);
					const TOptional<FHansaInventoryStockProjection> CandidateStock =
						InventoryView.QueryStock(CandidateId, GoodId.Value);
					if (!Candidate.IsSet() || InventoryCity(Candidate.GetValue(), Placement) != CityId ||
						!CandidateStock.IsSet() || CandidateStock->Available.GetRawValue() <= 0 ||
						!FHansaLocalLogisticsQueries::QueryRoadPath(CandidateId, Production.InputInventoryId,
							InventoryView, Placement, Buildings, &Registry).bConnected)
					{
						continue;
					}
					FHansaLogisticsRequestState Request;
					Request.Id = FHansaLogisticsRequestId::TryCreate(NextId++).Value;
					Request.SourceInventoryId = CandidateId;
					Request.DestinationInventoryId = Production.InputInventoryId;
					Request.GoodId = GoodId.Value;
					Request.RequestedQuantity = FHansaQuantity::FromRaw(FMath::Min(Need,
						CandidateStock->Available.GetRawValue()));
					Request.RemainingQuantity = Request.RequestedQuantity;
					Request.Priority = EHansaLogisticsPriority::High;
					Request.CreatedTick = CurrentTick;
					Requests.Add(MoveTemp(Request));
					break;
				}
			}

			if (OutputInventory->OwnerKind == EHansaInventoryOwnerKind::City)
			{
				continue;
			}
            TArray<FHansaCompiledGoodAmount> Outputs = Recipe->Outputs;
            const auto* B = Buildings.FindByPredicate([&](const auto& V) { return V.Id == Production.BuildingId; });
            const auto* BD = B ? Registry.FindBuilding(B->DefinitionId.ToString()) : nullptr;
            if (BD) for (const auto& RecipeId : BD->RecipeIds)
                if (const auto* Other = Registry.FindRecipe(RecipeId)) for (const auto& Output : Other->Outputs)
                    if (!Outputs.ContainsByPredicate([&](const auto& V) { return V.GoodId == Output.GoodId; })) Outputs.Add(Output);
            for (const FHansaCompiledGoodAmount& Output : Outputs)
			{
				const THansaValueResult<FHansaGoodId> GoodId = FHansaGoodId::TryParse(Output.GoodId);
				const TOptional<FHansaInventoryStockProjection> Stock = GoodId
					? InventoryView.QueryStock(Production.OutputInventoryId, GoodId.Value)
					: TOptional<FHansaInventoryStockProjection>();
				if (!GoodId || !Stock.IsSet())
				{
					continue;
				}
				const int64 Available = Stock->Available.GetRawValue() -
					UnscheduledFromSource(Requests, Production.OutputInventoryId, GoodId.Value);
				if (Available <= 0)
				{
					continue;
				}
				TArray<FHansaInventoryId> Candidates;
				for (const FHansaCityMarketState& Market : Markets)
				{
					if (Market.CityId == CityId && Market.GoodId == GoodId.Value)
					{
						for (const FHansaInventoryId Id : Market.InventoryIds) AddCandidate(Candidates, Id);
					}
				}
				for (const FHansaInventoryProjection& Inventory : InventoryView.BuildProjection())
				{
					if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City ||
						Inventory.OwnerKind == EHansaInventoryOwnerKind::Warehouse)
					{
						AddCandidate(Candidates, Inventory.Id);
					}
				}
				for (const FHansaInventoryId CandidateId : Candidates)
				{
					if (CandidateId == Production.OutputInventoryId)
					{
						continue;
					}
					const TOptional<FHansaInventoryProjection> Candidate = InventoryView.QueryInventory(CandidateId);
					const TOptional<FHansaInventoryStockProjection> Accepted = InventoryView.QueryStock(CandidateId, GoodId.Value);
					if (!Candidate.IsSet() || !Accepted.IsSet() || Candidate->FreeCapacity.GetRawValue() <= 0 ||
						InventoryCity(Candidate.GetValue(), Placement) != CityId ||
						!FHansaLocalLogisticsQueries::QueryRoadPath(Production.OutputInventoryId, CandidateId,
							InventoryView, Placement, Buildings, &Registry).bConnected)
					{
						continue;
					}
					FHansaLogisticsRequestState Request;
					Request.Id = FHansaLogisticsRequestId::TryCreate(NextId++).Value;
					Request.SourceInventoryId = Production.OutputInventoryId;
					Request.DestinationInventoryId = CandidateId;
					Request.GoodId = GoodId.Value;
					Request.RequestedQuantity = FHansaQuantity::FromRaw(FMath::Min(Available,
						Candidate->FreeCapacity.GetRawValue()));
					Request.RemainingQuantity = Request.RequestedQuantity;
					Request.Priority = EHansaLogisticsPriority::Normal;
					Request.CreatedTick = CurrentTick;
					Requests.Add(MoveTemp(Request));
					break;
				}
			}
		}
		Requests.Sort([](const FHansaLogisticsRequestState& Left, const FHansaLogisticsRequestState& Right)
		{
			return Left.Id < Right.Id;
		});
	}

	void FHansaLocalLogisticsExecutor::AdvanceOneTick(
		TArray<FHansaLogisticsRequestState>& Requests,
		TArray<FHansaLogisticsJobState>& Jobs,
		uint64& NextJobValue,
		uint64& NextReservationValue,
		const FHansaLocalLogisticsSettings& Settings,
		FHansaInventoryLedger& InventoryLedger,
		const FHansaPlacementState& Placement,
		const TConstArrayView<FHansaBuildingState> Buildings,
		const FHansaEconomicRegistry* Registry,
		const TConstArrayView<FHansaHouseResearchState> Research,
		const FHansaSimulationTick CurrentTick)
	{
		// Pickup and delivery are separate ledger events; cargo lives in the job between them.
		// A pre-pickup topology pause releases its source reservation immediately. The same job
		// reacquires stock after reconnection, preventing disconnected jobs from deadlocking stock.
		for (FHansaLogisticsJobState& Job : Jobs)
		{
			if (Job.Status == EHansaLogisticsJobStatus::Completed)
			{
				continue;
			}
			FHansaLogisticsRequestState* Request = FindRequest(Requests, Job.RequestId);
			const FHansaLogisticsRoadPathProjection Path = FHansaLocalLogisticsQueries::QueryRoadPath(
				Job.SourceInventoryId, Job.DestinationInventoryId,
				InventoryLedger.CreateReadOnlyAccess(), Placement, Buildings, Registry);

			if (Job.Status == EHansaLogisticsJobStatus::AwaitingPickup ||
				Job.Status == EHansaLogisticsJobStatus::PausedAwaitingPickup)
			{
				if (!Path.bConnected)
				{
					if (Job.SourceReservationId.IsValid())
					{
						const FHansaInventoryTransactionResult Release = InventoryLedger.TryReleaseReservation(
							Job.SourceReservationId, CurrentTick,
							InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
						if (Release.IsSuccess())
						{
							Job.SourceReservationId = FHansaReservationId();
						}
					}
					Job.Status = EHansaLogisticsJobStatus::PausedAwaitingPickup;
					Job.PauseReason = Path.Failure;
					if (Request != nullptr)
					{
						Request->Status = EHansaLogisticsRequestStatus::InProgress;
						Request->Bottleneck = EHansaLogisticsBottleneck::DisconnectedRoad;
					}
					continue;
				}

				Job.SelectedMarketBuildingId = Path.SelectedMarketBuildingId;
				Job.RouteCells = Path.RouteCells;
				Job.RoadDistanceCells = Path.RoadDistanceCells;
				Job.RemainingTravelTicks = RouteTravelTicks(Settings, Path.RoadDistanceCells);
				const bool bResuming = Job.Status == EHansaLogisticsJobStatus::PausedAwaitingPickup;
				if (bResuming)
				{
					const TOptional<FHansaInventoryStockProjection> SourceStock =
						InventoryLedger.CreateReadOnlyAccess().QueryStock(Job.SourceInventoryId, Job.GoodId);
					if (!SourceStock.IsSet() || SourceStock->Available < Job.Quantity)
					{
						if (Request != nullptr) Request->Bottleneck = EHansaLogisticsBottleneck::SourceStockUnavailable;
						continue;
					}
					const FHansaReservationId ReservationId =
						FHansaReservationId::TryCreate(NextReservationValue++).Value;
					const FHansaInventoryTransactionResult Reservation = InventoryLedger.TryReserve(
						Job.SourceInventoryId, ReservationId, Job.GoodId, Job.Quantity, CurrentTick,
						InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
					if (!Reservation.IsSuccess())
					{
						if (Request != nullptr) Request->Bottleneck = EHansaLogisticsBottleneck::SourceStockUnavailable;
						continue;
					}
					Job.SourceReservationId = ReservationId;
					Job.PickupTick = TickOffset(CurrentTick, Settings.PickupDelayTicks);
					Job.Status = EHansaLogisticsJobStatus::AwaitingPickup;
					Job.PauseReason = EHansaLogisticsRoadPathFailure::None;
					if (Request != nullptr) Request->Bottleneck = EHansaLogisticsBottleneck::None;
				}
				Job.DeliveryTick = TickOffset(Job.PickupTick, Job.RemainingTravelTicks);
				if (bResuming || Job.PickupTick.GetValue() > CurrentTick.GetValue())
				{
					continue;
				}

				const FHansaInventoryTransactionResult Pickup = InventoryLedger.TryTransfer(
					FHansaInventoryEndpoint::Inventory(Job.SourceInventoryId),
					FHansaInventoryEndpoint::Sink(TEXT("LocalLogistics.Pickup")),
					Job.GoodId, Job.Quantity, CurrentTick,
					InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1,
					Job.SourceReservationId);
				if (Pickup.IsSuccess())
				{
					Job.CargoQuantity = Job.Quantity;
					Job.SourceReservationId = FHansaReservationId();
					Job.ElapsedTravelTicks = 0;
					Job.RemainingTravelTicks = RouteTravelTicks(Settings, Job.RoadDistanceCells);
					Job.DeliveryTick = TickOffset(CurrentTick, Job.RemainingTravelTicks);
					Job.Status = EHansaLogisticsJobStatus::InTransit;
					Job.PauseReason = EHansaLogisticsRoadPathFailure::None;
					if (Request != nullptr) Request->Bottleneck = EHansaLogisticsBottleneck::None;
				}
				else
				{
					if (Job.SourceReservationId.IsValid())
					{
						const FHansaInventoryTransactionResult Release = InventoryLedger.TryReleaseReservation(
							Job.SourceReservationId, CurrentTick,
							InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
						if (Release.IsSuccess()) Job.SourceReservationId = FHansaReservationId();
					}
					Job.Status = EHansaLogisticsJobStatus::PausedAwaitingPickup;
					Job.PauseReason = EHansaLogisticsRoadPathFailure::None;
					if (Request != nullptr) Request->Bottleneck = EHansaLogisticsBottleneck::SourceStockUnavailable;
				}
				continue;
			}

			if (Job.Status == EHansaLogisticsJobStatus::InTransit ||
				Job.Status == EHansaLogisticsJobStatus::PausedInTransit)
			{
				if (!Path.bConnected)
				{
					Job.Status = EHansaLogisticsJobStatus::PausedInTransit;
					Job.PauseReason = Path.Failure;
					if (Request != nullptr) Request->Bottleneck = EHansaLogisticsBottleneck::DisconnectedRoad;
					continue;
				}
				const bool bWasPaused = Job.Status == EHansaLogisticsJobStatus::PausedInTransit;
				const bool bRouteChanged = Job.RouteCells != Path.RouteCells ||
					Job.SelectedMarketBuildingId != Path.SelectedMarketBuildingId;
				if (bWasPaused || bRouteChanged || Job.RouteCells.IsEmpty())
				{
					Job.SelectedMarketBuildingId = Path.SelectedMarketBuildingId;
					Job.RouteCells = Path.RouteCells;
					Job.RoadDistanceCells = Path.RoadDistanceCells;
					const int32 NewTravelTicks = RouteTravelTicks(Settings, Path.RoadDistanceCells);
					Job.RemainingTravelTicks = FMath::Max(1, NewTravelTicks - Job.ElapsedTravelTicks);
					Job.DeliveryTick = TickOffset(CurrentTick, Job.RemainingTravelTicks);
				}
				Job.Status = EHansaLogisticsJobStatus::InTransit;
				Job.PauseReason = EHansaLogisticsRoadPathFailure::None;
				if (Request != nullptr) Request->Bottleneck = EHansaLogisticsBottleneck::None;
				if (bWasPaused || bRouteChanged)
				{
					continue;
				}
				if (Job.RemainingTravelTicks > 0)
				{
					int32 Progress = 1;
					if (const FHansaBuildingState* Warehouse = Buildings.FindByPredicate(
						[&Job](const FHansaBuildingState& Building) { return Building.Id == Job.SelectedMarketBuildingId; }))
					{
						const int32 Bonus = FHansaResearchEffectResolver::GetBasisPoints(
							Research, Warehouse->OwnerId, EHansaResearchEffectKind::WarehouseHandlingBasisPoints,
							Warehouse->DefinitionId.ToString());
						Progress = FHansaResearchEffectResolver::WorkUnitsForTick(Bonus, CurrentTick) / 10000;
					}
					Job.ElapsedTravelTicks += Progress;
					Job.RemainingTravelTicks = FMath::Max(0, Job.RemainingTravelTicks - Progress);
					Job.DeliveryTick = TickOffset(CurrentTick, Job.RemainingTravelTicks);
				}
				if (Job.RemainingTravelTicks > 0)
				{
					continue;
				}
				const FHansaInventoryTransactionResult Delivery = InventoryLedger.TryTransfer(
					FHansaInventoryEndpoint::Source(TEXT("LocalLogistics.Delivery")),
					FHansaInventoryEndpoint::Inventory(Job.DestinationInventoryId),
					Job.GoodId, Job.CargoQuantity, CurrentTick,
					InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
				if (Delivery.IsSuccess())
				{
					Job.CargoQuantity = FHansaQuantity();
					Job.Status = EHansaLogisticsJobStatus::Completed;
					Job.PauseReason = EHansaLogisticsRoadPathFailure::None;
					if (Request != nullptr)
					{
						Request->RemainingQuantity = FHansaQuantity::TrySubtract(
							Request->RemainingQuantity, Job.Quantity).Value;
						Request->InFlightQuantity = FHansaQuantity::TrySubtract(
							Request->InFlightQuantity, Job.Quantity).Value;
						Request->Bottleneck = EHansaLogisticsBottleneck::None;
						Request->Status = Request->RemainingQuantity.GetRawValue() == 0
							? EHansaLogisticsRequestStatus::Completed
							: EHansaLogisticsRequestStatus::Pending;
					}
				}
				else if (Request != nullptr && Delivery.Error == EHansaInventoryTransactionError::CapacityExceeded)
				{
					Request->Bottleneck = EHansaLogisticsBottleneck::DestinationFull;
				}
			}
		}

		TArray<int32> RequestOrder;
		for (int32 Index = 0; Index < Requests.Num(); ++Index)
		{
			if (Requests[Index].Status != EHansaLogisticsRequestStatus::Completed)
			{
				RequestOrder.Add(Index);
			}
		}
		RequestOrder.Sort([&Requests](const int32 Left, const int32 Right)
		{
			const FHansaLogisticsRequestState& A = Requests[Left];
			const FHansaLogisticsRequestState& B = Requests[Right];
			if (A.Priority != B.Priority)
			{
				return static_cast<uint8>(A.Priority) > static_cast<uint8>(B.Priority);
			}
			return A.CreatedTick != B.CreatedTick ? A.CreatedTick < B.CreatedTick : A.Id < B.Id;
		});

		const FHansaInventoryReadOnlyAccess InventoryView = InventoryLedger.CreateReadOnlyAccess();
		for (const int32 RequestIndex : RequestOrder)
		{
			FHansaLogisticsRequestState& Request = Requests[RequestIndex];
			const int64 Unscheduled = Request.RemainingQuantity.GetRawValue() - Request.InFlightQuantity.GetRawValue();
			if (Unscheduled <= 0)
			{
				Request.Status = EHansaLogisticsRequestStatus::InProgress;
				continue;
			}
			const TOptional<FHansaInventoryProjection> Source = InventoryView.QueryInventory(Request.SourceInventoryId);
			if (!Source.IsSet())
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::SourceInventoryMissing;
				continue;
			}
			const TOptional<FHansaInventoryProjection> Destination = InventoryView.QueryInventory(Request.DestinationInventoryId);
			if (!Destination.IsSet())
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::DestinationInventoryMissing;
				continue;
			}
			const FHansaLogisticsRoadPathProjection Path = FHansaLocalLogisticsQueries::QueryRoadPath(
				Request.SourceInventoryId, Request.DestinationInventoryId, InventoryView, Placement, Buildings, Registry);
			if (!Path.bConnected)
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::DisconnectedRoad;
				continue;
			}
			const TOptional<FHansaInventoryStockProjection> SourceStock = InventoryView.QueryStock(
				Request.SourceInventoryId, Request.GoodId);
			if (!SourceStock.IsSet() || SourceStock->Available.GetRawValue() <= 0)
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::SourceStockUnavailable;
				continue;
			}
			const int64 DestinationFree = Destination->FreeCapacity.GetRawValue() -
				CommittedDestinationQuantity(Jobs, Request.DestinationInventoryId);
			if (DestinationFree <= 0)
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::DestinationFull;
				continue;
			}
			if (ActiveJobCount(Jobs) >= Settings.MaximumConcurrentJobs)
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::FleetCapacity;
				continue;
			}

			const int64 QuantityRaw = FMath::Min(
				FMath::Min(Unscheduled, Settings.JobCapacity.GetRawValue()),
				FMath::Min(FMath::Max<int64>(0, SourceStock->Available.GetRawValue() - InventoryView.QueryProtectedRaw(Request.SourceInventoryId, Request.GoodId)), DestinationFree));
			if (QuantityRaw <= 0)
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::SourceStockUnavailable;
				continue;
			}
			const FHansaQuantity Quantity = FHansaQuantity::FromRaw(QuantityRaw);
			const FHansaReservationId ReservationId = FHansaReservationId::TryCreate(NextReservationValue++).Value;
			const FHansaInventoryTransactionResult Reservation = InventoryLedger.TryReserve(
				Request.SourceInventoryId, ReservationId, Request.GoodId, Quantity, CurrentTick,
				InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
			if (!Reservation.IsSuccess())
			{
				Request.Bottleneck = EHansaLogisticsBottleneck::SourceStockUnavailable;
				continue;
			}

			FHansaLogisticsJobState Job;
			Job.Id = FHansaLogisticsJobId::TryCreate(NextJobValue++).Value;
			Job.RequestId = Request.Id;
			Job.SourceReservationId = ReservationId;
			Job.SourceInventoryId = Request.SourceInventoryId;
			Job.DestinationInventoryId = Request.DestinationInventoryId;
			Job.GoodId = Request.GoodId;
			Job.Quantity = Quantity;
			Job.DispatchTick = CurrentTick;
			Job.PickupTick = TickOffset(CurrentTick, Settings.PickupDelayTicks);
			Job.DeliveryTick = TickOffset(Job.PickupTick,
				FMath::Max<int64>(1, static_cast<int64>(Path.RoadDistanceCells) * Settings.TicksPerRoadCell));
			Job.RoadDistanceCells = Path.RoadDistanceCells;
			Job.SelectedMarketBuildingId = Path.SelectedMarketBuildingId;
			Job.RouteCells = Path.RouteCells;
			Job.ElapsedTravelTicks = 0;
			Job.RemainingTravelTicks = RouteTravelTicks(Settings, Path.RoadDistanceCells);
			Jobs.Add(MoveTemp(Job));
			Request.InFlightQuantity = FHansaQuantity::TryAdd(Request.InFlightQuantity, Quantity).Value;
			Request.Status = EHansaLogisticsRequestStatus::InProgress;
			Request.Bottleneck = EHansaLogisticsBottleneck::None;
		}

		Jobs.Sort([](const FHansaLogisticsJobState& Left, const FHansaLogisticsJobState& Right)
		{
			return Left.Id < Right.Id;
		});
	}
}
