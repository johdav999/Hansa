#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaBuildMenu.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaBuildingWorldProjection.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace {
class FClickConstructionCapture final:public IAutomationLatentCommand {
public:
 explicit FClickConstructionCapture(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()){}
 ~FClickConstructionCapture() override {
  if(SavedCursor && FSlateApplication::IsInitialized()) {
   auto& S=FSlateApplication::Get();
   S.ProcessMouseButtonUpEvent(FPointerEvent(0,S.GetCursorPos(),S.GetCursorPos(),{},EKeys::LeftMouseButton,0,FModifierKeysState()));
   S.SetCursorPos(Original);
  }
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>90){Test->AddError(TEXT("Click construction viewport timed out"));return true;}
  if(!GEngine || !GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();
  auto* C=W?Cast<AHansaStrategyPlayerController>(W->GetFirstPlayerController()):nullptr;
  auto* H=C?Cast<AHansaRootHud>(C->GetHUD()):nullptr;
  if(!H || HansaWaitForFrontend(H))return false;
  auto* M=H->GetBuildMenuPresentationModel();if(!M)return false;
  auto& S=FSlateApplication::Get();
  if(FPlatformTime::Seconds()<Ready)return false;
  auto Move=[&](FVector2D P,bool Held){
   const auto Old=S.GetCursorPos();S.SetCursorPos(P);
   S.ProcessMouseMoveEvent(FPointerEvent(0,P,Old,Held?TSet<FKey>{EKeys::LeftMouseButton}:TSet<FKey>{},EKeys::Invalid,0,FModifierKeysState()));
  };
  auto FindSite=[&](FVector2D& Out){
   AHansaLubeckWorldFoundation* Foundation=nullptr;
   for(TActorIterator<AHansaLubeckWorldFoundation> It(W);It;++It){Foundation=*It;break;}
   if(!Foundation)return false;
   int32 Width,Height;C->GetViewportSize(Width,Height);
   int32 Valid=0,Visible=0;
   for(int32 Y=0;Y<40;++Y)for(int32 X=0;X<60;++X){
    M->TargetGridCell(X,Y);FVector2D P;
    Valid+=M->GetSnapshot().bCanConfirm?1:0;
    if(M->GetSnapshot().bCanConfirm && C->ProjectWorldLocationToScreen(Foundation->PlacementCellToWorld(X,Y,106),P,true)
      && P.X>380 && P.X<Width-380 && P.Y>120 && P.Y<Height*.4){
     ++Visible;FIntPoint Resolved;FVector World;
     if(!C->ResolvePlacementCellAtScreenPosition(P,Resolved,World) || Resolved!=FIntPoint(X,Y))continue;
     Out=V->GetGameViewportWidget()->GetCachedGeometry().LocalToAbsolute(P);
     const auto Path=S.LocateWindowUnderMouse(Out,S.GetInteractiveTopLevelWindows());
     if(!Path.IsValid() || Path.Widgets.Last().Widget!=V->GetGameViewportWidget())continue;
     M->ClearPointerTarget();return true;
    }
   }
   Test->AddInfo(FString::Printf(TEXT("Site search valid=%d visible=%d selected=%s cause=%s"),Valid,Visible,*M->GetSnapshot().SelectedBuildingId.ToString(),*M->GetSnapshot().ValidationCause.ToString()));
   return false;
  };
  switch(Stage){
  case -1:
   H->GetScenarioPresentationModel()->Close();
   M->SelectBuilding(TEXT("Building.Road"));M->BeginRoadDraw(18,10);M->UpdateRoadDraw(18,30);
   if(!M->EndRoadDraw(true))Test->AddInfo(FString::Printf(TEXT("Road preparation: %s"),*M->GetSnapshot().ValidationCause.ToString()));
   M->SelectBuilding(TEXT("Building.Residence.Laborer"));
   if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){
    Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();
    bool Found=false;
    for(TActorIterator<AHansaLubeckWorldFoundation> It(W);It && !Found;++It)
     for(int32 Y=0;Y<40 && !Found;++Y)for(int32 X=0;X<60;++X){
      M->TargetGridCell(X,Y);if(M->GetSnapshot().bCanConfirm){Camera->FocusWorldLocationIntent(It->PlacementCellToWorld(X,Y,106)-FRotator(0,Camera->GetCameraYawDegrees(),0).Vector()*1800.f);Found=true;break;}
     }
   }
   break;
  case 0:
   M->CancelIntent();M->SelectCategory(EHansaBuildCategory::Residences);break;
  case 1:
   {
    auto Card=H->GetRootWidget()->GetConstructionMenu()->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_Residence_Laborer"));
    if(!Test->TestTrue(TEXT("Actual house card is available"),Card.IsValid()))return true;
    Original=S.GetCursorPos();SavedCursor=true;
    auto P=Card->GetCachedGeometry().GetAbsolutePosition()+Card->GetCachedGeometry().GetAbsoluteSize()*.5;
    Move(P,false);
    S.ProcessMouseButtonDownEvent(nullptr,FPointerEvent(0,P,P,{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));
    S.ProcessMouseButtonUpEvent(FPointerEvent(0,P,P,{},EKeys::LeftMouseButton,0,FModifierKeysState()));
   }break;
  case 2:
   Test->TestEqual(TEXT("House card click selects residence"),M->GetSnapshot().SelectedBuildingId,FName(TEXT("Building.Residence.Laborer")));
   H->GetScenarioPresentationModel()->Close();
   if(auto* Camera=Cast<AHansaStrategyCameraPawn>(C->GetPawn())){Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();}
   Before=M->GetPlacedBuildingCount();
   if(!Test->TestTrue(TEXT("Find visible validated house site"),FindSite(Target)))return true;
   Move(Target,false);
   // Slate can still know the desktop cursor while the scene viewport cache has
   // been cleared by leaving it for a build button (not a held world drag).
   V->GetGameViewportWidget()->OnMouseLeave(FPointerEvent());
   C->PlayerTick(0.f);
   if(!Test->TestTrue(TEXT("Released-button preview survives an invalid viewport mouse cache"),M->GetSnapshot().bHasTarget && C->GetPlacementGhost() && C->GetPlacementGhost()->IsPreviewVisible()))return true;
   break;
  case 3:
   Test->TestEqual(TEXT("Mouse hover never constructs"),M->GetPlacedBuildingCount(),Before);
   if(!Test->TestTrue(TEXT("Passive movement creates a visible valid ghost"),M->GetSnapshot().bCanConfirm && C->GetPlacementGhost() && C->GetPlacementGhost()->IsPreviewVisible())) {
    Test->AddInfo(FString::Printf(TEXT("Preview selected=%s cell=%s hasTarget=%d valid=%d visible=%d cause=%s cursor=%s target=%s"),*M->GetSnapshot().SelectedBuildingId.ToString(),*M->GetSnapshot().AnchorCell.ToString(),M->GetSnapshot().bHasTarget,M->GetSnapshot().bCanConfirm,C->GetPlacementGhost()&&C->GetPlacementGhost()->IsPreviewVisible(),*M->GetSnapshot().ValidationCause.ToString(),*S.GetCursorPos().ToString(),*Target.ToString()));
    const auto Path=S.LocateWindowUnderMouse(S.GetCursorPos(),S.GetInteractiveTopLevelWindows());
    if(Path.IsValid())for(int32 I=0;I<Path.Widgets.Num();++I)Test->AddInfo(FString::Printf(TEXT("Pointer path: %s ownViewport=%d"),*Path.Widgets[I].Widget->GetTypeAsString(),Path.Widgets[I].Widget==V->GetGameViewportWidget()));return true;
   }
   S.ProcessMouseButtonDownEvent(nullptr,FPointerEvent(0,Target,Target,{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState()));break;
  case 4:
   if(!Test->TestEqual(TEXT("Native world press constructs exactly one house"),M->GetPlacedBuildingCount(),Before+1)){
    Test->AddInfo(FString::Printf(TEXT("After click selected=%s cell=%s canConfirm=%d stroke=%d key=%d cause=%s result=%s"),*M->GetSnapshot().SelectedBuildingId.ToString(),*M->GetSnapshot().AnchorCell.ToString(),M->GetSnapshot().bCanConfirm,M->IsBuildingStrokeActive(),C->IsInputKeyDown(EKeys::LeftMouseButton),*M->GetSnapshot().ValidationCause.ToString(),*M->GetSnapshot().LastResult.ToString()));return true;
   }
   Test->TestTrue(TEXT("Held mouse starts stroke"),M->IsBuildingStrokeActive());
   if(!Test->TestTrue(TEXT("Find another visible valid site"),FindSite(Target)))return true;
   Move(Target,true);break;
  case 5:
   After=M->GetPlacedBuildingCount();
   Test->TestTrue(TEXT("Held mouse movement constructs another house"),After>Before+1);
   S.ProcessMouseButtonUpEvent(FPointerEvent(0,Target,Target,{},EKeys::LeftMouseButton,0,FModifierKeysState()));break;
  case 6:
   Test->TestFalse(TEXT("Mouse release stops stroke"),M->IsBuildingStrokeActive());
   Move(Target+FVector2D(80,0),false);break;
  case 7:
   Test->TestEqual(TEXT("Movement after release does not construct"),M->GetPlacedBuildingCount(),After);
   {
    TArray<FColor> Pixels;FIntVector Size;TArray64<uint8> Png;
    if(S.TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){
     FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
     FFileHelper::SaveArrayToFile(Png,*(FPaths::ProjectSavedDir()/TEXT("P22/click-stroke-native.png")));
    }
   }
   M->CancelIntent();return true;
  }
  ++Stage;Ready=FPlatformTime::Seconds()+.4;return false;
 }
private:
 FAutomationTestBase* Test;double Started,Ready=0;int32 Stage=-1,Before=0,After=0;
 bool SavedCursor=false;FVector2D Original,Target;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaClickConstructionViewport,"Hansa.UI.Construction.ClickStrokeViewport",
 EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaClickConstructionViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FClickConstructionCapture(this));return true;}
#endif
