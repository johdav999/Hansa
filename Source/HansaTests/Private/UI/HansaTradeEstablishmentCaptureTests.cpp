#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaTradeEstablishmentTestSupport.h"
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
class FTradeEstablishmentCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeEstablishmentCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
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
  static const TCHAR* States[]={TEXT("site"),TEXT("proposal-review"),TEXT("source"),TEXT("funding-review"),TEXT("constructing"),TEXT("complete"),TEXT("operations"),TEXT("office-progress")};
  auto ConfirmWithController=[&]{Test->TestTrue(TEXT("Focus confirmation for controller"),Root->FocusSemanticId(TEXT("TradeMap.Station.Confirm")));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));};
  if(!Prepared){
   if(Stage==0){FString Error;if(!Hansa::Tests::PrepareEstablishment(*Host,Error)){Test->AddError(Error);return true;}float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Test->TestTrue(TEXT("Open production trade UI"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock"));Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence"));Root->ActivateSemanticId(TEXT("TradeMap.Page.Workspace"));Test->TestTrue(TEXT("Choose site"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Site")));Test->TestTrue(TEXT("Expose site requirements"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));Focus=TEXT("TradeMap.Station.Terms");}
   if(Stage==1){Test->TestTrue(TEXT("Review proposal"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Action")));Focus=TEXT("TradeMap.Station.Confirm");}
   if(Stage==2){ConfirmWithController();Test->TestTrue(TEXT("Controller confirms proposal"),Model->GetSnapshot().Establishment.bProposed);Test->TestTrue(TEXT("Inspect empty source"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Source")));Test->TestTrue(TEXT("Choose second source"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Source")));Focus=TEXT("TradeMap.Station.Source");}
   if(Stage==3){Test->TestTrue(TEXT("Review exact spending"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Action")));Focus=TEXT("TradeMap.Station.Confirm");}
   if(Stage==4){ConfirmWithController();Test->TestTrue(TEXT("Constructing"),Model->GetSnapshot().Establishment.bConstructing);Focus=TEXT("TradeMap.Station.Site");}
   if(Stage==5){Host->AdvanceTicks(3);Test->TestTrue(TEXT("Station complete"),Model->GetSnapshot().Establishment.bComplete);Focus=TEXT("TradeMap.Station.Action");}
   if(Stage==6){Test->TestTrue(TEXT("Inspect operations"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Action")));Focus=TEXT("TradeMap.Ledger.Stock");}
   if(Stage==7){Test->TestTrue(TEXT("Return to Presence progression"),Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence")));Focus=TEXT("TradeMap.Navigate.Presence");}
   if(Stage==4)Focus=TEXT("TradeMap.Station.Terms");
   Test->TestTrue(TEXT("Focus actual workflow control"),Root->FocusSemanticId(Focus));Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  const auto* Control=Nodes.FindByPredicate([&](const auto& N){return N.Id==Focus;});
  Test->TestTrue(TEXT("Workflow action is revealed and reachable"),Control&&Control->State.bVisible&&Control->Bounds.Height()>=40);
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native viewport capture failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/(Stage==7?TEXT("TradeWorkspace/TG11"):TEXT("TradeWorkspace/TG08"));IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("station-%dx%d-scale%.1f-%s"),Size.X,Size.Y,Scale,States[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save native screenshot"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("# selectedCity=%s uiRevision=%llu station=%lld viewport=%dx%d scale=%.1f\n"),*Model->GetSnapshot().SelectedCityStableId.ToString(),Model->GetRevision(),Model->GetSnapshot().TradeStationValue,Size.X,Size.Y,Scale);
  if(Host){const auto P=Host->BuildProjection();if(P)Evidence+=FString::Printf(TEXT("# tick=%lld fingerprint=%llu fixture=TG08-qualified-two-owned-ships-saved-campaign\n"),P.Value.GetClock().GetTick().GetValue(),P.Value.GetFingerprint().Value);}
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bFocused,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage>=8;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;FString Focus;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeEstablishmentViewport,"Hansa.UI.TradeEstablishment.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaTradeEstablishmentViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeEstablishmentCapture(this));return true;}
#endif
