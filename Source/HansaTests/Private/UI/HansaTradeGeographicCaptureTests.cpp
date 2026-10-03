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
class FTradeGeographicCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeGeographicCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>240){Test->AddError(TEXT("Geographic map capture timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto Key=[](FKey K){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
  if(!Prepared){
   if(Stage==0){float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Test->TestTrue(TEXT("Open real trade map"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));Root->ActivateSemanticId(TEXT("TradeMap.Page.Map"));for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("TradeMap.City.City_")))Cities.Add(N.Id);Test->TestEqual(TEXT("All 30 current cities remain discoverable"),Cities.Num(),30);Name=TEXT("Routes");}
   if(Stage==1){Root->ActivateSemanticId(TEXT("TradeMap.Chart.Overlay"));Name=TEXT("GoodReports");}
   if(Stage==2){Root->ActivateSemanticId(TEXT("TradeMap.Chart.Overlay"));Name=TEXT("Presence");}
   if(Stage==3){Root->ActivateSemanticId(TEXT("TradeMap.Chart.Overlay"));Root->ActivateSemanticId(TEXT("TradeMap.Chart.Overlay"));Root->ActivateSemanticId(TEXT("TradeMap.Chart.Thickness"));Test->TestTrue(TEXT("Canvas takes real focus"),Root->ActivateSemanticId(TEXT("TradeMap.Chart.Focus")));Key(EKeys::Gamepad_DPad_Right);Key(EKeys::Gamepad_FaceButton_Bottom);Name=TEXT("ControllerCity");}
   if(Stage==4){Root->ActivateSemanticId(TEXT("TradeMap.Chart.Focus"));Key(EKeys::Gamepad_RightShoulder);Key(EKeys::D);Name=TEXT("ZoomPan");}
   if(Stage==5){Root->ActivateSemanticId(TEXT("TradeMap.Chart.Focus"));Key(EKeys::Home);Key(EKeys::Gamepad_FaceButton_Left);Name=TEXT("FitRoute");}
   if(Stage>=6){if(!Cities.IsValidIndex(Stage-6))return true;const auto Id=Cities[Stage-6];Test->TestTrue(TEXT("City selection uses production intent"),Root->ActivateSemanticId(Id));Test->TestTrue(TEXT("Every city accepts focus and reveals itself"),Root->FocusSemanticId(Id));Name=Id.RightChop(14);}
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.4)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  const auto* Canvas=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Canvas");});
  Test->TestTrue(TEXT("Map retains usable height with large text and controls"),Canvas&&Canvas->Bounds.Height()>=120);

  if(Stage>=6){const auto* N=Nodes.FindByPredicate([&](const auto& Node){return Node.Id==Cities[Stage-6];});Test->TestTrue(TEXT("Selected geographic marker is reachable with visible hit bounds"),N&&N->State.bVisible&&N->State.bSelected&&N->Bounds.Width()>=40&&N->Bounds.Height()>=40);}
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real viewport capture failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/FString::Printf(TEXT("TradeWorkspace/TG03/scale-%.1f"),Scale);IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("map-%dx%d-%s"),Size.X,Size.Y,*Name);TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Write native screenshot"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bSelected,N.State.bFocused,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return Stage>=6+Cities.Num();
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;FString Name;TArray<FString> Cities;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeGeographicViewport,"Hansa.UI.TradeGeographic.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaTradeGeographicViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeGeographicCapture(this));return true;}
#endif
