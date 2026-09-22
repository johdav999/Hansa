#include "World/HansaBuildingWorldProjection.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "World/HansaCompoundPresentation.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaRoadSplineComponent.h"
#include "World/HansaRoadRuns.h"
#include "World/HansaGrainFarmPresentation.h"
#include "World/HansaBakeryPresentation.h"
#include "World/HansaLumberCampPresentation.h"
#include "World/HansaSawmillPresentation.h"
#include "World/HansaResidencePresentation.h"
#include "World/HansaMarketPresentation.h"
#include "World/HansaWarehousePresentation.h"
#include "World/HansaHarborPresentation.h"
#include "World/HansaRoadTopology.h"
#include "World/HansaRoadPresentation.h"
#include "EngineUtils.h"
#include "HansaLog.h"

#include "Components/SceneComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaDefinitionBase.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "LandscapeProxy.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "UI/HansaBuildMenuPresentationModel.h"

namespace
{
	FLinearColor HansaColor(const TCHAR* Hex)
	{
		return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
	}

	double RotationYaw(const Hansa::Simulation::EHansaGridRotation Rotation)
	{
		using namespace Hansa::Simulation;
		switch (Rotation)
		{
		case EHansaGridRotation::East: return 90.0;
		case EHansaGridRotation::South: return 180.0;
		case EHansaGridRotation::West: return 270.0;
		case EHansaGridRotation::North:
		default: return 0.0;
		}
	}

	void ConfigurePresentationComponent(UStaticMeshComponent& Component)
	{
		Component.SetMobility(EComponentMobility::Movable);
		Component.SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component.SetGenerateOverlapEvents(false);
		Component.SetCanEverAffectNavigation(false);
		Component.CastShadow = false;
	}
}

namespace Hansa::Game
{
	bool FHansaPlacementProjectionRegistry::Reconcile(
		const TConstArrayView<Simulation::FHansaBuildingWorldProjection> Projections,
		FHansaPlacementProjectionDelta& OutDelta)
	{
		OutDelta = {};
		TArray<Simulation::FHansaBuildingWorldProjection> Canonical;
		Canonical.Append(Projections);
		Canonical.Sort([](
			const Simulation::FHansaBuildingWorldProjection& Left,
			const Simulation::FHansaBuildingWorldProjection& Right)
		{
			return Left.BuildingId < Right.BuildingId;
		});

		TMap<Simulation::FHansaBuildingId, Simulation::FHansaBuildingWorldProjection> Candidate;
		Candidate.Reserve(Canonical.Num());
		Simulation::FHansaBuildingId PreviousId;
		for (const Simulation::FHansaBuildingWorldProjection& Projection : Canonical)
		{
			if (!Projection.BuildingId.IsValid() || !Projection.OwnerId.IsValid() ||
				!Projection.Placement.CityId.IsValid() || !Projection.Placement.BuildingDefinitionId.IsValid() ||
				Projection.OccupiedCells.IsEmpty() || Projection.FootprintWidthCells <= 0 ||
				Projection.FootprintHeightCells <= 0 || !Projection.ConstructionProgress.IsNormalized() ||
				(PreviousId.IsValid() && PreviousId == Projection.BuildingId))
			{
				return false;
			}
			PreviousId = Projection.BuildingId;
			Candidate.Add(Projection.BuildingId, Projection);
		}

		for (const Simulation::FHansaBuildingWorldProjection& Projection : Canonical)
		{
			const Simulation::FHansaBuildingWorldProjection* Existing = Entries.Find(Projection.BuildingId);
			if (Existing == nullptr)
			{
				OutDelta.Created.Add(Projection.BuildingId);
			}
			else if (*Existing != Projection)
			{
				OutDelta.Updated.Add(Projection.BuildingId);
			}
		}
		for (const TPair<Simulation::FHansaBuildingId, Simulation::FHansaBuildingWorldProjection>& Entry : Entries)
		{
			if (!Candidate.Contains(Entry.Key))
			{
				OutDelta.Removed.Add(Entry.Key);
			}
		}
		OutDelta.Removed.Sort();
		Entries = MoveTemp(Candidate);
		return true;
	}

	void FHansaPlacementProjectionRegistry::Reset()
	{
		Entries.Reset();
	}

	const Simulation::FHansaBuildingWorldProjection* FHansaPlacementProjectionRegistry::Find(
		const Simulation::FHansaBuildingId BuildingId) const
	{
		return Entries.Find(BuildingId);
	}

	TArray<Simulation::FHansaBuildingId> FHansaPlacementProjectionRegistry::GetCanonicalIds() const
	{
		TArray<Simulation::FHansaBuildingId> Result;
		Entries.GetKeys(Result);
		Result.Sort();
		return Result;
	}
}

AHansaBuildingWorldProjectionActor::AHansaBuildingWorldProjectionActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
	SceneRoot->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadDisconnectedFinder(
		TEXT("/Game/Mesh/hansa-road-disconnected-symbol/SM_HansaRoadDisconnectedSymbol.SM_HansaRoadDisconnectedSymbol"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	ConeMesh = ConeFinder.Object;
	SphereMesh = SphereFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
	BuildingMesh->SetupAttachment(SceneRoot);
	BuildingMesh->SetStaticMesh(CubeMesh);
	ConfigurePresentationComponent(*BuildingMesh);
	BuildingMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BuildingMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BuildingMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BuildingMesh->ComponentTags.Add(TEXT("Hansa.Projection.Selectable"));

	BuildingPresentation = CreateDefaultSubobject<UChildActorComponent>(TEXT("BuildingPresentation"));
	BuildingPresentation->SetupAttachment(SceneRoot);
	BuildingPresentation->SetMobility(EComponentMobility::Movable);

	ConstructionPlaceholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConstructionPlaceholder"));
	ConstructionPlaceholder->SetupAttachment(SceneRoot);
	ConstructionPlaceholder->SetStaticMesh(CubeMesh);
	ConfigurePresentationComponent(*ConstructionPlaceholder);
	ConstructionPlaceholder->ComponentTags.Add(TEXT("Hansa.Projection.Construction"));

	SelectionOutline = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SelectionOutline"));
	SelectionOutline->SetupAttachment(SceneRoot);
	SelectionOutline->SetStaticMesh(CubeMesh);
	ConfigurePresentationComponent(*SelectionOutline);
	SelectionOutline->ComponentTags.Add(TEXT("Hansa.Projection.SelectionOutline"));
	SelectionOutline->SetVisibility(false, true);

	SelectionCornerSegments.Reserve(8);
	for (int32 SegmentIndex = 0; SegmentIndex < 8; ++SegmentIndex)
	{
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(
			FName(*FString::Printf(TEXT("SelectionCorner_%d"), SegmentIndex)));
		Segment->SetupAttachment(SceneRoot);
		Segment->SetStaticMesh(CubeMesh);
		ConfigurePresentationComponent(*Segment);
		Segment->ComponentTags.Add(TEXT("Hansa.Projection.SelectionCorner"));
		Segment->SetVisibility(false, true);
		SelectionCornerSegments.Add(Segment);
	}

	StatusMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusMarker"));
	StatusMarker->SetupAttachment(SceneRoot);
	StatusMarker->SetStaticMesh(ConeMesh);
	ConfigurePresentationComponent(*StatusMarker);
	StatusMarker->ComponentTags.Add(TEXT("Hansa.Projection.StatusMarker"));

	RoadDisconnectedMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RoadDisconnectedMarker"));
	RoadDisconnectedMarker->SetupAttachment(SceneRoot);
	RoadDisconnectedMarker->SetStaticMesh(RoadDisconnectedFinder.Object);
	ConfigurePresentationComponent(*RoadDisconnectedMarker);
	RoadDisconnectedMarker->ComponentTags.Add(TEXT("Hansa.Projection.RoadDisconnected"));
	RoadDisconnectedMarker->SetVisibility(false, true);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MarketRangeFinder(
        TEXT("/Game/Mesh/hansa-market-range-symbol/SM_HansaMarketNotInRange.SM_HansaMarketNotInRange"));
    MarketNotInRangeMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MarketNotInRangeMarker"));
    MarketNotInRangeMarker->SetupAttachment(SceneRoot);
    MarketNotInRangeMarker->SetStaticMesh(MarketRangeFinder.Object);
    ConfigurePresentationComponent(*MarketNotInRangeMarker);
    MarketNotInRangeMarker->ComponentTags.Add(TEXT("Hansa.Projection.MarketNotInRange"));
    MarketNotInRangeMarker->SetVisibility(false, true);
}

void AHansaBuildingWorldProjectionActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
    bool bReducedMotion = false;
    if (GConfig) GConfig->GetBool(TEXT("Hansa.UI"), TEXT("ReducedMotion"), bReducedMotion, GGameUserSettingsIni);
    if (bReducedMotion) return;
    if (bMarketNotInRange && MarketNotInRangeMarker != nullptr && MarketNotInRangeMarker->IsVisible())
        MarketNotInRangeMarker->AddLocalRotation(FRotator(0.0, RoadDisconnectedRotationDegreesPerSecond * DeltaSeconds, 0.0));
	if (!bRoadDisconnected || RoadDisconnectedMarker == nullptr || !RoadDisconnectedMarker->IsVisible())
	{
		return;
	}
	RoadDisconnectedMarker->AddLocalRotation(
		FRotator(0.0, RoadDisconnectedRotationDegreesPerSecond * DeltaSeconds, 0.0));
}

void AHansaBuildingWorldProjectionActor::EnsureMaterials()
{
	if (!DynamicMaterials.IsEmpty() || BaseMaterial == nullptr)
	{
		return;
	}
	for (UStaticMeshComponent* Component : { BuildingMesh, ConstructionPlaceholder, SelectionOutline, StatusMarker })
	{
		if (Component == BuildingMesh && BuildingMesh->GetStaticMesh() != CubeMesh)
		{
			DynamicMaterials.Add(nullptr);
			continue;
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (Material != nullptr)
		{
			Component->SetMaterial(0, Material);
			DynamicMaterials.Add(Material);
		}
	}

	if (DynamicMaterials.Num() == 4)
	{
		for (UStaticMeshComponent* Segment : SelectionCornerSegments)
		{
			Segment->SetMaterial(0, DynamicMaterials[2]);
		}
	}
}

void AHansaBuildingWorldProjectionActor::ApplyProjection(
	const Hansa::Simulation::FHansaBuildingWorldProjection& Projection,
	const AHansaLubeckWorldFoundation& Foundation, const uint8 RoadNeighborMask)
{
	using namespace Hansa::Simulation;
	BuildingId = Projection.BuildingId;
	BuildingDefinitionId = Projection.Placement.BuildingDefinitionId.ToString();
	WorldStatus = Projection.Status;
	ProductionBlocker = Projection.ProductionBlocker;
	bRoad = BuildingDefinitionId == TEXT("Building.Road");
	bRoadDisconnected = Projection.Status != EHansaBuildingWorldStatus::UnderConstruction &&
		Projection.bRequiresRoad && !Projection.bHasRoadAccess;
    bMarketNotInRange = !bRoad && Projection.Status != EHansaBuildingWorldStatus::UnderConstruction &&
        Projection.bRequiresRoad && !Projection.bHasMarketAccess;
	UE_LOG(LogHansa, Verbose,
		TEXT("[RoadConnectivity] marker building=%llu definition=%s status=%s roadRequired=%d ")
		TEXT("roadConnected=%d roadFailure=%s marketConnected=%d marketFailure=%s markerVisible=%d"),
		static_cast<unsigned long long>(Projection.BuildingId.GetValue()),
		*BuildingDefinitionId,
		LexToString(Projection.Status),
		Projection.bRequiresRoad,
		Projection.bHasRoadAccess,
		LexToString(Projection.RoadAccessFailure),
		Projection.bHasMarketAccess,
		LexToString(Projection.MarketAccessFailure),
		bRoadDisconnected);
	if (PresentationDefinition == nullptr || PresentationDefinition->StableDefinitionId != BuildingDefinitionId)
	{
		PresentationDefinition = UHansaDefinitionBase::ResolveByStableId(BuildingDefinitionId);
	}
	const UHansaBuildingDefinition* BuildingDefinition = Cast<UHansaBuildingDefinition>(PresentationDefinition);
	UClass* PresentationClass = BuildingDefinition != nullptr ? BuildingDefinition->LoadPresentationActorClass() : nullptr;
	if (BuildingPresentation->GetChildActorClass() != PresentationClass)
	{
		BuildingPresentation->SetRelativeTransform(FTransform::Identity);
		BuildingPresentation->SetChildActorClass(PresentationClass);
		PresentationBounds = FBox(ForceInit);
		if (AActor* Child = BuildingPresentation->GetChildActor())
		{
			PresentationBounds = Child->CalculateComponentsBoundingBoxInLocalSpace(true, true);
			// The managed projection owns selection and navigation. Child art cannot intercept clicks.
			TArray<UPrimitiveComponent*> Primitives;
			Child->GetComponents<UPrimitiveComponent>(Primitives, true);
			for (UPrimitiveComponent* Primitive : Primitives)
			{
				// A mesh-default external collision profile can override SetCollisionEnabled.
                // Assigning the profile also clears UStaticMeshComponent default inheritance.
                Primitive->SetCollisionProfileName(TEXT("NoCollision"), false);
                Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Primitive->SetGenerateOverlapEvents(false);
				Primitive->SetCanEverAffectNavigation(false);
			}
		}
	}
	if (auto* Road = Cast<AHansaRoadPresentation>(BuildingPresentation->GetChildActor()))
	{
		Road->ApplyNeighbors(RoadNeighborMask);
		PresentationBounds = Road->CalculateComponentsBoundingBoxInLocalSpace(true, true);
	}
	if (auto* Residence = Cast<AHansaResidencePresentation>(BuildingPresentation->GetChildActor()))
	{
		Residence->ApplyParcel(Projection.Placement.Anchor.X, Projection.Placement.Anchor.Y);
		PresentationBounds = Residence->CalculateComponentsBoundingBoxInLocalSpace(true, true);
	}
 if(auto* Compound=Cast<AHansaCompoundPresentation>(BuildingPresentation->GetChildActor()))
 {
  auto* D=BuildingDefinition?BuildingDefinition->LoadResidentialCompound():nullptr;
  FName Context=TEXT("Straight");
  const uint8 Turn=static_cast<uint8>(Projection.Placement.Rotation);
  const bool Left=(Projection.AdjacentRoadMask & (1u<<((Turn+3)%4)))!=0;
  const bool Right=(Projection.AdjacentRoadMask & (1u<<((Turn+1)%4)))!=0;
  auto Eligible=[&](FName C){return D&&D->Layouts.ContainsByPredicate([&](const FHansaCompoundLayout& L){return L.Context==C&&L.DevelopmentStage==BuildingDefinition->CompoundStage&&(L.DistrictIds.IsEmpty()||L.DistrictIds.Contains(BuildingDefinition->CompoundDistrictId));});};
  if(Left&&Eligible(TEXT("CornerLeft")))Context=TEXT("CornerLeft");
  else if(Right&&Eligible(TEXT("CornerRight")))Context=TEXT("CornerRight");
  else if(Projection.bMapEdge&&Eligible(TEXT("Edge")))Context=TEXT("Edge");
  const bool FootprintMatches=D&&D->FootprintWidthCells==Projection.FootprintWidthCells&&D->FootprintHeightCells==Projection.FootprintHeightCells;
  Compound->ApplyCompound(FootprintMatches?D:nullptr,UHansaResidentialCompoundDefinition::ParcelSeed(Projection.Placement.CityId.ToString(),BuildingId.GetValue(),BuildingId.GetGeneration()),BuildingDefinition?BuildingDefinition->CompoundStage:1,Context,BuildingDefinition?BuildingDefinition->CompoundDistrictId:FString());
  PresentationBounds=Compound->GetParcelBounds();
 }
	const bool bAuthoredActor = BuildingPresentation->GetChildActor() != nullptr && PresentationBounds.IsValid;
	UStaticMesh* AuthoredMesh = PresentationDefinition != nullptr ? PresentationDefinition->LoadPresentationMesh() : nullptr;
	UStaticMesh* ResolvedMesh = !bAuthoredActor && AuthoredMesh != nullptr ? AuthoredMesh : CubeMesh.Get();
	const bool bAuthoredMesh = ResolvedMesh != CubeMesh;
	const bool bAuthoredVisual = bAuthoredMesh || bAuthoredActor;
	const FBox VisualBounds = bAuthoredActor ? PresentationBounds : ResolvedMesh->GetBoundingBox();
	if (BuildingMesh->GetStaticMesh() != ResolvedMesh)
	{
		BuildingMesh->EmptyOverrideMaterials();
		DynamicMaterials.Reset();
		BuildingMesh->SetStaticMesh(ResolvedMesh);
	}
	BuildingMesh->SetCastShadow(bAuthoredMesh);

	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = MIN_int32;
	int32 MaxY = MIN_int32;
	for (const FHansaGridCoordinate Cell : Projection.OccupiedCells)
	{
		MinX = FMath::Min(MinX, Cell.X);
		MinY = FMath::Min(MinY, Cell.Y);
		MaxX = FMath::Max(MaxX, Cell.X);
		MaxY = FMath::Max(MaxY, Cell.Y);
	}

	const double Width = FMath::Max(1, Projection.FootprintWidthCells) *
		Hansa::Game::LubeckPlacementGrid::CellSize - 40.0;
	const double Depth = FMath::Max(1, Projection.FootprintHeightCells) *
		Hansa::Game::LubeckPlacementGrid::CellSize - 40.0;
	FVector MeshCenter = FVector::ZeroVector;
	if (bAuthoredVisual)
	{
		const FBox Bounds = VisualBounds;
		MeshCenter = Bounds.GetCenter();
	}
	BuildingMesh->SetRelativeLocation(bAuthoredMesh ? FVector(-MeshCenter.X, -MeshCenter.Y, 0.0) : FVector::ZeroVector);
	double Height = bRoad ? 20.0 : 320.0;
	double MeshBottom = 0.0;
	if (bAuthoredVisual)
	{
		const FBox MeshBounds = VisualBounds;
		Height = MeshBounds.GetSize().Z;
		MeshBottom = MeshBounds.Min.Z;
	}
	const FVector FirstCenter = Hansa::Game::LubeckPlacementGrid::GridToWorld({ MinX, MinY });
	const FVector LastCenter = Hansa::Game::LubeckPlacementGrid::GridToWorld({ MaxX, MaxY });
	FVector LocalCenter = (FirstCenter + LastCenter) * 0.5;
	const bool bHarborDeckDatum = Cast<AHansaHarborPresentation>(BuildingPresentation->GetChildActor()) != nullptr ||
		Cast<AHansaRoadPresentation>(BuildingPresentation->GetChildActor()) != nullptr;
	LocalCenter.Z = bRoad && bAuthoredActor ? AHansaRoadPresentation::GroundBaseHeight() :
		bHarborDeckDatum ? 100.0 : bAuthoredVisual ? 100.0 - MeshBottom : 100.0 + Height * 0.5;
	const FTransform LocalTransform(
		FRotator(0.0, bRoad ? 0.0 : RotationYaw(Projection.Placement.Rotation), 0.0), LocalCenter);
	FTransform GroundedTransform = LocalTransform * Foundation.GetActorTransform();
    GroundedTransform.SetLocation(Foundation.GroundPlacementPosition(GroundedTransform.GetLocation(),
        bRoad && bAuthoredActor ? AHansaRoadPresentation::GroundBaseHeight() : 100.0));
    if (bRoad && !bAuthoredActor) GroundedTransform.SetRotation(Hansa::Game::TerrainPlacement::RoadRotation(
        GetWorld(), GroundedTransform.GetLocation(), GroundedTransform.GetRotation()));
    SetActorTransform(GroundedTransform);
	if (auto* Road=Cast<AHansaRoadPresentation>(BuildingPresentation->GetChildActor())) Road->ApplyGround(Foundation);

	BuildingMesh->SetRelativeScale3D(bAuthoredMesh
		? FVector::OneVector
		: FVector(Width / 100.0, Depth / 100.0, Height / 100.0));

	if (bAuthoredActor)
	{
		BuildingPresentation->SetRelativeLocation(bHarborDeckDatum ? FVector::ZeroVector : FVector(-MeshCenter.X, -MeshCenter.Y, 0.0));
		BuildingPresentation->SetRelativeScale3D(FVector::OneVector);
		// An invisible box remains a stable click target throughout the animation.
		BuildingMesh->SetRelativeLocation(FVector(bRoad ? MeshCenter.X : 0.0,
			bRoad ? MeshCenter.Y : 0.0, MeshBottom + Height * 0.5));
		BuildingMesh->SetRelativeScale3D(VisualBounds.GetSize() / 100.0);
	}
 if(auto* Compound=Cast<AHansaCompoundPresentation>(BuildingPresentation->GetChildActor()))
  Compound->FitGround(true,(Projection.AdjacentRoadMask&(1u<<static_cast<uint8>(Projection.Placement.Rotation)))!=0);
	const double PlaceholderHeight = bRoad ? 12.0 : 80.0;
	ConstructionPlaceholder->SetRelativeLocation(FVector(
		0.0, 0.0, bAuthoredVisual ? MeshBottom + PlaceholderHeight * 0.5 : -(Height - PlaceholderHeight) * 0.5));
	ConstructionPlaceholder->SetRelativeScale3D(
		FVector(Width / 100.0, Depth / 100.0, PlaceholderHeight / 100.0));
	const double SelectionGroundZ = bRoad && bAuthoredActor ? 13.0 :
		bHarborDeckDatum ? -3.0 : bAuthoredVisual ? MeshBottom - 3.0 : -Height * 0.5 - 3.0;
	ConfigureSelectionFootprint(Width, Depth, SelectionGroundZ);
	StatusMarker->SetRelativeLocation(
		FVector(0.0, 0.0, bAuthoredVisual ? MeshBottom + Height + 100.0 : Height * 0.5 + 100.0));
	StatusMarker->SetRelativeScale3D(FVector(0.45, 0.45, 0.8));
	RoadDisconnectedMarker->SetRelativeLocation(
		FVector(0.0, 0.0, bAuthoredVisual ? MeshBottom + Height + 250.0 : Height * 0.5 + 250.0));
	RoadDisconnectedMarker->SetRelativeScale3D(FVector(2.25));
    MarketNotInRangeMarker->SetRelativeLocation(RoadDisconnectedMarker->GetRelativeLocation() +
        FVector(bRoadDisconnected ? 360.0 : 0.0, 0.0, 0.0));
    MarketNotInRangeMarker->SetRelativeScale3D(FVector(2.25));

	Tags.Reset();
	Tags.Add(TEXT("Hansa.Projection.Building"));
	Tags.Add(FName(*FString::Printf(TEXT("Hansa.Entity.Building.%llu.%u"),
		static_cast<unsigned long long>(BuildingId.GetValue()), BuildingId.GetGeneration())));
	Tags.Add(FName(*FString::Printf(TEXT("Hansa.Definition.%s"), *BuildingDefinitionId)));
	Tags.Add(FName(*FString::Printf(TEXT("Hansa.Status.%s"), LexToString(WorldStatus))));
	if (bRoad)
	{
		Tags.Add(TEXT("Hansa.Projection.Road"));
	}
	if (bRoadDisconnected)
	{
		Tags.Add(TEXT("Hansa.Status.RoadDisconnected"));
	}
    if (bMarketNotInRange) Tags.Add(TEXT("Hansa.Status.MarketNotInRange"));
	EnsureMaterials();
	ApplyVisualState();
}

void AHansaBuildingWorldProjectionActor::SetSelected(const bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		SelectionOutline->SetVisibility(bSelected, true);
		for (UStaticMeshComponent* Segment : SelectionCornerSegments)
		{
			Segment->SetVisibility(bSelected, true);
		}
		return;
	}
	bSelected = bInSelected;
	// Selection uses footprint markers only. Expanded mesh shells bleed through detailed/open
	// geometry (notably roof tiles), recolouring the authored building surface.
	SelectionOutline->SetVisibility(bSelected, true);
	for (UStaticMeshComponent* Segment : SelectionCornerSegments)
	{
		Segment->SetVisibility(bSelected, true);
	}
	SetSelectionDepthEnabled(bSelected);
}

void AHansaBuildingWorldProjectionActor::ConfigureSelectionFootprint(
	const double Width, const double Depth, const double GroundZ)
{
	const double HalfWidth = (Width + 40.0) * 0.5;
	const double HalfDepth = (Depth + 40.0) * 0.5;
	const double BracketLength = FMath::Clamp(FMath::Min(Width, Depth) * 0.18, 70.0, 140.0);
	const double BracketThickness = 12.0;
	const double MarkerHeight = 6.0;
	// The building is grounded at its centre, but its footprint may cross a slope.
	// Lift each complete L above both bars' terrain samples, including their edges.
	// Keep the authored datum as a floor for decks and worlds without terrain.
	const auto TerrainLift = [this](const UStaticMeshComponent* Marker)
	{
		double Lift = 0.0;
		for (const double X : { -50.0, 0.0, 50.0 })
		{
			for (const double Y : { -50.0, 0.0, 50.0 })
			{
				const FVector Bottom = Marker->GetComponentTransform().TransformPosition(FVector(X, Y, -50.0));
				FHitResult Hit;
				if (Hansa::Game::TerrainPlacement::Trace(GetWorld(), Bottom + FVector(0, 0, 1000000),
					Bottom - FVector(0, 0, 1000000), Hit))
				{
					Lift = FMath::Max(Lift, Hit.ImpactPoint.Z + 2.0 - Bottom.Z);
				}
			}
		}
		return Lift;
	};

	int32 SegmentIndex = 0;
	for (const double XSign : { -1.0, 1.0 })
	{
		for (const double YSign : { -1.0, 1.0 })
		{
			UStaticMeshComponent* Horizontal = SelectionCornerSegments[SegmentIndex++];
			Horizontal->SetRelativeLocation(FVector(
				XSign * (HalfWidth - BracketLength * 0.5), YSign * HalfDepth, GroundZ));
			Horizontal->SetRelativeRotation(FRotator::ZeroRotator);
			Horizontal->SetRelativeScale3D(FVector(
				BracketLength / 100.0, BracketThickness / 100.0, MarkerHeight / 100.0));

			UStaticMeshComponent* Vertical = SelectionCornerSegments[SegmentIndex++];
			Vertical->SetRelativeLocation(FVector(
				XSign * HalfWidth, YSign * (HalfDepth - BracketLength * 0.5), GroundZ));
			Vertical->SetRelativeRotation(FRotator::ZeroRotator);
			Vertical->SetRelativeScale3D(FVector(
				BracketThickness / 100.0, BracketLength / 100.0, MarkerHeight / 100.0));
			const FVector Lift(0.0, 0.0, FMath::Max(TerrainLift(Horizontal), TerrainLift(Vertical)));
			Horizontal->AddWorldOffset(Lift);
			Vertical->AddWorldOffset(Lift);
		}
	}

	// SelectionOutline remains the stable public/test component, but is now the small
	// non-colour diamond cue from the approved reference rather than an opaque slab.
	SelectionOutline->SetRelativeLocation(FVector(0.0, -HalfDepth - 48.0, GroundZ));
	SelectionOutline->SetRelativeRotation(FRotator(0.0, 45.0, 0.0));
	SelectionOutline->SetRelativeScale3D(FVector(0.34, 0.34, MarkerHeight / 100.0));
	SelectionOutline->AddWorldOffset(FVector(0.0, 0.0, TerrainLift(SelectionOutline)));

}

void AHansaBuildingWorldProjectionActor::SetSelectionDepthEnabled(const bool bEnabled)
{
	TArray<UPrimitiveComponent*> Primitives;
	if (BuildingMesh != nullptr)
	{
		Primitives.Add(BuildingMesh);
	}
	if (ConstructionPlaceholder != nullptr)
	{
		Primitives.Add(ConstructionPlaceholder);
	}
	if (AActor* Presentation = BuildingPresentation != nullptr ? BuildingPresentation->GetChildActor() : nullptr)
	{
		TArray<UPrimitiveComponent*> PresentationPrimitives;
		Presentation->GetComponents<UPrimitiveComponent>(PresentationPrimitives, true);
		Primitives.Append(PresentationPrimitives);
	}
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive == nullptr)
		{
			continue;
		}
		Primitive->SetRenderCustomDepth(bEnabled);
		if (bEnabled)
		{
			Primitive->SetCustomDepthStencilValue(1);
		}
	}
}

FName AHansaBuildingWorldProjectionActor::GetStatusName() const
{
	return FName(Hansa::Simulation::LexToString(WorldStatus));
}

void AHansaBuildingWorldProjectionActor::ApplyVisualState()
{
	using namespace Hansa::Simulation;
	const bool bConstructing = WorldStatus == EHansaBuildingWorldStatus::UnderConstruction;
	const bool bBlocked = WorldStatus == EHansaBuildingWorldStatus::Blocked;
	AActor* Presentation = BuildingPresentation->GetChildActor();
	AHansaGrainFarmPresentation* GrainFarm = Cast<AHansaGrainFarmPresentation>(Presentation);
	AHansaBakeryPresentation* Bakery = Cast<AHansaBakeryPresentation>(Presentation);
	AHansaLumberCampPresentation* LumberCamp = Cast<AHansaLumberCampPresentation>(Presentation);
	AHansaSawmillPresentation* Sawmill = Cast<AHansaSawmillPresentation>(Presentation);
	AHansaResidencePresentation* Residence = Cast<AHansaResidencePresentation>(Presentation);
	AHansaMarketPresentation* Market = Cast<AHansaMarketPresentation>(Presentation);
	AHansaWarehousePresentation* Warehouse = Cast<AHansaWarehousePresentation>(Presentation);
	AHansaHarborPresentation* Harbor = Cast<AHansaHarborPresentation>(Presentation);
	AHansaRoadPresentation* Road = Cast<AHansaRoadPresentation>(Presentation);
	BuildingMesh->SetVisibility(!bConstructing && (Presentation == nullptr || !PresentationBounds.IsValid), true);
	if (Presentation != nullptr)
	{
		Presentation->SetActorHiddenInGame(!PresentationBounds.IsValid || (bConstructing && GrainFarm == nullptr && Bakery == nullptr && LumberCamp == nullptr && Sawmill == nullptr && Residence == nullptr && Market == nullptr && Warehouse == nullptr && Harbor == nullptr && Road == nullptr));
		if (GrainFarm != nullptr) GrainFarm->ApplyStatus(WorldStatus);
		if (Bakery != nullptr) Bakery->ApplyStatus(WorldStatus);
		if (LumberCamp != nullptr) LumberCamp->ApplyStatus(WorldStatus);
		if (Sawmill != nullptr) Sawmill->ApplyStatus(WorldStatus);
		if (Market != nullptr) Market->ApplyStatus(WorldStatus);
		if (Warehouse != nullptr) Warehouse->ApplyStatus(WorldStatus);
		if (Harbor != nullptr) Harbor->ApplyStatus(WorldStatus);
		TArray<AActor*> PresentationChildren;
		Presentation->GetAllChildActors(PresentationChildren, true);
		for (AActor* Child : PresentationChildren)
		{
			Child->SetActorHiddenInGame(bConstructing);
			// Transform/construction-script refreshes can recreate nested Blueprint
			// components with their defaults. Reassert projection ownership each time.
			TArray<UPrimitiveComponent*> ChildPrimitives;
			Child->GetComponents<UPrimitiveComponent>(ChildPrimitives);
			for (UPrimitiveComponent* Primitive : ChildPrimitives)
			{
				// A mesh-default external collision profile can override SetCollisionEnabled.
                // Assigning the profile also clears UStaticMeshComponent default inheritance.
                Primitive->SetCollisionProfileName(TEXT("NoCollision"), false);
                Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Primitive->SetGenerateOverlapEvents(false);
				Primitive->SetCanEverAffectNavigation(false);
			}
			// Mechanical animation remains a read-only visual projection. Preserve phase
			// and the authored component rate; never recreate the rotor on a status update.
			if (BuildingDefinitionId == TEXT("Building.Mill"))
			{
				if (URotatingMovementComponent* Movement = Child->FindComponentByClass<URotatingMovementComponent>())
				{
					const URotatingMovementComponent* Defaults = Cast<URotatingMovementComponent>(Movement->GetArchetype());
					Movement->RotationRate = !bConstructing && !bBlocked && Defaults != nullptr
                        ? Defaults->RotationRate : FRotator::ZeroRotator;
                    Movement->SetComponentTickEnabled(false);
				}
			}
		}
	}
	ConstructionPlaceholder->SetVisibility(bConstructing && GrainFarm == nullptr && Bakery == nullptr && LumberCamp == nullptr && Sawmill == nullptr && Residence == nullptr && Market == nullptr && Warehouse == nullptr && Harbor == nullptr && Road == nullptr, true);
	SelectionOutline->SetVisibility(bSelected, true);
	for (UStaticMeshComponent* Segment : SelectionCornerSegments)
	{
		Segment->SetVisibility(bSelected, true);
	}
	StatusMarker->SetVisibility(bConstructing || bBlocked, true);
	StatusMarker->SetStaticMesh(bBlocked ? SphereMesh : ConeMesh);
	RoadDisconnectedMarker->SetVisibility(bRoadDisconnected, true);
	MarketNotInRangeMarker->SetVisibility(bMarketNotInRange, true);
	SetActorTickEnabled(bRoadDisconnected || bMarketNotInRange);

	if (DynamicMaterials.Num() == 4)
	{
		const FLinearColor BodyColor = bRoad ? HansaColor(TEXT("795137")) : HansaColor(TEXT("A44C3F"));
		if (DynamicMaterials[0] != nullptr)
		{
			DynamicMaterials[0]->SetVectorParameterValue(TEXT("Color"), BodyColor);
		}
		DynamicMaterials[1]->SetVectorParameterValue(TEXT("Color"), HansaColor(TEXT("D09132")));
		DynamicMaterials[2]->SetVectorParameterValue(TEXT("Color"), HansaColor(TEXT("C19A52")));
		DynamicMaterials[3]->SetVectorParameterValue(
			TEXT("Color"), bBlocked ? HansaColor(TEXT("762F32")) : HansaColor(TEXT("D09132")));
	}
	SetSelectionDepthEnabled(bSelected);
}

AHansaBuildingPlacementGhost::AHansaBuildingPlacementGhost()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
	SceneRoot->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = CubeFinder.Object;
	BaseMaterial = MaterialFinder.Object;

	BuildingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingGhost"));
	BuildingMesh->SetupAttachment(SceneRoot);
	ConfigurePresentationComponent(*BuildingMesh);
	BuildingPresentation = CreateDefaultSubobject<UChildActorComponent>(TEXT("AuthoredBuildingGhost"));
	BuildingPresentation->SetupAttachment(SceneRoot);
	BuildingPresentation->SetMobility(EComponentMobility::Movable);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Outline = CreateDefaultSubobject<UStaticMeshComponent>(
			*FString::Printf(TEXT("FootprintOutline%d"), Index));
		Outline->SetupAttachment(SceneRoot);
		Outline->SetStaticMesh(CubeMesh);
		ConfigurePresentationComponent(*Outline);
		OutlineMeshes.Add(Outline);
	}

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("PlacementStatus"));
	StatusText->SetupAttachment(SceneRoot);
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextCenter);
	StatusText->SetWorldSize(54.0f);
	StatusText->SetTextRenderColor(FColor::White);
	StatusText->SetCastShadow(false);
	StatusText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tags.Add(TEXT("Hansa.Placement.Ghost"));
    TerrainFeedbackMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainFeedback"));
    TerrainFeedbackMesh->SetupAttachment(SceneRoot);
    TerrainFeedbackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TerrainFeedbackMesh->SetCanEverAffectNavigation(false);
    TerrainFeedbackMesh->SetCastShadow(false);
	SetActorHiddenInGame(true);
}

UStaticMeshComponent* AHansaBuildingPlacementGhost::AcquireFootprintCell(const int32 Index)
{
	while (FootprintCellMeshes.Num() <= Index)
	{
		UStaticMeshComponent* Cell = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("FootprintCell%d"), FootprintCellMeshes.Num()));
		Cell->SetupAttachment(SceneRoot);
		Cell->SetStaticMesh(CubeMesh);
		ConfigurePresentationComponent(*Cell);
		Cell->RegisterComponent();
		FootprintCellMeshes.Add(Cell);
	}
	return FootprintCellMeshes[Index];
}

UStaticMeshComponent* AHansaBuildingPlacementGhost::AcquireRoadPiece(const int32 Index)
{
	while (RoadPieceMeshes.Num() <= Index)
	{
		UStaticMeshComponent* Piece = NewObject<UHansaRoadSplineComponent>(this,
			*FString::Printf(TEXT("AuthoredRoadPreview%d"), RoadPieceMeshes.Num()));
		Piece->SetupAttachment(SceneRoot);
		ConfigurePresentationComponent(*Piece);
		Piece->RegisterComponent();
		RoadPieceMeshes.Add(Piece);
	}
	return RoadPieceMeshes[Index];
}

void AHansaBuildingPlacementGhost::ApplyPreview(
	const FName BuildingDefinitionId,
	const FIntPoint AnchorCell,
	const int32 RotationQuarterTurns,
	const TConstArrayView<FIntPoint> FootprintCells,
	const EHansaPlacementFeedback Feedback,
	const FText& Reason,
	const AHansaLubeckWorldFoundation& Foundation,
	const bool bDeferRoadFeedback, const uint8 AdjacentRoadMask, const uint64 ParcelSeed)
{
	PreviewBuildingId = BuildingDefinitionId;
	ActiveRoadPieceCount = 0;
	for (UStaticMeshComponent* Piece : RoadPieceMeshes) Piece->SetVisibility(false, true);
	if (PresentationDefinition == nullptr || PresentationDefinition->StableDefinitionId != BuildingDefinitionId.ToString())
	{
		PresentationDefinition = UHansaDefinitionBase::ResolveByStableId(BuildingDefinitionId.ToString());
		PresentationBounds = FBox(ForceInit);
	}
	const UHansaBuildingDefinition* Definition = Cast<UHansaBuildingDefinition>(PresentationDefinition);
	UClass* PresentationClass = Definition != nullptr ? Definition->LoadPresentationActorClass() : nullptr;
	if (BuildingPresentation->GetChildActorClass() != PresentationClass)
	{
		BuildingPresentation->SetChildActorClass(PresentationClass);
		PresentationBounds = FBox(ForceInit);
		if (AActor* Child = BuildingPresentation->GetChildActor())
		{
			PresentationBounds = Child->CalculateComponentsBoundingBoxInLocalSpace(true, true);
			TArray<UPrimitiveComponent*> Primitives;
			Child->GetComponents<UPrimitiveComponent>(Primitives, true);
			for (UPrimitiveComponent* Primitive : Primitives)
			{
				// A mesh-default external collision profile can override SetCollisionEnabled.
                // Assigning the profile also clears UStaticMeshComponent default inheritance.
                Primitive->SetCollisionProfileName(TEXT("NoCollision"), false);
                Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Primitive->SetGenerateOverlapEvents(false);
				Primitive->SetCanEverAffectNavigation(false);
				Primitive->SetCastShadow(false);
			}
		}
	}

	if (auto* Residence = Cast<AHansaResidencePresentation>(BuildingPresentation->GetChildActor()))
	{
		Residence->ApplyParcel(AnchorCell.X, AnchorCell.Y);
		PresentationBounds = Residence->CalculateComponentsBoundingBoxInLocalSpace(true, true);
	}
 if(auto* Compound=Cast<AHansaCompoundPresentation>(BuildingPresentation->GetChildActor()))
 {
  auto* D=Definition?Definition->LoadResidentialCompound():nullptr;
  FName Context=TEXT("Straight");const uint8 Turn=RotationQuarterTurns%4;
  auto Eligible=[&](FName C){return D&&D->Layouts.ContainsByPredicate([&](const FHansaCompoundLayout& L){return L.Context==C&&L.DevelopmentStage==Definition->CompoundStage&&(L.DistrictIds.IsEmpty()||L.DistrictIds.Contains(Definition->CompoundDistrictId));});};
  if((AdjacentRoadMask&(1u<<((Turn+3)%4)))&&Eligible(TEXT("CornerLeft")))Context=TEXT("CornerLeft");
  else if((AdjacentRoadMask&(1u<<((Turn+1)%4)))&&Eligible(TEXT("CornerRight")))Context=TEXT("CornerRight");
  Compound->ApplyCompound(D,
   ParcelSeed ? ParcelSeed : UHansaResidentialCompoundDefinition::ParcelSeed(TEXT("PlacementPreview"),static_cast<uint64>(static_cast<uint32>(AnchorCell.X))<<32|static_cast<uint32>(AnchorCell.Y),0),
   Definition?Definition->CompoundStage:1,Context,Definition?Definition->CompoundDistrictId:FString());
  PresentationBounds=Compound->GetParcelBounds();
 }
	UStaticMesh* AuthoredMesh = PresentationDefinition != nullptr ? PresentationDefinition->LoadPresentationMesh() : nullptr;
	const bool bAuthoredActor = BuildingPresentation->GetChildActor() != nullptr && PresentationBounds.IsValid;
	BuildingMesh->SetStaticMesh(!bAuthoredActor && AuthoredMesh != nullptr ? AuthoredMesh : CubeMesh.Get());
	BuildingMesh->SetVisibility(!bAuthoredActor, true);
	BuildingPresentation->SetVisibility(bAuthoredActor, true);
	if (AActor* Child = BuildingPresentation->GetChildActor()) Child->SetActorHiddenInGame(!bAuthoredActor);

	TArray<FIntPoint> Cells;
	Cells.Append(FootprintCells);
	if (Cells.IsEmpty()) Cells.Add(AnchorCell);
	int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
	for (const FIntPoint Cell : Cells)
	{
		MinX = FMath::Min(MinX, Cell.X); MinY = FMath::Min(MinY, Cell.Y);
		MaxX = FMath::Max(MaxX, Cell.X); MaxY = FMath::Max(MaxY, Cell.Y);
	}
	const bool bRoadKitPreview=Cast<AHansaRoadPresentation>(BuildingPresentation->GetChildActor())!=nullptr;
	const float PreviewHeight=bRoadKitPreview ? AHansaRoadPresentation::GroundBaseHeight()+6 : 106;
	const FVector First = Hansa::Game::LubeckPlacementGrid::GridToWorld({MinX, MinY}, PreviewHeight);
    const FVector Last = Hansa::Game::LubeckPlacementGrid::GridToWorld({MaxX, MaxY}, PreviewHeight);
    const FVector Center = Foundation.GroundPlacementPosition(
        Foundation.GetActorTransform().TransformPosition((First + Last) * 0.5),
        bRoadKitPreview ? AHansaRoadPresentation::GroundBaseHeight() : 100.0);
	SetActorLocation(Center);
    SetActorRotation(Foundation.GetActorQuat());

	const double Width = (MaxX - MinX + 1) * Hansa::Game::LubeckPlacementGrid::CellSize - 40.0;
	const double Depth = (MaxY - MinY + 1) * Hansa::Game::LubeckPlacementGrid::CellSize - 40.0;
	const FBox VisualBounds = bAuthoredActor ? PresentationBounds : BuildingMesh->GetStaticMesh()->GetBoundingBox();
	const FVector VisualSize = VisualBounds.GetSize();
	const FVector VisualCenter = VisualBounds.GetCenter();
	const double VisualHeight = (AuthoredMesh != nullptr || bAuthoredActor) ? VisualSize.Z : 260.0;
	const double VisualBottom = (AuthoredMesh != nullptr || bAuthoredActor) ? VisualBounds.Min.Z : 0.0;
	const FRotator VisualRotation(0.0, (RotationQuarterTurns % 4) * 90.0, 0.0);
	BuildingMesh->SetRelativeRotation(VisualRotation);
	BuildingPresentation->SetRelativeRotation(VisualRotation);
	BuildingMesh->SetRelativeLocation(AuthoredMesh != nullptr
		? FVector(-VisualCenter.X, -VisualCenter.Y, -VisualBottom) : FVector(0.0, 0.0, VisualHeight * 0.5));
	BuildingMesh->SetRelativeScale3D(AuthoredMesh != nullptr
		? FVector::OneVector : FVector(Width / 100.0, Depth / 100.0, VisualHeight / 100.0));
	const bool bHarborDeckDatum = Cast<AHansaHarborPresentation>(BuildingPresentation->GetChildActor()) != nullptr ||
		Cast<AHansaRoadPresentation>(BuildingPresentation->GetChildActor()) != nullptr;
	BuildingPresentation->SetRelativeLocation(bHarborDeckDatum ? FVector::ZeroVector : FVector(-VisualCenter.X, -VisualCenter.Y, -VisualBottom));
	BuildingPresentation->SetRelativeScale3D(FVector::OneVector);
	if (auto* Road=Cast<AHansaRoadPresentation>(BuildingPresentation->GetChildActor())) Road->ApplyGround(Foundation);

 if(auto* Compound=Cast<AHansaCompoundPresentation>(BuildingPresentation->GetChildActor()))
  Compound->FitGround(false,(AdjacentRoadMask&(1u<<(RotationQuarterTurns%4)))!=0);
	ActiveFootprintCellCount = Cells.Num();
	TerrainFeedbackMesh->SetVisibility(false);
	for (UStaticMeshComponent* Outline : OutlineMeshes) Outline->SetVisibility(true);
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		UStaticMeshComponent* CellMesh = AcquireFootprintCell(Index);
		const FVector CellWorld = Foundation.PlacementCellToWorld(Cells[Index].X, Cells[Index].Y, bRoadKitPreview ? PreviewHeight-3 : 103.0f, bRoadKitPreview ? AHansaRoadPresentation::GroundBaseHeight() : 100.0f);
		CellMesh->SetRelativeLocation(GetActorTransform().InverseTransformPosition(CellWorld) + FVector(0.0, 0.0,
			Feedback == EHansaPlacementFeedback::Invalid && Index % 2 != 0 ? 8.0 : 0.0));
		const double CellScale = Feedback == EHansaPlacementFeedback::Warning && Index % 2 != 0 ? 3.35 : 3.72;
		CellMesh->SetRelativeScale3D(FVector(CellScale, CellScale, 0.06));
		CellMesh->SetRelativeRotation(FRotator::ZeroRotator);
		CellMesh->SetVisibility(true, true);
	}
	for (int32 Index = Cells.Num(); Index < FootprintCellMeshes.Num(); ++Index) FootprintCellMeshes[Index]->SetVisibility(false, true);

	const double HalfWidth = (MaxX - MinX + 1) * Hansa::Game::LubeckPlacementGrid::CellSize * 0.5;
	const double HalfDepth = (MaxY - MinY + 1) * Hansa::Game::LubeckPlacementGrid::CellSize * 0.5;
	const double Thickness = 22.0;
	OutlineMeshes[0]->SetRelativeLocation(FVector(0.0, HalfDepth, 12.0));
	OutlineMeshes[1]->SetRelativeLocation(FVector(0.0, -HalfDepth, 12.0));
	OutlineMeshes[2]->SetRelativeLocation(FVector(HalfWidth, 0.0, 12.0));
	OutlineMeshes[3]->SetRelativeLocation(FVector(-HalfWidth, 0.0, 12.0));
	OutlineMeshes[0]->SetRelativeScale3D(FVector(HalfWidth * 2.0 / 100.0, Thickness / 100.0, 0.12));
	OutlineMeshes[1]->SetRelativeScale3D(OutlineMeshes[0]->GetRelativeScale3D());
	OutlineMeshes[2]->SetRelativeScale3D(FVector(Thickness / 100.0, HalfDepth * 2.0 / 100.0, 0.12));
	OutlineMeshes[3]->SetRelativeScale3D(OutlineMeshes[2]->GetRelativeScale3D());
	StatusText->SetRelativeLocation(FVector(0.0, 0.0, FMath::Max(180.0, VisualHeight + 90.0)));
	ApplyFeedbackVisuals(Feedback, Reason);
	SetActorHiddenInGame(false);
	if (BuildingDefinitionId == TEXT("Building.Road") && !bDeferRoadFeedback) FitRoadFeedbackToTerrain(Foundation);
}

void AHansaBuildingPlacementGhost::ApplyRoadPreview(
	const TConstArrayView<FHansaRoadPreviewCell> Cells,
	const EHansaPlacementFeedback Feedback,
	const FText& Reason,
	const AHansaLubeckWorldFoundation& Foundation)
{
	TArray<FIntPoint> Coordinates;
	Coordinates.Reserve(Cells.Num());
	for (const FHansaRoadPreviewCell& Cell : Cells) Coordinates.Add(Cell.Cell);
	const FIntPoint Anchor = Coordinates.IsEmpty() ? FIntPoint::ZeroValue : Coordinates.Last();
	ApplyPreview(TEXT("Building.Road"), Anchor, 0, Coordinates, Feedback, Reason, Foundation, true);

	BuildingMesh->SetVisibility(false, true);
	BuildingPresentation->SetVisibility(false, true);
	if (AActor* Child = BuildingPresentation->GetChildActor()) Child->SetActorHiddenInGame(true);
	const UHansaBuildingDefinition* RoadDefinition = Cast<UHansaBuildingDefinition>(PresentationDefinition);
	UStaticMesh* RoadMesh = RoadDefinition != nullptr ? RoadDefinition->LoadPresentationMesh() : nullptr;
	const UClass* RoadClass = RoadDefinition ? RoadDefinition->LoadPresentationActorClass() : nullptr;
	const auto* RoadKit = RoadClass ? Cast<AHansaRoadPresentation>(RoadClass->GetDefaultObject()) : nullptr;
	const bool bAuthoredRoadMesh = RoadMesh != nullptr &&
		!RoadMesh->GetPathName().StartsWith(TEXT("/Engine/BasicShapes/"));
	auto ResolveRoadMaterial = [this](TObjectPtr<UMaterialInstanceDynamic>& Material, const FLinearColor Color)
	{
		if (Material == nullptr && BaseMaterial != nullptr)
		{
			Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (Material != nullptr) Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
		return Material.Get();
	};
	UMaterialInstanceDynamic* ValidMaterial = ResolveRoadMaterial(RoadValidMaterial, HansaColor(TEXT("1A7F75")));
	UMaterialInstanceDynamic* ExistingMaterial = ResolveRoadMaterial(RoadExistingMaterial, HansaColor(TEXT("397FA3")));
	UMaterialInstanceDynamic* InvalidMaterial = ResolveRoadMaterial(RoadInvalidMaterial, HansaColor(TEXT("762F32")));
	for (int32 Index = 0; Index < Cells.Num() && Index < FootprintCellMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* CellMesh = FootprintCellMeshes[Index];
		const EHansaRoadPreviewCellState State = Cells[Index].State;
		CellMesh->SetMaterial(0, State == EHansaRoadPreviewCellState::Invalid ? InvalidMaterial
			: State == EHansaRoadPreviewCellState::ExistingRoad ? ExistingMaterial : ValidMaterial);
		CellMesh->SetRelativeRotation(State == EHansaRoadPreviewCellState::ExistingRoad
			? FRotator(0.0f, 45.0f, 0.0f) : FRotator::ZeroRotator);
		const double Scale = State == EHansaRoadPreviewCellState::Invalid ? 3.20
			: State == EHansaRoadPreviewCellState::ExistingRoad ? 2.75 : 3.72;
		CellMesh->SetRelativeScale3D(FVector(Scale, Scale, State == EHansaRoadPreviewCellState::Invalid ? 0.12 : 0.06));
		if (State == EHansaRoadPreviewCellState::Invalid)
		{
			CellMesh->AddRelativeLocation(FVector(0.0, 0.0, Index % 2 == 0 ? 10.0 : 22.0));
		}
	}
	ActiveRoadPieceCount = 0;
	if (RoadKit || bAuthoredRoadMesh)
	{
		// Connectivity depends on coordinates, not array order or merely sharing an axis.
		TSet<FIntPoint> RoadCoordinates;
		for (TActorIterator<AHansaPlacementProjectionManager> It(GetWorld()); It; ++It)
			if (It->IsBoundTo(Foundation)) RoadCoordinates.Append(It->GetRoadCells());
		for (const auto& Cell : Cells)
			if (Cell.State != EHansaRoadPreviewCellState::Invalid) RoadCoordinates.Add(Cell.Cell);
		TArray<FIntPoint> DisplayCells = Coordinates;
		if (RoadKit)
			for (const auto& Cell : Cells)
				if (Cell.State != EHansaRoadPreviewCellState::Invalid)
					for (const FIntPoint Offset : {FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)})
						if (RoadCoordinates.Contains(Cell.Cell+Offset)) DisplayCells.AddUnique(Cell.Cell+Offset);
		for (int32 Index = 0; Index < DisplayCells.Num(); ++Index)
		{
			// Invalid cells retain the explicit footprint feedback, never an apparently connected road.
			if (Index < Cells.Num() && Cells[Index].State == EHansaRoadPreviewCellState::Invalid) continue;
			if (!RoadKit && Index < Cells.Num() && Cells[Index].State == EHansaRoadPreviewCellState::ExistingRoad) continue;
			UStaticMeshComponent* Piece = AcquireRoadPiece(ActiveRoadPieceCount++);
			// Preserve cached terrain material and height field.
			const FIntPoint Current = DisplayCells[Index];
			using namespace Hansa::Game::RoadTopology;
			const uint8 Neighbors = Mask(
				RoadCoordinates.Contains(Current+FIntPoint(1,0)), RoadCoordinates.Contains(Current+FIntPoint(0,1)),
				RoadCoordinates.Contains(Current+FIntPoint(-1,0)), RoadCoordinates.Contains(Current+FIntPoint(0,-1)));
			const FChoice Choice = Resolve(Neighbors);
			Piece->SetStaticMesh(RoadKit ? RoadKit->MeshForMask(Neighbors) : RoadMesh);
			// Until a reviewed kit is bound, the legacy single mesh remains a straight fallback.
			const bool bHasVerticalNeighbor = (Neighbors & (PositiveY | NegativeY)) != 0;
			const bool bHasHorizontalNeighbor = (Neighbors & (PositiveX | NegativeX)) != 0;
			const float Yaw = RoadKit ? Choice.QuarterTurns * 90.0f : Choice.Tile == ETile::Straight || Choice.Tile == ETile::End
				? (Choice.QuarterTurns % 2) * 90.0f : bHasVerticalNeighbor && !bHasHorizontalNeighbor ? 90.0f : 0.0f;
			const FVector World = Foundation.GroundPlacementPosition(Foundation.GetActorTransform().TransformPosition(
                Hansa::Game::LubeckPlacementGrid::GridToWorld({Current.X, Current.Y},
                    RoadKit ? AHansaRoadPresentation::GroundBaseHeight()+6 : 106.0f)),
                RoadKit ? AHansaRoadPresentation::GroundBaseHeight() : 100.0);

			Piece->SetRelativeLocation(GetActorTransform().InverseTransformPosition(World) -
				(RoadKit ? FVector::ZeroVector : RoadMesh->GetBoundingBox().GetCenter()));
			Piece->SetWorldRotation(Foundation.GetActorQuat() * FRotator(0.0f, Yaw, 0.0f).Quaternion());
			Piece->SetRelativeScale3D(FVector::OneVector);
			Piece->ComponentTags.Reset();
			Piece->ComponentTags.Add(FName(*FString::Printf(TEXT("Hansa.RoadTopology.Mask.%u"), Neighbors)));
			Piece->ComponentTags.Add(bHasHorizontalNeighbor && bHasVerticalNeighbor
				? TEXT("Hansa.RoadPreview.Corner") : TEXT("Hansa.RoadPreview.Straight"));
			Piece->SetVisibility(true, true);
            if (auto* Spline = Cast<UHansaRoadSplineComponent>(Piece)) Spline->FitTerrain(6.f,true);
		}
	}
	for (int32 Index = ActiveRoadPieceCount; Index < RoadPieceMeshes.Num(); ++Index)
	{
		RoadPieceMeshes[Index]->SetVisibility(false, true);
	}
	FitRoadFeedbackToTerrain(Foundation);
}

void AHansaBuildingPlacementGhost::FitRoadFeedbackToTerrain(const AHansaLubeckWorldFoundation& Foundation)
{
    // Centre-grounded cubes intersect slopes. Ground a tessellated surface for
    // each coloured cell and for the long border instead.
    TerrainFeedbackMesh->ClearAllMeshSections();
    const FTransform WorldToGhost = GetActorTransform().Inverse();
    const double Datum = Cast<AHansaRoadPresentation>(BuildingPresentation->GetChildActor())
        ? AHansaRoadPresentation::GroundBaseHeight() : 100.0;
    int32 SectionIndex = 0;
    auto AddSurface = [&](UStaticMeshComponent* Source, double Clearance)
    {
        const FTransform Transform = Source->GetComponentTransform();
        const FVector Size = Transform.GetScale3D().GetAbs() * 100.0;
        const int32 Columns = FMath::Max(1, FMath::CeilToInt(Size.X / 50.0));
        const int32 Rows = FMath::Max(1, FMath::CeilToInt(Size.Y / 50.0));
        FHitResult CentreHit;
        const FVector Centre = Transform.GetLocation();
        Hansa::Game::TerrainPlacement::Trace(GetWorld(), Centre + FVector(0,0,1000000), Centre - FVector(0,0,1000000), CentreHit);
        const auto* Landscape = Cast<ALandscapeProxy>(CentreHit.GetActor());
        if (Landscape && !Landscape->GetActorUpVector().Equals(FVector::UpVector, .0001)) Landscape = nullptr;
        auto GroundPoint = [&](FVector Point)
        {
            if (Landscape)
            {
                const TOptional<float> Height = Landscape->GetHeightAtLocation(Point);
                if (Height.IsSet()) { Point.Z = Height.GetValue() + Clearance; return Point; }
            }
            FVector Local = Foundation.GetActorTransform().InverseTransformPosition(Point);
            Local.Z = Datum;
            Point.Z = Foundation.GetActorTransform().TransformPosition(Local).Z + Clearance;
            return Foundation.GroundPlacementPosition(Point, Datum);
        };
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        for (int32 Y = 0; Y <= Rows; ++Y)
            for (int32 X = 0; X <= Columns; ++X)
                Vertices.Add(GroundPoint(Transform.TransformPosition(FVector(
                    -50.0 + 100.0 * X / Columns, -50.0 + 100.0 * Y / Rows, 0))));
        TArray<double> Lift;
        Lift.Init(0.0, Vertices.Num());
        for (int32 Y = 0; Y < Rows; ++Y)
            for (int32 X = 0; X < Columns; ++X)
            {
                const int32 A = Y * (Columns + 1) + X, B = A + 1, C = A + Columns + 1, D = C + 1;
                // Catch crests between samples without lifting the whole cell.
                const double Residual = FMath::Max(0.0, GroundPoint((Vertices[A] + Vertices[D]) * .5).Z
                    - (Vertices[A].Z + Vertices[D].Z) * .5);
                for (int32 I : {A, B, C, D}) Lift[I] = FMath::Max(Lift[I], Residual);
                Triangles.Append({A, D, B, A, C, D});
            }
        for (int32 I = 0; I < Vertices.Num(); ++I)
        {
            Vertices[I].Z += Lift[I];
            Vertices[I] = WorldToGhost.TransformPosition(Vertices[I]);
        }
        TerrainFeedbackMesh->CreateMeshSection(SectionIndex, Vertices, Triangles,
            TArray<FVector>(), TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        TerrainFeedbackMesh->SetMaterial(SectionIndex++, Source->GetMaterial(0));
        Source->SetVisibility(false);
    };
    for (int32 Index = 0; Index < ActiveFootprintCellCount; ++Index)
    {
        UStaticMeshComponent* Cell = FootprintCellMeshes[Index];
        AddSurface(Cell, Cell->GetMaterial(0) == RoadInvalidMaterial ? (Index % 2 == 0 ? 22.0 : 34.0) : 12.0);
    }
    for (UStaticMeshComponent* Outline : OutlineMeshes) AddSurface(Outline, 24.0);
    TerrainFeedbackMesh->SetVisibility(ActiveFootprintCellCount > 0);
}
void AHansaBuildingPlacementGhost::ApplyFeedbackVisuals(
	const EHansaPlacementFeedback Feedback, const FText& Reason)
{
	if (FeedbackMaterial == nullptr && BaseMaterial != nullptr)
	{
		FeedbackMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}
	const FLinearColor Color = Feedback == EHansaPlacementFeedback::Invalid ? HansaColor(TEXT("762F32"))
		: Feedback == EHansaPlacementFeedback::Warning ? HansaColor(TEXT("D09132"))
		: HansaColor(TEXT("1A7F75"));
	if (FeedbackMaterial != nullptr)
	{
		FeedbackMaterial->SetVectorParameterValue(TEXT("Color"), Color);
		BuildingMesh->SetMaterial(0, FeedbackMaterial);
		for (UStaticMeshComponent* Cell : FootprintCellMeshes) Cell->SetMaterial(0, FeedbackMaterial);
		for (UStaticMeshComponent* Outline : OutlineMeshes) Outline->SetMaterial(0, FeedbackMaterial);
		if (AActor* Child = BuildingPresentation->GetChildActor())
		{
			TArray<UMeshComponent*> Meshes;
			Child->GetComponents<UMeshComponent>(Meshes, true);
			for (UMeshComponent* Mesh : Meshes) Mesh->SetOverlayMaterial(FeedbackMaterial);
		}
	}
	const TCHAR* Glyph = Feedback == EHansaPlacementFeedback::Invalid ? TEXT("X INVALID")
		: Feedback == EHansaPlacementFeedback::Warning ? TEXT("! WARNING") : TEXT("+ VALID");
	StatusText->SetText(FText::Format(NSLOCTEXT("HansaPlacementGhost", "Status", "{0}\n{1}"), FText::FromString(Glyph), Reason));
	StatusText->SetTextRenderColor(Color.ToFColorSRGB());
}

void AHansaBuildingPlacementGhost::HidePreview()
{
	SetActorHiddenInGame(true);
	ActiveFootprintCellCount = 0;
	ActiveRoadPieceCount = 0;
}

AHansaPlacementProjectionManager::AHansaPlacementProjectionManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif
	Tags.Add(TEXT("Hansa.Projection.Manager"));
}

bool AHansaPlacementProjectionManager::Synchronize(
	const Hansa::Simulation::FHansaSimulationProjection& Projection,
	AHansaLubeckWorldFoundation& Foundation)
{
	if (GetWorld() == nullptr)
	{
		return false;
	}
	if (BoundFoundation.Get() != &Foundation)
	{
		TearDownProjections();
		BoundFoundation = &Foundation;
	}

	Hansa::Game::FHansaPlacementProjectionDelta Delta;
	if (!Registry.Reconcile(Projection.GetBuildingWorldProjections(), Delta))
	{
		return false;
	}
	for (const Hansa::Simulation::FHansaBuildingId BuildingId : Delta.Removed)
	{
		RemoveActor(BuildingId);
	}
	const TSet<FIntPoint> PreviousRoadCells = RoadCells;
	RoadCells.Reset();
	for (const auto& Building : Projection.GetBuildingWorldProjections())
		if (Building.Placement.BuildingDefinitionId.ToString() == TEXT("Building.Road") &&
			Building.Placement.CityId.ToString() == TEXT("City.Lubeck"))
			for (const auto Cell : Building.OccupiedCells) RoadCells.Add(FIntPoint(Cell.X, Cell.Y));
    TSet<FIntPoint> AffectedRoadCells;
    auto Affect=[&](FIntPoint Cell)
    {
        AffectedRoadCells.Add(Cell);
        for(FIntPoint Offset:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)}) AffectedRoadCells.Add(Cell+Offset);
    };
    for(FIntPoint Cell:PreviousRoadCells)if(!RoadCells.Contains(Cell))Affect(Cell);
    for(FIntPoint Cell:RoadCells)if(!PreviousRoadCells.Contains(Cell))Affect(Cell);
    if(!AffectedRoadCells.IsEmpty())
    {
        RoadRuns=Hansa::Game::BuildRoadRuns(RoadCells);
        RoadRunMasks=Hansa::Game::RoadRunNeighborMasks(RoadRuns);
    }
    const bool bFoundationMoved=!LastRoadFoundationTransform.Equals(Foundation.GetActorTransform());
    LastRoadFoundationTransform=Foundation.GetActorTransform();
    for (const Hansa::Simulation::FHansaBuildingId BuildingId : Registry.GetCanonicalIds())
    {
        const auto* Entry=Registry.Find(BuildingId);
        if(Entry && Entry->Placement.BuildingDefinitionId.ToString()==TEXT("Building.Road") &&
            !bFoundationMoved && !Delta.Created.Contains(BuildingId) && !Delta.Updated.Contains(BuildingId) &&
            !AffectedRoadCells.Contains(FIntPoint(Entry->Placement.Anchor.X,Entry->Placement.Anchor.Y))) continue;
        if (!SpawnOrUpdate(BuildingId, Foundation))
		{
			TearDownProjections();
			return false;
		}
	}
	RefreshWarehouseInventories(Projection);
	return true;
}

void AHansaPlacementProjectionManager::RefreshWarehouseInventories(
	const Hansa::Simulation::FHansaSimulationProjection& Projection)
{
	using namespace Hansa::Simulation;
	TMap<FHansaBuildingId, const FHansaInventoryProjection*> Inventories;
	for (const auto& Inventory : Projection.GetInventories())
		if (Inventory.OwnerKind == EHansaInventoryOwnerKind::Warehouse || Inventory.OwnerKind == EHansaInventoryOwnerKind::Building)
			Inventories.Add(Inventory.BuildingId, &Inventory);
    for (const auto& Entry : ProjectionActors)
        if (auto* Actor = Entry.Value.Get())
        {
            if (auto* Warehouse = Cast<AHansaWarehousePresentation>(Actor->BuildingPresentation->GetChildActor()))
            {
                const auto* Inventory = Inventories.Find(Entry.Key);
                Warehouse->ApplyInventory(Inventory ? *Inventory : nullptr);
            }
            Actor->ApplyProduction(Projection.GetProductions().FindByPredicate([&](const auto& P){return P.BuildingId==Entry.Key;}),Projection.GetInventories());
        }
}

bool AHansaPlacementProjectionManager::ConsumeEvents(
	const TConstArrayView<Hansa::Simulation::FHansaDomainEvent> Events,
	const Hansa::Simulation::FHansaSimulationProjection& Projection,
	AHansaLubeckWorldFoundation& Foundation)
{
	using namespace Hansa::Simulation;
	const bool bWorldProjectionChanged = Events.ContainsByPredicate([](const FHansaDomainEvent& Event)
	{
		return Event.GetType() == EHansaDomainEventType::BuildingPlaced ||
			Event.GetType() == EHansaDomainEventType::ConstructionProgressed ||
			Event.GetType() == EHansaDomainEventType::ConstructionCompleted ||
			Event.GetType() == EHansaDomainEventType::ConstructionCancelled ||
			Event.GetType() == EHansaDomainEventType::BuildingRemoved ||
			Event.GetType() == EHansaDomainEventType::ProductionBlockerChanged ||
			Event.GetType() == EHansaDomainEventType::ProductionActiveChanged;
	});
	if (bWorldProjectionChanged) return Synchronize(Projection, Foundation);
	// Transfers can change cargo without changing any building-world projection.
	RefreshWarehouseInventories(Projection);
	return true;
}

bool AHansaPlacementProjectionManager::RebuildFromProjection(
	const Hansa::Simulation::FHansaSimulationProjection& Projection,
	AHansaLubeckWorldFoundation& Foundation)
{
	TearDownProjections();
	BoundFoundation = &Foundation;
	return Synchronize(Projection, Foundation);
}

void AHansaPlacementProjectionManager::TearDownProjections()
{
	for (const TPair<Hansa::Simulation::FHansaBuildingId, TWeakObjectPtr<AHansaBuildingWorldProjectionActor>>& Entry :
		ProjectionActors)
	{
		if (AHansaBuildingWorldProjectionActor* Actor = Entry.Value.Get())
		{
			Actor->Destroy();
		}
	}
	ProjectionActors.Reset();
	RoadCells.Reset();
    RoadRuns.Reset(); RoadRunMasks.Reset();
	Registry.Reset();
	SelectedBuildingId = {};
	BoundFoundation.Reset();
}

void AHansaPlacementProjectionManager::SelectBuilding(const Hansa::Simulation::FHansaBuildingId BuildingId)
{
	SelectedBuildingId = BuildingId;
	for (const TPair<Hansa::Simulation::FHansaBuildingId, TWeakObjectPtr<AHansaBuildingWorldProjectionActor>>& Entry :
		ProjectionActors)
	{
		if (AHansaBuildingWorldProjectionActor* Actor = Entry.Value.Get())
		{
			Actor->SetSelected(Entry.Key == BuildingId);
		}
	}
}

void AHansaPlacementProjectionManager::ClearSelection()
{
	SelectBuilding({});
}

AHansaBuildingWorldProjectionActor* AHansaPlacementProjectionManager::FindProjectionActor(
	const Hansa::Simulation::FHansaBuildingId BuildingId) const
{
	const TWeakObjectPtr<AHansaBuildingWorldProjectionActor>* Found = ProjectionActors.Find(BuildingId);
	return Found != nullptr ? Found->Get() : nullptr;
}

void AHansaPlacementProjectionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TearDownProjections();
	Super::EndPlay(EndPlayReason);
}

bool AHansaPlacementProjectionManager::SpawnOrUpdate(
	const Hansa::Simulation::FHansaBuildingId BuildingId,
	AHansaLubeckWorldFoundation& Foundation)
{
	const Hansa::Simulation::FHansaBuildingWorldProjection* Projection = Registry.Find(BuildingId);
	if (Projection == nullptr)
	{
		return false;
	}

	AHansaBuildingWorldProjectionActor* Actor = FindProjectionActor(BuildingId);
	if (Actor == nullptr)
	{
		FActorSpawnParameters Parameters;
		Parameters.Name = MakeUniqueObjectName(
			GetWorld(), AHansaBuildingWorldProjectionActor::StaticClass(),
			FName(*FString::Printf(TEXT("HansaBuildingProjection_%llu_%u"),
				static_cast<unsigned long long>(BuildingId.GetValue()), BuildingId.GetGeneration())));
		Parameters.Owner = this;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Actor = GetWorld()->SpawnActor<AHansaBuildingWorldProjectionActor>(
			AHansaBuildingWorldProjectionActor::StaticClass(), FTransform::Identity, Parameters);
		if (Actor == nullptr)
		{
			return false;
		}
		Actor->AttachToActor(&Foundation, FAttachmentTransformRules::KeepWorldTransform);
		ProjectionActors.Add(BuildingId, Actor);
	}
	const auto Cell = Projection->Placement.Anchor;
	const FIntPoint Point(Cell.X, Cell.Y);
	const uint8 Neighbors = RoadRunMasks.FindRef(Point);
	Actor->ApplyProjection(*Projection, Foundation, Neighbors);
	Actor->SetSelected(BuildingId == SelectedBuildingId);
	return true;
}

void AHansaPlacementProjectionManager::RemoveActor(const Hansa::Simulation::FHansaBuildingId BuildingId)
{
	if (TWeakObjectPtr<AHansaBuildingWorldProjectionActor>* Found = ProjectionActors.Find(BuildingId))
	{
		if (AHansaBuildingWorldProjectionActor* Actor = Found->Get())
		{
			Actor->Destroy();
		}
		ProjectionActors.Remove(BuildingId);
	}
	if (SelectedBuildingId == BuildingId)
	{
		SelectedBuildingId = {};
	}
}

void AHansaBuildingWorldProjectionActor::ApplyProduction(const Hansa::Simulation::FHansaProductionProjection* P,
    TConstArrayView<Hansa::Simulation::FHansaInventoryProjection> Inventories)
{
    using namespace Hansa::Simulation;
    ProductionObservation={};
    if(!P)return;
    ProductionObservation.bAvailable=true;
    ProductionObservation.ProgressTicks=P->ProgressTicks; ProductionObservation.CycleTicks=P->CycleTicks;
    ProductionObservation.CompletedCycles=static_cast<int64>(P->CompletedCycles);
    ProductionObservation.Blocker=FName(LexToString(P->Blocker));
    ProductionObservation.bWorking=P->bActive && P->Blocker==EHansaProductionBlocker::None && WorldStatus!=EHansaBuildingWorldStatus::UnderConstruction;
    auto Has = [&](FHansaInventoryId Id, const TCHAR* Good)
    {
        for(const auto& I:Inventories) if(I.Id==Id)
            for(const auto& S:I.Stocks) if(S.GoodId.ToString()==Good)return S.Stock.GetRawValue()>0;
        return false;
    };
    const bool Complete=WorldStatus!=EHansaBuildingWorldStatus::UnderConstruction;
    AActor* Skin=BuildingPresentation->GetChildActor();
    if(auto* Bakery=Cast<AHansaBakeryPresentation>(Skin))
    {Bakery->FlourSack->SetVisibility(Complete&&Has(P->InputInventoryId,TEXT("Good.Flour"))); Bakery->BreadCrate->SetVisibility(Complete&&Has(P->OutputInventoryId,TEXT("Good.Bread")));}
    if(auto* Saw=Cast<AHansaSawmillPresentation>(Skin))
    {Saw->LogInput->SetVisibility(Complete&&Has(P->InputInventoryId,TEXT("Good.Timber"))); Saw->PlankOutput->SetVisibility(Complete&&Has(P->OutputInventoryId,TEXT("Good.Planks"))); Saw->SawWork->SetVisibility(Complete&&ProductionObservation.bWorking);}
    if(auto* Lumber=Cast<AHansaLumberCampPresentation>(Skin))
    {Lumber->LogPile->SetVisibility(Complete&&Has(P->OutputInventoryId,TEXT("Good.Timber"))); Lumber->CutTimber->SetVisibility(Complete&&Has(P->OutputInventoryId,TEXT("Good.Timber")));}
    if(auto* Farm=Cast<AHansaGrainFarmPresentation>(Skin)) Farm->WorkProps->SetVisibility(Complete&&ProductionObservation.bWorking);
    const bool bHasAuthoredActor = Skin != nullptr && PresentationBounds.IsValid;
    const bool bHasAuthoredMesh = BuildingMesh != nullptr &&
        BuildingMesh->GetStaticMesh() != nullptr && BuildingMesh->GetStaticMesh() != CubeMesh;
    if(!bHasAuthoredActor && !bHasAuthoredMesh)
    {
        ProductionObservation.PresentationFailure=TEXT("Verified production role asset unavailable");
        // A placeholder must not masquerade as a verified working factory.
        BuildingMesh->SetVisibility(false,true);
    }
    SampleProduction(0);
}
void AHansaBuildingWorldProjectionActor::SampleProduction(double Fraction)
{
    if(!ProductionObservation.bAvailable || BuildingDefinitionId!=TEXT("Building.Mill"))return;
    auto* Skin=BuildingPresentation->GetChildActor(); if(!Skin)return;
    const double Phase=double(ProductionObservation.CompletedCycles)*ProductionObservation.CycleTicks+ProductionObservation.ProgressTicks+
        (ProductionObservation.bWorking?FMath::Clamp(Fraction,0.,0.999999):0.);
    TArray<AActor*> MechanicalChildren; Skin->GetAllChildActors(MechanicalChildren,true); MechanicalChildren.Add(Skin);
    for(auto* Child:MechanicalChildren) if(auto* Movement=Child->FindComponentByClass<URotatingMovementComponent>())
    {
        Movement->SetComponentTickEnabled(false);
        USceneComponent* Component=Movement->UpdatedComponent;
        const auto* Defaults=Cast<URotatingMovementComponent>(Movement->GetArchetype());
        if(!Component || !Defaults)continue;
        if(!MechanicalRestRotations.Contains(Component))MechanicalRestRotations.Add(Component,Component->GetRelativeRotation());
        const auto Rate=Defaults->RotationRate;
        Component->SetRelativeRotation(MechanicalRestRotations[Component]+FRotator(FMath::Fmod(Phase*Rate.Pitch,360.),FMath::Fmod(Phase*Rate.Yaw,360.),FMath::Fmod(Phase*Rate.Roll,360.)));
    }
    for(auto It=MechanicalRestRotations.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
}

FHansaCompoundWorldObservation AHansaBuildingWorldProjectionActor::QueryCompound() const
{
 FHansaCompoundWorldObservation Out;
 if(const auto* Compound=Cast<AHansaCompoundPresentation>(BuildingPresentation->GetChildActor()))
 {
  Out.bActive=Compound->GetParcelBounds().IsValid != 0;
  Out.DefinitionId=Compound->Definition?Compound->Definition->StableDefinitionId:FString();
  Out.LayoutId=Compound->SelectedLayoutId;Out.Diagnostics=Compound->Diagnostics;Out.BatchCount=Compound->Batches.Num();
  for(const UHierarchicalInstancedStaticMeshComponent* Batch:Compound->Batches)if(Batch)Out.InstanceCount+=Batch->GetInstanceCount();
  Out.GroundSampleCount=Compound->GroundSampleCount;Out.MaximumFoundationDepth=Compound->MaximumFoundationDepth;
  Out.bTerrainComplete=Compound->bTerrainComplete;Out.GrassExclusionCount=Compound->GrassExclusionOwners.Num();
  Out.WorldAccessNodes=Compound->AccessNodes;
  for(auto& Node:Out.WorldAccessNodes)Node.Position=Compound->GetActorTransform().TransformPosition(Node.Position);
 }
 return Out;
}
