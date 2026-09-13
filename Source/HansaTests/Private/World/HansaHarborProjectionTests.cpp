#include "World/HansaHarborPresentation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "Components/ChildActorComponent.h"
#include "Components/StaticMeshComponent.h"
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
#endif
