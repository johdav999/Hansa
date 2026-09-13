#include "Algo/Reverse.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Diagnostics/HansaStateHash.h"
#include "Logistics/HansaLocalLogistics.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::LocalLogistics
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue Require(const THansaValueResult<TValue>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename TId>
	TId LocalLogisticsTestsEntity(const uint64 Value)
	{
		return Require(TId::TryCreate(Value));
	}

	template <typename TId>
	TId Definition(const TCHAR* Value)
	{
		return Require(TId::TryParse(Value));
	}

	FHansaBuildingState CompletedBuilding(const uint64 Id, const TCHAR* DefinitionId)
	{
		FHansaBuildingState Result;
		Result.Id = LocalLogisticsTestsEntity<FHansaBuildingId>(Id);
		Result.DefinitionId = Definition<FHansaBuildingTypeId>(DefinitionId);
		Result.OwnerId = LocalLogisticsTestsEntity<FHansaHouseId>(1);
		Result.ConstructionProgress = Require(FHansaRate::TryMakeNormalized(FHansaRate::Scale));
		Result.ConstructionState = EHansaConstructionState::Completed;
		return Result;
	}

	FHansaPlacedBuildingRecord Placement(
		const uint64 Id,
		const TCHAR* DefinitionId,
		const int32 X,
		const int32 Y)
	{
		FHansaPlacedBuildingRecord Result;
		Result.BuildingId = LocalLogisticsTestsEntity<FHansaBuildingId>(Id);
		Result.OwnerId = LocalLogisticsTestsEntity<FHansaHouseId>(1);
		Result.Spec.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Result.Spec.BuildingDefinitionId = Definition<FHansaBuildingTypeId>(DefinitionId);
		Result.Spec.Anchor = { X, Y };
		Result.OccupiedCells = { { X, Y } };
		return Result;
	}

	FHansaInventoryInitialization BuildingInventory(
		const uint64 InventoryId,
		const uint64 BuildingId,
		const EHansaInventoryOwnerKind Kind,
		const int64 Capacity,
		const int64 Stock)
	{
		const FHansaGoodId Grain = Definition<FHansaGoodId>(TEXT("Good.Grain"));
		const FHansaGoodId Flour = Definition<FHansaGoodId>(TEXT("Good.Flour"));
		FHansaInventoryInitialization Result;
		Result.Id = LocalLogisticsTestsEntity<FHansaInventoryId>(InventoryId);
		Result.OwnerKind = Kind;
		Result.BuildingId = LocalLogisticsTestsEntity<FHansaBuildingId>(BuildingId);
		Result.Capacity = FHansaQuantity::FromRaw(Capacity);
		Result.AcceptedGoods = { Grain, Flour };
		if (Stock > 0)
		{
			Result.InitialStock = { { Grain, FHansaQuantity::FromRaw(Stock) } };
		}
		return Result;
	}

	FHansaInventoryInitialization CityMarketInventory(const int64 Stock,
		const uint64 InventoryId = 3, const uint64 MarketBuildingId = 0)
	{
		const FHansaGoodId Grain = Definition<FHansaGoodId>(TEXT("Good.Grain"));
		const FHansaGoodId Flour = Definition<FHansaGoodId>(TEXT("Good.Flour"));
		FHansaInventoryInitialization Result;
		Result.Id = LocalLogisticsTestsEntity<FHansaInventoryId>(InventoryId);
		Result.OwnerKind = EHansaInventoryOwnerKind::City;
		Result.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		if (MarketBuildingId > 0) Result.BuildingId = LocalLogisticsTestsEntity<FHansaBuildingId>(MarketBuildingId);
		Result.Capacity = FHansaQuantity::FromRaw(1'000);
		Result.AcceptedGoods = { Grain, Flour };
		if (Stock > 0)
		{
			Result.InitialStock = { { Grain, FHansaQuantity::FromRaw(Stock) } };
		}
		return Result;
	}

	FHansaLogisticsRequestInitialization Request(
		const uint64 RequestId,
		const uint64 SourceInventory,
		const uint64 DestinationInventory,
		const int64 Quantity,
		const EHansaLogisticsPriority Priority = EHansaLogisticsPriority::Normal)
	{
		FHansaLogisticsRequestInitialization Result;
		Result.Id = LocalLogisticsTestsEntity<FHansaLogisticsRequestId>(RequestId);
		Result.SourceInventoryId = LocalLogisticsTestsEntity<FHansaInventoryId>(SourceInventory);
		Result.DestinationInventoryId = LocalLogisticsTestsEntity<FHansaInventoryId>(DestinationInventory);
		Result.GoodId = Definition<FHansaGoodId>(TEXT("Good.Grain"));
		Result.Quantity = FHansaQuantity::FromRaw(Quantity);
		Result.Priority = Priority;
		return Result;
	}

	FHansaSimulationInitialization MakeInitialization(
		const bool bDisconnected = false,
		const bool bDestinationFull = false,
		const bool bReverseDiscovery = false,
		TArray<FHansaLogisticsRequestInitialization> Requests = {})
	{
		FHansaSimulationInitialization Result;
		Result.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Require(FHansaSimulationTick::TryCreate(0))));
		Result.CampaignSeed = 0x4C4F474953544943ULL;
		Result.Houses = { { LocalLogisticsTestsEntity<FHansaHouseId>(1), FHansaMoney::FromRaw(100'000) } };
		Result.Cities = { {
			Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck")), FHansaQuantity() } };

		Result.Buildings = {
			CompletedBuilding(1, TEXT("Building.Warehouse")),
			CompletedBuilding(2, TEXT("Building.Factory")),
			CompletedBuilding(4, TEXT("Building.Dock")),
			CompletedBuilding(5, TEXT("Building.Market"))
		};
		for (int32 X = 0; X <= 7; ++X)
		{
			if (bDisconnected && X == 3)
			{
				continue;
			}
			Result.Buildings.Add(CompletedBuilding(10 + X, TEXT("Building.Road")));
		}

		FHansaPlacementMapInitialization Map;
		Map.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { 7, 1 };
		Map.RoadBuildingDefinitionId = Definition<FHansaBuildingTypeId>(TEXT("Building.Road"));
		for (int32 X = 0; X <= 7; ++X)
		{
			for (int32 Y = 0; Y <= 1; ++Y)
			{
				Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, LocalLogisticsTestsEntity<FHansaHouseId>(1), false });
			}
		}
		Result.Placement.Maps = { MoveTemp(Map) };
		Result.Placement.Entitlements = { {
			LocalLogisticsTestsEntity<FHansaHouseId>(1),
			Definition<FHansaBuildingTypeId>(TEXT("Building.Road")) } };
		Result.Placement.Placements = {
			Placement(1, TEXT("Building.Warehouse"), 0, 1),
			Placement(2, TEXT("Building.Factory"), 3, 1),
			Placement(4, TEXT("Building.Dock"), 6, 1),
			Placement(5, TEXT("Building.Market"), 7, 1)
		};
		for (int32 X = 0; X <= 7; ++X)
		{
			if (!bDisconnected || X != 3)
			{
				Result.Placement.Placements.Add(Placement(10 + X, TEXT("Building.Road"), X, 0));
			}
		}

		Result.Inventories = {
			BuildingInventory(1, 1, EHansaInventoryOwnerKind::Warehouse, 1'000, 300),
			BuildingInventory(2, 2, EHansaInventoryOwnerKind::Building, 1'000, bDestinationFull ? 1'000 : 0),
			CityMarketInventory(0),
			BuildingInventory(4, 4, EHansaInventoryOwnerKind::Warehouse, 1'000, 0)
		};
		Result.LocalLogisticsSettings.JobCapacity = FHansaQuantity::FromRaw(100);
		Result.LocalLogisticsSettings.PickupDelayTicks = 1;
		Result.LocalLogisticsSettings.TicksPerRoadCell = 1;
		Result.LocalLogisticsSettings.MaximumConcurrentJobs = 4;
		Result.LocalLogisticsRequests = Requests.IsEmpty()
			? TArray<FHansaLogisticsRequestInitialization> { Request(1, 1, 2, 250) }
			: MoveTemp(Requests);

		if (bReverseDiscovery)
		{
			Algo::Reverse(Result.Buildings);
			Algo::Reverse(Result.Placement.Placements);
			Algo::Reverse(Result.Placement.Maps[0].Cells);
			Algo::Reverse(Result.Inventories);
			Algo::Reverse(Result.LocalLogisticsRequests);
		}
		return Result;
	}

	FHansaEconomicRegistry MakeEconomicRegistry()
	{
		FHansaCompiledGoodDefinition Grain;
		Grain.StableId = TEXT("Good.Grain");
		FHansaCompiledGoodDefinition Flour;
		Flour.StableId = TEXT("Good.Flour");
		FHansaCompiledRecipeDefinition Recipe;
		Recipe.StableId = TEXT("Recipe.Factory");
		Recipe.Inputs = { { TEXT("Good.Grain"), 50 } };
		Recipe.Outputs = { { TEXT("Good.Flour"), 20 } };
		Recipe.CycleTicks = 1;
		FHansaCompiledBuildingDefinition Factory;
		Factory.StableId = TEXT("Building.Factory");
		Factory.RecipeIds = { TEXT("Recipe.Factory") };
		Factory.FootprintWidthCells = 1;
		Factory.FootprintHeightCells = 1;
		Factory.BuildTicks = 1;
		FHansaCompiledBuildingDefinition Road;
		Road.StableId = TEXT("Building.Road");
		Road.FootprintWidthCells = 1;
		Road.FootprintHeightCells = 1;
		Road.BuildTicks = 1;
		return FHansaEconomicRegistry(
			{ Grain, Flour }, { Recipe }, { Factory, Road }, 0x4C4F474953544943ULL);
	}

	FHansaSimulationDefinitionContext MakeDefinitions()
	{
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Definition<FHansaScenarioId>(TEXT("Scenario.LocalLogisticsTest")),
			0x4C4F474953544943ULL,
			MakeEconomicRegistry()));
	}

	FHansaSimulationDefinitionContext MakeDefinitions(FHansaPlacementTopology PlacementTopology)
	{
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Definition<FHansaScenarioId>(TEXT("Scenario.LocalLogisticsTest")),
			0x4C4F474953544943ULL,
			MakeEconomicRegistry(),
			MoveTemp(PlacementTopology)));
	}

	FHansaCommandHeader CommandHeader(const FHansaSimulationReadOnlyAccess& ReadOnly)
	{
		FHansaCommandHeader Result;
		Result.CommandId = LocalLogisticsTestsEntity<FHansaCommandId>(
			ReadOnly.GetLastProcessedCommandId().GetValue() + 1);
		Result.Authority.IssuingHouseId = LocalLogisticsTestsEntity<FHansaHouseId>(1);
		Result.Authority.PrincipalId = 7;
		Result.Authority.Origin = EHansaCommandOrigin::PlayerInput;
		Result.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Result.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
		return Result;
	}

	FHansaCommandGatewayResult RemoveBuilding(
		FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions,
		FHansaSimulationTransientCache& Cache,
		const uint64 BuildingId)
	{
		const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
		const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(
			CommandHeader(View), FHansaRemoveBuildingCommand {
				LocalLogisticsTestsEntity<FHansaBuildingId>(BuildingId) });
		const TArray<FHansaGameplayCommand> Commands { Command };
		return FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
	}

	FHansaCommandGatewayResult PlaceRoad(
		FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions,
		FHansaSimulationTransientCache& Cache,
		const uint64 BuildingId,
		const int32 X,
		const int32 Y)
	{
		const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
		FHansaPlacementSpec Spec;
		Spec.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Spec.BuildingDefinitionId = Definition<FHansaBuildingTypeId>(TEXT("Building.Road"));
		Spec.Anchor = { X, Y };
		const FHansaGameplayCommand Command = FHansaGameplayCommand::Create(
			CommandHeader(View), FHansaPlaceBuildingCommand {
				LocalLogisticsTestsEntity<FHansaBuildingId>(BuildingId), Spec });
		const TArray<FHansaGameplayCommand> Commands { Command };
		return FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
	}

	void AddAlternateRoadRow(FHansaSimulationInitialization& Initialization)
	{
		Initialization.Placement.Maps[0].BoundsMax.Y = 2;
		for (int32 X = 0; X <= 7; ++X)
		{
			Initialization.Placement.Maps[0].Cells.Add({ { X, 2 }, EHansaPlacementTerrain::Land,
				LocalLogisticsTestsEntity<FHansaHouseId>(1), false });
			Initialization.Buildings.Add(CompletedBuilding(30 + X, TEXT("Building.Road")));
			Initialization.Placement.Placements.Add(Placement(30 + X, TEXT("Building.Road"), X, 2));
		}
	}

	bool Step(FHansaSimulationState& State, const FHansaSimulationDefinitionContext& Definitions,
		FHansaSimulationTransientCache& Cache)
	{
		return FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache).IsSuccess();
	}

	int64 Stock(const FHansaSimulationState& State, const FHansaSimulationDefinitionContext& Definitions,
		const uint64 InventoryId)
	{
		const TOptional<FHansaInventoryStockProjection> Value = State.CreateReadOnlyAccess(Definitions)
			.GetInventories().QueryStock(LocalLogisticsTestsEntity<FHansaInventoryId>(InventoryId),
				Definition<FHansaGoodId>(TEXT("Good.Grain")));
		return Value.IsSet() ? Value->Stock.GetRawValue() : -1;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsProductionDemandTest,
	"Hansa.Simulation.Logistics.ProductionCreatesDeterministicRequests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsProductionDemandTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	FHansaSimulationInitialization Initialization = MakeInitialization();
	Initialization.LocalLogisticsRequests.Reset();
	FHansaProductionInitialization Production;
	Production.Id = LocalLogisticsTestsEntity<FHansaProductionId>(1);
	Production.Kind = EHansaProductionKind::BuildingRecipe;
	Production.BuildingId = LocalLogisticsTestsEntity<FHansaBuildingId>(2);
	Production.RecipeId = Definition<FHansaRecipeId>(TEXT("Recipe.Factory"));
	Production.InputInventoryId = LocalLogisticsTestsEntity<FHansaInventoryId>(2);
	Production.OutputInventoryId = LocalLogisticsTestsEntity<FHansaInventoryId>(2);
	Initialization.Productions = { Production };
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Production-demand tick succeeds"), Step(State, Definitions, Cache));
	const TArray<FHansaLogisticsRequestProjection> Requests =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsRequestProjection();
	TestEqual(TEXT("A missing production input creates one deterministic request"), Requests.Num(), 1);
	TestEqual(TEXT("Warehouse stock is selected as the stable source"),
		Requests[0].SourceInventoryId, LocalLogisticsTestsEntity<FHansaInventoryId>(1));
	TestEqual(TEXT("Production inventory is the delivery destination"),
		Requests[0].DestinationInventoryId, LocalLogisticsTestsEntity<FHansaInventoryId>(2));
	TestEqual(TEXT("Production input requests receive high priority"),
		Requests[0].Priority, EHansaLogisticsPriority::High);
	TestEqual(TEXT("Only the recipe deficit is requested"), Requests[0].RequestedQuantity.GetRawValue(), int64(50));
	while (State.CreateReadOnlyAccess(Definitions).GetClock().GetTick().GetValue() < 9)
	{
		TestTrue(TEXT("Production logistics progression succeeds"), Step(State, Definitions, Cache));
	}
	const FHansaGoodId Flour = Definition<FHansaGoodId>(TEXT("Good.Flour"));
	const bool bOutputCollectionCreated = State.CreateReadOnlyAccess(Definitions)
		.BuildLogisticsRequestProjection().ContainsByPredicate([Flour](const FHansaLogisticsRequestProjection& RequestProjection)
		{
			return RequestProjection.SourceInventoryId == LocalLogisticsTestsEntity<FHansaInventoryId>(2) &&
				RequestProjection.DestinationInventoryId == LocalLogisticsTestsEntity<FHansaInventoryId>(1) &&
				RequestProjection.GoodId == Flour;
		});
	TestTrue(TEXT("Produced goods create a deterministic warehouse collection request"), bOutputCollectionCreated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsDeliveryInvariantTest,
	"Hansa.Simulation.Logistics.CapacityPickupDelayAndCompletedDelivery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsDeliveryInvariantTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MakeInitialization()));
	FHansaSimulationTransientCache Cache;
	const FHansaSimulationReadOnlyAccess Initial = State.CreateReadOnlyAccess(Definitions);
	TestTrue(TEXT("Warehouse, production building and dock share the completed road graph"),
		Initial.QueryLogisticsRoadPath(LocalLogisticsTestsEntity<FHansaInventoryId>(1), LocalLogisticsTestsEntity<FHansaInventoryId>(4)).bConnected);
	TestTrue(TEXT("The city inventory is anchored to a completed physical market"),
		Initial.QueryLogisticsRoadPath(LocalLogisticsTestsEntity<FHansaInventoryId>(2), LocalLogisticsTestsEntity<FHansaInventoryId>(3)).bConnected);

	TestTrue(TEXT("Dispatch tick succeeds"), Step(State, Definitions, Cache));
	auto View = State.CreateReadOnlyAccess(Definitions);
	TArray<FHansaLogisticsJobProjection> Jobs = View.BuildLogisticsJobProjection();
	TestEqual(TEXT("One capacity-bounded job is dispatched per request per tick"), Jobs.Num(), 1);
	TestEqual(TEXT("Job respects configured capacity"), Jobs[0].Quantity.GetRawValue(), int64(100));
	TestEqual(TEXT("Dispatch does not teleport or remove stock before pickup"), Stock(State, Definitions, 1), int64(300));
	TestEqual(TEXT("Destination remains unchanged before delivery"), Stock(State, Definitions, 2), int64(0));

	TestTrue(TEXT("Pickup tick succeeds"), Step(State, Definitions, Cache));
	View = State.CreateReadOnlyAccess(Definitions);
	Jobs = View.BuildLogisticsJobProjection();
	TestEqual(TEXT("First cargo leaves source only at pickup"), Stock(State, Definitions, 1), int64(200));
	TestEqual(TEXT("Picked cargo is authoritative in transit"), Jobs[0].CargoQuantity.GetRawValue(), int64(100));
	TestEqual(TEXT("No delivery occurs before the road-derived completion tick"), Stock(State, Definitions, 2), int64(0));
	int64 Conserved = Stock(State, Definitions, 1) + Stock(State, Definitions, 2);
	for (const FHansaLogisticsJobProjection& Job : Jobs)
	{
		Conserved += Job.CargoQuantity.GetRawValue();
	}
	TestEqual(TEXT("Inventory plus in-transit cargo is conserved"), Conserved, int64(300));

	while (State.CreateReadOnlyAccess(Definitions).GetClock().GetTick().GetValue() < 9)
	{
		TestTrue(TEXT("Delivery progression tick succeeds"), Step(State, Definitions, Cache));
	}
	View = State.CreateReadOnlyAccess(Definitions);
	const TOptional<FHansaLogisticsRequestProjection> Completed =
		View.QueryLogisticsRequest(LocalLogisticsTestsEntity<FHansaLogisticsRequestId>(1));
	TestTrue(TEXT("Typed request query returns the completed demand"), Completed.IsSet());
	TestEqual(TEXT("All requested goods arrive only through completed jobs"), Stock(State, Definitions, 2), int64(250));
	TestEqual(TEXT("Unrequested stock remains at the warehouse"), Stock(State, Definitions, 1), int64(50));
	TestEqual(TEXT("Request reaches completed status"), Completed->Status, EHansaLogisticsRequestStatus::Completed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsBottleneckTest,
	"Hansa.Simulation.Logistics.DisconnectedRoadAndFullDestinationBottlenecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsBottleneckTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	FHansaSimulationState Disconnected = Require(FHansaSimulationState::TryCreate(MakeInitialization(true)));
	TestTrue(TEXT("Disconnected fixture advances"), Step(Disconnected, Definitions, Cache));
	const auto DisconnectedRequest = Disconnected.CreateReadOnlyAccess(Definitions)
		.QueryLogisticsRequest(LocalLogisticsTestsEntity<FHansaLogisticsRequestId>(1));
	TestEqual(TEXT("Disconnected road is preserved as the causal factor"),
		DisconnectedRequest->Bottleneck, EHansaLogisticsBottleneck::DisconnectedRoad);
	TestEqual(TEXT("Disconnected request dispatches no jobs"),
		Disconnected.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection().Num(), 0);

	FHansaSimulationState Full = Require(FHansaSimulationState::TryCreate(MakeInitialization(false, true)));
	Cache.Discard();
	TestTrue(TEXT("Full-destination fixture advances"), Step(Full, Definitions, Cache));
	const auto FullRequest = Full.CreateReadOnlyAccess(Definitions)
		.QueryLogisticsRequest(LocalLogisticsTestsEntity<FHansaLogisticsRequestId>(1));
	TestEqual(TEXT("Full destination is preserved as the causal factor"),
		FullRequest->Bottleneck, EHansaLogisticsBottleneck::DestinationFull);
	TestEqual(TEXT("Full destination receives no reserved delivery"),
		Full.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsPriorityTest,
	"Hansa.Simulation.Logistics.CompetingRequestsUseStablePriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsPriorityTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	TArray<FHansaLogisticsRequestInitialization> Requests = {
		Request(1, 1, 2, 50, EHansaLogisticsPriority::Low),
		Request(2, 1, 2, 50, EHansaLogisticsPriority::Critical)
	};
	FHansaSimulationInitialization Initialization = MakeInitialization(false, false, false, MoveTemp(Requests));
	Initialization.LocalLogisticsSettings.MaximumConcurrentJobs = 1;
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Priority fixture advances"), Step(State, Definitions, Cache));
	const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
	const TArray<FHansaLogisticsJobProjection> Jobs = View.BuildLogisticsJobProjection();
	TestEqual(TEXT("Only one fleet slot is consumed"), Jobs.Num(), 1);
	TestEqual(TEXT("Critical request wins despite its later stable id"),
		Jobs[0].RequestId, LocalLogisticsTestsEntity<FHansaLogisticsRequestId>(2));
	TestEqual(TEXT("Lower-priority request reports fleet contention"),
		View.QueryLogisticsRequest(LocalLogisticsTestsEntity<FHansaLogisticsRequestId>(1))->Bottleneck,
		EHansaLogisticsBottleneck::FleetCapacity);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsPhysicalMarketAccessTest,
	"Hansa.Simulation.Logistics.PhysicalMarketAccessAndDeterministicSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsPhysicalMarketAccessTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	auto Query = [&Definitions](FHansaSimulationInitialization Initialization,
		const uint64 SourceInventoryId, const uint64 DestinationInventoryId)
	{
		const FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
		return State.CreateReadOnlyAccess(Definitions).QueryLogisticsRoadPath(
			LocalLogisticsTestsEntity<FHansaInventoryId>(SourceInventoryId),
			LocalLogisticsTestsEntity<FHansaInventoryId>(DestinationInventoryId));
	};
	auto FindBuilding = [](FHansaSimulationInitialization& Initialization, const uint64 BuildingId)
		-> FHansaBuildingState*
	{
		return Initialization.Buildings.FindByPredicate([BuildingId](const FHansaBuildingState& Building)
		{
			return Building.Id == LocalLogisticsTestsEntity<FHansaBuildingId>(BuildingId);
		});
	};
	auto FindPlacement = [](FHansaSimulationInitialization& Initialization, const uint64 BuildingId)
		-> FHansaPlacedBuildingRecord*
	{
		return Initialization.Placement.Placements.FindByPredicate(
			[BuildingId](const FHansaPlacedBuildingRecord& Record)
			{
				return Record.BuildingId == LocalLogisticsTestsEntity<FHansaBuildingId>(BuildingId);
			});
	};

	FHansaSimulationInitialization NoMarket = MakeInitialization();
	NoMarket.Buildings.RemoveAll([](const FHansaBuildingState& Building)
	{
		return Building.DefinitionId.ToString() == TEXT("Building.Market");
	});
	NoMarket.Placement.Placements.RemoveAll([](const FHansaPlacedBuildingRecord& Record)
	{
		return Record.Spec.BuildingDefinitionId.ToString() == TEXT("Building.Market");
	});
	const FHansaPlacementState NoMarketPlacement =
		Require(FHansaPlacementState::TryCreate(NoMarket.Placement));
	const FHansaLogisticsRoadPathProjection NoMarketRoadAccess =
		FHansaLocalLogisticsQueries::QueryBuildingRoadAccess(
			LocalLogisticsTestsEntity<FHansaBuildingId>(2), NoMarketPlacement, NoMarket.Buildings);
	TestTrue(TEXT("Road adjacency remains valid without a Market"), NoMarketRoadAccess.bConnected);
	TestTrue(TEXT("Road-only diagnostics retain the adjacent completed road cell"),
		NoMarketRoadAccess.SourceAccessCells.Contains(FHansaGridCoordinate { 3, 0 }));
	const FHansaLogisticsRoadPathProjection NoMarketPath = Query(MoveTemp(NoMarket), 2, 3);
	TestFalse(TEXT("Roads without a completed market do not grant city-economy access"), NoMarketPath.bConnected);
	TestEqual(TEXT("A missing physical market has a causal result"),
		NoMarketPath.Failure, EHansaLogisticsRoadPathFailure::NoOperationalMarket);
	TestEqual(TEXT("A missing market exposes an actionable remedy"),
		NoMarketPath.RemedyKey, FName(TEXT("Hansa.Logistics.Path.Remedy.BuildAndCompleteMarket")));

	FHansaSimulationInitialization IsolatedMarket = MakeInitialization();
	IsolatedMarket.Placement.Maps[0].BoundsMax.Y = 2;
	IsolatedMarket.Placement.Maps[0].Cells.Add({ { 7, 2 }, EHansaPlacementTerrain::Land,
		LocalLogisticsTestsEntity<FHansaHouseId>(1), false });
	FHansaPlacedBuildingRecord* IsolatedMarketPlacement = FindPlacement(IsolatedMarket, 5);
	FHansaPlacedBuildingRecord* IsolatedRoadPlacement = FindPlacement(IsolatedMarket, 17);
	check(IsolatedMarketPlacement != nullptr && IsolatedRoadPlacement != nullptr);
	IsolatedMarketPlacement->Spec.Anchor = { 7, 2 };
	IsolatedMarketPlacement->OccupiedCells = { { 7, 2 } };
	IsolatedRoadPlacement->Spec.Anchor = { 7, 1 };
	IsolatedRoadPlacement->OccupiedCells = { { 7, 1 } };
	const FHansaLogisticsRoadPathProjection IsolatedMarketPath = Query(MoveTemp(IsolatedMarket), 2, 3);
	TestFalse(TEXT("A road component without the market cannot use another component as a hand-off"),
		IsolatedMarketPath.bConnected);
	TestEqual(TEXT("An isolated market reports that the source network does not reach it"),
		IsolatedMarketPath.Failure, EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket);

	FHansaSimulationInitialization Diagonal = MakeInitialization();
	Diagonal.Buildings.RemoveAll([](const FHansaBuildingState& Building)
	{
		return Building.DefinitionId.ToString() == TEXT("Building.Road") &&
			Building.Id != LocalLogisticsTestsEntity<FHansaBuildingId>(11);
	});
	Diagonal.Placement.Placements.RemoveAll([](const FHansaPlacedBuildingRecord& Record)
	{
		return Record.Spec.BuildingDefinitionId.ToString() == TEXT("Building.Road") &&
			Record.BuildingId != LocalLogisticsTestsEntity<FHansaBuildingId>(11);
	});
	FHansaPlacedBuildingRecord* DiagonalMarketPlacement = FindPlacement(Diagonal, 5);
	check(DiagonalMarketPlacement != nullptr);
	DiagonalMarketPlacement->Spec.Anchor = { 2, 0 };
	DiagonalMarketPlacement->OccupiedCells = { { 2, 0 } };
	const FHansaLogisticsRoadPathProjection DiagonalPath = Query(MoveTemp(Diagonal), 1, 3);
	TestFalse(TEXT("Diagonal building contact does not connect to a road"), DiagonalPath.bConnected);
	TestEqual(TEXT("Diagonal contact is diagnosed as missing source road adjacency"),
		DiagonalPath.Failure, EHansaLogisticsRoadPathFailure::SourceNotAdjacentToRoad);

	FHansaSimulationInitialization UnfinishedRoad = MakeInitialization();
	FHansaBuildingState* RoadAcrossNetwork = FindBuilding(UnfinishedRoad, 13);
	check(RoadAcrossNetwork != nullptr);
	RoadAcrossNetwork->ConstructionState = EHansaConstructionState::UnderConstruction;
	RoadAcrossNetwork->ConstructionProgress = FHansaRate();
	const FHansaLogisticsRoadPathProjection BrokenNetworkPath = Query(MoveTemp(UnfinishedRoad), 1, 4);
	TestFalse(TEXT("An unfinished road does not bridge completed road components"), BrokenNetworkPath.bConnected);
	TestEqual(TEXT("The component without the market reports the market disconnection"),
		BrokenNetworkPath.Failure, EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket);
	const FHansaLogisticsRoadPathProjection ReconnectedPath = Query(MakeInitialization(), 1, 4);
	TestTrue(TEXT("Completing the missing road immediately restores authoritative connectivity"),
		ReconnectedPath.bConnected);

	FHansaSimulationInitialization UnfinishedMarket = MakeInitialization();
	FHansaBuildingState* MarketBuilding = FindBuilding(UnfinishedMarket, 5);
	check(MarketBuilding != nullptr);
	MarketBuilding->ConstructionState = EHansaConstructionState::UnderConstruction;
	MarketBuilding->ConstructionProgress = FHansaRate();
	const FHansaLogisticsRoadPathProjection UnfinishedMarketPath = Query(MoveTemp(UnfinishedMarket), 1, 4);
	TestFalse(TEXT("An unfinished market cannot grant market access"), UnfinishedMarketPath.bConnected);
	TestEqual(TEXT("An unfinished market is not operational"),
		UnfinishedMarketPath.Failure, EHansaLogisticsRoadPathFailure::NoOperationalMarket);

	FHansaSimulationInitialization MarketWithoutRoad = MakeInitialization();
	MarketWithoutRoad.Buildings.RemoveAll([](const FHansaBuildingState& Building)
	{
		return Building.Id == LocalLogisticsTestsEntity<FHansaBuildingId>(17);
	});
	MarketWithoutRoad.Placement.Placements.RemoveAll([](const FHansaPlacedBuildingRecord& Record)
	{
		return Record.BuildingId == LocalLogisticsTestsEntity<FHansaBuildingId>(17);
	});
	const FHansaLogisticsRoadPathProjection MarketWithoutRoadPath = Query(MoveTemp(MarketWithoutRoad), 2, 3);
	TestFalse(TEXT("A completed market still needs orthogonal road access"), MarketWithoutRoadPath.bConnected);
	TestEqual(TEXT("A market without a road has a distinct causal result"),
		MarketWithoutRoadPath.Failure, EHansaLogisticsRoadPathFailure::NoMarketRoadAccess);

	FHansaSimulationInitialization MultipleMarkets = MakeInitialization();
	MultipleMarkets.Buildings.Add(CompletedBuilding(6, TEXT("Building.Market")));
	MultipleMarkets.Placement.Placements.Add(Placement(6, TEXT("Building.Market"), 1, 1));
	const FHansaLogisticsRoadPathProjection ClosestMarketPath = Query(MultipleMarkets, 2, 3);
	TestTrue(TEXT("A city inventory selects an eligible physical market"), ClosestMarketPath.bMarketEligible);
	TestEqual(TEXT("A legacy city inventory stays bound to the lowest stable market"),
		ClosestMarketPath.SelectedMarketBuildingId, LocalLogisticsTestsEntity<FHansaBuildingId>(5));
	const FHansaLogisticsRoadPathProjection StableTiePath = Query(MoveTemp(MultipleMarkets), 1, 2);
	TestEqual(TEXT("Equal delivery routes use stable market building id as the tie-breaker"),
		StableTiePath.SelectedMarketBuildingId, LocalLogisticsTestsEntity<FHansaBuildingId>(5));

	FHansaSimulationInitialization SeparateMarkets = MakeInitialization(true);
	SeparateMarkets.Buildings.Add(CompletedBuilding(6, TEXT("Building.Market")));
	SeparateMarkets.Placement.Placements.Add(Placement(6, TEXT("Building.Market"), 2, 1));
	FHansaInventoryInitialization* RightMarketInventory = SeparateMarkets.Inventories.FindByPredicate(
		[](const FHansaInventoryInitialization& Inventory)
		{
			return Inventory.Id == LocalLogisticsTestsEntity<FHansaInventoryId>(3);
		});
	if (!TestNotNull(TEXT("Right market inventory exists"), RightMarketInventory)) return false;
	RightMarketInventory->BuildingId = LocalLogisticsTestsEntity<FHansaBuildingId>(5);
	SeparateMarkets.Inventories.Add(CityMarketInventory(200, 5, 6));
	const FHansaLogisticsRoadPathProjection SeparateMarketPath = Query(SeparateMarkets, 3, 5);
	TestFalse(TEXT("Disconnected physical markets cannot exchange city stock"), SeparateMarketPath.bConnected);
	TestEqual(TEXT("Separate markets report disconnected endpoints"), SeparateMarketPath.Failure,
		EHansaLogisticsRoadPathFailure::EndpointsDisconnected);
	const FHansaLogisticsRoadPathProjection IsolatedWarehousePath = Query(MoveTemp(SeparateMarkets), 1, 3);
	TestFalse(TEXT("A warehouse cannot reach stock bound to another market network"),
		IsolatedWarehousePath.bConnected);

	FHansaSimulationInitialization OtherOwner = MakeInitialization();
	OtherOwner.Houses.Add({ LocalLogisticsTestsEntity<FHansaHouseId>(2), FHansaMoney::FromRaw(100000) });
	FindBuilding(OtherOwner, 5)->OwnerId = LocalLogisticsTestsEntity<FHansaHouseId>(2);
	FindPlacement(OtherOwner, 5)->OwnerId = LocalLogisticsTestsEntity<FHansaHouseId>(2);
	TestFalse(TEXT("Existing placement ownership rules reject a market on land owned by another house"),
		FHansaSimulationState::TryCreate(MoveTemp(OtherOwner)).IsSuccess());

	FHansaSimulationInitialization CrossCity = MakeInitialization();
	const FHansaCityDefinitionId Rostock = Definition<FHansaCityDefinitionId>(TEXT("City.Rostock"));
	CrossCity.Cities.Add({ Rostock, FHansaQuantity() });
	FHansaPlacementMapInitialization RostockMap = CrossCity.Placement.Maps[0];
	RostockMap.CityId = Rostock;
	CrossCity.Placement.Maps.Add(MoveTemp(RostockMap));
	FHansaInventoryInitialization RostockInventory = CityMarketInventory(0);
	RostockInventory.Id = LocalLogisticsTestsEntity<FHansaInventoryId>(8);
	RostockInventory.CityId = Rostock;
	CrossCity.Inventories.Add(MoveTemp(RostockInventory));
	const FHansaLogisticsRoadPathProjection CrossCityPath = Query(MoveTemp(CrossCity), 1, 8);
	TestFalse(TEXT("Local roads cannot cross city boundaries"), CrossCityPath.bConnected);
	TestEqual(TEXT("Cross-city inventory access requires an intercity route"),
		CrossCityPath.Failure, EHansaLogisticsRoadPathFailure::DifferentCities);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsTopologyBeforePickupTest,
	"Hansa.Simulation.Logistics.TopologyChangeBeforePickupReleasesReservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsTopologyBeforePickupTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	FHansaSimulationInitialization Initialization = MakeInitialization(false, false, false,
		{ Request(1, 1, 2, 100) });
	FHansaPlacementTopology PlacementTopology = Require(
		FHansaPlacementTopology::TryCreate(MoveTemp(Initialization.Placement.Maps)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(MoveTemp(PlacementTopology));
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(
		MoveTemp(Initialization), Definitions.GetPlacementTopologyShared()));
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Delivery dispatch succeeds"), Step(State, Definitions, Cache));
	const FHansaLogisticsJobProjection Dispatched =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0];
	TestEqual(TEXT("Delivery waits for its pickup tick"), Dispatched.Status,
		EHansaLogisticsJobStatus::AwaitingPickup);
	TestTrue(TEXT("Dispatch persists a deterministic route"), !Dispatched.RouteCells.IsEmpty());

	FHansaSimulationState SourceRemoval = State;
	FHansaSimulationTransientCache SourceCache;
	TestEqual(TEXT("An active source cannot be demolished"),
		RemoveBuilding(SourceRemoval, Definitions, SourceCache, 1).GetError(),
		EHansaCommandGatewayError::TargetHasCargoObligations);
	FHansaSimulationState DestinationRemoval = State;
	FHansaSimulationTransientCache DestinationCache;
	TestEqual(TEXT("An active destination cannot be demolished"),
		RemoveBuilding(DestinationRemoval, Definitions, DestinationCache, 2).GetError(),
		EHansaCommandGatewayError::TargetHasCargoObligations);
	FHansaSimulationState MarketRemoval = State;
	FHansaSimulationTransientCache MarketCache;
	TestEqual(TEXT("The selected market cannot be demolished while the delivery is active"),
		RemoveBuilding(MarketRemoval, Definitions, MarketCache, 5).GetError(),
		EHansaCommandGatewayError::TargetHasCargoObligations);

	TestTrue(TEXT("Removing a route road is a valid player command"),
		RemoveBuilding(State, Definitions, Cache, 12).IsSuccess());
	auto Jobs = State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection();
	TestEqual(TEXT("A broken route pauses before pickup"), Jobs[0].Status,
		EHansaLogisticsJobStatus::PausedAwaitingPickup);
	TestEqual(TEXT("Blocked pickup leaves source stock untouched"), Stock(State, Definitions, 1), int64(300));
	TestEqual(TEXT("Blocked pickup carries no cargo"), Jobs[0].CargoQuantity.GetRawValue(), int64(0));
	TestEqual(TEXT("The source reservation is released immediately"),
		State.CreateReadOnlyAccess(Definitions).GetInventories().CaptureSnapshot().GetReservations().Num(), 0);
	TestEqual(TEXT("The paused job exposes the causal route failure"), Jobs[0].PauseReason,
		EHansaLogisticsRoadPathFailure::SourceNotConnectedToMarket);

	TestTrue(TEXT("Replacement road placement succeeds"), PlaceRoad(State, Definitions, Cache, 50, 2, 0).IsSuccess());
	TestTrue(TEXT("Replacement road construction completes"), Step(State, Definitions, Cache));
	TestTrue(TEXT("Completed replacement road is observed on the next logistics tick"), Step(State, Definitions, Cache));
	Jobs = State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection();
	TestEqual(TEXT("Reconnection reacquires stock for the same job"), Jobs[0].Status,
		EHansaLogisticsJobStatus::AwaitingPickup);
	TestEqual(TEXT("Reconnection itself does not remove stock"), Stock(State, Definitions, 1), int64(300));
	TestEqual(TEXT("Exactly one reservation is reacquired"),
		State.CreateReadOnlyAccess(Definitions).GetInventories().CaptureSnapshot().GetReservations().Num(), 1);
	TestTrue(TEXT("The resumed pickup tick succeeds"), Step(State, Definitions, Cache));
	TestEqual(TEXT("The same job owns cargo after its delayed pickup"),
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0].CargoQuantity.GetRawValue(), int64(100));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsPausedFleetCapacityTest,
	"Hansa.Simulation.Logistics.PausedJobReleasesFleetCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsPausedFleetCapacityTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	FHansaSimulationInitialization Initialization = MakeInitialization(false, false, false, {
		Request(1, 1, 2, 100, EHansaLogisticsPriority::Normal),
		Request(2, 3, 2, 100, EHansaLogisticsPriority::Low) });
	Initialization.LocalLogisticsSettings.MaximumConcurrentJobs = 1;
	FHansaInventoryInitialization* MarketInventory = Initialization.Inventories.FindByPredicate(
		[](const FHansaInventoryInitialization& Inventory)
		{
			return Inventory.Id == LocalLogisticsTestsEntity<FHansaInventoryId>(3);
		});
	if (!TestNotNull(TEXT("Market inventory exists"), MarketInventory)) return false;
	MarketInventory->InitialStock = { {
		Definition<FHansaGoodId>(TEXT("Good.Grain")), FHansaQuantity::FromRaw(100) } };
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("First request dispatches"), Step(State, Definitions, Cache));
	TestEqual(TEXT("One moving fleet slot is initially occupied"),
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection().Num(), 1);
	TestTrue(TEXT("Breaking the first route succeeds"), RemoveBuilding(State, Definitions, Cache, 12).IsSuccess());
	const TArray<FHansaLogisticsJobProjection> Jobs =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection();
	TestEqual(TEXT("The paused job remains accounted for and a second job can dispatch"), Jobs.Num(), 2);
	TestEqual(TEXT("First job is paused before pickup"), Jobs[0].Status,
		EHansaLogisticsJobStatus::PausedAwaitingPickup);
	TestEqual(TEXT("Paused job no longer consumes the moving fleet slot"), Jobs[1].Status,
		EHansaLogisticsJobStatus::AwaitingPickup);
	TestEqual(TEXT("The eligible market-side request owns the free fleet slot"), Jobs[1].RequestId,
		LocalLogisticsTestsEntity<FHansaLogisticsRequestId>(2));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsInTransitPauseSaveTest,
	"Hansa.Simulation.Logistics.InTransitPauseSaveAndResume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsInTransitPauseSaveTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	FHansaSimulationInitialization Initialization = MakeInitialization(false, false, false,
		{ Request(1, 1, 2, 100) });
	FHansaPlacementTopology PlacementTopology = Require(
		FHansaPlacementTopology::TryCreate(MoveTemp(Initialization.Placement.Maps)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions(MoveTemp(PlacementTopology));
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(
		MoveTemp(Initialization), Definitions.GetPlacementTopologyShared()));
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Dispatch succeeds"), Step(State, Definitions, Cache));
	TestTrue(TEXT("Pickup succeeds"), Step(State, Definitions, Cache));
	while (State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0].RemainingTravelTicks > 1)
	{
		TestTrue(TEXT("Travel advances toward the final road tick"), Step(State, Definitions, Cache));
	}
	const FHansaLogisticsJobProjection BeforeBreak =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0];
	TestEqual(TEXT("Fixture reaches the tick immediately before delivery"), BeforeBreak.RemainingTravelTicks, 1);
	TestTrue(TEXT("Final-road removal succeeds"), RemoveBuilding(State, Definitions, Cache, 12).IsSuccess());
	const FHansaLogisticsJobProjection Paused =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0];
	TestEqual(TEXT("A road break immediately before delivery pauses cargo"), Paused.Status,
		EHansaLogisticsJobStatus::PausedInTransit);
	TestEqual(TEXT("Elapsed travel is preserved while blocked"), Paused.ElapsedTravelTicks,
		BeforeBreak.ElapsedTravelTicks);
	TestEqual(TEXT("Remaining travel is preserved while blocked"), Paused.RemainingTravelTicks,
		BeforeBreak.RemainingTravelTicks);
	TestEqual(TEXT("Blocked cargo cannot appear at its destination"), Stock(State, Definitions, 2), int64(0));
	TestEqual(TEXT("Blocked goods remain authoritative cargo"), Paused.CargoQuantity.GetRawValue(), int64(100));

	FHansaSaveSnapshot Snapshot;
	Snapshot.State = State;
	Snapshot.BuildVersion = TEXT("Prompt3-test");
	Snapshot.SavedUtc = TEXT("2026-09-11T00:00:00Z");
	Snapshot.DisplayName = TEXT("Paused local delivery");
	Snapshot.Players = { { 7, LocalLogisticsTestsEntity<FHansaHouseId>(1) } };
	TArray<uint8> Bytes;
	const FHansaSaveResult Encoded = FHansaSaveEnvelope::Encode(Snapshot, Definitions, Bytes);
	if (!TestTrue(*Encoded.Message, Encoded.IsSuccess())) return false;
	FHansaSaveSnapshot Loaded;
	const FHansaSaveResult Decoded = FHansaSaveEnvelope::Decode(Bytes, Definitions, Loaded);
	if (!TestTrue(*Decoded.Message, Decoded.IsSuccess())) return false;
	const FHansaLogisticsJobProjection Restored =
		Loaded.State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0];
	TestEqual(TEXT("Save/load preserves paused status"), Restored.Status, Paused.Status);
	TestEqual(TEXT("Save/load preserves exact cargo"), Restored.CargoQuantity, Paused.CargoQuantity);
	TestEqual(TEXT("Save/load preserves route"), Restored.RouteCells, Paused.RouteCells);
	TestEqual(TEXT("Save/load preserves elapsed travel"), Restored.ElapsedTravelTicks, Paused.ElapsedTravelTicks);
	TestEqual(TEXT("Save/load preserves remaining travel"), Restored.RemainingTravelTicks, Paused.RemainingTravelTicks);

	FHansaSimulationTransientCache LoadedCache;
	TestTrue(TEXT("Blocked loaded state remains stable"), Step(Loaded.State, Definitions, LoadedCache));
	TestEqual(TEXT("A saved blocked job does not deliver by its old delivery tick"),
		Stock(Loaded.State, Definitions, 2), int64(0));
	TestTrue(TEXT("Replacement road placement succeeds after load"),
		PlaceRoad(Loaded.State, Definitions, LoadedCache, 50, 2, 0).IsSuccess());
	TestTrue(TEXT("The replacement road completes after load"), Step(Loaded.State, Definitions, LoadedCache));
	TestTrue(TEXT("The completed road reconnects the loaded job"), Step(Loaded.State, Definitions, LoadedCache));
	const FHansaLogisticsJobProjection Resumed =
		Loaded.State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0];
	TestEqual(TEXT("Paused cargo resumes in transit"), Resumed.Status, EHansaLogisticsJobStatus::InTransit);
	TestEqual(TEXT("Resume preserves completed travel"), Resumed.ElapsedTravelTicks, Paused.ElapsedTravelTicks);
	TestEqual(TEXT("Resume cannot deliver on the reconnection tick"), Stock(Loaded.State, Definitions, 2), int64(0));
	TestTrue(TEXT("The final connected travel tick succeeds"), Step(Loaded.State, Definitions, LoadedCache));
	TestEqual(TEXT("Cargo arrives only after connected travel completes"), Stock(Loaded.State, Definitions, 2), int64(100));
	TestEqual(TEXT("Source, destination, and cargo remain exactly conserved"),
		Stock(Loaded.State, Definitions, 1) + Stock(Loaded.State, Definitions, 2) +
		Loaded.State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0].CargoQuantity.GetRawValue(),
		int64(300));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsAlternateRouteTest,
	"Hansa.Simulation.Logistics.ActiveDeliveryUsesDeterministicAlternateRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsAlternateRouteTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	FHansaSimulationInitialization Initialization = MakeInitialization(false, false, false,
		{ Request(1, 1, 2, 100) });
	AddAlternateRoadRow(Initialization);
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Alternate-route dispatch succeeds"), Step(State, Definitions, Cache));
	TestTrue(TEXT("Alternate-route pickup succeeds"), Step(State, Definitions, Cache));
	TestTrue(TEXT("Initial route advances"), Step(State, Definitions, Cache));
	const FHansaLogisticsJobProjection BeforeReroute =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0];
	TestTrue(TEXT("Stable tie breaking initially selects the lower road row"),
		BeforeReroute.RouteCells.ContainsByPredicate([](const FHansaGridCoordinate Cell) { return Cell.Y == 0; }));
	TestTrue(TEXT("Removing one route cell succeeds because an alternate route exists"),
		RemoveBuilding(State, Definitions, Cache, 12).IsSuccess());
	const FHansaLogisticsJobProjection Rerouted =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0];
	TestEqual(TEXT("An available alternate route avoids a pause"), Rerouted.Status,
		EHansaLogisticsJobStatus::InTransit);
	TestTrue(TEXT("The authoritative route moves to the intact road row"),
		Rerouted.RouteCells.ContainsByPredicate([](const FHansaGridCoordinate Cell) { return Cell.Y == 2; }));
	TestEqual(TEXT("The reroute update preserves completed travel without moving prematurely"),
		Rerouted.ElapsedTravelTicks, BeforeReroute.ElapsedTravelTicks);
	TestEqual(TEXT("Rerouting does not duplicate or drop cargo"), Rerouted.CargoQuantity.GetRawValue(), int64(100));
	TestTrue(TEXT("Travel advances on the next tick of the alternate route"), Step(State, Definitions, Cache));
	TestTrue(TEXT("Completed travel increases after one full alternate-route tick"),
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0].ElapsedTravelTicks >
		Rerouted.ElapsedTravelTicks);
	while (State.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection()[0].Status !=
		EHansaLogisticsJobStatus::Completed)
	{
		TestTrue(TEXT("Rerouted delivery advances"), Step(State, Definitions, Cache));
	}
	TestEqual(TEXT("Rerouted cargo reaches the destination once"), Stock(State, Definitions, 2), int64(100));
	TestEqual(TEXT("Rerouted transfer conserves all goods"),
		Stock(State, Definitions, 1) + Stock(State, Definitions, 2), int64(300));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLocalLogisticsHashDeterminismTest,
	"Hansa.Simulation.Logistics.StateHashIgnoresDiscoveryOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLocalLogisticsHashDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::LocalLogistics;
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState Forward = Require(FHansaSimulationState::TryCreate(MakeInitialization()));
	FHansaSimulationState Reversed = Require(FHansaSimulationState::TryCreate(MakeInitialization(false, false, true)));
	FHansaSimulationTransientCache ForwardCache;
	FHansaSimulationTransientCache ReversedCache;
	TestEqual(TEXT("Canonical initial logistics state hashes match"),
		Forward.CreateReadOnlyAccess(Definitions).BuildStateHashReport().GetOverallHash(),
		Reversed.CreateReadOnlyAccess(Definitions).BuildStateHashReport().GetOverallHash());
	for (int32 Tick = 0; Tick < 9; ++Tick)
	{
		TestTrue(TEXT("Forward deterministic step succeeds"), Step(Forward, Definitions, ForwardCache));
		TestTrue(TEXT("Reversed deterministic step succeeds"), Step(Reversed, Definitions, ReversedCache));
		TestEqual(TEXT("Per-tick logistics state hashes match"),
			Forward.CreateReadOnlyAccess(Definitions).BuildStateHashReport().GetOverallHash(),
			Reversed.CreateReadOnlyAccess(Definitions).BuildStateHashReport().GetOverallHash());
	}
	const FHansaSubsystemStateHash* LogisticsHash =
		Forward.CreateReadOnlyAccess(Definitions).BuildStateHashReport().Find(EHansaStateHashSubsystem::Logistics);
	TestNotNull(TEXT("Logistics has an independently diagnosable state-hash subsystem"), LogisticsHash);
	return true;
}

#endif
