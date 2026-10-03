#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaVisitingTradeTestSupport.h"
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
class FVisitingCapture final : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Start=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false;FString Focus;
public:
 explicit FVisitingCapture(FAutomationTestBase* T):Test(T){}
 bool Update() override{
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Visiting trade viewport timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){if(!Root->ActivateSemanticId(TEXT("Frontend.NewGame"))){Test->AddError(TEXT("Production New Game failed before visiting-trade setup; inspect runtime catalog diagnostics."));return true;}return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* M=Hud->GetMarketTablePresentationModel();auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;if(!Host)return false;
  auto Act=[&](const FString& S){return Root->ActivateSemanticId(TEXT("Market.Detail.SpotTrade.")+S);};
  auto Key=[&](FKey K){FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(K,FModifierKeysState(),0,false,0,0));};
  if(!Prepared){
   if(Stage==0){FString Error;if(!Hansa::Tests::PrepareVisitingTrade(*Host,Error)){Test->AddError(Error);return true;}
    float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});
    Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap"));Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock"));Test->TestTrue(TEXT("Map opens selected city market"),Root->ActivateSemanticId(TEXT("TradeMap.Overview.Market")));M->SelectGoodIntent(TEXT("Good.Timber"));Focus=TEXT("Ship");
   }
   if(Stage==1){Key(EKeys::Enter);Test->TestEqual(TEXT("Keyboard chooses owned Cog"),M->GetSnapshot().SelectedGood.SpotTradeVehicleValue,int64(1));Focus=TEXT("Confirm");}
   if(Stage==2){Key(EKeys::Gamepad_FaceButton_Bottom);Test->TestTrue(TEXT("Controller purchase has receipt"),M->GetSnapshot().SelectedGood.SpotTradeResult.ToString().Contains(TEXT("Executed receipt")));Focus=TEXT("Side");}
   if(Stage==3){Key(EKeys::Enter);M->AdjustSpotTradeQuantityIntent(5000);Focus=TEXT("Confirm");}
   if(Stage==4){Key(EKeys::Gamepad_FaceButton_Bottom);Test->TestTrue(TEXT("Controller partial sale has reason"),M->GetSnapshot().SelectedGood.SpotTradeResult.ToString().Contains(TEXT("ship stock limited")));Focus=TEXT("Manifest");}
   if(Stage==5){Key(EKeys::Enter);Test->TestTrue(TEXT("Manifest visible"),M->GetSnapshot().SelectedGood.SpotTradeResult.ToString().Contains(TEXT("manifest")));Focus=TEXT("Ship");}
   if(Stage==6){
    const auto Table=Root->GetCityOverview()->GetMarketTable();auto Button=Table->ResolveSemanticWidget(TEXT("Market.Detail.SpotTrade.Ship"));auto Window=V->GetWindow();
    FWidgetPath Builder;auto Children=Builder.GeneratePathToWidget(FWidgetMatcher(Button.ToSharedRef()),FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));TArray<FWidgetAndPointer> Widgets;Widgets.Emplace(FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));for(int32 I=0;I<Children.Num();++I)Widgets.Emplace(Children[I]);FWidgetPath Path{MakeArrayView(Widgets)};
    const auto G=Button->GetCachedGeometry();const auto Pos=G.GetAbsolutePosition()+G.GetAbsoluteSize()*.5;FPointerEvent Down(0,Pos,Pos,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()),Up(0,Pos,Pos,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());Button->OnMouseEnter(G,Down);FSlateApplication::Get().RoutePointerDownEvent(Path,Down);FSlateApplication::Get().RoutePointerUpEvent(Path,Up);Button->OnMouseLeave(Up);Test->TestEqual(TEXT("Native mouse chooses second Cog"),M->GetSnapshot().SelectedGood.SpotTradeVehicleValue,int64(2));Focus=TEXT("Confirm");
   }
   if(Stage==7){Key(EKeys::Gamepad_FaceButton_Bottom);Test->TestFalse(TEXT("Second ship submission provides feedback"),M->GetSnapshot().SelectedGood.SpotTradeResult.IsEmpty());Focus=TEXT("Receipt");}
   Test->TestTrue(TEXT("Control focus is reachable"),Root->FocusSemanticId(TEXT("Market.Detail.SpotTrade.")+Focus));Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<.15)return false;
  const auto Nodes=Root->GetSemanticSnapshot();const auto* N=Nodes.FindByPredicate([&](const auto& X){return X.Id==TEXT("Market.Detail.SpotTrade.")+Focus;});
  Test->TestTrue(TEXT("Focused comparison control is visibly revealed"),N&&N->State.bVisible&&N->Bounds.Height()>=40);
  if(Stage<8){
   TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return true;}
   float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG15");IFileManager::Get().MakeDirectory(*Dir,true);
   FString Culture;FParse::Value(FCommandLine::Get(),TEXT("culture="),Culture);if(FParse::Param(FCommandLine::Get(),TEXT("HansaGuiPseudoLocale"))&&Stage>=6)Culture=TEXT("LEET");
   const FString Base=Dir/FString::Printf(TEXT("visiting-%dx%d-scale%.1f-%02d%s"),Size.X,Size.Y,Scale,Stage,Culture.IsEmpty()?TEXT(""):*FString(TEXT("-")+Culture));
   TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
   FString Evidence=FString::Printf(TEXT("# stage=%d revision=%lld focus=%s packaged=%d\n"),Stage,int64(M->GetRevision()),*Focus,FPlatformProperties::RequiresCookedData());
   for(const auto& X:Nodes)if(X.Id.StartsWith(TEXT("Market.Detail.SpotTrade.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*X.Id,X.State.bVisible,X.State.bFocused,X.Bounds.Min.X,X.Bounds.Min.Y,X.Bounds.Max.X,X.Bounds.Max.Y,*X.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  }
  ++Stage;Prepared=false;if(Stage>=8&&FParse::Param(FCommandLine::Get(),TEXT("HansaGuiPseudoLocale")))FInternationalization::Get().SetCurrentCulture(TEXT("en"));return Stage>=8;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisitingTradeViewport,"Hansa.UI.VisitingTrade.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FVisitingTradeViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FVisitingCapture(this));return true;}
#endif
