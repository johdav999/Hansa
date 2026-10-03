#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "ShaderCompiler.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaTerrainPlacement.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaGameMode.h"
#include "UI/HansaRootHud.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "../UI/HansaFrontendCaptureSupport.h"

namespace
{
class FHarborShoreCapture : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds(), ReadyAt = 0;
    TWeakObjectPtr<AHansaBuildingWorldProjectionActor> Dock;
    TWeakObjectPtr<AHansaLubeckWorldFoundation> Foundation;
    Hansa::Simulation::FHansaBuildingWorldProjection Projection;
public:
    explicit FHarborShoreCapture(FAutomationTestBase* In) : Test(In) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 180) { Test->AddError(TEXT("Dock shoreline capture timed out")); return true; }
        auto* V = GEngine ? GEngine->GameViewport.Get() : nullptr;
        auto* W = V ? V->GetWorld() : nullptr;
        auto* C = W ? W->GetFirstPlayerController() : nullptr;
        auto* Hud = C ? Cast<AHansaRootHud>(C->GetHUD()) : nullptr;
        if (!Hud || !Hud->GetRootWidget() || HansaWaitForFrontend(Hud)) return false;
        auto* Mode = Cast<AHansaGameMode>(W->GetAuthGameMode());
        auto* Host = Mode ? Mode->GetSimulationHost() : nullptr;
        auto* Camera = Cast<AHansaStrategyCameraPawn>(C->GetPawn());
        if (!Host || !Camera) return false;
        if (!Dock.IsValid())
        {
            for (TActorIterator<AHansaLubeckWorldFoundation> It(W); It; ++It) { Foundation = *It; break; }
            if (!Foundation.IsValid()) return false;
            Hud->GetScenarioPresentationModel()->AcknowledgeBriefing();
            Hud->GetScenarioPresentationModel()->DismissHelp();
            Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
            Hud->GetBuildMenuPresentationModel()->CancelIntent();
            Hud->GetBuildMenuPresentationModel()->SetOpen(false);
            // Reproduce the reported dock in the actual production world, at the original XY/yaw.
            const FVector Reported(3806663.6904761903, 4407202.3809523806, 0);
            int32 X = 0, Y = 0;
            if (!Foundation->WorldToPlacementCell(Reported, X, Y)) { Test->AddError(TEXT("Production map does not contain reported dock")); return true; }
            using namespace Hansa::Simulation;
            Projection.BuildingId = FHansaBuildingId::TryCreate(99001).Value;
            Projection.OwnerId = Host->GetHouseId();
            Projection.Placement.CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
            Projection.Placement.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Dock")).Value;
            Projection.Placement.Anchor = {X-1,Y-2}; Projection.Placement.Rotation = EHansaGridRotation::East;
            Projection.FootprintWidthCells = 5; Projection.FootprintHeightCells = 3;
            Projection.Status = EHansaBuildingWorldStatus::Ready;
            Projection.ConstructionProgress = FHansaRate::TryMakeNormalized(FHansaRate::Scale).Value;
            for (int32 DX=0; DX<3; ++DX) for (int32 DY=0; DY<5; ++DY) Projection.OccupiedCells.Add({X-1+DX,Y-2+DY});
            Dock = W->SpawnActor<AHansaBuildingWorldProjectionActor>();
            Camera->bEnableMouseEdgePan = false; Camera->ClearCameraIntents();
            Camera->MinimumZoomDistance = 1000; Camera->FocusWorldLocationIntent(Reported);
            Camera->AddZoomIntent((Camera->GetZoomDistance()-3500)/Camera->ZoomUnitsPerStep);
            ReadyAt = FPlatformTime::Seconds() + 8;
            return false;
        }
        if (FPlatformTime::Seconds() < ReadyAt || (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())) return false;
        Dock->ApplyProjection(Projection, *Foundation);
        const FVector Location = Dock->GetActorLocation();
        FHitResult Ground;
        Test->TestTrue(TEXT("Reported dock terrain is loaded"), Hansa::Game::TerrainPlacement::Trace(W,
            Location+FVector(0,0,1000000), Location-FVector(0,0,1000000), Ground));
        Test->TestTrue(TEXT("Dock deck clears actual Baltic water"), Location.Z >= 224.9);
        Test->TestTrue(TEXT("Deck is raised above submerged centre terrain"), Location.Z > Ground.ImpactPoint.Z + 100);
        // Allow a rendered frame after positioning before reading pixels.
        static_cast<void>(Location);
        if (ReadyAt > 0) { ReadyAt = -1; return false; }
        TArray<FColor> Pixels; FIntVector Size;
        if (!V->GetGameViewportWidget() || !FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(), Pixels, Size))
        { Test->AddError(TEXT("Dock viewport capture failed")); return true; }
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X, Size.Y, Pixels, PNG);
        const FString Base = FPaths::ProjectDir()/TEXT("Docs/Images/World/DockPlacement")/FString::Printf(TEXT("dock--shore-fixed--%dx%d"),Size.X,Size.Y);
        Test->TestTrue(TEXT("Native dock capture saved"), FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png"))));
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("Deck: %s\nCentre terrain Z: %.3f\nNative viewport: %dx%d\nTest-only projection on production terrain; not a gameplay command acceptance proof.\n"),*Location.ToString(),Ground.ImpactPoint.Z,Size.X,Size.Y),*(Base+TEXT(".txt")));
        Dock->Destroy(); return true;
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHarborShoreViewportTest,"Hansa.World.Harbor.ShoreViewport",
    EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaHarborShoreViewportTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FHarborShoreCapture(this)); return true; }
#endif
