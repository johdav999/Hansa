#include "World/HansaCargoVehiclePresentation.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "UObject/ConstructorHelpers.h"

using namespace Hansa::Simulation;

AHansaCargoVehiclePresentation::AHansaCargoVehiclePresentation()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("VehicleDatum")));
    SelectionMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CogSelectionMarker"));
    SelectionMarker->SetupAttachment(GetRootComponent());
    SelectionMarker->SetMobility(EComponentMobility::Movable);
    SelectionMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SelectionMarker->SetGenerateOverlapEvents(false);
    SelectionMarker->SetCanEverAffectNavigation(false);
    SelectionMarker->SetCastShadow(false);
    SelectionMarker->SetRelativeLocation(FVector(0, 0, 35));
    SelectionMarker->ComponentTags.Add(TEXT("Hansa.Selection.Cog"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMesh(
        TEXT("/Game/Mesh/cog-selection-marker/SM_CogSelectionMarker.SM_CogSelectionMarker"));
    SelectionMarker->SetStaticMesh(MarkerMesh.Object);
    SelectionMarker->SetVisibility(false);
    Selection = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoSelection"));
    Selection->SetupAttachment(GetRootComponent());
    Selection->SetCollisionProfileName(TEXT("NoCollision"));
    Selection->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Selection->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Selection->SetCanEverAffectNavigation(false);
    Selection->SetGenerateOverlapEvents(false);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Rig = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rig"));
    Sail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sail"));
    FurledSail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FurledSail"));
    Cargo = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cargo"));
    TArray<UStaticMeshComponent*> Components{Body, Rig, Sail, FurledSail, Cargo};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wheel%d"), Index));
        Wheels.Add(Wheel); Components.Add(Wheel);
    }
    for (auto* Component : Components)
    {
        Component->SetupAttachment(GetRootComponent());
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }
    LoadPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CargoLoadPoint"));
    LoadPoint->SetupAttachment(GetRootComponent());
    LoadPoint->ComponentTags.Add(TEXT("Vehicle.Cargo.Load"));
    Cargo->SetVisibility(false);
    Sail->SetVisibility(false);
}

void AHansaCargoVehiclePresentation::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Selection->SetBoxExtent(bSeaVehicle ? FVector(1200,480,700) : FVector(260,100,100));
    Selection->SetRelativeLocation(bSeaVehicle ? FVector(0,0,500) : FVector(60,0,100));
    Cargo->SetRelativeLocation(bSeaVehicle ? FVector(-190,0,193) : FVector(0,0,89));
    LoadPoint->SetRelativeLocation(bSeaVehicle ? FVector(0,381,225) : FVector(-145,0,89));
    for (int32 Index = 0; Index < Wheels.Num(); ++Index)
    {
        Wheels[Index]->SetRelativeLocation(FVector(Index < 2 ? -92 : 92, Index % 2 ? 91 : -91, 57.5));
        Wheels[Index]->SetVisibility(!bSeaVehicle);
    }
    Rig->SetVisibility(bSeaVehicle);
    ClearProjection();
}

void AHansaCargoVehiclePresentation::ClearProjection()
{
    SetSelected(false);
    bHeadingInitialized = false;
    VehicleId = {}; LogisticsJobId = {}; CargoGoodId = {}; CargoMilliUnits = 0;
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetCargoVisible(false);
    Sail->SetVisibility(false); FurledSail->SetVisibility(bSeaVehicle);
    SetWheelTravelDistance(0);
}

void AHansaCargoVehiclePresentation::SetCargoVisible(const bool bVisible)
{
    // An occupied-hold cue, not a count or commodity inventory.
    Cargo->SetVisibility(bVisible);
}

bool AHansaCargoVehiclePresentation::ApplyVehicle(const FHansaVehicleProjection& Vehicle, const FHansaRouteProjection* Route)
{
    const bool bModeMatches = bSeaVehicle ? Vehicle.Mode == EHansaRouteMode::Sea : Vehicle.Mode == EHansaRouteMode::Land;
    const FString ExpectedDefinition = bSeaVehicle ? TEXT("Vehicle.Cog") : TEXT("Vehicle.Wagon");
    if (!Vehicle.Id.IsValid() || !Vehicle.OwnerId.IsValid() || !Vehicle.CargoInventoryId.IsValid() ||
        Vehicle.DefinitionId.ToString() != ExpectedDefinition || !bModeMatches ||
        Vehicle.Capacity.GetRawValue() <= 0 || Vehicle.Cargo.GetRawValue() < 0 || Vehicle.Cargo.GetRawValue() > Vehicle.Capacity.GetRawValue() ||
        (Route && (Route->VehicleId != Vehicle.Id || Route->OwnerId != Vehicle.OwnerId || Route->Mode != Vehicle.Mode)))
    {
        ClearProjection(); return false;
    }
    VehicleId = Vehicle.Id; LogisticsJobId = {}; CargoGoodId = {}; CargoMilliUnits = Vehicle.Cargo.GetRawValue();
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetCargoVisible(Vehicle.Cargo.GetRawValue() > 0);
    const bool bUnderway = bSeaVehicle && Route && Route->Lifecycle == EHansaRouteLifecycleState::Traveling;
    Sail->SetVisibility(bUnderway); FurledSail->SetVisibility(bSeaVehicle && !bUnderway);
    return true;
}

bool AHansaCargoVehiclePresentation::ApplyLocalDelivery(const FHansaLogisticsJobProjection& Job)
{
    if (bSeaVehicle || !Job.Id.IsValid() || !Job.RequestId.IsValid() ||
        !Job.SourceInventoryId.IsValid() || !Job.DestinationInventoryId.IsValid() || !Job.GoodId.IsValid() ||
        Job.Quantity.GetRawValue() <= 0 || Job.CargoQuantity.GetRawValue() < 0 || Job.CargoQuantity.GetRawValue() > Job.Quantity.GetRawValue() ||
        (Job.Status != EHansaLogisticsJobStatus::AwaitingPickup && Job.Status != EHansaLogisticsJobStatus::InTransit &&
         Job.Status != EHansaLogisticsJobStatus::Completed && Job.Status != EHansaLogisticsJobStatus::PausedAwaitingPickup &&
         Job.Status != EHansaLogisticsJobStatus::PausedInTransit))
    {
        ClearProjection(); return false;
    }
    VehicleId = {}; LogisticsJobId = Job.Id; CargoGoodId = Job.GoodId; CargoMilliUnits = Job.CargoQuantity.GetRawValue();
    SetActorHiddenInGame(false);
    SetCargoVisible((Job.Status == EHansaLogisticsJobStatus::InTransit ||
        Job.Status == EHansaLogisticsJobStatus::PausedInTransit) && Job.CargoQuantity.GetRawValue() > 0);
    return true;
}

void AHansaCargoVehiclePresentation::SetWheelTravelDistance(const double Centimetres)
{
    const double Angle = FMath::IsFinite(Centimetres) ? FMath::Fmod(Centimetres / 57.4 * 180.0 / PI, 360.0) : 0.0;
    for (const auto& Wheel : Wheels) Wheel->SetRelativeRotation(FRotator(bSeaVehicle ? 0.0 : -Angle, 0, 0));
}

void AHansaCargoVehiclePresentation::SetSelected(bool bSelected)
{
    SelectionMarker->SetVisibility(bSelected && bSeaVehicle && !IsHidden());
    Body->SetRenderCustomDepth(bSelected);
    Body->SetCustomDepthStencilValue(1);
}

void AHansaCargoVehiclePresentation::SampleHeading(TOptional<FRotator> Heading, double PresentationTime)
{
    if (!FMath::IsFinite(PresentationTime) || (Heading.IsSet() && Heading->ContainsNaN())) return;
    if (!bSeaVehicle)
    {
        if (Heading.IsSet()) SetActorRotation(Heading.GetValue());
        return;
    }
    if (!bHeadingInitialized || PresentationTime < HeadingLastTime)
    {
        // New identities and restored clocks must not inherit a pooled vessel's turn.
        HeadingStartYaw = HeadingTargetYaw = Heading.IsSet() ? Heading->Yaw : GetActorRotation().Yaw;
        HeadingStartTime = PresentationTime;
        HeadingDuration = 0;
        bHeadingInitialized = true;
    }
    const double Alpha = HeadingDuration > 0
        ? FMath::Clamp((PresentationTime - HeadingStartTime) / HeadingDuration, 0., 1.) : 1.;
    const double Ease = Alpha * Alpha * (3. - 2. * Alpha);
    const double Yaw = HeadingStartYaw + FMath::FindDeltaAngleDegrees(HeadingStartYaw, HeadingTargetYaw) * Ease;
    SetActorRotation(FRotator(0, Yaw, 0));
    HeadingLastTime = PresentationTime;
    if (Heading.IsSet() && FMath::Abs(FMath::FindDeltaAngleDegrees(HeadingTargetYaw, Heading->Yaw)) > 0.01)
    {
        HeadingStartYaw = Yaw;
        HeadingTargetYaw = Heading->Yaw;
        HeadingStartTime = PresentationTime;
        // A quarter turn takes 1.5 simulation ticks, easing into and out of the turn.
        HeadingDuration = FMath::Max(0.25, FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, HeadingTargetYaw)) / 60.);
    }
}
void AHansaCargoVehiclePresentation::RebaseHeadingClock(double PresentationTime)
{
    if (!bHeadingInitialized || !FMath::IsFinite(PresentationTime)) return;
    HeadingStartTime += PresentationTime - HeadingLastTime;
    HeadingLastTime = PresentationTime;
}