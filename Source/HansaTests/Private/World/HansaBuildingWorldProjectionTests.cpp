#include "World/HansaBuildingWorldProjection.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaLubeckWorldFoundation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::WorldProjection
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue Require(THansaValueResult<TValue> Result)
	{
		check(Result.IsSuccess());
		return MoveTemp(Result.Value);
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

	FHansaBuildingWorldProjection MakeWorldProjection(
		const uint64 BuildingValue,
		const TCHAR* DefinitionId,
		const int32 X,
		const int64 Progress = 0)
	{
		FHansaBuildingWorldProjection Result;
		Result.BuildingId = Entity<FHansaBuildingId>(BuildingValue);
		Result.OwnerId = Entity<FHansaHouseId>(1);
		Result.Placement.CityId = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		Result.Placement.BuildingDefinitionId = Definition<FHansaBuildingTypeId>(DefinitionId);
		Result.Placement.Anchor = { X, 2 };
		Result.OccupiedCells.Add({ X, 2 });
		Result.ConstructionProgress = Require(FHansaRate::TryMakeNormalized(Progress));
		Result.Status = Progress >= FHansaRate::Scale
			? EHansaBuildingWorldStatus::Ready
			: EHansaBuildingWorldStatus::UnderConstruction;
		return Result;
	}

	FHansaSimulationDefinitionContext MakeDefinitions()
	{
		FHansaCompiledBuildingDefinition Road;
		Road.StableId = TEXT("Building.Road");
		Road.FootprintWidthCells = 1;
		Road.FootprintHeightCells = 1;
		FHansaCompiledBuildingDefinition Warehouse;
		Warehouse.StableId = TEXT("Building.Warehouse");
		Warehouse.FootprintWidthCells = 2;
		Warehouse.FootprintHeightCells = 1;
		FHansaEconomicRegistry Registry({}, {}, { Road, Warehouse }, 0x50303350524F4AULL);
		return Require(FHansaSimulationDefinitionContext::TryCreate(
			Definition<FHansaScenarioId>(TEXT("Scenario.Foundation")), Registry.GetRegistryHash(), MoveTemp(Registry)));
	}

	FHansaSimulationProjection MakeSimulationProjection(const bool bIncludeWarehouse, const int64 WarehouseProgress)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Require(FHansaSimulationClock::TryCreate(
			Require(FHansaSimulationVersion::TryCreate(1)), Require(FHansaSimulationTick::TryCreate(0))));
		Initialization.CampaignSeed = 50303;
		const FHansaHouseId House = Entity<FHansaHouseId>(1);
		const FHansaCityDefinitionId City = Definition<FHansaCityDefinitionId>(TEXT("City.Lubeck"));
		const FHansaBuildingTypeId RoadDefinition = Definition<FHansaBuildingTypeId>(TEXT("Building.Road"));
		const FHansaBuildingTypeId WarehouseDefinition = Definition<FHansaBuildingTypeId>(TEXT("Building.Warehouse"));
		Initialization.Houses.Add({ House, FHansaMoney::FromRaw(10'000) });
		Initialization.Cities.Add({ City, FHansaQuantity() });

		FHansaPlacementMapInitialization Map;
		Map.CityId = City;
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { 9, 9 };
		Map.RoadBuildingDefinitionId = RoadDefinition;
		for (int32 X = 0; X < 10; ++X)
		{
			for (int32 Y = 0; Y < 10; ++Y)
			{
				Map.Cells.Add({ { X, Y }, EHansaPlacementTerrain::Land, House, false });
			}
		}
		Initialization.Placement.Maps.Add(MoveTemp(Map));
		Initialization.Placement.Entitlements.Add({ House, RoadDefinition });
		Initialization.Placement.Entitlements.Add({ House, WarehouseDefinition });

		const FHansaBuildingId RoadId = Entity<FHansaBuildingId>(2);
		Initialization.Buildings.Add({ RoadId, RoadDefinition, House,
			Require(FHansaRate::TryMakeNormalized(FHansaRate::Scale)) });
		Initialization.Placement.Placements.Add({
			RoadId,
			House,
			{ City, RoadDefinition, { 6, 4 }, EHansaGridRotation::North },
			{ { 6, 4 } }
		});

		if (bIncludeWarehouse)
		{
			const FHansaBuildingId WarehouseId = Entity<FHansaBuildingId>(1);
			Initialization.Buildings.Add({ WarehouseId, WarehouseDefinition, House,
				Require(FHansaRate::TryMakeNormalized(WarehouseProgress)) });
			Initialization.Placement.Placements.Add({
				WarehouseId,
				House,
				{ City, WarehouseDefinition, { 2, 3 }, EHansaGridRotation::East },
				{ { 2, 3 }, { 2, 4 } }
			});
		}

		FHansaSimulationState State = Require(FHansaSimulationState::TryCreate(MoveTemp(Initialization)));
		return Require(State.CreateReadOnlyAccess(MakeDefinitions()).BuildProjection());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementProjectionRegistryTest,
	"Hansa.UI.World.PlacementProjectionRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementProjectionRegistryTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Game;
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::WorldProjection;

	FHansaPlacementProjectionRegistry Registry;
	FHansaPlacementProjectionDelta Delta;
	FHansaBuildingWorldProjection Warehouse = MakeWorldProjection(1, TEXT("Building.Warehouse"), 2);
	FHansaBuildingWorldProjection Road = MakeWorldProjection(2, TEXT("Building.Road"), 6, FHansaRate::Scale);
	TestTrue(TEXT("Discovery order is accepted"), Registry.Reconcile({ Road, Warehouse }, Delta));
	TestEqual(TEXT("Both world projections are created"), Delta.Created.Num(), 2);
	TestEqual(TEXT("Created IDs are canonical"), Delta.Created[0].GetValue(), uint64(1));
	TestEqual(TEXT("Registry owns one stable entry per entity"), Registry.Num(), 2);

	TestTrue(TEXT("An identical projection is a no-op"), Registry.Reconcile({ Warehouse, Road }, Delta));
	TestTrue(TEXT("No-op reconciliation emits no changes"),
		Delta.Created.IsEmpty() && Delta.Updated.IsEmpty() && Delta.Removed.IsEmpty());

	Warehouse.ConstructionProgress = Require(FHansaRate::TryMakeNormalized(FHansaRate::Scale));
	Warehouse.Status = EHansaBuildingWorldStatus::Ready;
	TestTrue(TEXT("A status change reconciles"), Registry.Reconcile({ Warehouse, Road }, Delta));
	TestEqual(TEXT("Only the changed entity updates"), Delta.Updated.Num(), 1);
	TestEqual(TEXT("Stable entity mapping survives updates"), Delta.Updated[0].GetValue(), uint64(1));

	TestTrue(TEXT("Missing projections are removed"), Registry.Reconcile({ Road }, Delta));
	TestEqual(TEXT("The absent building is removed"), Delta.Removed.Num(), 1);
	TestEqual(TEXT("Road remains mapped"), Registry.Num(), 1);

	TestFalse(TEXT("Duplicate entity projections are rejected transactionally"), Registry.Reconcile({ Road, Road }, Delta));
	TestEqual(TEXT("Rejected reconciliation preserves prior mapping"), Registry.Num(), 1);

	Registry.Reset();
	TestTrue(TEXT("Map reload reconstructs from authoritative projection"), Registry.Reconcile({ Road, Warehouse }, Delta));
	TestEqual(TEXT("Reload reconstructs every entity"), Registry.Num(), 2);
	TestEqual(TEXT("Reload creation remains canonical"), Registry.GetCanonicalIds()[0].GetValue(), uint64(1));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementProjectionActorLifecycleTest,
	"Hansa.UI.World.PlacementProjectionActorLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementProjectionActorLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::WorldProjection;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaPlacementProjectionTestWorld"));
	TestNotNull(TEXT("Transient projection test world is created"), World);
	if (World == nullptr)
	{
		return false;
	}
	AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	AHansaPlacementProjectionManager* Manager = World->SpawnActor<AHansaPlacementProjectionManager>();
	TestNotNull(TEXT("World foundation spawns"), Foundation);
	TestNotNull(TEXT("Projection manager spawns"), Manager);
	if (Foundation == nullptr || Manager == nullptr)
	{
		World->DestroyWorld(false);
		return false;
	}

	const FHansaSimulationProjection Initial = MakeSimulationProjection(true, 0);
	TestTrue(TEXT("Initial projection creates managed Actors"), Manager->Synchronize(Initial, *Foundation));
	TestEqual(TEXT("One Actor exists per stable building ID"), Manager->GetProjectionCount(), 2);
	AHansaBuildingWorldProjectionActor* Warehouse =
		Manager->FindProjectionActor(Entity<FHansaBuildingId>(1));
	AHansaBuildingWorldProjectionActor* Road = Manager->FindProjectionActor(Entity<FHansaBuildingId>(2));
	TestNotNull(TEXT("Warehouse mapping is queryable by stable entity ID"), Warehouse);
	TestNotNull(TEXT("Road mapping is queryable by stable entity ID"), Road);
	if (Warehouse != nullptr && Road != nullptr)
	{
		TestTrue(TEXT("Incomplete buildings use a construction placeholder"),
			Warehouse->GetWorldStatus() == EHansaBuildingWorldStatus::UnderConstruction &&
			Warehouse->ConstructionPlaceholder->IsVisible() && !Warehouse->BuildingMesh->IsVisible());
		TestTrue(TEXT("Road projections use the road visual kind"), Road->IsRoad());
		Manager->SelectBuilding(Warehouse->GetBuildingId());
		TestTrue(TEXT("Selection is presentation-only and shows a native outline"),
			Warehouse->IsSelected() && Warehouse->SelectionOutline->IsVisible());
		FHansaBuildingWorldProjection Blocked = MakeWorldProjection(
			1, TEXT("Building.Warehouse"), 2, FHansaRate::Scale);
		Blocked.Status = EHansaBuildingWorldStatus::Blocked;
		Blocked.ProductionBlocker = EHansaProductionBlocker::MissingInput;
		Blocked.FootprintWidthCells = 2;
		Warehouse->ApplyProjection(Blocked, *Foundation);
		TestTrue(TEXT("Blocked status uses a visible shape marker and stable typed name"),
			Warehouse->StatusMarker->IsVisible() && Warehouse->GetStatusName() == TEXT("Blocked"));
	}

	const FHansaSimulationProjection Updated = MakeSimulationProjection(true, FHansaRate::Scale);
	TestTrue(TEXT("Projection updates reuse the stable Actor"), Manager->Synchronize(Updated, *Foundation));
	TestTrue(TEXT("Completed construction swaps to the building mesh"),
		Warehouse == Manager->FindProjectionActor(Entity<FHansaBuildingId>(1)) &&
		Warehouse->GetWorldStatus() == EHansaBuildingWorldStatus::Ready && Warehouse->BuildingMesh->IsVisible());

	const FHansaSimulationProjection Removed = MakeSimulationProjection(false, 0);
	TestTrue(TEXT("Projection removal tears down only the missing Actor"), Manager->Synchronize(Removed, *Foundation));
	TestEqual(TEXT("One projected road remains"), Manager->GetProjectionCount(), 1);
	TestNull(TEXT("Removed stable ID no longer resolves"), Manager->FindProjectionActor(Entity<FHansaBuildingId>(1)));

	AHansaLubeckWorldFoundation* ReloadedFoundation = World->SpawnActor<AHansaLubeckWorldFoundation>(
		AHansaLubeckWorldFoundation::StaticClass(), FTransform(FVector(1000.0, 500.0, 0.0)));
	TestNotNull(TEXT("Reloaded map foundation spawns"), ReloadedFoundation);
	if (ReloadedFoundation != nullptr)
	{
		TestTrue(TEXT("Map reload rebuilds from the authoritative projection"),
			Manager->RebuildFromProjection(Initial, *ReloadedFoundation));
		TestEqual(TEXT("Reload reconstruction restores every projection"), Manager->GetProjectionCount(), 2);
		AHansaBuildingWorldProjectionActor* ReloadedWarehouse =
			Manager->FindProjectionActor(Entity<FHansaBuildingId>(1));
		TestTrue(TEXT("Rebuilt Actors bind to the new foundation"),
			ReloadedWarehouse != nullptr && ReloadedWarehouse->GetAttachParentActor() == ReloadedFoundation);
	}

	Manager->TearDownProjections();
	TestEqual(TEXT("Explicit teardown releases every managed mapping"), Manager->GetProjectionCount(), 0);
	World->DestroyWorld(false);
	return !HasAnyErrors();
}

#endif
