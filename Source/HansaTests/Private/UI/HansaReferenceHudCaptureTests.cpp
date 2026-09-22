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
#include "UI/SHansaRootHud.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaBuildingWorldProjection.h"
#include "Widgets/SViewport.h"
namespace {
class FReferenceHudCapture final:public IAutomationLatentCommand {
public:
 explicit FReferenceHudCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update()override{
  if(FPlatformTime::Seconds()-Start>150){Test->AddError(TEXT("Reference HUD capture timeout"));return true;}
  if(!GEngine||!GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();if(!W||!W->HasBegunPlay())return false;
  auto* C=Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController());
  auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!H||!H->GetRootWidget()||HansaWaitForFrontend(H))return false;
  auto Root=H->GetRootWidget();
  if(!Prepared){
   H->GetScenarioPresentationModel()->Close();
   H->GetPresentationModel()->SetSpeed(EHansaHudGameSpeed::Paused);
   if(auto* P=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){P->bEnableMouseEdgePan=false;P->ClearCameraIntents();}
   if(Stage==0){H->GetInspectorPresentationModel()->CloseIntent();H->GetBuildMenuPresentationModel()->SetOpen(false);}
   if(Stage==1){
    Test->TestTrue(TEXT("Storage category opens through ordinary intent"),Root->ActivateSemanticId(TEXT("BuildMenu.Category.Storage")));
    bool Selected=false;
    for(TActorIterator<AHansaBuildingWorldProjectionActor> It(W);It;++It)if(It->GetBuildingDefinitionId()==TEXT("Building.Warehouse")){C->OnWorldSelectionChanged.Broadcast(*It,FHitResult());Selected=true;break;}
    if(!Selected){
     Test->AddInfo(TEXT("This new-game scenario has no placed warehouse; inspecting its real objective alert instead."));
     for(const auto& N:Root->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("HUD.AlertStack.Alert."))&&N.Id.EndsWith(TEXT(".OpenCause"))){Root->ActivateSemanticId(N.Id);break;}
     H->GetScenarioPresentationModel()->Close();
     auto* I=H->GetInspectorPresentationModel();
     const auto& Alerts=H->GetPresentationModel()->GetSnapshot().Alerts;
     if(!Alerts.IsEmpty()){const auto& A=Alerts[0];I->OpenFromAlert(A.StableId,A.AffectedObject,A.Age,A.AffectedBuildingValue,A.Causal,TEXT("HUD.AlertStack.Toggle"));}
    }
    Test->TestTrue(TEXT("Map zoom control exists"),Root->ResolveSemanticWidget(TEXT("HUD.Minimap.ZoomIn")).IsValid());
   }
   if(Stage==2){
    Test->TestTrue(TEXT("Map accepts keyboard/controller focus"),Root->FocusSemanticId(TEXT("HUD.Minimap")));
    auto* P=Cast<AHansaStrategyCameraPawn>(C->GetPawn());
    const auto Before=P?P->GetFocusLocation2D():FVector2D::ZeroVector;
    if(auto Map=Root->ResolveSemanticWidget(TEXT("HUD.Minimap")))Map->OnKeyDown(Map->GetCachedGeometry(),FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0));
    Test->TestTrue(TEXT("Map keyboard intent moves actual camera"),P&&P->GetFocusLocation2D()!=Before);
   }
   if(Stage==3)Root->SetPreferences({true,true,true,1.f});
   if(Stage==4)Root->SetPreferences({false,true,false,.8f});
   if(Stage==5)Root->SetPreferences({false,true,false,1.4f});
   Prepared=true;Ready=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-Ready<2)return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport screenshot failed"));return true;}
  const FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("ReferenceHud/hud-%dx%d-%d"),Size.X,Size.Y,Stage);
  TArray64<uint8>Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);Test->TestTrue(TEXT("Saved real viewport"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence;
  for(const auto& N:Root->GetSemanticSnapshot()){
   if(!N.Id.StartsWith(TEXT("HUD."))&&!N.Id.StartsWith(TEXT("BuildMenu."))&&!N.Id.StartsWith(TEXT("Inspector.")))continue;
   Evidence+=FString::Printf(TEXT("%s\t%d\t%d,%d,%d,%d\t%s\n"),*N.Id,N.State.bVisible,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   if(N.State.bVisible&&(N.Id==TEXT("HUD.TopStatus")||N.Id==TEXT("HUD.Minimap")||N.Id==TEXT("BuildMenu.Root")||N.Id==TEXT("HUD.InspectorHost")))
    Test->TestTrue(*FString::Printf(TEXT("%s fits viewport stage %d"),*N.Id,Stage),N.Bounds.Min.X>=0&&N.Bounds.Min.Y>=0&&N.Bounds.Max.X<=Size.X&&N.Bounds.Max.Y<=Size.Y);
  }
  FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv")));
  ++Stage;Prepared=false;return Stage==6;
 }
private:FAutomationTestBase* Test;double Start,Ready=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaReferenceHudViewport,"Hansa.UI.ReferenceHud.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaReferenceHudViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FReferenceHudCapture(this));return true;}
#endif

