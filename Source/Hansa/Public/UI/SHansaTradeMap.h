#pragma once

#include "CoreMinimal.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaUiComponents.h"

class SBorder;
class SBox;
class SScrollBox;
class SButton;
class STextBlock;
class SVerticalBox;
class SEditableTextBox;

namespace Hansa::UI
{
	class STradeRouteCanvas;
	class HANSA_API SHansaTradeMap final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaTradeMap) : _Model(nullptr), _InitialViewportSize(FIntPoint(1280,720)) {}
			SLATE_ARGUMENT(FUiPreferences, Preferences)
			SLATE_ARGUMENT(UHansaTradeMapPresentationModel*, Model)
			SLATE_ARGUMENT(FIntPoint, InitialViewportSize)
		SLATE_END_ARGS()
		~SHansaTradeMap();
		void Construct(const FArguments& Arguments);
		TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const { const auto* W=SemanticWidgets.Find(Id); return W?W->Pin():nullptr; }
		void SetPresentationSize(FIntPoint Size);
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	private:
		void Refresh(const FHansaTradeMapSnapshot& Snapshot, uint64 Revision);
		void RebuildRoutes(const FHansaTradeMapSnapshot& Snapshot);
		void RebuildStops(const FHansaTradeMapSnapshot& Snapshot);
		void MapWidget(const FString& Id, const TSharedPtr<SWidget>& Widget);
		FReply Invoke(const FString Id);
		TWeakObjectPtr<UHansaTradeMapPresentationModel> Model;
		FDelegateHandle ChangedHandle;
		FUiPreferences Preferences;
		FString PresentedContentKey;
		FString PresentedStopKey;
		FIntPoint PresentationSize{1280,720};
        FEditableTextBoxStyle RouteNameStyle;
        TSharedPtr<SEditableTextBox> RouteNameInput;
        TSharedPtr<STextBlock> CogLabel, ReviewText, ValidationText;
        TSharedPtr<SVerticalBox> SetupPanel, EditPanel, ReviewPanel, ExistingPanel;
		TSharedPtr<SBox> PresentationBox;
		TSharedPtr<SScrollBox> RouteScroll,EditorScroll;
		TSharedPtr<SVerticalBox> RouteList;
		TSharedPtr<SVerticalBox> StopList;
		TSharedPtr<SBox> CanvasHost;
		TSharedPtr<STradeRouteCanvas> RouteCanvas;
		TSharedPtr<STextBlock> ModeText;
		TSharedPtr<STextBlock> RouteTitle;
		TSharedPtr<STextBlock> RouteMetrics;
		TSharedPtr<STextBlock> ReserveRisk;
		TSharedPtr<SBorder> RouteStateCard;
		TSharedPtr<STextBlock> RouteStateHeading;
		TSharedPtr<STextBlock> RouteStateDetail;
		TSharedPtr<SButton> ToggleActiveButton;
		TSharedPtr<STextBlock> ToggleActiveText;
		TSharedPtr<STextBlock> ToggleActiveHint;
		TSharedPtr<STextBlock> EditorStatus;
		TSharedPtr<SBorder> BottomPanel;
		TMap<FString,TWeakPtr<SWidget>> SemanticWidgets;
		TArray<FString> FocusOrder;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FButtonStyle IconButtonStyle;
		FSlateBrush WorkingBrush;
		FSlateBrush FloatingBrush;
		FSlateBrush OverlayBrush;
		FTextBlockStyle LightBodyStyle;
		FTextBlockStyle LightCaptionStyle;
		FTextBlockStyle DarkBodyStyle;
		FTextBlockStyle LightHeadingStyle;
		FTextBlockStyle HeadingStyle;
	};
}
