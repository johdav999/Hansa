#include "World/HansaRoadSplineComponent.h"
#include "World/HansaRoadRuns.h"
#include "World/HansaRoadPresentation.h"
#include "World/HansaTerrainPlacement.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadTerrainFitTest,"Hansa.World.RoadTerrain.Fitting",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaRoadTerrainFitTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    auto* Ground=World->SpawnActor<AStaticMeshActor>();
    Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Ground->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Ground->SetActorScale3D(FVector(100,100,1));Ground->SetActorLocation(FVector(0,0,-50));
    Ground->Tags.Add(TEXT("Hansa.Terrain"));
    auto* Kit=LoadClass<AHansaRoadPresentation>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/BP_Road_Review.BP_Road_Review_C"));
    // Resolve the approved class through its stable road definition when its asset name differs.
    if (!Kit) Kit=AHansaRoadPresentation::StaticClass();
    auto* A=World->SpawnActor<AHansaRoadPresentation>(Kit);
    auto* B=World->SpawnActor<AHansaRoadPresentation>(Kit);
    auto* Left=Cast<UHansaRoadSplineComponent>(A->Surface);
    auto* Right=Cast<UHansaRoadSplineComponent>(B->Surface);
    if(!TestNotNull(TEXT("Production surface supports runtime spline fitting"),Left) || !Right)return false;
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Meshes/SM_HansaRoad_Straight.SM_HansaRoad_Straight"));
    if(!TestNotNull(TEXT("Approved source tessellation"),Mesh))return false;
    Left->SetStaticMesh(Mesh);Right->SetStaticMesh(Mesh);
    B->SetActorLocation(FVector(400,0,0));
    for (FRotator Slope:{FRotator::ZeroRotator,FRotator(12,0,0),FRotator(0,0,10),FRotator(8,0,7)})
    {
        Ground->SetActorRotation(Slope);
        TestTrue(TEXT("Fit complete terrain"),Left->FitTerrain(3,false,true));
        TestTrue(TEXT("Fit adjacent cell"),Right->FitTerrain(3,false,true));
        for(int32 Y=-125;Y<=125;Y+=25)
        {
            double ZA=0,ZB=0;const FVector P(200,Y,0);
            TestTrue(TEXT("Shared edge available"),Left->SampleGroundHeight(P,ZA)&&Right->SampleGroundHeight(P,ZB));
            TestTrue(TEXT("Independently fitted boundaries agree"),FMath::Abs(ZA-ZB)<.01);
            FHitResult Hit;Hansa::Game::TerrainPlacement::Trace(World,P+FVector(0,0,10000),P-FVector(0,0,10000),Hit);
            TestTrue(TEXT("Both shoulders follow actual collision"),FMath::Abs(ZA-Hit.ImpactPoint.Z)<.02);
        }
        TestTrue(TEXT("Repeated projection reuses cached samples"),Left->FitTerrain(3));
        TestEqual(TEXT("No traces for unchanged terrain projection"),Left->LastTraceCount,0);
        TestEqual(TEXT("Planar grade needs one spline section"),Left->GetSectionCount(),1);
    }
    Left->FitTerrain(6,true,true);
    TestTrue(TEXT("Preview cannot leave marks in the road RVT"),Left->RuntimeVirtualTextures.IsEmpty());
    TestTrue(TEXT("Physical fallback stays visible without RVT"),Left->bRenderInMainPass);
    double Before=0,After=0;Left->SampleGroundHeight(FVector::ZeroVector,Before);
    Ground->AddActorWorldOffset(FVector(0,0,100));
    Left->FitTerrain(3,false,true);Left->SampleGroundHeight(FVector::ZeroVector,After);
    TestTrue(TEXT("Explicit terrain refresh replaces cached height"),FMath::Abs(After-Before-100)<.02);
    // A narrow raised ridge requires shorter spline sections and still fits the full road width.
    auto* Ridge=World->SpawnActor<AStaticMeshActor>();
    Ridge->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Ridge->GetStaticMeshComponent()->SetStaticMesh(Ground->GetStaticMeshComponent()->GetStaticMesh());
    Ridge->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    Ridge->SetActorScale3D(FVector(1,10,1));Ridge->SetActorLocation(FVector(0,0,140));Ridge->Tags.Add(TEXT("Hansa.Terrain"));
    TestTrue(TEXT("Ridge sampled"),Left->FitTerrain(3,false,true));
    TestTrue(TEXT("Ridge adaptively splits the centreline"),Left->GetSectionCount()>1);
    TArray<UStaticMeshComponent*> Adaptive;A->GetComponents(Adaptive);
    for(auto* Section:Adaptive)if(auto* Material=Cast<UMaterialInstanceDynamic>(Section->GetMaterial(0)))
        TestFalse(TEXT("Adaptive sections have a supported material parent"),Material->Parent->IsA<UMaterialInstanceDynamic>());
    Ridge->Destroy();Ground->Destroy();
    TestFalse(TEXT("Unavailable terrain is explicitly diagnosed"),Left->FitTerrain(3,false,true));
    TestFalse(TEXT("Stale fit not reported as valid"),Left->bTerrainFitted);
    TestFalse(TEXT("Missing terrain diagnostic is retained"),Left->FitDiagnostic.IsEmpty());
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadRunsTest,"Hansa.World.RoadTerrain.Runs",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaRoadRunsTest::RunTest(const FString&)
{
    using namespace Hansa::Game;
    TSet<FIntPoint> Cells={{0,0},{1,0},{-1,0},{0,1},{0,-1},{5,5}};
    const auto Runs=BuildRoadRuns(Cells);
    TestEqual(TEXT("Four junction arms plus isolated tile"),Runs.Num(),5);
    const auto Masks=RoadRunNeighborMasks(Runs);
    TestEqual(TEXT("Junction accumulates every run port"),Masks.FindRef({0,0}),uint8(15));
    TestEqual(TEXT("Isolated tile has no invented port"),Masks.FindRef({5,5}),uint8(0));
    Cells.Remove({0,-1});
    TestEqual(TEXT("Removing an arm updates junction topology"),RoadRunNeighborMasks(BuildRoadRuns(Cells)).FindRef({0,0}),uint8(7));
    const TSet<FIntPoint> Loop={{10,10},{11,10},{11,11},{10,11}};
    const auto Cycle=BuildRoadRuns(Loop);
    TestEqual(TEXT("Closed loop remains a single run"),Cycle.Num(),1);
    TestTrue(TEXT("Loop closes explicitly"),Cycle[0].Num()==5 && Cycle[0][0]==Cycle[0].Last());
    const TSet<FIntPoint> Reordered={{11,11},{10,11},{10,10},{11,10}};
    TestTrue(TEXT("Runs do not depend on hash-set insertion order"),Cycle==BuildRoadRuns(Reordered));
    return !HasAnyErrors();
}
#endif
