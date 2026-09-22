#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
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
class FFirewoodCapture final:public IAutomationLatentCommand {
public:
 explicit FFirewoodCapture(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  using namespace Hansa::Simulation;
  if(FPlatformTime::Seconds()-Started>180){Test->AddError(TEXT("Firewood native flow timed out"));return true;}
  auto* V=GEngine?GEngine->GameViewport.Get():nullptr;if(!V||!V->GetWorld()||!V->GetWorld()->HasBegunPlay())return false;
  auto* W=V->GetWorld();auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* Hud=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;auto* GM=W->GetAuthGameMode<AHansaGameMode>();auto* Host=GM?GM->GetSimulationHost():nullptr;
  if(!Hud||!Host||!Hud->GetRootWidget().IsValid()||HansaWaitForFrontend(Hud))return false;
  auto Root=Hud->GetRootWidget();
  if(!Placed){
   if(!Test->TestTrue(TEXT("Normal New Game loads accepted catalog"),Host->IsReady()))return true;
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
   if(!Test->TestTrue(TEXT("Yard builds through normal controls"),Place(TEXT("Building.WoodcutterYard"))))return true;
   if(!Test->TestTrue(TEXT("Market builds through normal controls"),Place(TEXT("Building.Market"))))return true;
   Build->SetOpen(false);Placed=true;Ready=FPlatformTime::Seconds()+2;return false;
  }
  if(FPlatformTime::Seconds()<Ready)return false;
  if(!Selected){
   const FString Id=Stage==0?TEXT("Building.WoodcutterYard"):TEXT("Building.Market");
   for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==Id){
    C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());
    if(Stage==0){
     bool MeshFound=false;TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
     for(auto* M:Meshes)if(M->GetStaticMesh()&&M->GetStaticMesh()->GetPathName().Contains(TEXT("/Game/Mesh/hansa-woodcutter-yard/")))MeshFound=true;
     Test->TestTrue(TEXT("Live yard uses approved production mesh"),MeshFound);
    }
    if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(It->GetActorLocation());}
    Selected=true;break;
   }
   if(!Selected)return false;
   Root->ActivateSemanticId(TEXT("Session.Help.Hide"));
   if(Stage==1){
    Test->TestTrue(TEXT("Native reserve increase"),Root->ActivateSemanticId(TEXT("Inspector.Heating.Increase")));
    Test->TestTrue(TEXT("Native reserve release"),Root->ActivateSemanticId(TEXT("Inspector.Heating.Override")));
    Test->TestTrue(TEXT("Authoritative release visible"),Host->QueryHeating().bOverride);
    Root->GetInspector()->FocusSemanticId(TEXT("Inspector.Heating.Override"));
   }
   if(Stage==2){
    Test->TestTrue(TEXT("Native reserve restore"),Root->ActivateSemanticId(TEXT("Inspector.Heating.Override")));
    Test->TestFalse(TEXT("Authoritative protection restored"),Host->QueryHeating().bOverride);
    Root->SetPreferences({true,true,true});
   }
   Ready=FPlatformTime::Seconds()+2;return false;
  }
  TArray<FColor> Pixels;FIntVector Size;
  if(!Test->TestTrue(TEXT("Capture actual viewport"),FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)))return true;
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("Automation/Firewood");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("firewood-%dx%d-stage%d"),Size.X,Size.Y,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence;for(const auto& Node:Root->GetSemanticSnapshot())Evidence+=Node.Id+TEXT("\t")+Node.State.Value+TEXT("\n");
  const auto H=Host->QueryHeating();Evidence+=FString::Printf(TEXT("AuthoritativeHeating\tdays=%d override=%d multiplier=%d stock=%lld protected=%lld\n"),H.ReserveDays,H.bOverride,H.SeasonMultiplier,H.StockRaw,H.ProtectedRaw);
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Selected=false;Ready=FPlatformTime::Seconds()+1;return Stage>=3;
 }
private:FAutomationTestBase* Test;double Started,Ready=0;int Stage=0;bool Placed=false,Selected=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirewoodViewport,"Hansa.UI.Firewood.RealFlow",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FFirewoodViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FFirewoodCapture(this));return true;}
#endif
