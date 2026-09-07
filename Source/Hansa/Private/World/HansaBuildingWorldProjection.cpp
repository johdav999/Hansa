#include "World/HansaBuildingWorldProjection.h"

#include "Components/SceneComponent.h"
#include "Components/ChildActorComponent.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaDefinitionBase.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaLubeckWorldFoundation.h"

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
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;
	SceneRoot->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
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

	StatusMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusMarker"));
	StatusMarker->SetupAttachment(SceneRoot);
	StatusMarker->SetStaticMesh(ConeMesh);
	ConfigurePresentationComponent(*StatusMarker);
	StatusMarker->ComponentTags.Add(TEXT("Hansa.Projection.StatusMarker"));
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
}

void AHansaBuildingWorldProjectionActor::ApplyProjection(
	const Hansa::Simulation::FHansaBuildingWorldProjection& Projection,
	const AHansaLubeckWorldFoundation& Foundation)
{
	using namespace Hansa::Simulation;
	BuildingId = Projection.BuildingId;
	BuildingDefinitionId = Projection.Placement.BuildingDefinitionId.ToString();
	WorldStatus = Projection.Status;
	ProductionBlocker = Projection.ProductionBlocker;
	bRoad = BuildingDefinitionId == TEXT("Building.Road");
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
				Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Primitive->SetGenerateOverlapEvents(false);
				Primitive->SetCanEverAffectNavigation(false);
			}
		}
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
	double MeshScale = 1.0;
	FVector MeshCenter = FVector::ZeroVector;
	if (bAuthoredVisual)
	{
		const FBox Bounds = VisualBounds;
		const FVector Size = Bounds.GetSize();
		MeshScale = FMath::Min(1.0, FMath::Min(Width / FMath::Max(1.0, Size.X), Depth / FMath::Max(1.0, Size.Y)));
		MeshCenter = Bounds.GetCenter() * MeshScale;
	}
	BuildingMesh->SetRelativeLocation(bAuthoredMesh ? FVector(-MeshCenter.X, -MeshCenter.Y, 0.0) : FVector::ZeroVector);
	double Height = bRoad ? 20.0 : 320.0;
	double MeshBottom = 0.0;
	if (bAuthoredVisual)
	{
		const FBox MeshBounds = VisualBounds;
		Height = MeshBounds.GetSize().Z * MeshScale;
		MeshBottom = MeshBounds.Min.Z * MeshScale;
	}
	const FVector FirstCenter = Hansa::Game::LubeckPlacementGrid::GridToWorld({ MinX, MinY });
	const FVector LastCenter = Hansa::Game::LubeckPlacementGrid::GridToWorld({ MaxX, MaxY });
	FVector LocalCenter = (FirstCenter + LastCenter) * 0.5;
	LocalCenter.Z = bAuthoredVisual ? 100.0 - MeshBottom : 100.0 + Height * 0.5;
	const FTransform LocalTransform(
		FRotator(0.0, RotationYaw(Projection.Placement.Rotation), 0.0), LocalCenter);
	SetActorTransform(LocalTransform * Foundation.GetActorTransform());

	BuildingMesh->SetRelativeScale3D(bAuthoredMesh
		? FVector(MeshScale)
		: FVector(Width / 100.0, Depth / 100.0, Height / 100.0));

	if (bAuthoredActor)
	{
		BuildingPresentation->SetRelativeLocation(FVector(-MeshCenter.X, -MeshCenter.Y, 0.0));
		BuildingPresentation->SetRelativeScale3D(FVector(MeshScale));
		// An invisible box remains a stable click target throughout the animation.
		BuildingMesh->SetRelativeLocation(FVector(0.0, 0.0, MeshBottom + Height * 0.5));
		BuildingMesh->SetRelativeScale3D(VisualBounds.GetSize() * MeshScale / 100.0);
	}
	const double PlaceholderHeight = bRoad ? 12.0 : 80.0;
	ConstructionPlaceholder->SetRelativeLocation(FVector(
		0.0, 0.0, bAuthoredVisual ? MeshBottom + PlaceholderHeight * 0.5 : -(Height - PlaceholderHeight) * 0.5));
	ConstructionPlaceholder->SetRelativeScale3D(
		FVector(Width / 100.0, Depth / 100.0, PlaceholderHeight / 100.0));
	SelectionOutline->SetRelativeLocation(
		FVector(0.0, 0.0, bAuthoredVisual ? MeshBottom - 3.0 : -Height * 0.5 - 3.0));
	SelectionOutline->SetRelativeScale3D(FVector((Width + 40.0) / 100.0, (Depth + 40.0) / 100.0, 0.06));
	StatusMarker->SetRelativeLocation(
		FVector(0.0, 0.0, bAuthoredVisual ? MeshBottom + Height + 100.0 : Height * 0.5 + 100.0));
	StatusMarker->SetRelativeScale3D(FVector(0.45, 0.45, 0.8));

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
	EnsureMaterials();
	ApplyVisualState();
}

void AHansaBuildingWorldProjectionActor::SetSelected(const bool bInSelected)
{
	bSelected = bInSelected;
	ApplyVisualState();
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
	BuildingMesh->SetVisibility(!bConstructing && (Presentation == nullptr || !PresentationBounds.IsValid), true);
	if (Presentation != nullptr)
	{
		Presentation->SetActorHiddenInGame(bConstructing);
		TArray<AActor*> PresentationChildren;
		Presentation->GetAllChildActors(PresentationChildren, true);
		for (AActor* Child : PresentationChildren) Child->SetActorHiddenInGame(bConstructing);
	}
	ConstructionPlaceholder->SetVisibility(bConstructing, true);
	SelectionOutline->SetVisibility(bSelected, true);
	StatusMarker->SetVisibility(bConstructing || bBlocked, true);
	StatusMarker->SetStaticMesh(bBlocked ? SphereMesh : ConeMesh);

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
	for (const Hansa::Simulation::FHansaBuildingId BuildingId : Registry.GetCanonicalIds())
	{
		if (!SpawnOrUpdate(BuildingId, Foundation))
		{
			TearDownProjections();
			return false;
		}
	}
	return true;
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
	return !bWorldProjectionChanged || Synchronize(Projection, Foundation);
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
	Actor->ApplyProjection(*Projection, Foundation);
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
