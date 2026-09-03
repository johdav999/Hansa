#include "Algo/Reverse.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Diagnostics/HansaStateHash.h"
#include "Logistics/HansaLocalLogistics.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
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
	TId Entity(const uint64 Value)
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
		Result.Id = Entity<FHansaBuildingId>(Id);
		Result.DefinitionId = Definition<FHansaBuildingTypeId>(DefinitionId);
		Result.OwnerId = Entity<FHansaHouseId>(1);
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
		Result.BuildingId = Entity<FHansaBuildingId>(Id);
		Result.OwnerId = Entity<FHansaHouseId>(1);
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
		Result.Id = Entity<FHansaInventoryId>(InventoryId);
		Result.OwnerKind = Kind;
		Result.BuildingId = Entity<FHansaBuildingId>(BuildingId);
		Result.Capacity = FHansaQuantity::FromRaw(Capacity);
		Result.AcceptedGoods = { Grain, Flour };
		if (Stock > 0)
		{
			Result.InitialStock = { { Grain, FHansaQuantity::FromRaw(Stock) } };
		}
		return Result;
	}

	FHansaInventoryInitialization CityMarketInventory(const int64 Stock)
	{
		const FHansaGoodId Grain = Definition<FHansaGoodId>(TEXT("Good.Grain"));
		const FHansaGoodId Flour = Definition<FHansaGoodId>(TEXT("Good.Flour"));
		FHansaInventoryInitialization Result;
		Result.Id = Entity<FHansaInventoryId>(3);
		Result.OwnerKind = EHansaInventoryOwnerKind::City;
		Result.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
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
		Result.Id = Entity<FHansaLogisticsRequestId>(RequestId);
		Result.SourceInventoryId = Entity<FHansaInventoryId>(SourceInventory);
		Result.DestinationInventoryId = Entity<FHansaInventoryId>(DestinationInventory);
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
		Result.Houses = { { Entity<FHansaHouseId>(1), FHansaMoney::FromRaw(100'000) } };
		Result.Cities = { {
			Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck")), FHansaQuantity() } };

		Result.Buildings = {
			CompletedBuilding(1, TEXT("Building.Warehouse")),
			CompletedBuilding(2, TEXT("Building.Factory")),
			CompletedBuilding(4, TEXT("Building.Dock"))
		};
		for (int32 X = 0; X <= 6; ++X)
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
		Map.BoundsMax = { 6, 1 };
		Map.RoadBuildingDefinitionId = Definition<FHansaBuildingTypeId>(TEXT("Building.Road"));
		for (int32 X = 0; X <= 6; ++X)
		{
			for (int32 Y = 0; Y <= 1; ++Y)
			{
				Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, Entity<FHansaHouseId>(1), false });
			}
		}
		Result.Placement.Maps = { MoveTemp(Map) };
		Result.Placement.Placements = {
			Placement(1, TEXT("Building.Warehouse"), 0, 1),
			Placement(2, TEXT("Building.Factory"), 3, 1),
			Placement(4, TEXT("Building.Dock"), 6, 1)
		};
		for (int32 X = 0; X <= 6; ++X)
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

	FHansaSimulationDefinitionContext MakeDefinitions()
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
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Definition<FHansaScenarioId>(TEXT("Scenario.LocalLogisticsTest")),
			0x4C4F474953544943ULL,
			FHansaEconomicRegistry({ Grain, Flour }, { Recipe }, { Factory },
				0x4C4F474953544943ULL)));
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
			.GetInventories().QueryStock(Entity<FHansaInventoryId>(InventoryId),
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
	Production.Id = Entity<FHansaProductionId>(1);
	Production.Kind = EHansaProductionKind::BuildingRecipe;
	Production.BuildingId = Entity<FHansaBuildingId>(2);
	Production.RecipeId = Definition<FHansaRecipeId>(TEXT("Recipe.Factory"));
	Production.InputInventoryId = Entity<FHansaInventoryId>(2);
	Production.OutputInventoryId = Entity<FHansaInventoryId>(2);
	Initialization.Productions = { Production };
	FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	TestTrue(TEXT("Production-demand tick succeeds"), Step(State, Definitions, Cache));
	const TArray<FHansaLogisticsRequestProjection> Requests =
		State.CreateReadOnlyAccess(Definitions).BuildLogisticsRequestProjection();
	TestEqual(TEXT("A missing production input creates one deterministic request"), Requests.Num(), 1);
	TestEqual(TEXT("Warehouse stock is selected as the stable source"),
		Requests[0].SourceInventoryId, Entity<FHansaInventoryId>(1));
	TestEqual(TEXT("Production inventory is the delivery destination"),
		Requests[0].DestinationInventoryId, Entity<FHansaInventoryId>(2));
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
			return RequestProjection.SourceInventoryId == Entity<FHansaInventoryId>(2) &&
				RequestProjection.DestinationInventoryId == Entity<FHansaInventoryId>(1) &&
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
		Initial.QueryLogisticsRoadPath(Entity<FHansaInventoryId>(1), Entity<FHansaInventoryId>(4)).bConnected);
	TestTrue(TEXT("The city market aggregate is a typed road hand-off node"),
		Initial.QueryLogisticsRoadPath(Entity<FHansaInventoryId>(2), Entity<FHansaInventoryId>(3)).bConnected);

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
		View.QueryLogisticsRequest(Entity<FHansaLogisticsRequestId>(1));
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
		.QueryLogisticsRequest(Entity<FHansaLogisticsRequestId>(1));
	TestEqual(TEXT("Disconnected road is preserved as the causal factor"),
		DisconnectedRequest->Bottleneck, EHansaLogisticsBottleneck::DisconnectedRoad);
	TestEqual(TEXT("Disconnected request dispatches no jobs"),
		Disconnected.CreateReadOnlyAccess(Definitions).BuildLogisticsJobProjection().Num(), 0);

	FHansaSimulationState Full = Require(FHansaSimulationState::TryCreate(MakeInitialization(false, true)));
	Cache.Discard();
	TestTrue(TEXT("Full-destination fixture advances"), Step(Full, Definitions, Cache));
	const auto FullRequest = Full.CreateReadOnlyAccess(Definitions)
		.QueryLogisticsRequest(Entity<FHansaLogisticsRequestId>(1));
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
		Jobs[0].RequestId, Entity<FHansaLogisticsRequestId>(2));
	TestEqual(TEXT("Lower-priority request reports fleet contention"),
		View.QueryLogisticsRequest(Entity<FHansaLogisticsRequestId>(1))->Bottleneck,
		EHansaLogisticsBottleneck::FleetCapacity);
	return true;
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
