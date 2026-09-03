#include "Commands/HansaGameplayCommandGateway.h"
#include "Algo/Reverse.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Diagnostics/HansaStateHash.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Placement/HansaPlacement.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Placement
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue RequireValue(const THansaValueResult<TValue>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename TId>
	TId EntityId(const uint64 Value)
	{
		return RequireValue(TId::TryCreate(Value));
	}

	template <typename TId>
	TId DefinitionId(const TCHAR* Value)
	{
		return RequireValue(TId::TryParse(Value));
	}

	FHansaEconomicRegistry MakeRegistry()
	{
		FHansaCompiledBuildingDefinition Road;
		Road.StableId = TEXT("Building.Road");
		Road.FootprintWidthCells = 1;
		Road.FootprintHeightCells = 1;
		Road.BuildTicks = 1;

		FHansaCompiledBuildingDefinition Fishery;
		Fishery.StableId = TEXT("Building.Fishery");
		Fishery.FootprintWidthCells = 2;
		Fishery.FootprintHeightCells = 1;
		Fishery.BuildTicks = 10;
		Fishery.bRequiresShoreline = true;

		FHansaCompiledBuildingDefinition Residence;
		Residence.StableId = TEXT("Building.Residence.Laborer");
		Residence.FootprintWidthCells = 2;
		Residence.FootprintHeightCells = 3;
		Residence.BuildTicks = 20;
		Residence.bRequiresRoad = true;

		FHansaCompiledBuildingDefinition Warehouse;
		Warehouse.StableId = TEXT("Building.Warehouse");
		Warehouse.FootprintWidthCells = 2;
		Warehouse.FootprintHeightCells = 2;
		Warehouse.BuildTicks = 30;

		return FHansaEconomicRegistry({}, {}, { Fishery, Residence, Road, Warehouse }, 0x50534C4143454D54ULL);
	}

	FHansaSimulationDefinitionContext MakeDefinitions()
	{
		return RequireValue(FHansaSimulationDefinitionContext::TryCreate(
			DefinitionId<FHansaScenarioId>(TEXT("Scenario.PlacementTest")),
			0x50534C4143454D54ULL,
			MakeRegistry()));
	}

	FHansaPlacementInitialization MakePlacementInitialization(TArray<FHansaPlacedBuildingRecord> Placements = {})
	{
		const FHansaHouseId HouseOne = EntityId<FHansaHouseId>(1);
		const FHansaHouseId HouseTwo = EntityId<FHansaHouseId>(2);
		FHansaPlacementMapInitialization Map;
		Map.CityId = DefinitionId<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { 7, 7 };
		Map.RoadBuildingDefinitionId = DefinitionId<FHansaBuildingTypeId>(TEXT("Building.Road"));
		for (int32 X = 0; X <= 7; ++X)
		{
			for (int32 Y = 0; Y <= 7; ++Y)
			{
				FHansaPlacementGridCell Cell;
				Cell.Coordinate = { X, Y };
				Cell.Terrain = X == 7 ? EHansaPlacementTerrain::Water :
					(Y == 0 ? EHansaPlacementTerrain::Shore : EHansaPlacementTerrain::Land);
				Cell.OwnerId = X == 6 ? HouseTwo : HouseOne;
				Cell.bBlocked = X == 4 && Y == 4;
				Map.Cells.Add(Cell);
			}
		}

		FHansaPlacementInitialization Result;
		Result.Maps = { MoveTemp(Map) };
		for (const TCHAR* Id : {
			TEXT("Building.Road"),
			TEXT("Building.Fishery"),
			TEXT("Building.Residence.Laborer"),
			TEXT("Building.Warehouse") })
		{
			Result.Entitlements.Add({ HouseOne, DefinitionId<FHansaBuildingTypeId>(Id) });
		}
		Result.Entitlements.Add({ HouseTwo, DefinitionId<FHansaBuildingTypeId>(TEXT("Building.Road")) });
		Result.Placements = MoveTemp(Placements);
		return Result;
	}

	FHansaSimulationState MakeState(TArray<FHansaPlacedBuildingRecord> Placements = {})
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = RequireValue(FHansaSimulationClock::TryCreate(
			RequireValue(FHansaSimulationVersion::TryCreate(1)),
			RequireValue(FHansaSimulationTick::TryCreate(0))));
		Initialization.CampaignSeed = 0x4C554245434BULL;
		Initialization.Houses = {
			{ EntityId<FHansaHouseId>(1), FHansaMoney::FromRaw(100'000) },
			{ EntityId<FHansaHouseId>(2), FHansaMoney::FromRaw(100'000) }
		};
		Initialization.Cities = {
			{ DefinitionId<FHansaCityDefinitionId>(TEXT("City.Lubeck")), FHansaQuantity() }
		};
		for (const FHansaPlacedBuildingRecord& Placement : Placements)
		{
			Initialization.Buildings.Add({
				Placement.BuildingId,
				Placement.Spec.BuildingDefinitionId,
				Placement.OwnerId,
				FHansaRate()
			});
		}
		Initialization.Placement = MakePlacementInitialization(MoveTemp(Placements));
		return RequireValue(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
	}

	FHansaPlacementSpec Spec(
		const TCHAR* Definition,
		const int32 X,
		const int32 Y,
		const EHansaGridRotation Rotation = EHansaGridRotation::North)
	{
		FHansaPlacementSpec Result;
		Result.CityId = DefinitionId<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Result.BuildingDefinitionId = DefinitionId<FHansaBuildingTypeId>(Definition);
		Result.Anchor = { X, Y };
		Result.Rotation = Rotation;
		return Result;
	}

	FHansaCommandHeader Header(const uint64 Id, const uint64 Sequence, const int64 Tick, const uint64 House = 1)
	{
		FHansaCommandHeader Result;
		Result.CommandId = EntityId<FHansaCommandId>(Id);
		Result.Authority.IssuingHouseId = EntityId<FHansaHouseId>(House);
		Result.Authority.PrincipalId = House * 100 + 7;
		Result.Authority.Origin = EHansaCommandOrigin::PlayerInput;
		Result.RequestedExecutionTick = RequireValue(FHansaSimulationTick::TryCreate(Tick));
		Result.GlobalSequence = Sequence;
		return Result;
	}

	FHansaGameplayCommand Place(
		const uint64 CommandId,
		const uint64 Sequence,
		const int64 Tick,
		const uint64 BuildingId,
		const FHansaPlacementSpec& Placement,
		const uint64 House = 1)
	{
		return FHansaGameplayCommand::Create(
			Header(CommandId, Sequence, Tick, House),
			FHansaPlaceBuildingCommand { EntityId<FHansaBuildingId>(BuildingId), Placement });
	}

	bool HasReason(const FHansaPlacementValidationResult& Result, const EHansaPlacementFailure Failure)
	{
		return Result.GetReasons().ContainsByPredicate([Failure](const FHansaPlacementValidationReason& Reason)
		{
			return Reason.Failure == Failure;
		});
	}

	FHansaPlacedBuildingRecord RoadRecord(const uint64 BuildingId, const int32 X, const int32 Y)
	{
		FHansaPlacedBuildingRecord Result;
		Result.BuildingId = EntityId<FHansaBuildingId>(BuildingId);
		Result.OwnerId = EntityId<FHansaHouseId>(1);
		Result.Spec = Spec(TEXT("Building.Road"), X, Y);
		Result.OccupiedCells = { { X, Y } };
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementRotationBoundsTest,
	"Hansa.Simulation.Placement.RotationAndBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementRotationBoundsTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Placement;

	const FHansaSimulationState State = MakeState();
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
	const FHansaPlacementValidationResult North = View.ValidatePlacement(
		EntityId<FHansaHouseId>(1), Spec(TEXT("Building.Residence.Laborer"), 1, 2));
	const FHansaPlacementValidationResult East = View.ValidatePlacement(
		EntityId<FHansaHouseId>(1), Spec(TEXT("Building.Residence.Laborer"), 1, 2, EHansaGridRotation::East));
	TestEqual(TEXT("North rotation uses the authored 2x3 footprint"), North.GetOccupiedCells().Num(), 6);
	TestEqual(TEXT("East rotation preserves footprint area"), East.GetOccupiedCells().Num(), 6);
	TestTrue(TEXT("North reaches its third row"),
		North.GetOccupiedCells().Contains(FHansaGridCoordinate { 2, 4 }));
	TestTrue(TEXT("East swaps width and height"),
		East.GetOccupiedCells().Contains(FHansaGridCoordinate { 3, 3 }));

	const FHansaPlacementValidationResult Boundary = View.ValidatePlacement(
		EntityId<FHansaHouseId>(1), Spec(TEXT("Building.Residence.Laborer"), 6, 6, EHansaGridRotation::East));
	TestTrue(TEXT("A rotated footprint crossing the grid is rejected structurally"),
		HasReason(Boundary, EHansaPlacementFailure::OutsideBounds));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementValidationTest,
	"Hansa.Simulation.Placement.StructuredValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementValidationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Placement;

	const FHansaSimulationState State = MakeState();
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
	const FHansaHouseId HouseOne = EntityId<FHansaHouseId>(1);
	const FHansaHouseId HouseTwo = EntityId<FHansaHouseId>(2);

	const FHansaPlacementValidationResult RoadRequired = View.ValidatePlacement(
		HouseOne, Spec(TEXT("Building.Residence.Laborer"), 1, 2));
	TestTrue(TEXT("Road-dependent buildings identify missing adjacency"),
		HasReason(RoadRequired, EHansaPlacementFailure::RoadRequired));
	if (!RoadRequired.GetReasons().IsEmpty())
	{
		TestTrue(TEXT("Validation exposes localization-ready cause keys"),
			!RoadRequired.GetReasons()[0].MessageKey.IsNone());
		TestTrue(TEXT("Validation exposes localization-ready remedy keys"),
			!RoadRequired.GetReasons()[0].RemedyKey.IsNone());
	}

	TestTrue(TEXT("Fishery away from shore reports its terrain prerequisite"),
		HasReason(View.ValidatePlacement(HouseOne, Spec(TEXT("Building.Fishery"), 1, 2)),
			EHansaPlacementFailure::ShorelineRequired));
	TestTrue(TEXT("Fishery on the authored shore is valid"),
		View.ValidatePlacement(HouseOne, Spec(TEXT("Building.Fishery"), 1, 0)).CanPlace());
	TestTrue(TEXT("Water is not buildable terrain"),
		HasReason(View.ValidatePlacement(HouseOne, Spec(TEXT("Building.Road"), 7, 3)),
			EHansaPlacementFailure::TerrainNotBuildable));
	TestTrue(TEXT("Authored collision blockers reject placement"),
		HasReason(View.ValidatePlacement(HouseOne, Spec(TEXT("Building.Warehouse"), 4, 4)),
			EHansaPlacementFailure::CellBlocked));
	TestTrue(TEXT("Every footprint cell enforces land ownership"),
		HasReason(View.ValidatePlacement(HouseOne, Spec(TEXT("Building.Warehouse"), 5, 2)),
			EHansaPlacementFailure::WrongOwner));
	TestTrue(TEXT("Locked building types report a prerequisite failure"),
		HasReason(View.ValidatePlacement(HouseTwo, Spec(TEXT("Building.Warehouse"), 6, 2)),
			EHansaPlacementFailure::MissingPrerequisite));
	TestTrue(TEXT("Unknown compiled definitions fail closed"),
		HasReason(View.ValidatePlacement(HouseOne, Spec(TEXT("Building.Missing"), 1, 1)),
			EHansaPlacementFailure::UnknownBuildingDefinition));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementCommandTest,
	"Hansa.Simulation.Placement.CommandOccupancyAndRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementCommandTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Placement;

	FHansaSimulationState State = MakeState();
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	TArray<FHansaGameplayCommand> RoadCommands = {
		Place(1, 1, 0, 101, Spec(TEXT("Building.Road"), 1, 1))
	};
	const FHansaCommandGatewayResult RoadResult =
		FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, RoadCommands, Cache);
	TestTrue(TEXT("Road confirmation uses the normal command gateway"), RoadResult.IsSuccess());

	TArray<FHansaGameplayCommand> ResidenceCommands = {
		Place(2, 2, 1, 102, Spec(TEXT("Building.Residence.Laborer"), 1, 2))
	};
	const FHansaCommandGatewayResult ResidenceResult =
		FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, ResidenceCommands, Cache);
	TestTrue(TEXT("A road-adjacent building commits"), ResidenceResult.IsSuccess());
	TestEqual(TEXT("Authoritative occupancy contains both placements"),
		State.CreateReadOnlyAccess(Definitions).GetPlacement().GetPlacements().Num(), 2);
	const FHansaSimulationProjection Projection =
		State.CreateReadOnlyAccess(Definitions).BuildProjection().Value;
	TestEqual(TEXT("Placement projection is available to preview/presentation code"),
		Projection.GetPlacedBuildingCount(), 2);
	TestEqual(TEXT("Every placement has an immutable world presentation projection"),
		Projection.GetBuildingWorldProjections().Num(), 2);
	TestTrue(TEXT("The one-tick road is ready after the next command tick"),
		Projection.GetBuildingWorldProjections()[0].Status == EHansaBuildingWorldStatus::Ready);
	TestTrue(TEXT("The newly placed residence projects its construction placeholder state"),
		Projection.GetBuildingWorldProjections()[1].Status == EHansaBuildingWorldStatus::UnderConstruction);
	TestEqual(TEXT("Placement then progress then completion events are ordered"), ResidenceResult.GetEvents().Num(), 3);
	if (ResidenceResult.GetEvents().Num() == 3)
	{
		TestTrue(TEXT("Placement event is published first"),
			ResidenceResult.GetEvents()[0].GetType() == EHansaDomainEventType::BuildingPlaced);
		TestTrue(TEXT("Progress event follows placement"),
			ResidenceResult.GetEvents()[1].GetType() == EHansaDomainEventType::ConstructionProgressed);
		TestTrue(TEXT("Completion event follows progress"),
			ResidenceResult.GetEvents()[2].GetType() == EHansaDomainEventType::ConstructionCompleted);
		TestTrue(TEXT("Event retains the authoritative anchor"),
			ResidenceResult.GetEvents()[0].GetPlacement().Anchor == (FHansaGridCoordinate { 1, 2 }));
	}

	const FHansaDeterminismFingerprint BeforeOverlap = State.CreateReadOnlyAccess(Definitions).GetFingerprint();
	TArray<FHansaGameplayCommand> OverlapCommands = {
		Place(3, 3, 2, 103, Spec(TEXT("Building.Road"), 1, 1))
	};
	const FHansaCommandGatewayResult Overlap =
		FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, OverlapCommands, Cache);
	TestTrue(TEXT("Overlap is a structured placement rejection"),
		Overlap.GetError() == EHansaCommandGatewayError::PlacementRejected);
	TestTrue(TEXT("Gateway returns the same occupancy reason used by preview"),
		Overlap.GetPlacementValidation().IsSet() &&
		HasReason(Overlap.GetPlacementValidation().GetValue(), EHansaPlacementFailure::Occupied));
	TestTrue(TEXT("Rejected placement leaves the entire transaction unchanged"),
		State.CreateReadOnlyAccess(Definitions).GetFingerprint() == BeforeOverlap);
	TestEqual(TEXT("Rejected placement does not advance the tick"),
		State.CreateReadOnlyAccess(Definitions).GetClock().GetTick().GetValue(), int64(2));
	TestNotNull(TEXT("Placement has its own normalized state-hash subsystem"),
		State.CreateReadOnlyAccess(Definitions).BuildStateHashReport().Find(EHansaStateHashSubsystem::Placement));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRoadDragSessionTest,
	"Hansa.Simulation.Placement.RoadDragCancelRepeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRoadDragSessionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Placement;

	FHansaPlacementSession Session;
	Session.SelectBuilding(
		DefinitionId<FHansaCityDefinitionId>(TEXT("City.Lubeck")),
		DefinitionId<FHansaBuildingTypeId>(TEXT("Building.Road")),
		true,
		true);
	Session.BeginRoadDrag({ 1, 1 });
	Session.UpdateRoadDrag({ 3, 3 });
	const TArray<FHansaPlacementSpec> RoadSpecs = Session.BuildConfirmationSpecs();
	TestEqual(TEXT("Road drag expands to every inclusive Manhattan cell"), RoadSpecs.Num(), 5);
	if (RoadSpecs.Num() == 5)
	{
		TestTrue(TEXT("Road drag uses a deterministic horizontal-first bend"),
			RoadSpecs[0].Anchor == (FHansaGridCoordinate { 1, 1 }) &&
			RoadSpecs[1].Anchor == (FHansaGridCoordinate { 2, 1 }) &&
			RoadSpecs[2].Anchor == (FHansaGridCoordinate { 3, 1 }) &&
			RoadSpecs[3].Anchor == (FHansaGridCoordinate { 3, 2 }) &&
			RoadSpecs[4].Anchor == (FHansaGridCoordinate { 3, 3 }));

		FHansaSimulationState State = MakeState();
		const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
		FHansaSimulationTransientCache Cache;
		TArray<FHansaGameplayCommand> Commands;
		for (int32 Index = 0; Index < RoadSpecs.Num(); ++Index)
		{
			Commands.Add(Place(
				static_cast<uint64>(Index + 1),
				static_cast<uint64>(Index + 1),
				0,
				static_cast<uint64>(100 + Index),
				RoadSpecs[Index]));
		}
		const FHansaCommandGatewayResult DragResult =
			FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
		TestTrue(TEXT("Road drag confirms as one transactional normal-command batch"), DragResult.IsSuccess());
		TestEqual(TEXT("Every confirmed drag cell becomes authoritative occupancy"),
			State.CreateReadOnlyAccess(Definitions).GetPlacement().GetPlacements().Num(), 5);
	}
	Session.OnConfirmationSucceeded();
	TestTrue(TEXT("Repeat mode keeps the road tool selected"), Session.IsActive());
	TestEqual(TEXT("A confirmed drag clears transient anchors"), Session.BuildConfirmationSpecs().Num(), 0);
	Session.Cancel();
	TestFalse(TEXT("Cancel exits placement mode"), Session.IsActive());

	Session.SelectBuilding(
		DefinitionId<FHansaCityDefinitionId>(TEXT("City.Lubeck")),
		DefinitionId<FHansaBuildingTypeId>(TEXT("Building.Warehouse")),
		false,
		false);
	Session.SetAnchor({ 2, 2 });
	Session.RotateClockwise();
	TestTrue(TEXT("Single-building rotation updates the confirmation intent"),
		Session.BuildConfirmationSpecs().Num() == 1 &&
		Session.BuildConfirmationSpecs()[0].Rotation == EHansaGridRotation::East);
	Session.OnConfirmationSucceeded();
	TestFalse(TEXT("Non-repeat confirmation exits placement mode"), Session.IsActive());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementCanonicalOrderTest,
	"Hansa.Simulation.Placement.CanonicalRestoreOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementCanonicalOrderTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Placement;

	TArray<FHansaPlacedBuildingRecord> Forward = {
		RoadRecord(100, 1, 1),
		RoadRecord(200, 3, 1)
	};
	TArray<FHansaPlacedBuildingRecord> Reversed = Forward;
	Algo::Reverse(Reversed);
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	const FHansaSimulationState First = MakeState(Forward);
	const FHansaSimulationState Second = MakeState(Reversed);
	const FHansaStateHashReport FirstHash = First.CreateReadOnlyAccess(Definitions).BuildStateHashReport();
	const FHansaStateHashReport SecondHash = Second.CreateReadOnlyAccess(Definitions).BuildStateHashReport();
	TestEqual(TEXT("Placement restore canonicalizes discovery order"),
		FirstHash.Find(EHansaStateHashSubsystem::Placement)->Value,
		SecondHash.Find(EHansaStateHashSubsystem::Placement)->Value);
	TestTrue(TEXT("Canonical placement order participates in the overall checksum"), FirstHash == SecondHash);
	return !HasAnyErrors();
}

#endif
