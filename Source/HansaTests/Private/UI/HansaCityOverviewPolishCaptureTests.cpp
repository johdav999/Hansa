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
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaCityOverview.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "Widgets/SViewport.h"
#include "Layout/WidgetPath.h"

namespace {
class FCityOverviewCapture final:public IAutomationLatentCommand {
public:
 explicit FCityOverviewCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("P24 viewport timed out"));return true;}
  if(!GEngine || !GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W || !W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!Hud || !Hud->GetRootWidget().IsValid())return false;
  if(HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();auto Screen=Root->GetCityOverview();auto* Model=Hud->GetCityOverviewPresentationModel();
  auto Click=[&](const FString& Id){
   auto Button=Screen->ResolveSemanticWidget(Id);auto Window=V->GetWindow();if(!Button || !Window){Test->AddError(TEXT("Native mouse target unavailable"));return;}
   FWidgetPath Builder;auto Children=Builder.GeneratePathToWidget(FWidgetMatcher(Button.ToSharedRef()),FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));
   TArray<FWidgetAndPointer> Widgets;Widgets.Emplace(FArrangedWidget(Window.ToSharedRef(),Window->GetWindowGeometryInScreen()));for(int32 I=0;I<Children.Num();++I)Widgets.Emplace(Children[I]);
   FWidgetPath Path{MakeArrayView(Widgets)};if(!Test->TestTrue(TEXT("Native pointer path reaches city button"),Path.GetLastWidget()==Button))return;
   auto G=Button->GetCachedGeometry();auto P=G.GetAbsolutePosition()+G.GetAbsoluteSize()*.5;
   FPointerEvent Down(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()),Up(0,P,P,TSet<FKey>{},EKeys::LeftMouseButton,0,FModifierKeysState());
   Button->OnMouseEnter(G,Down);FSlateApplication::Get().RoutePointerDownEvent(Path,Down);FSlateApplication::Get().RoutePointerUpEvent(Path,Up);Button->OnMouseLeave(Up);
  };
  if(!Prepared){
   Hud->GetScenarioPresentationModel()->Close();Hud->GetBuildMenuPresentationModel()->SetOpen(false);Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   switch(Stage){
   case 0:Root->ActivateSemanticId(TEXT("HUD.TopStatus.CityOverview"));break;
   case 1:{const auto Rows=Model->GetActiveRows();if(Rows.IsEmpty()){Test->AddError(TEXT("Live population rows required"));return true;}
    Selected=Rows[0].StableId;FString Id=Selected.ToString().Replace(TEXT("."),TEXT("_"));
    Screen->FocusSemanticId(TEXT("CityOverview.Row.")+Id);
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Native Enter selects the actual cohort"),Model->GetSnapshot().SelectedRowStableId,Selected);break;}
   case 2:Screen->FocusSemanticId(TEXT("CityOverview.Tab.Population"));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_RightShoulder,FModifierKeysState(),0,false,0,0));Test->TestEqual(TEXT("Controller shoulder opens Production"),Model->GetSnapshot().ActiveTab,EHansaCityOverviewTab::Production);break;
   case 3:Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Market"));break;
   case 4:Click(TEXT("CityOverview.City.Rostock"));Test->TestEqual(TEXT("Native mouse switches to Rostock"),Model->GetSnapshot().CityStableId,FName(TEXT("City.Rostock")));Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Population"));Test->TestEqual(TEXT("Rostock civic data is explicitly unavailable"),Model->GetSnapshot().LoadState,EHansaCityOverviewLoadState::Empty);break;
   case 5:Test->TestFalse(TEXT("Construction activation is blocked behind remote overview"),Root->ActivateSemanticId(TEXT("BuildMenu.Category.Production")));Test->TestFalse(TEXT("Construction focus is blocked behind remote overview"),Root->FocusSemanticId(TEXT("BuildMenu.Category.Production")));Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Production"));break;
   case 6:Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Market"));Test->TestTrue(TEXT("Rostock has report-aware market rows"),!Model->GetActiveRows().IsEmpty());break;
   case 7:Model->SetLoading();break;
   case 8:Model->SetError(FText::FromString(TEXT("The city report could not be prepared.")),FText::FromString(TEXT("Try again to request a fresh city report.")));Screen->FocusSemanticId(TEXT("CityOverview.State.Retry"));break;
   case 9:FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));Test->TestEqual(TEXT("Retry recovers without fabricated data"),Model->GetSnapshot().LoadState,EHansaCityOverviewLoadState::Ready);Screen->ActivateSemanticId(TEXT("CityOverview.City.Lubeck"));Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Population"));Root->SetPreferences({true,true,true});break;
   case 10:Model->SetLoading(FText::FromString(TEXT("Freie und Hansestadt Lübeck – nördlicher Hafen- und Handwerksspeicherbezirk")));break;
   case 11:{Model->SetError(FText::FromString(TEXT("Capture refresh")),FText::FromString(TEXT("Retry")));Model->RetryIntent();Model->SelectTabIntent(EHansaCityOverviewTab::Population);
    FName Target;for(const auto& Row:Model->GetActiveRows())if(Row.bCausalActionEnabled){Target=Row.StableId;break;}
    Test->TestFalse(TEXT("An actual population need links to supply"),Target.IsNone());
    Test->TestTrue(TEXT("Need causal navigation opens supply"),Model->ActivateCausalIntent(Target));
    Test->TestEqual(TEXT("Causal navigation reaches Production"),Model->GetSnapshot().ActiveTab,EHansaCityOverviewTab::Production);break;}
   case 12:{FName Target;for(const auto& Row:Model->GetActiveRows())if(Row.bCausalActionEnabled){Target=Row.StableId;break;}
    FString Id=Target.ToString().Replace(TEXT("."),TEXT("_"));Screen->FocusSemanticId(TEXT("CityOverview.Row.")+Id+TEXT(".Reveal"));
break;}
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  if(Stage==12 && !Activated){
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    Test->TestFalse(TEXT("Building cause closes the overview"),Model->GetSnapshot().bOpen);Test->TestTrue(TEXT("Building cause opens the production inspector"),Hud->GetInspectorPresentationModel()->GetSnapshot().bOpen);
    Activated=true;Ready=FPlatformTime::Seconds();return false;
  }
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native viewport readback failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  const TCHAR* Names[]={TEXT("population"),TEXT("focus"),TEXT("production"),TEXT("market"),TEXT("rostock-population"),TEXT("rostock-production"),TEXT("rostock-market"),TEXT("loading"),TEXT("error"),TEXT("accessible"),TEXT("localized"),TEXT("supply-chain"),TEXT("building-cause")};
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P24/city-%dx%d-%s"),X,Y,Names[Stage]);TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved native capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot())Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  Test->TestTrue(TEXT("Saved semantic evidence"),FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv"))));
  if(Stage>=4 && Stage<=6)for(const auto& N:Screen->GetSemanticSnapshot())Test->TestFalse(TEXT("Remote overview exposes no construction target"),N.State.bVisible && N.Id.StartsWith(TEXT("BuildMenu.")));
  ++Stage;Prepared=false;return Stage==13;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false,Activated=false;FName Selected;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOverviewViewport,"Hansa.UI.CityOverviewPolish.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FCityOverviewViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FCityOverviewCapture(this));return true;}
#endif
