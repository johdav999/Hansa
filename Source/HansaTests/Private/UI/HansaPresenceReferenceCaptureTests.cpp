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
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/SViewport.h"
namespace {
class FPresenceReferenceCapture final : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Initialized=false,Prepared=false;
public:
 explicit FPresenceReferenceCapture(FAutomationTestBase* T):Test(T){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Presence reference capture timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* M=Hud->GetTradeMapPresentationModel();float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  if(!Initialized){Root->SetPreferences({Scale>1,true,Scale>1,Scale});Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   Test->TestTrue(TEXT("Open real trade view"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));
   Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock"));Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence"));Root->ActivateSemanticId(TEXT("TradeMap.Page.Workspace"));
   Test->TestTrue(TEXT("Choose a real site"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Site")));Initialized=true;
  }
  if(!Prepared){
   if(Stage==1)Test->TestTrue(TEXT("Focus reveals lower content"),Root->FocusSemanticId(TEXT("TradeMap.Station.Terms")));
   if(Stage==2){Test->TestTrue(TEXT("Market footer can receive focus"),Root->FocusSemanticId(TEXT("TradeMap.Station.Market")));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));}
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.6)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  if(Stage<2){const auto* Scroll=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Station.Scroll");});const auto* Footer=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Station.Footer");});
   Test->TestTrue(TEXT("Scrollable body remains usable"),Scroll&&Scroll->Bounds.Height()>=100);
   Test->TestTrue(TEXT("Footer does not overlap body"),Scroll&&Footer&&Footer->Bounds.Min.Y>=Scroll->Bounds.Max.Y);
   for(const TCHAR* Id:{TEXT("TradeMap.Station.Market"),TEXT("TradeMap.Station.Action")}){const auto* N=Nodes.FindByPredicate([&](const auto& Node){return Node.Id==Id;});Test->TestTrue(TEXT("Footer action remains visible"),N&&N->State.bVisible&&N->Bounds.Height()>=FMath::FloorToInt(40*Scale));}
   Test->TestEqual(TEXT("Actual new-player blockers"),M->GetSnapshot().Establishment.UnmetRequirements,6);Test->TestFalse(TEXT("New player cannot reserve yet"),M->GetSnapshot().Establishment.bCanPropose);
  }
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real viewport capture failed"));return true;}
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("PresenceReference");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("presence-%dx%d-scale%.1f-stage%d"),Size.X,Size.Y,Scale,Stage);TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence=FString::Printf(TEXT("# ordinary-new-game city=%s revision=%llu viewport=%dx%d scale=%.1f\n"),*M->GetSnapshot().SelectedCityStableId.ToString(),M->GetRevision(),Size.X,Size.Y,Scale);
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bFocused,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return Stage>=3;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPresenceReferenceViewport,"Hansa.UI.PresenceReference.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FPresenceReferenceViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FPresenceReferenceCapture(this));return true;}
#endif
