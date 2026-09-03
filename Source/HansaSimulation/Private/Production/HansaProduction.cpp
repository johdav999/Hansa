#include "Production/HansaProduction.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Optional.h"
#include "Production/HansaProductionInternal.h"

namespace Hansa::Simulation
{
	namespace
	{
		const FHansaBuildingState* FindBuilding(
			const TArray<FHansaBuildingState>& Buildings,
			const FHansaBuildingId BuildingId)
		{
			for (const FHansaBuildingState& Building : Buildings)
			{
				if (Building.Id == BuildingId)
				{
					return &Building;
				}
				if (BuildingId < Building.Id)
				{
					break;
				}
			}
			return nullptr;
		}

		TOptional<FHansaGoodId> ParseGood(const FString& StableId)
		{
			const THansaValueResult<FHansaGoodId> Parsed = FHansaGoodId::TryParse(StableId);
			return Parsed ? TOptional<FHansaGoodId>(Parsed.Value) : TOptional<FHansaGoodId>();
		}

		void ClearCausalBlocker(FHansaProductionState& Production)
		{
			Production.Blocker = EHansaProductionBlocker::None;
			Production.BlockingGoodId = FHansaGoodId();
			Production.BlockingRequiredQuantity = FHansaQuantity();
			Production.BlockingAvailableQuantity = FHansaQuantity();
		}

		void SetBlocker(
			FHansaProductionState& Production,
			const EHansaProductionBlocker Blocker,
			const FHansaGoodId GoodId = FHansaGoodId(),
			const FHansaQuantity Required = FHansaQuantity(),
			const FHansaQuantity Available = FHansaQuantity())
		{
			Production.Blocker = Blocker;
			Production.BlockingGoodId = GoodId;
			Production.BlockingRequiredQuantity = Required;
			Production.BlockingAvailableQuantity = Available;
		}

		void AddBlockerEventIfChanged(
			const EHansaProductionBlocker Previous,
			const FHansaProductionState& Production,
			TArray<FHansaProductionStepEvent>& OutEvents)
		{
			if (Previous == Production.Blocker)
			{
				return;
			}
			FHansaProductionStepEvent Event;
			Event.Kind = EHansaProductionStepEventKind::BlockerChanged;
			Event.ProductionId = Production.Id;
			Event.BuildingId = Production.BuildingId;
			Event.RecipeId = Production.RecipeId;
			Event.Blocker = Production.Blocker;
			Event.CompletedCycles = Production.CompletedCycles;
			OutEvents.Add(Event);
		}

		bool TryAllocateReservationId(
			const FHansaInventoryLedger& Ledger,
			uint64& NextReservationValue,
			FHansaReservationId& OutId)
		{
			const FHansaInventorySnapshot Snapshot = Ledger.CreateReadOnlyAccess().CaptureSnapshot();
			while (NextReservationValue != 0)
			{
				const THansaValueResult<FHansaReservationId> Candidate =
					FHansaReservationId::TryCreate(NextReservationValue++);
				if (!Candidate)
				{
					return false;
				}
				bool bAlreadyUsed = false;
				for (const FHansaInventoryReservation& Reservation : Snapshot.GetReservations())
				{
					if (Reservation.Id == Candidate.Value)
					{
						bAlreadyUsed = true;
						break;
					}
				}
				if (!bAlreadyUsed)
				{
					OutId = Candidate.Value;
					return true;
				}
			}
			return false;
		}

		bool HasOutputCapacity(
			const FHansaInventoryReadOnlyAccess& InventoryAccess,
			const FHansaInventoryId InventoryId,
			const TArray<FHansaCompiledGoodAmount>& Outputs,
			const FHansaQuantity CapacityFreedByInputs,
			FHansaProductionState& Production)
		{
			const TOptional<FHansaInventoryProjection> Inventory = InventoryAccess.QueryInventory(InventoryId);
			if (!Inventory.IsSet())
			{
				SetBlocker(Production, EHansaProductionBlocker::InventoryTransactionFailed);
				return false;
			}
			FHansaQuantity TotalOutput;
			for (const FHansaCompiledGoodAmount& Output : Outputs)
			{
				const TOptional<FHansaGoodId> GoodId = ParseGood(Output.GoodId);
				if (!GoodId.IsSet() || Output.QuantityMilliUnits <= 0)
				{
					SetBlocker(Production, EHansaProductionBlocker::MissingDefinition);
					return false;
				}
				if (!InventoryAccess.QueryStock(InventoryId, GoodId.GetValue()).IsSet())
				{
					SetBlocker(Production, EHansaProductionBlocker::StorageBlocked,
						GoodId.GetValue(), FHansaQuantity::FromRaw(Output.QuantityMilliUnits));
					return false;
				}
				const THansaValueResult<FHansaQuantity> Added = FHansaQuantity::TryAdd(
					TotalOutput, FHansaQuantity::FromRaw(Output.QuantityMilliUnits));
				if (!Added)
				{
					SetBlocker(Production, EHansaProductionBlocker::StorageBlocked, GoodId.GetValue());
					return false;
				}
				TotalOutput = Added.Value;
			}
			const THansaValueResult<FHansaQuantity> EffectiveFreeCapacity = FHansaQuantity::TryAdd(
				Inventory->FreeCapacity, CapacityFreedByInputs);
			if (!EffectiveFreeCapacity || TotalOutput.GetRawValue() > EffectiveFreeCapacity.Value.GetRawValue())
			{
				const TOptional<FHansaGoodId> FirstGood = Outputs.IsEmpty()
					? TOptional<FHansaGoodId>()
					: ParseGood(Outputs[0].GoodId);
				SetBlocker(Production, EHansaProductionBlocker::StorageBlocked,
					FirstGood.IsSet() ? FirstGood.GetValue() : FHansaGoodId(),
					TotalOutput, EffectiveFreeCapacity ? EffectiveFreeCapacity.Value : Inventory->FreeCapacity);
				return false;
			}
			return true;
		}

		uint64 NextMovementSequence(const FHansaInventoryLedger& Ledger)
		{
			const uint64 Last = Ledger.CreateReadOnlyAccess().GetLastMovementSequence();
			return Last == TNumericLimits<uint64>::Max() ? 0 : Last + 1;
		}

		bool TryReserveInputs(
			FHansaProductionState& Production,
			const FHansaCompiledRecipeDefinition& Recipe,
			FHansaInventoryLedger& InventoryLedger,
			uint64& NextReservationValue,
			const FHansaSimulationTick Tick)
		{
			FHansaInventoryLedger CandidateLedger = InventoryLedger;
			uint64 CandidateNextReservationValue = NextReservationValue;
			TArray<FHansaProductionInputReservation> Reservations;
			Reservations.Reserve(Recipe.Inputs.Num());
			for (const FHansaCompiledGoodAmount& Input : Recipe.Inputs)
			{
				const TOptional<FHansaGoodId> GoodId = ParseGood(Input.GoodId);
				if (!GoodId.IsSet() || Input.QuantityMilliUnits <= 0)
				{
					SetBlocker(Production, EHansaProductionBlocker::MissingDefinition);
					return false;
				}
				const TOptional<FHansaInventoryStockProjection> Stock =
					CandidateLedger.CreateReadOnlyAccess().QueryStock(Production.InputInventoryId, GoodId.GetValue());
				const FHansaQuantity Required = FHansaQuantity::FromRaw(Input.QuantityMilliUnits);
				const FHansaQuantity Available = Stock.IsSet() ? Stock->Available : FHansaQuantity();
				if (!Stock.IsSet() || Available.GetRawValue() < Required.GetRawValue())
				{
					SetBlocker(Production, EHansaProductionBlocker::MissingInput,
						GoodId.GetValue(), Required, Available);
					return false;
				}
				FHansaReservationId ReservationId;
				if (!TryAllocateReservationId(CandidateLedger, CandidateNextReservationValue, ReservationId))
				{
					SetBlocker(Production, EHansaProductionBlocker::InventoryTransactionFailed,
						GoodId.GetValue(), Required, Available);
					return false;
				}
				const uint64 Sequence = NextMovementSequence(CandidateLedger);
				if (Sequence == 0 || !CandidateLedger.TryReserve(
					Production.InputInventoryId, ReservationId, GoodId.GetValue(), Required, Tick, Sequence).IsSuccess())
				{
					SetBlocker(Production, EHansaProductionBlocker::InventoryTransactionFailed,
						GoodId.GetValue(), Required, Available);
					return false;
				}
				Reservations.Add({ GoodId.GetValue(), ReservationId, Required });
			}
		InventoryLedger = MoveTemp(CandidateLedger);
		NextReservationValue = CandidateNextReservationValue;
		Production.InputReservations = MoveTemp(Reservations);
		return true;
		}

		bool TryCompleteRecipe(
			FHansaProductionState& Production,
			const FHansaCompiledRecipeDefinition& Recipe,
			FHansaInventoryLedger& InventoryLedger,
			const FHansaSimulationTick Tick)
		{
			FHansaQuantity CapacityFreedByInputs;
			if (Production.InputInventoryId == Production.OutputInventoryId)
			{
				for (const FHansaProductionInputReservation& Reservation : Production.InputReservations)
				{
					const THansaValueResult<FHansaQuantity> Added = FHansaQuantity::TryAdd(
						CapacityFreedByInputs, Reservation.Quantity);
					if (!Added)
					{
						SetBlocker(Production, EHansaProductionBlocker::InventoryTransactionFailed);
						return false;
					}
					CapacityFreedByInputs = Added.Value;
				}
			}
			if (!HasOutputCapacity(
				InventoryLedger.CreateReadOnlyAccess(), Production.OutputInventoryId, Recipe.Outputs,
				CapacityFreedByInputs, Production))
			{
				return false;
			}
			FHansaInventoryLedger CandidateLedger = InventoryLedger;
			for (const FHansaProductionInputReservation& Reservation : Production.InputReservations)
			{
				const uint64 Sequence = NextMovementSequence(CandidateLedger);
				if (Sequence == 0 || !CandidateLedger.TryTransfer(
					FHansaInventoryEndpoint::Inventory(Production.InputInventoryId),
					FHansaInventoryEndpoint::Sink(TEXT("Sink.ProductionInput")),
					Reservation.GoodId,
					Reservation.Quantity,
					Tick,
					Sequence,
					Reservation.ReservationId).IsSuccess())
				{
					SetBlocker(Production, EHansaProductionBlocker::InventoryTransactionFailed,
						Reservation.GoodId, Reservation.Quantity);
					return false;
				}
			}
			for (const FHansaCompiledGoodAmount& Output : Recipe.Outputs)
			{
				const TOptional<FHansaGoodId> GoodId = ParseGood(Output.GoodId);
				const uint64 Sequence = NextMovementSequence(CandidateLedger);
				if (!GoodId.IsSet() || Sequence == 0 || !CandidateLedger.TryTransfer(
					FHansaInventoryEndpoint::Source(TEXT("Source.ProductionOutput")),
					FHansaInventoryEndpoint::Inventory(Production.OutputInventoryId),
					GoodId.GetValue(),
					FHansaQuantity::FromRaw(Output.QuantityMilliUnits),
					Tick,
					Sequence).IsSuccess())
				{
					SetBlocker(Production, EHansaProductionBlocker::InventoryTransactionFailed,
						GoodId.IsSet() ? GoodId.GetValue() : FHansaGoodId());
					return false;
				}
			}
			InventoryLedger = MoveTemp(CandidateLedger);
			Production.InputReservations.Reset();
			return true;
		}

		bool TryCompleteBackgroundSupply(
			FHansaProductionState& Production,
			FHansaInventoryLedger& InventoryLedger,
			const FHansaSimulationTick Tick)
		{
			TArray<FHansaCompiledGoodAmount> Outputs;
			Outputs.Add({ Production.SupplyGoodId.ToString(), Production.SupplyQuantityPerCycle.GetRawValue() });
			if (!HasOutputCapacity(
				InventoryLedger.CreateReadOnlyAccess(), Production.OutputInventoryId, Outputs,
				FHansaQuantity(), Production))
			{
				return false;
			}
			FHansaInventoryLedger CandidateLedger = InventoryLedger;
			const uint64 Sequence = NextMovementSequence(CandidateLedger);
			if (Sequence == 0 || !CandidateLedger.TryTransfer(
				FHansaInventoryEndpoint::Source(TEXT("Source.BackgroundSupply")),
				FHansaInventoryEndpoint::Inventory(Production.OutputInventoryId),
				Production.SupplyGoodId,
				Production.SupplyQuantityPerCycle,
				Tick,
				Sequence).IsSuccess())
			{
				SetBlocker(Production, EHansaProductionBlocker::InventoryTransactionFailed,
					Production.SupplyGoodId, Production.SupplyQuantityPerCycle);
				return false;
			}
			InventoryLedger = MoveTemp(CandidateLedger);
			return true;
		}
	}

	const TCHAR* LexToString(const EHansaProductionBlocker Blocker)
	{
		switch (Blocker)
		{
		case EHansaProductionBlocker::None: return TEXT("None");
		case EHansaProductionBlocker::Inactive: return TEXT("Inactive");
		case EHansaProductionBlocker::ConstructionIncomplete: return TEXT("ConstructionIncomplete");
		case EHansaProductionBlocker::MissingDefinition: return TEXT("MissingDefinition");
		case EHansaProductionBlocker::InsufficientLaborerWorkforce: return TEXT("InsufficientLaborerWorkforce");
		case EHansaProductionBlocker::InsufficientArtisanWorkforce: return TEXT("InsufficientArtisanWorkforce");
		case EHansaProductionBlocker::MissingInput: return TEXT("MissingInput");
		case EHansaProductionBlocker::StorageBlocked: return TEXT("StorageBlocked");
		case EHansaProductionBlocker::InventoryTransactionFailed: return TEXT("InventoryTransactionFailed");
		default: return TEXT("UnknownProductionBlocker");
		}
	}

	void FHansaProductionExecutor::AdvanceOneTick(
		TArray<FHansaProductionState>& Productions,
		uint64& NextReservationValue,
		const TArray<FHansaBuildingState>& Buildings,
		FHansaInventoryLedger& InventoryLedger,
		const FHansaEconomicRegistry* EconomicRegistry,
		const FHansaSimulationTick Tick,
		TArray<FHansaProductionStepEvent>& OutEvents)
	{
		for (FHansaProductionState& Production : Productions)
		{
			const EHansaProductionBlocker PreviousBlocker = Production.Blocker;
			Production.bCompletedCycleLastTick = false;
			ClearCausalBlocker(Production);
			if (!Production.bActive)
			{
				SetBlocker(Production, EHansaProductionBlocker::Inactive);
				AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
				continue;
			}

			const FHansaCompiledRecipeDefinition* Recipe = nullptr;
			const FHansaCompiledBuildingDefinition* BuildingDefinition = nullptr;
			int32 CycleTicks = Production.SupplyCycleTicks;
			if (Production.Kind == EHansaProductionKind::BuildingRecipe)
			{
				const FHansaBuildingState* Building = FindBuilding(Buildings, Production.BuildingId);
				if (Building == nullptr || Building->ConstructionProgress.GetPartsPerMillion() != FHansaRate::Scale)
				{
					SetBlocker(Production, Building == nullptr
						? EHansaProductionBlocker::MissingDefinition
						: EHansaProductionBlocker::ConstructionIncomplete);
					AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
					continue;
				}
				Recipe = EconomicRegistry != nullptr ? EconomicRegistry->FindRecipe(Production.RecipeId.ToString()) : nullptr;
				BuildingDefinition = EconomicRegistry != nullptr
					? EconomicRegistry->FindBuilding(Building->DefinitionId.ToString()) : nullptr;
				if (Recipe == nullptr || BuildingDefinition == nullptr || Recipe->CycleTicks <= 0 ||
					!BuildingDefinition->RecipeIds.Contains(Production.RecipeId.ToString()))
				{
					SetBlocker(Production, EHansaProductionBlocker::MissingDefinition);
					AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
					continue;
				}
				CycleTicks = Recipe->CycleTicks;
				const int32 RequiredLaborers = FMath::Max(Recipe->LaborerWorkforce, BuildingDefinition->LaborerWorkforce);
				const int32 RequiredArtisans = FMath::Max(Recipe->ArtisanWorkforce, BuildingDefinition->ArtisanWorkforce);
				if (Production.AllocatedLaborerWorkforce < RequiredLaborers)
				{
					SetBlocker(Production, EHansaProductionBlocker::InsufficientLaborerWorkforce);
					AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
					continue;
				}
				if (Production.AllocatedArtisanWorkforce < RequiredArtisans)
				{
					SetBlocker(Production, EHansaProductionBlocker::InsufficientArtisanWorkforce);
					AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
					continue;
				}
				if (Production.ProgressTicks == 0 && !Recipe->Inputs.IsEmpty() &&
					!TryReserveInputs(Production, *Recipe, InventoryLedger, NextReservationValue, Tick))
				{
					AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
					continue;
				}
			}
			else if (EconomicRegistry == nullptr ||
				EconomicRegistry->FindGood(Production.SupplyGoodId.ToString()) == nullptr || CycleTicks <= 0)
			{
				SetBlocker(Production, EHansaProductionBlocker::MissingDefinition);
				AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
				continue;
			}

			if (Production.ProgressTicks < CycleTicks)
			{
				++Production.ProgressTicks;
			}
			if (Production.ProgressTicks < CycleTicks)
			{
				AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
				continue;
			}

			const bool bCompleted = Production.Kind == EHansaProductionKind::BuildingRecipe
				? TryCompleteRecipe(Production, *Recipe, InventoryLedger, Tick)
				: TryCompleteBackgroundSupply(Production, InventoryLedger, Tick);
			if (!bCompleted)
			{
				AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
				continue;
			}
			Production.ProgressTicks = 0;
			++Production.CompletedCycles;
			Production.bCompletedCycleLastTick = true;
			ClearCausalBlocker(Production);
			FHansaProductionStepEvent Event;
			Event.Kind = EHansaProductionStepEventKind::CycleCompleted;
			Event.ProductionId = Production.Id;
			Event.BuildingId = Production.BuildingId;
			Event.RecipeId = Production.RecipeId;
			Event.CompletedCycles = Production.CompletedCycles;
			OutEvents.Add(Event);
		}
	}
}
