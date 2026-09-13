#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "LandscapeComponent.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/WorldSettings.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeSubsystem.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "EngineUtils.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionCustom.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "WaterBodyCustomActor.h"
#include "WaterBodyCustomComponent.h"
#include "WaterZoneActor.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckWorldArt.h"
#include "World/HansaGameMode.h"

namespace
{
    const FString Root(TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30"));
    bool Save(UObject* Asset)
    {
        FAssetRegistryModule::AssetCreated(Asset);Asset->MarkPackageDirty();
        FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
        return UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    }
    template<class T> T* Expression(UMaterial* M)
    {
        auto* E=NewObject<T>(M);M->GetExpressionCollection().AddExpression(E);return E;
    }
    UMaterial* GroundMaterial()
    {
        auto* M=NewObject<UMaterial>(CreatePackage(*(Root/TEXT("M_Lubeck_Ground"))),TEXT("M_Lubeck_Ground"),RF_Public|RF_Standalone);
        auto* Pos=Expression<UMaterialExpressionWorldPosition>(M);
        auto* XY=Expression<UMaterialExpressionComponentMask>(M);XY->R=true;XY->G=true;XY->Input.Connect(0,Pos);
        auto* Scale=Expression<UMaterialExpressionConstant>(M);Scale->R=.0025f; // approved native 4 m P18 source scale
        auto* UV=Expression<UMaterialExpressionMultiply>(M);UV->A.Connect(0,XY);UV->B.Connect(0,Scale);
        auto Sample=[&](const TCHAR* Name,EMaterialSamplerType Type){auto* E=Expression<UMaterialExpressionTextureSample>(M);E->Texture=LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("/Game/Mesh/hansa-dirt-road/Textures/%s.%s"),Name,Name));E->SamplerType=Type;E->Coordinates.Connect(0,UV);return E;};
        auto* Base=Sample(TEXT("T_Road_BaseColor"),SAMPLERTYPE_Color);
        auto* Normal=Sample(TEXT("T_Road_Normal"),SAMPLERTYPE_Normal);
        auto* Rough=Sample(TEXT("T_Road_Roughness"),SAMPLERTYPE_Masks);
        auto* Tint=Expression<UMaterialExpressionCustom>(M);Tint->OutputType=CMOT_Float3;
        FCustomInput P;P.InputName=TEXT("P");P.Input.Connect(0,Pos);Tint->Inputs.Add(P);
        FCustomInput C;C.InputName=TEXT("Soil");C.Input.Connect(0,Base);Tint->Inputs.Add(C);
        // Restrained macro breakup and bank moisture. This is gameplay grading, never a historical cover claim.
        Tint->Code=TEXT("float macro=0.94+0.06*sin(P.x/3700.0)*sin(P.y/5100.0); float dry=saturate((P.z+40.0)/110.0); return Soil*lerp(float3(.40,.43,.38),float3(.74,.80,.55),dry)*macro;");
        M->GetEditorOnlyData()->BaseColor.Connect(0,Tint);M->GetEditorOnlyData()->Normal.Connect(0,Normal);M->GetEditorOnlyData()->Roughness.Connect(0,Rough);
        M->PostEditChange();return Save(M)?M:nullptr;
    }
    UStaticMesh* WaterMesh()
    {
        FMeshDescription D;FStaticMeshAttributes A(D);A.Register();auto Positions=A.GetVertexPositions();auto Normals=A.GetVertexInstanceNormals();auto UVs=A.GetVertexInstanceUVs();UVs.SetNumChannels(1);
        const FPolygonGroupID G=D.CreatePolygonGroup();TArray<FVertexInstanceID> V;
        for(const FVector3f P:{FVector3f(-100000,-100000,0),FVector3f(100000,-100000,0),FVector3f(100000,100000,0),FVector3f(-100000,100000,0)})
        {
            auto I=D.CreateVertex();Positions[I]=P;auto VI=D.CreateVertexInstance(I);Normals[VI]=FVector3f(0,0,1);UVs.Set(VI,0,FVector2f(P.X/400,P.Y/400));V.Add(VI);
        }
        D.CreatePolygon(G,V);
        auto* M=NewObject<UStaticMesh>(CreatePackage(*(Root/TEXT("SM_Lubeck_RiverSurface"))),TEXT("SM_Lubeck_RiverSurface"),RF_Public|RF_Standalone);
        M->GetStaticMaterials().Add(FStaticMaterial());M->SetNumSourceModels(1);M->CreateMeshDescription(0,MoveTemp(D));M->CommitMeshDescription(0);M->Build(false);M->PostEditChange();return Save(M)?M:nullptr;
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckWorldArtAuthoring,"Hansa.World.LubeckArt.BuildCandidate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLubeckWorldArtAuthoring::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("P30Authoring"))){AddInfo(TEXT("Read-only normal test run; -P30Authoring is required."));return true;}
    TArray<UPackage*> Dirty;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);TArray<UPackage*> Content;UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
    if(!Dirty.IsEmpty()||!Content.IsEmpty()||GEditor->PlayWorld){AddError(TEXT("Requires clean editor outside PIE; no user work is saved or discarded."));return false;}
    const FString Map=Root/TEXT("L_Lubeck_WorldArt_Candidate");
    if(FPackageName::DoesPackageExist(Map)||FPackageName::DoesPackageExist(Root/TEXT("M_Lubeck_Ground"))){AddError(TEXT("Candidate destination exists; inspect it instead of overwriting."));return false;}
    auto* Material=GroundMaterial();auto* Mesh=WaterMesh();if(!Material||!Mesh){AddError(TEXT("Candidate material/mesh creation failed"));return false;}
    UWorld* W=GEditor->NewMap(true);if(!W||!W->GetWorldPartition()){AddError(TEXT("World Partition map required"));return false;}
    W->GetWorldSettings()->DefaultGameMode=AHansaGameMode::StaticClass();
    auto* F=W->SpawnActor<AHansaLubeckWorldFoundation>();F->bUseAuthoredWorld=true;F->RerunConstructionScripts();
    W->SpawnActor<AHansaLubeckAutomationStart>(AHansaLubeckAutomationStart::StaticClass(),Hansa::Game::LubeckMap::AutomationStartTransform());
    W->SpawnActor<AHansaLubeckWorldArt>();
    constexpr int Side=505;TArray<uint16> Heights;Heights.Reserve(Side*Side);
    for(int Y=0;Y<Side;++Y)for(int X=0;X<Side;++X)Heights.Add(FMath::Clamp(FMath::RoundToInt(Hansa::Game::LubeckWorldArt::GroundHeight(FVector2D(-25200+X*100,-25200+Y*100))*128./25.+32768.),0,65535));
    auto* L=W->SpawnActor<ALandscape>(FVector(-25200,-25200,0),FRotator::ZeroRotator);L->SetActorScale3D(FVector(100,100,25));L->SetActorLabel(TEXT("Terrain.Lubeck.GameplayGrading"));L->Tags.Add(TEXT("Presentation.Terrain.Lubeck.GameplayGrading"));
    TMap<FGuid,TArray<uint16>> HL;HL.Add(FGuid(),MoveTemp(Heights));TMap<FGuid,TArray<FLandscapeImportLayerInfo>> ML;ML.Add(FGuid(),{});
    L->Import(FGuid::NewGuid(),0,0,Side-1,Side-1,1,63,HL,nullptr,ML,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
    L->LandscapeMaterial=Material;if(auto* Layer=L->GetEditLayer(0)){Layer->SetName(TEXT("Gameplay_Grading"),true);Layer->SetLocked(true,true);}L->PostEditChange();
    W->GetSubsystem<ULandscapeSubsystem>()->ChangeGridSize(L->GetLandscapeInfo(),4);
    auto* Water=W->SpawnActor<AWaterBodyCustom>(FVector(0,0,Hansa::Game::LubeckWorldArt::WaterHeight),FRotator::ZeroRotator);Water->SetActorLabel(TEXT("Water.Lubeck.Trave.Gameplay"));Water->Tags.Add(TEXT("Water.Lubeck.Trave.Gameplay"));
    Water->GetWaterBodyComponent()->SetWaterMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Water/Materials/WaterSurface/Water_Material_CustomMesh.Water_Material_CustomMesh")));Water->GetWaterBodyComponent()->SetWaterMeshOverride(Mesh);Water->GetWaterBodyComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);Water->GetWaterBodyComponent()->SetCanEverAffectNavigation(false);Water->PostEditChange();
    auto* Zone=W->SpawnActor<AWaterZone>();Zone->SetActorLabel(TEXT("WaterZone.Lubeck.Gameplay"));
    if(!UEditorLoadingAndSavingUtils::SaveMap(W,Map)){AddError(TEXT("Candidate map save failed"));return false;}
    TArray<UPackage*> Packages;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Packages);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);for(auto* P:Content)Packages.AddUnique(P);
    if(!UEditorLoadingAndSavingUtils::SavePackages(Packages,true)){AddError(TEXT("External Landscape actor save failed"));return false;}
    AddInfo(TEXT("Saved staged gameplay-coordinate Landscape and native Water candidate; no production map, survey or gameplay definition changed."));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckArtRevision,"Hansa.World.LubeckArt.ReviseCandidate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLubeckArtRevision::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("P30Authoring"))){AddInfo(TEXT("Explicit -P30Authoring required."));return true;}
    TArray<UPackage*> Dirty,Content;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
    if(!Dirty.IsEmpty()||!Content.IsEmpty()||GEditor->PlayWorld){AddError(TEXT("Refusing dirty editor/PIE"));return false;}
    const FString Map=Root/TEXT("L_Lubeck_WorldArt_Candidate");if(!FEditorFileUtils::LoadMap(Map,false,true)){AddError(TEXT("Candidate not found"));return false;}
    UWorld* W=GEditor->GetEditorWorldContext().World();
    FWorldPartitionHelpers::FForEachActorWithLoadingParams LoadParams;LoadParams.bKeepReferences=true;
    FWorldPartitionHelpers::FForEachActorWithLoadingResult LoadedActors;
    FWorldPartitionHelpers::ForEachActorWithLoading(W->GetWorldPartition(),[](const FWorldPartitionActorDescInstance*){return true;},LoadParams,LoadedActors);

    auto* M=LoadObject<UMaterial>(nullptr,*(Root/TEXT("M_Lubeck_Ground.M_Lubeck_Ground")));if(!M)return false;
    for(UMaterialExpression* E:M->GetExpressionCollection().Expressions)if(auto* S=Cast<UMaterialExpressionTextureSample>(E))if(S->Texture&&S->Texture->GetName()==TEXT("T_Road_Roughness"))S->SamplerType=SAMPLERTYPE_Masks;
    M->PostEditChange();M->MarkPackageDirty();
    auto* Parent=LoadObject<UMaterialInterface>(nullptr,TEXT("/Water/Materials/WaterSurface/Water_Material_CustomMesh.Water_Material_CustomMesh"));if(!Parent){AddError(TEXT("Native Water custom-mesh material unavailable"));return false;}
    auto* MI=LoadObject<UMaterialInstanceConstant>(nullptr,*(Root/TEXT("MI_Lubeck_Trave.MI_Lubeck_Trave")));
    if(!MI){MI=NewObject<UMaterialInstanceConstant>(CreatePackage(*(Root/TEXT("MI_Lubeck_Trave"))),TEXT("MI_Lubeck_Trave"),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(MI);}
    MI->SetParentEditorOnly(Parent);MI->PostEditChange();MI->MarkPackageDirty();
    for(TActorIterator<AWaterBodyCustom> I(W);I;++I){I->GetWaterBodyComponent()->SetWaterMaterial(MI);I->PostEditChange();I->MarkPackageDirty();}
    for(TActorIterator<AHansaLubeckWorldArt> I(W);I;++I){I->Exposure->Settings.AutoExposureMinBrightness=13;I->Exposure->Settings.AutoExposureMaxBrightness=13;I->MarkPackageDirty();}
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);for(auto* P:Content)Dirty.AddUnique(P);
    return TestTrue(TEXT("Save only reviewed candidate material/exposure changes"),UEditorLoadingAndSavingUtils::SavePackages(Dirty,true));
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckArtBake,"Hansa.World.LubeckArt.BakeCandidate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FLubeckArtBake::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("P30Authoring"))){AddInfo(TEXT("Explicit -P30Authoring required."));return true;}
    TArray<UPackage*> Dirty,Content;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
    if(!Dirty.IsEmpty()||!Content.IsEmpty()||GEditor->PlayWorld){AddError(TEXT("Refusing dirty editor/PIE"));return false;}
    if(!FEditorFileUtils::LoadMap(Root/TEXT("L_Lubeck_WorldArt_Candidate"),false,true))return false;
    UWorld* W=GEditor->GetEditorWorldContext().World();
    FWorldPartitionHelpers::FForEachActorWithLoadingParams LoadParams;LoadParams.bKeepReferences=true;
    FWorldPartitionHelpers::FForEachActorWithLoadingResult LoadedActors;
    FWorldPartitionHelpers::ForEachActorWithLoading(W->GetWorldPartition(),[](const FWorldPartitionActorDescInstance*){return true;},LoadParams,LoadedActors);
int Landscapes=0;
    for(TActorIterator<ALandscape> I(W);I;++I)
    {
        ++Landscapes;I->ForceLayersFullUpdate();
        for(const auto& Pair:I->GetLandscapeInfo()->XYtoComponentMap)
        {
            auto* Component=Pair.Value;Component->UpdateCachedBounds();Component->UpdateBounds();Component->MarkRenderStateDirty();Component->MarkPackageDirty();
        }
        TArray<uint16> H;H.SetNumZeroed(505*505);FLandscapeEditDataInterface Edit(I->GetLandscapeInfo(),FGuid(),false);Edit.SetShouldDirtyPackage(false);Edit.GetHeightDataFast(0,0,504,504,H.GetData(),505);
        for(int Y=0;Y<505;Y+=63)for(int X=0;X<505;X+=63)
        {
            const double Expected=Hansa::Game::LubeckWorldArt::GroundHeight(FVector2D(-25200+X*100,-25200+Y*100));
            const double Actual=(double(H[Y*505+X])-32768.)*25./128.;
            if(Y==0||X==252)AddInfo(FString::Printf(TEXT("height %d,%d expected %.2f actual %.2f components %d"),X,Y,Expected,Actual,I->GetLandscapeInfo()->XYtoComponentMap.Num()));
            TestTrue(TEXT("Baked Landscape equals grading within encoding precision"),FMath::Abs(Expected-Actual)<.2);
        }
    }
    TestEqual(TEXT("Exactly one Landscape"),Landscapes,1);
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);for(auto* P:Content)Dirty.AddUnique(P);
    return TestTrue(TEXT("Save baked Landscape and collision"),UEditorLoadingAndSavingUtils::SavePackages(Dirty,true))&&!HasAnyErrors();
}
#endif
