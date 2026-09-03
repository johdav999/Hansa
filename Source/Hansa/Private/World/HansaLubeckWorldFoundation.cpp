#include "World/HansaLubeckWorldFoundation.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
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
	PrimaryActorTick.bCanEverTick = false;
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
	SunLight->SetRelativeRotation(FRotator(-50.0, -35.0, 0.0));
	SunLight->SetIntensity(5.0f);
	SunLight->SetLightColor(FLinearColor(1.0f, 0.84f, 0.68f));

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("PlaceholderSkyLight"));
	SkyLight->SetupAttachment(SceneRoot);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetIntensity(0.6f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	PlaceholderBaseMaterial = BasicShapeMaterial.Object;
	const auto AddBox = [this](
		const TCHAR* Name,
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

	// Three overlapping masses form a non-rectangular western bank with room for every MVP chain.
	AddBox(TEXT("LandCore"), FVector(-4200.0, 500.0, -25.0), FVector(78.0, 105.0, 2.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Land"), true);
	AddBox(TEXT("LandNorth"), FVector(-900.0, 4300.0, -25.0), FVector(62.0, 35.0, 2.0), FRotator(0.0, -8.0, 0.0),
		TEXT("Hansa.World.Surface.Land"), true);
	AddBox(TEXT("LandSouth"), FVector(-1700.0, -4300.0, -25.0), FVector(55.0, 34.0, 2.0), FRotator(0.0, 12.0, 0.0),
		TEXT("Hansa.World.Surface.Land"), true);

	// The shore separates generic land placement from shoreline-only fishery/dock validation in S05-P02.
	AddBox(TEXT("ShoreNorth"), FVector(-50.0, 3100.0, 15.0), FVector(9.0, 35.0, 0.6), FRotator(0.0, -8.0, 0.0),
		TEXT("Hansa.World.Surface.Shore"), true);
	AddBox(TEXT("ShoreCentral"), FVector(-180.0, -150.0, 15.0), FVector(9.0, 34.0, 0.6), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Shore"), true);
	AddBox(TEXT("ShoreSouth"), FVector(-420.0, -3550.0, 15.0), FVector(9.0, 34.0, 0.6), FRotator(0.0, 12.0, 0.0),
		TEXT("Hansa.World.Surface.Shore"), true);

	// Quay and two piers reserve a readable harbor connection without committing final environment art.
	AddBox(TEXT("Quay"), FVector(390.0, -500.0, 65.0), FVector(5.0, 24.0, 1.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Harbor"), true);
	AddBox(TEXT("PierNorth"), FVector(1225.0, 850.0, 65.0), FVector(18.0, 3.0, 1.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Harbor"), true);
	AddBox(TEXT("PierSouth"), FVector(1225.0, -1850.0, 65.0), FVector(18.0, 3.0, 1.0), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Harbor"), true);

	// Datum roads make the start area and harbor approach legible; authoritative roads arrive in S05-P02/P03.
	AddBox(TEXT("RoadHarbor"), FVector(-1900.0, -500.0, 90.0), FVector(42.0, 2.5, 0.25), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Road"), true);
	AddBox(TEXT("RoadSpine"), FVector(-3200.0, 700.0, 90.0), FVector(2.5, 52.0, 0.25), FRotator::ZeroRotator,
		TEXT("Hansa.World.Surface.Road"), true);
}

void AHansaLubeckWorldFoundation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	PlaceholderMaterials.Reset();

	for (UStaticMeshComponent* Component : TopologyComponents)
	{
		if (Component == nullptr || PlaceholderBaseMaterial == nullptr || Component->ComponentTags.IsEmpty())
		{
			continue;
		}

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

FTransform AHansaLubeckWorldFoundation::GetAutomationStartTransform() const
{
	return AutomationStart != nullptr ? AutomationStart->GetComponentTransform() :
		Hansa::Game::LubeckMap::AutomationStartTransform() * GetActorTransform();
}

FVector2D AHansaLubeckWorldFoundation::GetCameraBoundsMin() const
{
	const FVector Origin = CameraBounds->Bounds.Origin - CameraBounds->Bounds.BoxExtent;
	return FVector2D(Origin.X, Origin.Y);
}

FVector2D AHansaLubeckWorldFoundation::GetCameraBoundsMax() const
{
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
	return Coordinate.X >= 0 && Coordinate.X < Hansa::Game::LubeckPlacementGrid::WidthCells &&
		Coordinate.Y >= 0 && Coordinate.Y < Hansa::Game::LubeckPlacementGrid::HeightCells;
}

FVector AHansaLubeckWorldFoundation::PlacementCellToWorld(
	const int32 X,
	const int32 Y,
	const float Height) const
{
	return GetActorTransform().TransformPosition(
		Hansa::Game::LubeckPlacementGrid::GridToWorld({ X, Y }, Height));
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
