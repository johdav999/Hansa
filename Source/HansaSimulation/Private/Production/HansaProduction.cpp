#include "Production/HansaProduction.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Optional.h"
#include "Production/HansaProductionInternal.h"

namespace Hansa::Simulation
{
	bool FHansaProductionExecutor::SynchronizeCompletedBuildings(
		TArray<FHansaProductionState>& Productions,
		const TArray<FHansaBuildingState>& Buildings,
		const FHansaPlacementState& Placement,
		FHansaInventoryLedger& InventoryLedger,
		const FHansaEconomicRegistry& Registry)
	{
		for (const auto& Building : Buildings)
		{
			// Scenario-authored prebuilt units have their own explicit production setup.
			// Only buildings that actually passed through player construction need activation/repair.
			if (Building.ConstructionState != EHansaConstructionState::Completed || Building.ConstructionElapsedTicks <= 0 ||
				Productions.ContainsByPredicate([&](const auto& P) { return P.BuildingId == Building.Id; })) continue;
			const auto* Definition = Registry.FindBuilding(Building.DefinitionId.ToString());
			const auto* Placed = Placement.FindPlacement(Building.Id);
			if (!Definition || Definition->RecipeIds.IsEmpty() || !Placed) continue;
			// The first authored recipe is the default for a newly activated building.
			// Existing scenario/save recipe choices, activity and progress are preserved.
			const auto RecipeId = FHansaRecipeId::TryParse(Definition->RecipeIds[0]);
			if (!RecipeId || !Registry.FindRecipe(Definition->RecipeIds[0]) ||
				Definition->StorageCapacityMilliUnits <= 0) return false;
			uint64 ProductionValue = 0;
			for (const auto& P : Productions) ProductionValue = FMath::Max(ProductionValue, P.Id.GetValue());
			if (ProductionValue == TNumericLimits<uint64>::Max()) return false;
			const auto ProductionId = FHansaProductionId::TryCreate(ProductionValue + 1);
			if (!ProductionId) return false;
			const auto Inventories = InventoryLedger.CreateReadOnlyAccess().BuildProjection();
			FHansaInventoryId BufferId;
			uint64 InventoryValue = 0;
			for (const auto& I : Inventories)
			{
				InventoryValue = FMath::Max(InventoryValue, I.Id.GetValue());
				if (!BufferId.IsValid() && I.OwnerKind == EHansaInventoryOwnerKind::Building &&
					I.BuildingId == Building.Id) BufferId = I.Id;
			}
			if (!BufferId.IsValid())
			{
				if (InventoryValue == TNumericLimits<uint64>::Max()) return false;
				const auto NewId = FHansaInventoryId::TryCreate(InventoryValue + 1);
				if (!NewId) return false;
				FHansaInventoryInitialization Buffer;
				Buffer.Id = NewId.Value;
				Buffer.OwnerKind = EHansaInventoryOwnerKind::Building;
				Buffer.BuildingId = Building.Id;
				Buffer.Capacity = FHansaQuantity::FromRaw(Definition->StorageCapacityMilliUnits);
				for (const auto& Good : Registry.GetGoods())
				{
					const auto Id = FHansaGoodId::TryParse(Good.StableId);
					if (!Id) return false;
					Buffer.AcceptedGoods.Add(Id.Value);
				}
				if (!InventoryLedger.TryAddEmptyInventory(MoveTemp(Buffer))) return false;
				BufferId = NewId.Value;
			}
			FHansaProductionState Production;
			Production.Id = ProductionId.Value;
			Production.BuildingId = Building.Id;
			// Building-recipe city is derived from placement; CityId is reserved for background supply.
			Production.RecipeId = RecipeId.Value;
			Production.InputInventoryId = BufferId;
			Production.OutputInventoryId = BufferId;
			Production.bUsesCityWorkforce = true;
			Productions.Add(MoveTemp(Production)); // Monotonic IDs retain canonical ordering.
		}
		return true;
	}

	namespace
	{
		const FHansaBuildingState* FindProductionBuilding(
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
			const FHansaPlacementState& Placement,
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
                    bool bProtectedSupply = false;
                    if (Input.GoodId == TEXT("Good.Firewood"))
                    {
                        for (const auto& Pool : CandidateLedger.CreateReadOnlyAccess().BuildProjection())
                        {
                            const auto* Placed = Placement.FindPlacement(Production.BuildingId);
                            if (!Placed || Pool.CityId != Placed->Spec.CityId || Pool.OwnerKind != EHansaInventoryOwnerKind::City) continue;
                            const auto Source = CandidateLedger.CreateReadOnlyAccess().QueryStock(Pool.Id, GoodId.GetValue());
                            const int64 Floor = CandidateLedger.CreateReadOnlyAccess().QueryProtectedRaw(Pool.Id, GoodId.GetValue());
                            if (Source && Floor > 0 && Source->Available.GetRawValue() >= Required.GetRawValue() &&
                                Source->Available.GetRawValue() - Floor < Required.GetRawValue()) bProtectedSupply = true;
                        }
                    }
                    SetBlocker(Production, bProtectedSupply ? EHansaProductionBlocker::HouseholdFuelProtected : EHansaProductionBlocker::MissingInput,
                        GoodId.GetValue(), Required, Available);
					return false;
				}
                if (Available.GetRawValue() - CandidateLedger.CreateReadOnlyAccess().QueryProtectedRaw(Production.InputInventoryId, GoodId.GetValue()) < Required.GetRawValue())
                {
                    SetBlocker(Production, EHansaProductionBlocker::HouseholdFuelProtected, GoodId.GetValue(), Required, Available);
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
		case EHansaProductionBlocker::HouseholdFuelProtected: return TEXT("HouseholdFuelProtected");
		case EHansaProductionBlocker::NoNearbyTrees: return TEXT("NoNearbyTrees");
		default: return TEXT("UnknownProductionBlocker");
		}
	}

	int32 CalculateWorkforceAdjustedCycleTicks(
		const int32 BaseCycleTicks,
		const int32 AllocatedLaborerWorkforce,
		const int32 RequiredLaborerWorkforce,
		const int32 AllocatedArtisanWorkforce,
		const int32 RequiredArtisanWorkforce)
	{
		if (BaseCycleTicks <= 0) return 0;
		const int64 Required = static_cast<int64>(FMath::Max(0, RequiredLaborerWorkforce)) +
			FMath::Max(0, RequiredArtisanWorkforce);
		if (Required <= 0) return BaseCycleTicks;
		const int64 Allocated = static_cast<int64>(FMath::Clamp(
			AllocatedLaborerWorkforce, 0, FMath::Max(0, RequiredLaborerWorkforce))) +
			FMath::Clamp(AllocatedArtisanWorkforce, 0, FMath::Max(0, RequiredArtisanWorkforce));
		if (Allocated <= 0) return 0;
		const int64 Numerator = static_cast<int64>(BaseCycleTicks) * Required;
		const int64 Adjusted = (Numerator + Allocated - 1) / Allocated;
		return static_cast<int32>(FMath::Min<int64>(Adjusted, TNumericLimits<int32>::Max()));
	}

	void FHansaProductionExecutor::AdvanceOneTick(
		TArray<FHansaProductionState>& Productions,
		uint64& NextReservationValue,
		const TArray<FHansaBuildingState>& Buildings,
		const FHansaPlacementState& Placement,
		FHansaInventoryLedger& InventoryLedger,
		const FHansaEconomicRegistry* EconomicRegistry,
		const TConstArrayView<FHansaHouseResearchState> Research,
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
				const FHansaBuildingState* Building = FindProductionBuilding(Buildings, Production.BuildingId);
				if (Building == nullptr || Building->ConstructionProgress.GetPartsPerMillion() != FHansaRate::Scale)
				{
					SetBlocker(Production, Building == nullptr
						? EHansaProductionBlocker::MissingDefinition
						: EHansaProductionBlocker::ConstructionIncomplete);
					AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
					continue;
				}
                if(Production.OutputTotals.IsEmpty() && Production.CompletedCycles>0 && EconomicRegistry)
                    if(const auto* Previous=EconomicRegistry->FindRecipe(Production.RecipeId.ToString()))
                    {
                        for(const auto& Output:Previous->Outputs) Production.OutputTotals.Add({FHansaGoodId::TryParse(Output.GoodId).Value,
                            Production.CompletedCycles>uint64(MAX_int64/FMath::Max(int64(1),Output.QuantityMilliUnits))?MAX_int64:int64(Production.CompletedCycles)*Output.QuantityMilliUnits});
                        Production.OutputTotals.Sort([](const auto& A,const auto& B){return A.GoodId<B.GoodId;});
                    }
                if (Production.ProgressTicks == 0 && Production.InputReservations.IsEmpty() && Production.RequestedRecipeId.IsValid() && EconomicRegistry)
                {
                    const auto* Definition = EconomicRegistry->FindBuilding(Building->DefinitionId.ToString());
                    const auto* Selected = EconomicRegistry->FindRecipe(Production.RequestedRecipeId.ToString());
                    if (Definition && Selected && Definition->RecipeIds.Contains(Selected->StableId))
                    {
                        Production.RecipeId = Production.RequestedRecipeId;
                        bool bInputsAvailable = true;
                        for (const auto& Input : Selected->Inputs)
                        {
                            const auto Good = FHansaGoodId::TryParse(Input.GoodId);
                            const auto Stock = Good ? InventoryLedger.CreateReadOnlyAccess().QueryStock(Production.InputInventoryId, Good.Value) : TOptional<FHansaInventoryStockProjection>();
                            if (!Stock || Stock->Available.GetRawValue() < Input.QuantityMilliUnits) bInputsAvailable = false;
                        }
                        if (!bInputsAvailable && Production.bFallbackToFresh && Definition->RecipeIds.Contains(TEXT("Recipe.CatchFish")))
                            Production.RecipeId = FHansaRecipeId::TryParse(TEXT("Recipe.CatchFish")).Value;
                    }
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
                // Pure economic fixtures have no geography. Spatial scenarios fail closed on missing trees/placement.
                if (Recipe->StableId == TEXT("Recipe.FellTimber") && !Placement.GetMaps().IsEmpty() &&
                    !Placement.HasNearbyTrees(Production.BuildingId, LumberHarvestRadiusCells))
                {
                    SetBlocker(Production, EHansaProductionBlocker::NoNearbyTrees);
                    AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
                    continue;
                }
				const int32 RequiredLaborers = FMath::Max(Recipe->LaborerWorkforce, BuildingDefinition->LaborerWorkforce);
				const int32 RequiredArtisans = FMath::Max(Recipe->ArtisanWorkforce, BuildingDefinition->ArtisanWorkforce);
				const int32 AllocatedWorkforce =
					FMath::Min(FMath::Max(0, Production.AllocatedLaborerWorkforce), RequiredLaborers) +
					FMath::Min(FMath::Max(0, Production.AllocatedArtisanWorkforce), RequiredArtisans);
				if (RequiredLaborers + RequiredArtisans > 0 && AllocatedWorkforce <= 0)
				{
					SetBlocker(Production, RequiredLaborers > 0
						? EHansaProductionBlocker::InsufficientLaborerWorkforce
						: EHansaProductionBlocker::InsufficientArtisanWorkforce);
					AddBlockerEventIfChanged(PreviousBlocker, Production, OutEvents);
					continue;
				}
				CycleTicks = CalculateWorkforceAdjustedCycleTicks(
					Recipe->CycleTicks,
					Production.AllocatedLaborerWorkforce, RequiredLaborers,
					Production.AllocatedArtisanWorkforce, RequiredArtisans);
				if (Production.ProgressTicks == 0 && !Recipe->Inputs.IsEmpty() &&
					!TryReserveInputs(Placement, Production, *Recipe, InventoryLedger, NextReservationValue, Tick))
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

			int32 ProgressThisTick = 1;
            const FHansaBuildingState* ResearchBuilding = FindProductionBuilding(Buildings, Production.BuildingId);
			if (Production.Kind == EHansaProductionKind::BuildingRecipe && ResearchBuilding != nullptr)
			{
				const int32 Bonus = FHansaResearchEffectResolver::GetBasisPoints(
					Research, ResearchBuilding->OwnerId, EHansaResearchEffectKind::ProductionThroughputBasisPoints,
					Production.RecipeId.ToString());
				ProgressThisTick = FHansaResearchEffectResolver::WorkUnitsForTick(Bonus, Tick) / 10000;
			}
			if (Production.ProgressTicks < CycleTicks)
			{
				Production.ProgressTicks = FMath::Min(CycleTicks, Production.ProgressTicks + ProgressThisTick);
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
            auto RecordOutput=[&](const FString& Good,int64 Amount,uint64 Cycles=1)
            {
                const auto Id=FHansaGoodId::TryParse(Good).Value;
                auto* Total=Production.OutputTotals.FindByPredicate([&](const auto& T){return T.GoodId==Id;});
                if(!Total){Production.OutputTotals.Add({Id,0});Total=&Production.OutputTotals.Last();}
                const int64 Added=Cycles>uint64(MAX_int64/FMath::Max(int64(1),Amount))?MAX_int64:int64(Cycles)*Amount;
                Total->QuantityMilliUnits+=FMath::Min(MAX_int64-Total->QuantityMilliUnits,Added);
            };
            if(Production.Kind==EHansaProductionKind::BuildingRecipe)for(const auto& Output:Recipe->Outputs) RecordOutput(Output.GoodId,Output.QuantityMilliUnits);
            else RecordOutput(Production.SupplyGoodId.ToString(),Production.SupplyQuantityPerCycle.GetRawValue());
            Production.OutputTotals.Sort([](const auto& A,const auto& B){return A.GoodId<B.GoodId;});
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
