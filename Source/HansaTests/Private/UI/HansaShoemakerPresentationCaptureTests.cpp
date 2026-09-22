#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
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
class FShoemakerPresentationCapture final:public IAutomationLatentCommand {
public:
 explicit FShoemakerPresentationCapture(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  using namespace Hansa::Simulation;
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Artisan workshop flow timed out"));return true;}
  auto* V=GEngine?GEngine->GameViewport.Get():nullptr;if(!V||!V->GetWorld()||!V->GetWorld()->HasBegunPlay())return false;
  auto* W=V->GetWorld();auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;auto* GM=W->GetAuthGameMode<AHansaGameMode>();auto* Host=GM?GM->GetSimulationHost():nullptr;
  if(!Hud||!Host||!Hud->GetRootWidget().IsValid()||HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();
  if(!Placed){
   Root->SetPreferences({false,true,false,1.f});
   if(!Test->TestTrue(TEXT("Normal New Game initializes"),Host->IsReady()))return true;
   Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Hud->GetScenarioPresentationModel()->Close();
   auto* Build=Hud->GetBuildMenuPresentationModel();const auto* Map=Host->FindPlacementMap();if(!Map)return true;
   auto Place=[&](const TCHAR* Id){
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==Id)return true;
    const auto* D=Host->FindBuildingDefinition(Id);if(!D)return false;
    for(const auto& Cell:Map->Cells){
     if(Cell.Terrain!=EHansaPlacementTerrain::Land)continue;
     FHansaPlacementSpec Spec{Host->GetCityId(),FHansaBuildingTypeId::TryParse(Id).Value,Cell.Coordinate,EHansaGridRotation::North};
     const auto Before=Host->ValidatePlacement(Spec);
     if(!Before.CanPlace()&&!(Before.GetReasons().Num()==1&&Before.GetPrimaryFailure()==EHansaPlacementFailure::RoadRequired))continue;
     if(!Before.CanPlace()){
      for(int Side=0;Side<4;++Side){
       const int X=Side==0?Spec.Anchor.X-1:Side==1?Spec.Anchor.X+D->FootprintWidthCells:Spec.Anchor.X;
       const int Y=Side==2?Spec.Anchor.Y-1:Side==3?Spec.Anchor.Y+D->FootprintHeightCells:Spec.Anchor.Y;
       Build->SelectBuilding(TEXT("Building.Road"));Build->TargetGridCell(X,Y);
       if(Build->GetSnapshot().bCanConfirm&&Build->ConfirmIntent()){Host->AdvanceTicks(Host->FindBuildingDefinition(TEXT("Building.Road"))->BuildTicks+2);break;}
      }
     }
     if(!Host->ValidatePlacement(Spec).CanPlace())continue;
     Build->SelectBuilding(Id);Build->TargetGridCell(Cell.Coordinate.X,Cell.Coordinate.Y);
     if(Build->GetSnapshot().bCanConfirm&&Build->ConfirmIntent()){Host->AdvanceTicks(D->BuildTicks+2);return true;}
    }return false;
   };
   for(const TCHAR* Id:{TEXT("Building.Shoemaker")})
    if(!Test->TestTrue(Id,Place(Id)))return true;
   Build->SetOpen(false);Placed=true;Ready=FPlatformTime::Seconds()+2;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  if(!Selected){
   const TCHAR* Ids[]={TEXT("Building.CharcoalBurner"),TEXT("Building.Smithy"),TEXT("Building.Tannery"),TEXT("Building.Shoemaker")};
   const FString Id=Ids[Stage];
   for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==Id){
    C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
    {
     bool MeshFound=false;TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
     for(auto* M:Meshes)if(M->GetStaticMesh()&&M->GetStaticMesh()->GetPathName().Contains(FParse::Param(FCommandLine::Get(), TEXT("ArtisanProductionCandidate")) ? TEXT("/Game/Hansa/Generated/Staging/ArtisanProductionModelsV1/") : TEXT("/Game/Mesh/hansa-artisan-production/")))MeshFound=true;
     Test->TestTrue(TEXT("Live Shoemaker uses its production mesh"),MeshFound);
    }
    if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(It->GetActorLocation());Camera->AddZoomIntent((Camera->GetZoomDistance()-2100.f)/Camera->ZoomUnitsPerStep);Camera->SetDragOrbitIntent(FVector2D((-145.f-Camera->GetCameraYawDegrees())/Camera->DragRotationDegreesPerPixel,(-30.f-Camera->GetCameraPitchDegrees())/-Camera->DragRotationDegreesPerPixel),true);}
    Selected=true;break;
   }
   if(!Selected)return false;
   Root->ActivateSemanticId(TEXT("Session.Help.Hide"));
   Ready=FPlatformTime::Seconds()+2;return false;
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!Test->TestTrue(TEXT("Capture actual viewport"),FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return true;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("ArtisanProduction");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("workshop-%dx%d-stage%d"),Size.X,Size.Y,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence;for(const auto& Node:Root->GetSemanticSnapshot())Evidence+=Node.Id+TEXT("\t")+Node.State.Value+TEXT("\n");
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Selected=false;Ready=FPlatformTime::Seconds()+1;return Stage>=4;
 }
private:FAutomationTestBase* Test;double Started,Ready=0;int Stage=3;bool Placed=false,Selected=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShoemakerPresentationViewport,"Hansa.UI.ShoemakerPresentation.RealWorkshop",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FShoemakerPresentationViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FShoemakerPresentationCapture(this));return true;}
#endif
