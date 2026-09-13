#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/HansaResearchPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/SViewport.h"

namespace {
class FResearchCapture final : public IAutomationLatentCommand {
public:
 explicit FResearchCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("Research native flow timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;
  if(!Hud||!Hud->GetRootWidget()||!Host)return false;
  if(HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();auto* Model=Hud->GetResearchPresentationModel();
  auto Press=[&](const TCHAR* Id){
   if(!Test->TestTrue(FString::Printf(TEXT("Native focus %s"),Id),Root->FocusSemanticId(Id)))return;
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
  };
  if(!Prepared){
   switch(Stage){
    case 0:FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});Hud->GetScenarioPresentationModel()->Close();Hud->GetBuildMenuPresentationModel()->SetOpen(false);Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Press(TEXT("HUD.TopStatus.Research.Open"));break;
    case 1:Press(TEXT("Research.Node.Technology_Commerce_TransactionFriction"));break;
    case 2:Press(TEXT("Research.Prerequisite.Technology_Commerce_MarketReports"));Press(TEXT("Research.Action.Queue"));Test->TestTrue(TEXT("Controller queues authoritative research"),!Host->BuildProjection().Value.GetResearch()[0].ActiveTechnologyId.IsEmpty());break;
    case 3:Press(TEXT("Research.Close"));Press(TEXT("HUD.TopStatus.Speed.Fastest"));Press(TEXT("HUD.TopStatus.Research.Open"));break;
    case 4:Press(TEXT("Research.Effect.0"));Test->TestTrue(TEXT("Applied commerce effect opens market"),Hud->GetCityOverviewPresentationModel()->GetSnapshot().bOpen);break;
    case 5:Hud->GetCityOverviewPresentationModel()->CloseIntent();Press(TEXT("HUD.TopStatus.Research.Open"));Press(TEXT("Research.Node.Technology_Production_ImprovedMilling"));Press(TEXT("Research.Effect.0"));Test->TestFalse(TEXT("Production causal link leaves research for building"),Model->GetSnapshot().bOpen);break;
    case 6:Press(TEXT("HUD.TopStatus.Research.Open"));Root->SetPreferences({true,true,true,Scale});Press(TEXT("Research.Node.Technology_Logistics_RouteScheduling"));break;
    case 7:Press(TEXT("Research.Effect.0"));Test->TestTrue(TEXT("Logistics effect opens affected route"),Hud->GetTradeMapPresentationModel()->GetSnapshot().bOpen);break;
    case 8:Hud->GetTradeMapPresentationModel()->CloseIntent();Press(TEXT("HUD.TopStatus.Research.Open"));Model->SetLoading(true);break;
    case 9:Model->SetError(FText::FromString(TEXT("Research could not start. Check the current requirements and try again.")));break;
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.8)return false;
  if(Stage==3){if(!Host->BuildProjection().Value.GetResearch()[0].IsCompleted(TEXT("Technology.Commerce.MarketReports")))return false;Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);}
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native screenshot failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
  Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  if(Stage==0){const auto Nodes=Root->GetSemanticSnapshot();const auto* N=Nodes.FindByPredicate([](const auto& It){return It.Id==TEXT("Research.Action.Queue");});Test->TestTrue(TEXT("Pinned action stays visible with 48px target"),N&&N->State.bEnabled&&N->Bounds.Height()>=47&&N->Bounds.Min.X>=0&&N->Bounds.Min.Y>=0&&N->Bounds.Max.X<=X&&N->Bounds.Max.Y<=Y);}
  const TCHAR* Names[]={TEXT("available"),TEXT("locked"),TEXT("active"),TEXT("completed"),TEXT("market-link"),TEXT("building-link"),TEXT("accessible-locked"),TEXT("route-link"),TEXT("loading-fixture"),TEXT("error-fixture")};
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P27/research-%dx%d-scale%d-%s"),X,Y,FMath::RoundToInt(Scale*100),Names[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Native capture saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot())Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==10;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;float Scale=1.f;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FResearchViewport,"Hansa.UI.Research.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FResearchViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FResearchCapture(this));return true;}
#endif
