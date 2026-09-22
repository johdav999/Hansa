#include "UI/SHansaMinimap.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaUiComponents.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
namespace Hansa::UI {
class SHansaMapImage final : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SHansaMapImage):_Controller(nullptr){}
 SLATE_ARGUMENT(AHansaStrategyPlayerController*,Controller)
 SLATE_END_ARGS()
 void Construct(const FArguments& A){
  Controller=A._Controller;
  SetToolTipText(NSLOCTEXT("HansaMinimap","Tip","City map: click to move camera. Arrow keys pan; + and - zoom the map."));
  RegisterActiveTimer(.5f,FWidgetActiveTimerDelegate::CreateSP(this,&SHansaMapImage::Refresh));
 }
 ~SHansaMapImage(){if(Capture.IsValid())Capture->Destroy();}
 virtual bool SupportsKeyboardFocus()const override{return true;}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(240);}
 void Zoom(float Factor){ZoomFactor=FMath::Clamp(ZoomFactor*Factor,1.f,8.f);Refresh(0,0);}
 void CenterCamera(){
  if(auto* P=Pawn())P->FocusWorldLocationIntent(FVector(Center.X,Center.Y,0));
 }
 void Toggle(){bShowView=!bShowView;Invalidate(EInvalidateWidgetReason::Paint);}
 AHansaStrategyCameraPawn* Pawn()const{return Controller.IsValid()?Cast<AHansaStrategyCameraPawn>(Controller->GetPawn()):nullptr;}
 EActiveTimerReturnType Refresh(double,float){
  auto* P=Pawn(); if(!P || !Controller->GetWorld())return EActiveTimerReturnType::Continue;
  const auto Min=P->GetViewBoundsMin(),Max=P->GetViewBoundsMax();
  Span=FMath::Max(Max.X-Min.X,Max.Y-Min.Y)/ZoomFactor;
  Center=ZoomFactor>1?P->GetFocusLocation2D():(Min+Max)*.5;
  if(!Capture.IsValid()){
   FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
   auto* Actor=Controller->GetWorld()->SpawnActor<ASceneCapture2D>(Params);
   if(!Actor)return EActiveTimerReturnType::Continue;
   Capture=Actor;
   auto* C=Actor->GetCaptureComponent2D();
   auto* Target=NewObject<UTextureRenderTarget2D>(C);
   Target->RenderTargetFormat=RTF_RGBA8;Target->ClearColor=FLinearColor(.02,.05,.07);Target->InitAutoFormat(512,512);
   C->TextureTarget=Target;C->ProjectionType=ECameraProjectionMode::Orthographic;
   C->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
   C->bCaptureEveryFrame=false;C->bCaptureOnMovement=false;
   C->ShowFlags.SetLighting(false);C->ShowFlags.SetPostProcessing(false);C->ShowFlags.SetBloom(false);C->ShowFlags.SetMotionBlur(false);C->ShowFlags.SetAtmosphere(false);C->ShowFlags.SetFog(false);
   C->PostProcessBlendWeight=0;
   Brush.SetResourceObject(Target);Brush.ImageSize=FVector2D(512);Brush.DrawAs=ESlateBrushDrawType::Image;
  }
  Capture->SetActorLocationAndRotation(FVector(Center.X,Center.Y,80000),FRotator(-90,0,0));
  Capture->GetCaptureComponent2D()->OrthoWidth=Span;

  Capture->GetCaptureComponent2D()->CaptureScene();
  Invalidate(EInvalidateWidgetReason::Paint);
  return EActiveTimerReturnType::Continue;
 }
 virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)override {
  if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
  if(auto* P=Pawn()){
   const FVector2D N=G.AbsoluteToLocal(E.GetScreenSpacePosition())/G.GetLocalSize();
   P->FocusWorldLocationIntent(FVector(Center.X+(.5-N.Y)*Span,Center.Y+(N.X-.5)*Span,0));
  }
  return FReply::Handled().SetUserFocus(SharedThis(this),EFocusCause::Mouse);
 }
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent& E)override{Zoom(E.GetWheelDelta()>0?1.25f:.8f);return FReply::Handled();}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& E)override{
  auto* P=Pawn();if(!P)return FReply::Unhandled();
  const FKey K=E.GetKey();FVector2D Delta(0,0);
  if(K==EKeys::Up||K==EKeys::Gamepad_DPad_Up)Delta.X=1;
  else if(K==EKeys::Down||K==EKeys::Gamepad_DPad_Down)Delta.X=-1;
  else if(K==EKeys::Left||K==EKeys::Gamepad_DPad_Left)Delta.Y=-1;
  else if(K==EKeys::Right||K==EKeys::Gamepad_DPad_Right)Delta.Y=1;
  else if(K==EKeys::Add||K==EKeys::Equals){Zoom(1.25);return FReply::Handled();}
  else if(K==EKeys::Subtract||K==EKeys::Hyphen){Zoom(.8);return FReply::Handled();}
  else return FReply::Unhandled();
  const FVector2D Focus=P->GetFocusLocation2D()+Delta*Span*.05;
  P->FocusWorldLocationIntent(FVector(Focus.X,Focus.Y,0));return FReply::Handled();
 }
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override{
  FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),Brush.GetResourceObject()?&Brush:FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,Brush.GetResourceObject()?FLinearColor::White:FLinearColor(.03,.06,.08));
  if(auto* P=Pawn();P && bShowView && Span>0){
   auto ToMap=[&](FVector2D W){return FVector2D(.5+(W.Y-Center.Y)/Span,.5-(W.X-Center.X)/Span)*G.GetLocalSize();};
   const FVector2D Pos=ToMap(P->GetFocusLocation2D());
   TArray<FVector2D> Mark={{Pos.X-5,Pos.Y},{Pos.X+5,Pos.Y},{Pos.X,Pos.Y},{Pos.X,Pos.Y-5},{Pos.X,Pos.Y+5}};
   FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Mark,ESlateDrawEffect::None,FLinearColor(1,.8,.4),true,2);
   if(Controller.IsValid()){
    int32 W,H;Controller->GetViewportSize(W,H);
    TArray<FVector2D> Corners;
    for(const FVector2D Screen:{FVector2D(0,0),FVector2D(W,0),FVector2D(W,H),FVector2D(0,H)}){
     FVector Origin,Dir;
     if(Controller->DeprojectScreenPositionToWorld(Screen.X,Screen.Y,Origin,Dir)&&Dir.Z<-.001){
      FVector Hit=Origin+Dir*(-Origin.Z/Dir.Z);Corners.Add(ToMap(FVector2D(Hit)));
     }
    }
    if(Corners.Num()==4){const FVector2D First= Corners[0];Corners.Add(First);FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),Corners,ESlateDrawEffect::None,FLinearColor(.9,.8,.55),true,1);}
   }
  }
  if(HasKeyboardFocus()){
   const FVector2D Z=G.GetLocalSize();TArray<FVector2D> Ring={{2,2},{Z.X-2,2},{Z.X-2,Z.Y-2},{2,Z.Y-2},{2,2}};
   FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Ring,ESlateDrawEffect::None,FLinearColor(1,.85,.45),true,3);
  }
  return Layer+2;
 }
private:
 TWeakObjectPtr<AHansaStrategyPlayerController> Controller;
 TWeakObjectPtr<ASceneCapture2D> Capture;
 FSlateBrush Brush;
 FVector2D Center=FVector2D::ZeroVector;
 float Span=24000,ZoomFactor=1;bool bShowView=true;
};
void SHansaMinimap::Construct(const FArguments& Args){
 TSharedPtr<SHansaMapImage> Map;
 auto Rail=SNew(SVerticalBox);
 ChildSlot[SNew(SHansaReferenceFrame).Padding(7)
 [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
  [SNew(SBox).WidthOverride(Args._MapSize).HeightOverride(Args._MapSize)[SAssignNew(Map,SHansaMapImage).Controller(Args._Controller).Clipping(EWidgetClipping::ClipToBounds)]]
  +SHorizontalBox::Slot().AutoWidth().Padding(5,0)[Rail]]];
 Targets.Add(TEXT("HUD.Minimap"),Map);
 auto Add=[&](const TCHAR* Id,EUiGlyph Glyph,FText Tip,TFunction<void()> Action){
  TSharedPtr<SHansaAction> B;
  Rail->AddSlot().FillHeight(1)[SAssignNew(B,SHansaAction).Compact(true).Kind(EHansaUiButtonStyle::Icon).Reason(Tip)
   .OnClicked_Lambda([Action]{Action();return FReply::Handled();})[SNew(SHansaGlyph).Glyph(Glyph).Size(24)]];
  Targets.Add(Id,B); Actions.Add(Id,Action);
 };
 Add(TEXT("HUD.Minimap.ZoomIn"),EUiGlyph::Plus,NSLOCTEXT("HansaMinimap","In","Zoom map in"),[Map]{Map->Zoom(1.25);});
 Add(TEXT("HUD.Minimap.ZoomOut"),EUiGlyph::Minus,NSLOCTEXT("HansaMinimap","Out","Zoom map out"),[Map]{Map->Zoom(.8);});
 Add(TEXT("HUD.Minimap.Overlay"),EUiGlyph::Eye,NSLOCTEXT("HansaMinimap","Overlay","Toggle camera footprint"),[Map]{Map->Toggle();});
 Add(TEXT("HUD.Minimap.Center"),EUiGlyph::Map,NSLOCTEXT("HansaMinimap","Center","Center camera on map"),[Map]{Map->CenterCamera();});
}
SHansaMinimap::~SHansaMinimap(){}
TSharedPtr<SWidget> SHansaMinimap::Resolve(const FString& Id)const{const auto* W=Targets.Find(Id);return W?*W:nullptr;}
bool SHansaMinimap::Activate(const FString& Id){if(auto* Action=Actions.Find(Id)){(*Action)();return true;}return false;}
}
