#include "Population/HansaPopulation.h"
#include "Population/HansaSeasonalNeeds.h"

#include "Population/HansaPopulationInternal.h"

#include "Construction/HansaConstruction.h"
#include "Logistics/HansaLocalLogistics.h"
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
		constexpr int32 PopulationBasisPointScale = 10000;
		constexpr int32 InitialHouseholdResidents = 2;

		int32 RatioBasisPoints(const int64 Numerator, const int64 Denominator)
		{
			if (Denominator <= 0) return PopulationBasisPointScale;
			const auto Ratio = FHansaCheckedIntegerMath::TryMultiplyDivide(
				FMath::Max<int64>(0, Numerator), PopulationBasisPointScale, Denominator, EHansaRoundingMode::TowardZero);
			return Ratio ? static_cast<int32>(FMath::Clamp<int64>(Ratio.Value, 0, PopulationBasisPointScale)) : 0;
		}

		int32 WeightedAverage(const int64 WeightedTotal, const int64 Weight)
		{
			return Weight > 0
				? static_cast<int32>(FMath::Clamp<int64>(WeightedTotal / Weight, 0, PopulationBasisPointScale))
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

		bool HasPlacementMap(const FHansaPlacementState& Placement, const FHansaCityDefinitionId CityId)
		{
			for (const FHansaPlacementMapInitialization& Map : Placement.GetMaps())
			{
				if (Map.CityId == CityId) return true;
			}
			return false;
		}

		bool HasCurrentMarketAccess(const FHansaPopulationCohortState& Cohort,
			const FHansaInventoryReadOnlyAccess& Inventories,
			const TArray<FHansaCityMarketState>& Markets,
			const TArray<FHansaBuildingState>& Buildings, const FHansaPlacementState& Placement,
			const FHansaEconomicRegistry& Registry)
		{
			const TOptional<FHansaInventoryProjection> ConsumptionInventory =
				Inventories.QueryInventory(Cohort.ConsumptionInventoryId);
			const bool bMarketStateExists = Markets.ContainsByPredicate([&Cohort](const FHansaCityMarketState& Market)
				{ return Market.CityId == Cohort.CityId; });
			const bool bPhysicalAccess = !HasPlacementMap(Placement, Cohort.CityId) ||
				FHansaLocalLogisticsQueries::QueryBuildingMarketAccess(
					Cohort.ResidenceBuildingId, Cohort.ConsumptionInventoryId,
					Inventories, Placement, Buildings, &Registry).bMarketEligible;
			return ConsumptionInventory.IsSet() &&
				ConsumptionInventory->OwnerKind == EHansaInventoryOwnerKind::City &&
				ConsumptionInventory->CityId == Cohort.CityId &&
				bMarketStateExists && bPhysicalAccess;
		}

		struct FPopulationSupplyPool
		{
			FHansaInventoryId InventoryId;
			FHansaGoodId GoodId;
			int64 OpeningAvailableRaw = 0;
			int64 AllocatedRaw = 0;
		};

		struct FPopulationConsumptionPlan
		{
			FHansaPopulationCohortId CohortId;
			int32 SupplyPoolIndex = INDEX_NONE;
			int64 DesiredRaw = 0;
			int64 AllocatedRaw = 0;
   FString NeedId;
   int32 FoodValue = 10000;
		};

		void RefreshPooledWorkforce(TArray<FHansaPopulationCohortState>& Cohorts,
			const FHansaEconomicRegistry& Registry)
		{
			struct FWorkforceRemainder
			{
				FHansaCityDefinitionId CityId;
				FHansaPopulationTierId TierId;
				int64 BasisPoints = 0;
			};
			TArray<FWorkforceRemainder> Remainders;
			// Cohorts are in canonical ID order. Carry fractional workers between
			// homes in the same city/tier, so their sum equals floor(city-tier total).
			// Attribute the whole workers back to homes for consistent UI/save totals.
			for (FHansaPopulationCohortState& Cohort : Cohorts)
			{
				Cohort.WorkforceSupply = 0;
				if (!Cohort.bResidenceOperational || Cohort.Residents <= 0) continue;
				const auto* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
				if (Tier == nullptr) continue;
				int32 Index = Remainders.IndexOfByPredicate([&Cohort](const auto& Pool)
					{ return Pool.CityId == Cohort.CityId && Pool.TierId == Cohort.TierId; });
				if (Index == INDEX_NONE) Index = Remainders.Add({ Cohort.CityId, Cohort.TierId, 0 });
				auto& Pool = Remainders[Index];
				const int64 Contribution = static_cast<int64>(Cohort.Residents) *
					Tier->WorkforcePerResidentBasisPoints + Pool.BasisPoints;
				Cohort.WorkforceSupply = static_cast<int32>(Contribution / PopulationBasisPointScale);
				Pool.BasisPoints = Contribution % PopulationBasisPointScale;
			}
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
			const FHansaInventoryProjection* ConsumptionInventory = nullptr;
			const bool bRequiresPhysicalMarketAccess = HasPlacementMap(Placement, ResidencePlacement->Spec.CityId);
			for (const FHansaInventoryProjection& Inventory : InventoryProjection)
			{
				if (Inventory.OwnerKind != EHansaInventoryOwnerKind::City ||
					Inventory.CityId != ResidencePlacement->Spec.CityId)
				{
					continue;
				}
				// A completed home owns a cohort even before it is connected to a physical Market.
				// Retain a deterministic city-inventory fallback so the population projection can
				// report zero market access instead of making the residence disappear from the UI.
				if (ConsumptionInventory == nullptr) ConsumptionInventory = &Inventory;
				if (!bRequiresPhysicalMarketAccess ||
					FHansaLocalLogisticsQueries::QueryBuildingMarketAccess(
						Building.Id, Inventory.Id, Inventories, Placement, Buildings, &Registry).bMarketEligible)
				{
					ConsumptionInventory = &Inventory;
					break;
				}
			}
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
			if (ConsumptionInventory == nullptr || !TierId || !CohortId) continue;
			FHansaPopulationCohortState Cohort;
			Cohort.Id = CohortId.Value;
			Cohort.ResidenceBuildingId = Building.Id;
			Cohort.CityId = ResidencePlacement->Spec.CityId;
			Cohort.ConsumptionInventoryId = ConsumptionInventory->Id;
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
		}
		RefreshPooledWorkforce(Cohorts, Registry);

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
			TArray<FHansaProductionState*> CityProductions;
			for (FHansaProductionState& Production : Productions)
			{
				if (!Production.bUsesCityWorkforce || Production.Kind != EHansaProductionKind::BuildingRecipe ||
					ResolveProductionCity(Production, Placement, Inventories) != CityId) continue;
				Production.AllocatedLaborerWorkforce = 0;
				Production.AllocatedArtisanWorkforce = 0;
				if (!Production.bActive) continue;
				CityProductions.Add(&Production);
			}
			// Give as many active units as possible their first worker before filling
			// remaining slots. Stable production order keeps allocation deterministic.
			for (FHansaProductionState* Production : CityProductions)
			{
				const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(Production->RecipeId.ToString());
				const FHansaBuildingState* Building = FindBuilding(Buildings, Production->BuildingId);
				const FHansaCompiledBuildingDefinition* BuildingDefinition = Building != nullptr
					? Registry.FindBuilding(Building->DefinitionId.ToString()) : nullptr;
				if (Recipe == nullptr || BuildingDefinition == nullptr) continue;
				const int32 LaborersRequired = FMath::Max(Recipe->LaborerWorkforce, BuildingDefinition->LaborerWorkforce);
				const int32 ArtisansRequired = FMath::Max(Recipe->ArtisanWorkforce, BuildingDefinition->ArtisanWorkforce);
				if (LaborersRequired > 0 && LaborersAvailable > 0)
				{
					Production->AllocatedLaborerWorkforce = 1;
					--LaborersAvailable;
				}
				else if (ArtisansRequired > 0 && ArtisansAvailable > 0)
				{
					Production->AllocatedArtisanWorkforce = 1;
					--ArtisansAvailable;
				}
			}
			for (FHansaProductionState* Production : CityProductions)
			{
				const FHansaCompiledRecipeDefinition* Recipe = Registry.FindRecipe(Production->RecipeId.ToString());
				const FHansaBuildingState* Building = FindBuilding(Buildings, Production->BuildingId);
				const FHansaCompiledBuildingDefinition* BuildingDefinition = Building != nullptr
					? Registry.FindBuilding(Building->DefinitionId.ToString()) : nullptr;
				if (Recipe == nullptr || BuildingDefinition == nullptr) continue;
				const int32 LaborersRequired = FMath::Max(Recipe->LaborerWorkforce, BuildingDefinition->LaborerWorkforce);
				const int32 ArtisansRequired = FMath::Max(Recipe->ArtisanWorkforce, BuildingDefinition->ArtisanWorkforce);
				const int32 AdditionalLaborers = FMath::Min(LaborersAvailable,
					LaborersRequired - Production->AllocatedLaborerWorkforce);
				const int32 AdditionalArtisans = FMath::Min(ArtisansAvailable,
					ArtisansRequired - Production->AllocatedArtisanWorkforce);
				Production->AllocatedLaborerWorkforce += AdditionalLaborers;
				Production->AllocatedArtisanWorkforce += AdditionalArtisans;
				LaborersAvailable -= AdditionalLaborers;
				ArtisansAvailable -= AdditionalArtisans;
			}
		}
	}

	void FHansaPopulationExecutor::AdvanceOneTick(TArray<FHansaPopulationCohortState>& Cohorts,
		FHansaInventoryLedger& InventoryLedger, const TArray<FHansaCityMarketState>& Markets,
		const TArray<FHansaBuildingState>& Buildings, const FHansaPlacementState& Placement,
		const FHansaEconomicRegistry& Registry,
		const FHansaSimulationTick Tick, const uint32 MinutesPerTick)
	{
		// Plan all household consumption against the same opening stock before applying
		// any transfer. Without this phase, stable cohort order lets the first residence
		// drain a scarce good and makes later residences report zero fulfillment.
		const FHansaInventoryReadOnlyAccess OpeningInventories = InventoryLedger.CreateReadOnlyAccess();
		TArray<FPopulationSupplyPool> SupplyPools;
		TArray<FPopulationConsumptionPlan> ConsumptionPlans;
		for (FHansaPopulationCohortState& Cohort : Cohorts)
		{
			const FHansaCompiledPopulationTierDefinition* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
			const FHansaBuildingState* Building = FindBuilding(Buildings, Cohort.ResidenceBuildingId);
			const bool bOperational = Tier != nullptr && Building != nullptr && Cohort.bResidenceOperational &&
				Building->ConstructionState == EHansaConstructionState::Completed;
            if (bOperational && !HasCurrentMarketAccess(Cohort, OpeningInventories, Markets, Buildings, Placement, Registry))
            {
                for (const auto& Inventory : OpeningInventories.BuildProjection())
                {
                    if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City && Inventory.CityId == Cohort.CityId &&
                        FHansaLocalLogisticsQueries::QueryBuildingMarketAccess(Cohort.ResidenceBuildingId,
                            Inventory.Id, OpeningInventories, Placement, Buildings, &Registry).bMarketEligible)
                    {
                        Cohort.ConsumptionInventoryId = Inventory.Id;
                        break;
                    }
                }
            }
			Cohort.bHasMarketAccess = bOperational && HasCurrentMarketAccess(
				Cohort, OpeningInventories, Markets, Buildings, Placement, Registry);
			if (!bOperational) continue;
			for (const FHansaCompiledPopulationTierNeed& Requirement : Tier->Needs)
			{
				const FHansaCompiledNeedDefinition* NeedDefinition = Registry.FindNeed(Requirement.NeedId);
				if (NeedDefinition == nullptr || NeedDefinition->Kind != EHansaCompiledNeedKind::Good) continue;
				const auto GoodId = FHansaGoodId::TryParse(NeedDefinition->GoodId);
				if (!GoodId) continue;
				int32 PoolIndex = SupplyPools.IndexOfByPredicate([&Cohort, &GoodId](const FPopulationSupplyPool& Pool)
				{ return Pool.InventoryId == Cohort.ConsumptionInventoryId && Pool.GoodId == GoodId.Value; });
				if (PoolIndex == INDEX_NONE)
				{
					FPopulationSupplyPool Pool;
					Pool.InventoryId = Cohort.ConsumptionInventoryId;
					Pool.GoodId = GoodId.Value;
					const TOptional<FHansaInventoryStockProjection> Stock =
						OpeningInventories.QueryStock(Pool.InventoryId, Pool.GoodId);
					Pool.OpeningAvailableRaw = Stock.IsSet() && OpeningInventories.IsHouseholdAvailable(Pool.InventoryId, Pool.GoodId) ? Stock->Available.GetRawValue() : 0;
					PoolIndex = SupplyPools.Add(MoveTemp(Pool));
				}
				if (!Cohort.bHasMarketAccess || Cohort.Residents <= 0) continue;
				const int64 RequiredRaw = FHansaSeasonalNeeds::Demand(*NeedDefinition, Requirement.ConsumptionMilliUnitsPerResidentPerTick, Cohort.Residents, Tick, MinutesPerTick);
				const auto Affordable = FHansaCheckedIntegerMath::TryMultiplyDivide(RequiredRaw,
					Cohort.PurchasingPowerBasisPoints, PopulationBasisPointScale, EHansaRoundingMode::TowardZero);
				if (!Affordable || Affordable.Value <= 0) continue;
				ConsumptionPlans.Add({ Cohort.Id, PoolIndex, Affordable.Value, 0, Requirement.NeedId, 10000 });
			}
		}
  auto AllocatePlans = [&](const int32 FirstPlan)
  {
		for (int32 PoolIndex = 0; PoolIndex < SupplyPools.Num(); ++PoolIndex)
		{
			int64 TotalDesiredRaw = 0;
			for (int32 P = FirstPlan; P < ConsumptionPlans.Num(); ++P)
			{
    const auto& Plan = ConsumptionPlans[P];
				if (Plan.SupplyPoolIndex != PoolIndex) continue;
				const auto Added = FHansaCheckedIntegerMath::TryAdd(TotalDesiredRaw, Plan.DesiredRaw);
				TotalDesiredRaw = Added ? Added.Value : TNumericLimits<int64>::Max();
			}
			const int64 SupplyRaw = FMath::Min(SupplyPools[PoolIndex].OpeningAvailableRaw - SupplyPools[PoolIndex].AllocatedRaw, TotalDesiredRaw);
			if (SupplyRaw <= 0 || TotalDesiredRaw <= 0) continue;
			int64 DistributedRaw = 0;
			for (int32 P = FirstPlan; P < ConsumptionPlans.Num(); ++P)
			{
    auto& Plan = ConsumptionPlans[P];
				if (Plan.SupplyPoolIndex != PoolIndex) continue;
				const auto Share = FHansaCheckedIntegerMath::TryMultiplyDivide(
					SupplyRaw, Plan.DesiredRaw, TotalDesiredRaw, EHansaRoundingMode::TowardZero);
				Plan.AllocatedRaw = Share ? FMath::Min(Plan.DesiredRaw, Share.Value) : 0;
				DistributedRaw += Plan.AllocatedRaw;
			}
			int64 RemainderRaw = SupplyRaw - DistributedRaw;
			while (RemainderRaw > 0)
			{
				bool bGrantedAny = false;
    // Rotate the last milli-unit between cohorts; stable ordering never permanently wins it.
    const int32 Count = ConsumptionPlans.Num() - FirstPlan;
				for (int32 Offset = 0; Offset < Count; ++Offset)
				{
     auto& Plan = ConsumptionPlans[FirstPlan + (Offset + Tick.GetValue() % Count) % Count];
					if (Plan.SupplyPoolIndex != PoolIndex || Plan.AllocatedRaw >= Plan.DesiredRaw) continue;
					++Plan.AllocatedRaw;
					--RemainderRaw;
					bGrantedAny = true;
					if (RemainderRaw == 0) break;
				}
				if (!bGrantedAny) break;
			}
			SupplyPools[PoolIndex].AllocatedRaw += SupplyRaw - RemainderRaw;
		}
  };
  auto FoodValue = [](int64 Raw, int32 Factor)
  {
   const auto Value=FHansaCheckedIntegerMath::TryMultiplyDivide(Raw,Factor,10000,EHansaRoundingMode::TowardZero);
   return Value ? Value.Value : MAX_int64;
  };
  AllocatePlans(0);
  int32 AlternativeCount = 0;
  for (const auto& Need : Registry.GetNeeds()) AlternativeCount = FMath::Max(AlternativeCount, Need.Alternatives.Num());
  for (int32 Preference = 0; Preference < AlternativeCount; ++Preference)
  {
   const int32 FirstPlan = ConsumptionPlans.Num();
   for (const auto& Cohort : Cohorts)
   {
    const auto* Tier = Registry.FindPopulationTier(Cohort.TierId.ToString());
    if (!Tier || !Cohort.bResidenceOperational || !Cohort.bHasMarketAccess || Cohort.Residents <= 0) continue;
    for (const auto& Requirement : Tier->Needs)
    {
     const auto* Need = Registry.FindNeed(Requirement.NeedId);
     if (!Need || !Need->Alternatives.IsValidIndex(Preference)) continue;
     const auto& Alternative = Need->Alternatives[Preference];
     const auto Good = FHansaGoodId::TryParse(Alternative.GoodId);
     if (!Good || Alternative.FulfillmentBasisPoints <= 0) continue;
     const auto AffordableAlternative = FHansaCheckedIntegerMath::TryMultiplyDivide(
      FHansaSeasonalNeeds::Demand(*Need, Requirement.ConsumptionMilliUnitsPerResidentPerTick, Cohort.Residents, Tick, MinutesPerTick),
      Cohort.PurchasingPowerBasisPoints, 10000, EHansaRoundingMode::TowardZero);
     int64 Remaining = AffordableAlternative ? AffordableAlternative.Value : 0;
     for (const auto& Plan : ConsumptionPlans)
      if (Plan.CohortId == Cohort.Id && Plan.NeedId == Requirement.NeedId)
       Remaining -= FoodValue(Plan.AllocatedRaw, Plan.FoodValue);
     int32 PoolIndex = SupplyPools.IndexOfByPredicate([&](const auto& Pool)
      { return Pool.InventoryId == Cohort.ConsumptionInventoryId && Pool.GoodId == Good.Value; });
     if (PoolIndex == INDEX_NONE)
     {
      const auto Stock = OpeningInventories.QueryStock(Cohort.ConsumptionInventoryId, Good.Value);
      PoolIndex = SupplyPools.Add({Cohort.ConsumptionInventoryId, Good.Value,
       Stock.IsSet() && OpeningInventories.IsHouseholdAvailable(Cohort.ConsumptionInventoryId, Good.Value) ? Stock->Available.GetRawValue() : 0, 0});
     }
     const auto Desired = FHansaCheckedIntegerMath::TryMultiplyDivide(FMath::Max<int64>(0, Remaining), 10000,
      Alternative.FulfillmentBasisPoints, EHansaRoundingMode::TowardZero);
     if (Desired && Desired.Value > 0)
      ConsumptionPlans.Add({Cohort.Id, PoolIndex, Desired.Value, 0, Requirement.NeedId, Alternative.FulfillmentBasisPoints});
    }
   }
   AllocatePlans(FirstPlan);
  }

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

			int64 AccessTotal = 0;
			int64 AffordabilityTotal = 0;
			int64 ReliabilityTotal = 0;
			int64 SatisfactionTotal = 0;
			int64 TotalWeight = 0;
			bool bBasicServicesSatisfied = false;
			for (const FHansaCompiledPopulationTierNeed& Requirement : Tier->Needs)
			{
				const FHansaCompiledNeedDefinition* NeedDefinition = Registry.FindNeed(Requirement.NeedId);
				const auto NeedId = FHansaNeedId::TryParse(Requirement.NeedId);
				if (NeedDefinition == nullptr || !NeedId) continue;
				if (FHansaSeasonalNeeds::Multiplier(*NeedDefinition, Tick, MinutesPerTick) == 0) continue;
				FHansaPopulationNeedState Need;
				Need.NeedId = NeedId.Value;
				Need.AffordabilityBasisPoints = Cohort.PurchasingPowerBasisPoints;
				if (NeedDefinition->Kind == EHansaCompiledNeedKind::Service)
				{
					Need.AccessBasisPoints = Cohort.ServiceAccessBasisPoints;
					Need.ReliabilityBasisPoints = Cohort.ServiceReliabilityBasisPoints;
					Need.SatisfactionBasisPoints = FMath::Min3(Need.AccessBasisPoints,
						Need.AffordabilityBasisPoints, Need.ReliabilityBasisPoints);
					bBasicServicesSatisfied |= Cohort.bHasMarketAccess &&
						Requirement.NeedId == TEXT("Need.BasicServices") &&
						Need.SatisfactionBasisPoints == PopulationBasisPointScale;
				}
				else
				{
					const auto GoodId = FHansaGoodId::TryParse(NeedDefinition->GoodId);
					if (GoodId)
					{
						Need.GoodId = GoodId.Value;
						const int64 RequiredRaw = FHansaSeasonalNeeds::Demand(*NeedDefinition, Requirement.ConsumptionMilliUnitsPerResidentPerTick, Cohort.Residents, Tick, MinutesPerTick);
						Need.RequiredLastTick = FHansaQuantity::FromRaw(RequiredRaw);
      int64 OpeningAvailableRaw = 0, ProspectiveAvailableRaw = 0, ConsumedRaw = 0;
      auto CountSupply = [&](const FString& Id, const int32 Factor)
      {
       const int32 Index = SupplyPools.IndexOfByPredicate([&](const auto& Pool)
        { return Pool.InventoryId == Cohort.ConsumptionInventoryId && Pool.GoodId.ToString() == Id; });
       if (Index != INDEX_NONE)
       {
        OpeningAvailableRaw += FMath::Min(MAX_int64 - OpeningAvailableRaw, FoodValue(SupplyPools[Index].OpeningAvailableRaw, Factor));
        ProspectiveAvailableRaw += FMath::Min(MAX_int64 - ProspectiveAvailableRaw, FoodValue(FMath::Max<int64>(0, SupplyPools[Index].OpeningAvailableRaw - SupplyPools[Index].AllocatedRaw), Factor));
       }
       else
       {
        const auto Good=FHansaGoodId::TryParse(Id);
        const auto Stock=Good ? OpeningInventories.QueryStock(Cohort.ConsumptionInventoryId,Good.Value) : TOptional<FHansaInventoryStockProjection>();
        if (Stock && OpeningInventories.IsHouseholdAvailable(Cohort.ConsumptionInventoryId,Good.Value))
        {const int64 Raw=FoodValue(Stock->Available.GetRawValue(),Factor);OpeningAvailableRaw+=FMath::Min(MAX_int64-OpeningAvailableRaw,Raw);ProspectiveAvailableRaw+=FMath::Min(MAX_int64-ProspectiveAvailableRaw,Raw);}
       }
      };
      CountSupply(NeedDefinition->GoodId, 10000);
      for (const auto& Alternative : NeedDefinition->Alternatives) CountSupply(Alternative.GoodId, Alternative.FulfillmentBasisPoints);
      for (const auto& Plan : ConsumptionPlans)
      {
       if (Plan.CohortId != Cohort.Id || Plan.NeedId != Requirement.NeedId || Plan.AllocatedRaw <= 0) continue;
       const auto& Pool = SupplyPools[Plan.SupplyPoolIndex];
       const uint64 Sequence = InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1;
       const auto Transfer = InventoryLedger.TryTransfer(FHansaInventoryEndpoint::Inventory(Cohort.ConsumptionInventoryId),
        FHansaInventoryEndpoint::Sink(TEXT("PopulationConsumption")), Pool.GoodId, FHansaQuantity::FromRaw(Plan.AllocatedRaw), Tick, Sequence);
       if (Transfer.IsSuccess())
       {
        const int64 Food = FoodValue(Transfer.AppliedQuantity.GetRawValue(), Plan.FoodValue);
        ConsumedRaw += Food;
        Need.SuppliedGoods.Add({Pool.GoodId, Transfer.AppliedQuantity.GetRawValue(), Food});
       }
      }
      Need.SuppliedGoods.Sort([](const auto& A,const auto& B){return A.GoodId<B.GoodId;});
      Need.ConsumedLastTick = FHansaQuantity::FromRaw(ConsumedRaw);
      Need.AccessBasisPoints = Cohort.bHasMarketAccess && OpeningAvailableRaw > 0 ? PopulationBasisPointScale : 0;
      const auto Affordable = FHansaCheckedIntegerMath::TryMultiplyDivide(RequiredRaw,
       Cohort.PurchasingPowerBasisPoints, PopulationBasisPointScale, EHansaRoundingMode::TowardZero);
      const int64 DesiredRaw = Affordable ? Affordable.Value : 0;
      Need.ReliabilityBasisPoints = Cohort.Residents == 0
       ? RatioBasisPoints(ProspectiveAvailableRaw, Requirement.ConsumptionMilliUnitsPerResidentPerTick)
       : RatioBasisPoints(ConsumedRaw, DesiredRaw);
      Need.SatisfactionBasisPoints = FMath::Min3(Need.AccessBasisPoints, Need.AffordabilityBasisPoints, Need.ReliabilityBasisPoints);
      if (RequiredRaw > 0)
      {
       const auto Reserve = FHansaCheckedIntegerMath::TryMultiplyDivide(OpeningAvailableRaw,
        static_cast<int64>(MinutesPerTick) * 1000, RequiredRaw * 1440, EHansaRoundingMode::TowardZero);
       Need.ReserveMilliDays = Reserve ? FMath::Max<int64>(0, Reserve.Value) : 0;
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
			if (Cohort.Residents == 0 && bBasicServicesSatisfied)
			{
				Cohort.Residents = FMath::Min(InitialHouseholdResidents, Cohort.ResidenceCapacity);
				Cohort.ResidentChangeLastTick = Cohort.Residents;
				Cohort.ConsecutiveGrowthTicks = 0;
				Cohort.ConsecutiveDeclineTicks = 0;
				continue;
			}

			const auto Recent = Cohort.ConsumptionHistory.RecentTotals(Tier->EvaluationTicks - 1);
			int64 MigrationWeightedTotal = 0;
			const FHansaCompiledPopulationTierNeed* Staple = nullptr;
			for (const auto& Requirement : Tier->Needs)
			{
				const auto* Need = Cohort.Needs.FindByPredicate([&Requirement](const auto& Item)
					{ return Item.NeedId.ToString() == Requirement.NeedId; });
				if (!Need) continue;
				int32 Fulfillment = Need->SatisfactionBasisPoints;
				if (Need->GoodId.IsValid())
				{
					const auto* History = Recent.FindByPredicate([Need](const auto& Item) { return Item.GoodId == Need->GoodId; });
					Fulfillment = RatioBasisPoints(Need->ConsumedLastTick.GetRawValue() + (History ? History->Consumed : 0),
						Need->RequiredLastTick.GetRawValue() + (History ? History->Required : 0));
					if (!Staple || Requirement.ImportanceBasisPoints > Staple->ImportanceBasisPoints) Staple = &Requirement;
				}
				MigrationWeightedTotal += static_cast<int64>(Fulfillment) * Requirement.ImportanceBasisPoints;
			}
			const int32 MigrationSatisfaction = WeightedAverage(MigrationWeightedTotal, TotalWeight);
			bool bGrowthReserve = true;
			if (Staple)
			{
				const auto* Definition = Registry.FindNeed(Staple->NeedId);
				const auto Good = FHansaGoodId::TryParse(Definition->GoodId);
				const auto Stock = InventoryLedger.CreateReadOnlyAccess().QueryStock(Cohort.ConsumptionInventoryId, Good.Value);
				int64 DemandAfterGrowth = static_cast<int64>(FMath::Min(Tier->GrowthResidentsPerEvaluation,
					Cohort.ResidenceCapacity - Cohort.Residents)) * Staple->ConsumptionMilliUnitsPerResidentPerTick;
				for (const auto& Other : Cohorts)
				{
					if (!Other.bResidenceOperational || Other.ConsumptionInventoryId != Cohort.ConsumptionInventoryId) continue;
					const auto* OtherTier = Registry.FindPopulationTier(Other.TierId.ToString());
					if (!OtherTier) continue;
					for (const auto& Requirement : OtherTier->Needs)
					{
						const auto* OtherNeed = Registry.FindNeed(Requirement.NeedId);
						if (OtherNeed && OtherNeed->GoodId == Definition->GoodId)
							DemandAfterGrowth += static_cast<int64>(Other.Residents) * Requirement.ConsumptionMilliUnitsPerResidentPerTick;
					}
				}
				const auto ReserveRequired = FHansaCheckedIntegerMath::TryMultiplyDivide(
					DemandAfterGrowth, FMath::Max(Tier->EvaluationTicks,
						FHansaConsumptionHistory::WindowTicks(MinutesPerTick)), 1, EHansaRoundingMode::TowardZero);
				bGrowthReserve = ReserveRequired && Stock.IsSet() && Stock->Available.GetRawValue() >= ReserveRequired.Value;
			}
			// Empty homes enter only through the founding-household rule above. Zero
			// demand must not count as satisfied migration while a market is still building.
			if (Cohort.Residents > 0 && Cohort.bHasMarketAccess &&
				MigrationSatisfaction >= Tier->GrowthSatisfactionBasisPoints && bGrowthReserve)
			{
				++Cohort.ConsecutiveGrowthTicks;
				Cohort.ConsecutiveDeclineTicks = 0;
			}
			else if (MigrationSatisfaction <= Tier->DeclineSatisfactionBasisPoints)
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
		}
		RefreshPooledWorkforce(Cohorts, Registry);
	}
}
