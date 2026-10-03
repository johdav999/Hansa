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
#include "World/HansaStrategyPlayerController.h"
#include "Widgets/SViewport.h"

namespace {
class FTradeWorkspaceCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeWorkspaceCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Production trade workspace launch timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  const TCHAR* Sections[]={TEXT("Map"),TEXT("Route"),TEXT("Presence"),TEXT("Orders"),TEXT("Specialization"),TEXT("Routes"),TEXT("Schedule"),TEXT("Creator"),TEXT("Review"),TEXT("PresenceFocus"),TEXT("RouteFocus")};
  const FString Id=Stage==0||Stage>=5?TEXT("TradeMap.Page.")+FString(Sections[Stage]):TEXT("TradeMap.Navigate.")+FString(Sections[Stage]);
  if(!Prepared){
   if(Stage==0){float Scale=1.f;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});Test->TestEqual(TEXT("Requested UI scale applied"),Root->GetPreferences().UiScale,Scale);Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Test->TestTrue(TEXT("Production HUD opens replacement trade workspace"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));}
   if(Stage==9){Root->ActivateSemanticId(TEXT("TradeMap.Creator.Discard"));Root->ActivateSemanticId(TEXT("TradeMap.Creator.Discard"));Test->TestTrue(TEXT("Locked presence tab remains reachable"),Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence")));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::PageDown,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::PageDown,FModifierKeysState(),0,false,0,0));}
   else if(Stage==10){Test->TestTrue(TEXT("Route save focus reveals its workflow"),Root->FocusSemanticId(TEXT("TradeMap.Editor.Save")));}
   else if(Stage==7){Test->TestTrue(TEXT("Native creator opens"),Root->ActivateSemanticId(TEXT("TradeMap.New")));Test->TestTrue(TEXT("Creator field is focus revealed"),Root->FocusSemanticId(TEXT("TradeMap.Creator.Name")));}
   else if(Stage==8){Test->TestTrue(TEXT("Native review opens"),Root->ActivateSemanticId(TEXT("TradeMap.Creator.Review")));}
   else if(Stage==0||Stage>=5){Test->TestTrue(TEXT("Native page intent accepted"),Root->ActivateSemanticId(Id));}
   else {
   Test->TestTrue(TEXT("Tab receives real keyboard focus"),Root->FocusSemanticId(Id));
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   }
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<1)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  if(Stage>0&&Stage<5)Test->TestTrue(TEXT("In-game tab is selected and visible"),Nodes.ContainsByPredicate([&](const auto& N){return N.Id==Id&&N.State.bVisible&&N.State.bSelected;}));
  if(Stage>=7&&Stage!=9){const FString Action=Stage==7?TEXT("TradeMap.Creator.Name"):Stage==8?TEXT("TradeMap.Creator.Edit"):Stage==9?TEXT("TradeMap.Station.Action"):TEXT("TradeMap.Editor.Save");Test->TestTrue(TEXT("Critical creator action remains visible"),Nodes.ContainsByPredicate([&](const auto& N){return N.Id==Action&&N.State.bVisible&&N.Bounds.Height()>=FMath::FloorToInt(47*Root->GetPreferences().UiScale);}));}
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real viewport capture failed"));return true;}
  float UiScale=1.f;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),UiScale);
  const FString Directory=FPaths::ProjectSavedDir()/FString::Printf(TEXT("TradeWorkspace/TG02/scale-%.1f"),UiScale);IFileManager::Get().MakeDirectory(*Directory,true);
  const FString Base=Directory/FString::Printf(TEXT("trade-%dx%d-%s"),Size.X,Size.Y,Sections[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved assembled-game screenshot"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap."))||N.Id.StartsWith(TEXT("HUD.TopStatus.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==11;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeWorkspaceViewport,"Hansa.UI.TradeWorkspace.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaTradeWorkspaceViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeWorkspaceCapture(this));return true;}
#endif
