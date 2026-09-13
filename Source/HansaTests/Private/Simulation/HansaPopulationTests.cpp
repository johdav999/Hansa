#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Population
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue Require(const THansaValueResult<TValue>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename TId>
	TId PopulationTestsEntity(const uint64 Value) { return Require(TId::TryCreate(Value)); }

	FHansaGoodId Good(const TCHAR* Value) { return Require(FHansaGoodId::TryParse(Value)); }
	FHansaBuildingTypeId BuildingType(const TCHAR* Value) { return Require(FHansaBuildingTypeId::TryParse(Value)); }

	FHansaEconomicRegistry MakeRegistry(const int32 EvaluationTicks = 2,
		const int64 UpgradeCurrencyCost = 1000, const int64 UpgradeBreadCost = 1000)
	{
		FHansaCompiledGoodDefinition Bread;
		Bread.StableId = TEXT("Good.Bread");
		Bread.BaseValueMilliMarks = 1000;
		TArray<FHansaCompiledGoodDefinition> Goods { Bread };

		FHansaCompiledNeedDefinition BreadNeed;
		BreadNeed.StableId = TEXT("Need.Bread");
		BreadNeed.Kind = EHansaCompiledNeedKind::Good;
		BreadNeed.GoodId = TEXT("Good.Bread");
		FHansaCompiledNeedDefinition Services;
		Services.StableId = TEXT("Need.BasicServices");
		Services.Kind = EHansaCompiledNeedKind::Service;
		TArray<FHansaCompiledNeedDefinition> Needs { BreadNeed, Services };

		FHansaCompiledPopulationTierDefinition Laborer;
		Laborer.StableId = TEXT("PopulationTier.Laborer");
		Laborer.Needs = { { TEXT("Need.BasicServices"), 0, 3000 }, { TEXT("Need.Bread"), 100, 7000 } };
		Laborer.WorkforcePerResidentBasisPoints = 6000;
		Laborer.GrowthSatisfactionBasisPoints = 8000;
		Laborer.DeclineSatisfactionBasisPoints = 3500;
		Laborer.EvaluationTicks = EvaluationTicks;
		Laborer.GrowthResidentsPerEvaluation = 2;
		Laborer.DeclineResidentsPerEvaluation = 3;

		FHansaCompiledPopulationTierDefinition Artisan = Laborer;
		Artisan.StableId = TEXT("PopulationTier.Artisan");
		Artisan.PreviousTierId = TEXT("PopulationTier.Laborer");
		Artisan.Needs[1].ConsumptionMilliUnitsPerResidentPerTick = 140;
		Artisan.WorkforcePerResidentBasisPoints = 7000;
		TArray<FHansaCompiledPopulationTierDefinition> Tiers { Laborer, Artisan };
		FHansaCompiledBuildingDefinition LaborerResidence;
		LaborerResidence.StableId = TEXT("Building.Residence.Laborer");
		LaborerResidence.ResidenceCapacity = 12;
		LaborerResidence.ResidentPopulationTierId = Laborer.StableId;
		LaborerResidence.UpgradeTargetBuildingId = TEXT("Building.Residence.Artisan");
		FHansaCompiledBuildingDefinition ArtisanResidence = LaborerResidence;
		ArtisanResidence.StableId = TEXT("Building.Residence.Artisan");
		ArtisanResidence.ResidenceCapacity = 8;
		ArtisanResidence.ResidentPopulationTierId = Artisan.StableId;
		ArtisanResidence.UpgradeTargetBuildingId.Reset();
		ArtisanResidence.ConstructionCostPfennig = UpgradeCurrencyCost;
		ArtisanResidence.ConstructionCosts = { { TEXT("Good.Bread"), UpgradeBreadCost } };
		FHansaCompiledRecipeDefinition WorkshopRecipe;
		WorkshopRecipe.StableId = TEXT("Recipe.CityWorkshop");
		WorkshopRecipe.Outputs = { { TEXT("Good.Bread"), 100 } };
		WorkshopRecipe.CycleTicks = 1;
		WorkshopRecipe.LaborerWorkforce = 7;
		WorkshopRecipe.bDeclaredSource = true;
		FHansaCompiledBuildingDefinition Workshop;
		Workshop.StableId = TEXT("Building.CityWorkshop");
		Workshop.RecipeIds.Add(WorkshopRecipe.StableId);
		Workshop.LaborerWorkforce = 7;
		FHansaCompiledBuildingDefinition MarketBuilding;
		MarketBuilding.StableId = TEXT("Building.Market");
		FHansaCompiledBuildingDefinition RoadBuilding;
		RoadBuilding.StableId = TEXT("Building.Road");

		return FHansaEconomicRegistry(MoveTemp(Goods), { WorkshopRecipe },
			{ LaborerResidence, ArtisanResidence, MarketBuilding, RoadBuilding, Workshop }, 0xA401000000000001ULL,
			MoveTemp(Needs), MoveTemp(Tiers));
	}

	FHansaSimulationDefinitionContext MakeDefinitions(const int32 EvaluationTicks = 2,
		const int64 UpgradeCurrencyCost = 1000, const int64 UpgradeBreadCost = 1000)
	{
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Require(FHansaScenarioId::TryParse(TEXT("Scenario.PopulationTest"))),
			0xA401000000000001ULL, MakeRegistry(EvaluationTicks, UpgradeCurrencyCost, UpgradeBreadCost)));
	}

	FHansaSimulationDefinitionContext MakeDefinitions(const FHansaPlacementTopology& PlacementTopology,
		const int32 EvaluationTicks = 2, const int64 UpgradeCurrencyCost = 1000,
		const int64 UpgradeBreadCost = 1000)
	{
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Require(FHansaScenarioId::TryParse(TEXT("Scenario.PopulationTest"))),
			0xA401000000000001ULL, MakeRegistry(EvaluationTicks, UpgradeCurrencyCost, UpgradeBreadCost),
			PlacementTopology));
	}

	FHansaSimulationState MakeState(const TCHAR* TierId, const int64 BreadStock,
		const int32 PurchasingPower = 10000, const int32 ServiceAccess = 10000,
		const int32 ServiceReliability = 10000, const int32 Residents = 10, const int32 Capacity = 12,
		const bool bHasMarket = true, const bool bConstructionComplete = true,
		const bool bCityWorkforceProduction = false, const bool bIncludeCohort = true,
		const bool bIncludeSecondResidence = false, const TCHAR* SecondTierId = nullptr)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Require(FHansaSimulationTick::TryCreate(0)), 10));
		Initialization.Houses.Add({ PopulationTestsEntity<FHansaHouseId>(1), FHansaMoney::FromRaw(100000) });
		const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
		Initialization.Cities.Add({ City, FHansaQuantity() });
		FHansaBuildingState Residence;
		Residence.Id = PopulationTestsEntity<FHansaBuildingId>(1);
		Residence.DefinitionId = BuildingType(FCString::Strcmp(TierId, TEXT("PopulationTier.Artisan")) == 0
			? TEXT("Building.Residence.Artisan") : TEXT("Building.Residence.Laborer"));
		Residence.OwnerId = PopulationTestsEntity<FHansaHouseId>(1);
		Residence.ConstructionProgress = FHansaRate::FromPartsPerMillion(
			bConstructionComplete ? FHansaRate::Scale : 0);
		Residence.ConstructionState = bConstructionComplete
			? EHansaConstructionState::Completed : EHansaConstructionState::UnderConstruction;
		Initialization.Buildings.Add(Residence);
		FHansaBuildingState PhysicalMarket;
		PhysicalMarket.Id = PopulationTestsEntity<FHansaBuildingId>(3);
		PhysicalMarket.DefinitionId = BuildingType(TEXT("Building.Market"));
		PhysicalMarket.OwnerId = Residence.OwnerId;
		PhysicalMarket.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
		PhysicalMarket.ConstructionState = EHansaConstructionState::Completed;
		Initialization.Buildings.Add(PhysicalMarket);
		FHansaBuildingState Road;
		Road.Id = PopulationTestsEntity<FHansaBuildingId>(4);
		Road.DefinitionId = BuildingType(TEXT("Building.Road"));
		Road.OwnerId = Residence.OwnerId;
		Road.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
		Road.ConstructionState = EHansaConstructionState::Completed;
		Initialization.Buildings.Add(Road);
		FHansaBuildingState SecondResidence = Residence;
		if (SecondTierId != nullptr)
			SecondResidence.DefinitionId = BuildingType(FCString::Strcmp(SecondTierId, TEXT("PopulationTier.Artisan")) == 0
				? TEXT("Building.Residence.Artisan") : TEXT("Building.Residence.Laborer"));
		SecondResidence.Id = PopulationTestsEntity<FHansaBuildingId>(5);
		FHansaBuildingState SecondRoad = Road;
		SecondRoad.Id = PopulationTestsEntity<FHansaBuildingId>(6);
		if (bIncludeSecondResidence)
		{
			Initialization.Buildings.Add(SecondResidence);
			Initialization.Buildings.Add(SecondRoad);
		}
		FHansaPlacementMapInitialization Map;
		Map.CityId = City;
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { 3, 3 };
		Map.RoadBuildingDefinitionId = Road.DefinitionId;
		for (int32 X = 0; X < 4; ++X)
		{
			for (int32 Y = 0; Y < 4; ++Y)
			{
				Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, PopulationTestsEntity<FHansaHouseId>(1), false });
			}
		}
		Initialization.Placement.Maps.Add(MoveTemp(Map));
		Initialization.Placement.Placements.Add({ Residence.Id, Residence.OwnerId,
			{ City, Residence.DefinitionId, { 0, 0 }, EHansaGridRotation::North },
			{ { 0, 0 } } });
		Initialization.Placement.Placements.Add({ Road.Id, Road.OwnerId,
			{ City, Road.DefinitionId, { 1, 0 }, EHansaGridRotation::North }, { { 1, 0 } } });
		Initialization.Placement.Placements.Add({ PhysicalMarket.Id, PhysicalMarket.OwnerId,
			{ City, PhysicalMarket.DefinitionId, { 2, 0 }, EHansaGridRotation::North }, { { 2, 0 } } });
		if (bIncludeSecondResidence)
		{
			Initialization.Placement.Placements.Add({ SecondResidence.Id, SecondResidence.OwnerId,
				{ City, SecondResidence.DefinitionId, { 0, 1 }, EHansaGridRotation::North }, { { 0, 1 } } });
			Initialization.Placement.Placements.Add({ SecondRoad.Id, SecondRoad.OwnerId,
				{ City, SecondRoad.DefinitionId, { 1, 1 }, EHansaGridRotation::North }, { { 1, 1 } } });
		}
		if (bCityWorkforceProduction)
		{
			FHansaBuildingState Workshop;
			Workshop.Id = PopulationTestsEntity<FHansaBuildingId>(2);
			Workshop.DefinitionId = BuildingType(TEXT("Building.CityWorkshop"));
			Workshop.OwnerId = PopulationTestsEntity<FHansaHouseId>(1);
			Workshop.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
			Workshop.ConstructionState = EHansaConstructionState::Completed;
			Initialization.Buildings.Add(Workshop);
		}

		FHansaInventoryInitialization Inventory;
		Inventory.Id = PopulationTestsEntity<FHansaInventoryId>(1);
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.BuildingId = PhysicalMarket.Id;
		Inventory.Capacity = FHansaQuantity::FromRaw(100000000);
		Inventory.AcceptedGoods.Add(Good(TEXT("Good.Bread")));
		Inventory.InitialStock.Add({ Good(TEXT("Good.Bread")), FHansaQuantity::FromRaw(BreadStock) });
		Initialization.Inventories.Add(MoveTemp(Inventory));

		FHansaPopulationCohortInitialization Cohort;
		Cohort.Id = PopulationTestsEntity<FHansaPopulationCohortId>(1);
		Cohort.ResidenceBuildingId = PopulationTestsEntity<FHansaBuildingId>(1);
		Cohort.CityId = City;
		Cohort.ConsumptionInventoryId = PopulationTestsEntity<FHansaInventoryId>(1);
		Cohort.TierId = Require(FHansaPopulationTierId::TryParse(TierId));
		Cohort.Residents = Residents;
		Cohort.ResidenceCapacity = Capacity;
		Cohort.PurchasingPowerBasisPoints = PurchasingPower;
		Cohort.ServiceAccessBasisPoints = ServiceAccess;
		Cohort.ServiceReliabilityBasisPoints = ServiceReliability;
		if (bIncludeCohort) Initialization.PopulationCohorts.Add(Cohort);
		if (bIncludeSecondResidence)
		{
			FHansaPopulationCohortInitialization SecondCohort = Cohort;
			SecondCohort.Id = PopulationTestsEntity<FHansaPopulationCohortId>(2);
			SecondCohort.ResidenceBuildingId = SecondResidence.Id;
			if (SecondTierId != nullptr)
				SecondCohort.TierId = Require(FHansaPopulationTierId::TryParse(SecondTierId));
			Initialization.PopulationCohorts.Add(SecondCohort);
		}
		if (bCityWorkforceProduction)
		{
			FHansaProductionInitialization Production;
			Production.Id = PopulationTestsEntity<FHansaProductionId>(1);
			Production.BuildingId = PopulationTestsEntity<FHansaBuildingId>(2);
			Production.RecipeId = Require(FHansaRecipeId::TryParse(TEXT("Recipe.CityWorkshop")));
			Production.InputInventoryId = PopulationTestsEntity<FHansaInventoryId>(1);
			Production.OutputInventoryId = PopulationTestsEntity<FHansaInventoryId>(1);
			Production.bUsesCityWorkforce = true;
			Initialization.Productions.Add(Production);
		}
		FHansaCityMarketInitialization Market;
		// Population fixtures need a current market projection after each step. Market cadence itself
		// is covered separately by Hansa.Simulation.Market.CadenceHistoryAndStaleness.
		Initialization.MarketSettings.UpdateCadenceTicks = 1;
		Market.CityId = City;
		Market.GoodId = Good(TEXT("Good.Bread"));
		Market.InventoryIds.Add(PopulationTestsEntity<FHansaInventoryId>(1));
		Market.DesiredReserve = FHansaQuantity::FromRaw(10000);
		Market.MinimumPriceMilliMarks = 1;
		Market.MaximumPriceMilliMarks = 10000;
		Market.InitialPriceMilliMarks = 1000;
		if (bHasMarket) Initialization.Markets.Add(Market);
		return Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	}

	bool Step(FHansaSimulationState& State, const FHansaSimulationDefinitionContext& Definitions,
		FHansaSimulationTransientCache& Cache)
	{
		return FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache).IsSuccess();
	}

	FHansaCommandHeader Header(const FHansaSimulationReadOnlyAccess& View, const uint64 Sequence)
	{
		FHansaCommandHeader Result;
		Result.CommandId = PopulationTestsEntity<FHansaCommandId>(Sequence);
		Result.Authority.IssuingHouseId = PopulationTestsEntity<FHansaHouseId>(1);
		Result.Authority.PrincipalId = 1;
		Result.RequestedExecutionTick = View.GetClock().GetTick();
		Result.GlobalSequence = Sequence;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationConsumptionProjectionTest,
	"Hansa.Simulation.Population.ConsumptionAndProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationConsumptionProjectionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeState(TEXT("PopulationTier.Laborer"), 10000, 5000, 8000, 9000);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Population tick succeeds"), Step(State, Definitions, Cache));
	const TOptional<FHansaPopulationCohortProjection> Cohort = State.CreateReadOnlyAccess(Definitions)
		.QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	if (!TestTrue(TEXT("Cohort has a typed projection"), Cohort.IsSet())) return false;
	TestEqual(TEXT("Laborer workforce is supplied from residents"), Cohort->WorkforceSupply, 6);
	TestEqual(TEXT("Affordability remains a separate causal factor"), Cohort->AffordabilityBasisPoints, 5000);
	TestEqual(TEXT("Weighted satisfaction is bounded and explainable"), Cohort->SatisfactionBasisPoints, 5000);
	const FHansaPopulationNeedState* Bread = Cohort->Needs.FindByPredicate(
		[](const FHansaPopulationNeedState& Need) { return Need.NeedId.ToString() == TEXT("Need.Bread"); });
	if (!TestNotNull(TEXT("Bread need is projected"), Bread)) return false;
	TestEqual(TEXT("Bread demand is deterministic"), Bread->RequiredLastTick.GetRawValue(), int64(1000));
	TestEqual(TEXT("Purchasing power bounds consumption"), Bread->ConsumedLastTick.GetRawValue(), int64(500));
    TestEqual(TEXT("Residence history records its own ten-minute step"), Cohort->Consumption.CoveredMinutes, int64(10));
    if (!TestEqual(TEXT("Only product history is retained"), Cohort->Consumption.Goods.Num(), 1)) return false;
    TestEqual(TEXT("Residence history required amount"), Cohort->Consumption.Goods[0].Required, int64(1000));
    TestEqual(TEXT("Residence history actual consumption"), Cohort->Consumption.Goods[0].Consumed, int64(500));
	TestEqual(TEXT("Access detects available accepted stock"), Bread->AccessBasisPoints, 10000);
	TestEqual(TEXT("Reliability reports fulfillment of affordable demand"), Bread->ReliabilityBasisPoints, 10000);
	TestEqual(TEXT("Reserve days are reported in milli-days without floating point"), Bread->ReserveMilliDays, int64(69));
	const auto Summary = State.CreateReadOnlyAccess(Definitions).BuildProjection();
	TestTrue(TEXT("Aggregate projection succeeds"), Summary.IsSuccess());
	if (Summary) {
		TestEqual(TEXT("Aggregate residents are projected"), Summary.Value.GetTotalResidents(), 10);
		TestEqual(TEXT("Aggregate workforce is projected"), Summary.Value.GetTotalWorkforceSupply(), 6);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationFairScarcityAllocationTest,
	"Hansa.Simulation.Population.FairScarcityAllocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationFairScarcityAllocationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeState(TEXT("PopulationTier.Laborer"), 1000,
		10000, 10000, 10000, 10, 12, true, true, false, true, true);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Two-residence scarcity tick succeeds"), Step(State, Definitions, Cache));
	const auto First = State.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(
		PopulationTestsEntity<FHansaPopulationCohortId>(1));
	const auto Second = State.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(
		PopulationTestsEntity<FHansaPopulationCohortId>(2));
	if (!TestTrue(TEXT("Both residence cohorts remain queryable"), First.IsSet() && Second.IsSet())) return false;
	const FHansaPopulationNeedState* FirstBread = First->Needs.FindByPredicate(
		[](const FHansaPopulationNeedState& Need) { return Need.GoodId.ToString() == TEXT("Good.Bread"); });
	const FHansaPopulationNeedState* SecondBread = Second->Needs.FindByPredicate(
		[](const FHansaPopulationNeedState& Need) { return Need.GoodId.ToString() == TEXT("Good.Bread"); });
	if (!TestNotNull(TEXT("First residence projects bread"), FirstBread) ||
		!TestNotNull(TEXT("Second residence projects bread"), SecondBread)) return false;
	TestEqual(TEXT("First residence receives its proportional share"),
		FirstBread->ConsumedLastTick.GetRawValue(), int64(500));
	TestEqual(TEXT("Second residence receives its proportional share"),
		SecondBread->ConsumedLastTick.GetRawValue(), int64(500));
	TestEqual(TEXT("Equal demand produces equal reliability"),
		FirstBread->ReliabilityBasisPoints, SecondBread->ReliabilityBasisPoints);
	const auto Remaining = State.CreateReadOnlyAccess(Definitions).GetInventories().QueryStock(
		PopulationTestsEntity<FHansaInventoryId>(1), Good(TEXT("Good.Bread")));
	TestTrue(TEXT("Shared stock remains queryable"), Remaining.IsSet());
	if (Remaining) TestEqual(TEXT("Allocation conserves the scarce stock"), Remaining->Available.GetRawValue(), int64(0));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationPooledWorkforceTest,
	"Hansa.Simulation.Population.CityTierPooledWorkforce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationPooledWorkforceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const auto Definitions = MakeDefinitions();
	for (const TCHAR* Tier : { TEXT("PopulationTier.Laborer"), TEXT("PopulationTier.Artisan") })
	{
		for (const int32 Residents : { 1, 3 })
		{
			auto Split = MakeState(Tier, 100000, 10000, 10000, 10000, Residents, 12,
				true, true, false, true, true);
			auto Together = MakeState(Tier, 100000, 10000, 10000, 10000, Residents * 2);
			FHansaSimulationTransientCache SplitCache, TogetherCache;
			TestTrue(TEXT("Split homes advance"), Step(Split, Definitions, SplitCache));
			TestTrue(TEXT("Combined home advances"), Step(Together, Definitions, TogetherCache));
			const auto SplitView = Split.CreateReadOnlyAccess(Definitions).BuildProjection();
			const auto TogetherView = Together.CreateReadOnlyAccess(Definitions).BuildProjection();
			if (!SplitView || !TogetherView) return false;
			TestEqual(TEXT("Housing distribution does not reduce workforce"),
				SplitView.Value.GetTotalWorkforceSupply(), TogetherView.Value.GetTotalWorkforceSupply());
		}
	}
	auto Working = MakeState(TEXT("PopulationTier.Laborer"), 10000000, 10000, 10000, 10000,
		1, 12, true, true, true, true, true);
	FHansaSimulationTransientCache WorkingCache;
	TestTrue(TEXT("Small households supply production"), Step(Working, Definitions, WorkingCache));
	auto Production = Working.CreateReadOnlyAccess(Definitions).QueryProduction(PopulationTestsEntity<FHansaProductionId>(1));
	if (!Production) return false;
	TestEqual(TEXT("Two one-person homes provide one allocated worker"), Production->AllocatedLaborerWorkforce, 1);
	TestTrue(TEXT("Production is no longer blocked by per-house rounding"), Production->Blocker == EHansaProductionBlocker::None);
	TestTrue(TEXT("Growth tick advances"), Step(Working, Definitions, WorkingCache));
	const auto Grown = Working.CreateReadOnlyAccess(Definitions).BuildProjection();
	if (!Grown) return false;
	TestEqual(TEXT("Post-migration supply is pooled immediately"), Grown.Value.GetTotalWorkforceSupply(), 3);

	auto Mixed = MakeState(TEXT("PopulationTier.Laborer"), 100000, 10000, 10000, 10000,
		1, 12, true, true, false, true, true, TEXT("PopulationTier.Artisan"));
	FHansaSimulationTransientCache MixedCache;
	TestTrue(TEXT("Mixed tiers advance"), Step(Mixed, Definitions, MixedCache));
	const auto MixedView = Mixed.CreateReadOnlyAccess(Definitions).BuildProjection();
	if (!MixedView) return false;
	TestEqual(TEXT("Fractions from different tiers are not combined"), MixedView.Value.GetTotalWorkforceSupply(), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationSuppliedLongRunTest,
	"Hansa.Simulation.Population.SuppliedHomesPastSeventyDays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationSuppliedLongRunTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const auto Definitions = MakeDefinitions();
	auto State = MakeState(TEXT("PopulationTier.Laborer"), 30000000,
		10000, 10000, 10000, 10, 12, true, true, false, true, true);
	FHansaSimulationTransientCache Cache;
	for (int32 Index = 0; Index < 11000; ++Index)
	{
		if (!Step(State, Definitions, Cache))
		{
			AddError(FString::Printf(TEXT("Population failed at tick %d"), Index));
			return false;
		}
	}
	const auto View = State.CreateReadOnlyAccess(Definitions);
	const auto Stock = View.GetInventories().QueryStock(
		PopulationTestsEntity<FHansaInventoryId>(1), Good(TEXT("Good.Bread")));
	TestTrue(TEXT("Bread remains available after 76 days"), Stock.IsSet() && Stock->Available.GetRawValue() > 0);
	for (uint64 Id : { uint64(1), uint64(2) })
	{
		const auto Home = View.QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(Id));
		if (!TestTrue(TEXT("Supplied house exists"), Home.IsSet())) return false;
		TestEqual(TEXT("Supplied residence remains full"), Home->Residents, 12);
		const auto* Bread = Home->Needs.FindByPredicate([](const auto& Need)
			{ return Need.GoodId.ToString() == TEXT("Good.Bread"); });
		if (!TestNotNull(TEXT("Bread need remains observable"), Bread)) return false;
		TestEqual(TEXT("Current bread consumption continues"), Bread->ConsumedLastTick.GetRawValue(), int64(1200));
		TestTrue(TEXT("Rolling history covers the full 30 days"), Home->Consumption.bFullWindow);
		for (const auto& Total : Home->Consumption.Goods)
			TestEqual(TEXT("Rolling consumed amount equals supplied demand"), Total.Consumed, Total.Required);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationGrowthDeclineTest,
	"Hansa.Simulation.Population.BoundedGrowthAndDecline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationGrowthDeclineTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(2);
	FHansaSimulationTransientCache GrowthCache;
	FHansaSimulationTransientCache LimitedCache;
	auto Limited = MakeState(TEXT("PopulationTier.Laborer"), 100000, 10000, 10000, 10000, 10, 12);
	for (int32 Tick=0; Tick<8; ++Tick)
		TestTrue(TEXT("Fed city with a small reserve advances"), Step(Limited, Definitions, LimitedCache));
	const auto LimitedHome = Limited.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Current fulfillment alone does not authorize growth"),
		LimitedHome.IsSet() && LimitedHome->Residents == 10 && LimitedHome->SatisfactionBasisPoints == 10000);
	// Enough stock for the 30-day post-growth reserve, not just this evaluation.
	FHansaSimulationState Growth = MakeState(TEXT("PopulationTier.Laborer"), 10000000, 10000, 10000, 10000, 10, 12);
	TestTrue(TEXT("First growth tick succeeds"), Step(Growth, Definitions, GrowthCache));
	TestTrue(TEXT("Second growth tick succeeds"), Step(Growth, Definitions, GrowthCache));
	const auto Grown = Growth.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Grown cohort remains queryable"), Grown.IsSet());
	if (Grown) {
		TestEqual(TEXT("Growth is bounded by residence capacity"), Grown->Residents, 12);
		TestEqual(TEXT("Growth cause is exposed"), Grown->ResidentChangeLastTick, 2);
	}
	const auto GrowingCity = Growth.CreateReadOnlyAccess(Definitions).QueryCityPopulation(
		Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))));
	TestTrue(TEXT("Growing city remains queryable"), GrowingCity.IsSet());
	if (GrowingCity)
	{
		TestTrue(TEXT("Positive migration exposes a typed growing trend"),
			GrowingCity->Trend == EHansaPopulationTrend::Growing);
	}

	FHansaSimulationTransientCache DeclineCache;
	FHansaSimulationState Decline = MakeState(TEXT("PopulationTier.Laborer"), 0, 10000, 0, 0, 10, 12);
	TestTrue(TEXT("First decline tick succeeds"), Step(Decline, Definitions, DeclineCache));
	TestTrue(TEXT("Second decline tick succeeds"), Step(Decline, Definitions, DeclineCache));
	const auto Declined = Decline.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Declined cohort remains queryable"), Declined.IsSet());
	if (Declined) {
		TestEqual(TEXT("Low satisfaction causes bounded decline"), Declined->Residents, 7);
		TestEqual(TEXT("Decline cause is exposed"), Declined->ResidentChangeLastTick, -3);
		TestEqual(TEXT("Empty stock and unavailable services report no access"), Declined->AccessBasisPoints, 0);
	}
	const auto DecliningCity = Decline.CreateReadOnlyAccess(Definitions).QueryCityPopulation(
		Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))));
	TestTrue(TEXT("Declining city remains queryable"), DecliningCity.IsSet());
	if (DecliningCity)
	{
		TestTrue(TEXT("Negative migration exposes a typed declining trend"),
			DecliningCity->Trend == EHansaPopulationTrend::Declining);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationStableLongRunTest,
	"Hansa.Simulation.Population.StableLongRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationStableLongRunTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(5);
	FHansaSimulationState First = MakeState(TEXT("PopulationTier.Artisan"), 100000000, 10000, 10000, 10000, 8, 8);
	FHansaSimulationState Second = MakeState(TEXT("PopulationTier.Artisan"), 100000000, 10000, 10000, 10000, 8, 8);
	FHansaSimulationTransientCache FirstCache;
	FHansaSimulationTransientCache SecondCache;
	for (int32 TickIndex = 0; TickIndex < 1000; ++TickIndex)
	{
		if (!Step(First, Definitions, FirstCache) || !Step(Second, Definitions, SecondCache))
		{
			AddError(TEXT("Long-run population step failed"));
			return false;
		}
	}
	const auto FirstProjection = First.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Long-run artisan cohort remains queryable"), FirstProjection.IsSet());
	if (FirstProjection) {
		TestEqual(TEXT("Population stays at capacity under sustained satisfaction"), FirstProjection->Residents, 8);
		TestEqual(TEXT("Artisan workforce share is deterministic"), FirstProjection->WorkforceSupply, 5);
		TestTrue(TEXT("All bounded factors remain valid"), FirstProjection->SatisfactionBasisPoints >= 0 && FirstProjection->SatisfactionBasisPoints <= 10000);
	}
	TestEqual(TEXT("Identical long-run executions keep the same fingerprint"),
		First.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,
		Second.CreateReadOnlyAccess(Definitions).GetFingerprint().Value);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationResidenceAndMarketAccessTest,
	"Hansa.Simulation.Population.ConstructedResidenceAndMarketAccess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationResidenceAndMarketAccessTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache NewResidenceCache;
	FHansaSimulationState NewResidence = MakeState(TEXT("PopulationTier.Laborer"), 100000,
		10000, 10000, 10000, 0, 12, true, true, false, false);
	TestTrue(TEXT("New constructed residence city advances"), Step(NewResidence, Definitions, NewResidenceCache));
	const auto CreatedCohort = NewResidence.CreateReadOnlyAccess(Definitions)
		.QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Completed placed residence creates its authoritative cohort"), CreatedCohort.IsSet());
	if (CreatedCohort)
	{
		TestEqual(TEXT("Created cohort uses the authored tier"), CreatedCohort->TierId.ToString(),
			FString(TEXT("PopulationTier.Laborer")));
		TestEqual(TEXT("Created cohort uses authored housing capacity"), CreatedCohort->ResidenceCapacity, 12);
		TestTrue(TEXT("Completed created cohort is operational"), CreatedCohort->bResidenceOperational);
	}
	FHansaSimulationTransientCache Cache;
	FHansaSimulationState NoMarket = MakeState(TEXT("PopulationTier.Laborer"), 100000,
		10000, 10000, 10000, 10, 12, false, true);
	TestTrue(TEXT("No-market city still advances"), Step(NoMarket, Definitions, Cache));
	const auto NoMarketCohort = NoMarket.CreateReadOnlyAccess(Definitions)
		.QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("No-market cohort remains queryable"), NoMarketCohort.IsSet());
	if (NoMarketCohort)
	{
		TestFalse(TEXT("Market access is an explicit causal flag"), NoMarketCohort->bHasMarketAccess);
		const FHansaPopulationNeedState* Bread = NoMarketCohort->Needs.FindByPredicate(
			[](const FHansaPopulationNeedState& Need) { return Need.GoodId.ToString() == TEXT("Good.Bread"); });
		TestTrue(TEXT("Unsatisfied bread need is retained"), Bread != nullptr &&
			Bread->ConsumedLastTick.GetRawValue() == 0 && Bread->AccessBasisPoints == 0);
	}

	FHansaSimulationTransientCache ConstructionCache;
	FHansaSimulationState Unfinished = MakeState(TEXT("PopulationTier.Laborer"), 100000,
		10000, 10000, 10000, 10, 12, true, false);
	TestTrue(TEXT("Unfinished residence city advances"), Step(Unfinished, Definitions, ConstructionCache));
	const auto UnfinishedCohort = Unfinished.CreateReadOnlyAccess(Definitions)
		.QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Unfinished cohort remains queryable"), UnfinishedCohort.IsSet());
	if (UnfinishedCohort)
	{
		TestFalse(TEXT("Unfinished residence is not operational"), UnfinishedCohort->bResidenceOperational);
		TestEqual(TEXT("Unfinished residence provides no workforce"), UnfinishedCohort->WorkforceSupply, 0);
		TestEqual(TEXT("Unfinished residence consumes no needs"), UnfinishedCohort->Needs.Num(), 0);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationManualResidenceProgressionTest,
	"Hansa.Simulation.Population.ManualResidenceProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationManualResidenceProgressionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	FHansaSimulationState State = MakeState(TEXT("PopulationTier.Laborer"), 100000, 10000, 10000, 10000, 8, 12);
	const FHansaSimulationDefinitionContext BootstrapDefinitions = MakeDefinitions();
	const FHansaPlacementTopology* PlacementTopology =
		State.CreateReadOnlyAccess(BootstrapDefinitions).GetPlacement().GetTopology();
	if (!TestNotNull(TEXT("Population fixture has compiled placement topology"), PlacementTopology)) return false;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(*PlacementTopology);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Residence establishes qualifying satisfaction"), Step(State, Definitions, Cache));
	const FHansaSimulationReadOnlyAccess Before = State.CreateReadOnlyAccess(Definitions);
	const int64 MoneyBefore = Before.GetHouses()[0].Money.GetRawValue();
	const TArray<FHansaGameplayCommand> Commands {
		FHansaGameplayCommand::Create(Header(Before, 1), FHansaUpgradeResidenceCommand { PopulationTestsEntity<FHansaBuildingId>(1) })
	};
	const FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions,
		Commands, Cache);
	TestTrue(TEXT("Satisfied residence accepts explicit upgrade"), Result.IsSuccess());
	TestTrue(TEXT("Upgrade publishes a typed event"), Result.GetEvents().Num() > 0 &&
		Result.GetEvents()[0].GetType() == EHansaDomainEventType::ResidenceUpgraded);
	const auto Cohort = State.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Upgraded cohort remains queryable"), Cohort.IsSet());
	if (Cohort)
	{
		TestEqual(TEXT("Upgrade advances the authored population tier"), Cohort->TierId.ToString(), FString(TEXT("PopulationTier.Artisan")));
		TestEqual(TEXT("Target residence capacity becomes authoritative"), Cohort->ResidenceCapacity, 8);
        TestEqual(TEXT("Upgrade retains both consumption steps"), Cohort->Consumption.CoveredMinutes, int64(20));
        if (!TestEqual(TEXT("Bread history remains"), Cohort->Consumption.Goods.Num(), 1)) return false;
        TestEqual(TEXT("Laborer and artisan demand both count; upgrade costs do not"), Cohort->Consumption.Goods[0].Required, int64(800+1120));
	}
	TestEqual(TEXT("Building identity advances with the cohort"),
		State.CreateReadOnlyAccess(Definitions).GetBuildings()[0].DefinitionId.ToString(),
		FString(TEXT("Building.Residence.Artisan")));
	const FHansaSimulationReadOnlyAccess After = State.CreateReadOnlyAccess(Definitions);
	TestEqual(TEXT("Upgrade charges the authored currency cost exactly once"),
		After.GetHouses()[0].Money.GetRawValue(), MoneyBefore - 1000);
	const TArray<FHansaInventoryMovement> UpgradeMovements = After.GetInventories().QueryRecentMovements(
		PopulationTestsEntity<FHansaInventoryId>(1), 64);
	TestEqual(TEXT("Upgrade records exactly one authored construction-cost resource movement"),
		UpgradeMovements.FilterByPredicate([](const FHansaInventoryMovement& Movement)
		{
			return Movement.ExternalEndpointId == TEXT("Construction.Cost") &&
				Movement.GoodId == Good(TEXT("Good.Bread")) && Movement.Quantity.GetRawValue() == 1000;
		}).Num(), 1);

	FHansaSaveSnapshot Saved;
	Saved.State = State; Saved.BuildVersion = TEXT("EMVP-P06-test"); Saved.SavedUtc = TEXT("2026-09-07T00:00:00Z");
	Saved.DisplayName = TEXT("Upgraded residence round trip"); Saved.Players.Add({ 1, PopulationTestsEntity<FHansaHouseId>(1) });
	TArray<uint8> Bytes;
	const FHansaSaveResult Encoded = FHansaSaveEnvelope::Encode(Saved, Definitions, Bytes);
	if (!TestTrue(*Encoded.Message, Encoded.IsSuccess())) return false;
	FHansaSaveSnapshot Loaded;
	const FHansaSaveResult Decoded = FHansaSaveEnvelope::Decode(Bytes, Definitions, Loaded);
	if (!TestTrue(*Decoded.Message, Decoded.IsSuccess())) return false;
	const auto LoadedCohort = Loaded.State.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Save/load preserves artisan population movement"), LoadedCohort.IsSet() &&
		LoadedCohort->TierId.ToString() == TEXT("PopulationTier.Artisan") && LoadedCohort->Residents == 8);
	TestEqual(TEXT("Save/load preserves the replacement building definition"),
		Loaded.State.CreateReadOnlyAccess(Definitions).GetBuildings()[0].DefinitionId.ToString(),
		FString(TEXT("Building.Residence.Artisan")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationResidenceUpgradeCostFailureTest,
	"Hansa.Simulation.Population.ResidenceUpgradeCostFailureIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationResidenceUpgradeCostFailureTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	(void)Parameters;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(2, 200000, 1000);
	FHansaSimulationState State = MakeState(TEXT("PopulationTier.Laborer"), 100000, 10000, 10000, 10000, 8, 12);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Residence establishes qualifying satisfaction before the cost check"), Step(State, Definitions, Cache));
	const FHansaSimulationReadOnlyAccess Before = State.CreateReadOnlyAccess(Definitions);
	const FHansaDeterminismFingerprint BeforeFingerprint = Before.GetFingerprint();
	const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(
		Header(Before, 1), FHansaUpgradeResidenceCommand { PopulationTestsEntity<FHansaBuildingId>(1) });
	const FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(
		State, Definitions, MakeArrayView(&Command, 1), Cache);
	TestEqual(TEXT("Unaffordable upgrade has a stable gateway error"), Result.GetError(),
		EHansaCommandGatewayError::ConstructionCostUnavailable);
	TestTrue(TEXT("Failure exposes the exact missing upgrade cost"), Result.GetConstructionCost().IsSet() &&
		Result.GetConstructionCost()->MissingCurrency.GetRawValue() == 100000);
	TestTrue(TEXT("Rejected upgrade leaves all simulation state unchanged"),
		State.CreateReadOnlyAccess(Definitions).GetFingerprint() == BeforeFingerprint);
	TestEqual(TEXT("Rejected upgrade keeps the laborer visual identity"),
		State.CreateReadOnlyAccess(Definitions).GetBuildings()[0].DefinitionId.ToString(),
		FString(TEXT("Building.Residence.Laborer")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationWorkforceRecoveryTest,
	"Hansa.Simulation.Population.WorkforceShortageAndRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationWorkforceRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(2);
	FHansaSimulationState State = MakeState(TEXT("PopulationTier.Laborer"), 10000000,
		10000, 10000, 10000, 10, 12, true, true, true);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Initial shortage tick succeeds"), Step(State, Definitions, Cache));
	const auto Shortage = State.CreateReadOnlyAccess(Definitions).QueryProduction(PopulationTestsEntity<FHansaProductionId>(1));
	TestTrue(TEXT("City production remains queryable during shortage"), Shortage.IsSet());
	if (Shortage)
	{
		TestEqual(TEXT("Available workers are assigned deterministically"), Shortage->AllocatedLaborerWorkforce, 6);
		TestTrue(TEXT("Partial staffing keeps production active"),
			Shortage->Blocker == EHansaProductionBlocker::None);
		TestEqual(TEXT("Six of seven workers lengthen a one-tick batch to two ticks"), Shortage->CycleTicks, 2);
	}
	const auto ShortageCity = State.CreateReadOnlyAccess(Definitions).QueryCityPopulation(
		Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))));
	TestTrue(TEXT("City loop projection exists"), ShortageCity.IsSet());
	if (ShortageCity)
	{
		TestEqual(TEXT("City projection reports population"), ShortageCity->TotalResidents, 10);
		TestEqual(TEXT("City projection reports assigned workforce"), ShortageCity->LaborerWorkforceAssigned, 6);
		TestEqual(TEXT("All current labor is committed"), ShortageCity->LaborerWorkforceAvailable, 0);
		TestTrue(TEXT("Staple reserve is projected in deterministic milli-days"),
			ShortageCity->StapleReserveMilliDays > 0);
	}
	TestTrue(TEXT("Growth threshold tick succeeds"), Step(State, Definitions, Cache));
	TestTrue(TEXT("Recovered allocation tick succeeds"), Step(State, Definitions, Cache));
	const auto Recovered = State.CreateReadOnlyAccess(Definitions).QueryProduction(PopulationTestsEntity<FHansaProductionId>(1));
	TestTrue(TEXT("Production remains queryable after recovery"), Recovered.IsSet());
	if (Recovered)
	{
		TestEqual(TEXT("Population growth supplies the required workforce"), Recovered->AllocatedLaborerWorkforce, 7);
		TestTrue(TEXT("Full staffing retains an unblocked production state"), Recovered->Blocker == EHansaProductionBlocker::None);
		TestTrue(TEXT("Recovered production completes work"), Recovered->CompletedCycles > 0);
	}
	const auto RecoveredCity = State.CreateReadOnlyAccess(Definitions).QueryCityPopulation(
		Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))));
	TestTrue(TEXT("Recovered city projection exists"), RecoveredCity.IsSet());
	if (RecoveredCity)
	{
		TestEqual(TEXT("Population trend explains the migration step"), RecoveredCity->TotalResidents, 12);
		TestEqual(TEXT("Recovered city supplies seven workers"), RecoveredCity->LaborerWorkforceSupply, 7);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationEmptyHomeRecoveryTest,
	"Hansa.Simulation.Population.EmptyHomeRecovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPopulationEmptyHomeRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const auto Definitions = MakeDefinitions();
	for (const int64 Stock : { int64(0), int64(1), int64(100000) })
	{
		auto State = MakeState(TEXT("PopulationTier.Laborer"), Stock, 10000, 10000, 10000, 0, 12);
		FHansaSimulationTransientCache Cache;
		for (int32 Tick = 0; Tick < 2; ++Tick) TestTrue(TEXT("Recovery tick"), Step(State, Definitions, Cache));
		const auto Cohort = State.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(PopulationTestsEntity<FHansaPopulationCohortId>(1));
		if (!TestTrue(TEXT("Home observable"), Cohort.IsSet())) return false;
		TestEqual(TEXT("Fulfilled basic services always seed two residents"), Cohort->Residents, 2);
		TestEqual(TEXT("Residents begin goods consumption on the tick after moving in"),
			Cohort->Needs[1].ConsumedLastTick.GetRawValue(), FMath::Min<int64>(Stock, 200));
        TestEqual(TEXT("Empty house history measures elapsed time"), Cohort->Consumption.CoveredMinutes, int64(20));
        for (const auto& Total : Cohort->Consumption.Goods)
        {
			TestEqual(TEXT("Only real post-move-in consumption is recorded"), Total.Consumed, FMath::Min<int64>(Stock, 200));
			TestEqual(TEXT("Only real post-move-in demand is recorded"), Total.Required, int64(200));
        }
	}
	auto NoServices = MakeState(TEXT("PopulationTier.Laborer"), 100000, 10000, 0, 10000, 0, 12);
	FHansaSimulationTransientCache NoServicesCache;
	TestTrue(TEXT("No-services tick"), Step(NoServices, Definitions, NoServicesCache));
	const auto Unseeded = NoServices.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(
		PopulationTestsEntity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Home without basic services remains observable"), Unseeded.IsSet());
	if (Unseeded) TestEqual(TEXT("Basic services are required for the initial household"), Unseeded->Residents, 0);
	return true;
}

#endif
