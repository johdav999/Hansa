#include "Editor.h"
#include "EditorViewportClient.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "World/HansaRoadPresentation.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "Tests/AutomationEditorCommon.h"
#include "FileHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/LightComponent.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "StaticMeshAttributes.h"
#include "EditorReimportHandler.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "PhysicsEngine/BodySetup.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadColorImportTest,"Hansa.World.Road.RepairStagedVertexImport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadColorImportTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("P18Authoring"))) {AddInfo(TEXT("Explicit -P18Authoring required; no assets changed."));return true;}
    for (const TCHAR* Role : {TEXT("Isolated"),TEXT("End"),TEXT("Straight"),TEXT("Corner"),TEXT("TJunction"),TEXT("Crossroads")})
    {
        const FString Name=FString(TEXT("SM_HansaRoad_"))+Role;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/Hansa/Generated/Staging/Road_P18/Meshes/")+Name+TEXT(".")+Name));
        if (!TestNotNull(TEXT("Task-owned staged mesh"),Mesh))return false;
        auto* Data=Cast<UFbxStaticMeshImportData>(Mesh->GetAssetImportData());
        if (!TestNotNull(TEXT("Legacy FBX import data"),Data))return false;
        if (!TestTrue(TEXT("Source restricted to P18 export job"),Data->GetFirstFilename().Contains(TEXT("hansa-road_P18_20260908"))))return false;
        Data->VertexColorImportOption=EVertexColorImportOption::Replace;
        if (!TestTrue(TEXT("Reimport retains authored vertex alpha"),FReimportManager::Instance()->Reimport(Mesh,false,false)))return false;
        Mesh->MarkPackageDirty();
    }
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadKitAssetTest, "Hansa.World.Road.StagedKitContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadKitAssetTest::RunTest(const FString& Parameters)
{
    UClass* Class = LoadClass<AHansaRoadPresentation>(nullptr,
        TEXT("/Game/Hansa/Generated/Staging/Road_P18/BP_Road_Review.BP_Road_Review_C"));
    if (!TestNotNull(TEXT("Imported six-piece staging Blueprint"), Class)) return false;
    const auto* Kit = Class->GetDefaultObject<AHansaRoadPresentation>();
    FDataValidationContext Context;
    TestTrue(TEXT("Road authoring validation passes"), Kit->IsDataValid(Context) == EDataValidationResult::Valid);
    for (UStaticMesh* Mesh : {Kit->Isolated.Get(),Kit->End.Get(),Kit->Straight.Get(),Kit->Corner.Get(),Kit->TJunction.Get(),Kit->Crossroads.Get()})
    {
        TestTrue(TEXT("Authored simple convex collision retained"),Mesh->GetBodySetup() && Mesh->GetBodySetup()->AggGeom.ConvexElems.Num()>0);
        const FMeshDescription* Description=Mesh->GetMeshDescription(0);
        if (!TestNotNull(TEXT("Imported source geometry retained"),Description))continue;
        const FStaticMeshConstAttributes Attributes(*Description);
        const auto Colors=Attributes.GetVertexInstanceColors();
        float MinAlpha=1,MaxAlpha=0;
        for (const FVertexInstanceID Id:Description->VertexInstances().GetElementIDs())
        {MinAlpha=FMath::Min(MinAlpha,Colors[Id].W);MaxAlpha=FMath::Max(MaxAlpha,Colors[Id].W);}
        TestTrue(*FString::Printf(TEXT("%s shoulder alpha range %.3f..%.3f"),*Mesh->GetName(),MinAlpha,MaxAlpha),MinAlpha<.05 && MaxAlpha>.95);
    }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated test world"), World)) return false;
    AHansaRoadPresentation* Actor = World->SpawnActor<AHansaRoadPresentation>(Class);
    if (!Actor) { World->DestroyWorld(false); AddError(TEXT("Road actor spawn failed")); return false; }
    using namespace Hansa::Game::RoadTopology;
    for (uint8 Mask = 0; Mask < 16; ++Mask)
    {
        Actor->ApplyNeighbors(Mask);
        TestTrue(TEXT("Exact mesh for all sixteen masks"), Actor->Surface->GetStaticMesh() == Kit->MeshForMask(Mask));
        TestTrue(TEXT("Source XY datum and unit component scale are preserved"), Actor->Surface->GetRelativeLocation().IsNearlyZero() &&
            Actor->Surface->GetRelativeScale3D().Equals(FVector::OneVector));
        const FVector Direction = Actor->Surface->GetRelativeRotation().Vector();
        TestTrue(TEXT("Quarter-turn orientation matches topology"), Direction.Equals(FRotator(0, Resolve(Mask).QuarterTurns*90, 0).Vector(), .001));
        TestEqual(TEXT("Road visuals never supply gameplay collision"), Actor->Surface->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        TestFalse(TEXT("Road visuals never modify navigation"), Actor->Surface->CanEverAffectNavigation());
    }
    World->DestroyWorld(false);
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadReviewSceneTest, "Hansa.World.Road.CreateReviewScene",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadReviewSceneTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("P18Authoring"))) {AddInfo(TEXT("Explicit -P18Authoring required; no maps changed."));return true;}
    const FString Filename = FPaths::ProjectContentDir() / TEXT("Hansa/Generated/Staging/Road_P18/L_Road_Review.umap");
    if (IFileManager::Get().FileExists(*Filename)) { AddError(TEXT("Review scene already exists; reopen it instead of overwriting.")); return false; }
    if (GEditor->GetEditorWorldContext().World()->GetOutermost()->IsDirty()) { AddError(TEXT("Save the current map before creating an isolated road review.")); return false; }
    UClass* Class = LoadClass<AHansaRoadPresentation>(nullptr, TEXT("/Game/Hansa/Generated/Staging/Road_P18/BP_Road_Review.BP_Road_Review_C"));
    if (!TestNotNull(TEXT("Staged kit class"), Class)) return false;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    auto* Ground = World->SpawnActor<AStaticMeshActor>();
    Ground->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
    Ground->SetActorScale3D(FVector(60,50,1));
    Ground->SetActorLocation(FVector(600,600,0));
    auto* Light = World->SpawnActor<ADirectionalLight>();
    Light->SetActorRotation(FRotator(-45,-35,0));
    Light->GetLightComponent()->SetIntensity(3);
    World->SpawnActor<ASkyLight>();
    // Canonical six roles, dry and wet in adjacent rows; network below proves shared ports.
    const uint8 Masks[] = {0,1,5,3,7,15};
    for (int32 Row=0; Row<2; ++Row)
        for (int32 Index=0; Index<6; ++Index)
        {
            auto* Road = World->SpawnActor<AHansaRoadPresentation>(Class, FVector(Index*500-650,Row*650+1500,0), FRotator::ZeroRotator);
            Road->ApplyNeighbors(Masks[Index]); Road->SetWetness(float(Row));
            Road->SetActorLabel(FString::Printf(TEXT("Road_%s_Mask%d"), Row ? TEXT("Wet") : TEXT("Dry"), Masks[Index]));
        }
    const TSet<FIntPoint> Cells = {{0,0},{1,0},{2,0},{3,0},{4,0},{1,-1},{1,1},{3,1},{3,2},{4,2}};
    using namespace Hansa::Game::RoadTopology;
    for (const FIntPoint Cell : Cells)
    {
        auto* Road = World->SpawnActor<AHansaRoadPresentation>(Class,FVector(Cell.X*400,Cell.Y*400,0),FRotator::ZeroRotator);
        Road->ApplyNeighbors(Mask(Cells.Contains(Cell+FIntPoint(1,0)),Cells.Contains(Cell+FIntPoint(0,1)),Cells.Contains(Cell+FIntPoint(-1,0)),Cells.Contains(Cell+FIntPoint(0,-1))));
    }
    return TestTrue(TEXT("Isolated review scene saved"), FEditorFileUtils::SaveLevel(World->PersistentLevel, Filename));
}

namespace
{
class FRoadCaptureWait final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test; FString Path; uint32 Width,Height; double Start=FPlatformTime::Seconds();
public:
    FRoadCaptureWait(FAutomationTestBase* InTest, FString InPath,uint32 W,uint32 H):Test(InTest),Path(MoveTemp(InPath)),Width(W),Height(H){}
    bool Update() override
    {
        TArray<uint8> Bytes;
        if (FFileHelper::LoadFileToArray(Bytes,*Path) && Bytes.Num()>24)
        {
            auto Dimension=[&](int32 Offset){return (uint32(Bytes[Offset])<<24)|(uint32(Bytes[Offset+1])<<16)|(uint32(Bytes[Offset+2])<<8)|Bytes[Offset+3];};
            Test->TestEqual(TEXT("Native width"),Dimension(16),Width);
            Test->TestEqual(TEXT("Native height"),Dimension(20),Height);
            Test->AddInfo(Path);return true;
        }
        if (FPlatformTime::Seconds()-Start>30){Test->AddError(TEXT("Road capture timed out"));return true;}
        return false;
    }
};
}
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FHansaRoadCaptureTest, "Hansa.World.Road.NativeReviewCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FHansaRoadCaptureTest::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    for (const TCHAR* View : {TEXT("Kit1080"),TEXT("Kit720"),TEXT("Detail"),TEXT("Ground"),TEXT("GroundDetail")}) {Names.Add(View);Commands.Add(View);}
}
bool FHansaRoadCaptureTest::RunTest(const FString& Parameters)
{
    UWorld* World=GEditor->GetEditorWorldContext().World();
    const bool bGround=Parameters.StartsWith(TEXT("Ground"));
    const FString Expected=bGround?TEXT("/Game/Hansa/Generated/Staging/Road_P18/L_Road_GroundReview"):TEXT("/Game/Hansa/Generated/Staging/Road_P18/L_Road_Review");
    if (!World->GetOutermost()->GetName().StartsWith(TEXT("/Game/Hansa/Generated/Staging/Road_P18/"))) {AddError(TEXT("Open isolated road review first"));return false;}
    if (World->GetOutermost()->GetName()!=Expected)
    {
        if (World->GetOutermost()->IsDirty()) {AddError(TEXT("Save task review map before switching captures"));return false;}
        if (!FEditorFileUtils::LoadMap(Expected,false,false))return false;
        World=GEditor->GetEditorWorldContext().World();
    }
    FViewport* Viewport=GEditor->GetActiveViewport();
    if (!TestNotNull(TEXT("Rendered editor viewport"),Viewport))return false;
    auto* Client=static_cast<FEditorViewportClient*>(Viewport->GetClient());
    Client->ExposureSettings.bFixed=true;
    Client->ExposureSettings.FixedEV100=-1.f;
    const bool bDetail=Parameters==TEXT("Detail"), b720=Parameters==TEXT("Kit720");
    const uint32 Width=b720?1280:1920,Height=b720?720:1080;
    const bool bGroundDetail=Parameters==TEXT("GroundDetail");
    const FRotator Rotation(bGroundDetail?-15:bDetail?-38:-65,bGroundDetail?0:-90,0);
    Client->SetViewLocation((bGroundDetail?FVector(-630,-200,75):bGround?FVector(-600,100,75):bDetail?FVector(400,0,0):FVector(650,800,0))-Rotation.Vector()*(bGroundDetail?950:bGround?2100:bDetail?1200:4300));
    Client->SetViewRotation(Rotation);Client->SetGameView(true);
    const FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("GenerationJobs/hansa-road_P18_20260908/renders/unreal-%llu.png"),FPlatformTime::Cycles64()));
    auto& Config=GetHighResScreenshotConfig();
    if (!Config.SetResolution(Width,Height,1))return false;
    Config.FilenameOverride=Path;
    if (!Viewport->TakeHighResScreenShot())return false;
    Viewport->Draw();ADD_LATENT_AUTOMATION_COMMAND(FRoadCaptureWait(this,Path,Width,Height));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadGroundReviewSceneTest,"Hansa.World.Road.CreateGroundReviewScene",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadGroundReviewSceneTest::RunTest(const FString& Parameters)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("P18Authoring"))) {AddInfo(TEXT("Explicit -P18Authoring required; no maps changed."));return true;}
    const FString Filename=FPaths::ProjectContentDir()/TEXT("Hansa/Generated/Staging/Road_P18/L_Road_GroundReview.umap");
    if (IFileManager::Get().FileExists(*Filename)) {AddError(TEXT("Ground review already exists; never overwrite"));return false;}
    if (GEditor->GetEditorWorldContext().World()->GetOutermost()->IsDirty()) {AddError(TEXT("Current map is dirty"));return false;}
    UClass* Class=LoadClass<AHansaRoadPresentation>(nullptr,TEXT("/Game/Hansa/Generated/Staging/Road_P18/BP_Road_Review.BP_Road_Review_C"));
    if (!Class)return false;
    UWorld* World=FAutomationEditorCommonUtils::CreateNewMap();
    auto* Foundation=World->SpawnActor<AHansaLubeckWorldFoundation>();
    const TSet<FIntPoint> Cells={{26,19},{27,19},{28,19},{29,19},{30,19},{29,20},{29,21},{28,21}};
    using namespace Hansa::Game::RoadTopology;
    for (const auto Cell:Cells)
    {
        auto* Road=World->SpawnActor<AHansaRoadPresentation>(Class,Foundation->PlacementCellToWorld(Cell.X,Cell.Y,AHansaRoadPresentation::GroundBaseHeight()),FRotator::ZeroRotator);
        Road->ApplyNeighbors(Mask(Cells.Contains(Cell+FIntPoint(1,0)),Cells.Contains(Cell+FIntPoint(0,1)),Cells.Contains(Cell+FIntPoint(-1,0)),Cells.Contains(Cell+FIntPoint(0,-1))));
        Road->ApplyGround(*Foundation);
    }
    return TestTrue(TEXT("Actual MVP ground transition review saved"),FEditorFileUtils::SaveLevel(World->PersistentLevel,Filename));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadGroundContractTest,"Hansa.World.Road.GroundContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadGroundContractTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::Game::LubeckPlacementGrid;
    for (const auto& Land:GetLandSurfaces())
        TestEqual(TEXT("MVP land uses a shared road datum"),Land.Location.Z+Land.Scale.Z*50,AHansaRoadPresentation::GroundBaseHeight());
    TestEqual(TEXT("Shader inputs cover every MVP shoreline box"),GetShoreSurfaces().Num(),3);
    for (const auto& Shore:GetShoreSurfaces())
        TestTrue(TEXT("Shore rise fits conservative WPO bound"),Shore.Location.Z+Shore.Scale.Z*50-AHansaRoadPresentation::GroundBaseHeight()<=12);
    return !HasAnyErrors();
}
#endif
