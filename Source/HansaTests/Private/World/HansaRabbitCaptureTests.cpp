#if WITH_DEV_AUTOMATION_TESTS
#include "../UI/HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "World/HansaAmbientRabbits.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"

namespace
{
class FRabbitCapture final : public IAutomationLatentCommand
{
public:
    explicit FRabbitCapture(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Started > 240) { Test->AddError(TEXT("Rabbit gameplay capture timed out")); return true; }
        if (!GEngine || !GEngine->GameViewport) return false;
        auto* View = GEngine->GameViewport.Get();
        auto* World = View->GetWorld();
        auto* PC = World ? World->GetFirstPlayerController() : nullptr;
        auto* Hud = PC ? Cast<AHansaRootHud>(PC->GetHUD()) : nullptr;
        if (!Hud || !Hud->GetRootWidget()) return false;
        if (HansaWaitForFrontend(Hud)) return false;
        if (Hud->GetScenarioPresentationModel()->GetSnapshot().Phase == EHansaScenarioPresentationPhase::Briefing)
        { Hud->GetRootWidget()->ActivateSemanticId(TEXT("Scenario.Begin")); return false; }
        auto* Mode = Cast<AHansaGameMode>(World->GetAuthGameMode());
        auto* Host = Mode ? Mode->GetSimulationHost() : nullptr;
        auto* Camera = Cast<AHansaStrategyCameraPawn>(PC->GetPawn());
        if (!Host || !Camera) return false;
        AHansaAmbientRabbits* Manager = nullptr;
        for (TActorIterator<AHansaAmbientRabbits> It(World); It; ++It) { Manager = *It; break; }
        if (!Manager || Manager->GetLiveRabbitCount() < 6) return false;
        if (!Manager->GetValidationError().IsEmpty()) { Test->AddError(Manager->GetValidationError()); return true; }
        if (Stage == 0 && !Prepared)
        {
            Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
            const auto Observations = Manager->QueryRabbits();
            FocusId = Observations[0].StableId;
            Camera->AddZoomIntent((Camera->GetZoomDistance()-1200)/Camera->ZoomUnitsPerStep);
            Camera->FocusWorldLocationIntent(Observations[0].Location);
            Ready = FPlatformTime::Seconds()+5;
            Prepared = true;
            return false;
        }
        const auto Observations = Manager->QueryRabbits();
        const auto* Rabbit = Observations.FindByPredicate([&](const auto& O) { return O.StableId == FocusId; });
        if (!Rabbit || !Rabbit->bVisible) return false;
        Camera->FocusWorldLocationIntent(Rabbit->Location);
        if (FPlatformTime::Seconds() < Ready) return false;
        if (Stage == 1 && Rabbit->Activity != EHansaRabbitActivity::Walk) return false;
        if (Stage == 2 && (Rabbit->Activity != EHansaRabbitActivity::Jump || Rabbit->AnimationTime < .4f || Rabbit->AnimationTime > .6f)) return false;
        TArray<FColor> Pixels; FIntVector Size;
        if (!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(), Pixels, Size))
        { Test->AddError(TEXT("Rabbit viewport capture failed")); return true; }
        const FString Directory = FPaths::ProjectSavedDir()/TEXT("GenerationJobs/rabbit-city_20260920_01/previews");
        IFileManager::Get().MakeDirectory(*Directory,true);
        const FString Base = Directory/FString::Printf(TEXT("gameplay-%d"),Stage);
        for (auto& Pixel : Pixels) Pixel.A = 255;
        TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
        FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png")));
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("id=%s\nposition=%s\nphase=%d\ntime=%.4f\npopulation=%d\njumps=%d\n"),
            *Rabbit->StableId,*Rabbit->Location.ToString(),int32(Rabbit->Activity),Rabbit->AnimationTime,
            Manager->GetLiveRabbitCount(),Manager->GetCompletedJumpCount()),*(Base+TEXT(".txt")));
        Test->AddInfo(FString::Printf(TEXT("Rabbit gameplay capture %d: %s"),Stage,*Base));
        ++Stage;
        return Stage >= 3;
    }
private:
    FAutomationTestBase* Test;
    double Started, Ready = 0;
    int32 Stage = 0;
    bool Prepared = false;
    FString FocusId;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRabbitCaptureTest,"Hansa.World.Rabbits.GameplayCapture",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FHansaRabbitCaptureTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FRabbitCapture(this)); return true; }
#endif
