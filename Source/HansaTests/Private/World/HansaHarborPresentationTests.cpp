#include "World/HansaHarborPresentation.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHarborModularContractTest, "Hansa.World.Harbor.ModularContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaHarborModularContractTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Test world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Harbor = World->SpawnActor<AHansaHarborPresentation>();
    if (!TestNotNull(TEXT("Harbor spawns"), Harbor)) return false;
    TArray<UStaticMeshComponent*> Roles; Harbor->GetComponents(Roles);
    TestEqual(TEXT("Seven modular visual roles"), Roles.Num(), 7);
    for (auto* Role : Roles)
    {
        TestNull(TEXT("Native class has no unapproved content"), Role->GetStaticMesh());
        TestTrue(TEXT("No fitting scale"), Role->GetRelativeScale3D().Equals(FVector::OneVector));
        TestEqual(TEXT("Selection and navigation remain authoritative elsewhere"), Role->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestFalse(TEXT("No navigation mutation"), Role->CanEverAffectNavigation());
    }
    for (int32 Repeat = 0; Repeat < 5; ++Repeat)
    {
        Harbor->OnConstruction(FTransform::Identity);
        TestEqual(TEXT("Four adjoining 4m decks"), Harbor->PierDeck->GetInstanceCount(), 4);
        TestEqual(TEXT("Two quay edges"), Harbor->QuayEdge->GetInstanceCount(), 2);
        TestEqual(TEXT("Two corner returns"), Harbor->QuayCorner->GetInstanceCount(), 2);
        TestEqual(TEXT("Bounded mooring instances"), Harbor->Moorings->GetInstanceCount(), 4);
    }
    for (int32 Rotation = 0; Rotation < 4; ++Rotation)
    {
        const FTransform Placement(FRotator(0, Rotation * 90, 0), FVector(100, 200, 100));
        Harbor->SetActorTransform(Placement);
        TestTrue(TEXT("Berth rotates with authored placement datum"), Harbor->Berth->GetComponentLocation().Equals(
            Placement.TransformPosition(FVector(1220, 0, -225)), .01));
        TestTrue(TEXT("Load point stays on deck datum"), Harbor->LoadPoint->GetComponentLocation().Equals(
            Placement.TransformPosition(FVector(800, 0, 0)), .01));
        TestTrue(TEXT("Berth stays at prototype water elevation"), FMath::IsNearlyEqual(Harbor->Berth->GetComponentLocation().Z, -125.0));
    }
    Harbor->ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus::UnderConstruction);
    TestFalse(TEXT("No working hoist during construction"), Harbor->Hoist->IsVisible());
    TestTrue(TEXT("Structural pier remains visible"), Harbor->PierDeck->IsVisible());
    Harbor->ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus::Blocked);
    TestTrue(TEXT("Blocked status does not erase built equipment"), Harbor->Hoist->IsVisible());
    TestFalse(TEXT("No cosmetic tick"), Harbor->PrimaryActorTick.bCanEverTick);
    TestFalse(TEXT("No replicated cosmetic state"), Harbor->GetIsReplicated());
    Harbor->Destroy();
    return !HasAnyErrors();
}
#endif
