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
class FTradeReferenceCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeReferenceCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
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
    Test->TestTrue(TEXT("Choose source stop through route editor"),Root->ActivateSemanticId(TEXT("TradeMap.Stop.1")));
    for(int I=0;I<6;++I)Test->TestTrue(TEXT("Adjust source reserve through ordinary editor intent"),Root->ActivateSemanticId(TEXT("TradeMap.Editor.Reserve.Decrease")));
    Test->TestTrue(TEXT("Save reviewed route reserve"),Root->ActivateSemanticId(TEXT("TradeMap.Editor.Save")));
    Test->TestTrue(TEXT("Start route receives ordinary focus"),Root->FocusSemanticId(TEXT("TradeMap.Editor.ToggleActive")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Root->ActivateSemanticId(TEXT("TradeMap.Page.Schedule"));
   }
   if(Stage==0){
    const auto RouteId=H->GetTradeMapPresentationModel()->GetSnapshot().SelectedRouteValue;
    for(int Step=0;Step<80;++Step){const auto P=Host->BuildProjection();const auto* R=P.Value.GetRoutes().FindByPredicate([&](const auto& X){return int64(X.Id.GetValue())==RouteId;});if(R&&R->LastTransfer.AppliedQuantity.GetRawValue()>0)break;Host->AdvanceTicks(1);}
   }
   if(Stage==2){
    auto* M=H->GetTradeMapPresentationModel();
    const auto Rival=M->GetSnapshot().Directory.FindByPredicate([](const auto& E){return !E.bOwned;});
    Test->TestNotNull(TEXT("Rival route available"),Rival);
    if(Rival){const int64 RivalId=Rival->RouteValue;Test->TestTrue(TEXT("Select rival through normal intent"),M->SelectRouteIntent(RivalId));}
   }
   if(Stage==3){
    Test->TestTrue(TEXT("Private manifest recovery receives focus"),Root->FocusSemanticId(TEXT("TradeMap.Schedule.ChooseOwned")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Test->TestFalse(TEXT("Recovery selects authorized cargo"),H->GetTradeMapPresentationModel()->GetSchedulePresentation().Rows.IsEmpty());
    Test->TestTrue(TEXT("Directory menu receives controller focus"),Root->FocusSemanticId(TEXT("TradeMap.Directory.More")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    Ready=FPlatformTime::Seconds();Prepared=true;return false;
   }
   if(Stage==4){
    Test->TestTrue(TEXT("Map tools receive focus"),Root->FocusSemanticId(TEXT("TradeMap.Chart.Tools")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Ready=FPlatformTime::Seconds();Prepared=true;return false;
   }
   if(Stage==5||Stage==6){
    if(Stage==6){auto* M=H->GetTradeMapPresentationModel();const auto* Rival=M->GetSnapshot().Directory.FindByPredicate([](const auto& E){return !E.bOwned;});if(Rival){const int64 Id=Rival->RouteValue;Test->TestTrue(TEXT("Select rival for compact manifest"),M->SelectRouteIntent(Id));}}
    Test->TestTrue(TEXT("Open compact manifest page"),Root->ActivateSemanticId(TEXT("TradeMap.Page.Schedule")));
    Test->TestTrue(TEXT("Manifest tab focus reveals compact content"),Root->FocusSemanticId(TEXT("TradeMap.Schedule.Tab.Cargo")));
    Test->TestEqual(TEXT("Manifest privacy remains correct after page changes"),H->GetTradeMapPresentationModel()->GetSchedulePresentation().Rows.IsEmpty(),Stage==6);
    Ready=FPlatformTime::Seconds();Prepared=true;return false;
   }
   if(Stage==7){
    Test->TestTrue(TEXT("Recover owned route for inspector scroll"),Root->ActivateSemanticId(TEXT("TradeMap.Schedule.ChooseOwned")));
    Test->TestTrue(TEXT("Port detail focus reveals lower overview content"),Root->FocusSemanticId(TEXT("TradeMap.Overview.Presence")));
    Ready=FPlatformTime::Seconds();Prepared=true;return false;
   }
   if(Stage==8){
    Test->TestTrue(TEXT("Open Presence for ordinary pointer scrolling"),Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence")));
    Ready=FPlatformTime::Seconds();Prepared=true;return false;
   }
   if(Stage==9){
    Test->TestTrue(TEXT("Open journey receives focus"),Root->FocusSemanticId(TEXT("TradeMap.Schedule.OpenJourney")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Open journey selects existing route editor"),H->GetTradeMapPresentationModel()->GetSnapshot().ActiveSection,FString(TEXT("Route")));
    Ready=FPlatformTime::Seconds();Prepared=true;return false;
   }
   const FString Id=TEXT("TradeMap.Schedule.Tab.Cargo");
   Test->TestTrue(TEXT("Schedule control receives keyboard focus"),Root->FocusSemanticId(Id));
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   Root->ActivateSemanticId(Stage==0?TEXT("TradeMap.Navigate.Overview"):TEXT("TradeMap.Navigate.Presence"));
   if(Stage==2)Test->TestTrue(TEXT("Rival schedule private"),H->GetTradeMapPresentationModel()->GetSchedulePresentation().Rows.IsEmpty());
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()-Ready<1)return false;
  const auto Nodes=Root->GetSemanticSnapshot();
  if(Stage==8&&!Scrolled){
   const auto* Action=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Station.Action");});
   if(Action){
    const FVector2D Position=V->GetGameViewportWidget()->GetCachedGeometry().LocalToAbsolute(FVector2D((Action->Bounds.Min.X+Action->Bounds.Max.X)*.5,FMath::Min((Action->Bounds.Min.Y+Action->Bounds.Max.Y)*.5,double(V->Viewport->GetSizeXY().Y-100))));
    FPointerEvent Move(0,Position,Position,TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());
    FSlateApplication::Get().ProcessMouseMoveEvent(Move,true);
    FPointerEvent Wheel(0,Position,Position,TSet<FKey>(),EKeys::Invalid,-20,FModifierKeysState());
    Test->TestTrue(TEXT("Presence handles ordinary pointer wheel"),FSlateApplication::Get().ProcessMouseWheelOrGestureEvent(Wheel,nullptr));
   }else Test->AddError(TEXT("Missing station action for pointer target"));
   Scrolled=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(Stage==8)Test->TestTrue(TEXT("Pointer scrolling exposes requirement rows"),Nodes.ContainsByPredicate([](const auto& N){return N.Id.StartsWith(TEXT("TradeMap.Presence.Requirement."))&&N.State.bVisible;}));
  if(Stage<3)Test->TestTrue(TEXT("Active contextual tab remains visible"),Nodes.ContainsByPredicate([&](const auto& N){return N.Id==(Stage==0?TEXT("TradeMap.Navigate.Overview"):TEXT("TradeMap.Navigate.Presence"))&&N.State.bVisible;}));
  
  if(Stage==3){
   Test->TestTrue(TEXT("Directory popup exposes filters semantically"),Nodes.ContainsByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.City.Filter")&&N.State.bVisible&&N.bCanFocus;}));
   Test->TestTrue(TEXT("Directory filter receives focus inside popup"),Root->FocusSemanticId(TEXT("TradeMap.City.Filter")));
   FSlateApplication::Get().DismissAllMenus();
  }
  if(Stage==4){
   Test->TestTrue(TEXT("Map popup exposes line control"),Nodes.ContainsByPredicate([](const auto& N){return N.Id==TEXT("TradeMap.Chart.Thickness")&&N.State.bVisible&&N.bCanFocus;}));
   Test->TestTrue(TEXT("Map layer control receives focus inside popup"),Root->FocusSemanticId(TEXT("TradeMap.Chart.Thickness")));
   FSlateApplication::Get().DismissAllMenus();
  }
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Schedule viewport screenshot failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/LabelsManifestV2");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("trade-%dx%d-scale-%.1f-%d"),Size.X,Size.Y,Scale,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save native capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return Stage==10;
 }
private: FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false,Scrolled=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeReferenceViewport,"Hansa.UI.TradeReference.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FTradeReferenceViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeReferenceCapture(this));return true;}
#endif

