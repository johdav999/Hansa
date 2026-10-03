#include "World/HansaHarborPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "World/HansaTerrainPlacement.h"
#include "EngineUtils.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "WaterSplineComponent.h"

namespace
{
    TOptional<double> WaterSurfaceHeight(UWorld* World, const FVector& Position)
    {
        if (!World) return {};
        TOptional<double> Height;
        double NearestDistance = MAX_dbl;
        for (TActorIterator<AWaterBody> It(World); It; ++It)
        {
            const auto* Body = It->GetWaterBodyComponent();
            const auto* Spline = It->GetWaterSpline();
            if (!Body || !Spline || !Body->Bounds.GetBox().IsInsideXY(Position)) continue;
            const double Distance = FVector::DistSquaredXY(Position,
                Spline->FindLocationClosestToWorldLocation(Position, ESplineCoordinateSpace::World));
            if (Distance >= NearestDistance) continue;
            const auto Query = Body->TryQueryWaterInfoClosestToWorldLocation(Position, EWaterBodyQueryFlags::ComputeLocation);
            if (Query.HasValue())
            {
                Height = Query.GetValue().GetWaterSurfaceLocation().Z;
                NearestDistance = Distance;
            }
        }
        if (Height.IsSet()) return Height;
        // The legacy city uses a non-colliding, explicitly tagged flat water surface.
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TInlineComponentArray<UPrimitiveComponent*> Components(*It);
            for (const auto* Component : Components)
                if (Component->ComponentHasTag(TEXT("Hansa.World.Surface.Water")) &&
                    Component->IsVisible() && Component->Bounds.GetBox().IsInsideXY(Position))
                    Height = Height.IsSet() ? FMath::Max(Height.GetValue(), Component->Bounds.GetBox().Max.Z)
                        : Component->Bounds.GetBox().Max.Z;
        }
        return Height;
    }
}

FVector AHansaHarborPresentation::GroundDeckLocation(const FVector& NominalDeck, const FQuat& Heading) const
{
    TOptional<double> DeckHeight;
    // Use the deck's centre and shore-side quay edge, never the lowest pile or seabed alone.
    for (const FVector Offset : { FVector::ZeroVector, FVector(-800, -400, 0),
        FVector(-800, 0, 0), FVector(-800, 400, 0) })
    {
        const FVector Sample = NominalDeck + Heading.RotateVector(Offset);
        FHitResult Hit;
        if (Hansa::Game::TerrainPlacement::Trace(GetWorld(), Sample + FVector(0, 0, 1000000),
            Sample - FVector(0, 0, 1000000), Hit))
            DeckHeight = DeckHeight.IsSet() ? FMath::Max(DeckHeight.GetValue(), Hit.ImpactPoint.Z) : Hit.ImpactPoint.Z;
    }
    // Match the authored deck-to-waterline contract (225 cm) at the actual berth.
    // A centre sample also covers a preview whose berth is outside the water body's bounds.
    for (const FVector Offset : { Berth->GetRelativeLocation(), FVector::ZeroVector })
    {
        const auto WaterHeight = WaterSurfaceHeight(GetWorld(), NominalDeck + Heading.RotateVector(Offset));
        if (!WaterHeight.IsSet()) continue;
        const double WaterDeck = WaterHeight.GetValue() - Berth->GetRelativeLocation().Z;
        DeckHeight = DeckHeight.IsSet() ? FMath::Max(DeckHeight.GetValue(), WaterDeck) : WaterDeck;
    }
    return FVector(NominalDeck.X, NominalDeck.Y, DeckHeight.Get(NominalDeck.Z));
}

AHansaHarborPresentation::AHansaHarborPresentation()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("HarborDeckDatum")));
    PierDeck = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PierDeck"));
    QuayEdge = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("QuayEdge"));
    QuayCorner = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("QuayCorner"));
    Steps = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Steps"));
    Moorings = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Moorings"));
    Hoist = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hoist"));
    Cargo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cargo"));
    for (auto* Component : TArray<UStaticMeshComponent*>{PierDeck, QuayEdge, QuayCorner, Steps, Moorings, Hoist, Cargo})
    {
        Component->SetupAttachment(GetRootComponent());
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }
    // FBX conversion mirrors source Y; preserve the exported assembly without scaling.
    Steps->SetRelativeLocation(FVector(-600, -190, 0));
    Hoist->SetRelativeLocation(FVector(580, -100, 0));
    Cargo->SetRelativeLocation(FVector(-600, 120, 0));
    // Empty skids/transfer tackle only: these must not pretend to represent simulated cargo.
    Berth = CreateDefaultSubobject<USceneComponent>(TEXT("HarborBerthCog"));
    Berth->ComponentTags.Add(TEXT("Harbor.Berth.Cog"));
    Berth->SetupAttachment(GetRootComponent());
    Berth->SetRelativeLocation(FVector(1220, 0, -225));
    Berth->SetRelativeRotation(FRotator(0, 90, 0));
    LoadPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HarborBerthLoad"));
    LoadPoint->ComponentTags.Add(TEXT("Harbor.Berth.Load"));
    LoadPoint->SetupAttachment(GetRootComponent());
    LoadPoint->SetRelativeLocation(FVector(800, 0, 0));
}

void AHansaHarborPresentation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // Fixed P19 berth contract: clear the Cog's complete 7.81 m beam and restore saved legacy yaw.
    Berth->SetRelativeLocation(FVector(1220, 0, -225));
    Berth->SetRelativeRotation(FRotator(0, 90, 0));
    PierDeck->ClearInstances(); QuayEdge->ClearInstances(); QuayCorner->ClearInstances(); Moorings->ClearInstances();
    for (int32 Index = 0; Index < 4; ++Index)
        PierDeck->AddInstance(FTransform(FVector(-600 + Index * 400, 0, 0)));
    for (int32 Sign : {-1, 1})
    {
        QuayEdge->AddInstance(FTransform(FVector(-800, Sign * 200, 0)));
        QuayCorner->AddInstance(FTransform(FRotator(0, Sign > 0 ? 0 : 180, 0), FVector(-800, Sign * 400, 0)));
        for (int32 X : {-200, 700})
            Moorings->AddInstance(FTransform(FVector(X, Sign * 175, 0)));
    }
}

void AHansaHarborPresentation::ApplyStatus(const Hansa::Simulation::EHansaBuildingWorldStatus Status)
{
    const bool bComplete = Status != Hansa::Simulation::EHansaBuildingWorldStatus::UnderConstruction;
    PierDeck->SetVisibility(true); QuayEdge->SetVisibility(true); QuayCorner->SetVisibility(true);
    Steps->SetVisibility(bComplete); Moorings->SetVisibility(bComplete);
    Hoist->SetVisibility(bComplete); Cargo->SetVisibility(bComplete);
}
