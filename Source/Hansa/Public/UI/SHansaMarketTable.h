#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaUiComponents.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SBorder;
class SButton;
class SSearchBox;
class SScrollBox;
class STextBlock;
class SVerticalBox;

namespace Hansa::UI
{
	class SHansaPriceHistoryChart;
	/** Native sticky-header, virtualized presentation of the ten MVP market goods. */
	class HANSA_API SHansaMarketTable final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaMarketTable) : _Model(nullptr) {}
			SLATE_ARGUMENT(UHansaMarketTablePresentationModel*, Model)
			SLATE_ARGUMENT(FUiPreferences, Preferences)
		SLATE_END_ARGS()

		~SHansaMarketTable();
		void Construct(const FArguments& Arguments);
		bool ActivateSemanticId(const FString& SemanticId);
        virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
        virtual void Tick(const FGeometry&, double, float) override;
		bool FocusSemanticId(const FString& SemanticId);
		TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const { const auto* W=SemanticWidgets.Find(Id); return W?W->Pin():nullptr; }
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		[[nodiscard]] const TArray<FString>& GetControllerFocusOrder() const { return FocusOrder; }
#if WITH_DEV_AUTOMATION_TESTS
		[[nodiscard]] int32 GetListRefreshCountForTesting() const { return ListRefreshCount; }
		bool SelectListItemForTesting(FName GoodStableId);
#endif

	private:
		void Refresh(const FHansaMarketTableSnapshot& Snapshot, uint64 Revision);
		void RebuildItems(const FHansaMarketTableSnapshot& Snapshot);
		TSharedRef<ITableRow> GenerateRow(TSharedPtr<FHansaMarketTableRowPresentation> Item, const TSharedRef<STableViewBase>& OwnerTable);
		void HandleRowSelectionChanged(TSharedPtr<FHansaMarketTableRowPresentation> Item, ESelectInfo::Type SelectInfo);
		FReply InvokeCategory();
		FReply InvokeTrend();
		FReply InvokeQuick();
		FReply InvokeClear();
		FReply InvokeSort(EHansaMarketSortColumn Column);
		FReply InvokeRow(FName GoodStableId);
		FReply InvokePin();
		FReply InvokeRoute();
		void RebuildDetailLists(const FHansaSelectedGoodPresentation& Detail);
		void HandleSearchChanged(const FText& Text);
		void MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget);
		static FString RowId(FName GoodStableId);
		static FString CellId(FName GoodStableId, const TCHAR* Column);

		TWeakObjectPtr<UHansaMarketTablePresentationModel> Model;
		FUiPreferences Preferences;
		FTableRowStyle RowStyle;
		FSearchBoxStyle SearchStyle;
		FDelegateHandle ChangedHandle;
		uint64 PresentedRevision = 0;
		FSlateBrush WorkingBrush;
		FSlateBrush DecisionBrush;
		FSlateBrush CriticalBrush;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FTextBlockStyle HeadingStyle;
		FTextBlockStyle BodyStyle;
		FTextBlockStyle DataStyle;
		FTextBlockStyle BodyOnDarkStyle;
		FTextBlockStyle DataOnDarkStyle;
		FTextBlockStyle CaptionStyle;
		FTextBlockStyle CaptionOnDarkStyle;
		TSharedPtr<SSearchBox> SearchBox;
		TSharedPtr<SHansaAction> CategoryButton;
		TSharedPtr<SHansaAction> TrendButton;
		TSharedPtr<SHansaAction> QuickButton;
		TSharedPtr<SHansaAction> ClearButton;
		TSharedPtr<STextBlock> ResultText;
		TMap<EHansaMarketSortColumn, TSharedPtr<SHansaAction>> HeaderButtons;
		TSharedPtr<SBorder> ListPanel;
		TSharedPtr<SListView<TSharedPtr<FHansaMarketTableRowPresentation>>> ListView;
		TSharedPtr<SBorder> EmptyPanel;
		TSharedPtr<STextBlock> EmptyTitle;
		TSharedPtr<STextBlock> EmptyDetail;
		TSharedPtr<SBorder> DetailEmptyPanel;
		TSharedPtr<SBorder> DetailContentPanel;
		TSharedPtr<STextBlock> DetailTitle;
		TSharedPtr<STextBlock> DetailConfidence;
		TSharedPtr<STextBlock> BaseValueText;
		TSharedPtr<STextBlock> LocalPriceText;
		TSharedPtr<STextBlock> DifferenceText;
		TSharedPtr<STextBlock> StockReserveText;
		TSharedPtr<STextBlock> ReserveDaysText;
		TSharedPtr<STextBlock> CitizenDemandText;
		TSharedPtr<STextBlock> IndustrialDemandText;
		TSharedPtr<STextBlock> IncomingSupplyText;
		TSharedPtr<STextBlock> ProductionText, ConsumptionText, SupplyBalanceText;
		TSharedPtr<SScrollBox> DetailScroll;
		TSharedPtr<SScrollBox> ControlScroll;
        FString PendingDetailScroll;
        int32 ScrollLayoutAttempts=0;
		TSharedPtr<STextBlock> ExplanationText;
		TSharedPtr<STextBlock> ChartSummaryText;
		TSharedPtr<STextBlock> PinReasonText;
		TSharedPtr<STextBlock> RouteReasonText;
		TSharedPtr<STextBlock> ActionResultText;
		TSharedPtr<SVerticalBox> FactorList;
		TSharedPtr<SVerticalBox> ConsumerList;
		TSharedPtr<SVerticalBox> ProducerList;
		TSharedPtr<SHansaAction> PinButton;
		TSharedPtr<SHansaAction> RouteButton;
		TSharedPtr<SHansaPriceHistoryChart> PriceChart;
		FHansaSelectedGoodPresentation PresentedDetail;
		TArray<TSharedPtr<FHansaMarketTableRowPresentation>> Items;
		TArray<FHansaMarketTableRowPresentation> PresentedRows;
		FName PresentedSelectedGoodStableId;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
		TArray<FString> FocusOrder;
#if WITH_DEV_AUTOMATION_TESTS
		int32 ListRefreshCount = 0;
#endif
	};
}
