#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVirtualTextureFeatureSwitch.h"
#include "VT/RuntimeVirtualTexture.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FName OutputName(const FExpressionInput& Input)
{
    return Input.Expression && Input.Expression->GetOutputs().IsValidIndex(Input.OutputIndex)
        ? Input.Expression->GetOutputs()[Input.OutputIndex].OutputName : NAME_None;
}
UTexture2D* Texture(const TCHAR* Path)
{
    return LoadObject<UTexture2D>(nullptr,Path);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadTerrainMaterialTest,"Hansa.World.RoadTerrain.MaterialContract",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaRoadTerrainMaterialTest::RunTest(const FString&)
{
    auto* Road=LoadObject<UMaterial>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/M_Road_Terrain.M_Road_Terrain"));
    auto* Source=LoadObject<UMaterial>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/M_Road_Earth.M_Road_Earth"));
    if(!TestNotNull(TEXT("Runtime road material is cooked production content"),Road)||!TestNotNull(TEXT("Brown road source material"),Source))return false;
    TestNull(TEXT("Road runtime emissive is zero"),Road->GetExpressionInputForProperty(MP_EmissiveColor)->Expression);
    TestNull(TEXT("Road source emissive is zero"),Source->GetExpressionInputForProperty(MP_EmissiveColor)->Expression);

    auto* Fit=Cast<UMaterialExpressionCustom>(Road->GetExpressionInputForProperty(MP_WorldPositionOffset)->Expression);
    if(!TestNotNull(TEXT("Residual height correction is wired to physical geometry"),Fit))return false;
    if(!TestEqual(TEXT("Height-fit shader contract"),Fit->Inputs.Num(),12))return false;
    for(int32 I:{2,3})
    {
        const auto& Input=Fit->Inputs[I].Input;
        TestTrue(TEXT("Coordinate transform retains translation in its fourth component"),Input.Expression &&
            Input.Expression->GetOutputs().IsValidIndex(Input.OutputIndex) && Input.Expression->GetOutputs()[Input.OutputIndex].OutputName==TEXT("RGBA"));
    }

    UMaterialExpressionMultiply* Albedo=nullptr;
    UMaterialExpressionScalarParameter* AlbedoScale=nullptr;
    UMaterialExpressionTextureSample* RoadBaseSample=nullptr;
    UMaterialExpressionRuntimeVirtualTextureOutput* Writer=nullptr;
    int32 Writers=0;
    for(UMaterialExpression* E:Road->GetExpressions())
    {
        if(auto* Multiply=Cast<UMaterialExpressionMultiply>(E);Multiply&&Multiply->Desc==TEXT("Hansa road albedo v2"))Albedo=Multiply;
        if(auto* Scalar=Cast<UMaterialExpressionScalarParameter>(E);Scalar&&Scalar->ParameterName==TEXT("RoadAlbedoScale"))AlbedoScale=Scalar;
        if(auto* Sample=Cast<UMaterialExpressionTextureSample>(E);Sample&&Sample->Texture&&Sample->Texture->GetName()==TEXT("T_Road_BaseColor"))RoadBaseSample=Sample;
        if(auto* Out=Cast<UMaterialExpressionRuntimeVirtualTextureOutput>(E)){Writer=Out;++Writers;}
        TestFalse(TEXT("Road writer never samples its own RVT"),E->IsA<UMaterialExpressionRuntimeVirtualTextureSample>());
    }
    TestNotNull(TEXT("Road samples its brown source texture"),RoadBaseSample);
    if(RoadBaseSample)TestEqual(TEXT("Road base-color sampler is color"),RoadBaseSample->SamplerType,SAMPLERTYPE_Color);
    TestNotNull(TEXT("Road has explicit albedo control"),Albedo);
    TestNotNull(TEXT("Road albedo scale parameter"),AlbedoScale);
    if(AlbedoScale)TestEqual(TEXT("Road albedo scale prevents near-white Base Color"),AlbedoScale->DefaultValue,.58f);
    TestTrue(TEXT("Road Base Color ends at albedo control"),Albedo&&Road->GetExpressionInputForProperty(MP_BaseColor)->Expression==Albedo);
    TestEqual(TEXT("One road RVT writer"),Writers,1);
    TestTrue(TEXT("RVT writes corrected road albedo"),Writer&&Writer->BaseColor.Expression==Albedo);

    auto* RoadBase=Texture(TEXT("/Game/Mesh/hansa-dirt-road/Textures/T_Road_BaseColor.T_Road_BaseColor"));
    auto* RoadNormal=Texture(TEXT("/Game/Mesh/hansa-dirt-road/Textures/T_Road_Normal.T_Road_Normal"));
    auto* RoadRough=Texture(TEXT("/Game/Mesh/hansa-dirt-road/Textures/T_Road_Roughness.T_Road_Roughness"));
    auto* Grass=Texture(TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Textures/T_Terrain_Lubeck_GrassLoam.T_Terrain_Lubeck_GrassLoam"));
    auto* Bank=Texture(TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Textures/T_Terrain_Lubeck_BankLoam.T_Terrain_Lubeck_BankLoam"));
    auto* Shore=Texture(TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Textures/T_Terrain_Lubeck_ShoreWetness.T_Terrain_Lubeck_ShoreWetness"));
    if(!RoadBase||!RoadNormal||!RoadRough||!Grass||!Bank||!Shore){AddError(TEXT("Road/terrain source textures must all exist"));return false;}
    TestTrue(TEXT("Road base color is sRGB"),RoadBase->SRGB);TestEqual(TEXT("Road base color compression"),RoadBase->CompressionSettings,TC_Default);
    TestFalse(TEXT("Road normal is linear"),RoadNormal->SRGB);TestEqual(TEXT("Road normal compression"),RoadNormal->CompressionSettings,TC_Normalmap);
    TestFalse(TEXT("Road roughness is linear"),RoadRough->SRGB);TestEqual(TEXT("Road roughness compression"),RoadRough->CompressionSettings,TC_Masks);
    TestTrue(TEXT("Grass-loam base color is sRGB"),Grass->SRGB);TestEqual(TEXT("Grass-loam compression"),Grass->CompressionSettings,TC_Default);
    TestTrue(TEXT("Bank-loam base color is sRGB"),Bank->SRGB);TestEqual(TEXT("Bank-loam compression"),Bank->CompressionSettings,TC_Default);
    TestFalse(TEXT("Shore wetness mask is linear"),Shore->SRGB);TestEqual(TEXT("Shore wetness compression"),Shore->CompressionSettings,TC_Masks);

    auto* RVT=LoadObject<URuntimeVirtualTexture>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/RVT_HansaRoad.RVT_HansaRoad"));
    if(!TestNotNull(TEXT("Road RVT asset"),RVT))return false;
    TestEqual(TEXT("RVT uses YCoCg base-color encoding"),RVT->GetMaterialType(),ERuntimeVirtualTextureMaterialType::BaseColor_Normal_Specular_Mask_YCoCg);
    for(const TCHAR* Path:{
        TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/M_Terrain_Hansa_Master.M_Terrain_Hansa_Master"),
        TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground.M_Lubeck_Ground")})
    {
        auto* Land=LoadObject<UMaterial>(nullptr,Path);
        if(!TestNotNull(TEXT("Existing Landscape master"),Land))continue;
        UMaterialExpressionRuntimeVirtualTextureSample* Receiver=nullptr;
        int32 Readers=0;bool bGrass=false,bBank=false,bRoadTexture=false;
        for(UMaterialExpression* E:Land->GetExpressions())
        {
            if(auto* Sample=Cast<UMaterialExpressionRuntimeVirtualTextureSample>(E);Sample&&Sample->VirtualTexture==RVT){Receiver=Sample;++Readers;}
            if(auto* Sample=Cast<UMaterialExpressionTextureSample>(E);Sample&&Sample->Texture)
            {
                const FString Name=Sample->Texture->GetName();
                bGrass|=Name==TEXT("T_Terrain_Lubeck_GrassLoam");bBank|=Name==TEXT("T_Terrain_Lubeck_BankLoam");bRoadTexture|=Name.StartsWith(TEXT("T_Road_"));
            }
            TestFalse(TEXT("Landscape cannot feed back into road RVT"),E->IsA<UMaterialExpressionRuntimeVirtualTextureOutput>());
        }
        TestEqual(TEXT("One road receiver per existing Landscape master"),Readers,1);
        if(Receiver)TestEqual(TEXT("RVT writer and receiver encodings match"),Receiver->MaterialType,RVT->GetMaterialType());
        for(const auto Property:{MP_BaseColor,MP_Roughness})
        {
            auto* Switch=Cast<UMaterialExpressionVirtualTextureFeatureSwitch>(Land->GetExpressionInputForProperty(Property)->Expression);
            auto* Blend=Switch?Cast<UMaterialExpressionLinearInterpolate>(Switch->Yes.Expression):nullptr;
            TestNotNull(TEXT("RVT receiver feature switch"),Switch);TestNotNull(TEXT("RVT receiver blend"),Blend);
            if(Blend&&Receiver)
            {
                TestTrue(TEXT("RVT blend is gated by the RVT mask"),Blend->Alpha.Expression==Receiver&&OutputName(Blend->Alpha)==TEXT("Mask"));
                TestTrue(TEXT("RVT blend samples the matching data channel"),Blend->B.Expression==Receiver&&OutputName(Blend->B)==(Property==MP_BaseColor?TEXT("BaseColor"):TEXT("Roughness")));
            }
        }
        TestTrue(TEXT("Terrain has grass-loam source"),bGrass);TestTrue(TEXT("Terrain has bank-loam source"),bBank);
        if(FString(Path).Contains(TEXT("LubeckWorldArt_P30")))
        {
            TestFalse(TEXT("P30 terrain never reuses road dirt textures"),bRoadTexture);
            TestNull(TEXT("P30 terrain does not reuse the road normal"),Land->GetExpressionInputForProperty(MP_Normal)->Expression);
        }
    }
    return !HasAnyErrors();
}
#endif
