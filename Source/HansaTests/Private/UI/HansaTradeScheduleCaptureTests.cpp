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
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UI/HansaTradeMapPresentationModel.h"
namespace {
class FTradeScheduleCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeScheduleCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Schedule viewport launch timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!H||!H->GetRootWidget())return false;
  auto Root=H->GetRootWidget();auto* F=H->GetFrontendPresentationModel();
  if(F&&F->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(F&&F->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(H->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  if(!Prepared){
   if(Stage==0){float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,Scale>1,Scale>1,Scale});H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Test->TestTrue(TEXT("Open real trade workspace"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));Test->TestTrue(TEXT("Open directory page"),Root->ActivateSemanticId(TEXT("TradeMap.Page.Schedule")));}
   auto* Host=Cast<AHansaGameMode>(W->GetAuthGameMode())->GetSimulationHost();
   if(Stage==0){
    Test->TestTrue(TEXT("Start route receives ordinary focus"),Root->FocusSemanticId(TEXT("TradeMap.Editor.ToggleActive")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Root->ActivateSemanticId(TEXT("TradeMap.Page.Schedule"));
   }
   if(Stage==1){
    const auto RouteId=H->GetTradeMapPresentationModel()->GetSnapshot().SelectedRouteValue;
    for(int Step=0;Step<80;++Step){const auto P=Host->BuildProjection();const auto* R=P.Value.GetRoutes().FindByPredicate([&](const auto& X){return int64(X.Id.GetValue())==RouteId;});if(R&&R->LastTransfer.AppliedQuantity.GetRawValue()>0)break;Host->AdvanceTicks(1);}
   }
   if(Stage==2)Host->AdvanceTicks(2);
   const FString Id=Stage==1?TEXT("TradeMap.Schedule.Tab.Cargo"):Stage==2?TEXT("TradeMap.Schedule.Tab.Arrivals"):TEXT("TradeMap.Schedule.Tab.Stops");
   Test->TestTrue(TEXT("Schedule control receives keyboard focus"),Root->FocusSemanticId(Id));
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<1)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  Test->TestTrue(TEXT("Schedule tab remains visible"),Nodes.ContainsByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Schedule.Tab.Cargo")&&N.State.bVisible&&N.Bounds.Height()>=40;}));
  if(Stage==1)Test->TestTrue(TEXT("Schedule contains visible semantic data"),Nodes.ContainsByPredicate([](const auto& N){return N.Id.StartsWith(TEXT("TradeMap.Schedule."))&&N.State.bVisible;}));
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Schedule viewport screenshot failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG06");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("schedule-%dx%d-scale-%.1f-%d"),Size.X,Size.Y,Scale,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save native capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return Stage==3;
 }
private: FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeScheduleViewport,"Hansa.UI.TradeSchedule.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FTradeScheduleViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeScheduleCapture(this));return true;}
#endif
