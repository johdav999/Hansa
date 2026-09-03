#include "Algo/Reverse.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Market
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
	FHansaSimulationTick Tick(const int64 Value) { return Require(FHansaSimulationTick::TryCreate(Value)); }

	FHansaEconomicRegistry MakeRegistry()
	{
		FHansaCompiledGoodDefinition Bread;
		Bread.StableId = TEXT("Good.Bread");
		Bread.BaseValueMilliMarks = 1000;
		FHansaCompiledGoodDefinition Grain = Bread;
		Grain.StableId = TEXT("Good.Grain");

		FHansaCompiledRecipeDefinition Recipe;
		Recipe.StableId = TEXT("Recipe.MarketTest");
		Recipe.Inputs = { { TEXT("Good.Grain"), 1000 } };
		Recipe.Outputs = { { TEXT("Good.Bread"), 1000 } };
		Recipe.CycleTicks = 10;

		FHansaCompiledBuildingDefinition Building;
		Building.StableId = TEXT("Building.Residence.Laborer");
		Building.RecipeIds.Add(Recipe.StableId);
		Building.ResidenceCapacity = 20;
		Building.ResidentPopulationTierId = TEXT("PopulationTier.Laborer");

		FHansaCompiledNeedDefinition Need;
		Need.StableId = TEXT("Need.Bread");
		Need.Kind = EHansaCompiledNeedKind::Good;
		Need.GoodId = TEXT("Good.Bread");
		FHansaCompiledPopulationTierDefinition Tier;
		Tier.StableId = TEXT("PopulationTier.Laborer");
		Tier.Needs = { { TEXT("Need.Bread"), 100, 10000 } };
		Tier.WorkforcePerResidentBasisPoints = 6000;
		Tier.GrowthSatisfactionBasisPoints = 10000;
		Tier.DeclineSatisfactionBasisPoints = 1;
		Tier.EvaluationTicks = 1000;
		Tier.GrowthResidentsPerEvaluation = 1;
		Tier.DeclineResidentsPerEvaluation = 1;

		return FHansaEconomicRegistry({ Bread, Grain }, { Recipe }, { Building }, 0xA402000000000001ULL,
			{ Need }, { Tier });
	}

	FHansaSimulationDefinitionContext MakeDefinitions()
	{
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Require(FHansaScenarioId::TryParse(TEXT("Scenario.MarketTest"))),
			0xA402000000000001ULL, MakeRegistry()));
	}

	FHansaSimulationState MakeState(const TCHAR* MarketGood, const int64 Stock, const int64 Reserve,
		const int64 Incoming, const int64 InitialPrice, const int32 Cadence = 1,
		const int32 HistoryCapacity = 8, const bool bPopulation = false, const bool bProduction = false,
		const int64 MaximumPrice = 1000000, const int32 PopulationPurchasingPowerBasisPoints = 10000)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Tick(0)));
		Initialization.CampaignSeed = 0xA402;
		const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
		Initialization.Cities.Add({ City, FHansaQuantity() });

		if (bPopulation || bProduction)
		{
			Initialization.Houses.Add({ Entity<FHansaHouseId>(1), FHansaMoney::FromRaw(100000) });
			FHansaBuildingState BuildingState;
			BuildingState.Id = Entity<FHansaBuildingId>(1);
			BuildingState.DefinitionId = Require(FHansaBuildingTypeId::TryParse(TEXT("Building.Residence.Laborer")));
			BuildingState.OwnerId = Entity<FHansaHouseId>(1);
			BuildingState.ConstructionProgress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
			BuildingState.ConstructionState = EHansaConstructionState::Completed;
			Initialization.Buildings.Add(BuildingState);
		}

		FHansaInventoryInitialization Inventory;
		Inventory.Id = Entity<FHansaInventoryId>(1);
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.Capacity = FHansaQuantity::FromRaw(100000000);
		Inventory.AcceptedGoods = { Good(TEXT("Good.Bread")), Good(TEXT("Good.Grain")) };
		Inventory.InitialStock.Add({ Good(MarketGood), FHansaQuantity::FromRaw(Stock) });
		Initialization.Inventories.Add(MoveTemp(Inventory));

		if (bPopulation)
		{
			FHansaPopulationCohortInitialization Cohort;
			Cohort.Id = Entity<FHansaPopulationCohortId>(1);
			Cohort.ResidenceBuildingId = Entity<FHansaBuildingId>(1);
			Cohort.CityId = City;
			Cohort.ConsumptionInventoryId = Entity<FHansaInventoryId>(1);
			Cohort.TierId = Require(FHansaPopulationTierId::TryParse(TEXT("PopulationTier.Laborer")));
			Cohort.Residents = 10;
			Cohort.ResidenceCapacity = 10;
			Cohort.PurchasingPowerBasisPoints = PopulationPurchasingPowerBasisPoints;
			Initialization.PopulationCohorts.Add(Cohort);
		}
		if (bProduction)
		{
			FHansaProductionInitialization Production;
			Production.Id = Entity<FHansaProductionId>(1);
			Production.BuildingId = Entity<FHansaBuildingId>(1);
			Production.RecipeId = Require(FHansaRecipeId::TryParse(TEXT("Recipe.MarketTest")));
			Production.InputInventoryId = Entity<FHansaInventoryId>(1);
			Production.OutputInventoryId = Entity<FHansaInventoryId>(1);
			Initialization.Productions.Add(Production);
		}

		Initialization.MarketSettings.UpdateCadenceTicks = Cadence;
		Initialization.MarketSettings.PriceHistoryCapacity = HistoryCapacity;
		Initialization.MarketSettings.TargetSmoothingBasisPoints = 2500;
		Initialization.MarketSettings.MaximumMovementBasisPointsPerUpdate = 1000;
		Initialization.MarketSettings.StaleAfterTicks = Cadence * 2;
		FHansaCityMarketInitialization Market;
		Market.CityId = City;
		Market.GoodId = Good(MarketGood);
		Market.InventoryIds.Add(Entity<FHansaInventoryId>(1));
		Market.DesiredReserve = FHansaQuantity::FromRaw(Reserve);
		Market.ConfirmedIncomingSupplyPerUpdate = FHansaQuantity::FromRaw(Incoming);
		Market.MinimumPriceMilliMarks = 100;
		Market.MaximumPriceMilliMarks = MaximumPrice;
		Market.InitialPriceMilliMarks = InitialPrice;
		Initialization.Markets.Add(Market);
		return Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	}

	FHansaSimulationState MakeTwoMarketState(const bool bReverseDiscovery)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Tick(0)));
		const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
		Initialization.Cities.Add({ City, FHansaQuantity() });
		FHansaInventoryInitialization Inventory;
		Inventory.Id = Entity<FHansaInventoryId>(1);
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = City;
		Inventory.Capacity = FHansaQuantity::FromRaw(1000000);
		Inventory.AcceptedGoods = { Good(TEXT("Good.Bread")), Good(TEXT("Good.Grain")) };
		Inventory.InitialStock = {
			{ Good(TEXT("Good.Bread")), FHansaQuantity::FromRaw(2000) },
			{ Good(TEXT("Good.Grain")), FHansaQuantity::FromRaw(8000) }
		};
		Initialization.Inventories.Add(MoveTemp(Inventory));
		Initialization.MarketSettings.UpdateCadenceTicks = 1;
		Initialization.MarketSettings.PriceHistoryCapacity = 4;
		Initialization.MarketSettings.StaleAfterTicks = 2;
		for (const TCHAR* GoodId : { TEXT("Good.Bread"), TEXT("Good.Grain") })
		{
			FHansaCityMarketInitialization Market;
			Market.CityId = City;
			Market.GoodId = Good(GoodId);
			Market.InventoryIds.Add(Entity<FHansaInventoryId>(1));
			Market.DesiredReserve = FHansaQuantity::FromRaw(5000);
			Market.MinimumPriceMilliMarks = 100;
			Market.MaximumPriceMilliMarks = 5000;
			Market.InitialPriceMilliMarks = 1000;
			Initialization.Markets.Add(Market);
		}
		if (bReverseDiscovery)
		{
			Algo::Reverse(Initialization.Markets);
			Algo::Reverse(Initialization.Inventories[0].AcceptedGoods);
			Algo::Reverse(Initialization.Inventories[0].InitialStock);
		}
		return Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	}

	bool Step(FHansaSimulationState& State, const FHansaSimulationDefinitionContext& Definitions,
		FHansaSimulationTransientCache& Cache)
	{
		return FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache).IsSuccess();
	}

	FHansaCityMarketProjection Market(const FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions, const TCHAR* GoodId)
	{
		const auto Result = State.CreateReadOnlyAccess(Definitions).QueryMarket(
			Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))), Good(GoodId));
		check(Result.IsSet());
		return Result.GetValue();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketScarcitySurplusTest,
	"Hansa.Simulation.Market.ScarcitySurplusAndZeroStock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketScarcitySurplusTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Market;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;

	FHansaSimulationState Scarce = MakeState(TEXT("Good.Bread"), 0, 10000, 0, 1000);
	TestTrue(TEXT("Zero-stock market step succeeds"), Step(Scarce, Definitions, Cache));
	const FHansaCityMarketProjection ScarceMarket = Market(Scarce, Definitions, TEXT("Good.Bread"));
	TestEqual(TEXT("Zero stock creates the maximum weighted scarcity factor"), ScarceMarket.Factors.ScarcityBasisPoints, 6000);
	TestEqual(TEXT("Movement is capped at ten percent per update"), ScarceMarket.CurrentPriceMilliMarks, int64(1100));
	TestEqual(TEXT("No-demand zero-stock case does not invent unmet demand"), ScarceMarket.UnmetDemand.GetRawValue(), int64(0));

	FHansaSimulationState Surplus = MakeState(TEXT("Good.Bread"), 20000, 10000, 0, 1000);
	FHansaSimulationTransientCache SurplusCache;
	TestTrue(TEXT("Surplus market step succeeds"), Step(Surplus, Definitions, SurplusCache));
	const FHansaCityMarketProjection SurplusMarket = Market(Surplus, Definitions, TEXT("Good.Bread"));
	TestEqual(TEXT("Surplus scarcity relief is bounded"), SurplusMarket.Factors.ScarcityBasisPoints, -3000);
	TestEqual(TEXT("Surplus lowers price through smoothing"), SurplusMarket.CurrentPriceMilliMarks, int64(925));

	FHansaSimulationState Balanced = MakeState(TEXT("Good.Bread"), 10000, 10000, 0, 1000);
	FHansaSimulationTransientCache BalancedCache;
	TestTrue(TEXT("Balanced no-demand market step succeeds"), Step(Balanced, Definitions, BalancedCache));
	TestEqual(TEXT("No demand at reserve leaves the base price stable"),
		Market(Balanced, Definitions, TEXT("Good.Bread")).CurrentPriceMilliMarks, int64(1000));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketDemandRecoveryTest,
	"Hansa.Simulation.Market.DemandAndRecoveringSupply",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketDemandRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Market;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();

	FHansaSimulationState CitizenState = MakeState(TEXT("Good.Bread"), 0, 10000, 0, 1000, 1, 8, true);
	FHansaSimulationTransientCache CitizenCache;
	TestTrue(TEXT("Citizen-demand market step succeeds"), Step(CitizenState, Definitions, CitizenCache));
	const FHansaCityMarketProjection CitizenMarket = Market(CitizenState, Definitions, TEXT("Good.Bread"));
	TestEqual(TEXT("Population requirements become citizen demand"), CitizenMarket.CitizenDemand.GetRawValue(), int64(1000));
	TestEqual(TEXT("Unsupplied citizen demand remains explicit"), CitizenMarket.UnmetDemand.GetRawValue(), int64(1000));
	TestTrue(TEXT("Citizen demand contributes positive pressure"), CitizenMarket.Factors.CitizenDemandBasisPoints > 0);
	FHansaSimulationState SuppliedCitizenState = MakeState(TEXT("Good.Bread"), 10000, 10000, 0, 1000, 1, 8, true);
	FHansaSimulationTransientCache SuppliedCitizenCache;
	TestTrue(TEXT("Supplied citizen-demand market step succeeds"), Step(SuppliedCitizenState, Definitions, SuppliedCitizenCache));
	TestEqual(TEXT("Consumed citizen demand is not mislabeled as unmet"),
		Market(SuppliedCitizenState, Definitions, TEXT("Good.Bread")).UnmetDemand.GetRawValue(), int64(0));

	FHansaSimulationState IndustryState = MakeState(TEXT("Good.Grain"), 10000, 10000, 0, 1000, 1, 8, false, true);
	FHansaSimulationTransientCache IndustryCache;
	TestTrue(TEXT("Industrial-demand market step succeeds"), Step(IndustryState, Definitions, IndustryCache));
	const FHansaCityMarketProjection IndustryMarket = Market(IndustryState, Definitions, TEXT("Good.Grain"));
	TestEqual(TEXT("Recipe input divided by cycle creates industrial demand"), IndustryMarket.IndustrialDemand.GetRawValue(), int64(100));
	TestTrue(TEXT("Industrial demand has its own positive factor"), IndustryMarket.Factors.IndustrialDemandBasisPoints > 0);

	FHansaSimulationState Recovery = MakeState(TEXT("Good.Bread"), 10000, 10000, 10000, 2000);
	FHansaSimulationTransientCache RecoveryCache;
	TestTrue(TEXT("Recovering-supply market step succeeds"), Step(Recovery, Definitions, RecoveryCache));
	const FHansaCityMarketProjection RecoveryMarket = Market(Recovery, Definitions, TEXT("Good.Bread"));
	TestEqual(TEXT("Confirmed incoming supply is projected"), RecoveryMarket.ExpectedIncomingSupply.GetRawValue(), int64(10000));
	TestEqual(TEXT("Incoming supply contributes bounded relief"), RecoveryMarket.Factors.IncomingSupplyBasisPoints, -2500);
	TestEqual(TEXT("Recovering supply lowers an elevated price within its cap"), RecoveryMarket.CurrentPriceMilliMarks, int64(1800));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketCadenceHistoryTest,
	"Hansa.Simulation.Market.CadenceHistoryAndStaleness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketCadenceHistoryTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Market;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeState(TEXT("Good.Bread"), 0, 10000, 0, 1000, 2, 3);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Unreported market starts stale"), Market(State, Definitions, TEXT("Good.Bread")).bIsStale);
	TestTrue(TEXT("First non-cadence tick succeeds"), Step(State, Definitions, Cache));
	TestEqual(TEXT("Non-cadence tick does not update price"), Market(State, Definitions, TEXT("Good.Bread")).CurrentPriceMilliMarks, int64(1000));
	TestTrue(TEXT("Second cadence tick succeeds"), Step(State, Definitions, Cache));
	const FHansaCityMarketProjection FirstReport = Market(State, Definitions, TEXT("Good.Bread"));
	TestEqual(TEXT("Cadence update records its exact tick"), FirstReport.LastUpdateTick, int64(2));
	TestEqual(TEXT("Fresh report age is zero"), FirstReport.ReportAgeTicks, int64(0));
	TestFalse(TEXT("Fresh report is not stale"), FirstReport.bIsStale);
	TestEqual(TEXT("Next update is projected exactly"), FirstReport.NextUpdateTick, int64(4));
	for (int32 Index = 0; Index < 6; ++Index) TestTrue(TEXT("History progression tick succeeds"), Step(State, Definitions, Cache));
	const FHansaCityMarketProjection Final = Market(State, Definitions, TEXT("Good.Bread"));
	TestEqual(TEXT("Bounded history retains only configured entries"), Final.PriceHistory.Num(), 3);
	TestEqual(TEXT("Recent average is computed from bounded authoritative history"),
		Final.RecentAveragePriceMilliMarks,
		(Final.PriceHistory[0].PriceMilliMarks + Final.PriceHistory[1].PriceMilliMarks + Final.PriceHistory[2].PriceMilliMarks) / 3);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketBoundsOrderTest,
	"Hansa.Simulation.Market.BoundsAndOrderIndependence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketBoundsOrderTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Market;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState First = MakeState(TEXT("Good.Bread"), 0, 10000, 0, 1000, 1, 8, false, false, 1500);
	FHansaSimulationState Second = MakeState(TEXT("Good.Bread"), 0, 10000, 0, 1000, 1, 8, false, false, 1500);
	FHansaSimulationTransientCache FirstCache;
	FHansaSimulationTransientCache SecondCache;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		TestTrue(TEXT("First bounded-price tick succeeds"), Step(First, Definitions, FirstCache));
		TestTrue(TEXT("Second bounded-price tick succeeds"), Step(Second, Definitions, SecondCache));
	}
	const FHansaCityMarketProjection Projection = Market(First, Definitions, TEXT("Good.Bread"));
	TestEqual(TEXT("Persistent scarcity cannot exceed authored maximum price"), Projection.CurrentPriceMilliMarks, int64(1500));
	TestTrue(TEXT("All history prices remain within authored bounds"),
		!Projection.PriceHistory.ContainsByPredicate([](const FHansaMarketPriceHistoryEntry& Entry)
		{
			return Entry.PriceMilliMarks < 100 || Entry.PriceMilliMarks > 1500;
		}));
	TestEqual(TEXT("Repeated executions remain bit-identical"),
		First.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,
		Second.CreateReadOnlyAccess(Definitions).GetFingerprint().Value);

	FHansaSimulationState Forward = MakeTwoMarketState(false);
	FHansaSimulationState Reversed = MakeTwoMarketState(true);
	FHansaSimulationTransientCache ForwardCache;
	FHansaSimulationTransientCache ReversedCache;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		TestTrue(TEXT("Forward discovery-order tick succeeds"), Step(Forward, Definitions, ForwardCache));
		TestTrue(TEXT("Reversed discovery-order tick succeeds"), Step(Reversed, Definitions, ReversedCache));
	}
	TestEqual(TEXT("Market and inventory discovery order cannot change prices or oscillation"),
		Forward.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,
		Reversed.CreateReadOnlyAccess(Definitions).GetFingerprint().Value);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketCausalQueriesTest,
	"Hansa.Simulation.Market.CausalQueriesAndExplanationTotals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketCausalQueriesTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Market;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeState(TEXT("Good.Bread"), 10000, 10000, 0, 1000,
		1, 8, true, true);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Causal query fixture advances"), Step(State, Definitions, Cache));
	const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
	const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
	const FHansaGoodId Bread = Good(TEXT("Good.Bread"));

	const TOptional<FHansaMarketExplanationProjection> Explanation = View.QueryMarketExplanation(City, Bread);
	TestTrue(TEXT("Market explanation is available"), Explanation.IsSet());
	if (!Explanation.IsSet()) return false;
	int32 ReconstructedMultiplier = Explanation->BaseMultiplierBasisPoints;
	for (int32 Index = 0; Index < Explanation->Factors.Num(); ++Index)
	{
		const FHansaMarketExplanationEntry& Factor = Explanation->Factors[Index];
		ReconstructedMultiplier += Factor.ContributionBasisPoints;
		TestTrue(TEXT("Every causal factor has a stable localization key"), !Factor.MessageKey.IsNone());
		TestFalse(TEXT("Every causal factor has human-readable localized source text"), Factor.Message.IsEmpty());
		if (Index > 0)
		{
			TestTrue(TEXT("Causal factors retain their reviewed stable order"),
				Explanation->Factors[Index - 1].Factor < Factor.Factor);
		}
	}
	TestEqual(TEXT("Explanation contributions exactly reconstruct the authoritative target multiplier"),
		ReconstructedMultiplier, Explanation->TargetMultiplierBasisPoints);
	TestEqual(TEXT("Explanation target is the market's authoritative calculation"),
		Explanation->TargetMultiplierBasisPoints, Market(State, Definitions, TEXT("Good.Bread")).Factors.TargetMultiplierBasisPoints);

	const TOptional<FHansaMarketPriceProjection> Price = View.QueryMarketPrice(City, Bread);
	const TOptional<FHansaMarketSupplyDemandProjection> Components = View.QueryMarketSupplyDemand(City, Bread);
	const TOptional<FHansaMarketReserveProjection> Reserve = View.QueryMarketReserveDays(City, Bread);
	TestTrue(TEXT("Typed price query is available"), Price.IsSet());
	TestTrue(TEXT("Typed supply-demand query is available"), Components.IsSet());
	TestTrue(TEXT("Typed reserve query is available"), Reserve.IsSet());
	if (!Price.IsSet() || !Components.IsSet() || !Reserve.IsSet()) return false;
	TestEqual(TEXT("Typed history query returns authoritative history"),
		View.QueryMarketPriceHistory(City, Bread).Num(), Price->History.Num());
	TestEqual(TEXT("Supply-demand total is the authoritative citizen plus industrial components"),
		Components->TotalDemand.GetRawValue(),
		Components->CitizenDemand.GetRawValue() + Components->IndustrialDemand.GetRawValue());
	TestTrue(TEXT("Reserve days explicitly report demand availability"), Reserve->bHasDemand);

	const TArray<FHansaMarketConsumerProjection> Consumers = View.QueryMarketConsumers(City, Bread);
	const TArray<FHansaMarketProducerProjection> Producers = View.QueryMarketProducers(City, Bread);
	TestEqual(TEXT("Bread consumer list identifies the residence cohort"), Consumers.Num(), 1);
	if (!Consumers.IsEmpty())
	{
		TestTrue(TEXT("Citizen consumer identity is typed"), Consumers[0].PopulationCohortId.IsValid());
		TestEqual(TEXT("Citizen list demand reconciles with market demand"),
			Consumers[0].DemandPerTick.GetRawValue(), Components->CitizenDemand.GetRawValue());
	}
	TestEqual(TEXT("Bread producer list identifies the recipe producer"), Producers.Num(), 1);
	if (!Producers.IsEmpty())
	{
		TestTrue(TEXT("Producer identity is typed"), Producers[0].ProductionId.IsValid());
		TestEqual(TEXT("Producer exposes the recipe output quantity"),
			Producers[0].NominalQuantityPerCycle.GetRawValue(), int64(1000));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketAlertsTest,
	"Hansa.Simulation.Market.ShortageReserveAndAffordabilityAlerts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketAlertsTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Market;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeState(TEXT("Good.Bread"), 0, 10000, 0, 1000,
		1, 8, true, false, 1000000, 4000);
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Alert fixture advances"), Step(State, Definitions, Cache));
	const FHansaCityDefinitionId City = Require(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
	const FHansaGoodId Bread = Good(TEXT("Good.Bread"));
	TArray<FHansaMarketAlertProjection> Alerts = State.CreateReadOnlyAccess(Definitions).QueryMarketAlerts(City, Bread);
	TestEqual(TEXT("Shortage, low-reserve and affordability alerts are active"), Alerts.Num(), 3);
	if (Alerts.Num() != 3) return false;
	TestTrue(TEXT("Critical alerts use stable type ordering"),
		Alerts[0].Type == EHansaMarketAlertType::Shortage &&
		Alerts[1].Type == EHansaMarketAlertType::LowReserve &&
		Alerts[2].Type == EHansaMarketAlertType::Affordability);
	for (const FHansaMarketAlertProjection& Alert : Alerts)
	{
		TestTrue(TEXT("Severe alert is typed"), Alert.Severity == EHansaMarketAlertSeverity::Critical);
		TestTrue(TEXT("Alert carries affected city and good IDs"), Alert.CityId == City && Alert.GoodId == Bread);
		TestTrue(TEXT("Alert cause is localization-ready"), !Alert.CauseMessageKey.IsNone() && !Alert.Cause.IsEmpty());
		TestEqual(TEXT("New alert starts at age zero"), Alert.AgeTicks, int64(0));
		TestTrue(TEXT("Alert includes at least one suggested player action"), !Alert.SuggestedActions.IsEmpty());
		for (const FHansaMarketSuggestedAction& Action : Alert.SuggestedActions)
		{
			TestTrue(TEXT("Suggested action has a stable localization key"), !Action.MessageKey.IsNone());
			TestFalse(TEXT("Suggested action has localized source text"), Action.Message.IsEmpty());
		}
	}
	TestEqual(TEXT("Shortage identifies its affected population consumer"), Alerts[0].PopulationCohortIds.Num(), 1);
	TestEqual(TEXT("Affordability identifies its affected population consumer"), Alerts[2].PopulationCohortIds.Num(), 1);

	TestTrue(TEXT("Persistent alert fixture advances again"), Step(State, Definitions, Cache));
	Alerts = State.CreateReadOnlyAccess(Definitions).BuildActiveMarketAlerts();
	TestEqual(TEXT("Aggregate active-alert query retains all alerts"), Alerts.Num(), 3);
	for (const FHansaMarketAlertProjection& Alert : Alerts)
	{
		TestEqual(TEXT("Persistent alert age advances deterministically"), Alert.AgeTicks, int64(1));
	}
	FHansaSimulationState Healthy = MakeState(TEXT("Good.Bread"), 11000, 10000, 0, 1000,
		1, 8, true, false);
	FHansaSimulationTransientCache HealthyCache;
	TestTrue(TEXT("Healthy alert fixture advances"), Step(Healthy, Definitions, HealthyCache));
	TestTrue(TEXT("Healthy supplied and affordable market has no active alerts"),
		Healthy.CreateReadOnlyAccess(Definitions).BuildActiveMarketAlerts().IsEmpty());
	return !HasAnyErrors();
}

#endif
