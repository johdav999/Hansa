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
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/SHansaCityOverview.h"
#include "UI/SHansaMarketTable.h"
#include "Layout/WidgetPath.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Internationalization/Internationalization.h"
namespace {
class FRecoveryCapture final : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false;FString Focus;
public:
 explicit FRecoveryCapture(FAutomationTestBase* T):Test(T){}
 bool Update() override{
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Recovery viewport timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){if(!Root->ActivateSemanticId(TEXT("Frontend.NewGame"))){Test->AddError(TEXT("Production New Game failed before recovery setup; inspect runtime catalog diagnostics."));return true;}return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* M=Hud->GetMarketTablePresentationModel();auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;if(!Host)return false;
  auto* Trade=Hud->GetTradeMapPresentationModel();
  auto Act=[&](const FString& A){return Root->ActivateSemanticId(TEXT("TradeMap.Recovery.")+A);};
  auto Key=[&](FKey K){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
  if(!Prepared){
   if(Stage==0){FString Error;if(!Hansa::Tests::PrepareLedger(*Host,Error,Hansa::Simulation::EHansaTradeStationOperationalState::Active,true)){Test->AddError(Error);return true;}
    float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
    Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap"));Trade->OpenRecovery(TEXT("City.Rostock"),100,TEXT("HUD.TopStatus.TradeMap"));Focus=TEXT("Review");
   }
   if(Stage==1){Key(EKeys::Enter);Test->TestTrue(TEXT("Keyboard enters closure review"),Trade->GetSnapshot().bRecoveryReview);Focus=TEXT("Confirm");}
   if(Stage==2){Key(EKeys::Gamepad_FaceButton_Bottom);Test->TestTrue(TEXT("Controller closure preserves outbound recovery"),Trade->GetSnapshot().Recovery.Status.Contains(TEXT("outbound")));Focus=TEXT("Item.Stock.Good.Timber");}
   if(Stage==3){Key(EKeys::Enter);Focus=TEXT("Detail");}
   if(Stage==4){Test->TestTrue(TEXT("Select exact route dependency"),Act(TEXT("Item.Route.100")));Focus=TEXT("Inspect");}
   if(Stage==5){
    auto Button=Root->ResolveSemanticWidget(TEXT("TradeMap.Recovery.Inspect"));auto Window=V->GetWindow();FWidgetPath Builder;auto Children=Builder.GeneratePathToWidget(FWidgetMatcher(Button.ToSharedRef()),FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));TArray<FWidgetAndPointer> Widgets;Widgets.Emplace(FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));for(int32 I=0;I<Children.Num();++I)Widgets.Emplace(Children[I]);FWidgetPath Path{MakeArrayView(Widgets)};
    const auto G=Button->GetCachedGeometry();const auto Pos=G.GetAbsolutePosition()+G.GetAbsoluteSize()*.5;FPointerEvent Down(0,Pos,Pos,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()),Up(0,Pos,Pos,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());Button->OnMouseEnter(G,Down);FSlateApplication::Get().RoutePointerDownEvent(Path,Down);FSlateApplication::Get().RoutePointerUpEvent(Path,Up);Button->OnMouseLeave(Up);
Test->TestEqual(TEXT("Recovery opens correct route"),Trade->GetSnapshot().SelectedRouteValue,int64(100));Trade->SelectSectionIntent(TEXT("Recovery"));Focus=TEXT("Back");}
   if(Stage==6){Root->ActivateSemanticId(TEXT("TradeMap.Close"));Test->TestTrue(TEXT("Ordinary alert opens exact recovery station"),Root->ActivateSemanticId(TEXT("HUD.AlertStack.Alert.Recovery_100.OpenCause")));Test->TestEqual(TEXT("Alert station identity"),Trade->GetSnapshot().Recovery.Station,int64(100));Focus=TEXT("Back");}
   Test->TestTrue(TEXT("Recovery focus reachable"),Root->FocusSemanticId(TEXT("TradeMap.Recovery.")+Focus));Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.15)return false;
  const auto Nodes=Root->GetSemanticSnapshot();const auto* N=Nodes.FindByPredicate([&](const auto& X){return X.Id==TEXT("TradeMap.Recovery.")+Focus;});
  Test->TestTrue(TEXT("Focused comparison control is visibly revealed"),N&&N->State.bVisible&&N->Bounds.Height()>=40);
  if(Stage<7){
   TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return true;}
   float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG16");IFileManager::Get().MakeDirectory(*Dir,true);
   FString Culture;FParse::Value(FCommandLine::Get(),TEXT("culture="),Culture);if(FParse::Param(FCommandLine::Get(),TEXT("HansaGuiPseudoLocale"))&&Stage>=6)Culture=TEXT("LEET");
   const FString Base=Dir/FString::Printf(TEXT("recovery-%dx%d-scale%.1f-%02d%s"),Size.X,Size.Y,Scale,Stage,Culture.IsEmpty()?TEXT(""):*FString(TEXT("-")+Culture));
   TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
   FString Evidence=FString::Printf(TEXT("# stage=%d revision=%lld focus=%s packaged=%d\n"),Stage,int64(Trade->GetRevision()),*Focus,FPlatformProperties::RequiresCookedData());
   for(const auto& X:Nodes)if(X.Id.StartsWith(TEXT("TradeMap.Recovery.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*X.Id,X.State.bVisible,X.State.bFocused,X.Bounds.Min.X,X.Bounds.Min.Y,X.Bounds.Max.X,X.Bounds.Max.Y,*X.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  }
  ++Stage;Prepared=false;if(Stage>=6&&FParse::Param(FCommandLine::Get(),TEXT("HansaGuiPseudoLocale")))FInternationalization::Get().SetCurrentCulture(TEXT("en"));return Stage>=7;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryViewport,"Hansa.UI.Recovery.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FRecoveryViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FRecoveryCapture(this));return true;}
#endif
