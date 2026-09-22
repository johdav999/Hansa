#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
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
  const TCHAR* Sections[]={TEXT("Route"),TEXT("Presence"),TEXT("Orders"),TEXT("Specialization")};
  if(!Prepared){
   if(Stage==0){Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Test->TestTrue(TEXT("Production HUD opens replacement trade workspace"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));}
   const FString Id=TEXT("TradeMap.Navigate.")+FString(Sections[Stage]);
   Test->TestTrue(TEXT("Tab receives real keyboard focus"),Root->FocusSemanticId(Id));
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<1)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  const FString Selected=TEXT("TradeMap.Navigate.")+FString(Sections[Stage]);
  Test->TestTrue(TEXT("In-game tab is selected and visible"),Nodes.ContainsByPredicate([&](const auto& N){return N.Id==Selected&&N.State.bVisible&&N.State.bSelected;}));
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real viewport capture failed"));return true;}
  const FString Directory=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace");IFileManager::Get().MakeDirectory(*Directory,true);
  const FString Base=Directory/FString::Printf(TEXT("trade-%dx%d-%s"),Size.X,Size.Y,Sections[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved assembled-game screenshot"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==4;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeWorkspaceViewport,"Hansa.UI.TradeWorkspace.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaTradeWorkspaceViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeWorkspaceCapture(this));return true;}
#endif
