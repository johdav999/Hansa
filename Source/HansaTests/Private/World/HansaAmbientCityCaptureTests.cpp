#if WITH_DEV_AUTOMATION_TESTS
#include "../UI/HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "ImageUtils.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaCityCentrePresentation.h"
#include "World/HansaAmbientPeople.h"
#include "World/HansaAmbientAnimals.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
class FAmbientCityCapture final : public IAutomationLatentCommand
{
    FAutomationTestBase* Test;
    double Started=FPlatformTime::Seconds(), Ready=0;
    int32 Stage=0;
    FVector SavedFocus=FVector::ZeroVector;
    TArray<FHansaAnimalObservation> Sleeping;
public:
    explicit FAmbientCityCapture(FAutomationTestBase* In):Test(In){}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Started>240) { Test->AddError(TEXT("Ambient city viewport timed out")); return true; }
        auto* V=GEngine?GEngine->GameViewport.Get():nullptr; auto* W=V?V->GetWorld():nullptr;
        auto* PC=W?W->GetFirstPlayerController():nullptr; auto* Hud=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;
        if (!Hud || !Hud->GetRootWidget() || !Hud->GetFrontendPresentationModel()) return false;
        auto* Front=Hud->GetFrontendPresentationModel();
        if (Front->GetSnapshot().Page==EHansaFrontendPage::Title) { Hud->GetRootWidget()->ActivateSemanticId(TEXT("Frontend.NewGame")); return false; }
        if (Front->GetSnapshot().Page==EHansaFrontendPage::Loading) return false;
        if (Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing) { Hud->GetRootWidget()->ActivateSemanticId(TEXT("Scenario.Begin")); return false; }
        auto* Mode=W->GetAuthGameMode<AHansaGameMode>(); auto* Host=Mode?Mode->GetSimulationHost():nullptr;
        auto* Camera=Cast<AHansaStrategyCameraPawn>(PC->GetPawn()); if (!Host || !Camera) return false;
        AHansaCityCentrePresentation* Centre=nullptr; AHansaAmbientPeople* People=nullptr; AHansaAmbientAnimals* Animals=nullptr;
        for(TActorIterator<AHansaCityCentrePresentation> It(W);It;++It)if(It->CityId==TEXT("City.Rostock")){Centre=*It;break;}
        for(TActorIterator<AHansaAmbientPeople> It(W);It;++It){People=*It;break;}
        for(TActorIterator<AHansaAmbientAnimals> It(W);It;++It){Animals=*It;break;}
        if(!Centre||!People||!Animals)return false;
        if(Stage==0)
        {
            Hud->GetScenarioPresentationModel()->DismissHelp(); Host->SetSpeed(EHansaRuntimeSimulationSpeed::Normal);
            Camera->bEnableMouseEdgePan=false; Camera->ClearCameraIntents(); Camera->MinimumZoomDistance=800;
            if(!Hud->VisitCityIntent(TEXT("City.Rostock"))){Test->AddError(TEXT("Normal Rostock visit rejected"));return true;}
            SavedFocus=Centre->GetMarketLocation(); Camera->FocusWorldLocationIntent(SavedFocus);
            Camera->AddZoomIntent((Camera->GetZoomDistance()-6500)/Camera->ZoomUnitsPerStep);
            Ready=FPlatformTime::Seconds()+12; Stage=1; return false;
        }
        if(FPlatformTime::Seconds()<Ready)return false;
        const auto Citizens=People->QueryPeople(); const auto Wildlife=Animals->QueryAnimals();
        if(Stage==1)
        {
            if(!Citizens.ContainsByPredicate([](const auto& P){return P.bVisible;}) || !Wildlife.ContainsByPredicate([](const auto& A){return A.bVisible && A.SpeciesId==TEXT("Dog");}))return false;
            Capture(V,TEXT("overview"),Citizens,Wildlife);
            const auto* Person=Citizens.FindByPredicate([](const auto& P){return P.bVisible;});
            SavedFocus=Person->Location; Camera->FocusWorldLocationIntent(SavedFocus);
            Camera->SetActorLocation(FVector(SavedFocus.X,SavedFocus.Y,SavedFocus.Z));
            Camera->AddZoomIntent((Camera->GetZoomDistance()-1400)/Camera->ZoomUnitsPerStep);
            Ready=FPlatformTime::Seconds()+3; Stage=2;return false;
        }
        if(Stage==2)
        {
            Capture(V,TEXT("citizens"),Citizens,Wildlife);
            const auto* Dog=Wildlife.FindByPredicate([](const auto& A){return A.SpeciesId==TEXT("Dog");});
            if(!Dog){Test->AddError(TEXT("No dog pool"));return true;}
            SavedFocus=Dog->Location; Camera->FocusWorldLocationIntent(SavedFocus);
            Camera->SetActorLocation(FVector(SavedFocus.X,SavedFocus.Y,SavedFocus.Z));
            Camera->AddZoomIntent((Camera->GetZoomDistance()-2600)/Camera->ZoomUnitsPerStep);
            Ready=FPlatformTime::Seconds()+3; Stage=3;return false;
        }
        if(Stage==3)
        {
            if(!Wildlife.ContainsByPredicate([](const auto& A){return A.bVisible && A.SpeciesId==TEXT("Dog");}))return false;
            Capture(V,TEXT("animals"),Citizens,Wildlife);
            Camera->FocusWorldLocationIntent(SavedFocus+FVector(200000,200000,0));
            Ready=FPlatformTime::Seconds()+1; Stage=4;return false;
        }
        if(Stage==4) { Sleeping=Wildlife; Ready=FPlatformTime::Seconds()+2; Stage=5;return false; }
        if(Stage==5)
        {
            for(int32 I=0;I<Wildlife.Num();++I)
            {
                Test->TestFalse(TEXT("Distant wildlife hidden in actual game"),Wildlife[I].bVisible);
                Test->TestEqual(TEXT("Distant wildlife pose frozen in actual game"),Wildlife[I].AnimationTime,Sleeping[I].AnimationTime);
                Test->TestEqual(TEXT("Distant wildlife motion frozen in actual game"),Wildlife[I].Location,Sleeping[I].Location);
            }
            for(const auto& P:Citizens)Test->TestFalse(TEXT("Distant citizens hidden in actual game"),P.bVisible);
            Camera->FocusWorldLocationIntent(Centre->GetMarketLocation());
            Camera->AddZoomIntent((Camera->GetZoomDistance()-6500)/Camera->ZoomUnitsPerStep);
            Ready=FPlatformTime::Seconds()+4; Stage=6;return false;
        }
        Test->TestEqual(TEXT("Return selects Rostock"),People->GetAmbientCity(),FName(TEXT("City.Rostock")));
        Test->TestTrue(TEXT("Return resumes crowd"),Citizens.ContainsByPredicate([](const auto& P){return P.bVisible;}));
        Test->TestTrue(TEXT("Return resumes wildlife"),Wildlife.ContainsByPredicate([](const auto& A){return A.bVisible;}));
        return true;
    }
    void Capture(UGameViewportClient* View,const TCHAR* Name,const TArray<FHansaPersonObservation>& Citizens,const TArray<FHansaAnimalObservation>& Wildlife)
    {
        TArray<FColor> Pixels; FIntVector Size;
        if(!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Ambient screenshot failed"));return;}
        const FString Directory=FPaths::ProjectDir()/TEXT("Docs/Images/World/AmbientCities"); IFileManager::Get().MakeDirectory(*Directory,true);
        const FString Base=Directory/FString::Printf(TEXT("rostock--%s--%dx%d"),Name,Size.X,Size.Y);
        for(auto& Pixel:Pixels)Pixel.A=255;
        TArray64<uint8> PNG; FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*(Base+TEXT(".png")));
        FString Evidence;
        if (auto* PC=View->GetWorld()->GetFirstPlayerController())
            if (const auto* Camera=Cast<AHansaStrategyCameraPawn>(PC->GetPawn()))
                Evidence+=FString::Printf(TEXT("CameraFocus=%s Zoom=%f Pawn=%s\n"),*Camera->GetFocusLocation2D().ToString(),Camera->GetZoomDistance(),*Camera->GetActorLocation().ToString());
        for(const auto& P:Citizens)Evidence+=FString::Printf(TEXT("Citizen %d city=%s visible=%d activity=%d location=%s landmark=%s\n"),P.PersonId,*P.CityId.ToString(),P.bVisible,int32(P.Activity),*P.Location.ToString(),*P.DestinationLandmark.ToString());
        for(const auto& A:Wildlife)Evidence+=FString::Printf(TEXT("%s visible=%d activity=%d location=%s pose=%f\n"),*A.StableId,A.bVisible,int32(A.Activity),*A.Location.ToString(),A.AnimationTime);
        FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));Test->AddInfo(Base);
    }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAmbientCityCaptureTest,"Hansa.World.AmbientCities.GameplayCapture",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FHansaAmbientCityCaptureTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FAmbientCityCapture(this));return true;}
#endif
