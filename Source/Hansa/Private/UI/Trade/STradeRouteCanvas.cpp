#include "STradeRouteCanvas.h"
#include "Placement/HansaRostockPlacement.h"
#include "TradeArtwork.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"

#define LOCTEXT_NAMESPACE "HansaRegionalMap"
namespace Hansa::UI
{
namespace {
#include "../HansaTradeChartData.inl"
}
void STradeRouteCanvas::Construct(const FArguments& A)
{
 Preferences=A._Preferences;Model=A._Model;TradeArtwork(TEXT("Land"),true);TradeArtwork(TEXT("Sea"),true);TradeArtwork(TEXT("CityMarker"));TradeArtwork(TEXT("CityMarkerSelected"));SetClipping(EWidgetClipping::ClipToBounds);
 SetToolTipText(LOCTEXT("Help","Arrows / D-pad: focus city. Enter / A: select. WASD / right stick: pan. +/- / shoulders: zoom. Home / Y: fit. R / X: next route. Tab: leave map. Lines show route connections, not navigable sailing tracks."));
}
const FHansaTradeMapCityPresentation* STradeRouteCanvas::FindCity(FName Id) const { return Snapshot.Cities.FindByPredicate([&](const auto& C){return C.StableId==Id;}); }
void STradeRouteCanvas::SetSnapshot(const FTradeRegionalMapView& In)
{
 const bool ChangedCity=Snapshot.SelectedCityStableId!=In.SelectedCityStableId;

 Snapshot=In;
 if(!FindCity(FocusedCity))FocusedCity=Snapshot.SelectedCityStableId;
 if(ChangedCity)if(const auto* C=FindCity(In.SelectedCityStableId))if(TradeGeometry::IsLocated(C->NormalizedPosition))Camera.Reveal(C->NormalizedPosition,GetCachedGeometry().GetLocalSize());
 Invalidate(EInvalidateWidgetReason::Paint);
}
void STradeRouteCanvas::Tick(const FGeometry& G,double Time,float Delta){
 SLeafWidget::Tick(G,Time,Delta);
 if(!bInitiallyFramed&&G.GetLocalSize().X>100&&G.GetLocalSize().Y>100){const auto Size=G.GetLocalSize();Camera.Zoom=1.55;Camera.Move(Size*.5-Camera.Point(TradeGeometry::Project(16.,56.),Size),Size);bInitiallyFramed=true;Invalidate(EInvalidateWidgetReason::Paint);}
}
bool STradeRouteCanvas::FrameSelection()
{
 const auto Size=GetCachedGeometry().GetLocalSize();
 if(Size.X<=0||Size.Y<=0)return false;
 FVector2D Position=Snapshot.ShipPosition;
 if(!Snapshot.bShipInTransit){const auto* C=FindCity(Snapshot.SelectedCityStableId);if(!C)return false;Position=C->NormalizedPosition;}
 if(!TradeGeometry::IsLocated(Position))return false;
 // Center the projected ship or its known port without stealing directory focus.
 Camera.Move(Size*.5-Camera.Point(Position,Size),Size);
 Invalidate(EInvalidateWidgetReason::Paint);return true;
}
bool STradeRouteCanvas::IsLeaseView() const { return Model.IsValid()&&Model->GetSnapshot().ActiveSection==TEXT("Construction")&&Snapshot.SelectedCityStableId==TEXT("City.Rostock"); }
void STradeRouteCanvas::ChangeZoom(float Delta) { if(IsLeaseView()){LeaseZoom=FMath::Clamp(LeaseZoom+Delta,1.,4.);Invalidate(EInvalidateWidgetReason::Paint);return;} const auto Size=GetCachedGeometry().GetLocalSize(); Camera.ZoomAt(Delta,Size*.5,Size);Invalidate(EInvalidateWidgetReason::Paint); }
void STradeRouteCanvas::ResetView() { LeaseZoom=1;LeasePan=FVector2D::ZeroVector;Camera.Reset();Invalidate(EInvalidateWidgetReason::Paint); }
void STradeRouteCanvas::CycleOverlay() { Overlay=static_cast<ETradeOverlay>((static_cast<int>(Overlay)+1)%4);Invalidate(EInvalidateWidgetReason::Paint); }
void STradeRouteCanvas::CycleThickness() { Thickness=Thickness>=3?1:Thickness+1;Invalidate(EInvalidateWidgetReason::Paint); }
FText STradeRouteCanvas::OverlayLabel() const { return Overlay==ETradeOverlay::Routes?LOCTEXT("Routes","Routes / fleet"):Overlay==ETradeOverlay::Good?LOCTEXT("Goods","Good reports"):Overlay==ETradeOverlay::Presence?LOCTEXT("Presence","Foreign presence"):LOCTEXT("Alerts","Station alerts"); }
FText STradeRouteCanvas::ThicknessLabel() const { return FText::Format(LOCTEXT("Thickness","Lines: {0}×"),FText::AsNumber(Thickness)); }
bool STradeRouteCanvas::FocusCity(FName Id)
{
 const auto* City=FindCity(Id);if(!City)return false;
 FocusedCity=Id;
 if(TradeGeometry::IsLocated(City->NormalizedPosition))
 {
  const auto Size=GetCachedGeometry().GetLocalSize();
  if(Size.Y>0&&Size.Y<240) { Camera.Zoom=FMath::Max(Camera.Zoom,4.);Camera.Move(Size*.5-Camera.Point(City->NormalizedPosition,Size),Size); }
  else Camera.Reveal(City->NormalizedPosition,Size);
 }
 if(auto* P=Model.Get())P->SetFocusedSemanticId(FName(*(FString(TEXT("TradeMap.City."))+Id.ToString().Replace(TEXT("."),TEXT("_")))));
 Invalidate(EInvalidateWidgetReason::Paint);return true;
}
FReply STradeRouteCanvas::OnFocusReceived(const FGeometry&,const FFocusEvent&)
{
 if(auto* P=Model.Get())if(!P->GetSnapshot().FocusedSemanticId.ToString().StartsWith(TEXT("TradeMap.City."))) { FocusCity(Snapshot.SelectedCityStableId);P->SetFocusedSemanticId(TEXT("TradeMap.Chart.CanvasFocus")); }
 Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
}
bool STradeRouteCanvas::NextRoute(int32 Direction)
{
 if(!Model.IsValid()||Snapshot.Routes.IsEmpty())return false;
 const int32 Index=Snapshot.Routes.IndexOfByPredicate([&](const auto& R){return R.RouteValue==Snapshot.SelectedRouteValue;});
 const bool Result=Model->SelectRouteIntent(Snapshot.Routes[(FMath::Max(0,Index)+Direction+Snapshot.Routes.Num())%Snapshot.Routes.Num()].RouteValue);
 if(Result&&HasKeyboardFocus())Model->SetFocusedSemanticId(TEXT("TradeMap.Chart.CanvasFocus"));
 return Result;
}
FReply STradeRouteCanvas::OnKeyDown(const FGeometry& G,const FKeyEvent& E)
{
 const auto Key=E.GetKey();FVector2D Direction=FVector2D::ZeroVector;
 if(Key==EKeys::Left||Key==EKeys::Gamepad_DPad_Left)Direction={-1,0};
 if(Key==EKeys::Right||Key==EKeys::Gamepad_DPad_Right)Direction={1,0};
 if(Key==EKeys::Up||Key==EKeys::Gamepad_DPad_Up)Direction={0,-1};
 if(Key==EKeys::Down||Key==EKeys::Gamepad_DPad_Down)Direction={0,1};
 if(!Direction.IsNearlyZero())
 {
  TArray<TPair<FName,FVector2D>> Points;for(const auto& C:Snapshot.Cities)if(TradeGeometry::IsLocated(C.NormalizedPosition))Points.Add({C.StableId,Camera.Point(C.NormalizedPosition,G.GetLocalSize())});
  const auto Id=TradeGeometry::SpatialNeighbor(Points,FocusedCity,Direction);if(!Id.IsNone())FocusCity(Id);return FReply::Handled();
 }
 if(Key==EKeys::Enter||Key==EKeys::SpaceBar||Key==EKeys::Gamepad_FaceButton_Bottom){if(auto* P=Model.Get())P->SelectCityIntent(FocusedCity);return FReply::Handled();}
 if(Key==EKeys::W||Key==EKeys::Gamepad_RightStick_Up)Direction={0,64};
 if(Key==EKeys::S||Key==EKeys::Gamepad_RightStick_Down)Direction={0,-64};
 if(Key==EKeys::A||Key==EKeys::Gamepad_RightStick_Left)Direction={64,0};
 if(Key==EKeys::D||Key==EKeys::Gamepad_RightStick_Right)Direction={-64,0};
 if(!Direction.IsNearlyZero()){Camera.Move(Direction,G.GetLocalSize());Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
 if(Key==EKeys::Add||Key==EKeys::Equals||Key==EKeys::Gamepad_RightShoulder){ChangeZoom(.5f);return FReply::Handled();}
 if(Key==EKeys::Subtract||Key==EKeys::Hyphen||Key==EKeys::Gamepad_LeftShoulder){ChangeZoom(-.5f);return FReply::Handled();}
 if(Key==EKeys::Home||Key==EKeys::Gamepad_FaceButton_Top){ResetView();return FReply::Handled();}
 if(Key==EKeys::R||Key==EKeys::Gamepad_FaceButton_Left){NextRoute(E.IsShiftDown()?-1:1);return FReply::Handled();}
 return FReply::Unhandled();
}
FReply STradeRouteCanvas::OnMouseWheel(const FGeometry& G,const FPointerEvent& E)
{
 if(IsLeaseView()){ChangeZoom(E.GetWheelDelta()*.5);return FReply::Handled();}
 Camera.ZoomAt(E.GetWheelDelta()*.5,G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize());Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();
}
bool STradeRouteCanvas::IsRoutePort(FName Id) const {
 const auto* Route=Snapshot.Routes.FindByPredicate([&](const auto& R){return R.RouteValue==Snapshot.SelectedRouteValue;});
 return Id==Snapshot.SelectedCityStableId||(Route&&Route->CityIds.Contains(Id));
}
FSlateFontInfo STradeRouteCanvas::LabelFont(FName Id) const {
 auto Font=GetComponentFont(IsRoutePort(Id)?EHansaUiTypographyToken::MapLabelSelected:EHansaUiTypographyToken::MapLabel,Preferences);
 // Keep essential map names readable even at the smallest UI density.
 Font.Size/=FMath::Min(1.f,Preferences.UiScale);
 Font.OutlineSettings.OutlineSize=2;
 Font.OutlineSettings.OutlineColor=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen);
 return Font;
}
TArray<TradeGeometry::FLabel> STradeRouteCanvas::Labels(FVector2D Size) const
{
 TArray<TradeGeometry::FLabelInput> Inputs;
 const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
 for(const auto& City:Snapshot.Cities)if(TradeGeometry::IsLocated(City.NormalizedPosition))
 {
  const auto S=Measure->Measure(City.Label,LabelFont(City.StableId));
  Inputs.Add({City.StableId,Camera.Point(City.NormalizedPosition,Size),FVector2D(S)+FVector2D(16,12),City.StableId==FocusedCity&&HasKeyboardFocus()?3:IsRoutePort(City.StableId)?2:City.StableId==HoveredCity?1:0});
 }
 return TradeGeometry::PlaceLabels(MoveTemp(Inputs),Size);
}
FName STradeRouteCanvas::HitCity(FVector2D P,FVector2D Size) const
{
 for(const auto& L:Labels(Size))if(L.Bounds.ContainsPoint(P))return L.Id;
 TArray<TPair<FName,double>> Near;
 for(const auto& C:Snapshot.Cities)if(TradeGeometry::IsLocated(C.NormalizedPosition)){const double D=(Camera.Point(C.NormalizedPosition,Size)-P).SizeSquared();if(D<=24*24)Near.Add({C.StableId,D});}
 Near.Sort([](const auto& A,const auto& B){return A.Value!=B.Value?A.Value<B.Value:A.Key.LexicalLess(B.Key);});
 return Near.IsEmpty()?NAME_None:Near[0].Key;
}
FReply STradeRouteCanvas::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E)
{
 if(E.GetEffectingButton()==EKeys::RightMouseButton)return FReply::Handled().SetUserFocus(SharedThis(this),EFocusCause::Mouse).CaptureMouse(SharedThis(this));
 if(E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
 const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition());
 if(Model.IsValid()&&Model->GetSnapshot().ActiveSection==TEXT("Construction")&&Snapshot.SelectedCityStableId==TEXT("City.Rostock"))
 {
  const auto Size=G.GetLocalSize();const double Scale=FMath::Max(1.,FMath::Min((Size.X-48)/40.,(Size.Y-150)/32.))*LeaseZoom;
  const FVector2D Origin=FVector2D((Size.X-40*Scale)*.5,70)+LeasePan;const FVector2D C((P.X-Origin.X)/Scale-4,32-(P.Y-Origin.Y)/Scale);
  for(const auto& L:Model->GetConstructionPresentation().Plots)if(C.X>=L.BoundsMin.X&&C.X<L.BoundsMax.X+1&&C.Y>=L.BoundsMin.Y&&C.Y<L.BoundsMax.Y+1){Model->ConstructionIntent(TEXT("Plot.")+LexToString(L.Id));break;}
  return FReply::Handled();
 }
 const auto City=HitCity(P,G.GetLocalSize());
 if(!City.IsNone()){FocusCity(City);if(auto* M=Model.Get())M->SelectCityIntent(City);}
 else if(Overlay==ETradeOverlay::Routes&&Model.IsValid())
 {
  for(const auto& R:Snapshot.Routes)for(int32 I=0;I<R.CityIds.Num();++I)
  {
   const auto* A=FindCity(R.CityIds[I]);const auto* B=FindCity(R.CityIds[(I+1)%R.CityIds.Num()]);
   if(!A||!B||!TradeGeometry::IsLocated(A->NormalizedPosition)||!TradeGeometry::IsLocated(B->NormalizedPosition))continue;
   const auto Start=Camera.Point(A->NormalizedPosition,G.GetLocalSize()),End=Camera.Point(B->NormalizedPosition,G.GetLocalSize()),D=End-Start;
   const double T=FMath::Clamp(FVector2D::DotProduct(P-Start,D)/FMath::Max(1.,D.SizeSquared()),0.,1.);
   if((P-(Start+D*T)).SizeSquared()<12*12){Model->SelectRouteIntent(R.RouteValue);return FReply::Handled().SetUserFocus(SharedThis(this),EFocusCause::Mouse);}
  }
 }
 return FReply::Handled().SetUserFocus(SharedThis(this),EFocusCause::Mouse);
}
FReply STradeRouteCanvas::OnMouseMove(const FGeometry& G,const FPointerEvent& E)
{
 if(HasMouseCapture()){if(IsLeaseView())LeasePan+=E.GetCursorDelta()/G.GetAccumulatedLayoutTransform().GetScale();else Camera.Move(E.GetCursorDelta()/G.GetAccumulatedLayoutTransform().GetScale(),G.GetLocalSize());Invalidate(EInvalidateWidgetReason::Paint);return FReply::Handled();}
 const auto Id=HitCity(G.AbsoluteToLocal(E.GetScreenSpacePosition()),G.GetLocalSize());
 if(Id!=HoveredCity){HoveredCity=Id;const auto* C=FindCity(Id);if(C)SetToolTipText(FText::Format(LOCTEXT("CityTooltip","{0}\n{1}\n{2}\n{3}"),C->Label,C->Information,C->GoodReport,C->CapabilitySummary));Invalidate(EInvalidateWidgetReason::Paint);}
 return FReply::Unhandled();
}
FReply STradeRouteCanvas::OnMouseButtonUp(const FGeometry&,const FPointerEvent& E) { return E.GetEffectingButton()==EKeys::RightMouseButton?FReply::Handled().ReleaseMouseCapture():FReply::Unhandled(); }
FVector2D STradeRouteCanvas::MarkerPosition(FName Id) const { const auto* C=FindCity(Id);return C&&TradeGeometry::IsLocated(C->NormalizedPosition)?Camera.Point(C->NormalizedPosition,GetCachedGeometry().GetLocalSize()):FVector2D(-1,-1); }
FText STradeRouteCanvas::Summary() const
{
 if(IsLeaseView())return LOCTEXT("LeaseSummary","Rostock quarter · 4 m cells · Select a lease, then place a permitted building in the city.");
 const auto* C=FindCity(Snapshot.SelectedCityStableId);
 if(Snapshot.bCreating)return LOCTEXT("DraftMap","Draft sea connections · review stops before activation. Not sailing tracks.");
 if(!C)return LOCTEXT("NoCities","No matching cities. Clear the directory filters to restore the regional map.");
 if(!TradeGeometry::IsLocated(C->NormalizedPosition))return FText::Format(LOCTEXT("Unlocated","{0}: location unavailable. Select another city; its inspector remains available."),C->Label);
 if(Overlay==ETradeOverlay::Good)return FText::Format(LOCTEXT("GoodSummary","{0} · {1}\n{2} · {3}"),C->Label,FText::FromName(Snapshot.GoodId),C->GoodReport,C->Information);
 if(Overlay==ETradeOverlay::Presence)return FText::Format(LOCTEXT("PresenceSummary","{0} · {1}\n{2}"),C->Label,C->PresenceReport,C->CapabilitySummary);
 if(Overlay==ETradeOverlay::Alerts)return FText::Format(LOCTEXT("AlertSummary","{0} · {1}"),C->Label,C->MapAlert.IsEmpty()?LOCTEXT("NoKnownAlert","No reported station alert"):C->MapAlert);
 const auto* R=Snapshot.Routes.FindByPredicate([&](const auto& V){return V.RouteValue==Snapshot.SelectedRouteValue;});
 return R?FText::Format(LOCTEXT("RouteSummary","{0} · {1} · {2}\nConnections, not sailing tracks · solid sea / dashed land · width = capacity"),R->Label,R->State,R->Capacity):LOCTEXT("RouteNone","No matching routes. Create a route or clear the directory filters.");
}
int32 STradeRouteCanvas::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
 const auto Size=G.GetLocalSize();const auto* White=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
 auto Color=[](EHansaUiColorToken T){return UHansaUiStyleLibrary::GetColor(T);};
 auto Line=[&](const TArray<FVector2D>& P,int32 L,FLinearColor C,float W=1.f){FSlateDrawElement::MakeLines(Out,L,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,W);};
 auto Box=[&](FVector2D P,FVector2D S,int32 L,FLinearColor C){FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(FVector2f(S),FSlateLayoutTransform(FVector2f(P))),White,ESlateDrawEffect::None,C);};
 auto Point=[&](FVector2D P){return Camera.Point(P,Size);};

 if(Model.IsValid()&&Model->GetSnapshot().ActiveSection==TEXT("Construction")&&Snapshot.SelectedCityStableId==TEXT("City.Rostock"))
 {
  const auto View=Model->GetConstructionPresentation();
  const double Scale=FMath::Max(1.,FMath::Min((Size.X-48)/40.,(Size.Y-150)/32.))*LeaseZoom;
  const FVector2D Origin=FVector2D((Size.X-40*Scale)*.5,70)+LeasePan;
  auto Cell=[&](double X,double Y){return Origin+FVector2D((X+4)*Scale,(32-Y)*Scale);};
  Box({},Size,Layer,Color(EHansaUiColorToken::BalticNavy));
  auto Text=[&](FString V,FVector2D P,int32 L){auto Font=LabelFont(NAME_None);Font.OutlineSettings.OutlineSize=0;FSlateDrawElement::MakeText(Out,L,G.ToPaintGeometry(FVector2f(500,24),FSlateLayoutTransform(FVector2f(P))),V,Font,ESlateDrawEffect::None,Color(EHansaUiColorToken::Linen));};
  Text(TEXT("Rostock · leased plots / city survey · N ↑"),{16,12},Layer+5);
  static const auto Map=Hansa::Simulation::RostockPlacement::CreateMap();
  for(const auto& C:Map.Cells)
  {
   const auto V=C.Coordinate;if(V.X< -4||V.X>=36||V.Y<0||V.Y>=32)continue;
   const bool Road=Map.PublicRoadCells.Contains(V);
   auto Shade=Color(C.Terrain==Hansa::Simulation::EHansaPlacementTerrain::Water?EHansaUiColorToken::BalticNavy:Road?EHansaUiColorToken::Linen:C.bBlocked?EHansaUiColorToken::Ink:EHansaUiColorToken::Chalk);
   Shade.A=Road?.6f:C.bBlocked?.8f:.15f;
   Box(Cell(V.X,V.Y+1),{Scale-1,Scale-1},Layer+1,Shade);
  }
  for(const auto& P:View.Plots)
  {
   const auto A=Cell(P.BoundsMin.X,P.BoundsMin.Y),B=Cell(P.BoundsMax.X+1,P.BoundsMax.Y+1);
   const auto C=Color(EHansaUiColorToken::Brass);const float W=P.Id==View.SelectedLease?3:1;
   Line({A,{B.X,A.Y},B,{A.X,B.Y},A},Layer+3,C,W);
   if(!P.bActive)Line({A,B},Layer+3,C,2);
   for(const auto V:P.OccupiedCells){Line({Cell(V.X,V.Y),Cell(V.X+1,V.Y+1)},Layer+4,C,2);Line({Cell(V.X+1,V.Y),Cell(V.X,V.Y+1)},Layer+4,C,2);}
   Text(FString::Printf(TEXT("Lease %llu · House %llu · %s"),P.Id,P.OwnerId,P.bActive?TEXT("Active"):TEXT("Suspended")),{A.X,B.Y-22},Layer+5);
  }
  Text(TEXT("Brass: lease · Light: civic road · Dark: protected site · ×: occupied"),{16,Size.Y-84},Layer+5);
  Text(TEXT("4 m per cell · Outside leases: autonomous city · Click a lease to select"),{16,Size.Y-62},Layer+5);
  return Layer+6;
 }
 // Both painted layers share chart-space UVs; authored coast geometry remains authoritative.
 const auto* Sea=TradeArtwork(TEXT("Sea"),true);const auto* Land=TradeArtwork(TEXT("Land"),true);
 FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),Sea,ESlateDrawEffect::None,FLinearColor::White);
 TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
 for(const auto& P:TradeLandVertices){Indices.Add(Vertices.Num());Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(Point(FVector2D(P))),FVector2f(P.X,P.Y/1.70f),FColor::White));}
 FSlateDrawElement::MakeCustomVerts(Out,Layer+1,FSlateApplication::Get().GetRenderer()->GetResourceHandle(*Land),Vertices,Indices,nullptr,0,0);
 for(const auto& Ring:TradeCoastlines){TArray<FVector2D> P;for(const auto& V:Ring)P.Add(Point(V));Line(P,Layer+2,FLinearColor(.20f,.23f,.16f,1),Preferences.bHighContrast?2:1);}
 for(const auto& River:TradeRivers){TArray<FVector2D> P;for(const auto& V:River)P.Add(Point(V));Line(P,Layer+2,FLinearColor(.26f,.40f,.44f,.7f));}

 // Geographic lease geometry stays at true chart scale; the badge is a locator, not an enlarged boundary.
 if(Model.IsValid()&&Snapshot.SelectedCityStableId==TEXT("City.Rostock"))if(const auto* City=FindCity(Snapshot.SelectedCityStableId))
 {
  const auto V=Model->GetConstructionPresentation();
  for(const auto& Lease:V.Plots)
  {
   auto Geo=[&](int32 X,int32 Y){const auto W=Hansa::Simulation::RostockPlacement::CellCenter(X,Y)-FVector(60000,0,0)-FVector(200,200,0);return City->NormalizedPosition+FVector2D(W.X/100./(111320.*FMath::Cos(FMath::DegreesToRadians(54.1)))/33.,-W.Y/100./111320./11.);};
   const auto A=Point(Geo(Lease.BoundsMin.X,Lease.BoundsMin.Y)),B=Point(Geo(Lease.BoundsMax.X+1,Lease.BoundsMax.Y+1));
   Line({A,{B.X,A.Y},B,{A.X,B.Y},A},Layer+3,Color(EHansaUiColorToken::Brass),1);
  }
  if(!V.Plots.IsEmpty())
  {
   const auto At=Point(City->NormalizedPosition)+FVector2D(20,24);
   Box(At,{100,22},Layer+7,Color(EHansaUiColorToken::BalticNavy));
   auto Font=LabelFont(NAME_None);Font.OutlineSettings.OutlineSize=0;
   FSlateDrawElement::MakeText(Out,Layer+8,G.ToPaintGeometry(FVector2f(100,22),FSlateLayoutTransform(FVector2f(At))),FString::Printf(TEXT("%d lease(s)"),V.Plots.Num()),Font,ESlateDrawEffect::None,Color(EHansaUiColorToken::Linen));
  }
 }
 if(Overlay==ETradeOverlay::Routes)for(const auto& R:Snapshot.Routes)for(int32 I=0;I<R.CityIds.Num();++I)
 {
  const auto* A=FindCity(R.CityIds[I]);const auto* B=FindCity(R.CityIds[(I+1)%R.CityIds.Num()]);
  if(!A||!B||!TradeGeometry::IsLocated(A->NormalizedPosition)||!TradeGeometry::IsLocated(B->NormalizedPosition))continue;
  const auto Start=Point(A->NormalizedPosition),End=Point(B->NormalizedPosition);
  const auto C=Color(R.RouteValue==Snapshot.SelectedRouteValue?EHansaUiColorToken::Brass:EHansaUiColorToken::Chalk);
  const float Width=Thickness*(1.f+FMath::Clamp(R.CapacityMilliUnits/50000.f,0.f,4.f));
  if(R.bSea){Line({Start,End},Layer+3,Color(EHansaUiColorToken::Ink),Width+2);Line({Start,End},Layer+4,C,Width);}
  else {const double Length=(End-Start).Size();for(double D=0;D<Length;D+=18)Line({FMath::Lerp(Start,End,D/Length),FMath::Lerp(Start,End,FMath::Min(D+10,Length)/Length)},Layer+3,C,Width);}
  // Paused routes have crossbars in addition to textual state; sea remains solid.
  if(!R.bActive){const auto Mid=(Start+End)*.5;Line({Mid+FVector2D(-5,-5),Mid+FVector2D(5,5)},Layer+4,C,2);Line({Mid+FVector2D(-5,5),Mid+FVector2D(5,-5)},Layer+4,C,2);}
 }
 if(Snapshot.bCreating&&Snapshot.Stops.Num()>1)for(int32 I=0;I<Snapshot.Stops.Num();++I)
 {
  const auto* A=FindCity(Snapshot.Stops[I].CityStableId);const auto* B=FindCity(Snapshot.Stops[(I+1)%Snapshot.Stops.Num()].CityStableId);
  if(A&&B&&TradeGeometry::IsLocated(A->NormalizedPosition)&&TradeGeometry::IsLocated(B->NormalizedPosition))Line({Point(A->NormalizedPosition),Point(B->NormalizedPosition)},Layer+3,Color(EHansaUiColorToken::Brass),Thickness*2);
 }
 if(Overlay==ETradeOverlay::Routes&&Snapshot.bShipInTransit&&TradeGeometry::IsLocated(Snapshot.ShipPosition))
  FSlateDrawElement::MakeBox(Out,Layer+4,G.ToPaintGeometry(FVector2f(28,28),FSlateLayoutTransform(FVector2f(Point(Snapshot.ShipPosition)-FVector2D(14,14)))),GetGeneratedIconBrush(EUiGlyph::Ship,FMath::CeilToInt(28*G.GetAccumulatedLayoutTransform().GetScale())),ESlateDrawEffect::None,FLinearColor::White);
 for(const auto& City:Snapshot.Cities)if(TradeGeometry::IsLocated(City.NormalizedPosition))
 {
  const auto P=Point(City.NormalizedPosition);const bool Selected=City.StableId==Snapshot.SelectedCityStableId,Focused=HasKeyboardFocus()&&City.StableId==FocusedCity;
  const bool Important=IsRoutePort(City.StableId);
  const float D=Important?26.f:14.f;
  FSlateDrawElement::MakeBox(Out,Layer+5,G.ToPaintGeometry(FVector2f(D,D),FSlateLayoutTransform(FVector2f(P-FVector2D(D*.5,D*.5)))),TradeIconArtwork(Important?TEXT("CityMarkerSelected"):TEXT("CityMarker"),FMath::CeilToInt(D*G.GetAccumulatedLayoutTransform().GetScale())));
  // Focus is a continuous native ring, independent of route-thickness controls.
  if(Focused||City.StableId==HoveredCity){TArray<FVector2D> Circle;const float Radius=Important?18:12;for(int I=0;I<=64;++I){const double A=2*PI*I/64;Circle.Add(P+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius);}Line(Circle,Layer+6,Color(EHansaUiColorToken::Linen),4);Line(Circle,Layer+7,Color(EHansaUiColorToken::Ink),1);}
  if((Overlay==ETradeOverlay::Good&&(City.bStale||City.bUnknown))||(Overlay==ETradeOverlay::Alerts&&!City.MapAlert.IsEmpty()))
   FSlateDrawElement::MakeBox(Out,Layer+6,G.ToPaintGeometry(FVector2f(20,20),FSlateLayoutTransform(FVector2f(P+FVector2D(8,-26)))),GetGeneratedIconBrush(EUiGlyph::Warning,20));
  if(Overlay==ETradeOverlay::Presence&&City.bHasPresence)
   FSlateDrawElement::MakeBox(Out,Layer+6,G.ToPaintGeometry(FVector2f(20,20),FSlateLayoutTransform(FVector2f(P+FVector2D(8,-26)))),GetGeneratedIconBrush(EUiGlyph::Harbor,20));
 }
 for(const auto& L:Labels(Size))if(const auto* C=FindCity(L.Id))
 {
  const FVector2D P(L.Bounds.Left,L.Bounds.Top),S(L.Bounds.Right-L.Bounds.Left,L.Bounds.Bottom-L.Bounds.Top);
  if(Preferences.bHighContrast)Box(P,S,Layer+7,Color(EHansaUiColorToken::Linen));
  FSlateDrawElement::MakeText(Out,Layer+8,G.ToPaintGeometry(FVector2f(S),FSlateLayoutTransform(FVector2f(P+FVector2D(8,6)))),C->Label,LabelFont(C->StableId),ESlateDrawEffect::None,Color(EHansaUiColorToken::Ink));
 }
 if(HasKeyboardFocus())Line({{2,2},{Size.X-2,2},{Size.X-2,Size.Y-2},{2,Size.Y-2},{2,2}},Layer+9,Color(EHansaUiColorToken::Brass),2);
 return Layer+9;
}
}
#undef LOCTEXT_NAMESPACE
