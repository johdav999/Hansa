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
#include "World/HansaAmbientAnimals.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"

namespace
{
class FDogCapture final : public IAutomationLatentCommand
{
public:
    explicit FDogCapture(FAutomationTestBase* InTest) : Test(InTest), Started(FPlatformTime::Seconds()) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Started > 240) { Test->AddError(TEXT("Dog gameplay capture timed out")); return true; }
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
        AHansaAmbientAnimals* Manager = nullptr;
        for (TActorIterator<AHansaAmbientAnimals> It(World); It; ++It) { Manager = *It; break; }
        if (!Manager || Manager->GetLiveAnimalCount(TEXT("Dog")) < 2) return false;
        if (!Manager->GetValidationError().IsEmpty()) { Test->AddError(Manager->GetValidationError()); return true; }
        if (Stage == 0 && !Prepared)
        {
            Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
            const auto Observations = Manager->QueryAnimals();
            const auto* FirstDog = Observations.FindByPredicate([](const auto& O) { return O.SpeciesId == TEXT("Dog"); });
            if (!FirstDog) return false;
            FocusId = FirstDog->StableId;
            Test->TestTrue(TEXT("Two or three dogs in actual game"), Manager->GetLiveAnimalCount(TEXT("Dog")) <= 3);
            Test->TestEqual(TEXT("Six rabbits coexist"), Manager->GetLiveAnimalCount(TEXT("Rabbit")),6);
            Camera->AddZoomIntent((Camera->GetZoomDistance()-650)/Camera->ZoomUnitsPerStep);
            Camera->FocusWorldLocationIntent(FirstDog->Location);
            Ready = FPlatformTime::Seconds()+5;
            Prepared = true;
            return false;
        }
        const auto Observations = Manager->QueryAnimals();
        const auto* Dog = Observations.FindByPredicate([&](const auto& O) { return O.StableId == FocusId; });
        if (!Dog || !Dog->bVisible) return false;
        Camera->FocusWorldLocationIntent(Dog->Location);
        if (FPlatformTime::Seconds() < Ready) return false;
        if (Stage == 1 && Dog->Activity != EHansaAnimalActivity::Walk) return false;
        if (Stage == 2 && (Dog->Activity != EHansaAnimalActivity::Still || Dog->bTurning)) return false;
        TArray<FColor> Pixels; FIntVector Size;
        if (!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(), Pixels, Size))
        { Test->AddError(TEXT("Dog viewport capture failed")); return true; }
        const FString Directory = FPaths::ProjectSavedDir()/TEXT("GenerationJobs/ambient-animals_20260923_01/previews");
        IFileManager::Get().MakeDirectory(*Directory,true);
        const FString Base = Directory/FString::Printf(TEXT("dog-gameplay-%d"),Stage);
        for (auto& Pixel : Pixels) Pixel.A = 255;
        TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);
        FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png")));
        FFileHelper::SaveStringToFile(FString::Printf(TEXT("id=%s\nposition=%s\nphase=%d\ntime=%.4f\npopulation=%d\njumps=%d\n"),
            *Dog->StableId,*Dog->Location.ToString(),int32(Dog->Activity),Dog->AnimationTime,
            Manager->GetLiveAnimalCount(TEXT("Dog")),Manager->GetCompletedJumpCount()),*(Base+TEXT(".txt")));
        Test->AddInfo(FString::Printf(TEXT("Dog gameplay capture %d: %s"),Stage,*Base));
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaDogCaptureTest,"Hansa.World.Animals.GameplayCapture",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FHansaDogCaptureTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FDogCapture(this)); return true; }
#endif
