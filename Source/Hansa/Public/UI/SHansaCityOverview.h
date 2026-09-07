#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SBorder;
class SButton;
class SBox;
class STextBlock;
class SHorizontalBox;
class UHansaCityOverviewPresentationModel;
class UHansaMarketTablePresentationModel;

namespace Hansa::UI
{
	class SHansaMarketTable;
	/** Native, event-refreshed and virtualized City Overview management screen. */
	class HANSA_API SHansaCityOverview final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaCityOverview) : _Model(nullptr), _MarketTableModel(nullptr), _InitialViewportSize(1280, 720) {}
			SLATE_ARGUMENT(UHansaCityOverviewPresentationModel*, Model)
			SLATE_ARGUMENT(UHansaMarketTablePresentationModel*, MarketTableModel)
			SLATE_ARGUMENT(FIntPoint, InitialViewportSize)
		SLATE_END_ARGS()

		~SHansaCityOverview();
		void Construct(const FArguments& Arguments);
		void SetPresentationSize(FIntPoint Size);
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;
#if WITH_DEV_AUTOMATION_TESTS
		[[nodiscard]] int32 GetListRefreshCountForTesting() const { return ListRefreshCount; }
#endif

		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	private:
		void Refresh(const FHansaCityOverviewSnapshot& Snapshot, uint64 Revision);
		TSharedRef<ITableRow> GenerateRow(TSharedPtr<FHansaCityOverviewRowPresentation> Item, const TSharedRef<STableViewBase>& OwnerTable);
		void RebuildListItems(const FHansaCityOverviewSnapshot& Snapshot);
		void RebuildFocusOrder(const FHansaCityOverviewSnapshot& Snapshot);
		FReply InvokeTab(EHansaCityOverviewTab Tab);
		FReply InvokeClose();
		FReply InvokeRetry();
		FReply InvokeRow(FName RowStableId);
		FReply InvokeCausal(FName RowStableId);
		void MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget);
		static FString RowSemanticId(FName StableId);
		static FString RevealSemanticId(FName StableId);
		static FString TabSemanticId(EHansaCityOverviewTab Tab);

		TWeakObjectPtr<UHansaCityOverviewPresentationModel> Model;
		TWeakObjectPtr<UHansaMarketTablePresentationModel> MarketTableModel;
		FDelegateHandle ChangedHandle;
		uint64 PresentedRevision = 0;
		FIntPoint PresentationSize = FIntPoint(1280, 720);
		FSlateBrush WorldOverlayBrush;
		FSlateBrush WorkingBrush;
		FSlateBrush DecisionBrush;
		FSlateBrush CriticalBrush;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FTextBlockStyle DarkHeadingStyle;
		FTextBlockStyle DarkBodyStyle;
		FTextBlockStyle LightHeadingStyle;
		FTextBlockStyle LightBodyStyle;
		FTextBlockStyle LightDataStyle;
		FTextBlockStyle LightCaptionStyle;
		TSharedPtr<SBox> PresentationBox;
		TSharedPtr<SBorder> RootWidget;
		TSharedPtr<STextBlock> TitleText;
		TSharedPtr<SButton> CloseButton;
		TSharedPtr<SHorizontalBox> SummaryBox;
		TArray<TSharedPtr<SBorder>> SummaryCards;
		TArray<TSharedPtr<STextBlock>> SummaryLabels;
		TArray<TSharedPtr<STextBlock>> SummaryValues;
		TArray<TSharedPtr<STextBlock>> SummaryDetails;
		TSharedPtr<SButton> PopulationTab;
		TSharedPtr<SButton> ProductionTab;
		TSharedPtr<SButton> MarketTab;
		TSharedPtr<SButton> AdministrationTab;
		TSharedPtr<SBorder> ListPanel;
		TSharedPtr<SListView<TSharedPtr<FHansaCityOverviewRowPresentation>>> ListView;
		TSharedPtr<SHansaMarketTable> MarketTableWidget;
		TSharedPtr<SBorder> StatePanel;
		TSharedPtr<STextBlock> StateTitleText;
		TSharedPtr<STextBlock> StateDetailText;
		TSharedPtr<SButton> RetryButton;
		TSharedPtr<STextBlock> LastActionText;
		TArray<TSharedPtr<FHansaCityOverviewRowPresentation>> ListItems;
		TArray<FHansaCityOverviewRowPresentation> PresentedRows;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
		TArray<FString> FocusOrder;
#if WITH_DEV_AUTOMATION_TESTS
		int32 ListRefreshCount = 0;
#endif
	};
}
