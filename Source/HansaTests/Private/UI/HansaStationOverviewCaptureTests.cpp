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
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaTradeStationPresentation.h"
#include "World/HansaGameMode.h"

namespace {
class FStationOverviewCapture final : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false,WorldSelected=false;
 TArray<uint8> OriginalCampaign;
public:
 explicit FStationOverviewCapture(FAutomationTestBase* In):Test(In){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("World station Orders viewport timed out"));return true;}
  auto* V=GEngine?GEngine->GameViewport.Get():nullptr;auto* W=V?V->GetWorld():nullptr;
  auto* Controller=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Front=Hud->GetFrontendPresentationModel();
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Mode=W->GetAuthGameMode<AHansaGameMode>();auto* Host=Mode?Mode->GetSimulationHost():nullptr;auto* Model=Hud->GetTradeMapPresentationModel();
  if(!Host||!Model)return false;
  const bool Office=Stage>=8;const int32 Step=Stage%8;
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  if(!Prepared){
   if(Step==0){
    Model->CloseIntent();Hud->GetScenarioPresentationModel()->DismissHelp();Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
    if(Stage==0){if(!Host->CaptureSaveBytes(OriginalCampaign,TEXT("Isolated world Orders baseline"),TEXT("2026-10-02T00:00:00Z"))){Test->AddError(TEXT("Cannot capture isolated campaign baseline"));return true;}}
    else if(!Host->RestoreSaveBytes(OriginalCampaign)){Test->AddError(TEXT("Cannot restore isolated campaign baseline"));return true;}
    FString Error;if(!Hansa::Tests::PrepareLedger(*Host,Error,Hansa::Simulation::EHansaTradeStationOperationalState::Active,false,Office)){Test->AddError(Error);return true;}
    Root->SetPreferences({Scale>1,true,Scale>1,Scale});
    Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
    WorldSelected=false;Ready=FPlatformTime::Seconds()+3;Prepared=true;return false;
   }
   if(Step==1){
    Test->TestTrue(TEXT("Native direct Orders tab opens"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders")));
    Test->TestTrue(TEXT("Native Details tab receives keyboard focus"),Root->FocusSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Native keyboard tab switching opens Orders"),Model->GetSnapshot().ActiveSection,FString(TEXT("Orders")));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_LeftShoulder,FModifierKeysState(),0,false,0,0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_LeftShoulder,FModifierKeysState(),0,false,0,0));
    Test->TestEqual(TEXT("Native controller shoulder opens Details"),Model->GetSnapshot().ActiveSection,FString(TEXT("Presence")));
    Test->TestTrue(TEXT("Reopen shared Orders list"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders")));
   }
   if(Step==2){Test->TestTrue(TEXT("Existing order opens in reused editor"),Root->ActivateSemanticId(TEXT("TradeMap.Orders.Row.2")));Test->TestTrue(TEXT("Order target remains reachable through native focus"),Root->FocusSemanticId(TEXT("TradeMap.Orders.Target.Value")));}
   if(Step==4)Test->TestTrue(TEXT("Dedicated Upgrade tab opens"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Upgrade")));
   if(Step==5){Test->TestTrue(TEXT("Return to Overview"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));Test->TestTrue(TEXT("Focus reveals lease disclosure at bottom of Overview"),Root->FocusSemanticId(TEXT("TradeMap.Station.Terms")));}
   if(Step==6)Test->TestTrue(TEXT("Lease disclosure opens rights"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));
   if(Step==7)Test->TestTrue(TEXT("Lease returns to Overview"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Return")));
   if(Step==3){
    Hansa::Simulation::FHansaManageStationOrderCommand Resume;Resume.StationId=Hansa::Simulation::FHansaTradeStationId::TryCreate(100).Value;Resume.OrderId=1;Resume.Action=Hansa::Simulation::EHansaStationOrderAction::Resume;
    Test->TestTrue(TEXT("Resume sale through authoritative gateway"),Host->ManageStationOrder(Resume).IsSuccess());Host->AdvanceTicks(10);
    Test->TestTrue(TEXT("Native Overview returns after executed order ticks"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));
   }
   Ready=FPlatformTime::Seconds()+2;Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  if(Step==0&&!WorldSelected){
   AHansaTradeStationPresentation* Building=nullptr;for(TActorIterator<AHansaTradeStationPresentation> It(W);It;++It)if(It->GetStationId()==100){Building=*It;break;}
   if(!Building)return false;
   FHitResult Hit;Test->TestTrue(TEXT("World building selectable through physical trace"),W->LineTraceSingleByChannel(Hit,Building->GetActorLocation()+FVector(0,0,3000),Building->GetActorLocation(),ECC_Visibility));
   Controller->OnWorldSelectionChanged.Broadcast(Hit.GetActor(),Hit);
   Test->TestTrue(TEXT("Physical selection opens local detail window"),Model->bWorldStationDetail&&Model->GetSnapshot().bOpen);
   Test->TestEqual(TEXT("Physical selection identifies correct building stage"),Model->GetSnapshot().Establishment.bOfficeBuilt,Office);
   WorldSelected=true;Ready=FPlatformTime::Seconds()+2;return false;
  }
  const auto Nodes=Root->GetSemanticSnapshot();
  for(const TCHAR* Id:{TEXT("TradeMap.WorldStation.Tab.Details"),TEXT("TradeMap.WorldStation.Tab.Orders"),TEXT("TradeMap.WorldStation.Tab.Upgrade")}){
   const auto* N=Nodes.FindByPredicate([&](const auto& X){return X.Id==Id;});
   Test->TestTrue(TEXT("Direct tab is visible with a usable hit area"),N&&N->State.bVisible&&N->bCanFocus&&N->Bounds.Height()>=40&&N->Bounds.Width()>=80&&N->Bounds.Min.Y>=0&&N->Bounds.Max.Y<=V->Viewport->GetSizeXY().Y);
   if(N)Test->TestEqual(TEXT("Native selected state follows active section"),N->State.bSelected,Id==FString(Step==1||Step==2?TEXT("TradeMap.WorldStation.Tab.Orders"):Step==4?TEXT("TradeMap.WorldStation.Tab.Upgrade"):TEXT("TradeMap.WorldStation.Tab.Details")));
  }
  Test->TestTrue(TEXT("Tab transitions retain world context"),Model->bWorldStationDetail&&Model->GetSnapshot().Establishment.StationId==100);
  Test->TestEqual(TEXT("Tab transitions use shared section"),Model->GetSnapshot().ActiveSection,FString(Step==1||Step==2?TEXT("Orders"):Step==4?TEXT("StationUpgrade"):TEXT("Presence")));
  if(Step==2)for(const TCHAR* Id:{TEXT("TradeMap.Orders.Save"),TEXT("TradeMap.Orders.Pause"),TEXT("TradeMap.Orders.Cancel"),TEXT("TradeMap.Orders.Back")}){
   const auto* N=Nodes.FindByPredicate([&](const auto& X){return X.Id==Id;});Test->TestTrue(TEXT("Existing order footer stays in viewport"),N&&N->State.bVisible&&N->Bounds.Height()>=40&&N->Bounds.Min.X>=0&&N->Bounds.Max.X<=V->Viewport->GetSizeXY().X&&N->Bounds.Max.Y<=V->Viewport->GetSizeXY().Y);
  }
  if(Step!=1&&Step!=2)for(const TCHAR* Id:{TEXT("TradeMap.Station.Footer"),TEXT("TradeMap.Station.ShowOnMap")}){
   const auto* N=Nodes.FindByPredicate([&](const auto& X){return X.Id==Id;});Test->TestTrue(TEXT("Overview and Upgrade retain fixed footer inside viewport"),N&&N->State.bVisible&&N->Bounds.Height()>=40&&N->Bounds.Min.X>=0&&N->Bounds.Max.X<=V->Viewport->GetSizeXY().X&&N->Bounds.Max.Y<=V->Viewport->GetSizeXY().Y);
  }
  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("World Orders screenshot failed"));return true;}
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/StationOverview");IFileManager::Get().MakeDirectory(*Dir,true);
  const TCHAR* States[]={TEXT("overview"),TEXT("orders"),TEXT("editor"),TEXT("return-overview"),TEXT("upgrade"),TEXT("overview-bottom"),TEXT("lease"),TEXT("return-lease")};
  const FString Base=Dir/FString::Printf(TEXT("native-%s--%s--%dx%d--scale%.1f"),Office?TEXT("merchant-office"):TEXT("trade-station"),States[Step],Size.X,Size.Y,Scale);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Save actual world Orders viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("# Fixture=isolated authoritative station 100; building=%s; physical world trace; viewport=%dx%d; scale=%.1f\n"),Office?TEXT("MerchantOffice"):TEXT("TradeStation"),Size.X,Size.Y,Scale);
  for(const auto& N:Nodes)if(N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bSelected,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return Stage>=16;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStationOverviewViewport,"Hansa.UI.StationOverview.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaStationOverviewViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FStationOverviewCapture(this));return true;}
#endif
