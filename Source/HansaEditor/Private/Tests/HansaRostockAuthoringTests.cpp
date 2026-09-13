#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "StaticMeshResources.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "LandscapeEditLayer.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/Package.h"
#include "WaterBodyCustomActor.h"
#include "WaterBodyCustomComponent.h"
#include "WaterZoneActor.h"
#include "World/HansaRostockQuarter.h"
namespace {
ALandscape* BuildRostockTerrain(UWorld* W,UMaterial* Ground)
{
    constexpr int Side=253;TArray<uint16> Heights;Heights.Reserve(Side*Side);
    for(int Y=0;Y<Side;++Y)for(int X=0;X<Side;++X)Heights.Add(FMath::RoundToInt(AHansaRostockQuarter::GroundHeight(-31500+Y*250)*128./25.+32768));
    auto* L=W->SpawnActor<ALandscape>(FVector(-31500,-31500,0),FRotator::ZeroRotator);L->SetActorScale3D(FVector(250,250,25));L->SetActorLabel(TEXT("Terrain.Rostock.InterpretedQuarter"));
    TMap<FGuid,TArray<uint16>> HL;HL.Add(FGuid(),MoveTemp(Heights));TMap<FGuid,TArray<FLandscapeImportLayerInfo>> ML;ML.Add(FGuid(),{});
    L->Import(FGuid::NewGuid(),0,0,252,252,1,63,HL,nullptr,ML,ELandscapeImportAlphamapType::Additive,TArrayView<const FLandscapeLayer>());
    L->LandscapeMaterial=Ground;if(auto* Layer=L->GetEditLayer(0)){Layer->SetName(TEXT("Interpreted_TradeQuarter"),true);Layer->SetLocked(true,true);}L->PostEditChange();L->ForceLayersFullUpdate();
    for(const auto& Pair:L->GetLandscapeInfo()->XYtoComponentMap){auto* C=Pair.Value;C->UpdateCachedBounds();C->UpdateBounds();C->MarkRenderStateDirty();C->MarkPackageDirty();}
    return L;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRostockAuthoring,"Hansa.World.Rostock.BuildCandidate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaRostockAuthoring::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("P31Authoring"))){AddInfo(TEXT("Explicit -P31Authoring required; no assets changed."));return true;}
    TArray<UPackage*> Dirty,Content;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
    if(!Dirty.IsEmpty()||!Content.IsEmpty()||GEditor->PlayWorld){AddError(TEXT("Refusing dirty editor or PIE"));return false;}
    const FString Root=TEXT("/Game/Hansa/Generated/Staging/Rostock_P31"),Map=Root/TEXT("L_Rostock_Quarter");
    if(FPackageName::DoesPackageExist(Map)){AddError(TEXT("Candidate already exists; refusing overwrite"));return false;}
    auto* SourceMaterial=LoadObject<UMaterial>(nullptr,TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground.M_Lubeck_Ground"));
    auto* SourceWater=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/SM_Lubeck_RiverSurface.SM_Lubeck_RiverSurface"));
    if(!SourceMaterial||!SourceWater){AddError(TEXT("P30 native ground/water authoring sources are required"));return false;}
    auto* Ground=DuplicateObject<UMaterial>(SourceMaterial,CreatePackage(*(Root/TEXT("M_Rostock_Ground"))),TEXT("M_Rostock_Ground"));Ground->SetFlags(RF_Public|RF_Standalone);Ground->PostEditChange();Ground->MarkPackageDirty();
    auto* Mesh=DuplicateObject<UStaticMesh>(SourceWater,CreatePackage(*(Root/TEXT("SM_Rostock_Warnow"))),TEXT("SM_Rostock_Warnow"));Mesh->SetFlags(RF_Public|RF_Standalone);Mesh->MarkPackageDirty();
    auto* WaterMI=NewObject<UMaterialInstanceConstant>(CreatePackage(*(Root/TEXT("MI_Rostock_Warnow"))),TEXT("MI_Rostock_Warnow"),RF_Public|RF_Standalone);
    WaterMI->SetParentEditorOnly(LoadObject<UMaterialInterface>(nullptr,TEXT("/Water/Materials/WaterSurface/Water_Material_CustomMesh.Water_Material_CustomMesh")));WaterMI->PostEditChange();WaterMI->MarkPackageDirty();
    UWorld* W=GEditor->NewMap(false);if(!W)return false;
    auto* Quarter=W->SpawnActor<AHansaRostockQuarter>();Quarter->SetActorLabel(TEXT("City.Rostock.TradeQuarter"));
    BuildRostockTerrain(W,Ground);
    auto* Water=W->SpawnActor<AWaterBodyCustom>(FVector(0,0,-125),FRotator::ZeroRotator);Water->Tags.Add(TEXT("Water.Rostock.Warnow"));Water->GetWaterBodyComponent()->SetWaterMaterial(WaterMI);Water->GetWaterBodyComponent()->SetWaterMeshOverride(Mesh);Water->GetWaterBodyComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);Water->GetWaterBodyComponent()->SetCanEverAffectNavigation(false);Water->PostEditChange();
    W->SpawnActor<AWaterZone>();
    if(!UEditorLoadingAndSavingUtils::SaveMap(W,Map))return false;
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);for(auto* P:Content)Dirty.AddUnique(P);
    return TestTrue(TEXT("Save interpreted remote quarter candidate"),UEditorLoadingAndSavingUtils::SavePackages(Dirty,true));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRostockRevision,"Hansa.World.Rostock.ReviseCandidate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaRostockRevision::RunTest(const FString&)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("P31Authoring"))){AddInfo(TEXT("Explicit authoring flag required"));return true;}
    TArray<UPackage*> Dirty,Content;UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);
    if(!Dirty.IsEmpty()||!Content.IsEmpty()||GEditor->PlayWorld)return false;
    const FString Root=TEXT("/Game/Hansa/Generated/Staging/Rostock_P31");if(!FEditorFileUtils::LoadMap(Root/TEXT("L_Rostock_Quarter"),false,true))return false;
    UWorld* W=GEditor->GetEditorWorldContext().World();
    auto* Ground=LoadObject<UMaterial>(nullptr,*(Root/TEXT("M_Rostock_Ground.M_Rostock_Ground")));auto* MI=LoadObject<UMaterialInstanceConstant>(nullptr,*(Root/TEXT("MI_Rostock_Warnow.MI_Rostock_Warnow")));
    if(!Ground||!MI)return false;
    // Native single-layer volume shading; no fluid-simulation/WaterInfo texture dependency for this custom surface.
    auto* WaterMaterial=LoadObject<UMaterial>(nullptr,*(Root/TEXT("M_Rostock_Warnow.M_Rostock_Warnow")));
    if(!WaterMaterial)
    {
        WaterMaterial=NewObject<UMaterial>(CreatePackage(*(Root/TEXT("M_Rostock_Warnow"))),TEXT("M_Rostock_Warnow"),RF_Public|RF_Standalone);
        WaterMaterial->SetShadingModel(MSM_SingleLayerWater);WaterMaterial->BlendMode=BLEND_Opaque;
        auto Scalar=[&](float V){auto* E=NewObject<UMaterialExpressionConstant>(WaterMaterial);E->R=V;WaterMaterial->GetExpressionCollection().AddExpression(E);return E;};
        auto Vector=[&](FLinearColor V){auto* E=NewObject<UMaterialExpressionConstant3Vector>(WaterMaterial);E->Constant=V;WaterMaterial->GetExpressionCollection().AddExpression(E);return E;};
        auto* Data=WaterMaterial->GetEditorOnlyData();Data->BaseColor.Connect(0,Vector(FLinearColor(.018f,.055f,.064f)));Data->Roughness.Connect(0,Scalar(.24f));Data->Specular.Connect(0,Scalar(.5f));Data->Opacity.Connect(0,Scalar(.15f));
        auto* Volume=NewObject<UMaterialExpressionSingleLayerWaterMaterialOutput>(WaterMaterial);WaterMaterial->GetExpressionCollection().AddExpression(Volume);
        Volume->ScatteringCoefficients.Connect(0,Vector(FLinearColor(.0012f,.003f,.0034f)));Volume->AbsorptionCoefficients.Connect(0,Vector(FLinearColor(.009f,.0045f,.0035f)));Volume->PhaseG.Connect(0,Scalar(.2f));
        auto* P=NewObject<UMaterialExpressionWorldPosition>(WaterMaterial);WaterMaterial->GetExpressionCollection().AddExpression(P);
        auto* T=NewObject<UMaterialExpressionTime>(WaterMaterial);WaterMaterial->GetExpressionCollection().AddExpression(T);
        auto* N=NewObject<UMaterialExpressionCustom>(WaterMaterial);WaterMaterial->GetExpressionCollection().AddExpression(N);N->OutputType=CMOT_Float3;
        FCustomInput PositionInput;PositionInput.InputName=TEXT("P");PositionInput.Input.Connect(0,P);N->Inputs.Add(PositionInput);FCustomInput TI;TI.InputName=TEXT("T");TI.Input.Connect(0,T);N->Inputs.Add(TI);
        N->Code=TEXT("return normalize(float3(.08*sin(P.x/47.+T*.6)+.04*sin(P.y/89.-T*.4), .07*cos(P.y/61.+T*.5), 1.));");Data->Normal.Connect(0,N);
        WaterMaterial->PostEditChange();WaterMaterial->MarkPackageDirty();
    }
    auto* WaterMesh=LoadObject<UStaticMesh>(nullptr,*(Root/TEXT("SM_Rostock_Warnow.SM_Rostock_Warnow")));
    if(!WaterMesh)return false;
    FMeshDescription Surface;FStaticMeshAttributes Attributes(Surface);Attributes.Register();auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();auto UV=Attributes.GetVertexInstanceUVs();UV.SetNumChannels(1);
    TArray<FVertexInstanceID> Corners;
    // Clockwise from above is the Unreal front face. Bounded to this quarter; no overlap with Lübeck.
    for(FVector3f P:{FVector3f(-31500,-31500,0),FVector3f(-31500,31500,0),FVector3f(31500,31500,0),FVector3f(31500,-31500,0)})
    {auto V=Surface.CreateVertex();Positions[V]=P;auto VI=Surface.CreateVertexInstance(V);Normals[VI]=FVector3f(0,0,1);UV.Set(VI,0,FVector2f(P.X/400,P.Y/400));Corners.Add(VI);}
    Surface.CreatePolygon(Surface.CreatePolygonGroup(),Corners);WaterMesh->CreateMeshDescription(0,MoveTemp(Surface));WaterMesh->CommitMeshDescription(0);WaterMesh->Build(false);WaterMesh->PostEditChange();WaterMesh->MarkPackageDirty();
    MI->SetParentEditorOnly(WaterMaterial);MI->PostEditChange();MI->MarkPackageDirty();

    Ground->PostEditChange();Ground->MarkPackageDirty();
    TArray<ALandscape*> PreviousTerrain;for(TActorIterator<ALandscape> I(W);I;++I)PreviousTerrain.Add(*I);
    for(auto* L:PreviousTerrain)W->DestroyActor(L);
    auto* Terrain=BuildRostockTerrain(W,Ground);for(auto C:Terrain->LandscapeComponents){C->UpdateMaterialInstances();C->UpdateCachedBounds();C->UpdateBounds();C->MarkRenderStateDirty();C->MarkPackageDirty();}Terrain->MarkPackageDirty();
    for(TActorIterator<AWaterBodyCustom> I(W);I;++I){I->GetWaterBodyComponent()->SetWaterMaterial(MI);I->PostEditChange();I->MarkPackageDirty();}
    for(TActorIterator<AHansaRostockQuarter> I(W);I;++I){I->RerunConstructionScripts();I->MarkPackageDirty();
        for(auto C:I->Modules)for(int Slot=0;Slot<C->GetNumMaterials();++Slot)if(auto* Interface=C->GetMaterial(Slot))if(auto* Material=Interface->GetMaterial())
        {if(Material->GetPathName().StartsWith(TEXT("/Game/Mesh/"))&&!Material->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes)){Material->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);Material->MarkPackageDirty();}}
}
    UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Dirty);UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Content);for(auto* P:Content)Dirty.AddUnique(P);
    return TestTrue(TEXT("Save corrected candidate materials"),UEditorLoadingAndSavingUtils::SavePackages(Dirty,true));
}
#endif
