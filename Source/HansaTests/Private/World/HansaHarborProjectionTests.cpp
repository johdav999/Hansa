#include "World/HansaHarborPresentation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHarborProjectionTest, "Hansa.World.Harbor.ProductionProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaHarborProjectionTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Simulation;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Transient projection world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
    if (!Foundation || !Actor) return false;
    FHansaBuildingWorldProjection Projection;
    Projection.BuildingId = FHansaBuildingId::TryCreate(900).Value;
    Projection.OwnerId = FHansaHouseId::TryCreate(1).Value;
    Projection.Placement.CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    Projection.Placement.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Dock")).Value;
    Projection.Placement.Anchor = {4,2};
    Projection.FootprintWidthCells = 5;
    Projection.FootprintHeightCells = 3;
    Projection.Status = EHansaBuildingWorldStatus::Ready;
    Projection.ConstructionProgress = FHansaRate::TryMakeNormalized(FHansaRate::Scale).Value;
    for (int32 Rotation = 0; Rotation < 4; ++Rotation)
    {
        Projection.Placement.Rotation = static_cast<EHansaGridRotation>(Rotation);
        Projection.OccupiedCells.Reset();
        const int32 Width = Rotation % 2 ? 3 : 5, Height = Rotation % 2 ? 5 : 3;
        for (int32 X=0; X<Width; ++X) for (int32 Y=0; Y<Height; ++Y)
            Projection.OccupiedCells.Add({4+X,2+Y});
        Actor->ApplyProjection(Projection, *Foundation);
        Actor->SetSelected(true);
        auto* Harbor = Cast<AHansaHarborPresentation>(Actor->BuildingPresentation->GetChildActor());
        if (!TestNotNull(TEXT("Dock resolves promoted modular harbor"), Harbor)) continue;
        TestTrue(TEXT("Pile bottoms do not lift deck"), FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 100.0));
        TestTrue(TEXT("No asymmetric auto-centering or fitting"), Actor->BuildingPresentation->GetRelativeLocation().IsNearlyZero() &&
            Actor->BuildingPresentation->GetRelativeScale3D().Equals(FVector::OneVector));
        TestTrue(TEXT("Selected outline is visible at deck datum, not pile bottom"),
            Actor->SelectionOutline->IsVisible() && FMath::IsNearlyEqual(Actor->SelectionOutline->GetComponentLocation().Z, 97.0));
        TestEqual(TEXT("Production berth matches prototype water datum"), Harbor->Berth->GetComponentLocation().Z, -125.0);
        AddInfo(FString::Printf(TEXT("Harbor world %s; berth local %s; child component %s"), *Harbor->GetActorLocation().ToString(), *Harbor->Berth->GetRelativeLocation().ToString(), *Actor->BuildingPresentation->GetComponentLocation().ToString()));
        Projection.Status = EHansaBuildingWorldStatus::UnderConstruction;
        Actor->ApplyProjection(Projection, *Foundation);
        TestFalse(TEXT("Construction does not use an engine cube"), Actor->ConstructionPlaceholder->IsVisible());
        TestFalse(TEXT("Construction suppresses working equipment"), Harbor->Hoist->IsVisible());
        Projection.Status = EHansaBuildingWorldStatus::Ready;
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHarborShoreHeightTest, "Hansa.World.Harbor.ShoreHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaHarborShoreHeightTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Simulation;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Shore regression world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Foundation = World->SpawnActor<AHansaLubeckWorldFoundation>();
    Foundation->SetActorTransform(FTransform(FRotator(0, 30, 0), FVector(60000, 0, 300)));
    auto* Actor = World->SpawnActor<AHansaBuildingWorldProjectionActor>();
    auto* Ghost = World->SpawnActor<AHansaBuildingPlacementGhost>();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto Surface = [&](const FVector& Center, double Top, bool bWater, FVector Scale)
    {
        auto* SurfaceActor = World->SpawnActor<AStaticMeshActor>();
        auto* Mesh = SurfaceActor->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cube);
        Mesh->SetCollisionProfileName(bWater ? TEXT("NoCollision") : TEXT("BlockAll"));
        if (bWater) Mesh->ComponentTags.Add(TEXT("Hansa.World.Surface.Water"));
        else SurfaceActor->Tags.Add(TEXT("Hansa.Terrain"));
        SurfaceActor->SetActorScale3D(Scale);
        SurfaceActor->SetActorLocation(FVector(Center.X, Center.Y, Top - 50 * Scale.Z));
        return SurfaceActor;
    };
    FHansaBuildingWorldProjection P;
    P.BuildingId = FHansaBuildingId::TryCreate(901).Value;
    P.OwnerId = FHansaHouseId::TryCreate(1).Value;
    P.Placement.CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    P.Placement.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Dock")).Value;
    P.Placement.Anchor = {4,2};
    P.FootprintWidthCells = 5; P.FootprintHeightCells = 3;
    P.Status = EHansaBuildingWorldStatus::Ready;
    P.ConstructionProgress = FHansaRate::TryMakeNormalized(FHansaRate::Scale).Value;
    const FVector Area = Foundation->GetActorTransform().TransformPosition(
        Hansa::Game::LubeckPlacementGrid::GridToWorld({6,4}));
    Surface(Area, 800, false, FVector(200,200,1)); // Seabed under elevated water.
    auto* Water = Surface(Area, 1000, true, FVector(200,200,.1));
    auto* Shore = Surface(Area, 1400, false, FVector(2,2,1));
    for (int32 Rotation = 0; Rotation < 4; ++Rotation)
    {
        P.Placement.Rotation = static_cast<EHansaGridRotation>(Rotation);
        P.OccupiedCells.Reset(); TArray<FIntPoint> Cells;
        const int32 Width = Rotation % 2 ? 3 : 5, Height = Rotation % 2 ? 5 : 3;
        for (int32 X = 0; X < Width; ++X) for (int32 Y = 0; Y < Height; ++Y)
        { P.OccupiedCells.Add({4+X,2+Y}); Cells.Add(FIntPoint(4+X,2+Y)); }
        const FVector Nominal = Foundation->GetActorTransform().TransformPosition(
            (Hansa::Game::LubeckPlacementGrid::GridToWorld({4,2}) +
                Hansa::Game::LubeckPlacementGrid::GridToWorld({3+Width,1+Height})) * .5);
        const FQuat Heading = Foundation->GetActorQuat() * FRotator(0, Rotation*90, 0).Quaternion();
        // Put dry land specifically at the rotated shore edge, leaving the centre underwater.
        const FVector ShoreXY = Nominal + Heading.RotateVector(FVector(-800,0,0));
        Shore->SetActorLocation(FVector(ShoreXY.X, ShoreXY.Y, 1350));
        Actor->ApplyProjection(P, *Foundation);
        TestTrue(TEXT("Deck clears high shore instead of snapping to seabed"),
            FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 1400., .1));
        Shore->SetActorLocation(FVector(ShoreXY.X, ShoreXY.Y, 850));
        Actor->ApplyProjection(P, *Foundation);
        TestTrue(TEXT("Low shore deck retains 225cm above local water"),
            FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 1225., .1));
        auto* Harbor = Cast<AHansaHarborPresentation>(Actor->BuildingPresentation->GetChildActor());
        if (!TestNotNull(TEXT("Promoted harbor used"), Harbor)) return false;
        TestTrue(TEXT("Berth aligns to water while piles remain below deck"),
            FMath::IsNearlyEqual(Harbor->Berth->GetComponentLocation().Z, 1000., .1) &&
            Harbor->CalculateComponentsBoundingBoxInLocalSpace(true,true).Min.Z < -225);
        Ghost->ApplyPreview(TEXT("Building.Dock"), Cells[0], Rotation, Cells,
            EHansaPlacementFeedback::Valid, FText::GetEmpty(), *Foundation);
        TestTrue(TEXT("Ghost uses same deck height plus 6cm preview clearance"),
            FMath::IsNearlyEqual(Ghost->GetActorLocation().Z, 1231., .1));
        P.Status = EHansaBuildingWorldStatus::UnderConstruction;
        Actor->ApplyProjection(P, *Foundation);
        P.Status = EHansaBuildingWorldStatus::Ready;
        Actor->ApplyProjection(P, *Foundation);
        TestTrue(TEXT("Completion and repeated projection do not accumulate height"),
            FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 1225., .1));
    }
    Water->AddActorWorldOffset(FVector(0,0,300));
    Actor->ApplyProjection(P, *Foundation);
    TestTrue(TEXT("Restoration uses current water elevation"),
        FMath::IsNearlyEqual(Actor->GetActorLocation().Z, 1525., .1));
    return !HasAnyErrors();
}
#endif
