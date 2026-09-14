#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaGameMode.h"
#include "World/HansaGameState.h"
#include "World/HansaLubeckWorldArt.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaPresentationClock.h"
#include "World/HansaTerrainPlacement.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureCube.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "World/HansaLubeckPlacementGrid.h"

namespace Hansa::Game::LubeckMap
{
	const FString& StableMapId()
	{
		static const FString Value(TEXT("Region.Lubeck.Mvp"));
		return Value;
	}

	const FName& AutomationStartId()
	{
		static const FName Value(TEXT("World.Lubeck.AutomationStart"));
		return Value;
	}

	FTransform AutomationStartTransform()
	{
		return FTransform(FRotator(0.0, 35.0, 0.0), FVector(-3200.0, -700.0, 150.0));
	}

	FVector2D CameraBoundsMin()
	{
		return FVector2D(-12000.0, -8000.0);
	}

	FVector2D CameraBoundsMax()
	{
		return FVector2D(12000.0, 8000.0);
	}
}

namespace
{
	constexpr TCHAR SurfaceTagPrefix[] = TEXT("Hansa.World.Surface.");

	FLinearColor ColorForSurface(const FName SurfaceTag)
	{
		if (SurfaceTag == TEXT("Hansa.World.Surface.Water"))
		{
			return FLinearColor(0.035f, 0.18f, 0.24f);
		}
		if (SurfaceTag == TEXT("Hansa.World.Surface.Shore"))
		{
			return FLinearColor(0.55f, 0.45f, 0.27f);
		}
		if (SurfaceTag == TEXT("Hansa.World.Surface.Harbor"))
		{
			return FLinearColor(0.24f, 0.25f, 0.23f);
		}
		if (SurfaceTag == TEXT("Hansa.World.Surface.Road"))
		{
			return FLinearColor(0.20f, 0.14f, 0.09f);
		}
		return FLinearColor(0.19f, 0.30f, 0.13f);
	}
}

AHansaLubeckWorldFoundation::AHansaLubeckWorldFoundation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
	StableMapId = Hansa::Game::LubeckMap::StableMapId();
	StableAutomationStartId = Hansa::Game::LubeckMap::AutomationStartId();
	Tags.Add(TEXT("Hansa.World.LubeckMvp"));

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
	SceneRoot->SetMobility(EComponentMobility::Static);

	CameraBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("CameraBounds"));
	CameraBounds->SetupAttachment(SceneRoot);
	CameraBounds->SetRelativeLocation(FVector(0.0, 0.0, 1000.0));
	CameraBounds->SetBoxExtent(FVector(12000.0, 8000.0, 5000.0));
	CameraBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CameraBounds->SetHiddenInGame(true);
	CameraBounds->ShapeColor = FColor(190, 150, 65);

	AutomationStart = CreateDefaultSubobject<UArrowComponent>(TEXT("AutomationStart"));
	AutomationStart->SetupAttachment(SceneRoot);
	AutomationStart->SetRelativeTransform(Hansa::Game::LubeckMap::AutomationStartTransform());
	AutomationStart->ArrowColor = FColor(255, 190, 48);
	AutomationStart->ArrowSize = 3.0f;

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("PlaceholderSun"));
	SunLight->SetupAttachment(SceneRoot);
	SunLight->SetMobility(EComponentMobility::Movable);
	SunLight->bAtmosphereSunLight = true;
	SunLight->ContactShadowLength = 0.0f;

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("PlaceholderSkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = false;
	SkyLight->SourceType = SLS_SpecifiedCubemap;
	static ConstructorHelpers::FObjectFinder<UTextureCube> AmbientCube(
		TEXT("/Engine/EngineResources/GrayLightTextureCube.GrayLightTextureCube"));
	SkyLight->Cubemap = AmbientCube.Object;

	SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("PlaceholderSkyAtmosphere"));
	SkyAtmosphere->SetupAttachment(SceneRoot);

	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PlaceholderExposure"));
	Exposure->SetupAttachment(SceneRoot);
	Exposure->bUnbound = true;
	Exposure->Priority = 100.0f;
	Exposure->Settings.bOverride_AutoExposureMethod = true;
	Exposure->Settings.AutoExposureMethod = AEM_Manual;
	Exposure->Settings.bOverride_AutoExposureMinBrightness = true;
	Exposure->Settings.bOverride_AutoExposureMaxBrightness = true;
	Exposure->Settings.bOverride_AutoExposureBias = true;
	Exposure->Settings.AutoExposureBias = Hansa::Game::LubeckWorldArt::ExposureCompensationStops;
	Exposure->Settings.bOverride_CameraShutterSpeed = true;
	Exposure->Settings.CameraShutterSpeed = Hansa::Game::LubeckWorldArt::ExposureShutterSpeedForEV100(14.0f);
	Exposure->Settings.bOverride_CameraISO = true;
	Exposure->Settings.CameraISO = 100.0f;
	Exposure->Settings.bOverride_DepthOfFieldFstop = true;
	Exposure->Settings.DepthOfFieldFstop = 4.0f;
	Exposure->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Exposure->Settings.AutoExposureApplyPhysicalCameraExposure = true;
	Exposure->Settings.bOverride_LocalExposureHighlightContrastScale = true;
	Exposure->Settings.LocalExposureHighlightContrastScale = 1.0f;
	Exposure->Settings.bOverride_LocalExposureShadowContrastScale = true;
	Exposure->Settings.LocalExposureShadowContrastScale = 1.0f;
	Exposure->Settings.bOverride_LocalExposureDetailStrength = true;
	Exposure->Settings.LocalExposureDetailStrength = 1.0f;
	Exposure->Settings.bOverride_LocalExposureHighlightContrastCurve = true;
	Exposure->Settings.LocalExposureHighlightContrastCurve = nullptr;
	Exposure->Settings.bOverride_LocalExposureShadowContrastCurve = true;
	Exposure->Settings.LocalExposureShadowContrastCurve = nullptr;
	Exposure->Settings.bOverride_AmbientOcclusionIntensity = true;
	Exposure->Settings.AmbientOcclusionIntensity = Hansa::Game::LubeckWorldArt::AmbientOcclusionIntensity;
	Exposure->Settings.bOverride_LumenAmbientOcclusionIntensity = true;
	Exposure->Settings.LumenAmbientOcclusionIntensity = Hansa::Game::LubeckWorldArt::LumenAmbientOcclusionIntensity;

	Hansa::Simulation::FHansaCalendarProjection PreviewCalendar;
	PreviewCalendar.ElapsedDays = 5;
	Hansa::Game::PresentationClock::LockToMidday(PreviewCalendar);
	ApplyLightingState(Hansa::Game::LubeckWorldArt::EvaluateLighting(PreviewCalendar, 0.0, 60));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	PlaceholderBaseMaterial = BasicShapeMaterial.Object;
	const auto AddBox = [this](
		const FName Name,
		const FVector& Location,
		const FVector& Scale,
		const FRotator& Rotation,
		const FName SurfaceTag,
		const bool bCollision)
	{
		UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetupAttachment(SceneRoot);
		Component->SetRelativeLocation(Location);
		Component->SetRelativeRotation(Rotation);
		Component->SetRelativeScale3D(Scale);
		Component->SetStaticMesh(CubeMesh.Object);
		Component->SetMaterial(0, PlaceholderBaseMaterial);
		Component->SetMobility(EComponentMobility::Static);
		Component->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->SetCanEverAffectNavigation(bCollision && SurfaceTag != TEXT("Hansa.World.Surface.Shore"));
		Component->ComponentTags.Add(SurfaceTag);
		Component->ComponentTags.Add(FName(*FString::Printf(TEXT("%sSelectable"), SurfaceTagPrefix)));
		TopologyComponents.Add(Component);
	};

	// Water sits below all walkable/selectable surfaces and defines the Trave/harbor side of the slice.
	AddBox(TEXT("Water"), FVector(0.0, 0.0, -175.0), FVector(240.0, 160.0, 1.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Water"), false);

	// Render and authoritative sampling consume the same rotated geometry. The shore top is deliberately
	// above the land placeholder so the validation strip remains visible at the land/water transition.
	for (const Hansa::Game::LubeckPlacementGrid::FHansaSurfaceBox& Surface :
		Hansa::Game::LubeckPlacementGrid::GetLandSurfaces())
	{
		AddBox(Surface.Name, Surface.Location, Surface.Scale, Surface.Rotation,
			TEXT("Hansa.World.Surface.Land"), true);
	}
	for (const Hansa::Game::LubeckPlacementGrid::FHansaSurfaceBox& Surface :
		Hansa::Game::LubeckPlacementGrid::GetShoreSurfaces())
	{
		AddBox(Surface.Name, Surface.Location, Surface.Scale, Surface.Rotation,
			TEXT("Hansa.World.Surface.Shore"), true);
	}

	// Quay and two piers reserve a readable harbor connection without committing final environment art.
	AddBox(TEXT("Quay"), FVector(390.0, -500.0, 65.0), FVector(5.0, 24.0, 1.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Harbor"), true);
	AddBox(TEXT("PierNorth"), FVector(1225.0, 850.0, 65.0), FVector(18.0, 3.0, 1.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Harbor"), true);
	AddBox(TEXT("PierSouth"), FVector(1225.0, -1850.0, 65.0), FVector(18.0, 3.0, 1.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Harbor"), true);

	// Roads are exclusively authoritative player/scenario placements.

}

void AHansaLubeckWorldFoundation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(GetWorld())) bUseAuthoredWorld = true;
	PlaceholderMaterials.Reset();
	SunLight->SetVisibility(!bUseAuthoredWorld);
	SkyLight->SetVisibility(!bUseAuthoredWorld);
	SkyAtmosphere->SetVisibility(!bUseAuthoredWorld);
	Exposure->bEnabled = !bUseAuthoredWorld;

	for (UStaticMeshComponent* Component : TopologyComponents)
	{
		if (Component == nullptr || PlaceholderBaseMaterial == nullptr || Component->ComponentTags.IsEmpty())
		{
			continue;
		}

        Component->SetVisibility(!bUseAuthoredWorld, true);
        Component->SetCollisionEnabled(bUseAuthoredWorld || Component->ComponentTags.Contains(TEXT("Hansa.World.Surface.Water"))
            ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
        if (bUseAuthoredWorld) continue;
		const FName SurfaceTag = Component->ComponentTags[0];
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(PlaceholderBaseMaterial, this);
		if (Material != nullptr)
		{
			Material->SetVectorParameterValue(TEXT("Color"), ColorForSurface(SurfaceTag));
			Component->SetMaterial(0, Material);
			PlaceholderMaterials.Add(Material);
		}
	}
}

void AHansaLubeckWorldFoundation::BeginPlay()
{
	Super::BeginPlay();
	ApplyAuthoritativeSimulationLighting();
}

void AHansaLubeckWorldFoundation::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyAuthoritativeSimulationLighting();
}

void AHansaLubeckWorldFoundation::ApplyLightingState(
	const Hansa::Game::LubeckWorldArt::FHansaLightingState& State)
{
	if (bUseAuthoredWorld || SunLight == nullptr || SkyLight == nullptr || Exposure == nullptr) return;
	SunLight->SetRelativeRotation(FRotator(-State.SolarElevationDegrees, State.SunYawDegrees, 0.0f));
	SunLight->SetIntensity(State.SunIntensityLux);
	SunLight->SetUseTemperature(true);
	SunLight->SetTemperature(State.SunTemperatureKelvin);
	SunLight->SetLightColor(FLinearColor::White);
	SunLight->SetLightSourceAngle(State.SunSourceAngleDegrees);
	SunLight->ContactShadowLength = 0.0f;
	SkyLight->SetIntensity(State.SkyLightIntensity);
	SkyLight->SetLightColor(FLinearColor::MakeFromColorTemperature(State.SkyTemperatureKelvin));
	Exposure->Settings.AutoExposureMinBrightness = State.ExposureEV100;
	Exposure->Settings.AutoExposureMaxBrightness = State.ExposureEV100;
	Exposure->Settings.CameraShutterSpeed = Hansa::Game::LubeckWorldArt::ExposureShutterSpeedForEV100(State.ExposureEV100);
}

bool AHansaLubeckWorldFoundation::ApplyAuthoritativeSimulationLighting()
{
	if (bUseAuthoredWorld) return false;
	UWorld* World = GetWorld();
	if (World == nullptr) return false;
	if (AHansaGameMode* Mode = World->GetAuthGameMode<AHansaGameMode>())
	{
		if (UHansaRuntimeSimulationHost* Host = Mode->GetSimulationHost())
		{
			Hansa::Simulation::FHansaCalendarProjection Calendar;
			double Fraction = 0.0;
			uint16 MinutesPerTick = 60;
			if (Host->TryGetPresentationCalendar(Calendar, Fraction, MinutesPerTick))
			{
				ApplyLightingState(Hansa::Game::LubeckWorldArt::EvaluateLighting(Calendar, Fraction, MinutesPerTick));
				return true;
			}
		}
	}
	if (const AHansaGameState* State = World->GetGameState<AHansaGameState>();
		State != nullptr && State->ServerSimulationTick >= 0)
	{
		const int64 TickValue = State->ServerSimulationTick;
		Hansa::Simulation::FHansaCalendarProjection Calendar;
		Calendar.ElapsedDays = TickValue / 24;
		Hansa::Game::PresentationClock::LockToMidday(Calendar);
		ApplyLightingState(Hansa::Game::LubeckWorldArt::EvaluateLighting(Calendar, 0.0, 60));
		return true;
	}
	return false;
}

FTransform AHansaLubeckWorldFoundation::GetAutomationStartTransform() const
{
	if (Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(GetWorld()))
		return FTransform(FRotator(0,35,0), Hansa::Game::TerrainPlacement::Ground(
			GetWorld(), Hansa::Game::LubeckPlacementGrid::SurveyStartLocation(), 100.0));
	return AutomationStart != nullptr ? AutomationStart->GetComponentTransform() :
		Hansa::Game::LubeckMap::AutomationStartTransform() * GetActorTransform();
}

FVector2D AHansaLubeckWorldFoundation::GetCameraBoundsMin() const
{
	if (Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(GetWorld())) return FVector2D(-201200.0,-201600.0);
	const FVector Origin = CameraBounds->Bounds.Origin - CameraBounds->Bounds.BoxExtent;
	return FVector2D(Origin.X, Origin.Y);
}

FVector2D AHansaLubeckWorldFoundation::GetCameraBoundsMax() const
{
	if (Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(GetWorld())) return FVector2D(201600.0,201200.0);
	const FVector End = CameraBounds->Bounds.Origin + CameraBounds->Bounds.BoxExtent;
	return FVector2D(End.X, End.Y);
}

bool AHansaLubeckWorldFoundation::WorldToPlacementCell(
	const FVector WorldLocation,
	int32& OutX,
	int32& OutY) const
{
	const FVector LocalLocation = GetActorTransform().InverseTransformPosition(WorldLocation);
	const Hansa::Simulation::FHansaGridCoordinate Coordinate =
		Hansa::Game::LubeckPlacementGrid::WorldToGrid(LocalLocation);
	OutX = Coordinate.X;
	OutY = Coordinate.Y;
	if (Hansa::Game::LubeckPlacementGrid::IsSurveyWorld(GetWorld()))
	{
		return Coordinate.X >= Hansa::Game::LubeckPlacementGrid::SurveyMinX &&
			Coordinate.X <= Hansa::Game::LubeckPlacementGrid::SurveyMaxX &&
			Coordinate.Y >= Hansa::Game::LubeckPlacementGrid::SurveyMinY &&
			Coordinate.Y <= Hansa::Game::LubeckPlacementGrid::SurveyMaxY;
	}
	return Coordinate.X >= 0 && Coordinate.X < Hansa::Game::LubeckPlacementGrid::WidthCells &&
		Coordinate.Y >= 0 && Coordinate.Y < Hansa::Game::LubeckPlacementGrid::HeightCells;
}

FVector AHansaLubeckWorldFoundation::PlacementCellToWorld(
	const int32 X,
	const int32 Y,
	const float Height, const float GroundDatum) const
{
	return GroundPlacementPosition(GetActorTransform().TransformPosition(
		Hansa::Game::LubeckPlacementGrid::GridToWorld({ X, Y }, Height)), GroundDatum);
}

AHansaLubeckAutomationStart::AHansaLubeckAutomationStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
	StableMapId = Hansa::Game::LubeckMap::StableMapId();
	StableStartId = Hansa::Game::LubeckMap::AutomationStartId();
	PlayerStartTag = StableStartId;
	Tags.Add(StableStartId);
}

FTransform AHansaLubeckWorldFoundation::GetCargoBerthTransform() const
{
    // The complete 7.81 m beam clears the north pier's eastern end (x=2125).
    return FTransform(FRotator(0,90,0),FVector(2650,850,-125))*GetActorTransform();
}

FVector AHansaLubeckWorldFoundation::GroundPlacementPosition(FVector Position, double LocalDatum) const
{
    FVector Local = GetActorTransform().InverseTransformPosition(Position);
    Local.Z = LocalDatum;
    return Hansa::Game::TerrainPlacement::Ground(GetWorld(), Position, GetActorTransform().TransformPosition(Local).Z);
}
