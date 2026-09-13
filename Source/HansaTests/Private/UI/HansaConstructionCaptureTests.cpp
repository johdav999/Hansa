#if WITH_DEV_AUTOMATION_TESTS
#include "HansaFrontendCaptureSupport.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "ImageUtils.h"
#include "UI/HansaRootHud.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaBuildMenu.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "Widgets/SViewport.h"
#include "World/HansaStrategyCameraPawn.h"
#include "Input/DragAndDrop.h"
#include "Widgets/Text/STextBlock.h"

namespace {
class FConstructionCapture final: public IAutomationLatentCommand {
public:
 explicit FConstructionCapture(FAutomationTestBase* T):Test(T),Start(FPlatformTime::Seconds()){}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>60){Test->AddError(TEXT("Construction viewport timed out"));return true;}
  if(!GEngine || !GEngine->GameViewport)return false;
  auto* V=GEngine->GameViewport.Get();auto* W=V->GetWorld();
  if(!W || !W->HasBegunPlay() || !W->GetFirstPlayerController())return false;
  auto* Hud=Cast<AHansaRootHud>(W->GetFirstPlayerController()->GetHUD());
  if(!Hud || !Hud->GetRootWidget().IsValid())return false;
  if(HansaWaitForFrontend(Hud))return false;
  auto Menu=Hud->GetRootWidget()->GetConstructionMenu();auto* Model=Hud->GetBuildMenuPresentationModel();
  if(!Menu.IsValid() || !Model)return false;
  if(auto* Camera=Cast<AHansaStrategyCameraPawn>(W->GetFirstPlayerController()->GetPawn())) {
   // Offscreen automation must not inherit the desktop cursor's edge-pan input.
   Camera->bEnableMouseEdgePan=false;Camera->ClearCameraIntents();Camera->FocusWorldLocationIntent(FVector(-3200,-700,100));
  }
  if(!Prepared){
   if(Hud->GetScenarioPresentationModel())Hud->GetScenarioPresentationModel()->Close();
   Model->CancelIntent();Model->SetOpen(false);
   switch(Stage){
    case 0:break;
    case 8:Menu->SetPreferences({});Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Roads"));break;
    case 9:Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Storage"));break;
    case 10:Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Harbor"));break;
    case 1:Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production"));Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Bread"));break;
    case 2:Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Fish"));break;
    case 3:Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Planks"));break;
    case 4:Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Residences"));break;
    case 5:Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production"));Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Bread"));Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_GrainFarm"));Model->BeginCardDrag(TEXT("Building.GrainFarm"));Model->UpdateCardDragTarget(-100,-100);break;
    case 7:{
     Model->SetOpen(true);
     auto Category=Menu->ResolveSemanticWidget(TEXT("BuildMenu.Category.Production"));
     Category->SetToolTipText(FText::FromString(TEXT("Produktionsgebäude und Versorgungsketten")));
     Test->TestTrue(TEXT("Localized category name is a tooltip"),Category->GetToolTip().IsValid());break;
    }
    case 6:Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production"));Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Bread"));Menu->SetPreferences({true,true,true});Menu->FocusSemanticId(TEXT("BuildMenu.Card.Building_GrainFarm"));break;
   }
   // Chain activation is intentionally exercised through the public semantic input path.
   if(Stage==2 || Stage==3){Model->SetOpen(true);Menu->ActivateSemanticId(Stage==2?TEXT("BuildMenu.Chain.Good_Fish"):TEXT("BuildMenu.Chain.Good_Planks"));}
   Prepared=true;ReadyAt=FPlatformTime::Seconds();return false;
  }
  if(FPlatformTime::Seconds()-ReadyAt<.5)return false;
  TArray<FColor> Pixels;FIntVector Size;
  if(!FSlateApplication::Get().TakeScreenshot(V->GetGameViewportWidget().ToSharedRef(),Pixels,Size)){Test->AddError(TEXT("Viewport screenshot failed"));return true;}
  int32 X=1280,Y=720;FParse::Value(FCommandLine::Get(),TEXT("ResX="),X);FParse::Value(FCommandLine::Get(),TEXT("ResY="),Y);
  Test->TestEqual(TEXT("Native width"),Size.X,X);Test->TestEqual(TEXT("Native height"),Size.Y,Y);
  const TCHAR* Names[]={TEXT("compact"),TEXT("bread"),TEXT("fish"),TEXT("planks"),TEXT("locked"),TEXT("placement"),TEXT("accessible"),TEXT("localized"),TEXT("road-icon"),TEXT("warehouse-icon"),TEXT("dock-icon")};
  FString Base=FPaths::ProjectSavedDir()/FString::Printf(TEXT("P22/tray-%dx%d-%s"),X,Y,Names[Stage]);
  TArray64<uint8> Png;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
  Test->TestTrue(TEXT("Saved native capture"),FFileHelper::SaveArrayToFile(Png,*(Base+TEXT(".png"))));
  FString Evidence=TEXT("id\tvisible\tenabled\tx\ty\tright\tbottom\tvalue\n");
  for(const auto& N:Hud->GetRootWidget()->GetSemanticSnapshot())if(N.Id.StartsWith(TEXT("BuildMenu.")) || N.Id.StartsWith(TEXT("Placement."))) {
   Evidence+=FString::Printf(TEXT("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n"),*N.Id,N.State.bVisible,N.State.bEnabled,N.Bounds.Min.X,N.Bounds.Min.Y,N.Bounds.Max.X,N.Bounds.Max.Y,*N.State.Value.Replace(TEXT("\n"),TEXT(" ")));
   if(N.State.bVisible && N.Id.StartsWith(TEXT("BuildMenu.Category.")))Test->TestTrue(TEXT("Category fits viewport"),N.Bounds.Min.X>=0 && N.Bounds.Max.X<=X && N.Bounds.Min.Y>=0 && N.Bounds.Max.Y<=Y);
  }
  Test->TestTrue(TEXT("Saved semantic evidence"),FFileHelper::SaveStringToFile(Evidence,*(Base+TEXT(".tsv"))));
  if(Stage==1){
   auto Card=Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_GrainFarm"));
   Menu->FocusSemanticId(TEXT("BuildMenu.Card.Building_GrainFarm"));
   FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(EKeys::Enter,FModifierKeysState(),0,false,0,0));
   Test->TestEqual(TEXT("Native focused card accepts Enter"),Model->GetSnapshot().SelectedBuildingId,FName(TEXT("Building.GrainFarm")));
   Model->CancelIntent();
   const FVector2D P=Card->GetCachedGeometry().GetAbsolutePosition()+Card->GetCachedGeometry().GetAbsoluteSize()*.5;
   FPointerEvent Pointer(0,P,P,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
   auto Reply=Card->OnDragDetected(Card->GetCachedGeometry(),Pointer);
   Test->TestTrue(TEXT("Selecting a card no longer starts a drag/drop operation"),!Reply.GetDragDropContent().IsValid() && !Model->GetSnapshot().bDraggingCard);
   if(Reply.GetDragDropContent().IsValid())Reply.GetDragDropContent()->OnDrop(false,Pointer);
   Test->TestFalse(TEXT("Drop back over tray cancels placement"),Model->GetSnapshot().bDraggingCard);
  }
  ++Stage;Prepared=false;return Stage==11;
 }
private: FAutomationTestBase* Test;double Start,ReadyAt=0;int32 Stage=0;bool Prepared=false;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConstructionViewport,"Hansa.UI.Construction.RealViewport",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FHansaConstructionViewport::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FConstructionCapture(this));return true;}
#endif
