#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaTradeDecisionTestSupport.h"
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
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Internationalization/Internationalization.h"
namespace {
class FDecisionCapture final : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false;FString Focus;
public:
 explicit FDecisionCapture(FAutomationTestBase* T):Test(T){}
 bool Update() override{
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Decision viewport timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){if(!Root->ActivateSemanticId(TEXT("Frontend.NewGame"))){Test->AddError(TEXT("Production New Game failed before specialization setup; inspect runtime catalog diagnostics."));return true;}return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* M=Hud->GetTradeMapPresentationModel();auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;if(!Host)return false;
  auto Act=[&](const FString& S){return Root->ActivateSemanticId(TEXT("TradeMap.Decisions.")+S);};
  auto Key=[&](FKey K){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
  if(!Prepared){
   if(Stage==0){FString Error;if(!Hansa::Tests::PrepareDecisions(*Host,Error)){Test->AddError(Error);return true;}
    float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});
    Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap"));Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock"));Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Decisions"));Focus=TEXT("Privilege.AdditionalCommercialPlot");
   }
   if(Stage==1){Act(TEXT("Privilege.AdditionalCommercialPlot"));Act(TEXT("Source"));Focus=TEXT("Review");}
   if(Stage==2){Root->FocusSemanticId(TEXT("TradeMap.Decisions.Review"));Key(EKeys::Enter);Test->TestTrue(TEXT("Keyboard opens review"),M->GetSnapshot().bDecisionReview);Focus=TEXT("Confirm");}
   if(Stage==3){Key(EKeys::Gamepad_FaceButton_Bottom);Test->TestEqual(TEXT("Controller grants privilege"),M->GetSnapshot().Decisions.Revision,int64(1));Focus=TEXT("Privilege.AdditionalCommercialPlot");}
   if(Stage==4){Act(TEXT("Project.PublicGranary"));Act(TEXT("Review"));Focus=TEXT("Confirm");}
   if(Stage==5){Key(EKeys::Gamepad_FaceButton_Bottom);Test->TestEqual(TEXT("Controller funds project"),M->GetSnapshot().Decisions.Revision,int64(2));Focus=TEXT("Project.PublicGranary");}
   if(Stage==6){Host->AdvanceTicks(3);Focus=TEXT("Project.PublicGranary");}
   if(Stage==7){Act(TEXT("Charter"));Focus=TEXT("Charter");}
   if(Stage==8){Focus=TEXT("Terms");}
   Test->TestTrue(TEXT("Controller focus reaches option or dimension"),Root->FocusSemanticId(TEXT("TradeMap.Decisions.")+Focus));Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.15)return false;
  const auto Nodes=Root->GetSemanticSnapshot();const auto* N=Nodes.FindByPredicate([&](const auto& X){return X.Id==TEXT("TradeMap.Decisions.")+Focus;});
  Test->TestTrue(TEXT("Focused comparison control is visibly revealed"),N&&N->State.bVisible&&N->Bounds.Height()>=40);
  if(Stage<9){
   TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return true;}
   float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG14");IFileManager::Get().MakeDirectory(*Dir,true);
   FString Culture;FParse::Value(FCommandLine::Get(),TEXT("culture="),Culture);if(FParse::Param(FCommandLine::Get(),TEXT("HansaGuiPseudoLocale"))&&Stage>=6)Culture=TEXT("LEET");
   const FString Base=Dir/FString::Printf(TEXT("decisions-%dx%d-scale%.1f-%02d%s"),Size.X,Size.Y,Scale,Stage,Culture.IsEmpty()?TEXT(""):*FString(TEXT("-")+Culture));
   TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
   FString Evidence=FString::Printf(TEXT("# stage=%d revision=%lld focus=%s packaged=%d\n"),Stage,M->GetSnapshot().Decisions.Revision,*Focus,FPlatformProperties::RequiresCookedData());
   for(const auto& X:Nodes)if(X.Id.StartsWith(TEXT("TradeMap.Decisions.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*X.Id,X.State.bVisible,X.State.bFocused,X.Bounds.Min.X,X.Bounds.Min.Y,X.Bounds.Max.X,X.Bounds.Max.Y,*X.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  }
  ++Stage;Prepared=false;if(Stage>=9&&FParse::Param(FCommandLine::Get(),TEXT("HansaGuiPseudoLocale")))FInternationalization::Get().SetCurrentCulture(TEXT("en"));return Stage>=9;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeDecisionViewport,"Hansa.UI.TradeDecisions.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FTradeDecisionViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FDecisionCapture(this));return true;}
#endif
