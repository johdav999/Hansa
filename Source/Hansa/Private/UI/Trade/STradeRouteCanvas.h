#pragma once
#include "HansaTradeWorkspaceComponents.h"
#include "UI/HansaTradeMapGeometry.h"
#include "Widgets/SLeafWidget.h"

namespace Hansa::UI
{
enum class ETradeOverlay : uint8 { Routes, Good, Presence, Alerts };
class STradeRouteCanvas final : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(STradeRouteCanvas){} SLATE_ARGUMENT(FUiPreferences,Preferences) SLATE_ARGUMENT(UHansaTradeMapPresentationModel*,Model) SLATE_END_ARGS()
 void Construct(const FArguments& A);
 void SetSnapshot(const FTradeRegionalMapView& In);
 void Tick(const FGeometry& G,double Time,float Delta) override;
 virtual FVector2D ComputeDesiredSize(float) const override { return {720,520}; }
 virtual bool SupportsKeyboardFocus() const override { return true; }
 virtual FReply OnFocusReceived(const FGeometry&,const FFocusEvent&) override;
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent&) override;
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
 virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
 virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
 void ChangeZoom(float Delta);
 void ResetView();
 void CycleOverlay();
 void CycleThickness();
 bool FocusCity(FName Id);
 bool FrameSelection();
 bool NextRoute(int32 Direction);
 FText Summary() const;
 FText OverlayLabel() const;
 FText ThicknessLabel() const;
 FVector2D MarkerPosition(FName Id) const;
 FName FocusedCity;
private:
 const FHansaTradeMapCityPresentation* FindCity(FName Id) const;
 TArray<TradeGeometry::FLabel> Labels(FVector2D Size) const;
 bool IsRoutePort(FName Id) const;
 FSlateFontInfo LabelFont(FName Id) const;
 FName HitCity(FVector2D Point,FVector2D Size) const;
 FName HoveredCity;
 FTradeRegionalMapView Snapshot;
 FUiPreferences Preferences;
 TWeakObjectPtr<UHansaTradeMapPresentationModel> Model;
 TradeGeometry::FCamera Camera;
 double LeaseZoom=1.; FVector2D LeasePan=FVector2D::ZeroVector;
 bool IsLeaseView() const;
 ETradeOverlay Overlay=ETradeOverlay::Routes;
 float Thickness=1.f; bool bInitiallyFramed=false;
};
}
