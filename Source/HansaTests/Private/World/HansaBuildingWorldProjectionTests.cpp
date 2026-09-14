#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaBakeryPresentation.h"
#include "World/HansaHarborPresentation.h"
#include "World/HansaResidencePresentation.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#if WITH_EDITOR
#include "StaticMeshAttributes.h"
#endif
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "UI/HansaBuildMenuPresentationModel.h"

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
	TId BuildingWorldProjectionTestsEntity(const uint64 Value)
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
		Result.BuildingId = BuildingWorldProjectionTestsEntity<FHansaBuildingId>(BuildingValue);
		Result.OwnerId = BuildingWorldProjectionTestsEntity<FHansaHouseId>(1);
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
		const FHansaHouseId House = BuildingWorldProjectionTestsEntity<FHansaHouseId>(1);
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

		const FHansaBuildingId RoadId = BuildingWorldProjectionTestsEntity<FHansaBuildingId>(2);
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
			const FHansaBuildingId WarehouseId = BuildingWorldProjectionTestsEntity<FHansaBuildingId>(1);
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
		Manager->FindProjectionActor(BuildingWorldProjectionTestsEntity<FHansaBuildingId>(1));
	AHansaBuildingWorldProjectionActor* Road = Manager->FindProjectionActor(BuildingWorldProjectionTestsEntity<FHansaBuildingId>(2));
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
		TestEqual(TEXT("Selection uses four L-shaped footprint brackets"),
			Warehouse->SelectionCornerSegments.Num(), 8);
		TestFalse(TEXT("Every footprint bracket segment is visible while selected"),
			Warehouse->SelectionCornerSegments.ContainsByPredicate(
				[](const UStaticMeshComponent* Segment) { return Segment == nullptr || !Segment->IsVisible(); }));
		TestTrue(TEXT("Selected visible geometry receives a mesh-hugging contour"),
			!Warehouse->SelectionContourMeshes.IsEmpty() &&
			Warehouse->SelectionContourMeshes.Num() == Warehouse->SelectionHaloMeshes.Num());
		TestTrue(TEXT("Selected geometry is custom-depth compatible"),
			Warehouse->ConstructionPlaceholder->bRenderCustomDepth);
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
		Warehouse == Manager->FindProjectionActor(BuildingWorldProjectionTestsEntity<FHansaBuildingId>(1)) &&
		Warehouse->GetWorldStatus() == EHansaBuildingWorldStatus::Ready && Warehouse->BuildingMesh->IsVisible());

	const FHansaSimulationProjection Removed = MakeSimulationProjection(false, 0);
	TestTrue(TEXT("Projection removal tears down only the missing Actor"), Manager->Synchronize(Removed, *Foundation));
	TestEqual(TEXT("One projected road remains"), Manager->GetProjectionCount(), 1);
	TestNull(TEXT("Removed stable ID no longer resolves"), Manager->FindProjectionActor(BuildingWorldProjectionTestsEntity<FHansaBuildingId>(1)));

	AHansaLubeckWorldFoundation* ReloadedFoundation = World->SpawnActor<AHansaLubeckWorldFoundation>(
		AHansaLubeckWorldFoundation::StaticClass(), FTransform(FVector(1000.0, 500.0, 0.0)));
	TestNotNull(TEXT("Reloaded map foundation spawns"), ReloadedFoundation);
	if (ReloadedFoundation != nullptr)
	{
		TestTrue(TEXT("Map reload rebuilds from the authoritative projection"),
			Manager->RebuildFromProjection(Initial, *ReloadedFoundation));
		TestEqual(TEXT("Reload reconstruction restores every projection"), Manager->GetProjectionCount(), 2);
		AHansaBuildingWorldProjectionActor* ReloadedWarehouse =
			Manager->FindProjectionActor(BuildingWorldProjectionTestsEntity<FHansaBuildingId>(1));
		TestTrue(TEXT("Rebuilt Actors bind to the new foundation"),
			ReloadedWarehouse != nullptr && ReloadedWarehouse->GetAttachParentActor() == ReloadedFoundation);
	}

	Manager->TearDownProjections();
	TestEqual(TEXT("Explicit teardown releases every managed mapping"), Manager->GetProjectionCount(), 0);
	World->DestroyWorld(false);
	return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLaborerResidenceApprovedMeshTest,
	"Hansa.UI.World.LaborerResidenceUsesApprovedMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLaborerResidenceApprovedMeshTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::WorldProjection;
	(void)Parameters;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaLaborerResidenceMeshTestWorld"));
	if (!TestNotNull(TEXT("Transient laborer-residence test world is created"), World))
	{
		return false;
	}
	AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	AHansaBuildingWorldProjectionActor* Residence = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
	if (!TestNotNull(TEXT("Laborer residence projection prerequisites spawn"), Foundation) ||
		!TestNotNull(TEXT("Laborer residence projection Actor spawns"), Residence))
	{
		World->DestroyWorld(false);
		return false;
	}

	FHansaBuildingWorldProjection Projection = MakeWorldProjection(
		3, TEXT("Building.Residence.Laborer"), 4, FHansaRate::Scale);
	Projection.FootprintWidthCells = 2;
	Projection.FootprintHeightCells = 2;
	Projection.OccupiedCells = { { 4, 2 }, { 4, 3 }, { 5, 2 }, { 5, 3 } };
	Residence->ApplyProjection(Projection, *Foundation);

	auto* Laborer = Cast<AHansaResidencePresentation>(Residence->BuildingPresentation->GetChildActor());
	UStaticMesh* Mesh = Laborer ? Laborer->ResidenceMesh->GetStaticMesh() : nullptr;
	TestTrue(TEXT("Ready laborer residence uses an approved P14 production variant"),
		Mesh && Mesh->GetPathName().StartsWith(TEXT("/Game/Mesh/hansa-residences/")) &&
		(Mesh == Laborer->VariantA || Mesh == Laborer->VariantB));
	TestTrue(TEXT("The approved residence renders at native scale with authored materials"),
		Laborer && Laborer->ResidenceMesh->GetRelativeScale3D().Equals(FVector::OneVector) &&
		Laborer->ResidenceMesh->GetMaterial(0) != nullptr);

	Projection.Status = EHansaBuildingWorldStatus::UnderConstruction;
	Projection.ConstructionProgress = Require(FHansaRate::TryMakeNormalized(0));
	Residence->ApplyProjection(Projection, *Foundation);
	TestTrue(TEXT("P14 construction retains the authored assembly instead of a generic cube"),
		Laborer && !Residence->ConstructionPlaceholder->IsVisible());
	Projection.Status = EHansaBuildingWorldStatus::Ready;
	Projection.ConstructionProgress = Require(FHansaRate::TryMakeNormalized(FHansaRate::Scale));
	Projection.Placement.BuildingDefinitionId = Definition<FHansaBuildingTypeId>(TEXT("Building.Residence.Artisan"));
	Residence->ApplyProjection(Projection, *Foundation);
	TestEqual(TEXT("Residence upgrade keeps the stable world Actor and replaces its authored definition"),
		Residence->GetBuildingDefinitionId(), FString(TEXT("Building.Residence.Artisan")));
	auto* Artisan = Cast<AHansaResidencePresentation>(Residence->BuildingPresentation->GetChildActor());
	TestTrue(TEXT("Residence upgrade replaces the laborer visual on the existing Actor"),
		Artisan && Artisan->ResidenceMesh->GetStaticMesh() != Mesh &&
		Artisan->GetClass()->GetPathName().Contains(TEXT("BP_Residence_Artisan_Review")));

	World->DestroyWorld(false);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaBreweryProductionPresentationTest,
	"Hansa.UI.World.BreweryProductionPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBreweryProductionPresentationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::WorldProjection;
	const auto* Brewery = Cast<UHansaBuildingDefinition>(UHansaDefinitionBase::ResolveByStableId(TEXT("Building.Brewery")));
	if (!TestNotNull(TEXT("Brewery resolves from production game data"), Brewery)) return false;
	UStaticMesh* Mesh = Brewery->LoadPresentationMesh();
	if (!TestNotNull(TEXT("Approved Brewery mesh loads"), Mesh)) return false;
	if (!TestEqual(TEXT("Brewery has exactly one production recipe"), Brewery->RecipeIds.Num(), 1)) return false;
	TestEqual(TEXT("Brewery keeps the Beer production recipe"), Brewery->RecipeIds[0], FString(TEXT("Recipe.BrewBeer")));
	TestEqual(TEXT("Brewery reserves four cells in width"), Brewery->FootprintWidthCells, 4);
	TestEqual(TEXT("Brewery reserves four cells in depth"), Brewery->FootprintHeightCells, 4);
	TestEqual(TEXT("Brewery uses the approved revision"), Mesh->GetPathName(),
		FString(TEXT("/Game/Mesh/hansa-brewery-huexstrasse128/Production/SM_HansaBrewery_Production.SM_HansaBrewery_Production")));
	const FVector Size = Mesh->GetBoundingBox().GetSize();
	TestTrue(TEXT("Full-size Brewery fits inside its reserved parcel"),
		Size.X <= 1560.0 && Size.Y <= 1560.0 && Size.Z > 1500.0);
	TestEqual(TEXT("All brewery material families survive import"), Mesh->GetStaticMaterials().Num(), 16);
	for (const FStaticMaterial& Material : Mesh->GetStaticMaterials())
		TestNotNull(TEXT("Brewery material slot resolves"), Material.MaterialInterface.Get());

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaBreweryProductionWorld"));
	ON_SCOPE_EXIT { World->DestroyWorld(false); };
	auto* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	auto* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
	FHansaBuildingWorldProjection Projection = MakeWorldProjection(7, TEXT("Building.Brewery"), 4, FHansaRate::Scale);
	Projection.FootprintWidthCells = 4;
	Projection.FootprintHeightCells = 4;
	Projection.OccupiedCells.Reset();
	for (int32 X = 4; X < 8; ++X)
		for (int32 Y = 2; Y < 6; ++Y) Projection.OccupiedCells.Add({X, Y});
	Actor->ApplyProjection(Projection, *Foundation);
	TestEqual(TEXT("Completed in-game Brewery renders the approved mesh"), Actor->BuildingMesh->GetStaticMesh().Get(), Mesh);
	TestTrue(TEXT("Completed Brewery is visible"), Actor->BuildingMesh->IsVisible());
	TestTrue(TEXT("Runtime preserves the authored metre scale"), Actor->BuildingMesh->GetRelativeScale3D().Equals(FVector::OneVector));
	TestTrue(TEXT("Runtime preserves the approved brick material"), Actor->BuildingMesh->GetMaterial(0) == Mesh->GetMaterial(0));
	Projection.Status = EHansaBuildingWorldStatus::UnderConstruction;
	Actor->ApplyProjection(Projection, *Foundation);
	TestTrue(TEXT("Unfinished Brewery retains normal construction behavior"), Actor->ConstructionPlaceholder->IsVisible());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaDataDrivenPresentationTest,
	"Hansa.UI.World.DataDrivenPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaDataDrivenPresentationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::WorldProjection;
	UHansaBuildingDefinition* Definition = Cast<UHansaBuildingDefinition>(UHansaDefinitionBase::ResolveByStableId(TEXT("Building.Bakery")));
	if (!TestNotNull(TEXT("Bakery resolves through Asset Manager stable identity"), Definition)) return false;
	const TSoftObjectPtr<UStaticMesh> Original = Definition->PresentationMesh;
	TestEqual(TEXT("Bakery data references the approved production mesh"), Original.ToSoftObjectPath().ToString(),
		FString(TEXT("/Game/Mesh/hansa-bakery/P10/Meshes/SM_Bakery_Body.SM_Bakery_Body")));
	const TSoftClassPtr<AActor> OriginalClass = Definition->PresentationActorClass;
	TestEqual(TEXT("Bakery production definition binds the approved role actor"),
		OriginalClass.ToSoftObjectPath().ToString(), FString(TEXT("/Script/Hansa.HansaBakeryPresentation")));
	Definition->PresentationActorClass.Reset();
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaDataDrivenMeshWorld"));
	AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	AHansaBuildingWorldProjectionActor* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
	FHansaBuildingWorldProjection Projection = MakeWorldProjection(6, TEXT("Building.Bakery"), 4, FHansaRate::Scale);
	Projection.FootprintWidthCells = 3;
	Projection.FootprintHeightCells = 2;
	Projection.OccupiedCells = { {4,2}, {4,3}, {5,2}, {5,3}, {6,2}, {6,3} };
	// Change the data in memory only: the actor must respond without a definition-ID mesh switch.
	Definition->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Actor->ApplyProjection(Projection, *Foundation);
	TestEqual(TEXT("Assigned data selects the mesh"), Actor->BuildingMesh->GetStaticMesh().Get(), Definition->LoadPresentationMesh());
	TestTrue(TEXT("Authored mesh retains its own material"), Actor->BuildingMesh->GetMaterial(0) == Definition->LoadPresentationMesh()->GetMaterial(0));
	Definition->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	Actor->ApplyProjection(Projection, *Foundation);
	TestTrue(TEXT("Changing data back rebuilds placeholder material state"), Actor->BuildingMesh->GetMaterial(0) != Definition->LoadPresentationMesh()->GetMaterial(0));
	Definition->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Game/Mesh/hansa-bakery/Meshes/SM_HansaBakery.SM_HansaBakery")));
	Actor->ApplyProjection(Projection, *Foundation);
	const FVector Scale = Actor->BuildingMesh->GetRelativeScale3D();
	if (Original.ToSoftObjectPath().ToString() != TEXT("/Engine/BasicShapes/Cube.Cube"))
	{
		TestTrue(TEXT("Authored presentation keeps its authored world scale"), Scale.Equals(FVector::OneVector));
		const FBox Bounds = Actor->BuildingMesh->GetStaticMesh()->GetBoundingBox();
		TestTrue(TEXT("An invalid authored footprint is exposed instead of hidden by runtime squeezing"),
			Bounds.GetSize().X > 1160.01 || Bounds.GetSize().Y > 760.01);
	}
	Projection.Status = EHansaBuildingWorldStatus::UnderConstruction;
	Actor->ApplyProjection(Projection, *Foundation);
	TestTrue(TEXT("Construction continues using the normal placeholder"), Actor->ConstructionPlaceholder->IsVisible() && !Actor->BuildingMesh->IsVisible());
	// Restore the approved production binding after testing the legacy mesh-only path.
	Definition->PresentationMesh = Original;
	Definition->PresentationActorClass = OriginalClass;
	Actor->ApplyProjection(Projection, *Foundation);
	AHansaBakeryPresentation* Bakery = Cast<AHansaBakeryPresentation>(Actor->BuildingPresentation->GetChildActor());
	if (TestNotNull(TEXT("Bakery definition creates the native role assembly"), Bakery))
	{
		TestTrue(TEXT("Bakery construction uses authored geometry, not the fallback"),
			Bakery->Construction->IsVisible() && !Bakery->Bakery->IsVisible() && !Actor->ConstructionPlaceholder->IsVisible());
		Projection.Status = EHansaBuildingWorldStatus::Ready;
		Actor->ApplyProjection(Projection, *Foundation);
		TestTrue(TEXT("Operating bakery exposes flour and bread roles"), Bakery->FlourSack->IsVisible() && Bakery->BreadCrate->IsVisible() && Bakery->Sign->IsVisible());
		Projection.Status = EHansaBuildingWorldStatus::Blocked;
		Actor->ApplyProjection(Projection, *Foundation);
		TestTrue(TEXT("Blocked bakery keeps identity but suppresses operating cues"),
			Bakery->Bakery->IsVisible() && Bakery->Sign->IsVisible() && !Bakery->FlourSack->IsVisible() && !Bakery->BreadCrate->IsVisible());
		Projection.Status = EHansaBuildingWorldStatus::Ready;
		Actor->ApplyProjection(Projection, *Foundation);
		TestTrue(TEXT("Resumed bakery retains authored scale"), Bakery->GetActorScale3D().Equals(FVector::OneVector) && Bakery->BreadCrate->IsVisible());
		for (UStaticMeshComponent* Part : {Bakery->Bakery.Get(), Bakery->Construction.Get(), Bakery->FlourSack.Get(), Bakery->BreadCrate.Get(), Bakery->Sign.Get()})
		{
			UStaticMesh* Mesh = Part->GetStaticMesh();
			if (!TestNotNull(TEXT("Every bakery role resolves"), Mesh)) continue;
			TestTrue(TEXT("Role mesh is canonical"), Mesh->GetPathName().StartsWith(TEXT("/Game/Mesh/hansa-bakery/P10/")));
			TestEqual(TEXT("Every bakery role has three LODs"), Mesh->GetNumLODs(), 3);
			TestEqual(TEXT("Role collision cannot intercept selection"), Part->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
				TestTrue(TEXT("Every material is assigned from the canonical bakery family"), Slot.MaterialInterface && Slot.MaterialInterface->GetPathName().StartsWith(TEXT("/Game/Mesh/hansa-bakery/")));
		}
	}
	World->DestroyWorld(false);

	UHansaGoodDefinition* Good = NewObject<UHansaGoodDefinition>();
	Definition->PresentationActorClass = OriginalClass;
	UHansaVehicleDefinition* Vehicle = NewObject<UHansaVehicleDefinition>();
	const uint64 GoodHash = Good->ComputeDeterministicContentHash();
	Good->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Vehicle->PresentationMesh = Good->PresentationMesh;
	TestNotNull(TEXT("Goods use the common mesh contract"), Good->LoadPresentationMesh());
	TestEqual(TEXT("Vehicles use the same mesh contract"), Vehicle->LoadPresentationMesh(), Good->LoadPresentationMesh());
	TestNotEqual(TEXT("Optional item mesh edits participate in content hashing"), Good->ComputeDeterministicContentHash(), GoodHash);
	Good->PresentationMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/Draft.Draft")));
	TestNull(TEXT("Staging meshes cannot be resolved for game presentation"), Good->LoadPresentationMesh());
	TestNull(TEXT("Unknown stable identity has a safe fallback"), UHansaDefinitionBase::ResolveByStableId(TEXT("Building.DoesNotExist")));
	return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
 FHansaMillBlueprintPresentationTest,
 "Hansa.UI.World.MillBlueprintPresentation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMillBlueprintPresentationTest::RunTest(const FString& Parameters)
{
 using namespace Hansa::Simulation;
 using namespace Hansa::Tests::WorldProjection;
 UHansaBuildingDefinition* Definition = Cast<UHansaBuildingDefinition>(UHansaDefinitionBase::ResolveByStableId(TEXT("Building.Mill")));
 if (!TestNotNull(TEXT("Mill definition resolves"), Definition)) return false;
 UClass* Class = Definition->LoadPresentationActorClass();
 if (!TestNotNull(TEXT("Mill resolves its promoted Blueprint class"), Class)) return false;
 TestEqual(TEXT("Mill uses the requested Blueprint"), Class->GetPathName(), FString(TEXT("/Game/Hansa/Core/Buildings/BP_HansaWindmill_Animated.BP_HansaWindmill_Animated_C")));
 bool bProducesFlour = false;
 for (const FString& RecipeId : Definition->RecipeIds)
 {
  const UHansaRecipeDefinition* Recipe = Cast<UHansaRecipeDefinition>(UHansaDefinitionBase::ResolveByStableId(*RecipeId));
  if (Recipe != nullptr)
   for (const FHansaGoodAmount& Output : Recipe->Outputs) bProducesFlour |= Output.GoodId == TEXT("Good.Flour");
 }
 TestTrue(TEXT("Stable mill still exposes flour output for the bread chain"), bProducesFlour);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaAnimatedMillTest"));
 AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
 AHansaBuildingWorldProjectionActor* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
 FHansaBuildingWorldProjection Projection = MakeWorldProjection(12, TEXT("Building.Mill"), 4, FHansaRate::Scale);
 Projection.FootprintWidthCells = 3; Projection.FootprintHeightCells = 3;
 Projection.OccupiedCells = { {4,2}, {4,3}, {4,4}, {5,2}, {5,3}, {5,4}, {6,2}, {6,3}, {6,4} };
 Actor->ApplyProjection(Projection, *Foundation);
 AActor* Mill = Actor->BuildingPresentation->GetChildActor();
 if (TestNotNull(TEXT("Projection creates a live Blueprint actor"), Mill))
 {
  TestEqual(TEXT("Spawned visual has authored class"), Mill->GetClass(), Class);
  const AStaticMeshActor* BodyActor = Cast<AStaticMeshActor>(Mill);
  UStaticMesh* Body = BodyActor ? BodyActor->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
  if (TestNotNull(TEXT("Mill has a production body"), Body))
  {
   TestEqual(TEXT("Body uses the audited P09 revision"), Body->GetPathName(), FString(TEXT("/Game/Mesh/hansa-mill/P09/Meshes/SM_Mill_Body.SM_Mill_Body")));
   TestEqual(TEXT("Body has three distance LODs"), Body->GetNumLODs(), 3);
   const FBox Bounds = Body->GetBoundingBox();
   TestTrue(TEXT("Grounded body fits the 3x3 inset without auto-fit"), Bounds.GetSize().X < 1160 && Bounds.GetSize().Y < 1160);
   TestTrue(TEXT("Body has a ground-level pivot"), FMath::Abs(Bounds.Min.Z) <= 2);
   TestTrue(TEXT("Body has simple convex collision, not complex-as-simple"), Body->GetBodySetup() &&
    Body->GetBodySetup()->AggGeom.ConvexElems.Num() > 0 && Body->GetBodySetup()->CollisionTraceFlag != CTF_UseComplexAsSimple);
   // NullRHI does not retain GPU colour buffers. Inspect the imported source
   // attribute, not renderer allocation, so commandlet validation stays useful.
#if WITH_EDITOR
   bool bHasWeathering = false;
   if (const FMeshDescription* Description = Body->GetMeshDescription(0))
   {
    const FStaticMeshConstAttributes Attributes(*Description);
    const auto Colors = Attributes.GetVertexInstanceColors();
    for (const FVertexInstanceID Id : Description->VertexInstances().GetElementIDs())
     bHasWeathering |= Colors[Id].X < .95f;
   }
   TestTrue(TEXT("Weathering vertex colours survive import"), bHasWeathering);
#endif
  }
  TestFalse(TEXT("Proxy box is invisible"), Actor->BuildingMesh->IsVisible());
  TestEqual(TEXT("Proxy remains selectable"), Actor->BuildingMesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);
  UChildActorComponent* Sails = Mill->FindComponentByClass<UChildActorComponent>();
  AActor* Rotor = Sails != nullptr ? Sails->GetChildActor() : nullptr;
  if (TestNotNull(TEXT("Nested rotor is retained"), Rotor))
  {
   const AStaticMeshActor* RotorActor = Cast<AStaticMeshActor>(Rotor);
   UStaticMesh* RotorMesh = RotorActor ? RotorActor->GetStaticMeshComponent()->GetStaticMesh() : nullptr;
   if (TestNotNull(TEXT("Rotor has its authored mesh"), RotorMesh))
   {
    TestEqual(TEXT("Nested child template uses P09 rotor"), RotorMesh->GetPathName(), FString(TEXT("/Game/Mesh/hansa-mill/P09/Meshes/SM_Mill_Rotor.SM_Mill_Rotor")));
    TestEqual(TEXT("Animated rotor has three LODs"), RotorMesh->GetNumLODs(), 3);
   }
   TestTrue(TEXT("Rotor keeps its authored pivot"), Sails->GetRelativeLocation().Equals(FVector(0,399,1083.5),.1));
   TestTrue(TEXT("Sails never obstruct placement or navigation"), RotorActor && RotorActor->GetStaticMeshComponent()->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
   URotatingMovementComponent* Movement = Rotor->FindComponentByClass<URotatingMovementComponent>();
   if (TestNotNull(TEXT("Native animation component survives game presentation"), Movement))
   {
    TestEqual(TEXT("Default speed is 6 rpm"), Movement->RotationRate.Pitch, 36.0);
    Movement->SetUpdatedComponent(Rotor->GetRootComponent());
    const FQuat Before = Rotor->GetActorQuat();
    Movement->TickComponent(0.5f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Sails turn during runtime tick"), Rotor->GetActorQuat().Equals(Before));
    const FQuat Turned = Rotor->GetActorQuat();
    Actor->ApplyProjection(Projection, *Foundation);
    TestTrue(TEXT("Projection refresh preserves animation phase"), Rotor->GetActorQuat().Equals(Turned));
    Projection.Status = EHansaBuildingWorldStatus::Blocked;
    Actor->ApplyProjection(Projection, *Foundation);
    TestTrue(TEXT("Blocked milling stops the existing rotor"), Movement->RotationRate.IsZero());
    Movement->TickComponent(0.5f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Blocked milling preserves sail phase"), Rotor->GetActorQuat().Equals(Turned));
    Projection.Status = EHansaBuildingWorldStatus::Ready;
    Actor->ApplyProjection(Projection, *Foundation);
    TestEqual(TEXT("Ready milling restores authored rate"), Movement->RotationRate.Pitch, 36.0);
    TestTrue(TEXT("Resuming does not snap the rotor"), Rotor->GetActorQuat().Equals(Turned));
   }
   Projection.Status = EHansaBuildingWorldStatus::UnderConstruction;
   Actor->ApplyProjection(Projection, *Foundation);
   TestTrue(TEXT("Construction hides the complete nested visual"), Mill->IsHidden() && Rotor->IsHidden() && Actor->ConstructionPlaceholder->IsVisible());
   Projection.Status = EHansaBuildingWorldStatus::Ready;
   Actor->ApplyProjection(Projection, *Foundation);
   TestTrue(TEXT("Completion reveals the same animated assembly"), !Mill->IsHidden() && !Rotor->IsHidden());
  }
  const FVector Scale = Actor->BuildingPresentation->GetRelativeScale3D();
  TestTrue(TEXT("Blueprint presentation keeps its authored world scale"), Scale.Equals(FVector::OneVector));
  Actor->SetSelected(true);
  TestTrue(TEXT("Selection outline remains on stable projection"), Actor->SelectionOutline->IsVisible());
 }
 Actor->Destroy();
 TestFalse(TEXT("Destroying projection destroys its visual"), IsValid(Mill));
 World->DestroyWorld(false);
 UHansaBuildingDefinition* Draft = NewObject<UHansaBuildingDefinition>();
 const uint64 LegacyHash = Draft->ComputeDeterministicContentHash();
 Draft->PresentationActorClass = Definition->PresentationActorClass;
 TestNotEqual(TEXT("Actor reference participates in content hash"), Draft->ComputeDeterministicContentHash(), LegacyHash);
 Draft->PresentationActorClass.Reset();
 TestEqual(TEXT("Clearing optional actor preserves legacy mesh hash"), Draft->ComputeDeterministicContentHash(), LegacyHash);
 Draft->PresentationActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/Draft.Draft_C")));
 TestNull(TEXT("Staging actor classes cannot be loaded for game presentation"), Draft->LoadPresentationActorClass());
 TArray<FHansaDefinitionValidationIssue> Issues; Draft->ValidateDefinition(Issues);
 TestTrue(TEXT("Invalid actor reference receives an actionable validation issue"), Issues.ContainsByPredicate([](const FHansaDefinitionValidationIssue& Issue){ return Issue.Code == TEXT("HSA-BUILDING-009"); }));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementGhostPresentationTest,
	"Hansa.UI.World.PlacementGhostAuthoredPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementGhostPresentationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HansaPlacementGhostTest"));
	AHansaLubeckWorldFoundation* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
	AHansaBuildingPlacementGhost* Ghost = World->SpawnActor<AHansaBuildingPlacementGhost>();
	if (!TestNotNull(TEXT("Disposable world creates its placement foundation"), Foundation) ||
		!TestNotNull(TEXT("Disposable world creates the native placement ghost"), Ghost))
	{
		World->DestroyWorld(false);
		return false;
	}
	const TArray<FIntPoint> Cells = { FIntPoint(4, 5), FIntPoint(5, 5), FIntPoint(4, 6), FIntPoint(5, 6) };
	Ghost->ApplyPreview(TEXT("Building.Warehouse"), FIntPoint(4, 5), 1, Cells,
		EHansaPlacementFeedback::Invalid, FText::FromString(TEXT("Footprint occupied")), *Foundation);
	TestTrue(TEXT("World-space ghost becomes visible while a site is targeted"), Ghost->IsPreviewVisible());
	TestEqual(TEXT("Ghost retains stable building-definition identity"), Ghost->GetPreviewBuildingId(), FName(TEXT("Building.Warehouse")));
	TestEqual(TEXT("Ghost renders every authoritative footprint cell"), Ghost->GetPreviewCellCount(), Cells.Num());
	TestTrue(TEXT("Ghost is snapped to the center of its authored grid footprint"),
		Ghost->GetActorLocation().Equals((Foundation->PlacementCellToWorld(4, 5, 106.0f) +
			Foundation->PlacementCellToWorld(5, 6, 106.0f)) * 0.5, 0.1));
	Ghost->ApplyPreview(TEXT("Building.Warehouse"), FIntPoint(4, 5), 2, Cells,
		EHansaPlacementFeedback::Warning, FText::FromString(TEXT("Repeated placement enabled")), *Foundation);
	TestEqual(TEXT("Rotation refresh preserves the same complete footprint"), Ghost->GetPreviewCellCount(), Cells.Num());
	UHansaBuildingDefinition* RoadDefinition = Cast<UHansaBuildingDefinition>(
		UHansaDefinitionBase::ResolveByStableId(TEXT("Building.Road")));
	UHansaBuildingDefinition* BakeryDefinition = Cast<UHansaBuildingDefinition>(
		UHansaDefinitionBase::ResolveByStableId(TEXT("Building.Bakery")));
	if (TestNotNull(TEXT("Road definition resolves for authored preview"), RoadDefinition) &&
		TestNotNull(TEXT("A production building mesh is available for contract verification"), BakeryDefinition))
	{
		const TSoftObjectPtr<UStaticMesh> OriginalRoadMesh = RoadDefinition->PresentationMesh;
		RoadDefinition->PresentationMesh = BakeryDefinition->PresentationMesh;
		const TArray<FHansaRoadPreviewCell> RoadCells = {
			{ FIntPoint(4, 5), EHansaRoadPreviewCellState::NewValid, NAME_None },
			{ FIntPoint(5, 5), EHansaRoadPreviewCellState::ExistingRoad, NAME_None },
			{ FIntPoint(6, 5), EHansaRoadPreviewCellState::Invalid, TEXT("Occupied") }
		};
		Ghost->ApplyRoadPreview(RoadCells, EHansaPlacementFeedback::Invalid,
			FText::FromString(TEXT("One cell is occupied")), *Foundation);
		TestEqual(TEXT("Road ghost exposes every typed path cell"), Ghost->GetPreviewCellCount(), 3);
		TestEqual(TEXT("Existing road is reused while new and invalid cells use authored ghost pieces"),
			Ghost->GetRoadPieceCount(), 2);
		RoadDefinition->PresentationMesh = OriginalRoadMesh;
	}
	Ghost->HidePreview();
	TestFalse(TEXT("Cancellation hides the preview without creating simulation state"), Ghost->IsPreviewVisible());
	World->DestroyWorld(false);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSelectionPreservesProductionVisibilityTest,
    "Hansa.World.Projection.SelectionPreservesProductionVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSelectionPreservesProductionVisibilityTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    auto* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
    if (!TestNotNull(TEXT("Building projection"), Actor)) return false;
    Hansa::Simulation::FHansaProductionProjection Production{};
    Actor->ApplyProduction(&Production, {});
    TestFalse(TEXT("Unavailable production artwork hides the fallback cube"), Actor->BuildingMesh->IsVisible());
    Actor->SetSelected(true);
    TestTrue(TEXT("Building selection shows its outline"), Actor->SelectionOutline->IsVisible());
	TestEqual(TEXT("Building selection exposes four shape-redundant footprint corners"),
		Actor->SelectionCornerSegments.Num(), 8);
	TestTrue(TEXT("Selected fallback geometry is custom-depth tagged"),
		Actor->ConstructionPlaceholder->bRenderCustomDepth);
    TestFalse(TEXT("Selection preserves hidden fallback"), Actor->BuildingMesh->IsVisible());
    Actor->SetSelected(false);
    TestFalse(TEXT("Ground click clears outline"), Actor->SelectionOutline->IsVisible());
	TestFalse(TEXT("Ground click clears every footprint corner"),
		Actor->SelectionCornerSegments.ContainsByPredicate(
			[](const UStaticMeshComponent* Segment) { return Segment != nullptr && Segment->IsVisible(); }));
	TestTrue(TEXT("Ground click destroys transient mesh contours"),
		Actor->SelectionContourMeshes.IsEmpty() && Actor->SelectionHaloMeshes.IsEmpty());
	TestFalse(TEXT("Ground click clears custom depth"), Actor->ConstructionPlaceholder->bRenderCustomDepth);
    TestFalse(TEXT("Ground click does not reveal a red fallback cube"), Actor->BuildingMesh->IsVisible());
    Actor->SetSelected(false);
    TestFalse(TEXT("Repeated ground clicks keep fallback hidden"), Actor->BuildingMesh->IsVisible());
    return !HasAnyErrors();
}
#endif
