#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaTradeLedgerTestSupport.h"
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
class FTradeLedgerCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeLedgerCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Station establishment viewport timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Model=Hud->GetTradeMapPresentationModel();
  auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;if(!Host)return false;
  const FString City=TEXT("Rostock");
  static const TCHAR* States[]={TEXT("stock"),TEXT("operations"),TEXT("detail"),TEXT("reserved"),TEXT("recovery"),TEXT("columns")};
  if(!Prepared){
   if(Stage==0){FString Error;if(!Hansa::Tests::PrepareLedger(*Host,Error)){Test->AddError(Error);return true;}float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Test->TestTrue(TEXT("Open production trade UI"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock"));Test->TestTrue(TEXT("Open ledger"),Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Ledger")));Focus=TEXT("TradeMap.Ledger.Stock");}
   if(Stage==1){Test->TestTrue(TEXT("Open operations"),Root->ActivateSemanticId(TEXT("TradeMap.Ledger.Operations")));Focus=TEXT("TradeMap.Ledger.Operations");}
   if(Stage==2){Test->TestTrue(TEXT("Open causal detail"),Root->ActivateSemanticId(TEXT("TradeMap.Ledger.Good.Good.Timber")));Focus=TEXT("TradeMap.Ledger.Orders");}
   if(Stage==3){Root->ActivateSemanticId(TEXT("TradeMap.Ledger.Stock"));Test->TestTrue(TEXT("Controller focuses stock filter"),Root->FocusSemanticId(TEXT("TradeMap.Ledger.Filter")));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));Focus=TEXT("TradeMap.Ledger.Filter");}
   if(Stage==4){FString Error;if(!Hansa::Tests::PrepareLedger(*Host,Error,Hansa::Simulation::EHansaTradeStationOperationalState::Underfunded)){Test->AddError(Error);return true;}Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Ledger"));Root->ActivateSemanticId(TEXT("TradeMap.Ledger.Operations"));Focus=TEXT("TradeMap.Ledger.Operations");}
   if(Stage==5){Root->ActivateSemanticId(TEXT("TradeMap.Ledger.Stock"));Root->ActivateSemanticId(TEXT("TradeMap.Ledger.Columns"));Root->ActivateSemanticId(TEXT("TradeMap.Ledger.Columns"));Focus=TEXT("TradeMap.Ledger.Columns");}
   Test->TestTrue(TEXT("Focus actual workflow control"),Root->FocusSemanticId(Focus));Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  if(Stage==4&&!Scrolled){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_RightStick_Down,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_RightStick_Down,FModifierKeysState(),0,false,0,0));Scrolled=true;Ready=FPlatformTime::Seconds();return false;}
  const auto Nodes=Root->GetSemanticSnapshot();
  const auto* Control=Nodes.FindByPredicate([&](const auto& N){return N.Id==Focus;});
  Test->TestTrue(TEXT("Workflow action is revealed and reachable"),Control&&Control->State.bVisible&&Control->Bounds.Height()>=40);
  const auto* Heading=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Ledger.Title");});
  Test->TestTrue(TEXT("Ledger heading remains visible"),Heading&&Heading->State.bVisible&&Heading->Bounds.Height()>20);
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native viewport capture failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG09");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("ledger-%dx%d-scale%.1f-%s"),Size.X,Size.Y,Scale,States[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save native screenshot"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("# selectedCity=%s uiRevision=%llu station=%lld viewport=%dx%d scale=%.1f\n"),*Model->GetSnapshot().SelectedCityStableId.ToString(),Model->GetRevision(),Model->GetSnapshot().TradeStationValue,Size.X,Size.Y,Scale);
  if(Host){const auto P=Host->BuildProjection();if(P)Evidence+=FString::Printf(TEXT("# tick=%lld fingerprint=%llu fixture=TG09-stocked-station-saved-campaign\n"),P.Value.GetClock().GetTick().GetValue(),P.Value.GetFingerprint().Value);}
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bFocused,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage>=6;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false,Scrolled=false;FString Focus;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeLedgerViewport,"Hansa.UI.TradeLedger.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaTradeLedgerViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeLedgerCapture(this));return true;}
#endif
