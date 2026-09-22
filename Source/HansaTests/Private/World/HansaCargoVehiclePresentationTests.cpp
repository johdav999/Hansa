#include "World/HansaCargoVehiclePresentation.h"
#include "Definitions/HansaTradeDefinitions.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCargoVehiclePresentationTest, "Hansa.World.Vehicles.ReadOnlyContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaCargoVehiclePresentationTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Simulation;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Cog = World->SpawnActor<AHansaCargoVehiclePresentation>();
    auto* Wagon = World->SpawnActor<AHansaCargoVehiclePresentation>();
    if (!Cog || !Wagon) return false;
    Wagon->bSeaVehicle = false; Wagon->OnConstruction(FTransform::Identity);
    FHansaVehicleProjection Vehicle;
    Vehicle.Id = FHansaVehicleId::TryCreate(1).Value;
    Vehicle.DefinitionId = FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;
    Vehicle.OwnerId = FHansaHouseId::TryCreate(1).Value;
    Vehicle.CargoInventoryId = FHansaInventoryId::TryCreate(1).Value;
    Vehicle.Capacity = FHansaQuantity::FromRaw(10000);
    FHansaRouteProjection Route;
    Route.VehicleId = Vehicle.Id; Route.OwnerId = Vehicle.OwnerId;
    Route.Lifecycle = EHansaRouteLifecycleState::AtStop;
    TestTrue(TEXT("Valid cargo entity accepted"), Cog->ApplyVehicle(Vehicle,&Route));
    TestFalse(TEXT("Empty holds have no invented cargo"), Cog->Cargo->IsVisible());
    TestTrue(TEXT("Berthed sail furled"), Cog->FurledSail->IsVisible() && !Cog->Sail->IsVisible());
    Route.Lifecycle = EHansaRouteLifecycleState::Traveling;
    Vehicle.Cargo = FHansaQuantity::FromRaw(5000);
    TestTrue(TEXT("Underway cargo accepted"), Cog->ApplyVehicle(Vehicle,&Route));
    TestTrue(TEXT("Traveling sail set"), Cog->Sail->IsVisible() && !Cog->FurledSail->IsVisible());
    TestTrue(TEXT("Cargo cue from snapshot"), Cog->Cargo->IsVisible());
    TestEqual(TEXT("Presenter cannot consume cargo"), Vehicle.Cargo.GetRawValue(), int64(5000));
    Route.VehicleId = FHansaVehicleId::TryCreate(2).Value;
    TestFalse(TEXT("Cross-entity route rejected"), Cog->ApplyVehicle(Vehicle,&Route));
    TestTrue(TEXT("Invalid projection hidden"), Cog->IsHidden());
    TestFalse(TEXT("Invalid projection drops identity"), Cog->GetVehicleId().IsValid());
    TestFalse(TEXT("Sea entity cannot use wagon"), Wagon->ApplyVehicle(Vehicle));
    FHansaLogisticsJobProjection Job;
    Job.Id = FHansaLogisticsJobId::TryCreate(7).Value;
    Job.RequestId = FHansaLogisticsRequestId::TryCreate(8).Value;
    Job.SourceInventoryId = FHansaInventoryId::TryCreate(1).Value;
    Job.DestinationInventoryId = FHansaInventoryId::TryCreate(2).Value;
    Job.GoodId = FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
    Job.Quantity = FHansaQuantity::FromRaw(1000);
    TestTrue(TEXT("Dispatched wagon accepted"), Wagon->ApplyLocalDelivery(Job));
    TestFalse(TEXT("No cargo before pickup"), Wagon->Cargo->IsVisible());
    Job.Status = EHansaLogisticsJobStatus::InTransit; Job.CargoQuantity = Job.Quantity;
    TestTrue(TEXT("In-transit job accepted"), Wagon->ApplyLocalDelivery(Job));
    TestTrue(TEXT("Job cargo cue visible"), Wagon->Cargo->IsVisible());
    TestTrue(TEXT("Stable local job identity"), Wagon->GetLogisticsJobId() == Job.Id);
    TestTrue(TEXT("Wagon exposes the authoritative good"), Wagon->GetCargoGoodId() == Job.GoodId);
    TestEqual(TEXT("Wagon exposes the authoritative cargo quantity"), Wagon->GetCargoMilliUnits(), Job.CargoQuantity.GetRawValue());
    TestFalse(TEXT("Local job cannot use sea hull"), Cog->ApplyLocalDelivery(Job));
    Wagon->SetWheelTravelDistance(57.4 * PI / 2);
    const FRotator Rotation = Wagon->Wheels[0]->GetRelativeRotation();
    Wagon->SetWheelTravelDistance(57.4 * PI / 2);
    TestTrue(TEXT("Absolute wheel phase is idempotent"), Rotation.Equals(Wagon->Wheels[0]->GetRelativeRotation()));
    TestTrue(TEXT("Wheel turns around axle Y"), FMath::IsNearlyEqual(Rotation.Pitch,-90.0,0.001));
    Job.Status = EHansaLogisticsJobStatus::Completed; Job.CargoQuantity = {};
    TestTrue(TEXT("Completed job accepted"), Wagon->ApplyLocalDelivery(Job));
    TestTrue(TEXT("Completed arrival stays visible without cargo until manager release"), !Wagon->IsHidden() && !Wagon->Cargo->IsVisible());
    TestEqual(TEXT("Waterline pivot remains unscaled"), Cog->GetActorScale3D(), FVector::OneVector);
    TestTrue(TEXT("Wheel contact datum"), FMath::IsNearlyEqual(Wagon->Wheels[0]->GetRelativeLocation().Z - 57.4,0.1,0.001));
    auto* Definition = NewObject<UHansaVehicleDefinition>();
    TestNull(TEXT("Legacy optional presentation stays empty"), Definition->LoadPresentationActorClass());
    Definition->PresentationActorClass = TSoftClassPtr<AHansaCargoVehiclePresentation>(FSoftObjectPath(TEXT("/Game/Hansa/Generated/Staging/Vehicles_P19/BP_Cog.BP_Cog_C")));
    TestNull(TEXT("Staging class rejected before loading"), Definition->LoadPresentationActorClass());
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCogSmoothHeadingTest, "Hansa.World.Vehicles.SmoothCogHeading",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaCogSmoothHeadingTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Cog = World->SpawnActor<AHansaCargoVehiclePresentation>();
    auto* Other = World->SpawnActor<AHansaCargoVehiclePresentation>();
    if (!Cog || !Other) return false;
    auto Heading = [](double Yaw) { return TOptional<FRotator>(FRotator(0, Yaw, 0)); };
    Cog->SampleHeading(Heading(0), 10);
    Cog->SampleHeading(Heading(90), 10);
    TestTrue(TEXT("New course does not snap"), FMath::IsNearlyZero(Cog->GetActorRotation().Yaw));
    Cog->SampleHeading(Heading(90), 10.25);
    const double EarlyYaw = Cog->GetActorRotation().Yaw;
    TestTrue(TEXT("Turn begins gradually"), EarlyYaw > 0 && EarlyYaw < 15);
    Cog->SampleHeading(Heading(90), 10.25);
    TestTrue(TEXT("Pause and repeated samples preserve heading"), FMath::IsNearlyEqual(Cog->GetActorRotation().Yaw, EarlyYaw));
    Cog->SampleHeading(Heading(90), 10.75);
    TestTrue(TEXT("Turn passes through intermediate angle"), FMath::IsNearlyEqual(Cog->GetActorRotation().Yaw, 45., 0.001));
    Other->SampleHeading(Heading(0), 10);
    Other->SampleHeading(Heading(90), 10);
    for (int32 Frame=1; Frame<=90; ++Frame) Other->SampleHeading(Heading(90), 10.+Frame/120.);
    TestTrue(TEXT("Heading independent of render frame rate"), Other->GetActorRotation().Equals(Cog->GetActorRotation(), 0.001));
    const FRotator BeforeCommand=Other->GetActorRotation();
    Other->RebaseHeadingClock(11.75);
    Other->SampleHeading(Heading(-90), 11.75);
    TestTrue(TEXT("Command tick cannot jump an in-progress turn"), BeforeCommand.Equals(Other->GetActorRotation(), 0.001));
    Cog->SampleHeading({}, 11.5);
    TestTrue(TEXT("Arrival finishes pending turn without overshoot"), FMath::IsNearlyEqual(Cog->GetActorRotation().Yaw, 90., 0.001));
    Cog->SampleHeading(Heading(-90), 11.5);
    TestTrue(TEXT("Reverse course starts without a jump"), FMath::IsNearlyEqual(Cog->GetActorRotation().Yaw, 90., 0.001));
    Cog->SampleHeading(Heading(-90), 12);
    const FRotator BeforeRetarget=Cog->GetActorRotation();
    Cog->SampleHeading(Heading(0), 12);
    TestTrue(TEXT("Mid-turn order keeps current orientation"), BeforeRetarget.Equals(Cog->GetActorRotation(), 0.001));
    Cog->ClearProjection();
    Cog->SampleHeading(Heading(179), 20);
    Cog->SampleHeading(Heading(-179), 20);
    Cog->SampleHeading(Heading(-179), 20.125);
    TestTrue(TEXT("Wraparound takes short two-degree arc"), FMath::Abs(FMath::Abs(Cog->GetActorRotation().Yaw)-180.) < 0.001);
    Cog->SampleHeading(Heading(30), 1);
    TestTrue(TEXT("Save rollback resets obsolete turn"), FMath::IsNearlyEqual(Cog->GetActorRotation().Yaw, 30., 0.001));
    return !HasAnyErrors();
}
#endif
