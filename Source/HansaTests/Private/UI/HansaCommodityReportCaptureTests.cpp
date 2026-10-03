#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaCityOverview.h"
#include "UI/SHansaMarketTable.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SScrollBox.h"

namespace {
class FCommodityReportCapture final : public IAutomationLatentCommand
{
public:
    explicit FCommodityReportCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
    bool Update() override
    {
        if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Commodity report capture timed out"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
        auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
        auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!H||!H->GetRootWidget())return false;
        auto Root=H->GetRootWidget();auto* Front=H->GetFrontendPresentationModel();
        if(Front&&Front->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
        if(Front&&Front->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
        if(H->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
        auto* M=H->GetMarketTablePresentationModel();auto Screen=Root->GetCityOverview();auto Table=Screen->GetMarketTable();
        auto Key=[](FKey K){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
        if(!Prepared)
        {
            if(Stage==0)
            {
                float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,Scale>1,Scale>1,Scale});
                H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);H->GetBuildMenuPresentationModel()->SetOpen(false);
                auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());Test->TestNotNull(TEXT("Authoritative game mode"),Mode);if(!Mode)return true;
                Mode->GetSimulationHost()->AdvanceTicks(1535);
                Test->TestTrue(TEXT("Open normal city overview"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.CityOverview")));
                Screen=Root->GetCityOverview();Table=Screen->GetMarketTable();
                Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Market"));Screen->ActivateSemanticId(TEXT("CityOverview.Market.Details"));
                Test->TestTrue(TEXT("Select actual raw hides report"),Table->ActivateSemanticId(TEXT("Market.Row.Good_RawHides")));
                const auto& D=M->GetSnapshot().SelectedGood;
                Test->TestTrue(TEXT("Raw hides has authoritative report"),D.bHasReport);
                Test->TestTrue(TEXT("Recorded price history exists"),D.History.Num()>1);
                const auto* Row=M->FindRow(TEXT("Good.RawHides"));Test->TestNotNull(TEXT("Raw hides row exists"),Row);
                if(Row)Test->TestEqual(TEXT("Detail price equals market price"),D.CurrentPriceMilliMarks,Row->PriceRaw);
            }
            if(Stage==1)
            {
                Test->TestTrue(TEXT("Fixed pin action accepts focus"),Table->FocusSemanticId(TEXT("Market.Detail.Action.Pin")));
                const bool Pinned=M->GetSnapshot().SelectedGood.bPinned;Key(EKeys::Enter);
                Test->TestTrue(TEXT("Enter toggles the real pin intent"),M->GetSnapshot().SelectedGood.bPinned!=Pinned);
                auto Scroll=StaticCastSharedPtr<SScrollBox>(Table->ResolveSemanticWidget(TEXT("Market.Detail.Scroll")));if(Scroll)Scroll->ScrollToEnd();
            }
            if(Stage==2)
            {
                Test->TestTrue(TEXT("Close report accepts controller focus"),Table->FocusSemanticId(TEXT("Market.Detail.Action.Close")));Key(EKeys::Gamepad_FaceButton_Bottom);
                Test->TestFalse(TEXT("Close hides report"),Table->ResolveSemanticWidget(TEXT("Market.Detail"))->GetVisibility().IsVisible());
                Test->TestEqual(TEXT("Close preserves selected good context"),M->GetSnapshot().SelectedGoodStableId,FName(TEXT("Good.RawHides")));
                Test->TestTrue(TEXT("Reselect same good reopens report"),Table->ActivateSemanticId(TEXT("Market.Row.Good_RawHides")));
                Test->TestTrue(TEXT("Reopened report visible"),Table->ResolveSemanticWidget(TEXT("Market.Detail"))->GetVisibility().IsVisible());
            }
            if(Stage==3){Test->TestTrue(TEXT("Other goods use same report family"),Table->ActivateSemanticId(TEXT("Market.Row.Good_Grain")));}
            if(Stage==4)
            {
                const auto Back=Screen->ResolveSemanticWidget(TEXT("CityOverview.Market.Back"));
                Test->TestTrue(TEXT("Compact market has a real return control"),Back.IsValid());
                const FString Target=Back&&Back->GetVisibility().IsVisible()?TEXT("CityOverview.Market.Back"):TEXT("CityOverview.Market.Details");
                Test->TestTrue(TEXT("Summary control accepts native focus"),Screen->FocusSemanticId(Target));Key(EKeys::Enter);
                Test->TestFalse(TEXT("Return restores market summary navigation"),Table->GetVisibility().IsVisible());
            }
            Prepared=true;Ready=FPlatformTime::Seconds();return false;
        }
        if(FPlatformTime::Seconds()-Ready<.5)return false;
        const auto Tree=Table->GetSemanticSnapshot();
        const auto* Action=Tree.FindByPredicate([](const auto& N){return N.Id==TEXT("Market.Detail.Action.BeginRoute");});
        if(Stage<4)
        {
            Test->TestTrue(TEXT("Route footer stays visible while body scrolls"),Action&&Action->State.bVisible&&Action->Bounds.Height()>=40);
            auto Scroll=Table->ResolveSemanticWidget(TEXT("Market.Detail.Scroll"));
            Test->TestTrue(TEXT("Report has usable body height at every supported scale"),Scroll&&Scroll->GetCachedGeometry().GetAbsoluteSize().Y>=140);
        }
        TArray<FColor> Pixels;FIntVector Size;
        if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real viewport capture failed"));return true;}
        int32 X=1920,Y=1080;float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
        Test->TestEqual(TEXT("Native screenshot width"),Size.X,X);Test->TestEqual(TEXT("Native screenshot height"),Size.Y,Y);
        const FString Dir=FPaths::ProjectSavedDir()/TEXT("CommodityReport");IFileManager::Get().MakeDirectory(*Dir,true);
        const FString Base=Dir/FString::Printf(TEXT("report-%dx%d-scale-%.1f-%d"),X,Y,Scale,Stage);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save actual viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
        FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
        for(const auto& N:Tree)Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
        FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
        ++Stage;Prepared=false;return Stage==5;
    }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCommodityReportViewport,"Hansa.UI.CommodityReport.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaCommodityReportViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FCommodityReportCapture(this));return true;}
#endif
