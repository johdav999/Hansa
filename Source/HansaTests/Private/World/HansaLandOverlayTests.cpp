#include "Misc/AutomationTest.h"
#include "World/HansaLandOverlayRenderer.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace Hansa::Simulation;
using namespace Hansa::Game::LandOverlay;
namespace
{
FHansaLandQueryResult Survey(int32 Max)
{
    FHansaLandQueryResult Q;
    Q.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    Q.ViewerHouseId=FHansaHouseId::TryCreate(1).Value;
    Q.BoundsMin={-1,-1}; Q.BoundsMax={Max+1,Max+1};
    for (int32 X=-1; X<=Max+1; ++X) for (int32 Y=-1; Y<=Max+1; ++Y)
    {
        FHansaLandCellView C; C.Coordinate={X,Y}; C.bSurveyKnown=true;
        C.RecordedOwnerId=FHansaHouseId::TryCreate(X%2==0?1:2).Value;
        C.Access=EHansaLandAccess::Permitted;
        if (X<0 || Y<0 || X>Max || Y>Max) C.Terrain=EHansaPlacementTerrain::Water;
        Q.Cells.Add(C);
    }
    return Q;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandOverlayContours,"Hansa.World.LandOverlay.Contours",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaLandOverlayContours::RunTest(const FString&)
{
    auto Q=Survey(4);
    for (auto& C:Q.Cells) if (C.Coordinate==FHansaGridCoordinate{2,2}) C.OccupyingBuildingId=FHansaBuildingId::TryCreate(10).Value;
    const auto G=Build(Q,{0,0},{4,4},EMode::Buildable,FIntPoint(0,0));
    TestTrue(TEXT("Valid geometry"),G.bValid);
    TestEqual(TEXT("Lubeck owner bands merge in permission mode"),G.Regions.Num(),1);
    if (G.Regions.Num()==1)
    {
        TestEqual(TEXT("Occupied cell is a true hole"),G.Regions[0].Cells.Num(),24);
        TestEqual(TEXT("Exterior and hole contours"),G.Regions[0].Boundaries.Num(),2);
        double TotalArea=0;
        for (const auto& B:G.Regions[0].Boundaries)
        {
            TestTrue(TEXT("Loops close"),B.bClosed);
            TestTrue(TEXT("Collinear edges are simplified"),B.Points.Num()<=6);
            for(int32 I=1;I<B.Points.Num();++I) TotalArea+=double(B.Points[I-1].X)*B.Points[I].Y-double(B.Points[I].X)*B.Points[I-1].Y;
        }
        TestEqual(TEXT("Signed contour area subtracts the hole"),TotalArea*.5,24.);
        TestTrue(TEXT("Cell selection resolves the displayed region"),G.Regions[0].bSelected);
    }
    TestTrue(TEXT("Ownership remains a distinct informational mode"),Build(Q,{0,0},{4,4},EMode::Ownership).Regions.Num()>1);
    TestTrue(TEXT("Off emits no regions"),Build(Q,{0,0},{4,4},EMode::Off).Regions.IsEmpty());
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandOverlaySeams,"Hansa.World.LandOverlay.ChunkSeams",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaLandOverlaySeams::RunTest(const FString&)
{
    auto Q=Survey(5);
    auto A=Build(Q,{0,0},{2,5},EMode::Buildable), B=Build(Q,{3,0},{5,5},EMode::Buildable);
    TestEqual(TEXT("Chunk split preserves exact outer perimeter"),A.UnitEdgeCount+B.UnitEdgeCount,24);
    for (const auto* G:{&A,&B}) for (const auto& R:G->Regions) for(const auto& P:R.Boundaries)
        for(int32 I=1;I<P.Points.Num();++I)
            TestFalse(TEXT("No false vertical line along the chunk seam"),P.Points[I-1].X==3 && P.Points[I].X==3);
    Q.Cells[0].bSurveyKnown=false;
    TestTrue(TEXT("Unknown differs from permission"),Classify(Q.Cells[0],EMode::Buildable).Surface==ESurface::Unknown);
    TestFalse(TEXT("Unbounded core is rejected"),Build(Q,{0,0},{32,32},EMode::Buildable).bValid);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandOverlayRibbons,"Hansa.World.LandOverlay.JoinedRibbons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaLandOverlayRibbons::RunTest(const FString&)
{
    const FVector2D Square[]={{0,0},{2,0},{2,2},{0,2},{0,0}};
    const auto Ribbon=BuildRibbon(Square,true,.1);
    TestEqual(TEXT("Shared quarter-cell cross-sections include one closing pair"),Ribbon.Num(),33);
    if (Ribbon.IsEmpty()) return false;
    TestTrue(TEXT("Loop meets at exactly the same join"),Ribbon[0].Left.Equals(Ribbon.Last().Left) && Ribbon[0].Right.Equals(Ribbon.Last().Right));
    for (const auto& S:Ribbon)
    {
        const FVector2D Center=(S.Left+S.Right)*.5;
        TestTrue(TEXT("Exact grid contour preserved through joins"),FMath::IsNearlyZero(Center.X)||FMath::IsNearlyZero(Center.Y)||FMath::IsNearlyEqual(Center.X,2.)||FMath::IsNearlyEqual(Center.Y,2.));
        TestTrue(TEXT("Miter cannot emit a long spike"),FVector2D::Distance(S.Left,S.Right)<=.142);
    }
    const FVector2D A[]={{-2,0},{0,0}},B[]={{0,0},{2,0}};
    const auto Left=BuildRibbon(A,false,.1),Right=BuildRibbon(B,false,.1);
    TestTrue(TEXT("Neighboring chunks have coincident strip cross-sections"),Left.Last().Left.Equals(Right[0].Left) && Left.Last().Right.Equals(Right[0].Right));
    TestEqual(TEXT("Dash phase does not restart at chunk boundaries"),Left.Last().Phase,Right[0].Phase);
    const FVector2D Duplicate[]={{0,0},{0,0},{1,0}};
    TestEqual(TEXT("Duplicate points do not introduce degenerate strips"),BuildRibbon(Duplicate,false,.1).Num(),5);
    TestTrue(TEXT("Invalid width emits no geometry"),BuildRibbon(Square,true,0).IsEmpty());
    const float Near=RibbonWidthForView(7000,60,1920,-55),Far=RibbonWidthForView(14000,60,1920,-55);
    TestTrue(TEXT("Distant borders grow to retain screen readability"),Far>Near*1.7f);
    TestTrue(TEXT("More viewport pixels require thinner ground strips"),RibbonWidthForView(7000,60,3840,-55)<Near);
    TestEqual(TEXT("Invalid view uses bounded fallback"),RibbonWidthForView(7000,60,0,-55),12.f);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandOverlayTerrainCache,"Hansa.World.LandOverlay.TerrainAndCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaLandOverlayTerrainCache::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* Ground=World->SpawnActor<AStaticMeshActor>();
    Ground->Tags.Add(TEXT("Hansa.Terrain"));
    Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Ground->SetActorLocation(FVector(400,400,-50)); Ground->SetActorScale3D(FVector(40,40,1));
    Ground->SetActorRotation(FRotator(4,0,0));
    auto* Roof=World->SpawnActor<AStaticMeshActor>();
    Roof->GetStaticMeshComponent()->SetStaticMesh(Ground->GetStaticMeshComponent()->GetStaticMesh());
    Roof->SetActorLocation(FVector(400,400,800)); Roof->SetActorScale3D(FVector(40,40,1));
    auto* Renderer=World->SpawnActor<AHansaLandOverlayRenderer>();
    TestTrue(TEXT("Cookable production shader resolves"),Renderer->HasGroundMaterial());
    auto Q=Survey(1); FHansaLandOverlayOptions O;
    const FTransform Grid(FQuat::Identity,FVector::ZeroVector,FVector(400));
    TestTrue(TEXT("Scoped snapshot renders"),Renderer->ApplyChunk({0,0},Q,{0,0},{1,1},Grid,O));
    const auto Before=Renderer->GetStats();
    TestTrue(TEXT("Terrain mesh emitted"),Before.Triangles>0);
    TestEqual(TEXT("Terrain is complete"),Before.MissingTerrainSamples,0);
    TArray<UProceduralMeshComponent*> Meshes; Renderer->GetComponents(Meshes);
    if (Meshes.IsEmpty()) { World->DestroyWorld(false); return false; }
    TestTrue(TEXT("Fill and ribbon have distinct production materials"),Meshes.Num()==1 && Meshes[0]->GetMaterial(0)!=Meshes[0]->GetMaterial(2));
    if(Meshes.Num()==1) if(const auto* Edge=Meshes[0]->GetProcMeshSection(2))
    {
        TestTrue(TEXT("Ribbons share vertices between connected strips"),Edge->ProcIndexBuffer.Num()>Edge->ProcVertexBuffer.Num()*2);
        for(const auto& V:Edge->ProcVertexBuffer)
            TestTrue(TEXT("Ribbon has explicit across-edge material coordinates"),V.UV0.Y==0 || V.UV0.Y==1);
    }
    double MinZ=DBL_MAX,MaxZ=-DBL_MAX;
    for(auto* M:Meshes)for(int32 I=0;I<M->GetNumSections();++I)if(auto* Section=M->GetProcMeshSection(I))
        for(const auto& V:Section->ProcVertexBuffer){MinZ=FMath::Min(MinZ,double(V.Position.Z));MaxZ=FMath::Max(MaxZ,double(V.Position.Z));}
    TestTrue(TEXT("Vertices follow the slope, not roofs"),MaxZ-MinZ>20 && MaxZ<200);
    Q.StateRevision=999;
    Renderer->ApplyChunk({0,0},Q,{0,0},{1,1},Grid,O);
    TestEqual(TEXT("Unrelated state revision does not rebuild geometry"),Renderer->GetStats().Rebuilds,Before.Rebuilds);
    ++GFrameCounter;Renderer->Clear(); Renderer->ApplyChunk({1,0},Q,{0,0},{1,1},Grid,O);
    TestEqual(TEXT("Culling and re-entry reuse a pooled component"),Renderer->GetStats().PooledComponents,1);
    ++GFrameCounter;Renderer->InvalidateTerrain(); Renderer->Tick(.5f);
    TestEqual(TEXT("Explicit stream invalidation rebuilds once"),Renderer->GetStats().Rebuilds,Before.Rebuilds+2);
    Renderer->Tick(.5f);
    TestEqual(TEXT("Stationary terrain remains cached"),Renderer->GetStats().Rebuilds,Before.Rebuilds+2);
    ++GFrameCounter;O.SelectedCell=FIntPoint(0,0);
    Renderer->ApplyChunk({1,0},Q,{0,0},{1,1},Grid,O);
    auto* Selection=Meshes[0]->GetProcMeshSection(3);
    TestTrue(TEXT("Selection has its own priority section"),Selection && !Selection->ProcVertexBuffer.IsEmpty());
    if(Selection) for(const auto& V:Selection->ProcVertexBuffer)
        TestTrue(TEXT("Selection marks the stable cell rather than the whole chunk"),V.Position.X<420 && V.Position.Y<420);
    World->DestroyWorld(false);
    return !HasAnyErrors();
}
#endif
