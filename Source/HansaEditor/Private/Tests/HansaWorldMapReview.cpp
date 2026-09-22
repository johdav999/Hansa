#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/TextRenderComponent.h"
#include "Engine/TextRenderActor.h"
#include "WaterBodyCustomActor.h"
#include "WaterBodyComponent.h"
#include "WaterBodyRiverActor.h"
#include "WaterBodyRiverComponent.h"
#include "WaterSplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "LevelEditorViewport.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "WorldPartition/WorldPartition.h"
#include "ImageUtils.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Serialization/JsonSerializer.h"
#include "World/HansaGameMode.h"

namespace
{
class FHansaWorldReview : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    TSharedPtr<FJsonObject> Manifest;
    TStrongObjectPtr<UTextureRenderTarget2D> Target;
    ASceneCapture2D* Camera=nullptr;
    double Started=FPlatformTime::Seconds(), Ready=0;
    int32 Stage=-1;
    bool Prepared=false;
    FString Output;
    TArray<FString> Views={TEXT("Overview"),TEXT("Lubeck"),TEXT("Stockholm"),TEXT("Reval"),TEXT("London"),TEXT("Bergen"),TEXT("Riga"),TEXT("LubeckClose"),TEXT("RiverClose")};
public:
    explicit FHansaWorldReview(FAutomationTestBase* In):Test(In)
    {if(FParse::Param(FCommandLine::Get(),TEXT("HansaRiverOnly")))Views={TEXT("RiverClose"),TEXT("RiverNoSea"),TEXT("RiverOnly")};}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Started>900){Test->AddError(TEXT("World review timed out"));return true;}
        if(Stage==-1)
        {
            const FString Map=TEXT("/Game/Hansa/Generated/Staging/HansaWorld_20260918/L_HansaWorld_WP");
            if(!FEditorFileUtils::LoadMap(Map,false,true)){Test->AddError(TEXT("Staged world missing"));return true;}
            UWorld* World=GEditor->GetEditorWorldContext().World();
            Test->TestEqual(TEXT("Campaign map uses production Hansa GameMode"),World->GetWorldSettings()->DefaultGameMode.Get(),AHansaGameMode::StaticClass());
            // Test the same normal-open path as the user. Force-loading here masked missing terrain.
            Test->TestFalse(TEXT("Authoring map opens without spatial streaming"),World->GetWorldPartition()->IsStreamingEnabled());
            int32 LoadedLandscapeComponents=0;
            for(TActorIterator<ALandscapeProxy> L(World);L;++L)LoadedLandscapeComponents+=L->LandscapeComponents.Num();
            Test->TestEqual(TEXT("Normal open loads all terrain components without loader helpers"),LoadedLandscapeComponents,832);
            FString Text;FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("SourceArt/Terrain/HansaWorld/Prototype_20260918/terrain-manifest.json")));
            if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Manifest)){Test->AddError(TEXT("Manifest unavailable"));return true;}
            Output=FPaths::ProjectSavedDir()/TEXT("GenerationJobs/HansaWorld_20260918/review");IFileManager::Get().MakeDirectory(*Output,true);
            FAssetCompilingManager::Get().FinishAllCompilation();
            if(FParse::Param(FCommandLine::Get(),TEXT("HansaWorldRepair")))
            {
                for(TActorIterator<ALandscape> L(World);L;++L)L->ForceLayersFullUpdate();
                for(TActorIterator<AWaterBodyRiver> R(World);R;++R)if(R->Tags.Contains(TEXT("HansaWorld.SplineRiver.v1")))
                {
                    FOnWaterBodyChangedParams Changed;Changed.bShapeOrPositionChanged=true;
                    R->GetWaterBodyComponent()->UpdateAll(Changed);R->GetWaterBodyComponent()->UpdateWaterBodyRenderData();
                    TArray<UStaticMeshComponent*> Surfaces;R->GetComponents(Surfaces);
                    for(auto* Surface:Surfaces)if(Surface->GetClass()->GetName()==TEXT("WaterBodyStaticMeshComponent"))
                    {
                        UStaticMesh* Mesh=Surface->GetStaticMesh();if(!Mesh||!Mesh->GetNumSourceModels())continue;
                        // Native Water's generated overlays can contain zero normals;
                        // rebuild smooth geometric normals before persisting the surface.
                        auto& Build=Mesh->GetSourceModel(0).BuildSettings;
                        Build.bRecomputeNormals=true;Build.bRecomputeTangents=true;
                        Mesh->Build(false);Mesh->MarkPackageDirty();
                    }
                    R->MarkPackageDirty();
                }
                FAssetCompilingManager::Get().FinishAllCompilation();
            }
            Target.Reset(NewObject<UTextureRenderTarget2D>());Target->InitCustomFormat(1920,1080,PF_B8G8R8A8,false);Target->UpdateResourceImmediate(true);
            Camera=World->SpawnActor<ASceneCapture2D>();Camera->SetFlags(RF_Transient);
            for(TActorIterator<AActor> A(World);A;++A)if(A->GetActorLabel().Contains(TEXT("City.Lubeck"))||A->GetActorLabel()==TEXT("SM_Sea_04_04"))
            {
                UE_LOG(LogTemp,Display,TEXT("WorldVisibility %s loc=%s hidden=%d editorHidden=%d"),*A->GetActorLabel(),*A->GetActorLocation().ToString(),A->IsHidden(),A->IsHiddenEd());
                TArray<UPrimitiveComponent*> Components;A->GetComponents(Components);for(auto* C:Components)
                    UE_LOG(LogTemp,Display,TEXT("WorldComponent %s registered=%d visible=%d hiddenGame=%d bounds=%s material=%s"),*C->GetName(),C->IsRegistered(),C->IsVisible(),C->bHiddenInGame,*C->Bounds.Origin.ToString(),*GetPathNameSafe(C->GetMaterial(0)));
            }
            auto* Capture=Camera->GetCaptureComponent2D();Capture->TextureTarget=Target.Get();Capture->CaptureSource=SCS_FinalColorLDR;
            Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;Capture->bAlwaysPersistRenderingState=true;Capture->FOVAngle=50;
            // Staged reference markers must be visible in game-view previews too.
            Capture->ShowFlags.SetGame(true);Capture->ShowFlags.SetEditor(false);
            Stage=0;Ready=FPlatformTime::Seconds()+10;return false;
        }
        if(Prepared&&Camera){Camera->GetCaptureComponent2D()->CaptureScene();FlushRenderingCommands();}
        if(FPlatformTime::Seconds()<Ready||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()))return false;
        UWorld* World=GEditor->GetEditorWorldContext().World();
        if(Stage==0&&!Prepared)
        {
            int32 Markers=0,Labels=0;for(TActorIterator<AActor> A(World);A;++A)
            {if(A->GetActorLabel().StartsWith(TEXT("DEV_CityMarker_")))++Markers;
             if(A->GetActorLabel().StartsWith(TEXT("DEV_CityLabel_")))++Labels;
             if(A->GetActorLabel().StartsWith(TEXT("DEV_CityMarker_"))||A->GetActorLabel().StartsWith(TEXT("DEV_CityLabel_")))
             {
                 Test->TestFalse(TEXT("City reference survives Play/Simulate duplication"),bool(A->bIsEditorOnlyActor));
                 Test->TestFalse(TEXT("City reference not distance-streamed"),A->GetIsSpatiallyLoaded());
                 Test->TestFalse(TEXT("City reference not hidden in game"),A->IsHidden());
                 Test->TestFalse(TEXT("City reference not hidden in editor"),A->IsHiddenEd());
                 TArray<UPrimitiveComponent*> Primitives;A->GetComponents(Primitives);
                 for(auto* C:Primitives)if(C->IsA<UStaticMeshComponent>()||C->IsA<UTextRenderComponent>())
                 {Test->TestTrue(TEXT("City reference component visible"),C->IsVisible());Test->TestFalse(TEXT("City reference component visible in preview"),C->bHiddenInGame);}
             }}
            Test->TestEqual(TEXT("31 city cubes"),Markers,31);Test->TestEqual(TEXT("31 city labels"),Labels,31);
            int32 RiverCount=0;double MaximumRise=0;
            for(TActorIterator<AWaterBodyRiver> R(World);R;++R)if(R->Tags.Contains(TEXT("HansaWorld.SplineRiver.v1")))
            {
                ++RiverCount;auto* Spline=R->GetWaterSpline();auto* Body=R->GetWaterBodyComponent();
                Test->TestFalse(TEXT("River does not destructively auto-carve"),Body->AffectsLandscape());
                Test->TestTrue(TEXT("Native spline-generated surface enabled"),Body->GetWaterBodyStaticMeshSettings().bEnableWaterBodyStaticMesh);
                Test->TestTrue(TEXT("River width/depth controls"),Spline->GetNumberOfSplinePoints()>=2);
                double Previous=Spline->GetLocationAtSplineInputKey(0,ESplineCoordinateSpace::World).Z;
                for(float Key=.1f;Key<=Spline->GetNumberOfSplinePoints()-1;Key+=.1f)
                {
                    const double Z=Spline->GetLocationAtSplineInputKey(Key,ESplineCoordinateSpace::World).Z;
                    MaximumRise=FMath::Max(MaximumRise,Z-Previous);Previous=Z;
                }
                TArray<USplineMeshComponent*> Meshes;R->GetComponents(Meshes);
                Test->TestEqual(TEXT("One connected native mesh per spline segment"),Meshes.Num(),Spline->GetNumberOfSplinePoints()-1);
                TArray<UStaticMeshComponent*> Surfaces;R->GetComponents(Surfaces);int32 VisibleSurfaces=0;
                for(auto* Mesh:Surfaces)if(Mesh->GetClass()->GetName()==TEXT("WaterBodyStaticMeshComponent"))
                {Test->TestTrue(TEXT("Native river surface mesh persists"),Mesh->GetStaticMesh()!=nullptr);if(Mesh->IsVisible()&&!Mesh->bHiddenInGame)++VisibleSurfaces;}
                Test->TestTrue(TEXT("Native river surface visible on normal reopen"),VisibleSurfaces>0);
            }
            if(FPaths::FileExists(FPaths::ProjectDir()/TEXT("SourceArt/Terrain/HansaWorld/Prototype_20260918/SplineRivers_v1/rivers.json")))
                Test->TestTrue(TEXT("Expected spline river migration is present"),RiverCount>0);
            if(RiverCount)
            {
                FString RiverText;TSharedPtr<FJsonObject> RiverManifest;
                FFileHelper::LoadFileToString(RiverText,*(FPaths::ProjectDir()/TEXT("SourceArt/Terrain/HansaWorld/Prototype_20260918/SplineRivers_v1/rivers.json")));
                Test->TestTrue(TEXT("River manifest parsed"),FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RiverText),RiverManifest));
                if(RiverManifest)Test->TestEqual(TEXT("All geographical river reaches reopened"),RiverCount,RiverManifest->GetArrayField(TEXT("rivers")).Num());
                Test->TestTrue(TEXT("Continuous downstream profile never rises more than 1mm"),MaximumRise<=.1);
                for(TActorIterator<AWaterBodyCustom> A(World);A;++A)if(A->GetActorLabel().StartsWith(TEXT("SM_InlandWater_")))
                {Test->TestFalse(TEXT("Legacy river tiles stay disabled"),A->GetWaterBodyComponent()->IsVisible());Test->TestTrue(TEXT("Legacy tiles persistently hidden in editor and game"),A->bHiddenEd&&A->IsHidden());}
                FFileHelper::SaveStringToFile(FString::Printf(TEXT("rivers=%d\nmaximumRiseCm=%.8f\n"),RiverCount,MaximumRise),*(Output/TEXT("river-readback.txt")));
            }
            FString SeaText;TSharedPtr<FJsonObject> SeaManifest;
            const FString SeaPath=FPaths::ProjectDir()/TEXT("SourceArt/Terrain/HansaWorld/Prototype_20260918/SplineRivers_v1/SeaCorridors_v1/sea-corridors.json");
            if(FFileHelper::LoadFileToString(SeaText,*SeaPath)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SeaText),SeaManifest))
            {
                TMap<FString,FString> ExpectedSea;
                for(const auto& V:SeaManifest->GetArrayField(TEXT("tiles")))
                {const auto E=V->AsObject();ExpectedSea.Add(E->GetStringField(TEXT("actor")),E->GetStringField(TEXT("mesh"))+TEXT("_")+E->GetStringField(TEXT("sha256")).Left(12));}
                int32 SeaCount=0,CutCount=0;
                for(TActorIterator<AWaterBodyCustom> A(World);A;++A)if(A->GetActorLabel().StartsWith(TEXT("SM_Sea_")))
                {
                    ++SeaCount;auto* B=A->GetWaterBodyComponent();auto* Mesh=B->GetWaterMeshOverride();
                    Test->TestTrue(TEXT("All sea actors remain visible"),B->IsVisible()&&!A->bHiddenEd&&!A->IsHidden());
                    if(const FString* Expected=ExpectedSea.Find(A->GetActorLabel()))
                    {++CutCount;Test->TestTrue(TEXT("Approved sea cutout persists on normal reopen"),Mesh&&Mesh->GetName()==*Expected);}
                    else Test->TestTrue(TEXT("Unchanged sea tiles retain original meshes"),Mesh&&Mesh->GetName()==A->GetActorLabel());
                }
                Test->TestEqual(TEXT("All 48 sea tiles retained"),SeaCount,48);
                Test->TestEqual(TEXT("Every approved sea cutout loaded"),CutCount,ExpectedSea.Num());
            }
            else if(RiverCount)Test->AddError(TEXT("Approved sea cutout manifest unavailable"));
            for(TActorIterator<ALandscape> L(World);L;++L)
            {
                auto* Base=L->GetEditLayerConst(FName(TEXT("Survey_Base")));Test->TestNotNull(TEXT("Survey layer"),Base);
                if(Base)Test->TestTrue(TEXT("Survey base locked"),Base->IsLocked());
                Test->TestEqual(TEXT("832 Landscape components"),L->GetLandscapeInfo()->XYtoComponentMap.Num(),832);
                const int32 W=4033,H=3277;TArray<uint16> Actual;Actual.SetNumZeroed(W*H);
                FLandscapeEditDataInterface Edit(L->GetLandscapeInfo(),FGuid(),false);Edit.SetShouldDirtyPackage(false);Edit.GetHeightDataFast(0,0,W-1,H-1,Actual.GetData(),W);
                const FString ExpectedName=RiverCount?TEXT("SourceArt/Terrain/HansaWorld/Prototype_20260918/SplineRivers_v1/terrain-final.r16"):TEXT("SourceArt/Terrain/HansaWorld/Prototype_20260918/terrain-final.r16");
                TArray<uint8> Expected;FFileHelper::LoadFileToArray(Expected,*(FPaths::ProjectDir()/ExpectedName));
                int32 MaxError=0;if(Expected.Num()==W*H*2)for(int32 N=0;N<W*H;++N)MaxError=FMath::Max(MaxError,FMath::Abs(int32(Actual[N])-int32(reinterpret_cast<uint16*>(Expected.GetData())[N])));
                Test->TestTrue(TEXT("Merged native Landscape within two height quanta"),Expected.Num()==W*H*2&&MaxError<=2);
                FFileHelper::SaveStringToFile(FString::Printf(TEXT("maximumHeightCodeError=%d\ncomponents=%d\n"),MaxError,L->GetLandscapeInfo()->XYtoComponentMap.Num()),*(Output/TEXT("native-readback.txt")));
            }
        }
        if(Stage>=Views.Num())
        {
            World->DestroyActor(Camera);
            if(FParse::Param(FCommandLine::Get(),TEXT("HansaWorldRepair"))&&!Test->HasAnyErrors())
            {
                // NullRHI authoring cannot merge edit layers. Persist the GPU-built surface and proxies.
                TArray<UPackage*> Packages;
                for(TActorIterator<AWaterBodyRiver> R(World);R;++R)if(R->Tags.Contains(TEXT("HansaWorld.SplineRiver.v1")))
                {R->MarkPackageDirty();Packages.AddUnique(R->GetPackage());}
                for(TActorIterator<ALandscapeProxy> L(World);L;++L)
                {
                    L->MarkPackageDirty();Packages.AddUnique(L->GetPackage());
                    for(ULandscapeComponent* Component:L->LandscapeComponents)if(auto* Heightmap=Component->GetHeightmap())
                    {Heightmap->MarkPackageDirty();Packages.AddUnique(Heightmap->GetPackage());}
                }
                for(auto* P:Packages)if(!P->GetName().Contains(TEXT("HansaWorld_20260918")))
                {Test->AddError(TEXT("Refusing to save unrelated terrain package"));return true;}
                const auto City=Manifest->GetArrayField(TEXT("cities"))[0]->AsObject();
                const FVector Location(City->GetNumberField(TEXT("x_cm")),City->GetNumberField(TEXT("y_cm"))+78000,City->GetNumberField(TEXT("z_cm"))+90000);
                for(auto* View:GEditor->GetLevelViewportClients())if(View&&View->IsPerspective())
                {View->SetViewLocation(Location);View->SetViewRotation(FRotator(-49.0856,-90,0));View->Invalidate();}
                Test->TestTrue(TEXT("Persist GPU-merged terrain packages"),UEditorLoadingAndSavingUtils::SavePackages(Packages,false));
                Test->TestTrue(TEXT("Persist authoring map camera"),UEditorLoadingAndSavingUtils::SaveMap(World,TEXT("/Game/Hansa/Generated/Staging/HansaWorld_20260918/L_HansaWorld_WP")));
            }
            return true;
        }
        if(!Prepared)
        {
        FVector Focus;float Distance=0;
        if(Stage==0)
        {Focus=FVector(3750000,3046875,0);Distance=12500000;}
        else
        {
            for(const auto& Value:Manifest->GetArrayField(TEXT("cities")))
            {const auto City=Value->AsObject();if(City->GetStringField(TEXT("id"))==TEXT("City.")+Views[Stage].Replace(TEXT("Close"),TEXT("")))
                Focus=FVector(City->GetNumberField(TEXT("x_cm")),City->GetNumberField(TEXT("y_cm")),City->GetNumberField(TEXT("z_cm")));}
            Distance=Views[Stage].EndsWith(TEXT("Close"))?30000:120000;
        }
        const FVector Position=Stage==0?Focus+FVector(0,0,Distance):Focus+FVector(0,Distance*.65,Distance*.75);
        Camera->SetActorLocationAndRotation(Position,Stage==0?FRotator(-90,-90,0):(Focus-Position).Rotation());
        if(Views[Stage].StartsWith(TEXT("River")))
        {
            const auto City=Manifest->GetArrayField(TEXT("cities"))[0]->AsObject();
            const FVector Centre(City->GetNumberField(TEXT("x_cm")),City->GetNumberField(TEXT("y_cm")),City->GetNumberField(TEXT("z_cm")));
            double Best=DBL_MAX;
            for(TActorIterator<AWaterBodyRiver> R(World);R;++R)
            {
                auto* S=R->GetWaterSpline();const FVector P=S->FindLocationClosestToWorldLocation(Centre,ESplineCoordinateSpace::World);
                if(FVector::DistSquared(P,Centre)<Best){Best=FVector::DistSquared(P,Centre);Focus=P;}
            }
            const FVector RiverPosition=Focus+FVector(0,14000,9500);
            Camera->SetActorLocationAndRotation(RiverPosition,(Focus-RiverPosition).Rotation());
            auto* RiverCapture=Camera->GetCaptureComponent2D();RiverCapture->HiddenActors.Empty();
            for(TActorIterator<AWaterBodyCustom> A(World);A;++A)
                if(Views[Stage]==TEXT("RiverOnly")||(Views[Stage]==TEXT("RiverNoSea")&&A->GetActorLabel().StartsWith(TEXT("SM_Sea_"))))RiverCapture->HiddenActors.Add(*A);
        }
        auto* Capture=Camera->GetCaptureComponent2D();
        // Overview geography evidence excludes aerial haze; local views retain the copied environment.
        Capture->ShowFlags.SetFog(Stage!=0);Capture->ShowFlags.SetAtmosphere(Stage!=0);
        World->SendAllEndOfFrameUpdates();Capture->CaptureScene();FlushRenderingCommands();
        Prepared=true;Ready=FPlatformTime::Seconds()+4;return false;
        }
        FReadSurfaceDataFlags ReadFlags(RCM_UNorm);ReadFlags.SetLinearToGamma(false);
        TArray<FColor> Pixels;Test->TestTrue(TEXT("Native map pixels"),Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels,ReadFlags));
        if(Pixels.Num()!=1920*1080){Test->AddError(TEXT("Invalid capture extent"));return true;}
        for(FColor& Pixel:Pixels)Pixel.A=255;
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(1920,1080,Pixels,Png);
        Test->TestTrue(TEXT("Saved actual world capture"),FFileHelper::SaveArrayToFile(Png,*(Output/Views[Stage]+TEXT(".png"))));
        ++Stage;Prepared=false;Ready=0;return false;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaWorldReviewTest,"Hansa.World.Campaign.Review",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaWorldReviewTest::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("HansaWorldReview"))){AddInfo(TEXT("Explicit -HansaWorldReview required; normal CI does not load staged campaign content."));return true;}
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHansaWorldReview>(this));return true;
}
#endif
