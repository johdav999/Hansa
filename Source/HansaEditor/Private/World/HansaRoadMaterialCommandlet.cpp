#include "World/HansaRoadMaterialCommandlet.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionVirtualTextureFeatureSwitch.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
template<class T> T* Node(UMaterial* M)
{
    T* N = NewObject<T>(M);
    M->GetExpressionCollection().AddExpression(N);
    return N;
}
bool Save(UObject* Object)
{
    UPackage* Package = Object->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
    if (IFileManager::Get().FileExists(*Filename))
    {
        const FString Backup = FPaths::ProjectSavedDir()/TEXT("RoadTerrain/Before")/FPackageName::GetShortName(Package)/TEXT("original.uasset");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
        if (!IFileManager::Get().FileExists(*Backup)) IFileManager::Get().Copy(*Backup,*Filename);
    }
    Package->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Package,Object,*Filename,Args);
}
UMaterialExpressionScalarParameter* Scalar(UMaterial* M, const TCHAR* Name, float Default)
{
    auto* N=Node<UMaterialExpressionScalarParameter>(M); N->ParameterName=Name; N->DefaultValue=Default; return N;
}
bool HasMarker(UMaterial* M)
{
    for (UMaterialExpression* E:M->GetExpressions()) if (E && E->Desc==TEXT("Hansa road terrain v1")) return true;
    return false;
}
}

UHansaRoadMaterialCommandlet::UHansaRoadMaterialCommandlet()
{
    IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}

int32 UHansaRoadMaterialCommandlet::Main(const FString& Params)
{
    const FString Root=TEXT("/Game/Mesh/hansa-dirt-road/Materials/");
    auto* Source=LoadObject<UMaterial>(nullptr,*(Root+TEXT("M_Road_Earth.M_Road_Earth")));
    if (!Source) return 1;
    auto* RVT=LoadObject<URuntimeVirtualTexture>(nullptr,*(Root+TEXT("RVT_HansaRoad.RVT_HansaRoad")));
    if (!RVT)
    {
        RVT=NewObject<URuntimeVirtualTexture>(CreatePackage(*(Root+TEXT("RVT_HansaRoad"))),TEXT("RVT_HansaRoad"),RF_Public|RF_Standalone);
        FAssetRegistryModule::AssetCreated(RVT);
    }
    auto SetProperty=[&](const TCHAR* Name,const TCHAR* Value)
    {
        FProperty* P=RVT->GetClass()->FindPropertyByName(Name);
        return P && P->ImportText_Direct(Value,P->ContainerPtrToValuePtr<void>(RVT),RVT,PPF_None);
    };
    if (!SetProperty(TEXT("MaterialType"),TEXT("BaseColor_Normal_Specular_Mask_YCoCg"))) return 2;
    SetProperty(TEXT("TileCount"),TEXT("8")); SetProperty(TEXT("TileSize"),TEXT("1"));
    RVT->PostEditChange();
    if (!Save(RVT)) return 3;
    auto* Road=LoadObject<UMaterial>(nullptr,*(Root+TEXT("M_Road_Terrain.M_Road_Terrain")));
    if (!Road)
    {
        Road=DuplicateObject<UMaterial>(Source,CreatePackage(*(Root+TEXT("M_Road_Terrain"))),TEXT("M_Road_Terrain"));
        Road->SetFlags(RF_Public|RF_Standalone); FAssetRegistryModule::AssetCreated(Road);
    }
    if (!HasMarker(Road))
    {
        auto* Height=Node<UMaterialExpressionTextureObjectParameter>(Road);
        Height->ParameterName=TEXT("RoadHeightField");
        Height->Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
        Height->SamplerType=SAMPLERTYPE_LinearColor;
        auto* X=Node<UMaterialExpressionVectorParameter>(Road);X->ParameterName=TEXT("RoadLocalX"); X->DefaultValue=FLinearColor(1,0,0,0);
        auto* Y=Node<UMaterialExpressionVectorParameter>(Road);Y->ParameterName=TEXT("RoadLocalY"); Y->DefaultValue=FLinearColor(0,1,0,0);
        auto* Position=Node<UMaterialExpressionWorldPosition>(Road);Position->WorldPositionShaderOffset=WPT_ExcludeAllShaderOffsets;
        auto* Vertex=Node<UMaterialExpressionVertexColor>(Road);
        auto* Fit=Node<UMaterialExpressionCustom>(Road);Fit->Desc=TEXT("Hansa road terrain v1"); Fit->OutputType=CMOT_Float3;
        const TArray<FName> Names={TEXT("HeightField"),TEXT("Position"),TEXT("LocalX"),TEXT("LocalY"),TEXT("Datum"),TEXT("Clearance"),TEXT("Enabled"),TEXT("Alpha")};
        Fit->Inputs.Reset(); for(FName Name:Names) {FCustomInput Input;Input.InputName=Name;Fit->Inputs.Add(Input);}
        Fit->Inputs[0].Input.Connect(0,Height);Fit->Inputs[1].Input.Connect(0,Position);
        Fit->Inputs[2].Input.Connect(5,X);Fit->Inputs[3].Input.Connect(5,Y);
        Fit->Inputs[4].Input.Connect(0,Scalar(Road,TEXT("RoadDatumZ"),0));
        Fit->Inputs[5].Input.Connect(0,Scalar(Road,TEXT("RoadClearance"),3));
        Fit->Inputs[6].Input.Connect(0,Scalar(Road,TEXT("RoadFitEnabled"),0));Fit->Inputs[7].Input.Connect(4,Vertex);
        Fit->Code=TEXT("float2 p=float2(dot(float4(Position,1),LocalX),dot(float4(Position,1),LocalY));\nfloat2 uv=((p+212.5)/12.5+0.5)/35.0;\nfloat z=Texture2DSampleLevel(HeightField,HeightFieldSampler,uv,0).r+Datum;\nreturn float3(0,0,(z+Clearance+2*saturate(Alpha)-Position.z)*Enabled);");
        Road->GetExpressionInputForProperty(MP_WorldPositionOffset)->Connect(0,Fit);
        auto* Out=Node<UMaterialExpressionRuntimeVirtualTextureOutput>(Road);
        Out->BaseColor=*Road->GetExpressionInputForProperty(MP_BaseColor);
        Out->Roughness=*Road->GetExpressionInputForProperty(MP_Roughness);
        auto* One=Node<UMaterialExpressionConstant>(Road);One->R=1;
        Out->Mask.Connect(0,One); Out->Opacity.Connect(4,Vertex);
        Road->bUsedWithSplineMeshes=true;
        Road->MaxWorldPositionOffsetDisplacement=0;
        Road->PostEditChange();
        if (!Save(Road)) return 4;
    }
    // Repair the coordinate-vector output on existing graph revisions as well.
    const TArray<TObjectPtr<UMaterialExpression>> RoadExpressions(Road->GetExpressions());
    for (UMaterialExpression* E:RoadExpressions)
        if (auto* Fit=Cast<UMaterialExpressionCustom>(E); Fit && Fit->Desc==TEXT("Hansa road terrain v1"))
        {
            Fit->Inputs[2].Input.OutputIndex=5; Fit->Inputs[3].Input.OutputIndex=5;
            if(Fit->Inputs.Num()==8)
            {
                const TArray<FName> Names={TEXT("SourcePosition"),TEXT("WorldX"),TEXT("WorldY"),TEXT("Section")};
                for(FName Name:Names){FCustomInput Input;Input.InputName=Name;Fit->Inputs.Add(Input);}
                auto* SourcePosition=Node<UMaterialExpressionPreSkinnedPosition>(Road);
                auto* WorldX=Node<UMaterialExpressionVectorParameter>(Road);WorldX->ParameterName=TEXT("RoadWorldX");WorldX->DefaultValue=FLinearColor(1,0,0,0);
                auto* WorldY=Node<UMaterialExpressionVectorParameter>(Road);WorldY->ParameterName=TEXT("RoadWorldY");WorldY->DefaultValue=FLinearColor(0,1,0,0);
                auto* Section=Node<UMaterialExpressionVectorParameter>(Road);Section->ParameterName=TEXT("RoadSection");Section->DefaultValue=FLinearColor(1,0,0,0);
                Fit->Inputs[8].Input.Connect(0,SourcePosition);Fit->Inputs[9].Input.Connect(5,WorldX);
                Fit->Inputs[10].Input.Connect(5,WorldY);Fit->Inputs[11].Input.Connect(5,Section);
            }
            Fit->Code=TEXT("float2 p=float2(SourcePosition.x*Section.x+Section.y,SourcePosition.y);\nfloat2 worldXY=float2(dot(float3(p,1),WorldX.xyz),dot(float3(p,1),WorldY.xyz));\nfloat2 uv=((p+212.5)/12.5+0.5)/35.0;\nfloat z=Texture2DSampleLevel(HeightField,HeightFieldSampler,uv,0).r+Datum;\nreturn (float3(worldXY,z+Clearance+2*saturate(Alpha))-Position)*Enabled;");
        }
    Road->PostEditChange(); if(!Save(Road))return 4;
    // These existing Landscape masters keep their original inputs and parameter instances.
    // Only the road writes into this RVT; Landscapes only read it, avoiding feedback.
    const TCHAR* Landscapes[]={
        TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/M_Terrain_Hansa_Master.M_Terrain_Hansa_Master"),
        TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground.M_Lubeck_Ground")};
    for (const TCHAR* Path:Landscapes)
    {
        auto* Land=LoadObject<UMaterial>(nullptr,Path);
        if (!Land) return 5;
        if (HasMarker(Land)) continue;
        if (Land->bUseMaterialAttributes) {UE_LOG(LogTemp,Error,TEXT("Road integration requires explicit material attribute handling for %s"),Path);return 6;}
        auto* Sample=Node<UMaterialExpressionRuntimeVirtualTextureSample>(Land);
        Sample->Desc=TEXT("Hansa road terrain v1");Sample->VirtualTexture=RVT;Sample->MaterialType=RVT->GetMaterialType();
        // Sample outputs are identified by name rather than engine-version-specific numeric slots.
        auto Output=[&](FName Name) -> int32 {const auto& Outputs=Sample->GetOutputs();for(int32 I=0;I<Outputs.Num();++I)if(Outputs[I].OutputName==Name)return I;return INDEX_NONE;};
        const int32 Color=Output(TEXT("BaseColor")),Mask=Output(TEXT("Mask")),Roughness=Output(TEXT("Roughness"));
        if (Color<0 || Mask<0 || Roughness<0) {UE_LOG(LogTemp,Error,TEXT("Unexpected RVT sample outputs"));return 7;}
        for (const auto Property:{MP_BaseColor,MP_Roughness})
        {
            auto* Input=Land->GetExpressionInputForProperty(Property);
            auto* Blend=Node<UMaterialExpressionLinearInterpolate>(Land);Blend->A=*Input;
            if (!Blend->A.Expression) {auto* Default=Node<UMaterialExpressionConstant>(Land);Default->R=Property==MP_Roughness?.8f:0;Blend->A.Connect(0,Default);}
            Blend->B.Connect(Property==MP_BaseColor?Color:Roughness,Sample);Blend->Alpha.Connect(Mask,Sample);
            auto* Switch=Node<UMaterialExpressionVirtualTextureFeatureSwitch>(Land);Switch->No=Blend->A;Switch->Yes.Connect(0,Blend);Input->Connect(0,Switch);
        }
        Land->PostEditChange();if(!Save(Land))return 8;
    }
    return 0;
}
