#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "Widgets/SViewport.h"
namespace {
class FTopMenuCapture final:public IAutomationLatentCommand {
public:
    explicit FTopMenuCapture(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
    bool Update() override {
        if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("Top menu viewport timeout"));return true;}
        if(!GEngine||!GEngine->GameViewport)return false;
        auto* V=GEngine->GameViewport.Get();auto* World=V->GetWorld();if(!World||!World->HasBegunPlay())return false;
        auto* PC=Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController());
        auto* Hud=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;
        if(!Hud||!Hud->GetRootWidget().IsValid()||HansaWaitForFrontend(Hud))return false;
        auto Root=Hud->GetRootWidget();auto* Model=Hud->GetPresentationModel();
        if(Stage==0 && (Model->GetSnapshot().TopProducts.IsEmpty() || Model->GetSnapshot().TopProducts[0].Value.ToString().Contains(TEXT("—"))))
        {
            Model->SetSpeed(EHansaHudGameSpeed::Normal);
            return false; // Capture a real measured balance after the first market update.
        }
        if(!Prepared){
            Hud->GetScenarioPresentationModel()->Close();Hud->GetInspectorPresentationModel()->CloseIntent();
            Hud->GetBuildMenuPresentationModel()->SetOpen(false);Model->SetSpeed(EHansaHudGameSpeed::Paused);
            Hansa::UI::FUiPreferences Prefs;Prefs.UiScale=Stage==1?.8f:Stage==2?1.4f:1.f;
            Prefs.bLargeText=Stage==2;Prefs.bHighContrast=Stage==2;Prefs.bReducedMotion=true;Root->SetPreferences(Prefs);
            Root->FocusSemanticId(TEXT("HUD.TopStatus.Speed.Normal"));
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
            Test->TestEqual(TEXT("Native keyboard activates speed"),Model->GetSnapshot().Speed,EHansaHudGameSpeed::Normal);
            Model->SetSpeed(EHansaHudGameSpeed::Paused);
            Prepared=true;Ready=FPlatformTime::Seconds();return false;
        }
        if(FPlatformTime::Seconds()-Ready<.3)return false;
        TArray<FColor> Pixels;FIntVector Size;
        if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport readback failed"));return true;}
        const auto Nodes=Root->GetSemanticSnapshot();
        auto Find=[&](const TCHAR* Id){return Nodes.FindByPredicate([&](const auto& N){return N.Id==Id;});};
        const auto* Money=Find(TEXT("HUD.TopStatus.Money"));const auto* Product=Find(TEXT("HUD.TopStatus.Product.1"));
        const auto* LastProduct=Find(TEXT("HUD.TopStatus.Product.3"));const auto* Labor=Find(TEXT("HUD.TopStatus.Workforce"));
        const auto* Speed=Find(TEXT("HUD.TopStatus.Speed"));const auto* Fps=Find(TEXT("HUD.TopStatus.FPS"));
        const auto* RightPanel=Find(TEXT("HUD.TopStatus.RightPanel"));
        if(!Money||!Product||!Labor||!Speed||!Fps||!RightPanel){Test->AddError(TEXT("Top menu semantics missing"));return true;}
        Test->TestTrue(TEXT("Treasury lies left of centered production"),Money->Bounds.Max.X<=Product->Bounds.Min.X);
        Test->TestTrue(TEXT("Resident tiers lie below production"),Labor->Bounds.Min.Y>=Product->Bounds.Max.Y);
        Test->TestTrue(TEXT("Speed controls are right of production"),Speed->Bounds.Min.X>=Product->Bounds.Max.X);
        Test->TestTrue(TEXT("Empty product slots are absent"),!LastProduct && !Find(TEXT("HUD.TopStatus.Product.2")));
        Test->TestTrue(TEXT("Bread is the only product"),Product->State.Value.StartsWith(TEXT("Bread ")));
        Test->TestTrue(TEXT("Goods group is centered"),FMath::Abs((Product->Bounds.Min.X+Product->Bounds.Max.X)/2-Size.X/2)<=3);
        Test->TestTrue(TEXT("FPS count is labeled and stays inside the right status panel"),Fps->State.Value.StartsWith(TEXT("FPS "))&&
            Fps->Bounds.Min.X>=RightPanel->Bounds.Min.X&&Fps->Bounds.Min.Y>=RightPanel->Bounds.Min.Y&&
            Fps->Bounds.Max.X<=RightPanel->Bounds.Max.X&&Fps->Bounds.Max.Y<=RightPanel->Bounds.Max.Y);
        FString Evidence=TEXT("id\tx\ty\tright\tbottom\tvalue\n");
        for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("HUD.TopStatus"))&&N.State.bVisible){
            Test->TestTrue(TEXT("Visible top-menu item stays in viewport"),N.Bounds.Min.X>=0&&N.Bounds.Min.Y>=0&&N.Bounds.Max.X<=Size.X&&N.Bounds.Max.Y<=Size.Y);
            Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
        }
        for(const TCHAR* Id:{TEXT("HUD.TopStatus.Money"),TEXT("HUD.TopStatus.MoneyTrend"),TEXT("HUD.TopStatus.Population"),TEXT("HUD.TopStatus.Workforce"),TEXT("HUD.TopStatus.WealthyCitizens"),TEXT("HUD.TopStatus.Product.1"),TEXT("HUD.TopStatus.Speed.Fast"),TEXT("HUD.TopStatus.FPS")}){
            auto Widget=Root->ResolveSemanticWidget(Id);Test->TestTrue(TEXT("Metric/control has explanatory tooltip"),Widget.IsValid()&&Widget->GetToolTip().IsValid());
        }
        const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("TopMenu/topmenu-%dx%d-scale%d"),Size.X,Size.Y,Stage);
        TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
        Test->TestTrue(TEXT("Native screenshot saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
        Test->TestTrue(TEXT("Correlated geometry saved"),FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv"))));
        ++Stage;Prepared=false;return Stage==3;
    }
private:FAutomationTestBase* Test;double Start,Ready=0;int Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTopMenuNativeTest,"Hansa.UI.TopMenu.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FHansaTopMenuNativeTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTopMenuCapture(this));return true;}
#endif
