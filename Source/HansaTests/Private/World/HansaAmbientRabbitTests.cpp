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
    auto* Manager = World->SpawnActor<AHansaAmbientRabbits>();
    Manager->Foundation = Foundation;
    Manager->Host = NewObject<UHansaRuntimeSimulationHost>(Manager);
    FString Error;
    if (!TestTrue(TEXT("Lubeck initialized"),Manager->Host->InitializeForLubeck(World,Error))) { AddError(Error); return false; }
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    Manager->Tick(.1f);
    if (!TestEqual(TEXT("Six safe rabbits spawned"),Manager->GetLiveRabbitCount(),6)) { AddError(Manager->ValidationError); return false; }
    const auto& First = Manager->Rabbits[0];
    FVector Ground;
    const FVector Start = First.Mesh->GetComponentLocation();
    TestTrue(TEXT("Spawn is valid outdoors"),Manager->SafeGround(Start,Ground));
    int32 X,Y;
    Foundation->WorldToPlacementCell(Start,X,Y);
    Manager->Occupied.Add(FIntPoint(X,Y));
    TestFalse(TEXT("New building footprint excludes rabbit"),Manager->SafeGround(Start,Ground));
    TestFalse(TEXT("Destination path cannot cross occupied cell"),Manager->SafePath(Start,Start+FVector(200,0,0)));
    Manager->RefreshObstacles();
    TestFalse(TEXT("Water/missing land is forbidden"),Manager->SafeGround(FVector(5000,0,250),Ground));
    TestFalse(TEXT("Outside-town fallback forbidden"),Manager->SafeGround(Start+FVector(50000,0,0),Ground));
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    Manager->Tick(.1f);
    TestTrue(TEXT("Pause freezes presentation"),First.Mesh->GetComponentLocation().Equals(Start));
    Manager->Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
    bool bWalked = false, bJumped = false, bAirborne = false;
    const auto Fingerprint = Manager->Host->BuildProjection().Value.GetFingerprint().Value;
    for (int32 Frame = 0; Frame < 1800; ++Frame)
    {
        Manager->Tick(.1f);
        for (const auto& Rabbit : Manager->Rabbits)
        {
            bWalked |= Rabbit.Activity == EHansaRabbitActivity::Walk;
            bJumped |= Rabbit.Activity == EHansaRabbitActivity::Jump;
            bAirborne |= Rabbit.Activity == EHansaRabbitActivity::Jump && Rabbit.Mesh->GetComponentLocation().Z > Rabbit.Start.Z + 10;
        }
    }
    TestTrue(TEXT("Rabbits walk"),bWalked);
    TestTrue(TEXT("Rabbits occasionally jump"),bJumped);
    TestTrue(TEXT("Jump applies vertical authored root motion"),bAirborne);
    TestTrue(TEXT("Jumps complete and recover"),Manager->GetCompletedJumpCount() > 0);
    TestEqual(TEXT("Population remains six"),Manager->GetLiveRabbitCount(),6);
    TestEqual(TEXT("Cosmetic movement cannot mutate the simulation"),Manager->Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
    AddInfo(FString::Printf(TEXT("Completed jumps in 180 seconds: %d"),Manager->GetCompletedJumpCount()));
    return true;
}
#endif
