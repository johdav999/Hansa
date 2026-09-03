#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaStrategyCameraModel.h"

#include "Components/StaticMeshComponent.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaIds.h"
#include "World/HansaGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaStrategyCameraIntentTest,
	"Hansa.UI.World.StrategyCameraIntents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaStrategyCameraIntentTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Game;

	FHansaStrategyCameraSettings Settings;
	Settings.BoundsMin = FVector2D(-1000.0, -1000.0);
	Settings.BoundsMax = FVector2D(1000.0, 1000.0);
	Settings.PanUnitsPerSecond = 100.0f;
	Settings.FastPanMultiplier = 3.0f;
	Settings.RotationDegreesPerSecond = 45.0f;
	Settings.ZoomUnitsPerStep = 250.0f;
	Settings.MinimumZoomDistance = 1000.0f;
	Settings.MaximumZoomDistance = 5000.0f;

	FHansaStrategyCameraState Initial;
	Initial.Focus = FVector2D::ZeroVector;
	Initial.YawDegrees = 0.0f;
	Initial.ZoomDistance = 3000.0f;

	FHansaStrategyCameraIntent PanForward;
	PanForward.Pan = FVector2D(0.0, 1.0);
	const FHansaStrategyCameraState Forward = FHansaStrategyCameraModel::Advance(Initial, PanForward, Settings, 2.0f);
	TestTrue(TEXT("Forward intent is relative to camera yaw"), Forward.Focus.Equals(FVector2D(200.0, 0.0)));

	FHansaStrategyCameraIntent PanRightFast;
	PanRightFast.Pan = FVector2D(1.0, 0.0);
	PanRightFast.bFastPan = true;
	const FHansaStrategyCameraState RightFast = FHansaStrategyCameraModel::Advance(Initial, PanRightFast, Settings, 2.0f);
	TestTrue(TEXT("Fast right intent uses the shared multiplier"), RightFast.Focus.Equals(FVector2D(0.0, 600.0)));

	FHansaStrategyCameraIntent Combined;
	Combined.Pan = FVector2D(1.0, 1.0);
	Combined.Rotate = 1.0f;
	Combined.ZoomSteps = 20.0f;
	const FHansaStrategyCameraState Advanced = FHansaStrategyCameraModel::Advance(Initial, Combined, Settings, 2.0f);
	TestTrue(TEXT("Diagonal pan is normalized before applying speed"), Advanced.Focus.Size() <= 200.01f);
	TestEqual(TEXT("Rotation intent advances at the configured rate"), Advanced.YawDegrees, 90.0f);
	TestEqual(TEXT("Zoom-in intent clamps at the minimum"), Advanced.ZoomDistance, 1000.0f);

	FHansaStrategyCameraState Outside = Initial;
	Outside.Focus = FVector2D(990.0, 990.0);
	const FHansaStrategyCameraState Clamped = FHansaStrategyCameraModel::Advance(Outside, Combined, Settings, 20.0f);
	TestTrue(TEXT("Pan focus remains inside authored map bounds"),
		Clamped.Focus.X <= Settings.BoundsMax.X && Clamped.Focus.Y <= Settings.BoundsMax.Y);

	FHansaStrategyCameraIntent ZoomOut;
	ZoomOut.ZoomSteps = -20.0f;
	const FHansaStrategyCameraState Far = FHansaStrategyCameraModel::Advance(Initial, ZoomOut, Settings, 0.0f);
	TestEqual(TEXT("Zoom-out intent clamps at the maximum"), Far.ZoomDistance, 5000.0f);

	const FHansaStrategyCameraState InvalidDelta = FHansaStrategyCameraModel::Advance(Initial, Combined, Settings, -1.0f);
	TestTrue(TEXT("Invalid frame deltas cannot mutate presentation state"),
		InvalidDelta.Focus.Equals(Initial.Focus) && InvalidDelta.YawDegrees == Initial.YawDegrees &&
		InvalidDelta.ZoomDistance == Initial.ZoomDistance);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLubeckMapContractTest,
	"Hansa.Content.World.LubeckMapContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLubeckMapContractTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;

	const FString& MapId = Hansa::Game::LubeckMap::StableMapId();
	TestEqual(TEXT("The MVP map has an immutable stable region ID"), MapId, FString(TEXT("Region.Lubeck.Mvp")));
	TestTrue(TEXT("The map ID is valid in the canonical definition namespace"),
		FHansaDefinitionId::TryParse(MapId).IsSuccess());
	TestEqual(TEXT("Automation uses a stable named start"),
		Hansa::Game::LubeckMap::AutomationStartId(), FName(TEXT("World.Lubeck.AutomationStart")));

	const FVector2D BoundsMin = Hansa::Game::LubeckMap::CameraBoundsMin();
	const FVector2D BoundsMax = Hansa::Game::LubeckMap::CameraBoundsMax();
	const FVector Start = Hansa::Game::LubeckMap::AutomationStartTransform().GetLocation();
	TestTrue(TEXT("Automation start lies inside camera bounds"),
		Start.X >= BoundsMin.X && Start.X <= BoundsMax.X && Start.Y >= BoundsMin.Y && Start.Y <= BoundsMax.Y);

	const AHansaLubeckWorldFoundation* Foundation = GetDefault<AHansaLubeckWorldFoundation>();
	TestNotNull(TEXT("Lübeck world foundation has a class default"), Foundation);
	if (Foundation != nullptr)
	{
#if WITH_EDITOR
		TestFalse(TEXT("The world foundation is always loaded by World Partition"), Foundation->GetIsSpatiallyLoaded());
#endif
		TArray<UStaticMeshComponent*> Components;
		Foundation->GetComponents<UStaticMeshComponent>(Components);
		TestTrue(TEXT("Representative topology has multiple independently tagged surfaces"), Components.Num() >= 12);

		TSet<FName> SurfaceTags;
		for (const UStaticMeshComponent* Component : Components)
		{
			for (const FName Tag : Component->ComponentTags)
			{
				if (Tag.ToString().StartsWith(TEXT("Hansa.World.Surface.")) && Tag != TEXT("Hansa.World.Surface.Selectable"))
				{
					SurfaceTags.Add(Tag);
				}
			}
		}
		TestTrue(TEXT("Land topology is tagged"), SurfaceTags.Contains(TEXT("Hansa.World.Surface.Land")));
		TestTrue(TEXT("Shore topology is tagged"), SurfaceTags.Contains(TEXT("Hansa.World.Surface.Shore")));
		TestTrue(TEXT("Harbor topology is tagged"), SurfaceTags.Contains(TEXT("Hansa.World.Surface.Harbor")));
		TestTrue(TEXT("Water topology is tagged"), SurfaceTags.Contains(TEXT("Hansa.World.Surface.Water")));
		TestTrue(TEXT("Road datum topology is tagged"), SurfaceTags.Contains(TEXT("Hansa.World.Surface.Road")));
	}
#if WITH_EDITOR
	TestFalse(TEXT("The deterministic PlayerStart is always loaded by World Partition"),
		GetDefault<AHansaLubeckAutomationStart>()->GetIsSpatiallyLoaded());
#endif

	UWorld* MapAsset = LoadObject<UWorld>(nullptr, TEXT("/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP.L_Lubeck_MVP"));
	TestNotNull(TEXT("The checked-in Lübeck gameplay map loads"), MapAsset);
	if (MapAsset != nullptr)
	{
		TestNotNull(TEXT("The checked-in Lübeck map uses World Partition"), MapAsset->GetWorldPartition());
		const AWorldSettings* WorldSettings = MapAsset->GetWorldSettings(false);
		TestNotNull(TEXT("The checked-in Lübeck map has world settings"), WorldSettings);
		if (WorldSettings != nullptr)
		{
			TestTrue(TEXT("The checked-in Lübeck map selects the Hansa game mode"),
				WorldSettings->DefaultGameMode == AHansaGameMode::StaticClass());
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaLubeckPlacementGridTest,
	"Hansa.Content.World.LubeckPlacementGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaLubeckPlacementGridTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;

	FHansaCompiledBuildingDefinition Road;
	Road.StableId = TEXT("Building.Road");
	Road.FootprintWidthCells = 1;
	Road.FootprintHeightCells = 1;
	FHansaCompiledBuildingDefinition Fishery;
	Fishery.StableId = TEXT("Building.Fishery");
	Fishery.FootprintWidthCells = 2;
	Fishery.FootprintHeightCells = 1;
	Fishery.bRequiresShoreline = true;
	const FHansaEconomicRegistry Registry({}, {}, { Fishery, Road }, 0x4C554245434B4752ULL);
	const FHansaHouseId Owner = FHansaHouseId::TryCreate(1).Value;
	const THansaValueResult<FHansaPlacementInitialization> Initialization =
		Hansa::Game::LubeckPlacementGrid::TryBuildInitialization(Owner, Registry);
	TestTrue(TEXT("Lübeck placement grid initialization succeeds"), Initialization.IsSuccess());
	if (!Initialization)
	{
		return false;
	}
	TestEqual(TEXT("Lübeck owns one buildable placement map"), Initialization.Value.Maps.Num(), 1);
	if (Initialization.Value.Maps.Num() == 1)
	{
		const FHansaPlacementMapInitialization& Map = Initialization.Value.Maps[0];
		TestEqual(TEXT("The 60x40 map exposes every cell explicitly"), Map.Cells.Num(), 2400);
		int32 Land = 0;
		int32 Shore = 0;
		int32 Water = 0;
		for (const FHansaPlacementGridCell& Cell : Map.Cells)
		{
			Land += Cell.Terrain == EHansaPlacementTerrain::Land ? 1 : 0;
			Shore += Cell.Terrain == EHansaPlacementTerrain::Shore ? 1 : 0;
			Water += Cell.Terrain == EHansaPlacementTerrain::Water ? 1 : 0;
		}
		TestTrue(TEXT("Grid includes land for the full vertical slice"), Land > 0);
		TestTrue(TEXT("Grid includes explicit shoreline cells"), Shore > 0);
		TestTrue(TEXT("Grid includes water for deterministic rejection"), Water > 0);
	}
	TestEqual(TEXT("Every compiled building receives an initial scenario entitlement"),
		Initialization.Value.Entitlements.Num(), 2);

	const FHansaGridCoordinate Coordinate { 17, 23 };
	const FVector WorldCenter = Hansa::Game::LubeckPlacementGrid::GridToWorld(Coordinate);
	TestTrue(TEXT("Grid/world conversion round-trips through native cell centers"),
		Hansa::Game::LubeckPlacementGrid::WorldToGrid(WorldCenter) == Coordinate);
	return !HasAnyErrors();
}

#endif
