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
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SViewport.h"

namespace {
class FTradeCreatorCapture final : public IAutomationLatentCommand {
public:
 explicit FTradeCreatorCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  using namespace Hansa::Simulation;
  if(FPlatformTime::Seconds()-Start>100){Test->AddError(TEXT("P26 real viewport delivery timed out"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;
  if(!Hud||!Hud->GetRootWidget()||!Host)return false;
  if(HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();auto* Model=Hud->GetTradeMapPresentationModel();
  auto Press=[&](const TCHAR* Id,bool Controller=false){
   if(!Test->TestTrue(FString::Printf(TEXT("Native focus %s"),Id),Root->FocusSemanticId(Id)))return;
   const auto Key=Controller?EKeys::Gamepad_FaceButton_Bottom:EKeys::Enter;
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key,FModifierKeysState(),0,false,0,0));
  };
  if(!Prepared){
   switch(Stage){
    case 0: FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({false,false,false,Scale});Hud->GetScenarioPresentationModel()->Close();Hud->GetBuildMenuPresentationModel()->SetOpen(false);Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Press(TEXT("HUD.TopStatus.TradeMap"));break;
    case 1: Press(TEXT("TradeMap.New"));Test->TestTrue(TEXT("Native new route enters creator"),Model->GetSnapshot().bCreating);break;
    case 2: {
     Press(TEXT("TradeMap.Creator.Cog"),true);Press(TEXT("TradeMap.Stop.0"));Press(TEXT("TradeMap.Editor.Reserve.Decrease"));
     auto Input=StaticCastSharedPtr<SEditableTextBox>(Root->ResolveSemanticWidget(TEXT("TradeMap.Creator.Name")));
     if(!Input){Test->AddError(TEXT("Name input missing"));return true;}Input->SetText(FText());Press(TEXT("TradeMap.Creator.Review"));
     Test->TestFalse(TEXT("Invalid name blocks departure"),Model->GetSnapshot().bCanCreate);break;
    }
    case 3: {
     Press(TEXT("TradeMap.Creator.Edit"));
     auto Input=StaticCastSharedPtr<SEditableTextBox>(Root->ResolveSemanticWidget(TEXT("TradeMap.Creator.Name")));
     Root->FocusSemanticId(TEXT("TradeMap.Creator.Name"));Input->SetText(FText());
     for(TCHAR Ch:FString(TEXT("Rostock provisions")))FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(Ch,FModifierKeysState(),0,false));
     Test->TestEqual(TEXT("Keyboard types route name"),Model->GetSnapshot().DraftName,FString(TEXT("Rostock provisions")));
     Press(TEXT("TradeMap.Creator.Review"));Test->TestTrue(TEXT("Corrected native draft validates"),Model->GetSnapshot().bCanCreate);break;
    }
    case 4: {
     Press(TEXT("TradeMap.Creator.Edit"));Press(TEXT("TradeMap.Creator.Add"));Press(TEXT("TradeMap.Creator.Remove"));
     Press(TEXT("TradeMap.Close"));Press(TEXT("HUD.TopStatus.TradeMap"));Test->TestTrue(TEXT("Closed draft is retained"),Model->GetSnapshot().bCreating);
     Press(TEXT("TradeMap.Creator.Review"));break;
    }
    case 5: {
     Press(TEXT("TradeMap.Creator.Activate"),true);RouteId=Model->GetSnapshot().SelectedRouteValue;
     Test->TestTrue(TEXT("Controller creates a new active route"),RouteId>3&&!Model->GetSnapshot().bCreating);
     Press(TEXT("TradeMap.Close"));Press(TEXT("HUD.TopStatus.Speed.Fastest"));Press(TEXT("HUD.TopStatus.TradeMap"));break;
    }
    case 6: break;
    case 7: Root->SetPreferences({true,true,true,Scale});Press(TEXT("TradeMap.New"));break;
    case 8: Press(TEXT("TradeMap.Creator.Review"));break;
   }
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<.7)return false;
  if(Stage==6){
   const auto P=Host->BuildProjection();const auto* R=P?P.Value.GetRoutes().FindByPredicate([this](const auto& It){return It.Id.GetValue()==RouteId;}):nullptr;
   if(!R||R->LastTransfer.Kind!=EHansaRouteCargoActionKind::Unload||R->LastTransfer.CityId.ToString()!=TEXT("City.Rostock")||R->LastTransfer.AppliedQuantity.GetRawValue()<=0)return false;
   Test->TestTrue(TEXT("Ordinary game clock produces Rostock cargo delivery"),true);
   Delivered=R->LastTransfer.AppliedQuantity.GetRawValue();Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Native screenshot failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
  Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  if(Stage==3||Stage==4) {
   const auto Nodes=Root->GetSemanticSnapshot();const auto* N=Nodes.FindByPredicate([](const auto& It){return It.Id==TEXT("TradeMap.Creator.Activate");});
   Test->TestTrue(TEXT("Departure action is visible, within viewport and at least 48px high"),N&&N->State.bVisible&&N->State.bEnabled&&N->Bounds.Height()>=47&&N->Bounds.Min.X>=0&&N->Bounds.Min.Y>=0&&N->Bounds.Max.X<=X&&N->Bounds.Max.Y<=Y);
  }
  const TCHAR* Names[]={TEXT("directory"),TEXT("draft"),TEXT("invalid-name"),TEXT("review"),TEXT("retained"),TEXT("departed"),TEXT("delivered"),TEXT("accessible-draft"),TEXT("accessible-review")};
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P26/trade-%dx%d-scale%d-%s"),X,Y,FMath::RoundToInt(Scale*100),Names[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
  Test->TestTrue(TEXT("Native capture saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("routeId=%lld deliveredMilliUnits=%lld\nid\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n"),RouteId,Delivered);
  for(const auto& N:Root->GetSemanticSnapshot())Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==9;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;int64 RouteId=0,Delivered=0;float Scale=1.f;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeCreatorViewport,"Hansa.UI.TradeCreator.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FTradeCreatorViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FTradeCreatorCapture(this));return true;}
#endif
