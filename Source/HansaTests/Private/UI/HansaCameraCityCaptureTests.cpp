#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaTradeStationPresentation.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SViewport.h"

namespace {
class FCameraCityCapture final : public IAutomationLatentCommand {
public:
    explicit FCameraCityCapture(FAutomationTestBase* InTest) : Test(InTest), Start(FPlatformTime::Seconds()) {}
    bool Update() override {
        const double Now = FPlatformTime::Seconds();
        if (Now - Start > 150) { Test->AddError(TEXT("Camera city viewport timeout")); return true; }
        auto* Viewport = GEngine ? GEngine->GameViewport.Get() : nullptr;
        auto* World = Viewport ? Viewport->GetWorld() : nullptr;
        auto* Controller = World ? World->GetFirstPlayerController() : nullptr;
        auto* Hud = Controller ? Cast<AHansaRootHud>(Controller->GetHUD()) : nullptr;
        if (!Hud || !Hud->GetRootWidget() || HansaWaitForFrontend(Hud)) return false;
        auto* Camera = Cast<AHansaStrategyCameraPawn>(Controller->GetPawn());
        if (!Camera) return false;
        if (!Prepared) {
            if (!Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(World)) { Test->AddError(TEXT("Run on L_HansaWorld_WP")); return true; }
            Hud->GetScenarioPresentationModel()->Close();
            Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
            Camera->bEnableMouseEdgePan = false;
            Camera->ClearCameraIntents();
            Positions = {Hansa::Game::LubeckPlacementGrid::CampaignLubeckCenter(),
                AHansaTradeStationPresentation::SiteTransform(World).TransformPosition(FVector(60000, 0, 100)),
                FVector(3918435.667681167, 4446494.377616651, 100),
                FVector(500000, 500000, 100), Hansa::Game::LubeckPlacementGrid::CampaignLubeckCenter()};
            Labels = {TEXT("Lübeck"), TEXT("Rostock"), TEXT("Wismar"), TEXT("Northern Europe"), TEXT("Lübeck")};
            Prepared = true;
            Camera->FocusWorldLocationIntent(Positions[0]);
            Ready = Now;
            return false;
        }
        if (Now - Ready < 2.) return false;
        const FString Expected = Labels[Step];
        Test->TestEqual(TEXT("Camera city model updates while simulation is paused"), Hud->GetPresentationModel()->GetSnapshot().CityBreadcrumb.ToString(), Expected);
        const auto Text = StaticCastSharedPtr<STextBlock>(Hud->GetRootWidget()->ResolveSemanticWidget(TEXT("HUD.TopStatus.CityBreadcrumb")));
        if (Test->TestTrue(TEXT("Native top panel city text exists"), Text.IsValid()))
            Test->TestEqual(TEXT("Actual top panel renders camera city"), Text->GetText().ToString(), Expected);
        // An ordinary simulation refresh must not restore the legacy home/visit label.
        auto* Mode = World->GetAuthGameMode<AHansaGameMode>();
        auto* Host = Mode ? Mode->GetSimulationHost() : nullptr;
        if (Host) {
            const FString SelectedReport = Hud->GetCityOverviewPresentationModel()->GetSnapshot().CityStableId.ToString();
            Hud->GetCityOverviewPresentationModel()->SelectCityIntent(TEXT("City.Lubeck"));
            Hud->GetCityOverviewPresentationModel()->OnRefreshRequested().Broadcast();
            Test->TestEqual(TEXT("Report refresh preserves camera identity"), Hud->GetPresentationModel()->GetSnapshot().CityBreadcrumb.ToString(), Expected);
            Hud->GetCityOverviewPresentationModel()->SelectCityIntent(FName(*SelectedReport));
        }
        TArray<FColor> Pixels; FIntVector Size;
        if (FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(), Pixels, Size)) {
            const FString Directory = FPaths::ProjectDir() / TEXT("Docs/Images/UI/CameraCity");
            IFileManager::Get().MakeDirectory(*Directory, true);
            TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, Png);
            Test->TestTrue(TEXT("Saved actual viewport"), FFileHelper::SaveArrayToFile(Png, *(Directory / FString::Printf(TEXT("camera-city--%d--%dx%d.png"), Step, Size.X, Size.Y))));
        } else Test->AddError(TEXT("Viewport screenshot failed"));
        if (++Step == Positions.Num()) return true;
        Camera->FocusWorldLocationIntent(Positions[Step]); Ready = Now; return false;
    }
private:
    FAutomationTestBase* Test; double Start, Ready = 0; bool Prepared = false; int32 Step = 0;
    TArray<FVector> Positions; TArray<FString> Labels;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCameraCityViewport, "Hansa.UI.HUD.CameraCity.RealViewport", EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FHansaCameraCityViewport::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCameraCityCapture(this)); return true; }
#endif
