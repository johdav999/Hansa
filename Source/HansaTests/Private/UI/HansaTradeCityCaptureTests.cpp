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
class FTradeCityCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeCityCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("City inspector viewport timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Model=Hud->GetTradeMapPresentationModel();
  const FString City=Stage<2?TEXT("Lubeck"):Stage<4?TEXT("Rostock"):TEXT("Hamburg");
  if(!Prepared){
   if(Stage==0){float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Test->TestTrue(TEXT("Open production trade UI"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));}
   Test->TestTrue(TEXT("Select city through ordinary port control"),Root->ActivateSemanticId(TEXT("TradeMap.Port.City.")+City));
   Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Overview"));Root->ActivateSemanticId(TEXT("TradeMap.Page.Workspace"));
   if(Stage%2)Test->TestTrue(TEXT("Primary remedy can receive keyboard/controller focus"),Root->FocusSemanticId(TEXT("TradeMap.Overview.Presence")));
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  const auto* Identity=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Overview.Identity");});
  Test->TestTrue(TEXT("City identity remains visible"),Identity&&Identity->State.bVisible&&Identity->Bounds.Height()>0);
  Test->TestEqual(TEXT("Actual city summary correlated"),Model->GetSnapshot().CityInspector.CityId,FName(*(TEXT("City.")+City)));
  if(Stage%2){const auto* Primary=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Overview.Presence");});Test->TestTrue(TEXT("Remedy focus reveals visible target"),Primary&&Primary->State.bVisible&&Primary->Bounds.Height()>=40);}
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native viewport capture failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG07");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("city-%dx%d-scale%.1f-%s-%s"),Size.X,Size.Y,Scale,*City,Stage%2?TEXT("remedy"):TEXT("overview"));
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save native screenshot"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;
  FString Evidence=FString::Printf(TEXT("# selectedCity=%s uiRevision=%llu station=%lld viewport=%dx%d scale=%.1f\n"),*Model->GetSnapshot().SelectedCityStableId.ToString(),Model->GetRevision(),Model->GetSnapshot().TradeStationValue,Size.X,Size.Y,Scale);
  if(Host){const auto P=Host->BuildProjection();if(P)Evidence+=FString::Printf(TEXT("# tick=%lld fingerprint=%llu fixture=normal-new-game\n"),P.Value.GetClock().GetTick().GetValue(),P.Value.GetFingerprint().Value);}
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bFocused,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  if(Stage%2){Test->TestTrue(TEXT("Primary action receives controller input"),Root->FocusSemanticId(TEXT("TradeMap.Overview.Presence")));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));Model->SetCitySearchIntent(TEXT(""));}
  ++Stage;Prepared=false;return Stage>=6;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeCityViewport,"Hansa.UI.TradeCity.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaTradeCityViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeCityCapture(this));return true;}
#endif
