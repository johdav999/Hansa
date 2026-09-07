#if WITH_DEV_AUTOMATION_TESTS

#include "AI/HansaMerchantAI.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"

using namespace Hansa::Simulation;

namespace
{
	template <typename T>
	T Required(const THansaValueResult<T>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename T>
	T Entity(const uint64 Value) { return Required(T::TryCreate(Value)); }

	FHansaEconomicRegistry UnknownReportRegistry()
	{
		FHansaCompiledGoodDefinition Grain;
		Grain.StableId = TEXT("Good.Grain");
		Grain.BaseValueMilliMarks = 1000;
		FHansaCompiledVehicleDefinition Cog;
		Cog.StableId = TEXT("Vehicle.Cog");
		Cog.Mode = EHansaRouteMode::Sea;
		Cog.CargoCapacityMilliUnits = 60'000;
		FHansaCompiledRouteDefinition Baltic;
		Baltic.StableId = TEXT("Route.BalticSea");
		Baltic.Mode = EHansaRouteMode::Sea;
		Baltic.Connections.Add({TEXT("City.Lubeck"), TEXT("City.Rostock"), 10});
		FHansaCompiledMerchantAITuning Tuning;
		Tuning.StableId = TEXT("AITuning.MerchantRival");
		Tuning.DecisionCadenceTicks = 5;
		Tuning.DecisionHistoryCapacity = 8;
		Tuning.MinimumDestinationDemandGapMilliUnits = 1;
		Tuning.MinimumGrossMarginMilliMarks = 1;
		Tuning.TradePlans.Add({TEXT("MerchantPlan.UnknownReports"), TEXT("Route.BalticSea"), TEXT("Vehicle.Cog"),
			TEXT("City.Rostock"), TEXT("City.Lubeck"), TEXT("Good.Grain"), 20'000, 30'000, 100});
		return FHansaEconomicRegistry({MoveTemp(Grain)}, {}, {}, 0xA110000000000001ULL,
			{}, {}, {}, {MoveTemp(Cog)}, {MoveTemp(Baltic)}, {}, {MoveTemp(Tuning)});
	}

	FHansaSimulationState UnknownReportState()
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Required(FHansaSimulationClock::TryCreate(
			Required(FHansaSimulationVersion::TryCreate(1)), Required(FHansaSimulationTick::TryCreate(0))));
		Initialization.CampaignSeed = 0xA110;
		const FHansaHouseId House = Entity<FHansaHouseId>(2);
		const FHansaCityDefinitionId Lubeck = Required(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")));
		const FHansaCityDefinitionId Rostock = Required(FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")));
		const FHansaGoodId Grain = Required(FHansaGoodId::TryParse(TEXT("Good.Grain")));
		Initialization.Houses.Add({House, FHansaMoney::FromRaw(50'000)});
		Initialization.Cities = {{Lubeck, FHansaQuantity()}, {Rostock, FHansaQuantity()}};
		for (uint64 Value = 1; Value <= 2; ++Value)
		{
			FHansaInventoryInitialization Inventory;
			Inventory.Id = Entity<FHansaInventoryId>(Value);
			Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
			Inventory.CityId = Value == 1 ? Lubeck : Rostock;
			Inventory.Capacity = FHansaQuantity::FromRaw(1'000'000);
			Inventory.AcceptedGoods.Add(Grain);
			Inventory.InitialStock.Add({Grain, FHansaQuantity::FromRaw(Value == 1 ? 1'000 : 100'000)});
			Initialization.Inventories.Add(MoveTemp(Inventory));
		}
		FHansaInventoryInitialization Cargo;
		Cargo.Id = Entity<FHansaInventoryId>(3);
		Cargo.OwnerKind = EHansaInventoryOwnerKind::Vehicle;
		Cargo.VehicleId = Entity<FHansaVehicleId>(1);
		Cargo.Capacity = FHansaQuantity::FromRaw(60'000);
		Cargo.AcceptedGoods.Add(Grain);
		Initialization.Inventories.Add(MoveTemp(Cargo));
		FHansaVehicleState Vehicle;
		Vehicle.Id = Entity<FHansaVehicleId>(1);
		Vehicle.DefinitionId = Required(FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")));
		Vehicle.OwnerId = House;
		Vehicle.CargoInventoryId = Entity<FHansaInventoryId>(3);
		Vehicle.Mode = EHansaRouteMode::Sea;
		Vehicle.Capacity = FHansaQuantity::FromRaw(60'000);
		Vehicle.CurrentCityId = Lubeck;
		Initialization.Vehicles.Add(Vehicle);
		FHansaRouteState Route;
		Route.Id = Entity<FHansaRouteId>(1);
		Route.OwnerId = House;
		Route.VehicleId = Vehicle.Id;
		Route.RouteDefinitionId = Required(FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")));
		Route.Mode = EHansaRouteMode::Sea;
		Route.Stops = {{Lubeck, {}}, {Rostock, {}}};
		Initialization.Routes.Add(Route);
		Initialization.MarketSettings.UpdateCadenceTicks = 100;
		Initialization.MarketSettings.PriceHistoryCapacity = 4;
		Initialization.MarketSettings.TargetSmoothingBasisPoints = 2500;
		Initialization.MarketSettings.MaximumMovementBasisPointsPerUpdate = 1000;
		Initialization.MarketSettings.StaleAfterTicks = 200;
		for (uint64 Value = 1; Value <= 2; ++Value)
		{
			FHansaCityMarketInitialization Market;
			Market.CityId = Value == 1 ? Lubeck : Rostock;
			Market.GoodId = Grain;
			Market.InventoryIds.Add(Entity<FHansaInventoryId>(Value));
			Market.DesiredReserve = FHansaQuantity::FromRaw(30'000);
			Market.ReportPolicy.ReportCadenceTicks = 100;
			Market.ReportPolicy.CurrentMaxAgeTicks = 0;
			Market.ReportPolicy.RecentMaxAgeTicks = 10;
			Market.ReportPolicy.StaleMaxAgeTicks = 100;
			Market.ReportPolicy.EstimatedMaxAgeTicks = 200;
			Market.MinimumPriceMilliMarks = 100;
			Market.MaximumPriceMilliMarks = 10'000;
			Market.InitialPriceMilliMarks = Value == 1 ? 3'000 : 500;
			Market.InitialReportTick = -1;
			Initialization.Markets.Add(Market);
		}
		return Required(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantAIInformationBoundaryTest,
	"Hansa.Simulation.AI.UnknownReportsBlockPrivilegedOpportunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMerchantAIInformationBoundaryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FHansaEconomicRegistry Registry = UnknownReportRegistry();
	const uint64 RegistryHash = Registry.GetRegistryHash();
	const auto Definitions = FHansaSimulationDefinitionContext::TryCreate(
		Required(FHansaScenarioId::TryParse(TEXT("Scenario.AIUnknownReports"))), RegistryHash, Registry);
	if (!TestTrue(TEXT("Unknown-report fixture definitions are valid"), Definitions.IsSuccess())) return false;
	FHansaSimulationState State = UnknownReportState();
	const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions.Value);
	const FHansaCompiledMerchantAITuning& Tuning = Registry.GetMerchantAITunings()[0];
	FHansaMerchantAIController Controller;
	const FHansaMerchantAIDecision Decision = Controller.Evaluate(View, Registry, Tuning,
		Entity<FHansaHouseId>(2), 2, Entity<FHansaCommandId>(1));
	TestFalse(TEXT("The rival emits no command when market reports are unknown"), Decision.Command.IsSet());
	TestEqual(TEXT("Unknown reports leave the rival without an eligible goal"), Decision.Trace.SelectedGoal, EHansaMerchantAIGoal::None);
	TestTrue(TEXT("The rejected opportunity explains the information boundary"),
		!Decision.Trace.ConsideredOptions.IsEmpty() && Decision.Trace.ConsideredOptions[0].Reason.Contains(TEXT("unknown")));
	return !HasAnyErrors();
}

#endif
