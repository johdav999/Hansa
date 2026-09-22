#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Widgets/SViewport.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaContextInspector.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"
namespace {
class FPreservedFishCapture final:public IAutomationLatentCommand {
public:
 explicit FPreservedFishCapture(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  using namespace Hansa::Simulation;
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Preserved fish real-flow capture timed out"));return true;}
  auto* V=GEngine?GEngine->GameViewport.Get():nullptr;if(!V||!V->GetWorld()||!V->GetWorld()->HasBegunPlay())return false;
  auto* W=V->GetWorld();auto* Controller=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=Controller?Cast<AHansaRootHud>(Controller->GetHUD()):nullptr;auto* GM=W->GetAuthGameMode<AHansaGameMode>();auto* Host=GM?GM->GetSimulationHost():nullptr;
  if(!Hud||!Host||!Hud->GetRootWidget().IsValid()||HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();auto* Inspector=Hud->GetInspectorPresentationModel();
  if(!Placed){
   if(!Test->TestTrue(TEXT("Ordinary New Game initialized the runtime"),Host->IsReady()))return true;
   Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Hud->GetScenarioPresentationModel()->Close();
   auto* Build=Hud->GetBuildMenuPresentationModel();const auto* Map=Host->FindPlacementMap();if(!Map)return true;
   const auto* Fishery=Host->FindBuildingDefinition(TEXT("Building.Fishery"));if(!Fishery)return true;
   for(const auto& Cell:Map->Cells){
    if(Cell.Terrain!=EHansaPlacementTerrain::Shore)continue;
    for(int DX=-2;DX<=0 && !Placed;++DX)for(int DY=-1;DY<=0 && !Placed;++DY){
     FHansaPlacementSpec Spec{Host->GetCityId(),FHansaBuildingTypeId::TryParse(TEXT("Building.Fishery")).Value,{Cell.Coordinate.X+DX,Cell.Coordinate.Y+DY},EHansaGridRotation::North};
     const auto Before=Host->ValidatePlacement(Spec);
     if(!Before.CanPlace() && !(Before.GetReasons().Num()==1&&Before.GetPrimaryFailure()==EHansaPlacementFailure::RoadRequired))continue;
     if(!Before.CanPlace()){
      for(int Side=0;Side<4;++Side){
       const int X=Side==0?Spec.Anchor.X-1:Side==1?Spec.Anchor.X+Fishery->FootprintWidthCells:Spec.Anchor.X;
       const int Y=Side==2?Spec.Anchor.Y-1:Side==3?Spec.Anchor.Y+Fishery->FootprintHeightCells:Spec.Anchor.Y;
       Build->SelectBuilding(TEXT("Building.Road"));Build->TargetGridCell(X,Y);
       if(Build->GetSnapshot().bCanConfirm&&Build->ConfirmIntent()){Host->AdvanceTicks(Host->FindBuildingDefinition(TEXT("Building.Road"))->BuildTicks);break;}
      }
     }
     Build->SelectBuilding(TEXT("Building.Fishery"));Build->TargetGridCell(Spec.Anchor.X,Spec.Anchor.Y);
     if(Build->GetSnapshot().bCanConfirm&&Build->ConfirmIntent()){Placed=true;Host->AdvanceTicks(Fishery->BuildTicks);break;}
    }
    if(Placed)break;
   }
   if(!Test->TestTrue(TEXT("Build shoreline fishery through ordinary build controls"),Placed))return true;
   Build->SetOpen(false);Ready=FPlatformTime::Seconds()+1;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  if(!Selected){
   for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==TEXT("Building.Fishery")||It->GetBuildingDefinitionId()==TEXT("Building.Fishery.SaltingShed")){
    Controller->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
    if(auto* Camera=Cast<AHansaStrategyCameraPawn>(Controller->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(It->GetActorLocation());}
    Root->ActivateSemanticId(TEXT("Session.Help.Hide"));Selected=true;break;
   }
   if(!Selected)return false;
   if(Stage==1){
    if(!Test->TestTrue(TEXT("Choose salted through native inspector"),Root->ActivateSemanticId(TEXT("Inspector.Preservation.Salted"))))return true;
    if(!Test->TestTrue(TEXT("Enable optional fresh fallback"),Root->ActivateSemanticId(TEXT("Inspector.Preservation.Fallback"))))return true;
    Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Preservation.Salted"));
   }
   if(Stage==2)Root->SetPreferences({true,true,true});
   Ready=FPlatformTime::Seconds()+1;return false;
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!Test->TestTrue(TEXT("Capture actual game viewport"),FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return true;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/PreservedFish");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("preservedfish-%dx%d-stage%d"),Size.X,Size.Y,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence;for(const auto& Node:Root->GetSemanticSnapshot())Evidence+=Node.Id+TEXT("\t")+Node.State.Value+TEXT("\n");FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  if(Stage==0){
   if(!Test->TestTrue(TEXT("Pay for salting shed through native inspector"),Root->ActivateSemanticId(TEXT("Inspector.Preservation.Upgrade"))))return true;
   Host->AdvanceTicks(Host->FindBuildingDefinition(TEXT("Building.Fishery.SaltingShed"))->BuildTicks+2);
  }
  if(Stage==1){
   const auto& Actions=Inspector->GetSnapshot().Actions;
   Test->TestTrue(TEXT("Selected salted mode remains explicit"),Actions.ContainsByPredicate([](const auto& A){return A.StableId==TEXT("Inspector.Preservation.Salted")&&A.bSelected;}));
  }
  ++Stage;Selected=false;Ready=FPlatformTime::Seconds()+1;return Stage>=3;
 }
private:FAutomationTestBase* Test;double Started,Ready=0;int Stage=0;bool Placed=false,Selected=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPreservedFishViewport,"Hansa.UI.PreservedFish.RealFlow",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FPreservedFishViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FPreservedFishCapture(this));return true;}
#endif
