#include "World/HansaTerrainPlacement.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaRoadPresentation.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTerrainPlacementTest, "Hansa.World.Terrain.Construction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaTerrainPlacementTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Simulation;
    using namespace Hansa::Game::TerrainPlacement;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
    Foundation->SetActorLocation(FVector(60000,0,200));
    const FVector Nominal = Foundation->PlacementCellToWorld(18,16);
    TestTrue(TEXT("Missing terrain preserves legacy coordinates"),
        Ground(World, Nominal, Nominal.Z).Equals(Nominal));
    auto Surface = [&](double Z, bool Tagged)
    {
        auto* Actor = World->SpawnActor<AStaticMeshActor>();
        auto* Mesh = Actor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Actor->SetActorScale3D(FVector(20,20,1));
        Actor->SetActorLocation(FVector(Nominal.X,Nominal.Y,Z-50));
        if (Tagged) Actor->Tags.Add(TEXT("Hansa.Terrain"));
        return Actor;
    };
    auto* Terrain = Surface(850,true);
    Surface(1800,false); // A roof above terrain must never become the construction ground.
    FHitResult Hit;
    TestTrue(TEXT("Ground collision available"),Trace(World,Nominal+FVector(0,0,10000),Nominal-FVector(0,0,10000),Hit));
    TestTrue(TEXT("Terrain query ignores roof"),FMath::IsNearlyEqual(Hit.ImpactPoint.Z,850.,.1));
    TestTrue(TEXT("Cell uses terrain with preview clearance"),FMath::IsNearlyEqual(Foundation->PlacementCellToWorld(18,16,106).Z,856.,.1));
    auto* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
    FHansaBuildingWorldProjection P;
    P.BuildingId=FHansaBuildingId::TryCreate(900).Value;
    P.OwnerId=FHansaHouseId::TryCreate(1).Value;
    P.Placement.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    P.Placement.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
    P.Placement.Anchor={18,16}; P.OccupiedCells={{18,16}};
    P.FootprintWidthCells=1; P.FootprintHeightCells=1;
    Actor->ApplyProjection(P,*Foundation,3);
    TestTrue(TEXT("Constructed road datum follows terrain"),FMath::IsNearlyEqual(Actor->GetActorLocation().Z,850.,.1));
    auto* Ghost=World->SpawnActor<AHansaBuildingPlacementGhost>();
    TArray<FIntPoint> Cells={{18,16}};
    Ghost->ApplyPreview(TEXT("Building.Road"),Cells[0],0,Cells,EHansaPlacementFeedback::Valid,FText::GetEmpty(),*Foundation);
    TestTrue(TEXT("Road ghost shares terrain datum plus clearance"),FMath::IsNearlyEqual(Ghost->GetActorLocation().Z,856.,.1));
    Terrain->AddActorWorldOffset(FVector(0,0,250));
    Actor->ApplyProjection(P,*Foundation,3);
    TestTrue(TEXT("Reprojection uses current scene terrain, without cumulative offsets"),FMath::IsNearlyEqual(Actor->GetActorLocation().Z,1100.,.1));
    Actor->ApplyProjection(P,*Foundation,3);
    TestTrue(TEXT("Repeated restoration is idempotent"),FMath::IsNearlyEqual(Actor->GetActorLocation().Z,1100.,.1));
    for (const TCHAR* Building : {TEXT("Building.Bakery"), TEXT("Building.Warehouse"), TEXT("Building.Residence.Laborer")})
    {
        P.Placement.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(Building).Value;
        P.Status=EHansaBuildingWorldStatus::UnderConstruction;
        Actor->ApplyProjection(P,*Foundation);
        const double ConstructionZ=Actor->GetActorLocation().Z;
        P.Status=EHansaBuildingWorldStatus::Ready;
        Actor->ApplyProjection(P,*Foundation);
        TestTrue(FString::Printf(TEXT("%s completion retains foundation height"),Building),
            FMath::IsNearlyEqual(Actor->GetActorLocation().Z,ConstructionZ,.1));
        Terrain->AddActorWorldOffset(FVector(0,0,100));
        Actor->ApplyProjection(P,*Foundation);
        TestTrue(FString::Printf(TEXT("%s tracks terrain while retaining model pivot"),Building),
            FMath::IsNearlyEqual(Actor->GetActorLocation().Z,ConstructionZ+100,.1));
        TestTrue(TEXT("Buildings remain upright"),Actor->GetActorUpVector().Equals(FVector::UpVector,.001));
        Terrain->AddActorWorldOffset(FVector(0,0,-100));
    }
    Terrain->SetActorRotation(FRotator(12,0,0));
    const FQuat Slope=RoadRotation(World,Nominal,FQuat::Identity);
    TestFalse(TEXT("Road tiles follow terrain slope"),Slope.Equals(FQuat::Identity,.001));
    return !HasAnyErrors();
}
#endif
