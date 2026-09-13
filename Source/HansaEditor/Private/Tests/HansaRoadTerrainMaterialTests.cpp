#include "Misc/AutomationTest.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "VT/RuntimeVirtualTexture.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadTerrainMaterialTest,"Hansa.World.RoadTerrain.MaterialContract",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaRoadTerrainMaterialTest::RunTest(const FString&)
{
    auto* Road=LoadObject<UMaterial>(nullptr,TEXT("/Game/Mesh/hansa-dirt-road/Materials/M_Road_Terrain.M_Road_Terrain"));
    if(!TestNotNull(TEXT("Runtime road material is cooked production content"),Road))return false;
    auto* Fit=Cast<UMaterialExpressionCustom>(Road->GetExpressionInputForProperty(MP_WorldPositionOffset)->Expression);
    if(!TestNotNull(TEXT("Residual height correction is wired to physical geometry"),Fit))return false;
    if(!TestEqual(TEXT("Height-fit shader contract"),Fit->Inputs.Num(),12))return false;
    for(int32 I:{2,3})
    {
        const auto& Input=Fit->Inputs[I].Input;
        TestTrue(TEXT("Coordinate transform retains translation in its fourth component"),Input.Expression &&
            Input.Expression->GetOutputs().IsValidIndex(Input.OutputIndex) && Input.Expression->GetOutputs()[Input.OutputIndex].OutputName==TEXT("RGBA"));
    }
    int32 Writers=0;
    for(UMaterialExpression* E:Road->GetExpressions())
    {
        Writers+=E->IsA<UMaterialExpressionRuntimeVirtualTextureOutput>();
        TestFalse(TEXT("Road writer never samples its own RVT"),E->IsA<UMaterialExpressionRuntimeVirtualTextureSample>());
    }
    TestEqual(TEXT("One road RVT writer"),Writers,1);
    for(const TCHAR* Path:{
        TEXT("/Game/Hansa/Generated/Staging/LubeckTerrain_20260907/Materials/M_Terrain_Hansa_Master.M_Terrain_Hansa_Master"),
        TEXT("/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/M_Lubeck_Ground.M_Lubeck_Ground")})
    {
        auto* Land=LoadObject<UMaterial>(nullptr,Path);
        if(!TestNotNull(TEXT("Existing Landscape master"),Land))continue;
        int32 Readers=0;
        for(UMaterialExpression* E:Land->GetExpressions())
        {
            if(auto* Sample=Cast<UMaterialExpressionRuntimeVirtualTextureSample>(E))
                if(Sample->VirtualTexture && Sample->VirtualTexture->GetName()==TEXT("RVT_HansaRoad"))++Readers;
            TestFalse(TEXT("Landscape cannot feed back into road RVT"),E->IsA<UMaterialExpressionRuntimeVirtualTextureOutput>());
        }
        TestEqual(TEXT("One road receiver per existing Landscape master"),Readers,1);
    }
    return !HasAnyErrors();
}
#endif
