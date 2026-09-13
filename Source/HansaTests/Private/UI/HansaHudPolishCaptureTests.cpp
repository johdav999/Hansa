#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaContextInspector.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaBuildingWorldProjection.h"
#include "Widgets/SViewport.h"
#include "Layout/WidgetPath.h"

namespace {
class FHudPolishCapture final:public IAutomationLatentCommand {
public:
 explicit FHudPolishCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("P23 game viewport timed out"));return true;}
  if(!GEngine || !GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W || !W->HasBegunPlay())return false;
  auto* Controller=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;
  if(!Hud || !Hud->GetRootWidget().IsValid())return false;
  if(HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();auto* Inspector=Hud->GetInspectorPresentationModel();auto* Model=Hud->GetPresentationModel();
  if(auto* Camera=Cast<AHansaStrategyCameraPawn>(Controller->GetPawn())){
   Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(FVector(-3200,-700,100));
  }
  auto Select=[&](const FString& Definition){
   for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==Definition){
    Controller->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
    Test->TestEqual(TEXT("Production selection callback resolves the actual building"),Inspector->GetSnapshot().BuildingValue,It->GetStableBuildingValue());return;
   }
   Test->AddError(FString::Printf(TEXT("Missing capture building: %s"),*Definition));
  };
  if(!Prepared){
   if(Hud->GetScenarioPresentationModel())Hud->GetScenarioPresentationModel()->Close();
   Hud->GetBuildMenuPresentationModel()->SetOpen(false);Model->SetSpeed(EHansaHudGameSpeed::Paused);
   switch(Stage){
    case 0:Inspector->CloseIntent();break;
    case 1:FSlateApplication::Get().SetKeyboardFocus(Root->ResolveSemanticWidget(TEXT("HUD.TopStatus.Speed.Fast")),EFocusCause::Navigation);
     Test->TestEqual(TEXT("Native focus is reflected in the semantic model"),Model->GetSnapshot().FocusedSemanticId,FName(TEXT("HUD.TopStatus.Speed.Fast")));
     FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
     FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
     Test->TestEqual(TEXT("Native speed button updates the production HUD model"),Model->GetSnapshot().Speed,EHansaHudGameSpeed::Fast);break;
    case 2:Select(TEXT("Building.Bakery"));Inspector->FrameIntent();break;
    case 3:Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.OpenCause"));break;
    case 4:Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.Pin"));break;
    case 5:Root->GetInspector()->RevealSemanticWidget(TEXT("Inspector.History"));break;
    case 6:Select(TEXT("Building.Residence.Laborer"));break;
    case 7:{
     auto State=Model->GetSnapshot();FHansaHudAlertPresentation Alert;Alert.StableId=TEXT("P23.CausalFixture");Alert.GroupId=TEXT("Production");
     Alert.Label=FText::FromString(TEXT("Bread production needs attention"));Alert.AffectedObject=FText::FromString(TEXT("Lübeck bakery"));Alert.Age=FText::FromString(TEXT("Current"));
     Alert.Causal=Inspector->GetSnapshot().Causal;Alert.Causal.Problem=FText::FromString(TEXT("Supply interrupted"));Alert.Causal.Cause=FText::FromString(TEXT("The selected production input is unavailable."));Alert.Causal.Remedy=FText::FromString(TEXT("Inspect the supplying chain."));Alert.Causal.Severity=EHansaCausalSeverity::Warning;Alert.bWarning=true;
     State.Alerts={Alert};State.bAlertStackExpanded=true;State.Notifications={{TEXT("P23.Notice"),FText::FromString(TEXT("Presentation fixture: input supply requires attention."))}};Model->ApplySnapshot(State);
     Test->TestTrue(TEXT("Alert opens the shared causal inspector"),Root->ActivateSemanticId(TEXT("HUD.AlertStack.Alert.P23_CausalFixture.OpenCause")));break;
    }
    case 8:Inspector->ShowStatus(EHansaInspectorDataState::Loading,FText::FromString(TEXT("Waiting for the city projection.")),FText::FromString(TEXT("The selection will update when data is ready.")),TEXT("HUD.AlertStack.Toggle"));break;
    case 9:Inspector->ShowStatus(EHansaInspectorDataState::Error,FText::FromString(TEXT("The selected object's data could not be prepared.")),FText::FromString(TEXT("Select the object again to retry.")),TEXT("HUD.AlertStack.Toggle"));break;
    case 10:Inspector->ShowStatus(EHansaInspectorDataState::Empty,FText::FromString(TEXT("No object selected.")),FText::FromString(TEXT("Choose a building in the city.")),TEXT("HUD.AlertStack.Toggle"));break;
    case 11:Root->SetPreferences({true,true,true});Select(TEXT("Building.Bakery"));Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Action.OpenCause"));break;
    case 12:{auto State=Model->GetSnapshot();State.CityBreadcrumb=FText::FromString(TEXT("Freie Hansestadt / Lübeck und umliegende Handelsniederlassungen"));Model->ApplySnapshot(State);break;}
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native viewport readback failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
  Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  const TCHAR* Names[]={TEXT("default"),TEXT("speed"),TEXT("production"),TEXT("cause"),TEXT("actions"),TEXT("history"),TEXT("residence"),TEXT("alert"),TEXT("loading"),TEXT("error"),TEXT("empty"),TEXT("accessible"),TEXT("localized")};
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P23/hud-%dx%d-%s"),X,Y,Names[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved native viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot()){
   if(!N.Id.StartsWith(TEXT("HUD.")) && !N.Id.StartsWith(TEXT("Inspector.")) && N.Id!=TEXT("BuildMenu.Root"))continue;
   Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   if(N.State.bVisible && (N.Id==TEXT("HUD.TopStatus") || N.Id==TEXT("Inspector.Root")))Test->TestTrue(TEXT("Visible edge panel fits the game viewport"),N.Bounds.Min.X>=0 && N.Bounds.Min.Y>=0 && N.Bounds.Max.X<=X && N.Bounds.Max.Y<=Y);
  }
  Test->TestTrue(TEXT("Saved semantic snapshot"),FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv"))));
  if(Stage==1){
   // RenderOffscreen windows are excluded from platform hit testing. Build the
   // real window-to-button path explicitly and route native pointer replies.
   auto Button=Root->ResolveSemanticWidget(TEXT("HUD.TopStatus.Speed.Pause"));
   auto Window=V->GetWindow();
   if(!Test->TestTrue(TEXT("Game viewport has a native window"),Window.IsValid()))return true;
   FWidgetPath Builder;
   auto Children=Builder.GeneratePathToWidget(FWidgetMatcher(Button.ToSharedRef()),FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));
   FArrangedChildren Arranged(EVisibility::Visible);
   Arranged.AddWidget(FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));
   for(int32 I=0;I<Children.Num();++I)Arranged.AddWidget(Children[I]);
   TArray<FWidgetAndPointer> PointerWidgets;
   for(int32 I=0;I<Arranged.Num();++I)PointerWidgets.Emplace(Arranged[I]);
   FWidgetPath Path{MakeArrayView(PointerWidgets)};
   if(!Test->TestTrue(TEXT("Native pointer path reaches visible button"),Path.GetLastWidget()==Button))return true;
   const auto Geometry=Button->GetCachedGeometry();
   const FVector2D P=Geometry.GetAbsolutePosition()+Geometry.GetAbsoluteSize()*.5;
   FPointerEvent Down(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
   FPointerEvent Up(0,P,P,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState());
   Button->OnMouseEnter(Geometry,Down);
   FSlateApplication::Get().RoutePointerDownEvent(Path,Down);
   FSlateApplication::Get().RoutePointerUpEvent(Path,Up);
   Button->OnMouseLeave(Up);
   Test->TestEqual(TEXT("Routed native mouse events pause through the HUD action"),Model->GetSnapshot().Speed,EHansaHudGameSpeed::Paused);
  }
  if(Stage==3){
   const auto Before=Inspector->GetSnapshot().FocusedSemanticId;
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_DPad_Down,FModifierKeysState(),0,false,0,0));
   Test->TestNotEqual(TEXT("Native controller navigation moves inspector focus"),Inspector->GetSnapshot().FocusedSemanticId,Before);
  }
  if(Stage==4){
   const bool Before=Inspector->GetSnapshot().bPinned;
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
   Test->TestNotEqual(TEXT("Native controller accept toggles the focused pin action"),Inspector->GetSnapshot().bPinned,Before);
  }
  ++Stage;Prepared=false;return Stage==13;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaHudPolishViewport,"Hansa.UI.HudPolish.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaHudPolishViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FHudPolishCapture(this));return true;}
#endif
