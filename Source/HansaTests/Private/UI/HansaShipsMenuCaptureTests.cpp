#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/SHansaBuildMenu.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaCargoProjectionManager.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "World/HansaGameMode.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Widgets/SViewport.h"

namespace {
class FShipsCapture final : public IAutomationLatentCommand {
public:
 explicit FShipsCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("Ships capture timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();auto Menu=Root->GetConstructionMenu();auto* M=H->GetBuildMenuPresentationModel();
  auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn());if(!Camera){Test->AddError(TEXT("Missing camera"));return true;}
  if(!Prepared){
   H->GetScenarioPresentationModel()->Close();H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();
   if(Stage==0){
    float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);Root->SetPreferences({Scale>1,Scale>1,Scale>1,Scale});
    Test->TestTrue(TEXT("Ships opens via production HUD"),Root->ActivateSemanticId(TEXT("BuildMenu.Ships")));
    Test->TestTrue(TEXT("Ships category receives focus and scroll reveal"),Root->FocusSemanticId(TEXT("BuildMenu.Ships")));
    auto* Mode=Cast<AHansaGameMode>(W->GetAuthGameMode());if(!Mode){Test->AddError(TEXT("Missing game mode"));return true;}const auto P=Mode->GetSimulationHost()->BuildProjection();
    int32 Owned=0;for(const auto& Ship:P.Value.GetVehicles())if(Ship.OwnerId==Mode->GetSimulationHost()->GetHouseId()&&Ship.Mode==Hansa::Simulation::EHansaRouteMode::Sea)++Owned;
    Test->TestEqual(TEXT("Every owned sea vessel and no rival vessels"),M->GetShips().Num(),Owned);
    if(M->GetShips().IsEmpty()){Test->AddError(TEXT("New Game requires starting ship"));return true;}
    ShipId=M->GetShips()[0].Id;Semantic=FString::Printf(TEXT("BuildMenu.Ship.%lld"),ShipId);
   }
   if(Stage==1){
    const FVector2D Before=Camera->GetViewState().Focus;
    Test->TestTrue(TEXT("Single selection"),M->SelectShip(ShipId,false));
    Test->TestTrue(TEXT("Single selection leaves camera"),Camera->GetViewState().Focus.Equals(Before));
    auto Slot=Menu->ResolveSemanticWidget(Semantic);Test->TestTrue(TEXT("Live native slot"),Slot.IsValid());if(!Slot)return true;
    const FVector2D Position=Slot->GetCachedGeometry().GetAbsolutePosition()+Slot->GetCachedGeometry().GetAbsoluteSize()*.5;
    const FPointerEvent Event(0,Position,Position,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
    Test->TestTrue(TEXT("Native double click handled"),Slot->OnMouseButtonDoubleClick(Slot->GetCachedGeometry(),Event).IsEventHandled());
    AssertCentered(W,Camera);
   }
   if(Stage==2){
    Camera->FocusWorldLocationIntent(Camera->GetActorLocation()+FVector(1500,1500,0));
    Test->TestTrue(TEXT("Ship focus available"),Root->FocusSemanticId(Semantic));
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Gamepad_FaceButton_Bottom,FModifierKeysState(),0,false,0,0));
    AssertCentered(W,Camera);
   }
   Prepared=true;ReadyFrame=GFrameCounter;return false;
  }
  if(GFrameCounter<ReadyFrame+8)return false;
  const auto Tree=Root->GetSemanticSnapshot();
  for(const auto& N:Tree)if(N.Id==TEXT("BuildMenu.Ships")||N.Id==Semantic)Test->TestTrue(TEXT("Fleet controls have readable height"),N.State.bVisible&&N.Bounds.Height()>=40);
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Real viewport screenshot failed"));return true;}
  float Scale=1;FParse::Value(FCommandLine::Get(),TEXT("HansaGuiScale="),Scale);
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("ShipsMenu");IFileManager::Get().MakeDirectory(*Dir,true);
  const FString Base=Dir/FString::Printf(TEXT("ships-%dx%d-scale-%.1f-%d"),Size.X,Size.Y,Scale,Stage);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png")));
  FString Evidence;for(const auto& N:Tree)if(N.Id.StartsWith(TEXT("BuildMenu.")))Evidence+=FString::Printf(TEXT("%s\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value);
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  Prepared=false;return ++Stage==3;
 }
 void AssertCentered(UWorld* W,AHansaStrategyCameraPawn* Camera){
  bool Found=false;for(TActorIterator<AHansaCargoProjectionManager> It(W);It;++It)for(const auto& O:It->QueryCargo())
   if(!O.VehicleId.IsEmpty()&&FCString::Atoi64(*O.VehicleId)==ShipId)
   {
    Found=true;
    Test->TestTrue(TEXT("Camera centers current ship position"),
     Camera->GetViewState().Focus.Equals(FVector2D(O.Location.X,O.Location.Y),1.));
    Test->TestTrue(TEXT("Selected cog has a visible world actor"),
     O.bVisible&&O.PresentationFailure.IsEmpty()&&It->FindActor(O.SemanticId)!=nullptr);
    if(Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(W))
     Test->TestTrue(TEXT("Starting Cog occupies authored sea water"),
      FVector::Dist2D(O.Location,Hansa::Game::LubeckPlacementGrid::CampaignLubeckWaterAnchor())<3000.0);
    for(TActorIterator<AHansaLubeckWorldFoundation> Foundation(W);Foundation;++Foundation)
    {
     Test->TestTrue(TEXT("Cog occupies the active Lubeck world"),
      FVector::Dist2D(O.Location,Foundation->GetActorLocation())<30000.);
     break;
    }
   }
  Test->TestTrue(TEXT("Ship has real world projection"),Found);
 }
private:FAutomationTestBase* Test;double Start;uint64 ReadyFrame=0;int32 Stage=0;int64 ShipId=0;FString Semantic;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaShipsViewport,"Hansa.UI.Ships.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaShipsViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FShipsCapture(this));return true;}
#endif
