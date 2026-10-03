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
class FCityOverviewReferenceCapture : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false,Initialized=false;
public:
 explicit FCityOverviewReferenceCapture(FAutomationTestBase* T):Test(T){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>120){Test->AddError(TEXT("City overview reference capture timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* World=V->GetWorld();if(!World||!World->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Model=Hud->GetTradeMapPresentationModel();
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  if(!Initialized){
   Root->SetPreferences({Scale>1,true,Scale>1,Scale});Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   auto* Mode=Cast<AHansaGameMode>(World->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;
   if(Host)Host->AdvanceTicks(1); // Time only: ordinary authoritative report ageing, no stock/price injection.
   Test->TestTrue(TEXT("Open ordinary trade map"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));
   Test->TestTrue(TEXT("Select Rostock"),Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock")));
   Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Overview"));Root->ActivateSemanticId(TEXT("TradeMap.Page.Workspace"));Initialized=true;
  }
  auto Key=[](FKey K){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
  if(!Prepared){
   if(Stage==1){Test->TestTrue(TEXT("More supports focus"),Root->FocusSemanticId(TEXT("TradeMap.City.More")));Key(EKeys::Enter);}
   if(Stage==2){Test->TestTrue(TEXT("Orders menu entry can be focused"),Root->FocusSemanticId(TEXT("TradeMap.Navigate.Orders")));Key(EKeys::Enter);}
   if(Stage==3){Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Overview"));Test->TestTrue(TEXT("Footer focus"),Root->FocusSemanticId(TEXT("TradeMap.Overview.Presence")));for(int32 Page=0;Page<12;++Page)Key(EKeys::PageDown);}
   if(Stage==4){Test->TestTrue(TEXT("Presence action focus"),Root->FocusSemanticId(TEXT("TradeMap.Overview.Presence")));Key(EKeys::Gamepad_FaceButton_Bottom);}
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  auto Find=[&](const TCHAR* Id){return Nodes.FindByPredicate([&](const auto& N){return N.Id==Id;});};
  if(Stage==0){
   const auto* Scroll=Find(TEXT("TradeMap.Overview.Scroll"));
   Test->TestTrue(TEXT("Overview retains a usable scroll viewport"),Scroll&&Scroll->Bounds.Height()>=100);
   if(Scale<=1&&V->Viewport->GetSizeXY().X>=1920){
    const auto* Rights=Find(TEXT("TradeMap.Overview.ConstructionStatus"));const auto* Footer=Find(TEXT("TradeMap.Overview.Market"));
    Test->TestTrue(TEXT("Normal overview exposes construction rights above fixed footer"),Rights&&Footer&&Scroll&&Rights->Bounds.Max.Y<=Scroll->Bounds.Max.Y-4*Scale&&Rights->Bounds.Height()>=FMath::FloorToInt(20*Scale));
   }
  }
  if(Stage==1){const auto* N=Find(TEXT("TradeMap.Navigate.Orders"));Test->TestTrue(TEXT("More exposes visible secondary navigation"),N&&N->State.bVisible&&N->Bounds.Height()>=40);}
  if(Stage==2)Test->TestEqual(TEXT("Keyboard opens Orders"),Model->GetSnapshot().ActiveSection,FString(TEXT("Orders")));
  if(Stage==3){const auto* Rights=Find(TEXT("TradeMap.Overview.ConstructionStatus"));Test->TestTrue(TEXT("Keyboard scrolling reaches construction status"),Rights&&Rights->Bounds.Height()>=FMath::FloorToInt(20*Scale));}
  if(Stage==4)Test->TestEqual(TEXT("Controller opens Presence"),Model->GetSnapshot().ActiveSection,FString(TEXT("Presence")));
  if(Stage==0||Stage==3)for(const TCHAR* Id:{TEXT("TradeMap.Overview.Market"),TEXT("TradeMap.Overview.Presence")}){
   const auto* N=Find(Id);Test->TestTrue(TEXT("Footer action remains visible and accessible"),N&&N->State.bVisible&&N->Bounds.Height()>=FMath::FloorToInt(40*Scale));
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real screenshot failed"));return true;}
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("CityOverviewReference");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("overview-%dx%d-scale%.1f-stage%d"),Size.X,Size.Y,Scale,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence=FString::Printf(TEXT("# city=%s revision=%llu viewport=%dx%d scale=%.1f state=%s normal-new-game\n"),*Model->GetSnapshot().SelectedCityStableId.ToString(),Model->GetRevision(),Size.X,Size.Y,Scale,*Model->GetSnapshot().ActiveSection);
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bFocused,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage>=5;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOverviewReferenceViewport,"Hansa.UI.CityOverviewReference.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FCityOverviewReferenceViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FCityOverviewReferenceCapture(this));return true;}
#endif
