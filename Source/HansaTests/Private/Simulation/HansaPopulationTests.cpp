#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
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
	TId Entity(const uint64 Value) { return Require(TId::TryCreate(Value)); }

	FHansaGoodId Good(const TCHAR* Value) { return Require(FHansaGoodId::TryParse(Value)); }
	FHansaBuildingTypeId BuildingType(const TCHAR* Value) { return Require(FHansaBuildingTypeId::TryParse(Value)); }

	FHansaEconomicRegistry MakeRegistry(const int32 EvaluationTicks = 2)
	{
		FHansaCompiledGoodDefinition Bread;
		Bread.StableId = TEXT("Good.Bread");
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

		return FHansaEconomicRegistry(MoveTemp(Goods), { WorkshopRecipe },
			{ LaborerResidence, ArtisanResidence, Workshop }, 0xA401000000000001ULL,
			MoveTemp(Needs), MoveTemp(Tiers));
	}

	FHansaSimulationDefinitionContext MakeDefinitions(const int32 EvaluationTicks = 2)
	{
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Require(FHansaScenarioId::TryParse(TEXT("Scenario.PopulationTest"))),
			0xA401000000000001ULL, MakeRegistry(EvaluationTicks)));
	}

	FHansaSimulationState MakeState(const TCHAR* TierId, const int64 BreadStock,
		const int32 PurchasingPower = 10000, const int32 ServiceAccess = 10000,
		const int32 ServiceReliability = 10000, const int32 Residents = 10, const int32 Capacity = 12,
		const bool bHasMarket = true, const bool bConstructionComplete = true,
		const bool bCityWorkforceProduction = false, const bool bIncludeCohort = true)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Require(FHansaSimulationTick::TryCreate(0)), 10));
		Initialization.Houses.Add({ Entity<FHansaHouseId>(1), FHansaMoney::FromRaw(100000) });
		const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
		Initialization.Cities.Add({ City, FHansaQuantity() });
		FHansaBuildingState Residence;
		Residence.Id = Entity<FHansaBuildingId>(1);
		Residence.DefinitionId = BuildingType(FCString::Strcmp(TierId, TEXT("PopulationTier.Artisan")) == 0
			? TEXT("Building.Residence.Artisan") : TEXT("Building.Residence.Laborer"));
		Residence.OwnerId = Entity<FHansaHouseId>(1);
		Residence.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
		Residence.ConstructionState = bConstructionComplete
			? EHansaConstructionState::Completed : EHansaConstructionState::UnderConstruction;
		Initialization.Buildings.Add(Residence);
		FHansaPlacementMapInitialization Map;
		Map.CityId = City;
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { 3, 3 };
		Map.RoadBuildingDefinitionId = BuildingType(TEXT("Building.CityWorkshop"));
		for (int32 X = 0; X < 4; ++X)
		{
			for (int32 Y = 0; Y < 4; ++Y)
			{
				Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, Entity<FHansaHouseId>(1), false });
			}
		}
		Initialization.Placement.Maps.Add(MoveTemp(Map));
		Initialization.Placement.Placements.Add({ Residence.Id, Residence.OwnerId,
			{ City, Residence.DefinitionId, { 0, 0 }, EHansaGridRotation::North },
			{ { 0, 0 } } });
		if (bCityWorkforceProduction)
		{
			FHansaBuildingState Workshop;
			Workshop.Id = Entity<FHansaBuildingId>(2);
			Workshop.DefinitionId = BuildingType(TEXT("Building.CityWorkshop"));
			Workshop.OwnerId = Entity<FHansaHouseId>(1);
			Workshop.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
			Workshop.ConstructionState = EHansaConstructionState::Completed;
			Initialization.Buildings.Add(Workshop);
		}

		FHansaInventoryInitialization Inventory;
		Inventory.Id = Entity<FHansaInventoryId>(1);
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.Capacity = FHansaQuantity::FromRaw(100000000);
		Inventory.AcceptedGoods.Add(Good(TEXT("Good.Bread")));
		Inventory.InitialStock.Add({ Good(TEXT("Good.Bread")), FHansaQuantity::FromRaw(BreadStock) });
		Initialization.Inventories.Add(MoveTemp(Inventory));

		FHansaPopulationCohortInitialization Cohort;
		Cohort.Id = Entity<FHansaPopulationCohortId>(1);
		Cohort.ResidenceBuildingId = Entity<FHansaBuildingId>(1);
		Cohort.CityId = City;
		Cohort.ConsumptionInventoryId = Entity<FHansaInventoryId>(1);
		Cohort.TierId = Require(FHansaPopulationTierId::TryParse(TierId));
		Cohort.Residents = Residents;
		Cohort.ResidenceCapacity = Capacity;
		Cohort.PurchasingPowerBasisPoints = PurchasingPower;
		Cohort.ServiceAccessBasisPoints = ServiceAccess;
		Cohort.ServiceReliabilityBasisPoints = ServiceReliability;
		if (bIncludeCohort) Initialization.PopulationCohorts.Add(Cohort);
		if (bCityWorkforceProduction)
		{
			FHansaProductionInitialization Production;
			Production.Id = Entity<FHansaProductionId>(1);
			Production.BuildingId = Entity<FHansaBuildingId>(2);
			Production.RecipeId = Require(FHansaRecipeId::TryParse(TEXT("Recipe.CityWorkshop")));
			Production.InputInventoryId = Entity<FHansaInventoryId>(1);
			Production.OutputInventoryId = Entity<FHansaInventoryId>(1);
			Production.bUsesCityWorkforce = true;
			Initialization.Productions.Add(Production);
		}
		FHansaCityMarketInitialization Market;
		Market.CityId = City;
		Market.GoodId = Good(TEXT("Good.Bread"));
		Market.InventoryIds.Add(Entity<FHansaInventoryId>(1));
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
		Result.CommandId = Entity<FHansaCommandId>(Sequence);
		Result.Authority.IssuingHouseId = Entity<FHansaHouseId>(1);
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
		.QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
	if (!TestTrue(TEXT("Cohort has a typed projection"), Cohort.IsSet())) return false;
	TestEqual(TEXT("Laborer workforce is supplied from residents"), Cohort->WorkforceSupply, 6);
	TestEqual(TEXT("Affordability remains a separate causal factor"), Cohort->AffordabilityBasisPoints, 5000);
	TestEqual(TEXT("Weighted satisfaction is bounded and explainable"), Cohort->SatisfactionBasisPoints, 5000);
	const FHansaPopulationNeedState* Bread = Cohort->Needs.FindByPredicate(
		[](const FHansaPopulationNeedState& Need) { return Need.NeedId.ToString() == TEXT("Need.Bread"); });
	if (!TestNotNull(TEXT("Bread need is projected"), Bread)) return false;
	TestEqual(TEXT("Bread demand is deterministic"), Bread->RequiredLastTick.GetRawValue(), int64(1000));
	TestEqual(TEXT("Purchasing power bounds consumption"), Bread->ConsumedLastTick.GetRawValue(), int64(500));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPopulationGrowthDeclineTest,
	"Hansa.Simulation.Population.BoundedGrowthAndDecline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPopulationGrowthDeclineTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Population;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(2);
	FHansaSimulationTransientCache GrowthCache;
	FHansaSimulationState Growth = MakeState(TEXT("PopulationTier.Laborer"), 100000, 10000, 10000, 10000, 10, 12);
	TestTrue(TEXT("First growth tick succeeds"), Step(Growth, Definitions, GrowthCache));
	TestTrue(TEXT("Second growth tick succeeds"), Step(Growth, Definitions, GrowthCache));
	const auto Grown = Growth.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
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
	const auto Declined = Decline.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
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
	const auto FirstProjection = First.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
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
		.QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
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
		.QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
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
		.QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
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
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeState(TEXT("PopulationTier.Laborer"), 100000, 10000, 10000, 10000, 8, 12);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Residence establishes qualifying satisfaction"), Step(State, Definitions, Cache));
	const FHansaSimulationReadOnlyAccess Before = State.CreateReadOnlyAccess(Definitions);
	const TArray<FHansaGameplayCommand> Commands {
		FHansaGameplayCommand::Create(Header(Before, 1), FHansaUpgradeResidenceCommand { Entity<FHansaBuildingId>(1) })
	};
	const FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions,
		Commands, Cache);
	TestTrue(TEXT("Satisfied residence accepts explicit upgrade"), Result.IsSuccess());
	TestTrue(TEXT("Upgrade publishes a typed event"), Result.GetEvents().Num() > 0 &&
		Result.GetEvents()[0].GetType() == EHansaDomainEventType::ResidenceUpgraded);
	const auto Cohort = State.CreateReadOnlyAccess(Definitions).QueryPopulationCohort(Entity<FHansaPopulationCohortId>(1));
	TestTrue(TEXT("Upgraded cohort remains queryable"), Cohort.IsSet());
	if (Cohort)
	{
		TestEqual(TEXT("Upgrade advances the authored population tier"), Cohort->TierId.ToString(), FString(TEXT("PopulationTier.Artisan")));
		TestEqual(TEXT("Target residence capacity becomes authoritative"), Cohort->ResidenceCapacity, 8);
	}
	TestEqual(TEXT("Building identity advances with the cohort"),
		State.CreateReadOnlyAccess(Definitions).GetBuildings()[0].DefinitionId.ToString(),
		FString(TEXT("Building.Residence.Artisan")));
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
	FHansaSimulationState State = MakeState(TEXT("PopulationTier.Laborer"), 100000,
		10000, 10000, 10000, 10, 12, true, true, true);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Initial shortage tick succeeds"), Step(State, Definitions, Cache));
	const auto Shortage = State.CreateReadOnlyAccess(Definitions).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("City production remains queryable during shortage"), Shortage.IsSet());
	if (Shortage)
	{
		TestEqual(TEXT("Available workers are assigned deterministically"), Shortage->AllocatedLaborerWorkforce, 6);
		TestTrue(TEXT("Shortage reports its causal production blocker"),
			Shortage->Blocker == EHansaProductionBlocker::InsufficientLaborerWorkforce);
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
	const auto Recovered = State.CreateReadOnlyAccess(Definitions).QueryProduction(Entity<FHansaProductionId>(1));
	TestTrue(TEXT("Production remains queryable after recovery"), Recovered.IsSet());
	if (Recovered)
	{
		TestEqual(TEXT("Population growth supplies the required workforce"), Recovered->AllocatedLaborerWorkforce, 7);
		TestTrue(TEXT("Workforce recovery clears the shortage blocker"), Recovered->Blocker == EHansaProductionBlocker::None);
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

#endif
