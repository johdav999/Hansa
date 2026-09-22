#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "Widgets/SViewport.h"

namespace {
class FAlertPanelCapture final : public IAutomationLatentCommand {
public:
 explicit FAlertPanelCapture(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>120){Test->AddError(TEXT("Alert viewport timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();auto* Model=H->GetPresentationModel();
  if(!Prepared){
   H->GetScenarioPresentationModel()->Close();H->GetBuildMenuPresentationModel()->SetOpen(false);Model->SetSpeed(EHansaHudGameSpeed::Paused);
   if(Stage==0){
    auto State=Model->GetSnapshot();
    const TCHAR* Goods[]={TEXT("Grain"),TEXT("Bread"),TEXT("Flour"),TEXT("Fish"),TEXT("Timber"),TEXT("Planks"),TEXT("Tools")};
    for(int32 I=0;I<7;++I){
     FHansaHudAlertPresentation A;A.StableId=FName(*FString::Printf(TEXT("Capture.Market.%d"),I));A.GroupId=TEXT("Market");
     A.Label=FText::FromString(FString::Printf(TEXT("%s reserve is low"),Goods[I]));A.AffectedObject=FText::FromString(TEXT("Lübeck"));A.Age=FText::FromString(TEXT("12 ticks"));
     A.bWarning=true;A.Causal.Severity=EHansaCausalSeverity::Warning;A.Causal.Problem=A.Label;
     A.Causal.Cause=FText::FromString(TEXT("Stock is below the desired reserve."));A.Causal.Evidence=FText::FromString(TEXT("Stock 4; reserve 12."));A.Causal.Remedy=FText::FromString(TEXT("Review production and incoming supply."));State.Alerts.Add(A);
    }
    Model->ApplySnapshot(State);
   }
   if(Stage==1){
    FString Id;for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.EndsWith(TEXT(".OpenCause"))&&N.Id.StartsWith(TEXT("HUD.AlertStack"))&&N.State.bVisible){Id=N.Id;break;}
    auto Before=Root->ResolveSemanticWidget(Id);Test->TestTrue(TEXT("Live alert action exists"),Before.IsValid());
    if(Before){Root->FocusSemanticId(Id);for(int32 Tick=1;Tick<=120;++Tick){auto State=Model->GetSnapshot();for(auto& A:State.Alerts){A.Age=FText::Format(FText::FromString(TEXT("{0} ticks")),FText::AsNumber(Tick));A.Causal.Evidence=FText::AsNumber(Tick);}Model->ApplySnapshot(State);Test->TestTrue(TEXT("Live tick retains native action"),Before==Root->ResolveSemanticWidget(Id));}}
   }
   if(Stage==2)Model->ToggleAlertStack();
   if(Stage==3){Model->ToggleAlertStack();Root->SetPreferences({true,true,true,1.f});}
   if(Stage==4)Root->SetPreferences({false,true,false,.8f});
   if(Stage==5){Root->SetPreferences({false,true,false,1.4f});
    Test->TestTrue(TEXT("Controller can reveal the last grouped market alert"),Root->FocusSemanticId(TEXT("HUD.AlertStack.Alert.Capture_Market_6.OpenCause")));
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.75)return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return true;}
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("AlertPanel/alerts-%dx%d-%d"),Size.X,Size.Y,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;
  for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("HUD.AlertStack"))){
   Evidence+=FString::Printf(TEXT("%s\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
   if(N.Id==TEXT("HUD.AlertStack")) Test->TestTrue(TEXT("Alert panel stays in viewport"),N.Bounds.Min.X>=0 && N.Bounds.Min.Y>=0 && N.Bounds.Max.X<=Size.X && N.Bounds.Max.Y<=Size.Y);
  }
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==6;
 }
private:FAutomationTestBase* Test;double Started,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAlertPanelViewport,"Hansa.UI.AlertPanel.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaAlertPanelViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FAlertPanelCapture(this));return true;}
#endif

