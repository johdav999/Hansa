#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "Components/StaticMeshComponent.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaTradeStationPresentation.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"

namespace {
class FStationSelectionCapture : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false;
public:
 explicit FStationSelectionCapture(FAutomationTestBase* In):Test(In){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Station selection capture timed out"));return true;}
  auto* View=GEngine?GEngine->GameViewport.Get():nullptr;auto* World=View?View->GetWorld():nullptr;
  auto* Controller=World?Cast<AHansaStrategyPlayerController>(World->GetFirstPlayerController()):nullptr;
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Front=Hud->GetFrontendPresentationModel();
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Game=World->GetAuthGameMode<AHansaGameMode>();auto* Host=Game?Game->GetSimulationHost():nullptr;
  auto* Model=Hud->GetTradeMapPresentationModel();auto* Camera=Cast<AHansaStrategyCameraPawn>(Controller->GetPawn());
  if(!Host||!Camera)return false;
  if(Stage==0&&!Prepared){
   Hud->GetScenarioPresentationModel()->DismissHelp();Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);FString Error;
   if(!Host->ApplyMerchantOfficeTestSetup(Error)){Test->AddError(Error);return true;}
   const auto P=Host->BuildProjection().Value;
   const auto* S=P.GetTradeStations().FindByPredicate([&](const auto& Entry){return Entry.Station.OwnerId==Host->GetHouseId()&&Entry.Station.CityId.ToString()==TEXT("City.Rostock");});
   if(!S){Test->AddError(TEXT("Selection fixture missing station"));return true;}
   Model->OpenWorldStation(TEXT("City.Rostock"),S->Station.Id.GetValue());
   Test->TestTrue(TEXT("Show on map selects world station"),Root->ActivateSemanticId(TEXT("TradeMap.Station.ShowOnMap")));
   Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->MinimumZoomDistance=1000;
   Camera->AddZoomIntent((Camera->GetZoomDistance()-4500)/Camera->ZoomUnitsPerStep);
   float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,true,Scale>1,Scale});
   Ready=FPlatformTime::Seconds()+8;Prepared=true;return false;
  }
  AHansaTradeStationPresentation* Station=nullptr;
  for(TActorIterator<AHansaTradeStationPresentation> It(World);It;++It){Station=*It;break;}
  if(!Station)return false;
  if(!Prepared){
   if(Stage==1||Stage==4)Controller->OnWorldSelectionChanged.Broadcast(nullptr,FHitResult());
   if(Stage==2||Stage==3){
    if(Stage==3){
     Test->TestTrue(TEXT("Reopen selected station for upgrade"),Model->OpenWorldStation(TEXT("City.Rostock"),Station->GetStationId()));
     Test->TestTrue(TEXT("Upgrade tab"),Root->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Upgrade")));
     Test->TestTrue(TEXT("Upgrade review"),Root->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
     Test->TestTrue(TEXT("Upgrade confirmation"),Root->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
     Host->AdvanceTicks(4);
     Test->TestTrue(TEXT("Completed office projection"),Model->GetSnapshot().Establishment.bOfficeBuilt);
    }
    Model->CloseIntent();FHitResult Hit;
    Test->TestTrue(TEXT("Physical warehouse trace"),World->LineTraceSingleByChannel(Hit,Station->GetActorLocation()+FVector(0,0,3000),Station->GetActorLocation(),ECC_Visibility));
    Controller->OnWorldSelectionChanged.Broadcast(Hit.GetActor(),Hit);
    Test->TestTrue(TEXT("Selection opens shared world inspector"),Model->bWorldStationDetail&&Model->GetSnapshot().bOpen);
   }
   Ready=FPlatformTime::Seconds()+2;Prepared=true;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  const bool Selected=Stage!=1&&Stage!=4;
  Test->TestEqual(TEXT("World selection state"),Station->IsSelected(),Selected);
  Test->TestEqual(TEXT("Diamond visible only while selected"),Station->SelectionOutline->IsVisible(),Selected);
  for(UStaticMeshComponent* Segment:Station->SelectionCornerSegments)Test->TestEqual(TEXT("Corner visible only while selected"),Segment->IsVisible(),Selected);
  // At compact large-text sizes the inspector covers the world centre. Close
  // its panel while retaining world selection so the actual marker is reviewable.
  if(View->Viewport->GetSizeXY().X<=1400&&Model->GetSnapshot().bOpen){Model->CloseIntent();Ready=FPlatformTime::Seconds()+.5;return false;}
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Selection screenshot failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const TCHAR* States[]={TEXT("station-map-selected"),TEXT("station-cleared"),TEXT("station-click-selected"),TEXT("office-click-selected"),TEXT("office-cleared")};
  const FString Dir=FPaths::ProjectDir()/TEXT("Docs/Images/UI/StationSelection");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("native--%s--%dx%d--scale%.1f"),States[Stage],Size.X,Size.Y,Scale);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
  Test->TestTrue(TEXT("Native capture saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=FString::Printf(TEXT("Fixture=isolated campaign; user saves untouched\nStation=%lld\nSelected=%d\nLocation=%s\n"),Station->GetStationId(),Station->IsSelected(),*Station->GetActorLocation().ToString());
  for(UStaticMeshComponent* Segment:Station->SelectionCornerSegments)Evidence+=FString::Printf(TEXT("%s Visible=%d Location=%s\n"),*Segment->GetName(),Segment->IsVisible(),*Segment->GetComponentLocation().ToString());
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));
  ++Stage;Prepared=false;return Stage==5;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStationSelectionViewport,"Hansa.UI.StationSelection.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaStationSelectionViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FStationSelectionCapture(this));return true;}
#endif
