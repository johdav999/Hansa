#include "Editor.h"
#include "EditorViewportClient.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
class FHansaHarborCaptureWait final : public IAutomationLatentCommand
{
public:
    FHansaHarborCaptureWait(FAutomationTestBase* InTest, FString InPath)
        : Test(InTest), Path(MoveTemp(InPath)), Started(FPlatformTime::Seconds()) {}
    virtual bool Update() override
    {
        TArray<uint8> Bytes;
        if (FFileHelper::LoadFileToArray(Bytes, *Path) && Bytes.Num() > 24)
        {
            const auto Dimension = [&Bytes](int32 Offset)
            { return (uint32(Bytes[Offset]) << 24) | (uint32(Bytes[Offset+1]) << 16) | (uint32(Bytes[Offset+2]) << 8) | Bytes[Offset+3]; };
            Test->TestEqual(TEXT("Native capture width"), Dimension(16), uint32(1280));
            Test->TestEqual(TEXT("Native capture height"), Dimension(20), uint32(720));
            Test->AddInfo(TEXT("P17 native viewport capture: ") + Path);
            return true;
        }
        if (FPlatformTime::Seconds() - Started > 30.0)
        {
            Test->AddError(TEXT("P17 native screenshot did not complete: ") + Path);
            return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test;
    FString Path;
    double Started;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHarborNativeCaptureTest, "Hansa.World.Harbor.NativeReviewCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaHarborNativeCaptureTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || World->GetOutermost()->GetName() != TEXT("/Game/Hansa/Generated/Staging/Harbor_P17/L_Harbor_Review"))
    {
        AddError(TEXT("Open the isolated P17 review level before requesting this rendered test."));
        return false;
    }
    FViewport* Viewport = GEditor->GetActiveViewport();
    if (!TestNotNull(TEXT("Real editor viewport (NullRHI is not supported)"), Viewport)) return false;
    auto* Client = static_cast<FEditorViewportClient*>(Viewport->GetClient());
    float Distance = 2500.f;
    FParse::Value(FCommandLine::Get(), TEXT("P17Distance="), Distance);
    Distance = FMath::Clamp(Distance, 1000.f, 12000.f);
    const FRotator Rotation(-40,145,0);
    Client->SetViewLocation(FVector(0,0,225) - Rotation.Vector() * Distance);
    Client->SetViewRotation(Rotation);
    FString Lighting;
    FParse::Value(FCommandLine::Get(), TEXT("P17Lighting="), Lighting);
    // Explicit isolated preview assumptions, not a claim of surveyed Rostock lighting.
    if (Lighting == TEXT("RostockPreview") || Lighting == TEXT("Neutral"))
    {
        int32 Lights = 0;
        for (TActorIterator<ADirectionalLight> It(World); It; ++It)
        {
            It->GetLightComponent()->SetUseTemperature(true);
            It->GetLightComponent()->SetTemperature(Lighting == TEXT("Neutral") ? 6500.f : 4800.f);
            ++Lights;
        }
        TestTrue(TEXT("Preview has a real directional light"), Lights > 0);
    }
    AddInfo(FString::Printf(TEXT("Isolated lighting profile %s; distance %.0f cm"), *Lighting, Distance));
    Client->SetGameView(true);
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
        FString::Printf(TEXT("GenerationJobs/hansa-harbor_P17_20260908/renders/native-%llu.png"), FPlatformTime::Cycles64()));
    auto& Config = GetHighResScreenshotConfig();
    if (!TestTrue(TEXT("Native render dimensions accepted"), Config.SetResolution(1280, 720, 1.0f))) return false;
    Config.FilenameOverride = Path;
    if (!TestTrue(TEXT("Real viewport screenshot queued"), Viewport->TakeHighResScreenShot())) return false;
    Viewport->Draw();
    ADD_LATENT_AUTOMATION_COMMAND(FHansaHarborCaptureWait(this, Path));
    return true;
}
#endif
