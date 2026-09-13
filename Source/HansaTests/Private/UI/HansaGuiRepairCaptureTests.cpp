#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaContextInspector.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaResearchPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaBuildingWorldProjection.h"
#include "Widgets/SViewport.h"

namespace {
class FGuiRepairCapture final : public IAutomationLatentCommand {
public:
 explicit FGuiRepairCapture(FAutomationTestBase* InTest):Test(InTest),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("GUI repair capture timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!Hud||!Hud->GetRootWidget())return false;
  if(HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();const int32 Screen=Stage%9;const bool Accessible=Stage>=9;
  auto Press=[&](const TCHAR* Id){
   Test->TestTrue(FString::Printf(TEXT("Native navigation target %s"),Id),Root->FocusSemanticId(Id));
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
  };
  if(!Prepared){
   Hud->GetScenarioPresentationModel()->Close();Hud->GetResearchPresentationModel()->Close();
   Hud->GetSaveLoadPresentationModel()->Close();Hud->GetInspectorPresentationModel()->CloseIntent();
   Root->ActivateSemanticId(TEXT("CityOverview.Close"));Root->ActivateSemanticId(TEXT("TradeMap.Close"));
   Hud->GetBuildMenuPresentationModel()->SetOpen(false);Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   float UiScale=1.f;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),UiScale);
   if(Screen==0)Root->SetPreferences({Accessible,Accessible,Accessible,UiScale});
   if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();}
   switch(Screen){
    case 0:break;
    case 1:
     for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==TEXT("Building.Bakery")){
      C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());Hud->GetInspectorPresentationModel()->FrameIntent();break;
     }
     Test->TestTrue(TEXT("Real bakery inspector opened"),Hud->GetInspectorPresentationModel()->GetSnapshot().bOpen);
     if(Hud->GetInspectorPresentationModel()->GetSnapshot().Causal.Severity==EHansaCausalSeverity::None)
      Test->TestFalse(TEXT("Healthy world building keeps diagnostics collapsed"),Hud->GetInspectorPresentationModel()->GetSnapshot().bCauseExpanded);
     break;
    case 2:Test->TestTrue(TEXT("Construction opens through existing category action"),Root->ActivateSemanticId(TEXT("BuildMenu.Category.Production")));break;
    case 3:Press(TEXT("HUD.TopStatus.CityOverview"));Root->ActivateSemanticId(TEXT("CityOverview.Tab.Population"));break;
    case 4:Press(TEXT("HUD.TopStatus.CityOverview"));Root->ActivateSemanticId(TEXT("CityOverview.Tab.Market"));Root->ActivateSemanticId(TEXT("CityOverview.Market.Details"));Root->ActivateSemanticId(TEXT("Market.Row.Good_Bread"));break;
    case 5:Press(TEXT("HUD.TopStatus.TradeMap"));break;
    case 6:Press(TEXT("HUD.TopStatus.Research.Open"));break;
    case 7:Hud->GetScenarioPresentationModel()->Open();break;
    case 8:Press(TEXT("HUD.TopStatus.SaveLoad"));break;
   }
   FSlateApplication::Get().SetCursorPos(FVector2D(2,2));FSlateApplication::Get().CloseToolTip();
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.8)return false;
  const TCHAR* Names[]={TEXT("hud"),TEXT("inspector"),TEXT("construction"),TEXT("city"),TEXT("market"),TEXT("trade"),TEXT("research"),TEXT("scenario"),TEXT("saveload")};
  const TCHAR* Roots[]={TEXT("HUD.Root"),TEXT("Inspector.Root"),TEXT("BuildMenu.Root"),TEXT("CityOverview.Root"),TEXT("Market.Root"),TEXT("TradeMap.Root"),TEXT("Research.Root"),TEXT("Scenario.Root"),TEXT("SaveLoad.Root")};
  auto Nodes=Root->GetSemanticSnapshot();
  const auto* ScreenNode=Nodes.FindByPredicate([&](const auto& N){return N.Id==Roots[Screen];});
  Test->TestTrue(FString::Printf(TEXT("%s reached in actual game"),Names[Screen]),ScreenNode&&ScreenNode->State.bVisible);
  if(Screen>=5){
   const FString Prefix=Screen==5?TEXT("TradeMap.Route."):Screen==6?TEXT("Research.Node."):Screen==7?TEXT("Scenario.Path."):TEXT("SaveLoad.Slot.");
   const auto* Target=Nodes.FindByPredicate([&](const auto& N){return N.Id.StartsWith(Prefix)&&N.bCanFocus&&N.State.bEnabled;});
   if(Target){
    Test->TestTrue(TEXT("Secondary screen accepts semantic focus"),Root->FocusSemanticId(Target->Id));
    const auto Widget=Root->ResolveSemanticWidget(Target->Id);
    Test->TestTrue(TEXT("Focus belongs to the current live native control"),Widget.IsValid()&&FSlateApplication::Get().GetKeyboardFocusedWidget()==Widget);
   }
  }
  // Verify internal native geometry, not merely semantic visibility or screenshot existence.
  const auto ViewGeometry=V->GetGameViewportWidget()->GetCachedGeometry();
  auto CheckArea=[&](const TCHAR* Id,float MinimumHeight){
   auto Widget=Root->ResolveSemanticWidget(Id);
   Test->TestTrue(FString::Printf(TEXT("Live geometry target %s"),Id),Widget.IsValid());
   if(Widget){auto G=Widget->GetCachedGeometry();auto Pos=ViewGeometry.AbsoluteToLocal(G.GetAbsolutePosition());auto Extent=G.GetAbsoluteSize()/ViewGeometry.Scale;
    Test->TestTrue(FString::Printf(TEXT("Usable content height for %s: %.1f >= %.1f"),Id,Extent.Y,MinimumHeight),Extent.Y>=MinimumHeight);
    Test->TestTrue(FString::Printf(TEXT("Native content remains within viewport: %s"),Id),Pos.X>=-1&&Pos.Y>=-1&&Pos.X+Extent.X<=ViewGeometry.GetLocalSize().X+1&&Pos.Y+Extent.Y<=ViewGeometry.GetLocalSize().Y+1);
   }
  };
  if(Screen==0)for(const TCHAR* Id:{TEXT("HUD.TopStatus.CityOverview"),TEXT("HUD.TopStatus.Research.Open"),TEXT("HUD.TopStatus.SaveLoad"),TEXT("HUD.TopStatus.TradeMap"),TEXT("HUD.TopStatus.Speed.Pause"),TEXT("HUD.TopStatus.Speed.Normal"),TEXT("HUD.TopStatus.Speed.Fast"),TEXT("HUD.TopStatus.Speed.Fastest")})CheckArea(Id,47.5f);
  if(Screen==3)CheckArea(TEXT("CityOverview.List"),80);
  if(Screen==4)CheckArea(TEXT("Market.List"),80);
  if(Screen==7)CheckArea(TEXT("Scenario.Content"),100);
  if(Screen==8){CheckArea(TEXT("SaveLoad.Content"),60);CheckArea(TEXT("SaveLoad.Action.Save"),47.5f);CheckArea(TEXT("SaveLoad.Action.Load"),47.5f);}
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Game viewport screenshot failed"));return true;}
  int32 X=1920,Y=1080;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
  Test->TestEqual(TEXT("Native screenshot width"),Size.X,X);Test->TestEqual(TEXT("Native screenshot height"),Size.Y,Y);
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("GuiRepair/Native");IFileManager::Get().MakeDirectory(*Folder,true);
  const FString Base=Folder/FString::Printf(TEXT("gui-%dx%d-scale%d-%s-%s"),X,Y,FMath::RoundToInt(Root->GetPreferences().UiScale*100),Accessible?TEXT("accessible"):TEXT("default"),Names[Screen]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
  Test->TestTrue(TEXT("Saved original native pixels"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Nodes)Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  Test->TestTrue(TEXT("Saved semantic evidence"),FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv"))));
  ++Stage;Prepared=false;return Stage==18;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaGuiRepairViewport,"Hansa.UI.GuiRepair.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaGuiRepairViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FGuiRepairCapture(this));return true;}
#endif
