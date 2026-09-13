#include "World/HansaHarborPresentation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

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
