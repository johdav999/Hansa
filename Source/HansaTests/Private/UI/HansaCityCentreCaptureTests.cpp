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
#include "World/HansaCityCentrePresentation.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaTerrainPlacement.h"
#include "Components/ChildActorComponent.h"
#include "Components/BoxComponent.h"
#include "UI/HansaInspectorPresentationModel.h"

namespace {
class FCityCentreCapture : public IAutomationLatentCommand {
 FAutomationTestBase* Test;double Started=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;
public:
 explicit FCityCentreCapture(FAutomationTestBase* In):Test(In){}
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
   int32 StartingCentres=0;for(TActorIterator<AHansaCityCentrePresentation> It(W);It;++It)++StartingCentres;
   Test->TestEqual(TEXT("Municipal centre precedes player station funding"),StartingCentres,1);
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
   Test->TestTrue(TEXT("Visit native Rostock centre"),Hud->VisitCityIntent(TEXT("City.Rostock")));
   Camera->MinimumZoomDistance=1000;Camera->AddZoomIntent((Camera->GetZoomDistance()-16000)/Camera->ZoomUnitsPerStep);
   Ready=FPlatformTime::Seconds()+8;Stage=2;return false;
  }
  AHansaCityCentrePresentation* Centre=nullptr;int32 Count=0;
  for(TActorIterator<AHansaCityCentrePresentation> It(W);It;++It){Centre=*It;++Count;}
  if(!Test->TestNotNull(TEXT("Starting centre exists"),Centre))return true;
  Test->TestEqual(TEXT("One centre"),Count,1);
  if(Stage==2){
   auto* Overview=Hud->GetCityOverviewPresentationModel();Overview->CloseIntent();Model->CloseIntent();
   FHitResult Hit;const FVector Market=Centre->GetMarketLocation();
   Test->TestTrue(TEXT("Normal market selection ray"),W->LineTraceSingleByChannel(Hit,Market+FVector(0,0,2000),Market,ECC_Visibility));
   Test->TestTrue(TEXT("Selection ray hits municipal centre"),Hit.GetActor()==Centre);
   Controller->OnWorldSelectionChanged.Broadcast(Hit.GetActor(),Hit);
   Test->TestFalse(TEXT("City-owned market click does not open overview"),Overview->GetSnapshot().bOpen);
   TArray<UBoxComponent*> Selections;Centre->GetComponents(Selections);int32 Checked=0;
   for(auto* Selection:Selections){
    const auto* Slot=Centre->FindSlot(Selection);if(!Slot||!Slot->bSelectable)continue;
    FHitResult BuildingHit(Centre,Selection,Selection->GetComponentLocation(),FVector::UpVector);
    Controller->OnWorldSelectionChanged.Broadcast(Centre,BuildingHit);++Checked;
    Test->TestFalse(*FString::Printf(TEXT("Municipal %s does not open overview"),*Slot->Id.ToString()),Overview->GetSnapshot().bOpen);
    Test->TestFalse(TEXT("Municipal scenery does not open player building details"),Hud->GetInspectorPresentationModel()->GetSnapshot().bOpen);
   }
   Test->TestEqual(TEXT("Every home, market and industry selection checked"),Checked,17);
   Controller->OnWorldSelectionChanged.Broadcast(Station,FHitResult());
   Test->TestTrue(TEXT("Owned station still opens its details"),Model->bWorldStationDetail&&Model->GetSnapshot().bOpen);
   Test->TestFalse(TEXT("Owned station does not open city overview"),Overview->GetSnapshot().bOpen);
   Controller->OnWorldSelectionChanged.Broadcast(Centre,Hit);
   Test->TestFalse(TEXT("Municipal click clears prior station details"),Model->GetSnapshot().bOpen);
   Test->TestTrue(TEXT("Explicit city overview navigation remains available"),Root->ActivateSemanticId(TEXT("HUD.TopStatus.CityOverview")));
   Test->TestTrue(TEXT("Explicit navigation opens overview"),Overview->GetSnapshot().bOpen);
   TArray<uint8> Saved;
   Test->TestTrue(TEXT("Save city presentation session"),Host->CaptureSaveBytes(Saved,TEXT("City centre"),TEXT("2026-09-29T00:00:00Z")).IsSuccess());
   Test->TestTrue(TEXT("Restore original simulation save"),Host->RestoreSaveBytes(Saved).IsSuccess());
   Test->TestTrue(TEXT("Centre survives restore"),IsValid(Centre)&&!Centre->IsHidden());
   Hud->GetCityOverviewPresentationModel()->CloseIntent();Model->CloseIntent();
   Ready=FPlatformTime::Seconds()+2;Stage=3;return false;
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Station screenshot failed"));return true;}
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Verification/ForeignBuildingSelection");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("rostock--waterfront-centre--%dx%d"),Size.X,Size.Y);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCityCentreViewport,"Hansa.UI.CityCentre.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaCityCentreViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FCityCentreCapture(this));return true;}
#endif
