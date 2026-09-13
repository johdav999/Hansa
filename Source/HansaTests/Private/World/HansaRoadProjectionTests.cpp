#include "World/HansaRoadPresentation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadProductionProjectionTest,"Hansa.World.Road.ProductionProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadProductionProjectionTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Simulation;
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    auto* Foundation=World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Actor=World->SpawnActor<AHansaBuildingWorldProjectionActor>();
    FHansaBuildingWorldProjection Projection;
    Projection.BuildingId=FHansaBuildingId::TryCreate(900).Value;Projection.OwnerId=FHansaHouseId::TryCreate(1).Value;
    Projection.Placement.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    Projection.Placement.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
    Projection.Placement.Anchor={18,16};Projection.OccupiedCells={{18,16}};
    Projection.FootprintWidthCells=1;Projection.FootprintHeightCells=1;
    for (uint8 Mask=0;Mask<16;++Mask)
    {
        Actor->ApplyProjection(Projection,*Foundation,Mask);
        auto* Road=Cast<AHansaRoadPresentation>(Actor->BuildingPresentation->GetChildActor());
        if (!TestNotNull(TEXT("Production road binding resolves actual Blueprint"),Road))return false;
        TestTrue(TEXT("Only approved production class"),Road->GetClass()->GetPathName().StartsWith(TEXT("/Game/Mesh/hansa-dirt-road/")));
        TestTrue(TEXT("Exact promoted mesh for every mask"),Road->Surface->GetStaticMesh()==Road->MeshForMask(Mask) && Road->MeshForMask(Mask)->GetPathName().StartsWith(TEXT("/Game/Mesh/hansa-dirt-road/")));
        TestFalse(TEXT("Road remains visible during construction"),Road->IsHidden());
        TestFalse(TEXT("No generic construction cube"),Actor->ConstructionPlaceholder->IsVisible());
        TestTrue(TEXT("Unit scale and cell datum retained"),Actor->BuildingPresentation->GetRelativeLocation().IsNearlyZero() && Actor->BuildingPresentation->GetRelativeScale3D().Equals(FVector::OneVector));
        Road->SetWetness(1);TestEqual(TEXT("Wetness is cosmetic and bounded"),Road->Wetness,1.f);
        Road->SetWetness(-1);TestEqual(TEXT("Invalid wetness clamps"),Road->Wetness,0.f);
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadProjectionJourneyTest,"Hansa.World.Road.CommandProjectionJourney",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadProjectionJourneyTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
    TStrongObjectPtr<UHansaRuntimeSimulationHost> OwnedHost(NewObject<UHansaRuntimeSimulationHost>());
    auto* Host=OwnedHost.Get();
    FString Error;
    if (!TestTrue(TEXT("Normal road host initializes"),Host->InitializeForLubeck(nullptr,Error,EHansaRuntimeScenario::EmptyLubeckBuild)))return false;
    if (!TestTrue(TEXT("Normal road session initializes"),Model->InitializeForLubeck(nullptr,Host,Error)))return false;
    auto* Definition=LoadObject<UHansaBuildingDefinition>(nullptr,TEXT("/Game/Hansa/Core/Buildings/DA_Building_Road.DA_Building_Road"));
    UClass* KitClass=LoadClass<AHansaRoadPresentation>(nullptr,TEXT("/Game/Hansa/Generated/Staging/Road_P18/BP_Road_Review.BP_Road_Review_C"));
    if (!Definition || !KitClass){AddError(TEXT("Road definition and reviewed staging kit required"));return false;}
    // Presentation-only injection after catalog compilation, restored on every exit; no assets are saved.
    const auto OriginalClass=Definition->PresentationActorClass;
    const auto OriginalMesh=Definition->PresentationMesh;
    ON_SCOPE_EXIT {Definition->PresentationActorClass=OriginalClass;Definition->PresentationMesh=OriginalMesh;};
    auto* NativeKit=GetMutableDefault<AHansaRoadPresentation>();
    const auto* StagedKit=KitClass->GetDefaultObject<AHansaRoadPresentation>();
    const TArray<TObjectPtr<UStaticMesh>> Before={NativeKit->Isolated,NativeKit->End,NativeKit->Straight,NativeKit->Corner,NativeKit->TJunction,NativeKit->Crossroads};
    ON_SCOPE_EXIT {NativeKit->Isolated=Before[0];NativeKit->End=Before[1];NativeKit->Straight=Before[2];NativeKit->Corner=Before[3];NativeKit->TJunction=Before[4];NativeKit->Crossroads=Before[5];};
    NativeKit->Isolated=StagedKit->Isolated;NativeKit->End=StagedKit->End;NativeKit->Straight=StagedKit->Straight;
    NativeKit->Corner=StagedKit->Corner;NativeKit->TJunction=StagedKit->TJunction;NativeKit->Crossroads=StagedKit->Crossroads;
    // Keep the production staging-class rejection intact; exercise the native actor with scoped test meshes.
    Definition->PresentationActorClass=AHansaRoadPresentation::StaticClass();
    Definition->PresentationMesh.Reset(); // Actor-only binding must work without a legacy mesh fallback.
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT {World->DestroyWorld(false);GEngine->DestroyWorldContext(World);};
    auto* Foundation=World->SpawnActor<AHansaLubeckWorldFoundation>();
    auto* Manager=World->SpawnActor<AHansaPlacementProjectionManager>();
    auto* Ghost=World->SpawnActor<AHansaBuildingPlacementGhost>();
    if (!Foundation || !Manager || !Ghost)return false;
    for (uint8 Mask=0;Mask<16;++Mask)
    {
        TArray<FHansaRoadPreviewCell> Cells;
        FHansaRoadPreviewCell Center;Center.Cell={18,16};Center.State=EHansaRoadPreviewCellState::NewValid;Cells.Add(Center);
        const FIntPoint Offsets[]={{1,0},{0,1},{-1,0},{0,-1}};
        for (int32 Bit=0;Bit<4;++Bit) if (Mask&(1<<Bit)) {auto Neighbor=Center;Neighbor.Cell+=Offsets[Bit];Cells.Add(Neighbor);}
        Ghost->ApplyRoadPreview(Cells,EHansaPlacementFeedback::Valid,FText::GetEmpty(),*Foundation);
        TArray<UStaticMeshComponent*> PreviewPieces;Ghost->GetComponents(PreviewPieces);
        bool bExact=false;
        for (const auto* Piece:PreviewPieces)
            bExact|=Piece->IsVisible() && Piece->ComponentTags.Contains(FName(*FString::Printf(TEXT("Hansa.RoadTopology.Mask.%u"),Mask))) && Piece->GetStaticMesh()==NativeKit->MeshForMask(Mask);
        TestTrue(TEXT("Live preview uses exact mesh across all junction masks"),bExact);
    }
    auto Refresh=[&]()
    {
        const auto Projection=Host->BuildProjection();
        if (!Projection.IsSuccess())return false;
        return Manager->Synchronize(Projection.Value,*Foundation);
    };
    auto Verify=[&]()
    {
        const auto Projection=Host->BuildProjection();
        if (!Projection.IsSuccess())return;
        const auto& Cells=Manager->GetRoadCells();
        for (const auto& Building:Projection.Value.GetBuildingWorldProjections())
        {
            if (Building.Placement.BuildingDefinitionId.ToString()!=TEXT("Building.Road"))continue;
            auto* Actor=Manager->FindProjectionActor(Building.BuildingId);
            if (!TestNotNull(TEXT("Command result has a final world actor"),Actor))continue;
            auto* Road=Cast<AHansaRoadPresentation>(Actor->BuildingPresentation->GetChildActor());
            if (!TestNotNull(TEXT("Final road uses actual kit"),Road))continue;
            const FIntPoint C(Building.Placement.Anchor.X,Building.Placement.Anchor.Y);
            const uint8 Mask=Hansa::Game::RoadTopology::Mask(Cells.Contains(C+FIntPoint(1,0)),Cells.Contains(C+FIntPoint(0,1)),Cells.Contains(C+FIntPoint(-1,0)),Cells.Contains(C+FIntPoint(0,-1)));
            TestTrue(TEXT("Final mesh reflects current authoritative neighbors"),Road->Surface->GetStaticMesh()==Road->MeshForMask(Mask));
            TestTrue(TEXT("Final actor sits on actual MVP land surface"),FMath::IsNearlyEqual(Actor->GetActorLocation().Z,AHansaRoadPresentation::GroundBaseHeight()));
            TestTrue(TEXT("No asymmetric actor recentering"),Actor->BuildingPresentation->GetRelativeLocation().IsNearlyZero());
            TestFalse(TEXT("Construction does not hide road surface"),Road->IsHidden());
            TestFalse(TEXT("No construction cube"),Actor->ConstructionPlaceholder->IsVisible());
        }
    };
    auto Draw=[&](FIntPoint A,FIntPoint B)
    {
        return Model->SelectBuilding(TEXT("Building.Road")) && Model->BeginRoadDraw(A.X,A.Y) &&
            Model->UpdateRoadDraw(B.X,B.Y) && Model->EndRoadDraw(true) && Refresh();
    };
    if (!TestTrue(TEXT("Straight road command"),Draw({18,16},{20,16})))return false;
    Verify();
    Model->SelectBuilding(TEXT("Building.Road"));
    TestTrue(TEXT("Begin neighboring branch outside existing stroke"),Model->BeginRoadDraw(19,17));
    const auto& Preview=Model->GetSnapshot();
    Ghost->ApplyRoadPreview(Preview.RoadPreviewCells,Preview.Feedback,FText::GetEmpty(),*Foundation);
    TArray<UStaticMeshComponent*> Pieces;Ghost->GetComponents(Pieces);
    bool bPreviewT=false;
    for (const auto* Piece:Pieces)
        bPreviewT |= Piece->IsVisible() && Piece->ComponentTags.Contains(TEXT("Hansa.RoadTopology.Mask.7"));
    TestTrue(TEXT("Preview refreshes existing neighbor into T outside stroke"),bPreviewT);
    TestTrue(TEXT("Commit T branch"),Model->EndRoadDraw(true));Refresh();Verify();
    TestTrue(TEXT("Complete crossroads"),Draw({19,15},{19,15}));Verify();
    TestTrue(TEXT("Corner continuation"),Draw({20,17},{22,18}));Verify();
    TArray<uint8> Bytes;
    TestTrue(TEXT("Save actual road commands"),Host->CaptureSaveBytes(Bytes,TEXT("P18 junctions"),TEXT("2026-09-08T15:00:00Z")).IsSuccess());
    const int32 Count=Host->GetPlacedBuildingCount();
    Model->SelectBuilding(TEXT("Building.Road"));Model->BeginRoadDraw(18,16);Model->UpdateRoadDraw(18,0);
    Ghost->ApplyRoadPreview(Model->GetSnapshot().RoadPreviewCells,Model->GetSnapshot().Feedback,FText::GetEmpty(),*Foundation);
    TestFalse(TEXT("Water release rejected"),Model->EndRoadDraw(true));
    TestEqual(TEXT("Invalid path leaves state unchanged"),Host->GetPlacedBuildingCount(),Count);
    Model->BeginRoadDraw(23,16);Model->UpdateRoadDraw(24,16);
    TestFalse(TEXT("Cancel submits no command"),Model->EndRoadDraw(false));
    TestEqual(TEXT("Cancellation leaves state unchanged"),Host->GetPlacedBuildingCount(),Count);
    const auto Projection=Host->BuildProjection();
    const auto* Branch=Projection.Value.GetBuildingWorldProjections().FindByPredicate([](const auto& B){return B.Placement.Anchor.X==19 && B.Placement.Anchor.Y==17;});
    if (!TestNotNull(TEXT("Branch exists"),Branch))return false;
    TestTrue(TEXT("Normal construction cancellation removes branch"),Host->CancelConstruction(Branch->BuildingId).IsSuccess());
    Refresh();Verify();
    TestFalse(TEXT("Removed branch gone from derived cells"),Manager->GetRoadCells().Contains({19,17}));
    TestTrue(TEXT("Restore road save"),Host->RestoreSaveBytes(Bytes).IsSuccess());
    const auto Restored=Host->BuildProjection();
    TestTrue(TEXT("Full rebuild after load"),Manager->RebuildFromProjection(Restored.Value,*Foundation));
    TestTrue(TEXT("Restored branch reconnects"),Manager->GetRoadCells().Contains({19,17}));Verify();
    return !HasAnyErrors();
}
#endif
