#include "Population/HansaPopulation.h"

#include "Population/HansaPopulationInternal.h"

#include "Construction/HansaConstruction.h"
#include "Math/NumericLimits.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaPopulationTrend Trend)
	{
		switch (Trend)
		{
		case EHansaPopulationTrend::Declining: return TEXT("Declining");
		case EHansaPopulationTrend::Stable: return TEXT("Stable");
		case EHansaPopulationTrend::Growing: return TEXT("Growing");
		default: return TEXT("Unknown");
		}
	}

	namespace
	{
		constexpr int32 BasisPointScale = 10000;

		int32 RatioBasisPoints(const int64 Numerator, const int64 Denominator)
		{
			if (Denominator <= 0) return BasisPointScale;
			const auto Ratio = FHansaCheckedIntegerMath::TryMultiplyDivide(
				FMath::Max<int64>(0, Numerator), BasisPointScale, Denominator, EHansaRoundingMode::TowardZero);
			return Ratio ? static_cast<int32>(FMath::Clamp<int64>(Ratio.Value, 0, BasisPointScale)) : 0;
		}

		int32 WeightedAverage(const int64 WeightedTotal, const int64 Weight)
		{
			return Weight > 0
				? static_cast<int32>(FMath::Clamp<int64>(WeightedTotal / Weight, 0, BasisPointScale))
				: 0;
		}

		const FHansaBuildingState* FindBuilding(const TArray<FHansaBuildingState>& Buildings,
			const FHansaBuildingId BuildingId)
		{
			return Buildings.FindByPredicate([BuildingId](const FHansaBuildingState& Building)
			{
				return Building.Id == BuildingId;
			});
		}

		FHansaCityDefinitionId ResolveProductionCity(const FHansaProductionState& Production,
			const FHansaPlacementState& Placement, const FHansaInventoryReadOnlyAccess& Inventories)
		{
			if (Production.CityId.IsValid()) return Production.CityId;
			if (const FHansaPlacedBuildingRecord* Record = Placement.FindPlacement(Production.BuildingId))
			{
				return Record->Spec.CityId;
			}
			const TOptional<FHansaInventoryProjection> Input = Inventories.QueryInventory(Production.InputInventoryId);
			if (Input.IsSet() && Input->OwnerKind == EHansaInventoryOwnerKind::City) return Input->CityId;
			const TOptional<FHansaInventoryProjection> Output = Inventories.QueryInventory(Production.OutputInventoryId);
			return Output.IsSet() && Output->OwnerKind == EHansaInventoryOwnerKind::City
				? Output->CityId : FHansaCityDefinitionId();
		}

		bool IsArtisanTier(const FHansaCompiledPopulationTierDefinition& Tier)
		{
			return !Tier.PreviousTierId.IsEmpty();
		}
	}

	void FHansaPopulationExecutor::SynchronizeResidencesAndAssignWorkforce(
		TArray<FHansaPopulationCohortState>& Cohorts, TArray<FHansaProductionState>& Productions,
		const TArray<FHansaBuildingState>& Buildings, const FHansaPlacementState& Placement,
		const FHansaInventoryLedger& InventoryLedger, const FHansaEconomicRegistry& Registry)
	{
		const FHansaInventoryReadOnlyAccess Inventories = InventoryLedger.CreateReadOnlyAccess();
		const TArray<FHansaInventoryProjection> InventoryProjection = Inventories.BuildProjection();
		for (const FHansaBuildingState& Building : Buildings)
		{
			if (Building.ConstructionState != EHansaConstructionState::Completed) continue;
			const FHansaCompiledBuildingDefinition* Definition = Registry.FindBuilding(Building.DefinitionId.ToString());
			if (Definition == nullptr || Definition->ResidenceCapacity <= 0 ||
				Definition->ResidentPopulationTierId.IsEmpty() ||
				Cohorts.ContainsByPredicate([&Building](const FHansaPopulationCohortState& Cohort)
				{ return Cohort.ResidenceBuildingId == Building.Id; }))
			{
				continue;
			}
			const FHansaPlacedBuildingRecord* ResidencePlacement = Placement.FindPlacement(Building.Id);
			if (ResidencePlacement == nullptr) continue;
			const FHansaInventoryProjection* MarketInventory = InventoryProjection.FindByPredicate(
				[ResidencePlacement](const FHansaInventoryProjection& Inventory)
				{
					return Inventory.OwnerKind == EHansaInventoryOwnerKind::City &&
						Inventory.CityId == ResidencePlacement->Spec.CityId;
				});
			const auto TierId = FHansaPopulationTierId::TryParse(Definition->ResidentPopulationTierId);
			uint64 CohortValue = Building.Id.GetValue();
			auto CohortId = FHansaPopulationCohortId::TryCreate(CohortValue, Building.Id.GetGeneration());
			while (CohortId && Cohorts.ContainsByPredicate([&CohortId](const FHansaPopulationCohortState& Existing)
				{ return Existing.Id == CohortId.Value; }))
			{
				if (CohortValue == TNumericLimits<uint64>::Max())
				{
					CohortId = THansaValueResult<FHansaPopulationCohortId>::Failure(EHansaValueError::OutOfRange);
					break;
				}
				CohortId = FHansaPopulationCohortId::TryCreate(++CohortValue, Building.Id.GetGeneration());
			}
			if (MarketInventory == nullptr || !TierId || !CohortId) continue;
			FHansaPopulationCohortState Cohort;
			Cohort.Id = CohortId.Value;
			Cohort.ResidenceBuildingId = Building.Id;
			Cohort.CityId = ResidencePlacement->Spec.CityId;
			Cohort.ConsumptionInventoryId = MarketInventory->Id;
			Cohort.TierId = TierId.Value;
			Cohort.ResidenceCapacity = Definition->ResidenceCapacity;
			Cohorts.Add(MoveTemp(Cohort));
		}
		Cohorts.Sort([](const FHansaPopulationCohortState& Left, const FHansaPopulationCohortState& Right)
		{
			return Left.Id < Right.Id;
		});

		for (FHansaPopulationCohortState& Cohort : Cohorts)
		{
			const FHansaBuildingState* Building = FindBuilding(Buildings, Cohort.ResidenceBuildingId);
			const FHansaCompiledBuildingDefinition* Definition = Building != nullptr
				? Registry.FindBuilding(Building->DefinitionId.ToString()) : nullptr;
			Cohort.bResidenceOperational = Building != nullptr &&
				Building->ConstructionState == EHansaConstructionState::Completed && Definition != nullptr &&
				Definition->ResidenceCapacity > 0 && !Definition->ResidentPopulationTierId.IsEmpty();
			if (!Cohort.bResidenceOperational)
			{
				Cohort.WorkforceSupply = 0;
				continue;
			}
			const auto TierId = FHansaPopulationTierId::TryParse(Definition->ResidentPopulationTierId);
			if (!TierId)
			{
				Cohort.bResidenceOperational = false;
				Cohort.WorkforceSupply = 0;
				continue;
			}
			Cohort.TierId = TierId.Value;
			Cohort.ResidenceCapacity = Definition->ResidenceCapacity;
			Cohort.Residents = FMath::Min(Cohort.Residents, Cohort.ResidenceCapacity);
			const FHansaCompiledPopulationTierDefinition* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
			Cohort.WorkforceSupply = Tier != nullptr
				? static_cast<int32>((static_cast<int64>(Cohort.Residents) *
					Tier->WorkforcePerResidentBasisPoints) / BasisPointScale)
				: 0;
		}

		TArray<FHansaCityDefinitionId> Cities;
		for (const FHansaPopulationCohortState& Cohort : Cohorts) Cities.AddUnique(Cohort.CityId);
		Cities.Sort();
		for (const FHansaCityDefinitionId CityId : Cities)
		{
			int32 LaborersAvailable = 0;
			int32 ArtisansAvailable = 0;
			for (const FHansaPopulationCohortState& Cohort : Cohorts)
			{
				if (Cohort.CityId != CityId || !Cohort.bResidenceOperational) continue;
				const FHansaCompiledPopulationTierDefinition* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
				if (Tier != nullptr && IsArtisanTier(*Tier)) ArtisansAvailable += Cohort.WorkforceSupply;
				else LaborersAvailable += Cohort.WorkforceSupply;
			}
			for (FHansaProductionState& Production : Productions)
			{
				if (!Production.bUsesCityWorkforce || Production.Kind != EHansaProductionKind::BuildingRecipe ||
					ResolveProductionCity(Production, Placement, Inventories) != CityId) continue;
				Production.AllocatedLaborerWorkforce = 0;
				Production.AllocatedArtisanWorkforce = 0;
				if (!Production.bActive) continue;
				const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(Production.RecipeId.ToString());
				const FHansaBuildingState* Building = FindBuilding(Buildings, Production.BuildingId);
				const FHansaCompiledBuildingDefinition* BuildingDefinition = Building != nullptr
					? Registry.FindBuilding(Building->DefinitionId.ToString()) : nullptr;
				if (Recipe == nullptr || BuildingDefinition == nullptr) continue;
				const int32 LaborersRequired = FMath::Max(Recipe->LaborerWorkforce, BuildingDefinition->LaborerWorkforce);
				const int32 ArtisansRequired = FMath::Max(Recipe->ArtisanWorkforce, BuildingDefinition->ArtisanWorkforce);
				Production.AllocatedLaborerWorkforce = FMath::Min(LaborersAvailable, LaborersRequired);
				Production.AllocatedArtisanWorkforce = FMath::Min(ArtisansAvailable, ArtisansRequired);
				LaborersAvailable -= Production.AllocatedLaborerWorkforce;
				ArtisansAvailable -= Production.AllocatedArtisanWorkforce;
			}
		}
	}

	void FHansaPopulationExecutor::AdvanceOneTick(TArray<FHansaPopulationCohortState>& Cohorts,
		FHansaInventoryLedger& InventoryLedger, const TArray<FHansaCityMarketState>& Markets,
		const TArray<FHansaBuildingState>& Buildings, const FHansaEconomicRegistry& Registry,
		const FHansaSimulationTick Tick, const uint32 MinutesPerTick)
	{
		for (FHansaPopulationCohortState& Cohort : Cohorts)
		{
			Cohort.Needs.Reset();
			Cohort.ResidentChangeLastTick = 0;
			const FHansaCompiledPopulationTierDefinition* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
			const FHansaBuildingState* Building = FindBuilding(Buildings, Cohort.ResidenceBuildingId);
			if (Tier == nullptr || Building == nullptr || !Cohort.bResidenceOperational ||
				Building->ConstructionState != EHansaConstructionState::Completed)
			{
				Cohort.AccessBasisPoints = 0;
				Cohort.AffordabilityBasisPoints = 0;
				Cohort.ReliabilityBasisPoints = 0;
				Cohort.SatisfactionBasisPoints = 0;
				Cohort.WorkforceSupply = 0;
				Cohort.bHasMarketAccess = false;
				continue;
			}

			const TOptional<FHansaInventoryProjection> ConsumptionInventory =
				InventoryLedger.CreateReadOnlyAccess().QueryInventory(Cohort.ConsumptionInventoryId);
			Cohort.bHasMarketAccess = ConsumptionInventory.IsSet() &&
				ConsumptionInventory->OwnerKind == EHansaInventoryOwnerKind::City &&
				ConsumptionInventory->CityId == Cohort.CityId &&
				Markets.ContainsByPredicate([&Cohort](const FHansaCityMarketState& Market)
				{ return Market.CityId == Cohort.CityId; });

			int64 AccessTotal = 0;
			int64 AffordabilityTotal = 0;
			int64 ReliabilityTotal = 0;
			int64 SatisfactionTotal = 0;
			int64 TotalWeight = 0;
			for (const FHansaCompiledPopulationTierNeed& Requirement : Tier->Needs)
			{
				const FHansaCompiledNeedDefinition* NeedDefinition = Registry.FindNeed(Requirement.NeedId);
				const auto NeedId = FHansaNeedId::TryParse(Requirement.NeedId);
				if (NeedDefinition == nullptr || !NeedId) continue;
				FHansaPopulationNeedState Need;
				Need.NeedId = NeedId.Value;
				Need.AffordabilityBasisPoints = Cohort.PurchasingPowerBasisPoints;
				if (NeedDefinition->Kind == EHansaCompiledNeedKind::Service)
				{
					Need.AccessBasisPoints = Cohort.ServiceAccessBasisPoints;
					Need.ReliabilityBasisPoints = Cohort.ServiceReliabilityBasisPoints;
					Need.SatisfactionBasisPoints = FMath::Min3(Need.AccessBasisPoints,
						Need.AffordabilityBasisPoints, Need.ReliabilityBasisPoints);
				}
				else
				{
					const auto GoodId = FHansaGoodId::TryParse(NeedDefinition->GoodId);
					if (GoodId)
					{
						Need.GoodId = GoodId.Value;
						const int64 RequiredRaw = static_cast<int64>(Cohort.Residents) *
							Requirement.ConsumptionMilliUnitsPerResidentPerTick;
						Need.RequiredLastTick = FHansaQuantity::FromRaw(RequiredRaw);
						const TOptional<FHansaInventoryStockProjection> Stock =
							InventoryLedger.CreateReadOnlyAccess().QueryStock(Cohort.ConsumptionInventoryId, GoodId.Value);
						const int64 AvailableRaw = Stock.IsSet() ? Stock->Available.GetRawValue() : 0;
						Need.AccessBasisPoints = Cohort.bHasMarketAccess &&
							Stock.IsSet() && AvailableRaw > 0 ? BasisPointScale : 0;
						const auto Affordable = FHansaCheckedIntegerMath::TryMultiplyDivide(RequiredRaw,
							Cohort.PurchasingPowerBasisPoints, BasisPointScale, EHansaRoundingMode::TowardZero);
						const int64 DesiredRaw = Affordable ? Affordable.Value : 0;
						const int64 ConsumedRaw = Need.AccessBasisPoints > 0 ? FMath::Min(AvailableRaw, DesiredRaw) : 0;
						Need.ReliabilityBasisPoints = RatioBasisPoints(ConsumedRaw, DesiredRaw);
						Need.SatisfactionBasisPoints = FMath::Min3(Need.AccessBasisPoints,
							Need.AffordabilityBasisPoints, Need.ReliabilityBasisPoints);
						if (RequiredRaw > 0)
						{
							const auto Reserve = FHansaCheckedIntegerMath::TryMultiplyDivide(AvailableRaw,
								static_cast<int64>(MinutesPerTick) * 1000, RequiredRaw * 1440,
								EHansaRoundingMode::TowardZero);
							Need.ReserveMilliDays = Reserve ? FMath::Max<int64>(0, Reserve.Value) : 0;
						}
						if (ConsumedRaw > 0)
						{
							const uint64 Sequence = InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1;
							const FHansaInventoryTransactionResult Transfer = InventoryLedger.TryTransfer(
								FHansaInventoryEndpoint::Inventory(Cohort.ConsumptionInventoryId),
								FHansaInventoryEndpoint::Sink(TEXT("PopulationConsumption")), GoodId.Value,
								FHansaQuantity::FromRaw(ConsumedRaw), Tick, Sequence);
							if (Transfer.IsSuccess()) Need.ConsumedLastTick = Transfer.AppliedQuantity;
							else
							{
								Need.ReliabilityBasisPoints = 0;
								Need.SatisfactionBasisPoints = 0;
							}
						}
					}
				}
				const int64 Weight = Requirement.ImportanceBasisPoints;
				TotalWeight += Weight;
				AccessTotal += static_cast<int64>(Need.AccessBasisPoints) * Weight;
				AffordabilityTotal += static_cast<int64>(Need.AffordabilityBasisPoints) * Weight;
				ReliabilityTotal += static_cast<int64>(Need.ReliabilityBasisPoints) * Weight;
				SatisfactionTotal += static_cast<int64>(Need.SatisfactionBasisPoints) * Weight;
				Cohort.Needs.Add(MoveTemp(Need));
			}

			Cohort.AccessBasisPoints = WeightedAverage(AccessTotal, TotalWeight);
			Cohort.AffordabilityBasisPoints = WeightedAverage(AffordabilityTotal, TotalWeight);
			Cohort.ReliabilityBasisPoints = WeightedAverage(ReliabilityTotal, TotalWeight);
			Cohort.SatisfactionBasisPoints = WeightedAverage(SatisfactionTotal, TotalWeight);
			Cohort.WorkforceSupply = static_cast<int32>((static_cast<int64>(Cohort.Residents) *
				Tier->WorkforcePerResidentBasisPoints) / BasisPointScale);

			if (Cohort.SatisfactionBasisPoints >= Tier->GrowthSatisfactionBasisPoints)
			{
				++Cohort.ConsecutiveGrowthTicks;
				Cohort.ConsecutiveDeclineTicks = 0;
			}
			else if (Cohort.SatisfactionBasisPoints <= Tier->DeclineSatisfactionBasisPoints)
			{
				++Cohort.ConsecutiveDeclineTicks;
				Cohort.ConsecutiveGrowthTicks = 0;
			}
			else
			{
				Cohort.ConsecutiveGrowthTicks = 0;
				Cohort.ConsecutiveDeclineTicks = 0;
			}
			if (Cohort.ConsecutiveGrowthTicks >= Tier->EvaluationTicks)
			{
				const int32 Before = Cohort.Residents;
				Cohort.Residents = FMath::Min(Cohort.ResidenceCapacity,
					Cohort.Residents + Tier->GrowthResidentsPerEvaluation);
				Cohort.ResidentChangeLastTick = Cohort.Residents - Before;
				Cohort.ConsecutiveGrowthTicks = 0;
			}
			else if (Cohort.ConsecutiveDeclineTicks >= Tier->EvaluationTicks)
			{
				const int32 Before = Cohort.Residents;
				Cohort.Residents = FMath::Max(0, Cohort.Residents - Tier->DeclineResidentsPerEvaluation);
				Cohort.ResidentChangeLastTick = Cohort.Residents - Before;
				Cohort.ConsecutiveDeclineTicks = 0;
			}
			Cohort.WorkforceSupply = static_cast<int32>((static_cast<int64>(Cohort.Residents) *
				Tier->WorkforcePerResidentBasisPoints) / BasisPointScale);
		}
	}
}
