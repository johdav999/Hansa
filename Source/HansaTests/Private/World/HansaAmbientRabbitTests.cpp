#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Animation/AnimSequence.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Materials/MaterialInterface.h"
#include "World/HansaAmbientRabbits.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRabbitAssetsTest, "Hansa.World.Rabbits.PromotedContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRabbitAssetsTest::RunTest(const FString&)
{
    auto* Mesh = LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Hansa/Animals/Rabbit/SK_Rabbit.SK_Rabbit"));
    auto* Walk = LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Hansa/Animals/Rabbit/A_Rabbit_Walk.A_Rabbit_Walk"));
    auto* Jump = LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Hansa/Animals/Rabbit/A_Rabbit_Jump.A_Rabbit_Jump"));
    FString Error;
    TestTrue(*FString::Printf(TEXT("Canonical promoted contract: %s"),*Error), AHansaAmbientRabbits::ValidateClips(Mesh,Walk,Jump,Error));
    if (!Error.IsEmpty()) AddError(Error);
    if (Mesh)
    {
        TestTrue(TEXT("Reduced runtime LOD exists"),Mesh->GetLODNum() >= 2);
        const auto* Render = Mesh->GetResourceForRendering();
        if (Render && Render->LODRenderData.IsValidIndex(1))
        {
            uint32 Triangles = 0;
            for (const auto& Section : Render->LODRenderData[1].RenderSections) Triangles += Section.NumTriangles;
            TestTrue(TEXT("Runtime rabbit under 60000 triangles"),Triangles > 1000 && Triangles < 60000);
            AddInfo(FString::Printf(TEXT("Rabbit runtime LOD triangles: %u"),Triangles));
        }
        TestTrue(TEXT("Production material assigned"),!Mesh->GetMaterials().IsEmpty() && Mesh->GetMaterials()[0].MaterialInterface &&
            Mesh->GetMaterials()[0].MaterialInterface->GetPathName().StartsWith(TEXT("/Game/Hansa/Animals/Rabbit/M_Rabbit")));
    }
    TestFalse(TEXT("Missing asset fails closed"), AHansaAmbientRabbits::ValidateClips(nullptr,Walk,Jump,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRabbitBehaviorTest, "Hansa.World.Rabbits.OutdoorBehavior",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRabbitBehaviorTest::RunTest(const FString&)
{
    for (const float TargetYaw : {90.f, 180.f, -90.f})
    {
        FHansaAmbientAnimal Turning;
        const FQuat Target = FRotator(0, TargetYaw, 0).Quaternion();
        AHansaAmbientAnimals::BeginTurn(Turning, Target);
        TestTrue(TEXT("Selecting heading does not snap"), Turning.Heading.Equals(FQuat::Identity));
        for (int32 Frame = 0; Frame < 200 && Turning.bTurning; ++Frame)
        {
            const FQuat Before = Turning.Heading;
            AHansaAmbientAnimals::AdvanceTurn(Turning, .016f);
            TestTrue(TEXT("Turn respects 120 degree/sec peak"),
                FMath::RadiansToDegrees(Before.AngularDistance(Turning.Heading)) <= 120.f*.016f + .001f);
        }
        TestFalse(TEXT("Turn finishes"), Turning.bTurning);
        TestTrue(TEXT("Turn reaches exact target"), Turning.Heading.Equals(Target, .0001));
    }
    FHansaAmbientAnimal Wrapped;
    Wrapped.Heading = FRotator(0, 179, 0).Quaternion();
    AHansaAmbientAnimals::BeginTurn(Wrapped, FRotator(0, -179, 0).Quaternion());
    AHansaAmbientAnimals::AdvanceTurn(Wrapped, Wrapped.TurnDuration*.5f);
    TestTrue(TEXT("Wrapped yaw takes shortest arc"), FMath::Abs(FMath::Abs(Wrapped.Heading.Rotator().Yaw)-180.) < .01);

    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Terrain = World->SpawnActor<AStaticMeshActor>();
    auto* Floor = Terrain->GetStaticMeshComponent();
    Floor->SetMobility(EComponentMobility::Movable);
    Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->SetCollisionProfileName(TEXT("BlockAll"));
    Terrain->Tags.Add(TEXT("Hansa.Terrain"));
    Terrain->SetActorScale3D(FVector(240,160,1));
    Terrain->SetActorLocation(FVector(0,0,200));
    auto* Manager = World->SpawnActor<AHansaAmbientAnimals>();
    Manager->Species = {FHansaAmbientAnimalProfile::Rabbit()};
    Manager->Foundation = Foundation;
    Manager->Host = NewObject<UHansaRuntimeSimulationHost>(Manager);
    FString Error;
    if (!TestTrue(TEXT("Lubeck initialized"),Manager->Host->InitializeForLubeck(World,Error))) { AddError(Error); return false; }
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    Manager->Tick(.1f);
    if (!TestEqual(TEXT("Six safe rabbits spawned"),Manager->GetLiveAnimalCount(TEXT("Rabbit")),6)) { AddError(Manager->ValidationError); return false; }
    const auto& First = Manager->Animals[0];
    FVector Ground;
    const FVector Start = First.Mesh->GetComponentLocation();
    TestTrue(TEXT("Spawn is valid outdoors"),Manager->SafeGround(Start,Ground,Manager->Species[0]));
    int32 X,Y;
    Foundation->WorldToPlacementCell(Start,X,Y);
    Manager->Occupied.Add(FIntPoint(X,Y));
    TestFalse(TEXT("New building footprint excludes rabbit"),Manager->SafeGround(Start,Ground,Manager->Species[0]));
    TestFalse(TEXT("Destination path cannot cross occupied cell"),Manager->SafePath(Start,Start+FVector(200,0,0),Manager->Species[0]));
    Manager->RefreshObstacles();
    TestFalse(TEXT("Water/missing land is forbidden"),Manager->SafeGround(FVector(5000,0,250),Ground,Manager->Species[0]));
    TestFalse(TEXT("Outside-town fallback forbidden"),Manager->SafeGround(Start+FVector(50000,0,0),Ground,Manager->Species[0]));
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    Manager->Tick(.1f);
    TestTrue(TEXT("Pause freezes presentation"),First.Mesh->GetComponentLocation().Equals(Start));
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    bool bWalked = false, bJumped = false, bAirborne = false, bTurned = false, bPausedTurn = false;
    const auto Fingerprint = Manager->Host->BuildProjection().Value.GetFingerprint().Value;
    for (int32 Frame = 0; Frame < 1800; ++Frame)
    {
        const auto Before = Manager->Animals;
        TArray<FVector> BeforeLocations;
        for (const auto& Rabbit : Before) BeforeLocations.Add(Rabbit.Mesh->GetComponentLocation());
        Manager->Tick(.1f);
        for (int32 I = 0; I < Before.Num(); ++I)
        {
            const auto& Rabbit = Manager->Animals[I];
            if (Before[I].bTurning)
            {
                bTurned = true;
                TestTrue(TEXT("Turning holds ground position"), Rabbit.Mesh->GetComponentLocation().Equals(BeforeLocations[I], .001));
                TestTrue(TEXT("Turning advances gradually"), FMath::RadiansToDegrees(Before[I].Heading.AngularDistance(Rabbit.Heading)) <= 12.001);
                TestEqual(TEXT("Turning does not consume movement time"), Rabbit.Elapsed, Before[I].Elapsed);
                if (!bPausedTurn)
                {
                    bPausedTurn = true;
                    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
                    const FQuat PausedHeading = Rabbit.Heading;
                    Manager->Tick(.1f);
                    TestTrue(TEXT("Pause freezes turning"), Rabbit.Heading.Equals(PausedHeading));
                    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
                }
            }
            if (Before[I].Activity == EHansaAnimalActivity::Jump && !Before[I].bTurning)
                TestTrue(TEXT("Jump heading stays fixed"), Rabbit.Heading.Equals(Before[I].Heading));
        }
        for (const auto& Rabbit : Manager->Animals)
        {
            bWalked |= Rabbit.Activity == EHansaAnimalActivity::Walk;
            bJumped |= Rabbit.Activity == EHansaAnimalActivity::Jump;
            bAirborne |= Rabbit.Activity == EHansaAnimalActivity::Jump && Rabbit.Mesh->GetComponentLocation().Z > Rabbit.Start.Z + 10;
        }
    }
    TestTrue(TEXT("Rabbits walk"),bWalked);
    TestTrue(TEXT("Rabbits turn before moving"),bTurned);
    TestTrue(TEXT("Pause during a turn exercised"),bPausedTurn);
    TestTrue(TEXT("Rabbits occasionally jump"),bJumped);
    TestTrue(TEXT("Jump applies vertical authored root motion"),bAirborne);
    TestTrue(TEXT("Jumps complete and recover"),Manager->GetCompletedJumpCount() > 0);
    TestEqual(TEXT("Population remains six"),Manager->GetLiveAnimalCount(TEXT("Rabbit")),6);
    TestEqual(TEXT("Cosmetic movement cannot mutate the simulation"),Manager->Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
    AddInfo(FString::Printf(TEXT("Completed jumps in 180 seconds: %d"),Manager->GetCompletedJumpCount()));
    return true;
}
#endif
