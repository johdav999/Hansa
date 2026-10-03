#include "UI/SHansaMinimap.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaUiComponents.h"
#include "UI/SHansaLandOverlay.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaRootHud.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Trade/TradeArtwork.h"
#include "World/HansaTradeStationPresentation.h"
#include "World/HansaLandOverlayRenderer.h"
#include "EngineUtils.h"
#include "Rendering/SlateRenderer.h"
#include "World/HansaStrategyPlayerController.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "LandscapeProxy.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
namespace Hansa::UI {
namespace {
#include "HansaMinimapCityLocations.inl"
}
class SHansaMapImage final : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SHansaMapImage):_Controller(nullptr){}
 SLATE_ARGUMENT(AHansaStrategyPlayerController*,Controller)
 SLATE_ARGUMENT(TSharedPtr<FHansaLandOverlayUiState>,LandState)
 SLATE_END_ARGS()
 void Construct(const FArguments& A){
  Controller=A._Controller;LandState=A._LandState;
  auto Tip=[this]{return HoveredCityName.IsEmpty()?NSLOCTEXT("HansaMinimap","Tip","Map: click to move camera. Arrow keys pan; + and - zoom the map."):HoveredCityName;};
  SetToolTip(SNew(SToolTip).Text_Lambda(Tip).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush"))).TextMargin(0)
   [SNew(SHansaReferenceFrame).Padding(12)
    [SNew(STextBlock).Text_Lambda(Tip).Font(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Body))
     .ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk)).WrapTextAt(280)]]);
  TradeIconArtwork(TEXT("CityMarker"),14);TradeIconArtwork(TEXT("CityMarkerSelected"),14);
  RegisterActiveTimer(.5f,FWidgetActiveTimerDelegate::CreateSP(this,&SHansaMapImage::Refresh));
 }
 ~SHansaMapImage(){if(Capture.IsValid())Capture->Destroy();if(StreamingActor.IsValid())StreamingActor->Destroy();}
 virtual bool SupportsKeyboardFocus()const override{return true;}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(240);}
 void Zoom(float Factor){ZoomFactor=FMath::Clamp(ZoomFactor*Factor,1.f,bCampaignMap?1024.f:8.f);Refresh(0,0);}
 void CenterCamera(){
  if(auto* P=Pawn()){
   const FVector2D Home=(P->GetViewBoundsMin()+P->GetViewBoundsMax())*.5;
   P->FocusWorldLocationIntent(FVector(Home.X,Home.Y,0));
  }
 }
 void Toggle(){bShowView=!bShowView;Invalidate(EInvalidateWidgetReason::Paint);}
 AHansaStrategyCameraPawn* Pawn()const{return Controller.IsValid()?Cast<AHansaStrategyCameraPawn>(Controller->GetPawn()):nullptr;}
 FVector2D WorldToMap(FVector2D World)const{
  if(bCampaignMap)return FVector2D(.5+(World.X-Center.X)/Span,.5+(World.Y-Center.Y)/Span);
  return Hansa::Game::LandOverlay::WorldToLandMap(World,Center,Span);
 }
 FVector2D MapToWorld(FVector2D Map)const{
  if(bCampaignMap)return Center+(Map-FVector2D(.5,.5))*Span;
  return Hansa::Game::LandOverlay::LandMapToWorld(Map,Center,Span);
 }
 EActiveTimerReturnType Refresh(double,float){
  auto* P=Pawn(); if(!P || !Controller->GetWorld())return EActiveTimerReturnType::Continue;
  bCampaignMap=Hansa::Game::LubeckPlacementGrid::IsCampaignWorld(Controller->GetWorld());
  // City view bounds are camera reference areas, not the campaign's map extent.
  // The promoted campaign loads its whole Landscape; cache its authored bounds.
  if(bCampaignMap && !CampaignBounds.bIsValid){
   for(TActorIterator<ALandscapeProxy> It(Controller->GetWorld());It;++It){
    const FBox Bounds=It->GetComponentsBoundingBox(true);
    if(Bounds.IsValid){CampaignBounds+=FVector2D(Bounds.Min);CampaignBounds+=FVector2D(Bounds.Max);}
   }
  }
  const auto Min=bCampaignMap&&CampaignBounds.bIsValid?CampaignBounds.Min:P->GetViewBoundsMin();
  const auto Max=bCampaignMap&&CampaignBounds.bIsValid?CampaignBounds.Max:P->GetViewBoundsMax();
  // Leave a small border so the outermost coast/terrain remains inside the frame.
  Span=FMath::Max(Max.X-Min.X,Max.Y-Min.Y)*1.08f/ZoomFactor;
  const FVector2D Focus=P->GetFocusLocation2D();
  Center=ZoomFactor>1?Focus:(Min+Max)*.5;
  if(!bCampaignMap && (FMath::Abs(Focus.X-Center.X)>Span*.5f||FMath::Abs(Focus.Y-Center.Y)>Span*.5f))Center=Focus;
  Cities.Reset();
  if(bCampaignMap)if(auto* Hud=Cast<AHansaRootHud>(Controller->GetHUD()))if(auto* Model=Hud->GetTradeMapPresentationModel())
   for(const auto& City:Model->GetMapCities())if(const auto* Location=CampaignCityLocations.Find(City.StableId)){
    FVector2D Position=*Location;
    // Gameplay centres were deliberately moved to the coast after terrain authoring.
    if(City.StableId==TEXT("City.Lubeck"))Position=FVector2D(Hansa::Game::LubeckPlacementGrid::CampaignLubeckCenter());
    if(City.StableId==TEXT("City.Rostock"))Position=FVector2D(AHansaTradeStationPresentation::SiteTransform(Controller->GetWorld()).TransformPosition(FVector(60000,0,100)));
    Cities.Add({City.StableId,City.Label,Position});
   }
  if(bPointerOver)UpdateHoveredCity(GetCachedGeometry());
  // A scene capture is not a World Partition streaming source. Without one it
  // renders only the cells near the player's camera, leaving most of the map black.
  if(!StreamingActor.IsValid()){
   FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
   AActor* Actor=Controller->GetWorld()->SpawnActor<AActor>(Params);
   if(Actor){
    StreamingActor=Actor;
    USceneComponent* Root=NewObject<USceneComponent>(Actor);
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();
    Actor->SetActorLocation(FVector(Center.X,Center.Y,0));
    StreamingSource=NewObject<UWorldPartitionStreamingSourceComponent>(Actor);
    StreamingSource->TargetState=EStreamingSourceTargetState::Activated;
    StreamingSource->Priority=EStreamingSourcePriority::Low;
    FStreamingSourceShape Shape;
    Shape.bUseGridLoadingRange=false;
    Shape.Radius=bCampaignMap?Span*.707107f+1000.f:FMath::Min(Span*.707107f+1000.f,40000.f);
    StreamingSource->Shapes.Add(Shape);
    StreamingSource->RegisterComponent();
    StreamingSource->EnableStreamingSource();
   }
  }
  if(StreamingActor.IsValid() && StreamingSource.IsValid()){
   StreamingActor->SetActorLocation(FVector(Center.X,Center.Y,0));
   FStreamingSourceShape Shape;
   Shape.bUseGridLoadingRange=false;
   Shape.Radius=bCampaignMap?Span*.707107f+1000.f:FMath::Min(Span*.707107f+1000.f,40000.f);
   StreamingSource->Shapes.Reset(1);
   StreamingSource->Shapes.Add(Shape);
  }
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
   C->ShowFlags.SetLighting(true);C->ShowFlags.SetPostProcessing(true);C->ShowFlags.SetBloom(false);C->ShowFlags.SetMotionBlur(false);C->ShowFlags.SetAtmosphere(false);C->ShowFlags.SetFog(false);
   C->PostProcessBlendWeight=1;
   C->bAlwaysPersistRenderingState=true;
   Brush.SetResourceObject(Target);Brush.ImageSize=FVector2D(512);Brush.DrawAs=ESlateBrushDrawType::Image;
  }
  // Campaign axes are X east / Y south. A -90 yaw puts north at the top.
  Capture->SetActorLocationAndRotation(FVector(Center.X,Center.Y,80000),FRotator(-90,bCampaignMap?-90:0,0));
  Capture->GetCaptureComponent2D()->OrthoWidth=Span;

  auto* CaptureComponent=Capture->GetCaptureComponent2D();
  if(P->Camera)CaptureComponent->PostProcessSettings=P->Camera->PostProcessSettings;
  CaptureComponent->HiddenActors.Reset();
  for(TActorIterator<AHansaLandOverlayRenderer> It(Controller->GetWorld());It;++It)CaptureComponent->HiddenActors.Add(*It);
  CaptureComponent->CaptureScene();
  if(LandState.IsValid() && LandState->GetSurvey().Get().StateRevision!=ContourRevision)
  {
   Contours.Reset();ContourRevision=LandState->GetSurvey().Get().StateRevision;
   TMap<int32,TArray<Hansa::Simulation::FHansaLandCellView>> Regions;
   for(const auto& Run:LandState->GetSurvey().Get().Cells)if(Run.RegionId>0)Regions.FindOrAdd(Run.RegionId).Add(Run);
   for(const auto& Region:Regions)Contours.Add(Region.Key,Hansa::Game::LandOverlay::SurveySelectionBoundaries(Region.Value,Region.Key));
  }
  Invalidate(EInvalidateWidgetReason::Paint);
  return EActiveTimerReturnType::Continue;
 }
 virtual FReply OnMouseMove(const FGeometry& G,const FPointerEvent& E)override{
  bPointerOver=true;HoverScreenPosition=E.GetScreenSpacePosition();UpdateHoveredCity(G);return FReply::Unhandled();
 }
 virtual void OnMouseLeave(const FPointerEvent& E)override{
  SLeafWidget::OnMouseLeave(E);bPointerOver=false;HoveredCity=NAME_None;HoveredCityName=FText();Invalidate(EInvalidateWidgetReason::Paint);
 }
 virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)override {
  if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
  if(auto* P=Pawn()){
   const FVector2D N=G.AbsoluteToLocal(E.GetScreenSpacePosition())/G.GetLocalSize();
   P->FocusWorldLocationIntent(FVector(MapToWorld(N),0));
  }
  return FReply::Handled().SetUserFocus(SharedThis(this),EFocusCause::Mouse);
 }
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent& E)override{Zoom(E.GetWheelDelta()>0?1.25f:.8f);return FReply::Handled();}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& E)override{
  auto* P=Pawn();if(!P)return FReply::Unhandled();
  const FKey K=E.GetKey();FVector2D Delta(0,0);
  if(K==EKeys::Up||K==EKeys::Gamepad_DPad_Up)Delta=bCampaignMap?FVector2D(0,-1):FVector2D(1,0);
  else if(K==EKeys::Down||K==EKeys::Gamepad_DPad_Down)Delta=bCampaignMap?FVector2D(0,1):FVector2D(-1,0);
  else if(K==EKeys::Left||K==EKeys::Gamepad_DPad_Left)Delta=bCampaignMap?FVector2D(-1,0):FVector2D(0,-1);
  else if(K==EKeys::Right||K==EKeys::Gamepad_DPad_Right)Delta=bCampaignMap?FVector2D(1,0):FVector2D(0,1);
  else if(K==EKeys::Add||K==EKeys::Equals){Zoom(1.25);return FReply::Handled();}
  else if(K==EKeys::Subtract||K==EKeys::Hyphen){Zoom(.8);return FReply::Handled();}
  else return FReply::Unhandled();
  const FVector2D Focus=P->GetFocusLocation2D()+Delta*Span*.05;
  P->FocusWorldLocationIntent(FVector(Focus.X,Focus.Y,0));return FReply::Handled();
 }
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const override{
  FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),Brush.GetResourceObject()?&Brush:FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")),ESlateDrawEffect::None,Brush.GetResourceObject()?FLinearColor::White:FLinearColor(.03,.06,.08));
  using namespace Hansa::Game::LandOverlay;
  auto ToMap=[&](FVector2D W){return WorldToMap(W)*G.GetLocalSize();};
  if(LandState.IsValid() && LandState->IsActive() && LandState->GetSurvey().IsReady() && Span>0)
  {
   const FTransform& Grid=LandState->GetGridTransform();
   auto Point=[&](FVector2D P){return ToMap(FVector2D(Grid.TransformPosition(FVector(P,0))));};
   TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
   auto Flush=[&]{if(!Indices.IsEmpty())FSlateDrawElement::MakeCustomVerts(Out,Layer+1,
    FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))),Vertices,Indices,nullptr,0,0);Vertices.Reset();Indices.Reset();};
   for(const auto& Run:LandState->GetSurvey().Get().Cells)
   {
    const auto Style=Classify(Run,LandState->GetMode());if(Style.Surface==ESurface::Hidden)continue;
    EHansaUiColorToken Token=EHansaUiColorToken::ProsperityTeal;
    if(Style.Surface==ESurface::Conditional)Token=EHansaUiColorToken::WarningAmber;
    if(Style.Surface==ESurface::Restricted)Token=EHansaUiColorToken::Oxblood;
    if(Style.Surface==ESurface::Unknown)Token=EHansaUiColorToken::HarborSlate;
    if(Style.Surface==ESurface::Ownership)Token=EHansaUiColorToken::HarborSlate;
    FLinearColor Color=UHansaUiStyleLibrary::GetColor(Token);Color.A=LandState->bHighContrast?.65f:.38f;
    const FVector2D A(Run.Coordinate.X,Run.Coordinate.Y),B=A+FVector2D(1,Run.RunLength);
    const FVector2D Corners[]={Point(A),Point({B.X,A.Y}),Point(B),Point({A.X,B.Y})};
    FBox2D Bounds(ForceInit);for(const auto& C:Corners)Bounds+=C;
    if(!Bounds.Intersect(FBox2D(FVector2D::ZeroVector,G.GetLocalSize())))continue;
    if(Vertices.Num()>60000)Flush();const SlateIndex N=Vertices.Num();
    for(const auto& C:Corners)Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(C),FVector2f::ZeroVector,Color.ToFColor(true)));
    Indices.Append({N,SlateIndex(N+1),SlateIndex(N+2),N,SlateIndex(N+2),SlateIndex(N+3)});
    // Non-color cue for restrictions/unknown and recorded ownership.
    if(Style.Surface!=ESurface::Permitted && Run.Coordinate.X%4==0 && Bounds.GetSize().GetMin()>3)
    { TArray<FVector2D> Slash={Corners[0],Corners[2]};FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Slash,ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen).CopyWithNewOpacity(.45),true,1); }
   }
   Flush();
   for(const auto& Region:Contours)for(const auto& Boundary:Region.Value)
   {
    TArray<FVector2D> Path;for(const auto& P:Boundary.Points)Path.Add(Point(FVector2D(P)));
    const bool Selected=Region.Key==LandState->GetSelectedRegion();
    FSlateDrawElement::MakeLines(Out,Layer+(Selected?4:3),G.ToPaintGeometry(),Path,ESlateDrawEffect::None,
     UHansaUiStyleLibrary::GetColor(Selected?EHansaUiColorToken::Brass:EHansaUiColorToken::Chalk).CopyWithNewOpacity(Selected?1.f:.55f),true,Selected?2.5f:1.f);
   }
  }
  if(auto* P=Pawn();P && bShowView && Span>0){

   const FVector2D Pos=ToMap(P->GetFocusLocation2D());
   TArray<FVector2D> Mark={{Pos.X-5,Pos.Y},{Pos.X+5,Pos.Y},{Pos.X,Pos.Y},{Pos.X,Pos.Y-5},{Pos.X,Pos.Y+5}};
   FSlateDrawElement::MakeLines(Out,Layer+7,G.ToPaintGeometry(),Mark,ESlateDrawEffect::None,FLinearColor(1,.8,.4),true,2);
   if(Controller.IsValid()){
    TArray<FVector2D> Corners;
    const auto Footprint=LandState.IsValid()?LandState->GetCameraFootprint():CameraGroundFootprint(Controller.Get(),0);
    for(const auto& Pnt:Footprint)Corners.Add(ToMap(FVector2D(Pnt)));
    if(Corners.Num()>=3){const FVector2D First= Corners[0];Corners.Add(First);FSlateDrawElement::MakeLines(Out,Layer+7,G.ToPaintGeometry(),Corners,ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen),true,1);}

   }
  }
  // Geographic point overlays stay a readable fixed screen size at every zoom.
  for(const auto& City:Cities){
   const FVector2D N=WorldToMap(City.Position);
   if(N.X<0||N.X>1||N.Y<0||N.Y>1)continue;
   const FVector2D Pos=N*G.GetLocalSize();const float D=City.Id==HoveredCity?14.f:10.f;
   FSlateDrawElement::MakeBox(Out,Layer+6,G.ToPaintGeometry(FVector2f(D,D),FSlateLayoutTransform(FVector2f(Pos-FVector2D(D*.5)))),
    TradeIconArtwork(City.Id==HoveredCity?TEXT("CityMarkerSelected"):TEXT("CityMarker"),FMath::CeilToInt(D*G.GetAccumulatedLayoutTransform().GetScale())));
  }
  if(HasKeyboardFocus()){
   const FVector2D Z=G.GetLocalSize();TArray<FVector2D> Ring={{2,2},{Z.X-2,2},{Z.X-2,Z.Y-2},{2,Z.Y-2},{2,2}};
   FSlateDrawElement::MakeLines(Out,Layer+8,G.ToPaintGeometry(),Ring,ESlateDrawEffect::None,FLinearColor(1,.85,.45),true,3);
  }
  return Layer+8;
 }
private:
 struct FCityMarker { FName Id;FText Label;FVector2D Position; };
 TArray<FCityMarker> Cities;
 FName HoveredCity;
 FText HoveredCityName;
 FVector2D HoverScreenPosition=FVector2D::ZeroVector;
 bool bPointerOver=false;
 void UpdateHoveredCity(const FGeometry& G){
  const FVector2D Pointer=G.AbsoluteToLocal(HoverScreenPosition),Size=G.GetLocalSize();
  FName Next;FText Name;double Nearest=100.;
  if(Span>0&&Pointer.X>=0&&Pointer.Y>=0&&Pointer.X<=Size.X&&Pointer.Y<=Size.Y)
   for(const auto& City:Cities){
    const FVector2D N=WorldToMap(City.Position);if(N.X<0||N.X>1||N.Y<0||N.Y>1)continue;
    const double Distance=(Pointer-N*Size).SizeSquared();
    if(Distance<Nearest){Nearest=Distance;Next=City.Id;Name=City.Label;}
   }
  if(Next!=HoveredCity){HoveredCity=Next;HoveredCityName=Name;Invalidate(EInvalidateWidgetReason::Paint);}
 }
 TWeakObjectPtr<AHansaStrategyPlayerController> Controller;
 TWeakObjectPtr<ASceneCapture2D> Capture;
 TWeakObjectPtr<AActor> StreamingActor;
 TWeakObjectPtr<UWorldPartitionStreamingSourceComponent> StreamingSource;
 TSharedPtr<FHansaLandOverlayUiState> LandState;
 uint64 ContourRevision=0;
 TMap<int32,TArray<Hansa::Game::LandOverlay::FBoundary>> Contours;
 FSlateBrush Brush;
 FVector2D Center=FVector2D::ZeroVector;
 FBox2D CampaignBounds=FBox2D(ForceInit);
 bool bCampaignMap=false;
 float Span=24000,ZoomFactor=1;bool bShowView=true;
};
void SHansaMinimap::Construct(const FArguments& Args){
 LandState=Args._LandState.IsValid()?Args._LandState:MakeShared<FHansaLandOverlayUiState>(Args._Controller);
 TSharedPtr<SHansaMapImage> Map;
 auto Rail=SNew(SVerticalBox);
 ChildSlot[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[SAssignNew(LandPanel,SHansaLandPanel).State(LandState).Preferences(Args._Preferences).OnClosed_Lambda([this]{if(LandButton.IsValid()&&FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(LandButton,EFocusCause::SetDirectly);}).Visibility_Lambda([State=LandState]{return State->IsPanelOpen()?EVisibility::Visible:EVisibility::Collapsed;})]
 +SVerticalBox::Slot().AutoHeight()[SNew(SHansaReferenceFrame).Padding(7)
 [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
  [SNew(SBox).WidthOverride(Args._MapSize).HeightOverride(Args._MapSize)[SAssignNew(Map,SHansaMapImage).Controller(Args._Controller).LandState(LandState).Clipping(EWidgetClipping::ClipToBounds)]]
  +SHorizontalBox::Slot().AutoWidth().Padding(5,0)[Rail]]]];
 Targets.Add(TEXT("HUD.Minimap"),Map);
 auto Add=[&](const TCHAR* Id,EUiGlyph Glyph,FText Tip,TFunction<void()> Action){
  TSharedPtr<SHansaAction> B;
  Rail->AddSlot().FillHeight(1)[SAssignNew(B,SHansaAction).Compact(true).Kind(EHansaUiButtonStyle::Icon).Reason(Tip)
   .OnClicked_Lambda([Action]{Action();return FReply::Handled();})[SNew(SHansaGlyph).Glyph(Glyph).Size(24)]];
  Targets.Add(Id,B); Actions.Add(Id,Action);
 };
 Add(TEXT("HUD.Minimap.ZoomIn"),EUiGlyph::ZoomIn,NSLOCTEXT("HansaMinimap","In","Zoom map in"),[Map]{Map->Zoom(1.25);});
 Add(TEXT("HUD.Minimap.ZoomOut"),EUiGlyph::ZoomOut,NSLOCTEXT("HansaMinimap","Out","Zoom map out"),[Map]{Map->Zoom(.8);});
 Add(TEXT("HUD.Minimap.Overlay"),EUiGlyph::Eye,NSLOCTEXT("HansaMinimap","Overlay","Toggle camera footprint"),[Map]{Map->Toggle();});
 Add(TEXT("HUD.Minimap.Center"),EUiGlyph::CenterMap,NSLOCTEXT("HansaMinimap","Center","Center camera on map"),[Map]{Map->CenterCamera();});
 Rail->AddSlot().FillHeight(1)[SAssignNew(LandButton,SHansaAction).Compact(true).Kind(EHansaUiButtonStyle::Icon).Preferences(Args._Preferences)
  .Label(NSLOCTEXT("HansaMinimap","Land","Land")).Reason(NSLOCTEXT("HansaMinimap","LandTip","Show construction permission and land ownership."))
  .OnClicked_Lambda([State=LandState]{State->ToggleLand();return FReply::Handled();})[SNew(SHansaGlyph).Glyph(EUiGlyph::Map).Size(24)]];
 Targets.Add(TEXT("HUD.Minimap.Land"),LandButton);
 Actions.Add(TEXT("HUD.Minimap.Land"),[State=LandState]{State->ToggleLand();});
 RegisterActiveTimer(.2f,FWidgetActiveTimerDelegate::CreateLambda([this](double,float)
 {
  if(!LandButton.IsValid()||!LandState.IsValid())return EActiveTimerReturnType::Stop;
  LandButton->SetState(LandState->IsActive()?EUiState::Selected:EUiState::Default,
   NSLOCTEXT("HansaMinimap","LandTip","Show construction permission and land ownership."));
  return EActiveTimerReturnType::Continue;
 }));
}
SHansaMinimap::~SHansaMinimap(){}
TSharedPtr<SWidget> SHansaMinimap::Resolve(const FString& Id)const{if(Id.StartsWith(TEXT("LandOverlay."))&&LandPanel)return LandPanel->Resolve(Id);const auto* W=Targets.Find(Id);return W?*W:nullptr;}
bool SHansaMinimap::Activate(const FString& Id){if(Id.StartsWith(TEXT("LandOverlay."))&&LandPanel)return LandPanel->Activate(Id);if(auto* Action=Actions.Find(Id)){(*Action)();return true;}return false;}
}
