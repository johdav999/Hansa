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
class FTradeOrdersCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeOrdersCapture(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Station orders viewport timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* Viewport=GEngine->GameViewport.Get();auto* World=Viewport->GetWorld();if(!World||!World->HasBegunPlay())return false;
  auto* Controller=Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController());
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Mode=Cast<AHansaGameMode>(World->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;
  if(!Host)return false;
  auto* Model=Hud->GetTradeMapPresentationModel();
  if(!Prepared){
   if(Stage==0){
    FString Error;if(!Hansa::Tests::PrepareLedger(*Host,Error,Hansa::Simulation::EHansaTradeStationOperationalState::Active,false,true)){Test->AddError(Error);return true;}
    float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
    Root->SetPreferences({Scale>1,true,Scale>1,Scale});
    Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
    Test->TestTrue(TEXT("Open real trade workspace"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));
    Test->TestTrue(TEXT("Select station city"),Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock")));
    Test->TestTrue(TEXT("Open station orders"),Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Orders")));
    Test->TestTrue(TEXT("Create order remains reachable in scrollable list"),Root->FocusSemanticId(TEXT("TradeMap.Orders.New")));
    Focus=TEXT("TradeMap.Orders.Row.1");
   } else if(Stage==1) {
    Test->TestTrue(TEXT("Select first live order"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.Row.1")));
    Focus=TEXT("TradeMap.Orders.Back");
   } else if(Stage==2) {
    Focus=TEXT("TradeMap.Orders.Target.Value");
   } else if(Stage==3) {
    Test->TestTrue(TEXT("Start native new-order editor"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.Back")));
    Test->TestTrue(TEXT("Open new order"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.New")));
    Test->TestTrue(TEXT("Open the native commodity menu"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.Good")));
    Test->TestTrue(TEXT("Commodity is reachable through controller focus"),Root->FocusSemanticId(TEXT("TradeMap.Orders.Choice.Good.Charcoal")));
    Test->TestTrue(TEXT("Select reference charcoal through the native menu intent"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.Choice.Good.Charcoal")));
    Test->TestTrue(TEXT("Select Buy explicitly"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.Buy")));
    Focus=TEXT("TradeMap.Orders.Budget.Value");
   } else {
    Test->TestTrue(TEXT("Select Sell explicitly"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.Sell")));
    Focus=TEXT("TradeMap.Orders.Save");
   }
   Test->TestTrue(TEXT("Focus order control"),Root->FocusSemanticId(Focus));
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.6)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  const auto* List=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Orders.List");});
  Test->TestTrue(TEXT("Native order list present with authorized values"),List&&!List->State.Value.IsEmpty()&&(Stage==0?List->State.bVisible:!List->State.bVisible));
  if(Stage==0)Test->TestTrue(TEXT("Illustrated native order rows are selectable and semantic"),Nodes.ContainsByPredicate([](const auto& N){return N.Id.StartsWith(TEXT("TradeMap.Orders.Row."))&&N.State.bVisible&&N.Bounds.Height()>=40&&N.State.Value.Contains(TEXT("target"));}));
  if(Stage>=1&&Stage<=2)for(const TCHAR* Action:{TEXT("TradeMap.Orders.Save"),TEXT("TradeMap.Orders.Pause"),TEXT("TradeMap.Orders.Back"),TEXT("TradeMap.Orders.Cancel")}){
   const auto* Button=Nodes.FindByPredicate([&](const auto& N){return N.Id==Action;});
   Test->TestTrue(*FString::Printf(TEXT("%s remains visible without form scrolling"),Action),Button&&Button->State.bVisible&&Button->Bounds.Height()>=40&&Viewport->Viewport&&Button->Bounds.Max.Y<=Viewport->Viewport->GetSizeXY().Y);
  }
  if(Stage>=3){
   for(const TCHAR* Action:{TEXT("TradeMap.Orders.Pause"),TEXT("TradeMap.Orders.Cancel")}){
    const auto* N=Nodes.FindByPredicate([&](const auto& X){return X.Id==Action;});Test->TestTrue(TEXT("New draft hides lifecycle actions"),N&&!N->State.bVisible);
   }
   Test->TestEqual(TEXT("Draft direction follows selected native tab"),Model->GetStationOrderEditor().bBuy,Stage==3);
   const auto* Budget=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Orders.Budget.Value");});
   Test->TestTrue(TEXT("Purchase budget is scoped to buying"),Budget&&Budget->State.bVisible==(Stage==3));
  }
  const auto* Control=Nodes.FindByPredicate([&](const auto& N){return N.Id==Focus;});
  Test->TestTrue(TEXT("Focused order control remains reachable"),Control&&Control->State.bVisible&&Control->Bounds.Height()>=40);
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(Viewport->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native viewport capture failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG10");
  IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("orders-%dx%d-scale%.1f-%s"),Size.X,Size.Y,Scale,Stage==0?TEXT("list"):Stage==1?TEXT("editor"):Stage==2?TEXT("editor-focused"):Stage==3?TEXT("new-buy"):TEXT("new-sell"));
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
  Test->TestTrue(TEXT("Save actual game viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("# selectedCity=%s revision=%llu station=%lld viewport=%dx%d scale=%.1f fixture=TG09-saved-campaign\n"),*Model->GetSnapshot().SelectedCityStableId.ToString(),Model->GetRevision(),Model->GetSnapshot().TradeStationValue,Size.X,Size.Y,Scale);
  const auto Projection=Host->BuildProjection();
  if(Projection)Evidence+=FString::Printf(TEXT("# tick=%lld fingerprint=%llu\n"),Projection.Value.GetClock().GetTick().GetValue(),Projection.Value.GetFingerprint().Value);
  for(const auto& Node:Nodes)if(Node.Id.StartsWith(TEXT("TradeMap.")))
   Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*Node.Id,Node.State.bVisible,Node.State.bFocused,Node.Bounds.Min.X,Node.Bounds.Min.Y,Node.Bounds.Max.X,Node.Bounds.Max.Y,*Node.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage>=5;
 }
private:
 FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;FString Focus;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeOrdersViewport,"Hansa.UI.TradeOrders.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaTradeOrdersViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeOrdersCapture(this));return true;}
#endif

