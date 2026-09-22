#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "Components/StaticMeshComponent.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaCargoVehiclePresentation.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Trade/HansaWaterNavigation.h"
#include "Widgets/SViewport.h"
using namespace Hansa::Simulation;
namespace {
class FShipNavigationCapture final:public IAutomationLatentCommand {
public:
 explicit FShipNavigationCapture(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()){}
 bool Update()override {
  const double Now=FPlatformTime::Seconds();
  if(Now-Started>180){Test->AddError(FString::Printf(TEXT("Ship capture timeout at stage %d"),Stage));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* View=GEngine->GameViewport.Get();auto* W=View->GetWorld();
  auto* PC=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* Hud=PC?Cast<AHansaRootHud>(PC->GetHUD()):nullptr;
  if(!Hud||!Hud->GetRootWidget()||HansaWaitForFrontend(Hud)||Now<Ready)return false;
  auto* Host=Cast<AHansaGameMode>(W->GetAuthGameMode())->GetSimulationHost();
  AHansaCargoProjectionManager* Manager=nullptr;for(TActorIterator<AHansaCargoProjectionManager> It(W);It;++It){Manager=*It;break;}
  if(!Manager)return false;
  AHansaCargoVehiclePresentation* Ship=nullptr;
  for(const auto& O:Manager->QueryCargo())if(O.bFreeNavigation){Ship=Manager->FindActor(O.SemanticId);break;}
  if(!Ship){Test->AddError(TEXT("Starting explorable Cog missing"));return true;}
  auto& Slate=FSlateApplication::Get();auto Root=Hud->GetRootWidget();
  const auto Projection=Host->BuildProjection();
  const auto* Vehicle=Projection.Value.GetVehicles().FindByPredicate([&](const auto& V){return V.Id==Ship->GetVehicleId();});
  if(!Vehicle)return true;
  auto MoveCursor=[&](FVector2D Screen){const auto Old=Slate.GetCursorPos();Pointer=View->GetGameViewportWidget()->GetCachedGeometry().LocalToAbsolute(Screen);Slate.SetCursorPos(Pointer);Slate.ProcessMouseMoveEvent(FPointerEvent(0,Pointer,Old,{},EKeys::Invalid,0,FModifierKeysState()));};
  auto Press=[&](FKey Key){Slate.ProcessMouseButtonDownEvent(nullptr,FPointerEvent(0,Pointer,Pointer,{Key},Key,0,FModifierKeysState()));};
  auto Release=[&](FKey Key){Slate.ProcessMouseButtonUpEvent(FPointerEvent(0,Pointer,Pointer,{},Key,0,FModifierKeysState()));};
  auto Capture=[&](const TCHAR* Label){
   TArray<FColor> Pixels;FIntVector Size;
   if(!Slate.TakeScreenshot(View->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport capture failed"));return;}
   const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("ShipNavigation/ship-%dx%d-%s"),Size.X,Size.Y,Label);
   TArray64<uint8>Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Native ship screenshot saved"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
   FString Evidence=FString::Printf(TEXT("tick=%lld\nship=%llu\ncell=%d,%d\ntarget=%d,%d\ncargo=%lld\nworld=%s\n"),Host->GetSimulationTick(),Vehicle->Id.GetValue(),Vehicle->Navigation.Cell.X,Vehicle->Navigation.Cell.Y,Target.X,Target.Y,Vehicle->Cargo.GetRawValue(),*Ship->GetActorLocation().ToString());
   for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("Inspector.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%s\n"),*N.Id,N.State.bVisible,*N.State.Value);
   FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".txt")));
  };
  if(Stage==0){
   Hud->GetScenarioPresentationModel()->AcknowledgeBriefing();Hud->GetScenarioPresentationModel()->DismissHelp();
   Host->SetMerchantAIEnabled(false);Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Hud->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   Hud->GetBuildMenuPresentationModel()->SetOpen(false);
   auto* Camera=Cast<AHansaStrategyCameraPawn>(PC->GetPawn());Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(Ship->GetActorLocation());
   Home=Vehicle->Navigation.Home;Ready=Now+3;++Stage;return false;
  }
  if(Stage==1){FVector2D P;PC->ProjectWorldLocationToScreen(Ship->GetActorLocation()+FVector(0,0,500),P,true);MoveCursor(P);++Stage;return false;}
  if(Stage==2){Press(EKeys::LeftMouseButton);++Stage;return false;}
  if(Stage==3){Release(EKeys::LeftMouseButton);Ready=Now+.3;++Stage;return false;}
  if(Stage==4){
   if(!Test->TestTrue(TEXT("Native left-click selects Cog and opens ship inspector"),PC->GetSelectedWorldActor()==Ship&&Hud->GetInspectorPresentationModel()->GetSnapshot().Kind==EHansaInspectorObjectKind::Cargo))return true;
   Test->TestTrue(TEXT("Selected Cog has imported visible marker"),Ship->SelectionMarker && Ship->SelectionMarker->GetStaticMesh() && Ship->SelectionMarker->IsVisible());
   Test->TestTrue(TEXT("Marker never intercepts orders"),Ship->SelectionMarker->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
   Capture(TEXT("selected"));bool Found=false;const auto* Map=Host->FindPlacementMap();TArray<FHansaGridCoordinate> Path;
   int32 Width=0,Height=0;PC->GetViewportSize(Width,Height);
   for(int32 X=-6;X<=6&&!Found;X+=2)for(int32 Y=-6;Y<=6&&!Found;Y+=2){
    if(FMath::Abs(X)+FMath::Abs(Y)<4)continue;Target={Home.X+X,Home.Y+Y};
    FVector2D P;auto Location=Hansa::Game::LubeckPlacementGrid::GridToWorld(Target,Ship->GetActorLocation().Z);
    if(PC->ProjectWorldLocationToScreen(Location,P,true)&&P.X>320&&P.X<Width-400&&P.Y>150&&P.Y<Height-240&&FHansaWaterNavigation::FindPath(*Map,Home,Target,Path)){Found=true;MoveCursor(P);}
   }
   if(!Test->TestTrue(TEXT("Reachable water target in unobscured viewport"),Found))return true;
   ++Stage;return false;
  }
  if(Stage==5){Press(EKeys::RightMouseButton);++Stage;return false;}
  if(Stage==6){Release(EKeys::RightMouseButton);Ready=Now+.3;++Stage;return false;}
  if(Stage==7){
   if(!Test->TestTrue(TEXT("Native right-click issues course to water"),Vehicle->Navigation.IsMoving()&&Vehicle->Navigation.Path.Last()==Target))return true;
   Capture(TEXT("ordered"));Root->ActivateSemanticId(TEXT("HUD.TopStatus.Speed.Normal"));++Stage;return false;
  }
  if(Stage==8){if(Vehicle->Navigation.IsMoving())return false;Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);
   Test->TestTrue(TEXT("Normal game clock reaches target"),Vehicle->Navigation.Cell==Target);
   Test->TestTrue(TEXT("World Cog matches authoritative cell"),FVector::Dist2D(Ship->GetActorLocation(),Hansa::Game::LubeckPlacementGrid::GridToWorld(Target))<5);
   Test->TestTrue(TEXT("Marker follows sailing Cog"),Ship->SelectionMarker->IsVisible() && FVector::Dist2D(Ship->SelectionMarker->GetComponentLocation(),Ship->GetActorLocation())<1);
   Capture(TEXT("arrived"));
   Test->TestTrue(TEXT("Return action is keyboard/controller reachable"),Root->FocusSemanticId(TEXT("Inspector.Ship.Home")));
   Test->TestTrue(TEXT("Return action issues normal command"),Root->ActivateSemanticId(TEXT("Inspector.Ship.Home")));
   Root->ActivateSemanticId(TEXT("HUD.TopStatus.Speed.Normal"));++Stage;return false;
  }
  if(Stage==9){if(Vehicle->Navigation.IsMoving())return false;Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Test->TestTrue(TEXT("Returns without teleporting"),Vehicle->Navigation.Cell==Home);Capture(TEXT("home"));Manager->ClearSelection();Test->TestFalse(TEXT("Deselection hides marker"),Ship->SelectionMarker->IsVisible());Ready=Now+.3;++Stage;return false;}
  if(Stage==10){Capture(TEXT("deselected"));return true;}
  return false;
 }
private:FAutomationTestBase* Test;double Started,Ready=0;int32 Stage=0;FVector2D Pointer;FHansaGridCoordinate Home,Target;
};}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaShipNavigationViewport,"Hansa.UI.ShipNavigation.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaShipNavigationViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FShipNavigationCapture(this));return true;}
#endif
