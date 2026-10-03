#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaTradeEstablishmentTestSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaTradeStationPresentation.h"
#include "World/HansaGameMode.h"
#include "World/HansaTerrainPlacement.h"
#include "Components/ChildActorComponent.h"

namespace {
class FWorldStationCapture : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;
public:
 explicit FWorldStationCapture(FAutomationTestBase* In):Test(In){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("World station capture timed out"));return true;}
  auto* V=GEngine?GEngine->GameViewport.Get():nullptr;auto* W=V?V->GetWorld():nullptr;
  auto* Controller=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();
  auto* Front=Hud->GetFrontendPresentationModel();
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Title){Root->ActivateSemanticId(TEXT("Frontend.NewGame"));return false;}
  if(Front->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Host=W->GetAuthGameMode<AHansaGameMode>()->GetSimulationHost();auto* Model=Hud->GetTradeMapPresentationModel();
  auto* Camera=Cast<AHansaStrategyCameraPawn>(Controller->GetPawn());if(!Host||!Camera)return false;
  if(Stage==0){
   Hud->GetScenarioPresentationModel()->DismissHelp();Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
   FString Error;if(!Hansa::Tests::PrepareEstablishment(*Host,Error)){Test->AddError(Error);return true;}
   using namespace Hansa::Simulation;FHansaTradeStationId Id;
   if(!Host->ProposeTradeStation(FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,TEXT("TradeStationSite.Rostock.Harbor.01"),Id)||
      !Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(2).Value)){Test->AddError(TEXT("Station fixture funding failed"));return true;}
   Host->AdvanceTicks(4);Model->Open(TEXT("Test"),NAME_None,NAME_None,false);Model->SelectCityIntent(TEXT("City.Rostock"));Model->SelectSectionIntent(TEXT("Presence"));
   Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();
   Ready=FPlatformTime::Seconds()+8;Stage=1;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  AHansaTradeStationPresentation* Station=nullptr;for(TActorIterator<AHansaTradeStationPresentation> It(W);It;++It){Station=*It;break;}if(!Station)return false;
  if(Stage==1){
   Test->TestEqual(TEXT("No city visit needed"),Hud->GetViewedCity(),FName(TEXT("City.Lubeck")));
   Test->TestTrue(TEXT("Show on map action"),Root->ActivateSemanticId(TEXT("TradeMap.Station.ShowOnMap")));
   Camera->MinimumZoomDistance=1000;Camera->AddZoomIntent((Camera->GetZoomDistance()-8500)/Camera->ZoomUnitsPerStep);
   Ready=FPlatformTime::Seconds()+8;Stage=2;return false;
  }
  if(Stage==2){
   // Exercise the same trace and selection notification used by a player click.
   Model->CloseIntent();FHitResult Hit;
   Test->TestTrue(TEXT("Real world warehouse hit"),W->LineTraceSingleByChannel(Hit,Station->GetActorLocation()+FVector(0,0,3000),Station->GetActorLocation(),ECC_Visibility));
   Controller->OnWorldSelectionChanged.Broadcast(Hit.GetActor(),Hit);
   Test->TestTrue(TEXT("Click opens shared station details"),Model->bWorldStationDetail&&Model->GetSnapshot().bOpen);
   Test->TestTrue(TEXT("Camera centers the authoritative station"),Camera->GetViewState().Focus.Equals(FVector2D(Station->GetActorLocation()),1.));
   const FText TreasuryBefore=Model->GetSnapshot().Establishment.Treasury;
   Host->AdvanceTicks(1);
   Test->TestFalse(TEXT("World details immediately receive upkeep changes"),TreasuryBefore.EqualTo(Model->GetSnapshot().Establishment.Treasury));
   TArray<uint8> Saved;
   Test->TestTrue(TEXT("Capture station save"),Host->CaptureSaveBytes(Saved,TEXT("World station viewport"),TEXT("2026-09-29T00:00:00Z")).IsSuccess());
   Test->TestTrue(TEXT("Restore while station details are open"),Host->RestoreSaveBytes(Saved).IsSuccess());
   Test->TestEqual(TEXT("Restored details retain station identity"),Model->GetSnapshot().Establishment.StationId,Station->GetStationId());
   Test->TestTrue(TEXT("Restored warehouse remains visible"),IsValid(Station)&&!Station->IsHidden());
   Test->TestTrue(TEXT("Lease terms open"),Root->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));
   Ready=FPlatformTime::Seconds()+2;Stage=3;return false;
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Station screenshot failed"));return true;}
  const FString Dir=FPaths::ProjectDir()/TEXT("Docs/Images/World/TradeStation");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("station--world-details--%dx%d"),Size.X,Size.Y);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence=FString::Printf(TEXT("Station=%lld\nWarehouse=%s\nViewedCity=%s\nFixture=isolated-authoritative-funded-station\n"),Station->GetStationId(),*Station->GetActorLocation().ToString(),*Hud->GetViewedCity().ToString());
  for(const auto& N:Root->GetSemanticSnapshot())if(N.State.bVisible&&N.Id.StartsWith(TEXT("TradeMap.")))Evidence+=FString::Printf(TEXT("%s\t%d,%d,%d,%d\t%s\n"),*N.Id,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
  Evidence+=FString::Printf(TEXT("Dock=%s\n"),*Station->Dock->GetComponentLocation().ToString());
  for(TActorIterator<AActor> It(W);It;++It)if(It->GetClass()->GetName().Contains(TEXT("WaterBody"))&&FVector::Dist2D(It->GetActorLocation(),Station->GetActorLocation())<100000){
   Evidence+=FString::Printf(TEXT("Water=%s Position=%s Hidden=%d Bounds=%s\n"),*It->GetName(),*It->GetActorLocation().ToString(),It->IsHidden(),*It->GetComponentsBoundingBox().ToString());
  }
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaWorldStationViewport,"Hansa.UI.WorldStation.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaWorldStationViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FWorldStationCapture(this));return true;}
#endif
