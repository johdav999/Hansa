#pragma once
#include "HansaTradeWorkspaceComponents.h"

namespace Hansa::UI
{
class STradeShipDetail final : public STradeComponent
{
public:
 SLATE_BEGIN_ARGS(STradeShipDetail) {} SLATE_END_ARGS()
 void Construct(const FArguments&, const TSharedRef<FTradeComponentContext>&);
 void Refresh();
 void SetViewportSize(FIntPoint Size);
 bool Dispatch(const FString& Id);
 TArray<FString> FocusOrder() const;
 TArray<TSharedPtr<SScrollBox>> Scrolls;
 virtual bool SupportsKeyboardFocus() const override { return true; }
 virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
 TSharedPtr<SOverlay> Layers;
 TSharedPtr<SBox> Body, Popup;
 TMap<FString,TWeakPtr<SWidget>> Cells;
 FString Search, CacheKey;
 bool bCloseReview = false;
 bool bAdvanced = false;
 bool bCompactLayout = false;
 bool bRefreshQueued = false;
 bool bRefreshFromTimer = false;
 FIntPoint ViewportSize = FIntPoint(1536,1024);
 FButtonStyle CellStyle, SelectedCellStyle;
 FSlateBrush HeaderBrush;
 TArray<FString> Order;
 TMap<FString,TFunction<FReply()>> Actions;
 FDelegateHandle FocusHandle;
 TSharedRef<SWidget> Matrix();
 TSharedRef<SWidget> Selector();
 TSharedRef<SWidget> Button(const FString&, const FText&, TFunction<FReply()>);
 TSharedRef<SWidget> Text(const FText&, int32 Size = 20, bool Dark = false);
 EActiveTimerReturnType ApplyDeferredRefresh(double, float);
};
}
