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
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionTextureSample.h"
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
bool SaveRoadAsset(UObject* Object)
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
template<class T> T* FindByDesc(UMaterial* M, const TCHAR* Desc)
{
    for (UMaterialExpression* E:M->GetExpressions()) if (auto* Match=Cast<T>(E); Match && Match->Desc==Desc) return Match;
    return nullptr;
}
int32 OutputIndex(UMaterialExpression* Expression, FName Name)
{
    const auto& Outputs=Expression->GetOutputs();
    for(int32 I=0;I<Outputs.Num();++I) if(Outputs[I].OutputName==Name) return I;
    return INDEX_NONE;
}
void SetTextureEncoding(UTexture2D* Texture, bool bSRGB, TextureCompressionSettings Compression)
{
    Texture->Modify();
    Texture->SRGB=bSRGB;
    Texture->CompressionSettings=Compression;
    Texture->PostEditChange();
}
}

#include "HansaGroundSurfaceAuthoring.inl"

UHansaRoadMaterialCommandlet::UHansaRoadMaterialCommandlet()
{
    IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}

int32 UHansaRoadMaterialCommandlet::Main(const FString& Params)
{
    const FString MaterialsRoot=TEXT("/Game/Mesh/hansa-dirt-road/Materials/");
    const FString RoadTexturesRoot=TEXT("/Game/Mesh/hansa-dirt-road/Textures/");
    const FString TerrainTexturesRoot=TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Textures/");
    auto LoadTexture=[](const FString& Root,const TCHAR* Name)
    {
        return LoadObject<UTexture2D>(nullptr,*FString::Printf(TEXT("%s%s.%s"),*Root,Name,Name));
    };
    auto* RoadBase=LoadTexture(RoadTexturesRoot,TEXT("T_Road_BaseColor"));
    auto* RoadNormal=LoadTexture(RoadTexturesRoot,TEXT("T_Road_Normal"));
    auto* RoadRoughness=LoadTexture(RoadTexturesRoot,TEXT("T_Road_Roughness"));
    auto* GrassLoam=LoadTexture(TerrainTexturesRoot,TEXT("T_Terrain_Lubeck_GrassLoam"));
    auto* BankLoam=LoadTexture(TerrainTexturesRoot,TEXT("T_Terrain_Lubeck_BankLoam"));
    auto* ShoreWetness=LoadTexture(TerrainTexturesRoot,TEXT("T_Terrain_Lubeck_ShoreWetness"));
    if(!RoadBase||!RoadNormal||!RoadRoughness||!GrassLoam||!BankLoam||!ShoreWetness) return 1;
    SetTextureEncoding(RoadBase,true,TC_Default);SetTextureEncoding(RoadNormal,false,TC_Normalmap);SetTextureEncoding(RoadRoughness,false,TC_Masks);
    SetTextureEncoding(GrassLoam,true,TC_Default);SetTextureEncoding(BankLoam,true,TC_Default);SetTextureEncoding(ShoreWetness,false,TC_Masks);
    for(UTexture2D* Texture:{RoadBase,RoadNormal,RoadRoughness,GrassLoam,BankLoam,ShoreWetness}) if(!SaveRoadAsset(Texture)) return 1;
    auto* Source=LoadObject<UMaterial>(nullptr,*(MaterialsRoot+TEXT("M_Road_Earth.M_Road_Earth")));
    if (!Source) return 1;
    auto* RVT=LoadObject<URuntimeVirtualTexture>(nullptr,*(MaterialsRoot+TEXT("RVT_HansaRoad.RVT_HansaRoad")));
    if (!RVT)
    {
        RVT=NewObject<URuntimeVirtualTexture>(CreatePackage(*(MaterialsRoot+TEXT("RVT_HansaRoad"))),TEXT("RVT_HansaRoad"),RF_Public|RF_Standalone);
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
    if (!SaveRoadAsset(RVT)) return 3;
    auto* Road=LoadObject<UMaterial>(nullptr,*(MaterialsRoot+TEXT("M_Road_Terrain.M_Road_Terrain")));
    if (!Road)
    {
        Road=DuplicateObject<UMaterial>(Source,CreatePackage(*(MaterialsRoot+TEXT("M_Road_Terrain"))),TEXT("M_Road_Terrain"));
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
        if (!SaveRoadAsset(Road)) return 4;
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
    UMaterialExpressionTextureSample* RoadBaseSample=nullptr;
    for(UMaterialExpression* E:Road->GetExpressions())
        if(auto* Sample=Cast<UMaterialExpressionTextureSample>(E);Sample && Sample->Texture==RoadBase) {RoadBaseSample=Sample;break;}
    if(!RoadBaseSample)
        for(UMaterialExpression* E:Road->GetExpressions())
            if(auto* Sample=Cast<UMaterialExpressionTextureSample>(E);Sample && Sample->SamplerType==SAMPLERTYPE_Color)
            {RoadBaseSample=Sample;RoadBaseSample->Texture=RoadBase;break;}
    if(!RoadBaseSample) {RoadBaseSample=Node<UMaterialExpressionTextureSample>(Road);RoadBaseSample->Texture=RoadBase;}
    RoadBaseSample->SamplerType=SAMPLERTYPE_Color;
    auto* AlbedoScale=FindByDesc<UMaterialExpressionScalarParameter>(Road,TEXT("Hansa road albedo scale v2"));
    if(!AlbedoScale)
    {
        for(UMaterialExpression* E:Road->GetExpressions())
            if(auto* Parameter=Cast<UMaterialExpressionScalarParameter>(E);Parameter && Parameter->ParameterName==TEXT("RoadAlbedoScale")) {AlbedoScale=Parameter;break;}
    }
    if(!AlbedoScale) AlbedoScale=Node<UMaterialExpressionScalarParameter>(Road);
    AlbedoScale->Desc=TEXT("Hansa road albedo scale v2");AlbedoScale->ParameterName=TEXT("RoadAlbedoScale");AlbedoScale->DefaultValue=.58f;AlbedoScale->Group=TEXT("Surface");
    auto* Albedo=FindByDesc<UMaterialExpressionMultiply>(Road,TEXT("Hansa road albedo v2"));
    auto* BaseInput=Road->GetExpressionInputForProperty(MP_BaseColor);
    if(!Albedo)
    {
        Albedo=Node<UMaterialExpressionMultiply>(Road);Albedo->Desc=TEXT("Hansa road albedo v2");
        if(BaseInput->Expression) Albedo->A=*BaseInput; else Albedo->A.Connect(0,RoadBaseSample);
    }
    Albedo->B.Connect(0,AlbedoScale);BaseInput->Connect(0,Albedo);
    // Road color is reflected albedo; emissive would hide its earth texture in Unlit/Base Color inspection.
    *Road->GetExpressionInputForProperty(MP_EmissiveColor)=FExpressionInput();
    for(UMaterialExpression* E:Road->GetExpressions())
        if(auto* Out=Cast<UMaterialExpressionRuntimeVirtualTextureOutput>(E)) Out->BaseColor.Connect(0,Albedo);
    ConfigureBrokenRoadShoulders(Road);
    Road->PostEditChange(); if(!SaveRoadAsset(Road))return 4;
    // These existing Landscape masters keep their original inputs and parameter instances.
    // Only the road writes into this RVT; Landscapes only read it, avoiding feedback.
    const TCHAR* Landscapes[]={
        TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/M_Terrain_Hansa_Master.M_Terrain_Hansa_Master"),
        TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground.M_Lubeck_Ground")};
    for (const TCHAR* Path:Landscapes)
    {
        auto* Land=LoadObject<UMaterial>(nullptr,Path);
        if (!Land) return 5;
        if (Land->bUseMaterialAttributes) {UE_LOG(LogTemp,Error,TEXT("Road integration requires explicit material attribute handling for %s"),Path);return 6;}
        auto* Sample=FindByDesc<UMaterialExpressionRuntimeVirtualTextureSample>(Land,TEXT("Hansa road terrain v1"));
        if(!Sample) Sample=Node<UMaterialExpressionRuntimeVirtualTextureSample>(Land);
        Sample->Desc=TEXT("Hansa road terrain v1");Sample->VirtualTexture=RVT;Sample->MaterialType=RVT->GetMaterialType();
        // Sample outputs are identified by name rather than engine-version-specific numeric slots.
        const int32 Color=OutputIndex(Sample,TEXT("BaseColor")),Mask=OutputIndex(Sample,TEXT("Mask")),Roughness=OutputIndex(Sample,TEXT("Roughness"));
        if (Color<0 || Mask<0 || Roughness<0) {UE_LOG(LogTemp,Error,TEXT("Unexpected RVT sample outputs"));return 7;}
        for (const auto Property:{MP_BaseColor,MP_Roughness})
        {
            auto* Input=Land->GetExpressionInputForProperty(Property);
            auto* Switch=Cast<UMaterialExpressionVirtualTextureFeatureSwitch>(Input->Expression);
            auto* Blend=Switch?Cast<UMaterialExpressionLinearInterpolate>(Switch->Yes.Expression):nullptr;
            if(!Switch)
            {
                Switch=Node<UMaterialExpressionVirtualTextureFeatureSwitch>(Land);Switch->No=*Input;Input->Connect(0,Switch);
            }
            if(!Blend)
            {
                Blend=Node<UMaterialExpressionLinearInterpolate>(Land);Blend->A=Switch->No;
                if(!Blend->A.Expression){auto* Default=Node<UMaterialExpressionConstant>(Land);Default->R=Property==MP_Roughness?.8f:0;Blend->A.Connect(0,Default);}
                Switch->Yes.Connect(0,Blend);
            }
            Blend->B.Connect(Property==MP_BaseColor?Color:Roughness,Sample);Blend->Alpha.Connect(Mask,Sample);
        }
        if(FString(Path).Contains(TEXT("LubeckWorldArt_P30")))
        {
            UMaterialExpressionTextureSample* Grass=nullptr;UMaterialExpressionTextureSample* Bank=nullptr;
            for(UMaterialExpression* E:Land->GetExpressions())
                if(auto* Texture=Cast<UMaterialExpressionTextureSample>(E);Texture && Texture->Texture)
                {
                    const FString Name=Texture->Texture->GetName();
                    if(Name==TEXT("T_Terrain_Lubeck_GrassLoam")||Name==TEXT("T_Road_BaseColor")){Texture->Texture=GrassLoam;Texture->SamplerType=SAMPLERTYPE_Color;Grass=Texture;}
                    else if(Name==TEXT("T_Terrain_Lubeck_BankLoam")||Name==TEXT("T_Road_Normal")){Texture->Texture=BankLoam;Texture->SamplerType=SAMPLERTYPE_Color;Bank=Texture;}
                    else if(Name==TEXT("T_Road_Roughness")){Texture->Texture=BankLoam;Texture->SamplerType=SAMPLERTYPE_Color;}
                }
            if(!Grass||!Bank){UE_LOG(LogTemp,Error,TEXT("P30 terrain requires distinct grass-loam and bank-loam samples"));return 9;}
            auto* HeightMask=FindByDesc<UMaterialExpressionCustom>(Land,TEXT("Hansa terrain albedo v2"));
            if(!HeightMask)
                for(UMaterialExpression* E:Land->GetExpressions()) if((HeightMask=Cast<UMaterialExpressionCustom>(E))) break;
            if(!HeightMask){UE_LOG(LogTemp,Error,TEXT("P30 terrain height mask is missing"));return 9;}
            HeightMask->Desc=TEXT("Hansa terrain albedo v2");HeightMask->OutputType=CMOT_Float1;
            HeightMask->Code=TEXT("return 1.0-saturate((P.z+40.0)/110.0);");
            auto* SourceBlend=FindByDesc<UMaterialExpressionLinearInterpolate>(Land,TEXT("Hansa terrain source blend v2"));
            if(!SourceBlend){SourceBlend=Node<UMaterialExpressionLinearInterpolate>(Land);SourceBlend->Desc=TEXT("Hansa terrain source blend v2");}
            SourceBlend->A.Connect(0,Grass);SourceBlend->B.Connect(0,Bank);SourceBlend->Alpha.Connect(0,HeightMask);
            auto* BaseSwitch=Cast<UMaterialExpressionVirtualTextureFeatureSwitch>(Land->GetExpressionInputForProperty(MP_BaseColor)->Expression);
            auto* BaseBlend=BaseSwitch?Cast<UMaterialExpressionLinearInterpolate>(BaseSwitch->Yes.Expression):nullptr;
            if(!BaseSwitch||!BaseBlend) return 9;
            BaseSwitch->No.Connect(0,SourceBlend);BaseBlend->A.Connect(0,SourceBlend);
            auto* TerrainRoughness=FindByDesc<UMaterialExpressionConstant>(Land,TEXT("Hansa terrain roughness v2"));
            if(!TerrainRoughness){TerrainRoughness=Node<UMaterialExpressionConstant>(Land);TerrainRoughness->Desc=TEXT("Hansa terrain roughness v2");}
            TerrainRoughness->R=.84f;
            auto* RoughSwitch=Cast<UMaterialExpressionVirtualTextureFeatureSwitch>(Land->GetExpressionInputForProperty(MP_Roughness)->Expression);
            auto* RoughBlend=RoughSwitch?Cast<UMaterialExpressionLinearInterpolate>(RoughSwitch->Yes.Expression):nullptr;
            if(!RoughSwitch||!RoughBlend) return 9;
            RoughSwitch->No.Connect(0,TerrainRoughness);RoughBlend->A.Connect(0,TerrainRoughness);
            *Land->GetExpressionInputForProperty(MP_Normal)=FExpressionInput();
        }
        if(!ConfigureGroundVariation(Land))return 10;
        Land->PostEditChange();if(!SaveRoadAsset(Land))return 8;
    }
    return 0;
}
