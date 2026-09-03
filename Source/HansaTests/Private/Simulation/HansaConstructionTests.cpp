#include "Commands/HansaGameplayCommandGateway.h"
#include "Construction/HansaConstruction.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Construction
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

	FHansaCompiledBuildingDefinition BuildingDefinition()
	{
		FHansaCompiledBuildingDefinition Building;
		Building.StableId = TEXT("Building.Warehouse");
		Building.ConstructionCosts = { { TEXT("Good.Timber"), 1000 } };
		Building.ConstructionCostPfennig = 100;
		Building.CancellationRefundBasisPoints = 5000;
		Building.FootprintWidthCells = 1;
		Building.FootprintHeightCells = 1;
		Building.BuildTicks = 3;
		return Building;
	}

	FHansaSimulationDefinitionContext Definitions()
	{
		FHansaCompiledGoodDefinition Timber;
		Timber.StableId = TEXT("Good.Timber");
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Definition<FHansaScenarioId>(TEXT("Scenario.ConstructionTest")),
			0x533036503031434FULL,
			FHansaEconomicRegistry({ Timber }, {}, { BuildingDefinition() }, 0x533036503031434FULL)));
	}

	FHansaPlacementInitialization Placement(const TArray<FHansaPlacedBuildingRecord>& Existing = {})
	{
		const FHansaHouseId HouseId = Entity<FHansaHouseId>(1);
		FHansaPlacementMapInitialization Map;
		Map.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { 3, 3 };
		Map.RoadBuildingDefinitionId = Definition<FHansaBuildingTypeId>(TEXT("Building.Warehouse"));
		for (int32 X = 0; X <= 3; ++X)
		{
			for (int32 Y = 0; Y <= 3; ++Y)
			{
				Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, HouseId, false });
			}
		}
		FHansaPlacementInitialization Result;
		Result.Maps = { MoveTemp(Map) };
		Result.Entitlements = { { HouseId, Definition<FHansaBuildingTypeId>(TEXT("Building.Warehouse")) } };
		Result.Placements = Existing;
		return Result;
	}

	FHansaSimulationState State(const int64 Money = 1000, const int64 Timber = 2000)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)),
			Require(FHansaSimulationTick::TryCreate(0))));
		Initialization.CampaignSeed = 0x434F4E5354525543ULL;
		Initialization.Houses = { { Entity<FHansaHouseId>(1), FHansaMoney::FromRaw(Money) } };
		Initialization.Cities = {
			{ Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck")), FHansaQuantity() }
		};
		Initialization.Placement = Placement();
		FHansaInventoryInitialization Inventory;
		Inventory.Id = Entity<FHansaInventoryId>(1);
		Inventory.OwnerKind = EHansaInventoryOwnerKind::City;
		Inventory.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Inventory.Capacity = FHansaQuantity::FromRaw(10'000);
		Inventory.AcceptedGoods = { Definition<FHansaGoodId>(TEXT("Good.Timber")) };
		Inventory.InitialStock = { { Definition<FHansaGoodId>(TEXT("Good.Timber")), FHansaQuantity::FromRaw(Timber) } };
		Initialization.Inventories = { MoveTemp(Inventory) };
		return Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	}

	FHansaPlacementSpec BuildSpec()
	{
		return {
			Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck")),
			Definition<FHansaBuildingTypeId>(TEXT("Building.Warehouse")),
			{ 1, 1 },
			EHansaGridRotation::North
		};
	}

	FHansaCommandHeader Header(const FHansaSimulationReadOnlyAccess& ReadOnly, const uint64 CommandId)
	{
		FHansaCommandHeader Result;
		Result.CommandId = Entity<FHansaCommandId>(CommandId);
		Result.Authority.IssuingHouseId = Entity<FHansaHouseId>(1);
		Result.Authority.PrincipalId = 7;
		Result.Authority.Origin = EHansaCommandOrigin::PlayerInput;
		Result.RequestedExecutionTick = ReadOnly.GetClock().GetTick();
		Result.GlobalSequence = ReadOnly.GetLastProcessedCommandSequence() + 1;
		return Result;
	}

	FHansaCommandGatewayResult Execute(
		FHansaSimulationState& StateValue,
		const FHansaSimulationDefinitionContext& DefinitionContext,
		FHansaSimulationTransientCache& Cache,
		const FHansaGameplayCommand& Command)
	{
		const TArray<FHansaGameplayCommand> Commands { Command };
		return FHansaGameplayCommandGateway::ExecuteTick(StateValue, DefinitionContext, Commands, Cache);
	}

	FHansaCommandGatewayResult Step(
		FHansaSimulationState& StateValue,
		const FHansaSimulationDefinitionContext& DefinitionContext,
		FHansaSimulationTransientCache& Cache)
	{
		return FHansaGameplayCommandGateway::ExecuteTick(StateValue, DefinitionContext, {}, Cache);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaConstructionCostAndProgressTest,
	"Hansa.Simulation.Construction.CostProgressCompletionAndSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaConstructionCostAndProgressTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Construction;

	FHansaSimulationState StateValue = State();
	const FHansaSimulationDefinitionContext DefinitionContext = Definitions();
	FHansaSimulationTransientCache Cache;
	const FHansaSimulationReadOnlyAccess Before = StateValue.CreateReadOnlyAccess(DefinitionContext);
	const FHansaConstructionCostProjection Preflight = Before.QueryConstructionCost(
		Entity<FHansaHouseId>(1), Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck")),
		Definition<FHansaBuildingTypeId>(TEXT("Building.Warehouse")));
	TestTrue(TEXT("Preflight reports an affordable typed currency/resource cost"), Preflight.IsAffordable());
	TestEqual(TEXT("Preflight exposes the currency unit"), Preflight.RequiredCurrency.GetRawValue(), int64(100));
	TestEqual(TEXT("Preflight exposes one resource"), Preflight.Resources.Num(), 1);

	const FHansaBuildingId BuildingId = Entity<FHansaBuildingId>(10);
	const FHansaCommandGatewayResult Placed = Execute(StateValue, DefinitionContext, Cache,
		FHansaGameplayCommand::Create(Header(Before, 1), { BuildingId, BuildSpec() }));
	TestTrue(TEXT("Affordable construction is placed through the normal gateway"), Placed.IsSuccess());
	const FHansaSimulationReadOnlyAccess AfterPlacement = StateValue.CreateReadOnlyAccess(DefinitionContext);
	TestEqual(TEXT("Currency is charged exactly once"), AfterPlacement.GetHouses()[0].Money.GetRawValue(), int64(900));
	const auto TimberStock = AfterPlacement.GetInventories().QueryStock(
		Entity<FHansaInventoryId>(1), Definition<FHansaGoodId>(TEXT("Good.Timber")));
	TestTrue(TEXT("Construction resource stock remains queryable"), TimberStock.IsSet());
	if (TimberStock.IsSet())
	{
		TestEqual(TEXT("Resources are charged exactly once"), TimberStock->Stock.GetRawValue(), int64(1000));
	}
	const auto Started = AfterPlacement.QueryConstruction(BuildingId);
	TestTrue(TEXT("Construction has an owning typed projection"), Started.IsSet());
	if (Started.IsSet())
	{
		TestTrue(TEXT("New work starts under construction"), Started->State == EHansaConstructionState::UnderConstruction);
		TestEqual(TEXT("Placement does not grant a free work tick"), Started->ElapsedTicks, 0);
	}

	const FHansaCommandGatewayResult FirstWork = Step(StateValue, DefinitionContext, Cache);
	TestTrue(TEXT("The first post-placement tick advances construction"), FirstWork.IsSuccess());
	TestTrue(TEXT("Progress emits an ordered typed event"), FirstWork.GetEvents().ContainsByPredicate([](const FHansaDomainEvent& Event)
	{
		return Event.GetType() == EHansaDomainEventType::ConstructionProgressed;
	}));
	Step(StateValue, DefinitionContext, Cache);
	const FHansaCommandGatewayResult Completed = Step(StateValue, DefinitionContext, Cache);
	TestTrue(TEXT("The exact build-time boundary completes"), Completed.GetEvents().ContainsByPredicate([](const FHansaDomainEvent& Event)
	{
		return Event.GetType() == EHansaDomainEventType::ConstructionCompleted;
	}));
	const auto Finished = StateValue.CreateReadOnlyAccess(DefinitionContext).QueryConstruction(BuildingId);
	TestTrue(TEXT("Completed state is projected"), Finished.IsSet() && Finished->State == EHansaConstructionState::Completed);
	if (Finished.IsSet())
	{
		TestEqual(TEXT("Completion progress is exact"), Finished->Progress.GetPartsPerMillion(), FHansaRate::Scale);
	}
	const FHansaSimulationSnapshot Snapshot = StateValue.CreateReadOnlyAccess(DefinitionContext).CaptureSnapshot();
	TestEqual(TEXT("Save-ready snapshot owns the construction record"), Snapshot.GetBuildings().Num(), 1);
	if (Snapshot.GetBuildings().Num() == 1)
	{
		TestTrue(TEXT("Snapshot preserves lifecycle state"),
			Snapshot.GetBuildings()[0].ConstructionState == EHansaConstructionState::Completed);
		TestEqual(TEXT("Snapshot preserves elapsed build ticks"),
			Snapshot.GetBuildings()[0].ConstructionElapsedTicks, 3);
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaConstructionCancellationBoundaryTest,
	"Hansa.Simulation.Construction.CancelRefundRemovalAndRejectionBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaConstructionCancellationBoundaryTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Construction;

	const FHansaSimulationDefinitionContext DefinitionContext = Definitions();
	FHansaSimulationState StateValue = State();
	FHansaSimulationTransientCache Cache;
	const FHansaBuildingId BuildingId = Entity<FHansaBuildingId>(10);
	FHansaCommandGatewayResult Result = Execute(StateValue, DefinitionContext, Cache,
		FHansaGameplayCommand::Create(Header(StateValue.CreateReadOnlyAccess(DefinitionContext), 1),
			FHansaPlaceBuildingCommand { BuildingId, BuildSpec() }));
	TestTrue(TEXT("Construction starts"), Result.IsSuccess());
	Step(StateValue, DefinitionContext, Cache);
	Result = Execute(StateValue, DefinitionContext, Cache,
		FHansaGameplayCommand::Create(Header(StateValue.CreateReadOnlyAccess(DefinitionContext), 2),
			FHansaCancelConstructionCommand { BuildingId }));
	TestTrue(TEXT("Cancellation is accepted before the completion boundary"), Result.IsSuccess());
	TestTrue(TEXT("Cancellation emits its typed event"), Result.GetEvents().Num() == 1 &&
		Result.GetEvents()[0].GetType() == EHansaDomainEventType::ConstructionCancelled);
	const FHansaSimulationReadOnlyAccess Cancelled = StateValue.CreateReadOnlyAccess(DefinitionContext);
	TestEqual(TEXT("Bounded currency refund cannot exceed the payment"), Cancelled.GetHouses()[0].Money.GetRawValue(), int64(950));
	const auto RefundStock = Cancelled.GetInventories().QueryStock(
		Entity<FHansaInventoryId>(1), Definition<FHansaGoodId>(TEXT("Good.Timber")));
	TestTrue(TEXT("Resource refund remains conserved"), RefundStock.IsSet() && RefundStock->Stock.GetRawValue() == 1500);
	TestEqual(TEXT("Cancelled site releases authoritative occupancy"), Cancelled.GetPlacement().GetPlacements().Num(), 0);

	const FHansaDeterminismFingerprint BeforeDuplicate = Cancelled.GetFingerprint();
	Result = Execute(StateValue, DefinitionContext, Cache,
		FHansaGameplayCommand::Create(Header(Cancelled, 3), FHansaCancelConstructionCommand { BuildingId }));
	TestTrue(TEXT("Duplicate cancellation is rejected"), Result.GetError() == EHansaCommandGatewayError::TargetNotFound);
	TestTrue(TEXT("Rejected duplicate cannot mint another refund"),
		StateValue.CreateReadOnlyAccess(DefinitionContext).GetFingerprint() == BeforeDuplicate);

	FHansaSimulationState CompleteState = State();
	FHansaSimulationTransientCache CompleteCache;
	Execute(CompleteState, DefinitionContext, CompleteCache,
		FHansaGameplayCommand::Create(Header(CompleteState.CreateReadOnlyAccess(DefinitionContext), 1),
			FHansaPlaceBuildingCommand { BuildingId, BuildSpec() }));
	Step(CompleteState, DefinitionContext, CompleteCache);
	Step(CompleteState, DefinitionContext, CompleteCache);
	Step(CompleteState, DefinitionContext, CompleteCache);
	const FHansaSimulationReadOnlyAccess CompleteView = CompleteState.CreateReadOnlyAccess(DefinitionContext);
	Result = Execute(CompleteState, DefinitionContext, CompleteCache,
		FHansaGameplayCommand::Create(Header(CompleteView, 2), FHansaCancelConstructionCommand { BuildingId }));
	TestTrue(TEXT("Cancellation at/after completion is rejected"),
		Result.GetError() == EHansaCommandGatewayError::ConstructionStateInvalid);
	Result = Execute(CompleteState, DefinitionContext, CompleteCache,
		FHansaGameplayCommand::Create(Header(CompleteView, 2), FHansaRemoveBuildingCommand { BuildingId }));
	TestTrue(TEXT("Completed construction uses the explicit safe-removal command"), Result.IsSuccess());
	TestEqual(TEXT("Removal releases the placement"),
		CompleteState.CreateReadOnlyAccess(DefinitionContext).GetPlacement().GetPlacements().Num(), 0);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaConstructionMissingCostTest,
	"Hansa.Simulation.Construction.MissingCostAtomicRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaConstructionMissingCostTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Construction;

	FHansaSimulationState StateValue = State(50, 400);
	const FHansaSimulationDefinitionContext DefinitionContext = Definitions();
	FHansaSimulationTransientCache Cache;
	const FHansaSimulationReadOnlyAccess Before = StateValue.CreateReadOnlyAccess(DefinitionContext);
	const FHansaDeterminismFingerprint BeforeHash = Before.GetFingerprint();
	const FHansaCommandGatewayResult Result = Execute(StateValue, DefinitionContext, Cache,
		FHansaGameplayCommand::Create(Header(Before, 1),
			FHansaPlaceBuildingCommand { Entity<FHansaBuildingId>(10), BuildSpec() }));
	TestTrue(TEXT("Missing construction costs return a stable rejection"),
		Result.GetError() == EHansaCommandGatewayError::ConstructionCostUnavailable);
	TestTrue(TEXT("Rejection includes a typed missing-cost projection"), Result.GetConstructionCost().IsSet());
	if (Result.GetConstructionCost().IsSet())
	{
		const FHansaConstructionCostProjection& Cost = Result.GetConstructionCost().GetValue();
		TestEqual(TEXT("Missing currency is exact"), Cost.MissingCurrency.GetRawValue(), int64(50));
		TestEqual(TEXT("Missing resource is exact"), Cost.Resources[0].Missing.GetRawValue(), int64(600));
	}
	TestTrue(TEXT("Rejected placement is fully transactional"),
		StateValue.CreateReadOnlyAccess(DefinitionContext).GetFingerprint() == BeforeHash);
	TestEqual(TEXT("Rejected placement emits no events"), Result.GetEvents().Num(), 0);
	return !HasAnyErrors();
}

#endif
