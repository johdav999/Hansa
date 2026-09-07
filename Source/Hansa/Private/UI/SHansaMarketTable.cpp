#include "UI/SHansaMarketTable.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SHansaMarketTable"

namespace Hansa::UI
{
	class SHansaPriceHistoryChart final : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaPriceHistoryChart) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&) {}
		void SetData(const TArray<FHansaMarketChartPointPresentation>& InPoints, const int64 Average, const int64 Minimum, const int64 Maximum, const bool bInStale)
		{
			Points = InPoints;
			bStale = bInStale;
			AverageNormalized = Maximum == Minimum ? 0.5f : FMath::Clamp(static_cast<float>(Average - Minimum) / static_cast<float>(Maximum - Minimum), 0.0f, 1.0f);
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320.0f, 150.0f); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
			FSlateWindowElementList& Elements, const int32 LayerId, const FWidgetStyle& WidgetStyle, const bool bParentEnabled) const override
		{
			const FVector2D Size = Geometry.GetLocalSize();
			const FLinearColor Grid(0.35f, 0.38f, 0.37f, 0.45f);
			const FLinearColor Brass = bStale ? FLinearColor(0.55f, 0.48f, 0.34f, 0.85f) : FLinearColor(0.76f, 0.60f, 0.32f, 1.0f);
			for (int32 Line = 0; Line < 4; ++Line)
			{
				const float Y = 8.0f + (Size.Y - 16.0f) * static_cast<float>(Line) / 3.0f;
				FSlateDrawElement::MakeLines(Elements, LayerId, Geometry.ToPaintGeometry(), { FVector2D(8.0f, Y), FVector2D(Size.X - 8.0f, Y) }, ESlateDrawEffect::None, Grid, true, 1.0f);
			}
			if (!Points.IsEmpty())
			{
				const float AverageY = 8.0f + (Size.Y - 16.0f) * (1.0f - AverageNormalized);
				for (float X = 8.0f; X < Size.X - 8.0f; X += 10.0f)
				{
					FSlateDrawElement::MakeLines(Elements, LayerId + 1, Geometry.ToPaintGeometry(), { FVector2D(X, AverageY), FVector2D(FMath::Min(X + 5.0f, Size.X - 8.0f), AverageY) }, ESlateDrawEffect::None, Grid, true, 1.0f);
				}
			}
			if (Points.Num() > 1)
			{
				TArray<FVector2D> LinePoints;
				for (int32 Index = 0; Index < Points.Num(); ++Index)
				{
					const float X = 8.0f + (Size.X - 16.0f) * static_cast<float>(Index) / static_cast<float>(Points.Num() - 1);
					const float Y = 8.0f + (Size.Y - 16.0f) * (1.0f - FMath::Clamp(Points[Index].NormalizedPrice, 0.0f, 1.0f));
					LinePoints.Add(FVector2D(X, Y));
				}
				FSlateDrawElement::MakeLines(Elements, LayerId + 2, Geometry.ToPaintGeometry(), LinePoints, ESlateDrawEffect::None, Brass, true, 2.0f);
			}
			return LayerId + 2;
		}

	private:
		TArray<FHansaMarketChartPointPresentation> Points;
		float AverageNormalized = 0.5f;
		bool bStale = false;
	};

	namespace
	{
		FString SafeId(FString Value) { Value.ReplaceInline(TEXT("."), TEXT("_")); return Value; }

		FText CategoryText(const EHansaMarketGoodCategory Value)
		{
			const FText Filter = Value == EHansaMarketGoodCategory::Food ? LOCTEXT("Food", "Food") :
				Value == EHansaMarketGoodCategory::Material ? LOCTEXT("Material", "Material") :
				Value == EHansaMarketGoodCategory::Manufactured ? LOCTEXT("Manufactured", "Manufactured") : LOCTEXT("All", "All");
			return FText::Format(LOCTEXT("CategoryFilter", "Category: {0}"), Filter);
		}

		FText TrendText(const EHansaMarketTrendFilter Value)
		{
			const FText Filter = Value == EHansaMarketTrendFilter::Rising ? LOCTEXT("Rising", "Rising") :
				Value == EHansaMarketTrendFilter::Stable ? LOCTEXT("Stable", "Stable") :
				Value == EHansaMarketTrendFilter::Falling ? LOCTEXT("Falling", "Falling") :
				Value == EHansaMarketTrendFilter::Unknown ? LOCTEXT("Unknown", "Unknown") : LOCTEXT("All", "All");
			return FText::Format(LOCTEXT("TrendFilter", "Trend: {0}"), Filter);
		}

		FText QuickText(const EHansaMarketQuickFilter Value)
		{
			const FText Filter = Value == EHansaMarketQuickFilter::Shortage ? LOCTEXT("Shortage", "Shortage") :
				Value == EHansaMarketQuickFilter::OwnedStock ? LOCTEXT("OwnedStock", "Owned stock") :
				Value == EHansaMarketQuickFilter::Incoming ? LOCTEXT("Incoming", "Incoming") :
				Value == EHansaMarketQuickFilter::Opportunity ? LOCTEXT("Opportunity", "Opportunity") : LOCTEXT("All", "All");
			return FText::Format(LOCTEXT("QuickFilter", "Filter: {0}"), Filter);
		}

		FString SortName(const EHansaMarketSortColumn Column)
		{
			switch (Column)
			{
			case EHansaMarketSortColumn::Stock: return TEXT("Stock");
			case EHansaMarketSortColumn::Reserve: return TEXT("Reserve");
			case EHansaMarketSortColumn::Demand: return TEXT("Demand");
			case EHansaMarketSortColumn::Price: return TEXT("Price");
			case EHansaMarketSortColumn::Trend: return TEXT("Trend");
			case EHansaMarketSortColumn::Incoming: return TEXT("Incoming");
			case EHansaMarketSortColumn::Status: return TEXT("Status");
			default: return TEXT("Good");
			}
		}

		FText CompactSortLabel(const EHansaMarketSortColumn Column)
		{
			switch (Column)
			{
			case EHansaMarketSortColumn::Stock: return LOCTEXT("StockCompact", "Qty");
			case EHansaMarketSortColumn::Reserve: return LOCTEXT("ReserveCompact", "Min");
			case EHansaMarketSortColumn::Demand: return LOCTEXT("DemandCompact", "Need");
			case EHansaMarketSortColumn::Price: return LOCTEXT("PriceCompact", "Price");
			case EHansaMarketSortColumn::Trend: return LOCTEXT("TrendCompact", "Trend");
			case EHansaMarketSortColumn::Incoming: return LOCTEXT("IncomingCompact", "ETA");
			case EHansaMarketSortColumn::Status: return LOCTEXT("StatusCompact", "State");
			default: return LOCTEXT("GoodCompact", "Good");
			}
		}
	}

	SHansaMarketTable::~SHansaMarketTable()
	{
		if (UHansaMarketTablePresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaMarketTable::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model;
		WorkingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		DecisionBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		CriticalBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Critical);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		HeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);
		BodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		DataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, false);
		BodyOnDarkStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, true);
		DataOnDarkStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, true);
		CaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);
		CaptionOnDarkStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, true);

		auto Header = [this](const TCHAR* Label, const EHansaMarketSortColumn Column, const float Width)
		{
			TSharedPtr<SButton> Button;
			TSharedPtr<STextBlock> HeaderText;
			TSharedRef<SBox> Box = SNew(SBox).WidthOverride(Width)
			[
				SAssignNew(Button, SButton).ButtonStyle(&SecondaryButtonStyle).ContentPadding(FMargin(2.0f, 4.0f))
				.ToolTipText(FText::Format(LOCTEXT("SortColumnTip", "Sort by {0}"), FText::FromString(SortName(Column))))
				.OnClicked(this, &SHansaMarketTable::InvokeSort, Column)
				[
					SAssignNew(HeaderText, STextBlock).TextStyle(&CaptionStyle).Clipping(EWidgetClipping::ClipToBounds)
				]
			];
			HeaderButtons.Add(Button);
			HeaderTexts.Add(HeaderText);
			MapWidget(FString::Printf(TEXT("Market.Header.%s"), Label), Button);
			return Box;
		};
		auto Metric = [this](const FText& Label, TSharedPtr<STextBlock>& Value)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Label).TextStyle(&CaptionStyle)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 8.0f)[SAssignNew(Value, STextBlock).TextStyle(&DataStyle).AutoWrapText(true)];
		};

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.60f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(SearchBox, SSearchBox).HintText(LOCTEXT("Search", "Search goods")).OnTextChanged(this, &SHansaMarketTable::HandleSearchChanged)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)[SAssignNew(ResultText, STextBlock).TextStyle(&CaptionStyle)]]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(CategoryButton, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaMarketTable::InvokeCategory)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(TrendButton, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaMarketTable::InvokeTrend)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(QuickButton, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaMarketTable::InvokeQuick)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(ClearButton, SButton).ButtonStyle(&PrimaryButtonStyle).Text(LOCTEXT("Clear", "Clear filters")).OnClicked(this, &SHansaMarketTable::InvokeClear)]]
				+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 4.0f, 2.0f, 0.0f)[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[Header(TEXT("Good"), EHansaMarketSortColumn::Good, 115.0f)]
					+ SHorizontalBox::Slot().AutoWidth()[Header(TEXT("Stock"), EHansaMarketSortColumn::Stock, 60.0f)]
					+ SHorizontalBox::Slot().AutoWidth()[Header(TEXT("Reserve"), EHansaMarketSortColumn::Reserve, 70.0f)]
					+ SHorizontalBox::Slot().AutoWidth()[Header(TEXT("Demand"), EHansaMarketSortColumn::Demand, 70.0f)]
					+ SHorizontalBox::Slot().AutoWidth()[Header(TEXT("Price"), EHansaMarketSortColumn::Price, 70.0f)]
					+ SHorizontalBox::Slot().AutoWidth()[Header(TEXT("Trend"), EHansaMarketSortColumn::Trend, 95.0f)]
					+ SHorizontalBox::Slot().AutoWidth()[Header(TEXT("Incoming"), EHansaMarketSortColumn::Incoming, 75.0f)]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[Header(TEXT("Status"), EHansaMarketSortColumn::Status, 115.0f)]]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(2.0f)[SNew(SOverlay)
					+ SOverlay::Slot()[SAssignNew(ListPanel, SBorder).BorderImage(&WorkingBrush).Padding(2.0f)[SAssignNew(ListView, SListView<TSharedPtr<FHansaMarketTableRowPresentation>>).ListItemsSource(&Items).SelectionMode(ESelectionMode::Single).OnGenerateRow(this, &SHansaMarketTable::GenerateRow).OnSelectionChanged(this, &SHansaMarketTable::HandleRowSelectionChanged)]]
					+ SOverlay::Slot()[SAssignNew(EmptyPanel, SBorder).BorderImage(&DecisionBrush).Padding(24.0f)[SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(EmptyTitle, STextBlock).TextStyle(&HeadingStyle)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)[SAssignNew(EmptyDetail, STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]]]]
			]
			+ SHorizontalBox::Slot().FillWidth(0.40f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SAssignNew(DetailEmptyPanel, SBorder).BorderImage(&DecisionBrush).Padding(24.0f)[SNew(STextBlock).Text(LOCTEXT("SelectDetail", "Select a good to inspect price, supply, demand and authoritative causes.")).TextStyle(&BodyStyle).AutoWrapText(true)]]
				+ SOverlay::Slot()[SAssignNew(DetailContentPanel, SBorder).BorderImage(&WorkingBrush).Padding(12.0f)[SNew(SScrollBox)
					+ SScrollBox::Slot()[SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(DetailTitle, STextBlock).TextStyle(&HeadingStyle)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 10.0f)[SAssignNew(DetailConfidence, STextBlock).TextStyle(&CaptionStyle)]
						+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)[Metric(LOCTEXT("BaseValue", "Base value"), BaseValueText)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)[Metric(LOCTEXT("LocalPrice", "Local price"), LocalPriceText)]]
						+ SVerticalBox::Slot().AutoHeight().Padding(3.0f)[Metric(LOCTEXT("VsAverage", "vs recent average"), DifferenceText)]
						+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)[Metric(LOCTEXT("StockReserveLabel", "Stock / desired reserve"), StockReserveText)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)[Metric(LOCTEXT("ReserveCoverage", "Reserve coverage"), ReserveDaysText)]]
						+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)[Metric(LOCTEXT("CitizenDemand", "Citizen demand"), CitizenDemandText)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)[Metric(LOCTEXT("IndustrialDemand", "Industrial demand"), IndustrialDemandText)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f)[Metric(LOCTEXT("IncomingSupply", "Incoming"), IncomingSupplyText)]]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 2.0f)[SNew(STextBlock).Text(LOCTEXT("PriceHistory", "Price history")).TextStyle(&BodyStyle)]
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(PriceChart, SHansaPriceHistoryChart)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 8.0f)[SAssignNew(ChartSummaryText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("WhyPrice", "Why this price?")).TextStyle(&BodyStyle)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 8.0f)[SAssignNew(ExplanationText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(FactorList, SVerticalBox)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 2.0f)[SNew(STextBlock).Text(LOCTEXT("Consumers", "Consumers")).TextStyle(&BodyStyle)]
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(ConsumerList, SVerticalBox)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 2.0f)[SNew(STextBlock).Text(LOCTEXT("Producers", "Producers")).TextStyle(&BodyStyle)]
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(ProducerList, SVerticalBox)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(PinButton, SButton).ButtonStyle(&PrimaryButtonStyle).OnClicked(this, &SHansaMarketTable::InvokePin)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(RouteButton, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaMarketTable::InvokeRoute)]]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)[SAssignNew(PinReasonText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)[SAssignNew(RouteReasonText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)[SAssignNew(ActionResultText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]]]]
			]
		];

		MapWidget(TEXT("Market.Root"), SharedThis(this));
		MapWidget(TEXT("Market.Search"), SearchBox);
		MapWidget(TEXT("Market.Filter.Category"), CategoryButton);
		MapWidget(TEXT("Market.Filter.Trend"), TrendButton);
		MapWidget(TEXT("Market.Filter.Quick"), QuickButton);
		MapWidget(TEXT("Market.Filter.Clear"), ClearButton);
		MapWidget(TEXT("Market.List"), ListPanel);
		MapWidget(TEXT("Market.Empty"), EmptyPanel);
		MapWidget(TEXT("Market.Detail"), DetailContentPanel);
		MapWidget(TEXT("Market.Detail.Empty"), DetailEmptyPanel);
		MapWidget(TEXT("Market.Detail.Action.Pin"), PinButton);
		MapWidget(TEXT("Market.Detail.Action.BeginRoute"), RouteButton);
		if (UHansaMarketTablePresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaMarketTable::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
	}

	FString SHansaMarketTable::RowId(const FName GoodStableId) { return FString::Printf(TEXT("Market.Row.%s"), *SafeId(GoodStableId.ToString())); }
	FString SHansaMarketTable::CellId(const FName GoodStableId, const TCHAR* Column) { return FString::Printf(TEXT("%s.%s"), *RowId(GoodStableId), Column); }

	TSharedRef<ITableRow> SHansaMarketTable::GenerateRow(TSharedPtr<FHansaMarketTableRowPresentation> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		const FHansaMarketTableRowPresentation Row = Item.IsValid() ? *Item : FHansaMarketTableRowPresentation();
		const bool bSelected = Model.IsValid() && Model->GetSnapshot().SelectedGoodStableId == Row.GoodStableId;
		TSharedPtr<SButton> GoodButton;
		const FTextBlockStyle& RowDataStyle = Row.bShortage ? DataOnDarkStyle : DataStyle;
		const FTextBlockStyle& RowStatusStyle = Row.bShortage ? CaptionOnDarkStyle : CaptionStyle;
		auto Cell = [](const FText& Text, const float Width, const FText& ToolTip, const FTextBlockStyle& Style)
		{
			return SNew(SBox).WidthOverride(Width).Padding(FMargin(5.0f, 2.0f))
			[
				SNew(STextBlock).Text(Text).TextStyle(&Style).ToolTipText(ToolTip).Clipping(EWidgetClipping::ClipToBounds)
			];
		};
		TSharedRef<STableRow<TSharedPtr<FHansaMarketTableRowPresentation>>> Result =
			SNew(STableRow<TSharedPtr<FHansaMarketTableRowPresentation>>, OwnerTable).Padding(1.0f)
			[
				SNew(SBorder).BorderImage(Row.bStale ? &DecisionBrush : (Row.bShortage ? &CriticalBrush : &WorkingBrush)).Padding(2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(115.0f)[SAssignNew(GoodButton, SButton).ButtonStyle(bSelected ? &PrimaryButtonStyle : &SecondaryButtonStyle).ContentPadding(FMargin(4.0f, 2.0f)).OnClicked(this, &SHansaMarketTable::InvokeRow, Row.GoodStableId).ToolTipText(Row.AccessibleLabel)[SNew(STextBlock).Text(FText::Format(LOCTEXT("GoodWithGlyph", "{0} {1}"), Row.GoodGlyph, Row.GoodLabel)).TextStyle(bSelected ? &BodyOnDarkStyle : &BodyStyle).Clipping(EWidgetClipping::ClipToBounds)]]]
					+ SHorizontalBox::Slot().AutoWidth()[Cell(Row.Stock, 60.0f, LOCTEXT("StockTip", "Owned city stock"), RowDataStyle)]
					+ SHorizontalBox::Slot().AutoWidth()[Cell(Row.Reserve, 70.0f, LOCTEXT("ReserveTip", "Desired reserve target"), RowDataStyle)]
					+ SHorizontalBox::Slot().AutoWidth()[Cell(Row.Demand, 70.0f, LOCTEXT("DemandTip", "Citizen plus industrial demand"), RowDataStyle)]
					+ SHorizontalBox::Slot().AutoWidth()[Cell(Row.Price, 70.0f, Row.bEstimated ? LOCTEXT("EstimatedPriceTip", "Estimated price from a stale report") : LOCTEXT("PriceTip", "Current local price"), RowDataStyle)]
					+ SHorizontalBox::Slot().AutoWidth()[Cell(FText::Format(LOCTEXT("TrendSpark", "{0} {1}"), Row.Trend, Row.Sparkline), 95.0f, LOCTEXT("TrendTip", "Price change versus recent average and recent price history"), RowDataStyle)]
					+ SHorizontalBox::Slot().AutoWidth()[Cell(Row.Incoming, 75.0f, LOCTEXT("IncomingTip", "Confirmed incoming supply"), RowDataStyle)]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[Cell(Row.Status, 115.0f, Row.ReportAge, RowStatusStyle)]
				]
			];
		if (Item.IsValid()) MapWidget(RowId(Row.GoodStableId), GoodButton);
		return Result;
	}

	void SHansaMarketTable::HandleRowSelectionChanged(
		TSharedPtr<FHansaMarketTableRowPresentation> Item,
		const ESelectInfo::Type SelectInfo)
	{
		if (!Item.IsValid() || !Model.IsValid() ||
			(SelectInfo != ESelectInfo::OnMouseClick && SelectInfo != ESelectInfo::OnKeyPress))
		{
			return;
		}
		Model->SelectGoodIntent(Item->GoodStableId);
	}

	void SHansaMarketTable::Refresh(const FHansaMarketTableSnapshot& Snapshot, const uint64 Revision)
	{
		PresentedRevision = Revision;
		CategoryButton->SetContent(SNew(STextBlock).Text(CategoryText(Snapshot.CategoryFilter)).TextStyle(&BodyStyle));
		TrendButton->SetContent(SNew(STextBlock).Text(TrendText(Snapshot.TrendFilter)).TextStyle(&BodyStyle));
		QuickButton->SetContent(SNew(STextBlock).Text(QuickText(Snapshot.QuickFilter)).TextStyle(&BodyStyle));
		ResultText->SetText(Snapshot.ResultSummary);
		for (int32 Index = 0; Index < HeaderTexts.Num(); ++Index)
		{
			const EHansaMarketSortColumn Column = static_cast<EHansaMarketSortColumn>(Index);
			const FString Arrow = Snapshot.SortColumn == Column ? (Snapshot.bSortAscending ? TEXT(" ↑") : TEXT(" ↓")) : FString();
			HeaderTexts[Index]->SetText(FText::Format(LOCTEXT("CompactSortState", "{0}{1}"), CompactSortLabel(Column), FText::FromString(Arrow)));
			if (HeaderButtons.IsValidIndex(Index)) HeaderButtons[Index]->SetButtonStyle(Snapshot.SortColumn == Column ? &PrimaryButtonStyle : &SecondaryButtonStyle);
		}
		EmptyTitle->SetText(Snapshot.EmptyTitle); EmptyDetail->SetText(Snapshot.EmptyDetail);
		const bool bEmpty = Snapshot.VisibleRows.IsEmpty();
		ListPanel->SetVisibility(bEmpty ? EVisibility::Collapsed : EVisibility::Visible);
		EmptyPanel->SetVisibility(bEmpty ? EVisibility::Visible : EVisibility::Collapsed);
		bool bRowsChanged = PresentedRows.Num() != Snapshot.VisibleRows.Num();
		for (int32 Index = 0; !bRowsChanged && Index < PresentedRows.Num(); ++Index) bRowsChanged = !(PresentedRows[Index] == Snapshot.VisibleRows[Index]);
		const bool bSelectionChanged = PresentedSelectedGoodStableId != Snapshot.SelectedGoodStableId;
		if (bRowsChanged || bSelectionChanged) RebuildItems(Snapshot);
		PresentedSelectedGoodStableId = Snapshot.SelectedGoodStableId;
		if (Snapshot.SelectedGoodStableId.IsNone())
		{
			ListView->ClearSelection();
		}
		else
		{
			for (const TSharedPtr<FHansaMarketTableRowPresentation>& Item : Items)
			{
				if (Item.IsValid() && Item->GoodStableId == Snapshot.SelectedGoodStableId)
				{
					ListView->SetSelection(Item, ESelectInfo::Direct);
					break;
				}
			}
		}
		const FHansaSelectedGoodPresentation& Detail = Snapshot.SelectedGood;
		DetailEmptyPanel->SetVisibility(Detail.bHasSelection ? EVisibility::Collapsed : EVisibility::Visible);
		DetailContentPanel->SetVisibility(Detail.bHasSelection ? EVisibility::Visible : EVisibility::Collapsed);
		DetailTitle->SetText(FText::Format(LOCTEXT("DetailTitle", "{0} {1}"), Detail.GoodGlyph, Detail.GoodLabel));
		DetailConfidence->SetText(Detail.Confidence);
		BaseValueText->SetText(Detail.BaseValue); LocalPriceText->SetText(Detail.LocalPrice); DifferenceText->SetText(Detail.RecentAverageDifference);
		StockReserveText->SetText(Detail.StockVersusReserve); ReserveDaysText->SetText(Detail.ReserveDays);
		CitizenDemandText->SetText(Detail.CitizenDemand); IndustrialDemandText->SetText(Detail.IndustrialDemand); IncomingSupplyText->SetText(Detail.IncomingSupply);
		ExplanationText->SetText(Detail.Explanation); ChartSummaryText->SetText(Detail.ChartSummary);
		PinButton->SetEnabled(Detail.bPinEnabled); RouteButton->SetEnabled(Detail.bRouteEnabled);
		PinButton->SetContent(SNew(STextBlock).Text(Detail.bPinned ? LOCTEXT("Unpin", "Unpin price & stock") : Detail.PinActionLabel).TextStyle(&BodyStyle));
		RouteButton->SetContent(SNew(STextBlock).Text(Detail.RouteActionLabel).TextStyle(&BodyStyle));
		PinReasonText->SetText(Detail.PinDisabledReason); PinReasonText->SetVisibility(!Detail.bPinEnabled && !Detail.PinDisabledReason.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed);
		RouteReasonText->SetText(Detail.RouteDisabledReason); RouteReasonText->SetVisibility(!Detail.bRouteEnabled && !Detail.RouteDisabledReason.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed);
		ActionResultText->SetText(Detail.LastActionResult); ActionResultText->SetVisibility(Detail.LastActionResult.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
		PriceChart->SetData(Detail.History, Detail.RecentAveragePriceMilliMarks, Detail.MinimumHistoryPriceMilliMarks, Detail.MaximumHistoryPriceMilliMarks, Detail.bStale);
		if (!(PresentedDetail == Detail)) RebuildDetailLists(Detail);
		PresentedDetail = Detail;
		FocusOrder = { TEXT("Market.Search"), TEXT("Market.Filter.Category"), TEXT("Market.Filter.Trend"), TEXT("Market.Filter.Quick"), TEXT("Market.Filter.Clear") };
		for (const auto& Row : Snapshot.VisibleRows) FocusOrder.Add(RowId(Row.GoodStableId));
		if (Detail.bPinEnabled) FocusOrder.Add(TEXT("Market.Detail.Action.Pin"));
		if (Detail.bRouteEnabled) FocusOrder.Add(TEXT("Market.Detail.Action.BeginRoute"));
	}

	void SHansaMarketTable::RebuildDetailLists(const FHansaSelectedGoodPresentation& Detail)
	{
		FactorList->ClearChildren(); ConsumerList->ClearChildren(); ProducerList->ClearChildren();
		for (const auto& Factor : Detail.Factors)
		{
			FactorList->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(Factor.Label).TextStyle(&CaptionStyle).AutoWrapText(true)]
				+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f)[SNew(STextBlock).Text(Factor.Contribution).TextStyle(&DataStyle)]];
		}
		auto AddRelationships = [this](const TArray<FHansaMarketRelationshipPresentation>& Relationships, const TSharedPtr<SVerticalBox>& List)
		{
			if (Relationships.IsEmpty()) List->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("NoneReported", "None reported")).TextStyle(&CaptionStyle)];
			for (const auto& Relationship : Relationships)
			{
				List->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Relationship.Label).TextStyle(&CaptionStyle)]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::Format(LOCTEXT("RelationshipDetail", "{0} · {1}"), Relationship.Detail, Relationship.Status)).TextStyle(&CaptionStyle).AutoWrapText(true)]];
			}
		};
		AddRelationships(Detail.Consumers, ConsumerList);
		AddRelationships(Detail.Producers, ProducerList);
	}

	void SHansaMarketTable::RebuildItems(const FHansaMarketTableSnapshot& Snapshot)
	{
		Items.Reset(); PresentedRows = Snapshot.VisibleRows;
		for (const auto& Row : Snapshot.VisibleRows) Items.Add(MakeShared<FHansaMarketTableRowPresentation>(Row));
		ListView->RequestListRefresh();
#if WITH_DEV_AUTOMATION_TESTS
		++ListRefreshCount;
#endif
	}

	FReply SHansaMarketTable::InvokeCategory() { return Model.IsValid() && Model->CycleCategoryFilterIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeTrend() { return Model.IsValid() && Model->CycleTrendFilterIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeQuick() { return Model.IsValid() && Model->CycleQuickFilterIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeClear() { return Model.IsValid() && Model->ClearFiltersIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeSort(const EHansaMarketSortColumn Column) { return Model.IsValid() && Model->SortByIntent(Column) ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeRow(const FName GoodStableId) { return Model.IsValid() && Model->SelectGoodIntent(GoodStableId) ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokePin() { return Model.IsValid() && Model->TogglePinIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeRoute() { return Model.IsValid() && Model->BeginRouteIntent() ? FReply::Handled() : FReply::Unhandled(); }
	void SHansaMarketTable::HandleSearchChanged(const FText& Text) { if (Model.IsValid()) Model->SetSearchTextIntent(Text); }
	void SHansaMarketTable::MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget) { SemanticWidgets.Add(SemanticId, Widget); }

#if WITH_DEV_AUTOMATION_TESTS
	bool SHansaMarketTable::SelectListItemForTesting(const FName GoodStableId)
	{
		for (const TSharedPtr<FHansaMarketTableRowPresentation>& Item : Items)
		{
			if (Item.IsValid() && Item->GoodStableId == GoodStableId)
			{
				ListView->SetSelection(Item, ESelectInfo::OnMouseClick);
				return Model.IsValid() && Model->GetSnapshot().SelectedGoodStableId == GoodStableId;
			}
		}
		return false;
	}
#endif

	bool SHansaMarketTable::ActivateSemanticId(const FString& SemanticId)
	{
		if (!Model.IsValid()) return false;
		if (SemanticId == TEXT("Market.Filter.Category")) return Model->CycleCategoryFilterIntent();
		if (SemanticId == TEXT("Market.Filter.Trend")) return Model->CycleTrendFilterIntent();
		if (SemanticId == TEXT("Market.Filter.Quick")) return Model->CycleQuickFilterIntent();
		if (SemanticId == TEXT("Market.Filter.Clear")) return Model->ClearFiltersIntent();
		if (SemanticId == TEXT("Market.Detail.Action.Pin")) return Model->TogglePinIntent();
		if (SemanticId == TEXT("Market.Detail.Action.BeginRoute")) return Model->BeginRouteIntent();
		if (SemanticId.StartsWith(TEXT("Market.Header.")))
		{
			const FString Name = SemanticId.RightChop(14);
			for (int32 Index = 0; Index < 8; ++Index) if (SortName(static_cast<EHansaMarketSortColumn>(Index)) == Name) return Model->SortByIntent(static_cast<EHansaMarketSortColumn>(Index));
		}
		for (const auto& Row : Model->GetSnapshot().AllRows) if (SemanticId == RowId(Row.GoodStableId)) return Model->SelectGoodIntent(Row.GoodStableId);
		return false;
	}

	bool SHansaMarketTable::FocusSemanticId(const FString& SemanticId)
	{
		const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found != nullptr ? Found->Pin() : nullptr;
		if (!Widget.IsValid() && SemanticId.StartsWith(TEXT("Market.Row.")))
		{
			for (const auto& Item : Items) if (Item.IsValid() && SemanticId == RowId(Item->GoodStableId))
			{
				ListView->SetSelection(Item, ESelectInfo::OnNavigation); ListView->RequestScrollIntoView(Item);
				if (Model.IsValid()) Model->SetFocusedSemanticId(FName(*SemanticId));
				if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(ListView, EFocusCause::Navigation);
				return true;
			}
		}
		if (!Widget.IsValid() || !Widget->IsEnabled()) return false;
		if (Model.IsValid()) Model->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
		return true;
	}

	TArray<FHansaHudSemanticNode> SHansaMarketTable::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Nodes;
		const UHansaMarketTablePresentationModel* Pinned = Model.Get(); if (Pinned == nullptr) return Nodes;
		const auto& Snapshot = Pinned->GetSnapshot();
		auto Add = [this, &Nodes, &Snapshot](const FString& Id, const FString& Parent, const FString& Label, const EHansaHudSemanticRole Role,
			const FString& Type = FString(), const FString& Value = FString(), const bool bActivate = false, const bool bFocus = false, const bool bSelected = false, const bool bWarning = false)
		{
			FHansaHudSemanticNode Node; Node.Id = Id; Node.ParentId = Parent; Node.Label = Label; Node.Role = Role;
			Node.State.ValueType = Type; Node.State.Value = Value; Node.bCanActivate = bActivate; Node.bCanFocus = bFocus;
			Node.State.bSelected = bSelected; Node.State.bWarning = bWarning; Node.State.bFocused = Snapshot.FocusedSemanticId == FName(*Id);
			if (const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(Id)) if (const TSharedPtr<SWidget> Widget = Found->Pin())
			{
				Node.State.bVisible = Widget->GetVisibility().IsVisible(); Node.State.bEnabled = Widget->IsEnabled();
			}
			Nodes.Add(MoveTemp(Node));
		};
		Add(TEXT("Market.Root"), TEXT("CityOverview.Root"), TEXT("Market goods table"), EHansaHudSemanticRole::Panel, TEXT("goods-count"), FString::FromInt(Snapshot.AllRows.Num()));
		Add(TEXT("Market.Toolbar"), TEXT("Market.Root"), TEXT("Market search and filters"), EHansaHudSemanticRole::Panel);
		Add(TEXT("Market.Search"), TEXT("Market.Toolbar"), TEXT("Search goods"), EHansaHudSemanticRole::Text, TEXT("search"), Snapshot.SearchText.ToString(), false, true);
		Add(TEXT("Market.Filter.Category"), TEXT("Market.Toolbar"), CategoryText(Snapshot.CategoryFilter).ToString(), EHansaHudSemanticRole::Button, TEXT("category-filter"), FString::FromInt(static_cast<int32>(Snapshot.CategoryFilter)), true, true);
		Add(TEXT("Market.Filter.Trend"), TEXT("Market.Toolbar"), TrendText(Snapshot.TrendFilter).ToString(), EHansaHudSemanticRole::Button, TEXT("trend-filter"), FString::FromInt(static_cast<int32>(Snapshot.TrendFilter)), true, true);
		Add(TEXT("Market.Filter.Quick"), TEXT("Market.Toolbar"), QuickText(Snapshot.QuickFilter).ToString(), EHansaHudSemanticRole::Button, TEXT("quick-filter"), FString::FromInt(static_cast<int32>(Snapshot.QuickFilter)), true, true);
		Add(TEXT("Market.Filter.Clear"), TEXT("Market.Toolbar"), TEXT("Clear filters"), EHansaHudSemanticRole::Button, TEXT("action"), TEXT("clear-filters"), true, true);
		Add(TEXT("Market.List"), TEXT("Market.Root"), TEXT("Virtualized market goods"), EHansaHudSemanticRole::List, TEXT("visible-count"), FString::FromInt(Snapshot.VisibleRows.Num()));
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const EHansaMarketSortColumn Column = static_cast<EHansaMarketSortColumn>(Index); const FString Name = SortName(Column);
			Add(TEXT("Market.Header.") + Name, TEXT("Market.List"), Name, EHansaHudSemanticRole::Button, TEXT("sort"),
				Snapshot.SortColumn == Column ? (Snapshot.bSortAscending ? TEXT("ascending") : TEXT("descending")) : TEXT("none"), true, true, Snapshot.SortColumn == Column);
		}
		for (const auto& Row : Snapshot.VisibleRows)
		{
			const FString Id = RowId(Row.GoodStableId);
			Add(Id, TEXT("Market.List"), Row.AccessibleLabel.ToString(), EHansaHudSemanticRole::ListItem, TEXT("good-id"), Row.GoodStableId.ToString(), true, true,
				Snapshot.SelectedGoodStableId == Row.GoodStableId, Row.bShortage || Row.bStale);
			for (const TPair<const TCHAR*, const FText*> Cell : { TPair<const TCHAR*, const FText*>(TEXT("Stock"), &Row.Stock), { TEXT("Reserve"), &Row.Reserve },
				{ TEXT("Demand"), &Row.Demand }, { TEXT("Price"), &Row.Price }, { TEXT("Trend"), &Row.Trend }, { TEXT("Incoming"), &Row.Incoming }, { TEXT("Status"), &Row.Status } })
			{
				Add(CellId(Row.GoodStableId, Cell.Key), Id, Cell.Key, EHansaHudSemanticRole::Status, TEXT("value"), Cell.Value->ToString());
			}
		}
		const auto& Detail = Snapshot.SelectedGood;
		Add(TEXT("Market.Detail"), TEXT("Market.Root"), Detail.bHasSelection ? Detail.GoodLabel.ToString() + TEXT(" selected-good details") : TEXT("Selected-good details"),
			EHansaHudSemanticRole::Panel, TEXT("good-id"), Detail.GoodStableId.ToString(), false, false, Detail.bHasSelection, Detail.bStale);
		Add(TEXT("Market.Detail.Summary"), TEXT("Market.Detail"), Detail.Explanation.ToString(), EHansaHudSemanticRole::Text, TEXT("causal-summary"), Detail.ChartSummary.ToString());
		for (const TPair<const TCHAR*, const FText*> Metric : { TPair<const TCHAR*, const FText*>(TEXT("BaseValue"), &Detail.BaseValue),
			{ TEXT("LocalPrice"), &Detail.LocalPrice }, { TEXT("RecentAverageDifference"), &Detail.RecentAverageDifference },
			{ TEXT("StockVersusReserve"), &Detail.StockVersusReserve }, { TEXT("ReserveDays"), &Detail.ReserveDays },
			{ TEXT("CitizenDemand"), &Detail.CitizenDemand }, { TEXT("IndustrialDemand"), &Detail.IndustrialDemand }, { TEXT("IncomingSupply"), &Detail.IncomingSupply } })
		{
			Add(FString(TEXT("Market.Detail.Metric.")) + Metric.Key, TEXT("Market.Detail"), Metric.Key, EHansaHudSemanticRole::Status, TEXT("value"), Metric.Value->ToString());
		}
		Add(TEXT("Market.Detail.Chart"), TEXT("Market.Detail"), TEXT("Price history"), EHansaHudSemanticRole::Panel, TEXT("chart"),
			FString::Printf(TEXT("motion=none;summary=%s"), *Detail.ChartSummary.ToString()));
		for (int32 Index = 0; Index < Detail.History.Num(); ++Index)
		{
			const auto& Point = Detail.History[Index];
			Add(FString::Printf(TEXT("Market.Detail.Chart.Point.%d"), Index), TEXT("Market.Detail.Chart"), Point.AccessibleLabel.ToString(), EHansaHudSemanticRole::Status,
				TEXT("price-milli-marks"), LexToString(Point.PriceMilliMarks));
		}
		Add(TEXT("Market.Detail.Factors"), TEXT("Market.Detail"), TEXT("Authoritative causal factors"), EHansaHudSemanticRole::List, TEXT("count"), LexToString(Detail.Factors.Num()));
		for (const auto& Factor : Detail.Factors)
		{
			Add(FString::Printf(TEXT("Market.Detail.Factor.%s"), *SafeId(Factor.StableId.ToString())), TEXT("Market.Detail.Factors"), Factor.AccessibleLabel.ToString(),
				EHansaHudSemanticRole::ListItem, TEXT("basis-points"), LexToString(Factor.ContributionBasisPoints), false, false, false, Factor.ContributionBasisPoints > 0);
		}
		Add(TEXT("Market.Detail.Consumers"), TEXT("Market.Detail"), TEXT("Consumers"), EHansaHudSemanticRole::List, TEXT("count"), LexToString(Detail.Consumers.Num()));
		for (const auto& Item : Detail.Consumers) Add(FString::Printf(TEXT("Market.Detail.Consumer.%s"), *SafeId(Item.StableId.ToString())), TEXT("Market.Detail.Consumers"), Item.AccessibleLabel.ToString(), EHansaHudSemanticRole::ListItem, TEXT("status"), Item.Status.ToString(), false, false, false, Item.bWarning);
		Add(TEXT("Market.Detail.Producers"), TEXT("Market.Detail"), TEXT("Producers"), EHansaHudSemanticRole::List, TEXT("count"), LexToString(Detail.Producers.Num()));
		for (const auto& Item : Detail.Producers) Add(FString::Printf(TEXT("Market.Detail.Producer.%s"), *SafeId(Item.StableId.ToString())), TEXT("Market.Detail.Producers"), Item.AccessibleLabel.ToString(), EHansaHudSemanticRole::ListItem, TEXT("status"), Item.Status.ToString(), false, false, false, Item.bWarning);
		Add(TEXT("Market.Detail.Action.Pin"), TEXT("Market.Detail"), Detail.PinActionLabel.ToString(), EHansaHudSemanticRole::Button, TEXT("availability"),
			Detail.bPinEnabled ? (Detail.bPinned ? TEXT("pinned") : TEXT("available")) : Detail.PinDisabledReason.ToString(), Detail.bPinEnabled, Detail.bPinEnabled, Detail.bPinned);
		Add(TEXT("Market.Detail.Action.BeginRoute"), TEXT("Market.Detail"), Detail.RouteActionLabel.ToString(), EHansaHudSemanticRole::Button, TEXT("availability"),
			Detail.bRouteEnabled ? TEXT("available") : Detail.RouteDisabledReason.ToString(), Detail.bRouteEnabled, Detail.bRouteEnabled);
		if (!Detail.LastActionResult.IsEmpty()) Add(TEXT("Market.Detail.Action.Result"), TEXT("Market.Detail"), Detail.LastActionResult.ToString(), EHansaHudSemanticRole::Status, TEXT("action-result"), Detail.LastActionResult.ToString());
		Add(TEXT("Market.Empty"), TEXT("Market.Root"), Snapshot.EmptyTitle.ToString(), EHansaHudSemanticRole::Status, TEXT("remedy"), Snapshot.EmptyDetail.ToString());
		return Nodes;
	}
}

#undef LOCTEXT_NAMESPACE
