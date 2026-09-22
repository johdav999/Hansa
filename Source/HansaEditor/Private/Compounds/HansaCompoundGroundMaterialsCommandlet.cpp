#include "Compounds/HansaCompoundGroundMaterialsCommandlet.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Engine/Texture2D.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

UHansaCompoundGroundMaterialsCommandlet::UHansaCompoundGroundMaterialsCommandlet()
{IsClient=false;IsEditor=true;LogToConsole=true;}
int32 UHansaCompoundGroundMaterialsCommandlet::Main(const FString& Params)
{
 const FString Path=TEXT("/Game/Hansa/World/Ground/M_CompoundDirt");
 if(!FParse::Param(*Params,TEXT("Apply"))){UE_LOG(LogTemp,Display,TEXT("Dry run: creates %s from existing road artwork; -Apply saves."),*Path);return 0;}
 if(FPackageName::DoesPackageExist(Path)){UE_LOG(LogTemp,Display,TEXT("Ground material already exists; no overwrite."));return 0;}
 auto* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Textures/T_Road_BaseColor.T_Road_BaseColor"));
 if(!Texture)return 1;
 auto* Package=CreatePackage(*Path);
 auto* M=NewObject<UMaterial>(Package,TEXT("M_CompoundDirt"),RF_Public|RF_Standalone);
 M->BlendMode=BLEND_Translucent;M->TwoSided=true;M->TranslucencyLightingMode=TLM_Surface;
 auto* Sample=NewObject<UMaterialExpressionTextureSample>(M);Sample->Texture=Texture;Sample->SamplerType=SAMPLERTYPE_Color;
 auto* Tint=NewObject<UMaterialExpressionMultiply>(M);Tint->A.Connect(0,Sample);Tint->ConstB=.48f;
 auto* Vertex=NewObject<UMaterialExpressionVertexColor>(M);
 auto* Position=NewObject<UMaterialExpressionWorldPosition>(M);
 auto* Opacity=NewObject<UMaterialExpressionCustom>(M);
 Opacity->OutputType=CMOT_Float1;
 Opacity->Description=TEXT("World-space broken dirt edge; vertex alpha defines yard and paths");
 FCustomInput Alpha;Alpha.InputName=TEXT("Coverage");Alpha.Input.Connect(4,Vertex);Opacity->Inputs.Add(Alpha);
 FCustomInput P;P.InputName=TEXT("P");P.Input.Connect(0,Position);Opacity->Inputs.Add(P);
 Opacity->Code=TEXT("float2 q=P.xy/27.0; float2 i=floor(q), f=frac(q); f=f*f*(3-2*f); float a=frac(sin(dot(i,float2(127.1,311.7)))*43758.5453); float b=frac(sin(dot(i+float2(1,0),float2(127.1,311.7)))*43758.5453); float c=frac(sin(dot(i+float2(0,1),float2(127.1,311.7)))*43758.5453); float d=frac(sin(dot(i+1,float2(127.1,311.7)))*43758.5453); float n=lerp(lerp(a,b,f.x),lerp(c,d,f.x),f.y); return saturate(Coverage-(1-Coverage)*n*0.8);");
 for(UMaterialExpression* E:TArray<UMaterialExpression*>{Sample,Tint,Vertex,Position,Opacity})M->GetExpressionCollection().AddExpression(E);
 M->GetEditorOnlyData()->BaseColor.Connect(0,Tint);
 M->GetEditorOnlyData()->Opacity.Connect(0,Opacity);
 M->GetEditorOnlyData()->Roughness.Constant=.96f;
 M->GetEditorOnlyData()->Specular.Constant=.12f;
 M->PostEditChange();M->MarkPackageDirty();FAssetRegistryModule::AssetCreated(M);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 return UPackage::SavePackage(Package,M,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args)?0:1;
}
