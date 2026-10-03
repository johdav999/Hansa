#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "HansaTradeSpecializationTestSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "Placement/HansaRostockPlacement.h"
#include "World/HansaBuildingWorldProjection.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "UI/SHansaRootHud.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaGameMode.h"
#include "Widgets/SViewport.h"
namespace {
class FConstructionCapture final : public IAutomationLatentCommand {
 FAutomationTestBase* Test; double Start=FPlatformTime::Seconds(),Ready=0;int32 Stage=0;bool Prepared=false,WorldReady=false,GhostTargeted=false;
public:
 explicit FConstructionCapture(FAutomationTestBase* T):Test(T){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>180){Test->AddError(TEXT("Construction viewport timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;if(!Hud||!Hud->GetRootWidget())return false;
  auto Root=Hud->GetRootWidget();auto* Frontend=Hud->GetFrontendPresentationModel();
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Title){if(!Root->ActivateSemanticId(TEXT("Frontend.NewGame"))){Test->AddError(TEXT("Production New Game failed"));return true;}return false;}
  if(Frontend&&Frontend->GetSnapshot().Page==EHansaFrontendPage::Loading)return false;
  if(Hud->GetScenarioPresentationModel()->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing){Root->ActivateSemanticId(TEXT("Scenario.Begin"));return false;}
  auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());auto* Host=Mode?Mode->GetSimulationHost():nullptr;if(!Host)return false;
  // Offscreen automation has no deliberate cursor position; prevent synthetic edge pan.
  if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn()))Camera->bEnableMouseEdgePan=false;
  if(!Prepared){
   if(Stage==0){FString Error;if(!Hansa::Tests::PrepareSpecialization(*Host,Error,[&](auto& Init){const auto* Q=Host->GetEconomicRegistry()->FindPresenceStage(TEXT("PresenceStage.MerchantQuarter"));for(auto& P:Init.ForeignPresences)if(P.HouseId==Host->GetHouseId()&&P.CityId.ToString()==TEXT("City.Rostock")){P.CurrentStageId=Q->StableId;P.GrantedCapabilityIds=Q->GrantedCapabilityIds;}})){Test->AddError(Error);return true;}Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap"));Root->ActivateSemanticId(TEXT("TradeMap.Port.City.Rostock"));Test->TestTrue(TEXT("Expansion tab opens in assembled viewport"),Root->ActivateSemanticId(TEXT("TradeMap.Navigate.Construction")));}
   if(Stage==1){Root->ActivateSemanticId(TEXT("TradeMap.Construction.Building.Warehouse"));Test->TestTrue(TEXT("Browser focus reveals building"),Root->FocusSemanticId(TEXT("TradeMap.Construction.Building.Warehouse")));}
   if(Stage==2)Test->TestTrue(TEXT("Cost dossier is reachable by focus"),Root->FocusSemanticId(TEXT("TradeMap.Construction.Detail")));
   if(Stage==3){if(!Root->ActivateSemanticId(TEXT("TradeMap.Construction.Place"))){Test->AddError(TEXT("Production place action failed"));return true;}}
   if(Stage==4){Hud->GetBuildMenuPresentationModel()->TargetGridCell(8,9);Test->TestFalse(TEXT("Street preview rejects"),Hud->GetBuildMenuPresentationModel()->GetSnapshot().bCanConfirm);}
   if(Stage==5){auto* B=Hud->GetBuildMenuPresentationModel();B->TargetGridCell(9,8);if(!Test->TestTrue(TEXT("World confirm commits"),B->ConfirmIntent()))return true;B->CancelIntent();}
   if(Stage==6){Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap"));Test->TestEqual(TEXT("Return restores selected card"),Hud->GetTradeMapPresentationModel()->GetSnapshot().FocusedSemanticId,FName(TEXT("TradeMap.Construction.Building.Warehouse")));}
   Ready=FPlatformTime::Seconds();Prepared=true;return false;
  }
  if(Stage==3)
  {
   if(Hud->IsCityVisitLoading())return false;
   if(!WorldReady){WorldReady=true;Ready=FPlatformTime::Seconds();return false;}
   if(FPlatformTime::Seconds()-Ready<1.)return false;
   if(Hud->GetViewedCity()!=TEXT("City.Rostock")){Test->AddError(Hud->GetCityVisitStatus().ToString());return true;}
   if(Hud->GetBuildMenuPresentationModel()->GetSnapshot().SelectedBuildingId.IsNone()){Test->AddError(TEXT("Loaded Rostock without selected building: ")+Hud->GetCityVisitStatus().ToString());return true;}
   if(!GhostTargeted)
   {
    // Semantic automation owns the target; the absent hardware cursor must not overwrite it.
    C->SetActorTickEnabled(false);Hud->GetBuildMenuPresentationModel()->TargetGridCell(9,8);
    GhostTargeted=true;Ready=FPlatformTime::Seconds();return false;
   }
   int32 Ghosts=0;for(TActorIterator<AHansaBuildingPlacementGhost> It(W);It;++It)if(It->IsPreviewVisible()){++Ghosts;Test->TestTrue(TEXT("Ghost uses Rostock world binding"),It->GetActorLocation().X>50000);
    TArray<UStaticMeshComponent*> Parts;It->GetComponents(Parts);for(auto* Part:Parts)if(Part->IsVisible()&&Part->GetName().StartsWith(TEXT("FootprintCell")))Test->TestTrue(TEXT("Every ghost footprint cell uses Rostock coordinates"),Part->GetComponentLocation().X>50000);}
   Test->TestEqual(TEXT("Exactly one placement preview visible"),Ghosts,1);
   FVector2D Screen;FIntPoint HitCell;FVector HitWorld;
   Test->TestTrue(FString::Printf(TEXT("Pointer deprojection binds to Rostock at %dx%d"),V->Viewport->GetSizeXY().X,V->Viewport->GetSizeXY().Y),C->ProjectWorldLocationToScreen(Hansa::Simulation::RostockPlacement::CellCenter(9,8),Screen)&&C->ResolvePlacementCellAtScreenPosition(Screen,HitCell,HitWorld)&&HitCell==FIntPoint(9,8)&&HitWorld.X>50000);
   Test->AddInfo(FString::Printf(TEXT("Target screen %s; camera %s"),*Screen.ToString(),*C->GetPawn()->GetActorLocation().ToString()));
   Test->TestTrue(TEXT("Placement target is visible above the construction tray"),Screen.X>0&&Screen.X<V->Viewport->GetSizeXY().X&&Screen.Y>120&&Screen.Y<V->Viewport->GetSizeXY().Y-330);
   Test->TestTrue(TEXT("World preview remains in Rostock"),Hud->GetBuildMenuPresentationModel()->GetPlacementCity()==TEXT("City.Rostock"));
  }
  if(FPlatformTime::Seconds()-Ready<1.)return false;
  if(Stage==5){int32 Found=0;for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==TEXT("Building.Warehouse")){++Found;Test->TestTrue(TEXT("Committed actor uses Rostock world binding"),It->GetActorLocation().X>50000);}Test->TestEqual(TEXT("Foreign building has world actor"),Found,1);}

  TArray<FColor> Pixels;FIntVector Size;if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return true;}
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("TradeWorkspace/TG13");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("construction-%dx%d-%d"),Size.X,Size.Y,Stage);TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence=FString::Printf(TEXT("tick=%lld fingerprint=%llu\n"),Host->BuildProjection().Value.GetClock().GetTick().GetValue(),Host->BuildProjection().Value.GetFingerprint().Value);
  for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("TradeMap.Construction.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));++Stage;Prepared=false;return Stage>=7;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeConstructionViewport,"Hansa.UI.TradeConstruction.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FTradeConstructionViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FConstructionCapture(this));return true;}
#endif
