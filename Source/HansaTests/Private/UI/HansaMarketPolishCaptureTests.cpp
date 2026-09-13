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
#include "UI/SHansaMarketTable.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "Widgets/SViewport.h"
#include "Layout/WidgetPath.h"

namespace {
class FMarketCapture final:public IAutomationLatentCommand {
public:
 explicit FMarketCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>90){Test->AddError(TEXT("P25 viewport timed out"));return true;}
  if(!GEngine || !GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W || !W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!Hud || !Hud->GetRootWidget().IsValid())return false;
  if(HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();auto Screen=Root->GetCityOverview();auto* City=Hud->GetCityOverviewPresentationModel(); auto* Model=Hud->GetMarketTablePresentationModel(); auto Table=Screen->GetMarketTable();
  auto Click=[&](const FString& Id){
   auto Button=Table->ResolveSemanticWidget(Id);auto Window=V->GetWindow();if(!Button || !Window){Test->AddError(TEXT("Native mouse target unavailable"));return;}
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
   case 0:Root->ActivateSemanticId(TEXT("HUD.TopStatus.CityOverview"));Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Market"));Screen->ActivateSemanticId(TEXT("CityOverview.Market.Details"));break;
   case 1:{
    Click(TEXT("Market.Filter.Clear"));const uint64 ClearRevision=Model->GetRevision();Click(TEXT("Market.Filter.Clear"));
    Table->FocusSemanticId(TEXT("Market.Filter.Clear"));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Repeated clearing of inactive filters does not mutate the market"),Model->GetRevision(),ClearRevision);
    Click(TEXT("Market.Row.Good_Bread"));Test->TestEqual(TEXT("Native click selects Bread"),Model->GetSnapshot().SelectedGoodStableId,FName(TEXT("Good.Bread")));
    const uint64 SelectionRevision=Model->GetRevision();Click(TEXT("Market.Row.Good_Bread"));
    Table->FocusSemanticId(TEXT("Market.Row.Good_Bread"));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Repeated native selection keeps the selected good and revision"),Model->GetRevision(),SelectionRevision);
    break;
   }
   case 2:Table->FocusSemanticId(TEXT("Market.Header.Price"));FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));Test->TestEqual(TEXT("Native Enter sorts Price"),Model->GetSnapshot().SortColumn,EHansaMarketSortColumn::Price);break;
   case 3:Model->SetSearchTextIntent(FText::FromString(TEXT("unavailable-good")));Test->TestEqual(TEXT("Filtering preserves selection"),Model->GetSnapshot().SelectedGoodStableId,FName(TEXT("Good.Bread")));Test->TestFalse(TEXT("Hidden row cannot activate"),Table->ActivateSemanticId(TEXT("Market.Row.Good_Grain")));break;
   case 4:Model->ClearFiltersIntent();Root->SetPreferences({true,true,true});Screen=Root->GetCityOverview();break;
   case 5:City->SetLoading();break;
   case 6:City->SetError(FText::FromString(TEXT("Market report unavailable")),FText::FromString(TEXT("Retry the authoritative city report.")));break;
   case 7:City->RetryIntent();break;
   case 8:{
    for(const auto& Row:Model->GetSnapshot().AllRows){Model->SelectGoodIntent(Row.GoodStableId);
     for(const auto& R:Model->GetSnapshot().SelectedGood.Producers)if(R.BuildingValue>0){Target=R.BuildingValue;TargetId=TEXT("Market.Detail.Producer.")+R.StableId.ToString().Replace(TEXT("."),TEXT("_"));break;}
     if(Target>0)break;
    }
    Test->TestTrue(TEXT("Live market has a world producer"),Target>0);
    Table->FocusSemanticId(TargetId);break;
   }
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.5)return false;
  if(Stage==9 && !Activated){
    const auto Tree=Table->GetSemanticSnapshot();const auto* TargetNode=Tree.FindByPredicate([&](const auto& N){return N.Id==TargetId;});
    Test->TestTrue(TEXT("Focused world action is visibly scrolled into the detail panel"),TargetNode && TargetNode->State.bVisible && TargetNode->Bounds.Height()>0);
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    Test->TestFalse(TEXT("Market navigation closes overview"),City->GetSnapshot().bOpen);
    Test->TestTrue(TEXT("Market navigation opens real inspector"),Hud->GetInspectorPresentationModel()->GetSnapshot().bOpen);
    Test->TestEqual(TEXT("Inspector selects exact authoritative building"),Hud->GetInspectorPresentationModel()->GetSnapshot().BuildingValue,Target);
    Activated=true;Ready=FPlatformTime::Seconds();return false;
  }
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native viewport readback failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  const TCHAR* Names[]={TEXT("ledger"),TEXT("bread"),TEXT("sort-focus"),TEXT("empty"),TEXT("accessible"),TEXT("loading"),TEXT("error"),TEXT("retry"),TEXT("relationships"),TEXT("world-building")};
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P25/market-%dx%d-%s"),X,Y,Names[Stage]);TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved native capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Root->GetSemanticSnapshot())Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  Test->TestTrue(TEXT("Saved semantic evidence"),FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv"))));
  ++Stage;Prepared=false;return Stage==10;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false,Activated=false;int64 Target=0;FString TargetId;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMarketViewport,"Hansa.UI.MarketPolish.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FMarketViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FMarketCapture(this));return true;}
#endif
