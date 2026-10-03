#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Camera/CameraComponent.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "RenderTimer.h"
#include "World/HansaLandOverlayRenderer.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "UI/HansaRootHud.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "../UI/HansaFrontendCaptureSupport.h"

namespace
{
class FLandViewport final : public IAutomationLatentCommand
{
public:
    explicit FLandViewport(FAutomationTestBase* In) : Test(In), Start(FPlatformTime::Seconds()) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Start>180) { Test->AddError(TEXT("Land overlay capture timed out")); return true; }
        if (!GEngine || !GEngine->GameViewport) return false;
        auto* Viewport=GEngine->GameViewport.Get(); auto* World=Viewport->GetWorld();
        auto* PC=World?World->GetFirstPlayerController():nullptr;
        auto* Hud=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;
        auto* Camera=PC?Cast<AHansaStrategyCameraPawn>(PC->GetPawn()):nullptr;
        auto* Mode=World?World->GetAuthGameMode<AHansaGameMode>():nullptr;
        auto* Host=Mode?Mode->GetSimulationHost():nullptr;
        if (!Hud || !Camera || !Host || !Hud->GetRootWidget() || HansaWaitForFrontend(Hud)) return false;
        if (!Renderer)
        {
            for(TActorIterator<AHansaLubeckWorldFoundation> It(World);It;++It){Foundation=*It;break;}
            if (!Foundation) return false;
            Hud->GetScenarioPresentationModel()->AcknowledgeBriefing();
            Hud->GetScenarioPresentationModel()->DismissHelp(); Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
            Camera->bEnableMouseEdgePan=false; Camera->ClearCameraIntents();
            Foundation->WorldToPlacementCell(Camera->GetActorLocation(),Center.X,Center.Y);
            Renderer=World->SpawnActor<AHansaLandOverlayRenderer>();
            Test->TestTrue(TEXT("Real viewport resolves native ground shader"),Renderer->HasGroundMaterial());
            AHansaLandOverlayRenderer::MakeGridTransform(Host->GetCityId(),*Foundation,GridTransform);
            Fingerprint=Host->BuildProjection().Value.GetFingerprint().Value;
        }
        if (!Prepared)
        {
            const float Zoom=Stage==3?13000.f:7000.f;
            Camera->AddZoomIntent((Camera->GetZoomDistance()-Zoom)/Camera->ZoomUnitsPerStep);
            FHansaLandOverlayOptions Options;
            Options.Mode=Stage==2 ? Hansa::Game::LandOverlay::EMode::Ownership : Hansa::Game::LandOverlay::EMode::Buildable;
            const auto NativeView=Viewport->Viewport->GetSizeXY();
            Options.LineWidthCm=Hansa::Game::LandOverlay::RibbonWidthForView(Zoom,
                Camera->Camera->FieldOfView,NativeView.X,Camera->GetCameraPitchDegrees());
            if(Stage==2)
            {
                const auto Nearby=Host->QueryLand(Host->GetHouseId(),Host->GetCityId(),
                    {Center.X-16,Center.Y-16},{Center.X+16,Center.Y+16});
                int32 Best=MAX_int32;
                for(const auto& Cell:Nearby.Cells)
                {
                    const auto Style=Hansa::Game::LandOverlay::Classify(Cell,Options.Mode);
                    if(Style.Surface!=Hansa::Game::LandOverlay::ESurface::Ownership) continue;
                    const FIntPoint P(Cell.Coordinate.X,Cell.Coordinate.Y), Delta=P-Center;
                    const int32 Distance=Delta.X*Delta.X+Delta.Y*Delta.Y;
                    if(Distance<Best){Best=Distance;Options.SelectedCell=P;}
                }
                Test->TestTrue(TEXT("Capture selects actual surveyed land"),Options.SelectedCell.IsSet());
            }
            for(int32 X=0;X<2;++X)for(int32 Y=0;Y<2;++Y)
            {
                const FIntPoint Min=Center+FIntPoint((X-1)*24,(Y-1)*24),Max=Min+FIntPoint(23,23);
                const auto Q=Host->QueryLand(Host->GetHouseId(),Host->GetCityId(),{Min.X-1,Min.Y-1},{Max.X+1,Max.Y+1});
                Test->TestTrue(TEXT("Real scoped terrain chunk"),Renderer->ApplyChunk({X,Y},Q,Min,Max,GridTransform,Options));
            }
            Renderer->SetActorHiddenInGame(Stage==0);
            Renderer->InvalidateTerrain();
            Ready=FPlatformTime::Seconds()+5; Prepared=true; Samples=0; GameMs=RenderMs=0;
            return false;
        }
        if (FPlatformTime::Seconds()<Ready)
        {
            if(FPlatformTime::Seconds()>Ready-1){++Samples;GameMs+=FPlatformTime::ToMilliseconds(GGameThreadTime);RenderMs+=FPlatformTime::ToMilliseconds(GRenderThreadTime);}
            return false;
        }
        const auto Stats=Renderer->GetStats();
        Test->TestTrue(TEXT("Overlay generated visible ground geometry"),Stats.Triangles>0);
        Test->TestTrue(TEXT("At most one component per chunk"),Stats.PooledComponents<=4);
        Test->TestEqual(TEXT("Rendering preserves authoritative state"),Host->BuildProjection().Value.GetFingerprint().Value,Fingerprint);
        TArray<FColor> Pixels; FIntVector Size;
        if(!Viewport->GetGameViewportWidget() || !FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size))
        {Test->AddError(TEXT("Native viewport capture failed"));return true;}
        const FIntPoint Native=Viewport->Viewport->GetSizeXY();
        Test->TestTrue(TEXT("Capture stays native size"),Native==FIntPoint(Size.X,Size.Y));
        const TCHAR* Names[]={TEXT("off"),TEXT("buildable-close"),TEXT("ownership-close"),TEXT("buildable-distant")};
        const TCHAR* Folder=FParse::Param(FCommandLine::Get(),TEXT("LandOverlayRibbonCapture"))?TEXT("RibbonImplementation"):TEXT("Implementation");
        const FString Base=FPaths::ProjectDir()/FString::Printf(TEXT("Docs/Images/UI/LandOverlay/%s/%s--%dx%d"),Folder,Names[Stage],Size.X,Size.Y);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Base),true);
        TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Save actual game capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
        const FString Evidence=FString::Printf(TEXT("map=%s\ncenter=%d,%d\ntriangles=%d\nchunks=%d\ncomponents=%d\nmissingSamples=%d\nrebuilds=%llu\ngameThreadMs=%.3f\nrenderThreadMs=%.3f\nsamples=%d\nfingerprint=%llu\n"),
            *World->GetPackage()->GetName(),Center.X,Center.Y,Stats.Triangles,Stats.ActiveChunks,Stats.PooledComponents,Stats.MissingTerrainSamples,Stats.Rebuilds,
            GameMs/FMath::Max(1,Samples),RenderMs/FMath::Max(1,Samples),Samples,Fingerprint);
        FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));
        ++Stage;Prepared=false;
        if(Stage==4){Renderer->Destroy();return true;}return false;
    }
private:
    FAutomationTestBase* Test;
    double Start,Ready=0,GameMs=0,RenderMs=0;
    int32 Stage=0,Samples=0;
    bool Prepared=false;
    uint64 Fingerprint=0;
    FIntPoint Center;
    FTransform GridTransform;
    AHansaLubeckWorldFoundation* Foundation=nullptr;
    AHansaLandOverlayRenderer* Renderer=nullptr;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaLandViewportCapture,"Hansa.World.LandOverlayCapture.RealViewport",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FHansaLandViewportCapture::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FLandViewport(this)); return true; }
#endif
