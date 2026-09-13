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
			const int64 Low=FMath::Min(Minimum,Average),High=FMath::Max(Maximum,Average);
            AverageNormalized=High==Low?.5f:float(Average-Low)/float(High-Low);
            for(auto& Point:Points) Point.NormalizedPrice=High==Low?.5f:float(Point.PriceMilliMarks-Low)/float(High-Low);
			Invalidate(EInvalidateWidgetReason::Paint);
		}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320.0f, 150.0f); }
		virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
			FSlateWindowElementList& Elements, const int32 LayerId, const FWidgetStyle& WidgetStyle, const bool bParentEnabled) const override
		{
			const FVector2D Size = Geometry.GetLocalSize();
			const FLinearColor Grid=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk);
			const FLinearColor Brass=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
            // P21 series treatment: dark outline preserves essential line contrast.
            auto PriceLine=[&](const TArray<FVector2D>& PointsToDraw,float Thickness=2.f) {
                FSlateDrawElement::MakeLines(Elements,LayerId+2,Geometry.ToPaintGeometry(),PointsToDraw,ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink),true,Thickness+2);
                FSlateDrawElement::MakeLines(Elements,LayerId+3,Geometry.ToPaintGeometry(),PointsToDraw,ESlateDrawEffect::None,Brass,true,Thickness);
            };
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
					const float X = 8.0f + (Size.X - 16.0f) * static_cast<float>(Points[Index].Tick-Points[0].Tick) / static_cast<float>(FMath::Max<int64>(1,Points.Last().Tick-Points[0].Tick));
					const float Y = 8.0f + (Size.Y - 16.0f) * (1.0f - FMath::Clamp(Points[Index].NormalizedPrice, 0.0f, 1.0f));
					LinePoints.Add(FVector2D(X, Y));
				}
				if(!bStale) PriceLine(LinePoints);
                else for(int32 I=1;I<LinePoints.Num();++I) {
                    const FVector2D A=LinePoints[I-1], B=LinePoints[I]; const double Length=(B-A).Size();
                    for(double D=0;D<Length;D+=12) PriceLine({FMath::Lerp(A,B,D/Length),FMath::Lerp(A,B,FMath::Min(D+7,Length)/Length)});
                }
			}
			if(Points.Num()==1) {
                const double X=Size.X*.5,Y=8+(Size.Y-16)*(1-Points[0].NormalizedPrice);
                PriceLine({FVector2D(X-4,Y),FVector2D(X+4,Y)},5);
            }
            return LayerId + 3;
		}

	private:
		TArray<FHansaMarketChartPointPresentation> Points;
		float AverageNormalized = 0.5f;
		bool bStale = false;
	};

	namespace
	{
		FString MarketTableSafeId(FString Value) { Value.ReplaceInline(TEXT("."), TEXT("_")); return Value; }

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
			case EHansaMarketSortColumn::Stock: return LOCTEXT("StockCompact", "Stock");
			case EHansaMarketSortColumn::Reserve: return LOCTEXT("ReserveCompact", "Reserve");
			case EHansaMarketSortColumn::Demand: return LOCTEXT("DemandCompact", "Demand");
			case EHansaMarketSortColumn::Price: return LOCTEXT("PriceCompact", "Price");
			case EHansaMarketSortColumn::Trend: return LOCTEXT("TrendCompact", "Trend");
			case EHansaMarketSortColumn::Incoming: return LOCTEXT("IncomingCompact", "Incoming");
			case EHansaMarketSortColumn::Status: return LOCTEXT("StatusCompact", "Status");
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
        Preferences = Arguments._Preferences; RowStyle = GetLedgerRowStyle(Preferences.bHighContrast);
		WorkingBrush = GetComponentStyle(EUiSurface::Panel, EUiState::Default, Preferences).Brush;
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

        HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences));
        BodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        DataStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
        CaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences));
        CaptionOnDarkStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences));
        SearchStyle=FCoreStyle::Get().GetWidgetStyle<FSearchBoxStyle>("SearchBox");
        SearchStyle.SetGlassImage(*GetGeneratedIconBrush(EUiGlyph::Search,20));
        SearchStyle.SetClearImage(*GetGeneratedIconBrush(EUiGlyph::Close,20));
        SearchStyle.SetUpArrowImage(*GetGeneratedIconBrush(EUiGlyph::Up,20));
        SearchStyle.SetDownArrowImage(*GetGeneratedIconBrush(EUiGlyph::Down,20));
        SearchStyle.GlassImage.ImageSize=FVector2D(20,20);SearchStyle.ClearImage.ImageSize=FVector2D(20,20);
        SearchStyle.TextBoxStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        SearchStyle.TextBoxStyle.SetForegroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink));
        SearchStyle.TextBoxStyle.SetBackgroundColor(FLinearColor::White);
        SearchStyle.TextBoxStyle.SetBackgroundImageNormal(WorkingBrush).SetBackgroundImageHovered(WorkingBrush).SetBackgroundImageFocused(WorkingBrush).SetBackgroundImageReadOnly(WorkingBrush);
        SearchStyle.TextBoxStyle.SetTextStyle(BodyStyle);
        SearchStyle.TextBoxStyle.TextStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        SearchStyle.TextBoxStyle.TextStyle.SetColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink));
        const auto Surface = GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences);
        HeadingStyle.SetColorAndOpacity(Surface.Foreground); BodyStyle.SetColorAndOpacity(Surface.Foreground);
        DataStyle.SetColorAndOpacity(Surface.Foreground); CaptionStyle.SetColorAndOpacity(Surface.Foreground);

		auto Header = [this](const TCHAR* Label, const EHansaMarketSortColumn Column, const float Width)
		{
			TSharedPtr<SHansaAction> Button;

			TSharedRef<SBox> Box = SNew(SBox)
			[
				SAssignNew(Button, SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Preferences).Compact(false)
				.ToolTipText(FText::Format(LOCTEXT("SortColumnTip", "Sort by {0}"), FText::FromString(SortName(Column))))
				.OnClicked(this, &SHansaMarketTable::InvokeSort, Column)
			];
			HeaderButtons.Add(Column, Button);

			MapWidget(FString::Printf(TEXT("Market.Header.%s"), Label), Button);
			return Box;
		};
		auto Metric = [this](const FText& Label, TSharedPtr<STextBlock>& Value)
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Label).TextStyle(&CaptionStyle).AutoWrapText(true)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 8.0f)[SAssignNew(Value, STextBlock).TextStyle(&DataStyle).AutoWrapText(true)];
		};

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.60f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(SearchBox, SSearchBox).Style(&SearchStyle).DelayChangeNotificationsWhileTyping(false).HintText(LOCTEXT("Search", "Search goods")).OnTextChanged(this, &SHansaMarketTable::HandleSearchChanged)]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f)[SAssignNew(ResultText, STextBlock).TextStyle(&CaptionOnDarkStyle)]]
                +SVerticalBox::Slot().AutoHeight().MaxHeight(TAttribute<float>::CreateLambda([this]{ return FMath::Clamp(float(GetCachedGeometry().GetLocalSize().Y-40.f)*.4f,48.f/FMath::Min(1.f,Preferences.UiScale),160.f/FMath::Min(1.f,Preferences.UiScale)); })).Padding(0,4)[SAssignNew(ControlScroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(CategoryButton, SHansaAction).Preferences(Preferences).Compact(false).OnClicked(this, &SHansaMarketTable::InvokeCategory)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(TrendButton, SHansaAction).Preferences(Preferences).Compact(false).OnClicked(this, &SHansaMarketTable::InvokeTrend)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(QuickButton, SHansaAction).Preferences(Preferences).Compact(false).OnClicked(this, &SHansaMarketTable::InvokeQuick)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(ClearButton, SHansaAction).Preferences(Preferences).Compact(false).Label(LOCTEXT("Clear", "Clear filters")).OnClicked(this, &SHansaMarketTable::InvokeClear)]]

                + SVerticalBox::Slot().AutoHeight().Padding(2,4)[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.5f)[Header(TEXT("Good"),EHansaMarketSortColumn::Good,0)]
                    + SHorizontalBox::Slot().FillWidth(1)[Header(TEXT("Stock"),EHansaMarketSortColumn::Stock,0)]
                    + SHorizontalBox::Slot().FillWidth(1)[Header(TEXT("Reserve"),EHansaMarketSortColumn::Reserve,0)]
                    + SHorizontalBox::Slot().FillWidth(1)[Header(TEXT("Demand"),EHansaMarketSortColumn::Demand,0)]]
                + SVerticalBox::Slot().AutoHeight().Padding(2,0,2,4)[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1)[Header(TEXT("Price"),EHansaMarketSortColumn::Price,0)]
                    + SHorizontalBox::Slot().FillWidth(1)[Header(TEXT("Trend"),EHansaMarketSortColumn::Trend,0)]
                    + SHorizontalBox::Slot().FillWidth(1)[Header(TEXT("Incoming"),EHansaMarketSortColumn::Incoming,0)]
                    + SHorizontalBox::Slot().FillWidth(1)[Header(TEXT("Status"),EHansaMarketSortColumn::Status,0)]]

                ]]

				+ SVerticalBox::Slot().FillHeight(1.f).Padding(2.0f)[SNew(SOverlay)
					+ SOverlay::Slot()[SAssignNew(ListPanel, SBorder).BorderImage(&WorkingBrush).Padding(2.0f)[SAssignNew(ListView, SListView<TSharedPtr<FHansaMarketTableRowPresentation>>).ListItemsSource(&Items).SelectionMode(ESelectionMode::Single).OnItemScrolledIntoView_Lambda([this](TSharedPtr<FHansaMarketTableRowPresentation> Item,const TSharedPtr<ITableRow>&){
                        if(Item && Model.IsValid() && Model->GetSnapshot().FocusedSemanticId==FName(*RowId(Item->GoodStableId)))
                            if(auto W=ResolveSemanticWidget(RowId(Item->GoodStableId))) FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);
                    }).OnGenerateRow(this, &SHansaMarketTable::GenerateRow).OnSelectionChanged(this, &SHansaMarketTable::HandleRowSelectionChanged)]]
					+ SOverlay::Slot()[SAssignNew(EmptyPanel, SBorder).BorderImage(&DecisionBrush).Padding(24.0f)[SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(EmptyTitle, STextBlock).TextStyle(&HeadingStyle)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)[SAssignNew(EmptyDetail, STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]]]]
			]
			+ SHorizontalBox::Slot().FillWidth(0.40f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[SAssignNew(DetailEmptyPanel, SBorder).BorderImage(&DecisionBrush).Padding(24.0f)[SNew(STextBlock).Text(LOCTEXT("SelectDetail", "Select a good to inspect its price, supply, demand and price factors.")).TextStyle(&BodyStyle).AutoWrapText(true)]]
				+ SOverlay::Slot()[SAssignNew(DetailContentPanel, SBorder).BorderImage(&WorkingBrush).Padding(12.0f)[SAssignNew(DetailScroll,SScrollBox).AnimateWheelScrolling(false)
					+ SScrollBox::Slot()[SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[SAssignNew(DetailTitle, STextBlock).TextStyle(&HeadingStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 10.0f)[SAssignNew(DetailConfidence, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
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

                        + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).Padding(3)[Metric(LOCTEXT("ProductionMetric","Production / tick"),ProductionText)]
                            + SHorizontalBox::Slot().FillWidth(1).Padding(3)[Metric(LOCTEXT("ConsumptionMetric","Consumed / tick"),ConsumptionText)]]
                        + SVerticalBox::Slot().AutoHeight().Padding(3,8)[SAssignNew(SupplyBalanceText,STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]
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
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(PinButton, SHansaAction).Preferences(Preferences).Compact(true).OnClicked(this, &SHansaMarketTable::InvokePin)]
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)[SAssignNew(RouteButton, SHansaAction).Preferences(Preferences).Compact(true).OnClicked(this, &SHansaMarketTable::InvokeRoute)]]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)[SAssignNew(PinReasonText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)[SAssignNew(RouteReasonText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(2.0f)[SAssignNew(ActionResultText, STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]]]]
			]
		];

		MapWidget(TEXT("Market.Root"), SharedThis(this));
        MapWidget(TEXT("Market.Controls"),ControlScroll);
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
        MapWidget(TEXT("Market.Detail.Metric.BaseValue"),BaseValueText);
        MapWidget(TEXT("Market.Detail.Metric.LocalPrice"),LocalPriceText);
        MapWidget(TEXT("Market.Detail.Metric.RecentAverageDifference"),DifferenceText);
        MapWidget(TEXT("Market.Detail.Metric.StockVersusReserve"),StockReserveText);
        MapWidget(TEXT("Market.Detail.Metric.ReserveDays"),ReserveDaysText);
        MapWidget(TEXT("Market.Detail.Metric.CitizenDemand"),CitizenDemandText);
        MapWidget(TEXT("Market.Detail.Metric.IndustrialDemand"),IndustrialDemandText);
        MapWidget(TEXT("Market.Detail.Metric.IncomingSupply"),IncomingSupplyText);
        MapWidget(TEXT("Market.Detail.Metric.Production"),ProductionText);
        MapWidget(TEXT("Market.Detail.Metric.Consumption"),ConsumptionText);
        MapWidget(TEXT("Market.Detail.Metric.SupplyBalance"),SupplyBalanceText);
        MapWidget(TEXT("Market.Detail.Chart"),PriceChart);
        MapWidget(TEXT("Market.Detail.Summary"),ExplanationText);
		MapWidget(TEXT("Market.Detail.Action.BeginRoute"), RouteButton);
		if (UHansaMarketTablePresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaMarketTable::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
	}

	FString SHansaMarketTable::RowId(const FName GoodStableId) { return FString::Printf(TEXT("Market.Row.%s"), *MarketTableSafeId(GoodStableId.ToString())); }
	FString SHansaMarketTable::CellId(const FName GoodStableId, const TCHAR* Column) { return FString::Printf(TEXT("%s.%s"), *RowId(GoodStableId), Column); }

	TSharedRef<ITableRow> SHansaMarketTable::GenerateRow(TSharedPtr<FHansaMarketTableRowPresentation> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{

        TSharedPtr<SHansaAction> GoodButton;
        auto TextCell = [this,Item](const FText& Label,FText FHansaMarketTableRowPresentation::* Field) {
            return SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Label).TextStyle(&CaptionStyle).AutoWrapText(true).Justification(ETextJustify::Right)]
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Item,Field]{ return Item.Get()->*Field; }).TextStyle(&DataStyle).AutoWrapText(true).Justification(ETextJustify::Right)];
        };
        auto Result = SNew(STableRow<TSharedPtr<FHansaMarketTableRowPresentation>>,OwnerTable).Style(&RowStyle).Padding(4)
        [
            SNew(SBorder).BorderImage(&WorkingBrush).Padding(8)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                     + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)[SNew(SHansaGlyph).Glyph(GlyphForGood(Item->GoodStableId)).Size(24)]
                    + SHorizontalBox::Slot().FillWidth(1.5f).Padding(0,0,8,0)[SAssignNew(GoodButton,SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Preferences).Compact(false).Label(Item->GoodLabel)
                        .OnClicked(this,&SHansaMarketTable::InvokeRow,Item->GoodStableId).ToolTipText_Lambda([Item]{return Item->AccessibleLabel;})]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(4).VAlign(VAlign_Center)[TextCell(LOCTEXT("RowStock","Stock"),&FHansaMarketTableRowPresentation::Stock)]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(4).VAlign(VAlign_Center)[TextCell(LOCTEXT("RowReserve","Reserve"),&FHansaMarketTableRowPresentation::Reserve)]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(4).VAlign(VAlign_Center)[TextCell(LOCTEXT("RowDemand","Demand"),&FHansaMarketTableRowPresentation::Demand)]
                    + SHorizontalBox::Slot().FillWidth(1.2f).Padding(4).VAlign(VAlign_Center)[TextCell(LOCTEXT("RowPrice","Price"),&FHansaMarketTableRowPresentation::Price)]]
                + SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(STextBlock).Text_Lambda([Item]{
                    return FText::Format(LOCTEXT("RowSecondary","{0} · Incoming {1} · {2} · {3}"), Item->Trend,Item->Incoming,Item->Status,Item->ReportAge);
                }).TextStyle(&CaptionStyle).AutoWrapText(true)]
            ]
        ];
        MapWidget(RowId(Item->GoodStableId),GoodButton);
        GoodButton->SetFocusHandler(FSimpleDelegate::CreateLambda([Weak=Model,Id=FName(*RowId(Item->GoodStableId))]{if(Weak.IsValid())Weak->SetFocusedSemanticId(Id);}));
        GoodButton->SetState(Model.IsValid() && Model->GetSnapshot().SelectedGoodStableId==Item->GoodStableId ? EUiState::Selected:EUiState::Default,FText());
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
		CategoryButton->SetLabel(CategoryText(Snapshot.CategoryFilter));
		TrendButton->SetLabel(TrendText(Snapshot.TrendFilter));
		QuickButton->SetLabel(QuickText(Snapshot.QuickFilter));
		if (!SearchBox->GetText().EqualTo(Snapshot.SearchText)) SearchBox->SetText(Snapshot.SearchText);
		ResultText->SetText(Snapshot.ResultSummary);
		for (const auto& Entry : HeaderButtons)
		{
			const EHansaMarketSortColumn Column = Entry.Key;
			const FString Arrow = Snapshot.SortColumn == Column ? (Snapshot.bSortAscending ? TEXT(" asc") : TEXT(" desc")) : FString();
			Entry.Value->SetLabel(FText::Format(LOCTEXT("CompactSortState", "{0}{1}"), CompactSortLabel(Column), FText::FromString(Arrow)));
			Entry.Value->SetState(Snapshot.SortColumn == Column ? EUiState::Selected : EUiState::Default, FText());
		}
		EmptyTitle->SetText(Snapshot.EmptyTitle); EmptyDetail->SetText(Snapshot.EmptyDetail);
		const bool bEmpty = Snapshot.VisibleRows.IsEmpty();
		ListPanel->SetVisibility(bEmpty ? EVisibility::Collapsed : EVisibility::Visible);
		EmptyPanel->SetVisibility(bEmpty ? EVisibility::Visible : EVisibility::Collapsed);
		bool bRowsChanged = PresentedRows.Num() != Snapshot.VisibleRows.Num();
		for (int32 Index = 0; !bRowsChanged && Index < PresentedRows.Num(); ++Index) bRowsChanged = !(PresentedRows[Index] == Snapshot.VisibleRows[Index]);

		if (bRowsChanged) RebuildItems(Snapshot);
        for(const auto& Item : Items) if(const auto* W=SemanticWidgets.Find(RowId(Item->GoodStableId))) if(auto B=W->Pin()) StaticCastSharedPtr<SHansaAction>(B)->SetState(Item->GoodStableId==Snapshot.SelectedGoodStableId?EUiState::Selected:EUiState::Default,FText());
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
		DetailTitle->SetText(Detail.GoodLabel);
		DetailConfidence->SetText(Detail.Confidence);
		BaseValueText->SetText(Detail.BaseValue); LocalPriceText->SetText(Detail.LocalPrice); DifferenceText->SetText(Detail.RecentAverageDifference);
		StockReserveText->SetText(Detail.StockVersusReserve); ReserveDaysText->SetText(Detail.ReserveDays);
		CitizenDemandText->SetText(Detail.CitizenDemand); IndustrialDemandText->SetText(Detail.IndustrialDemand); IncomingSupplyText->SetText(Detail.IncomingSupply);
		ProductionText->SetText(Detail.Production); ConsumptionText->SetText(Detail.Consumption); SupplyBalanceText->SetText(Detail.SupplyBalance);
		ExplanationText->SetText(Detail.Explanation); ChartSummaryText->SetText(Detail.ChartSummary);
		PinButton->SetState(Detail.bPinEnabled?EUiState::Default:EUiState::Disabled,Detail.PinDisabledReason); RouteButton->SetState(Detail.bRouteEnabled?EUiState::Default:EUiState::Disabled,Detail.RouteDisabledReason);
		PinButton->SetLabel(Detail.bPinned ? LOCTEXT("Unpin", "Unpin price & stock") : Detail.PinActionLabel);
		RouteButton->SetLabel(Detail.RouteActionLabel);
		PinReasonText->SetText(Detail.PinDisabledReason); PinReasonText->SetVisibility(!Detail.bPinEnabled && !Detail.PinDisabledReason.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed);
		RouteReasonText->SetText(Detail.RouteDisabledReason); RouteReasonText->SetVisibility(!Detail.bRouteEnabled && !Detail.RouteDisabledReason.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed);
		ActionResultText->SetText(Detail.LastActionResult); ActionResultText->SetVisibility(Detail.LastActionResult.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
		PriceChart->SetData(Detail.History, Detail.RecentAveragePriceMilliMarks, Detail.MinimumHistoryPriceMilliMarks, Detail.MaximumHistoryPriceMilliMarks, Detail.bStale);
		const auto PreviouslyFocused=ResolveSemanticWidget(Snapshot.FocusedSemanticId.ToString());
        const bool bRestoreDetailFocus=Snapshot.FocusedSemanticId.ToString().StartsWith(TEXT("Market.Detail.")) && PreviouslyFocused && PreviouslyFocused->HasKeyboardFocus();
        const bool bRelationshipsChanged=PresentedDetail.Factors!=Detail.Factors || PresentedDetail.Consumers!=Detail.Consumers || PresentedDetail.Producers!=Detail.Producers;
        if (bRelationshipsChanged) RebuildDetailLists(Detail);
		PresentedDetail = Detail;
		FocusOrder = { TEXT("Market.Search"), TEXT("Market.Filter.Category"), TEXT("Market.Filter.Trend"), TEXT("Market.Filter.Quick"), TEXT("Market.Filter.Clear") };
		for(int32 Index=0;Index<8;++Index) FocusOrder.Add(TEXT("Market.Header.")+SortName(static_cast<EHansaMarketSortColumn>(Index)));
		for (const auto& Row : Snapshot.VisibleRows) FocusOrder.Add(RowId(Row.GoodStableId));
		for(const auto& R:Detail.Consumers) if(R.BuildingValue>0) FocusOrder.Add(TEXT("Market.Detail.Consumer.")+MarketTableSafeId(R.StableId.ToString()));
        for(const auto& R:Detail.Producers) if(R.BuildingValue>0) FocusOrder.Add(TEXT("Market.Detail.Producer.")+MarketTableSafeId(R.StableId.ToString()));
        if (Detail.bPinEnabled) FocusOrder.Add(TEXT("Market.Detail.Action.Pin"));
		if (Detail.bRouteEnabled) FocusOrder.Add(TEXT("Market.Detail.Action.BeginRoute"));
        if(bRelationshipsChanged && bRestoreDetailFocus) FocusSemanticId(Snapshot.FocusedSemanticId.ToString());
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
		auto AddRelationships = [this](const TArray<FHansaMarketRelationshipPresentation>& Relationships, const TSharedPtr<SVerticalBox>& List, bool bProducer)
		{
			if (Relationships.IsEmpty()) List->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("NoneReported", "None reported")).TextStyle(&CaptionStyle)];
			for (const auto& Relationship : Relationships)
			{
				TSharedPtr<SHansaAction> Reveal;
                const FString Id=FString(bProducer?TEXT("Market.Detail.Producer."):TEXT("Market.Detail.Consumer."))+MarketTableSafeId(Relationship.StableId.ToString());
                List->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SAssignNew(Reveal,SHansaAction).Preferences(Preferences).Compact(false)
                        .Label(FText::Format(LOCTEXT("RevealBuilding","Reveal {0}"),Relationship.Label))
                        .State(Relationship.BuildingValue>0?EUiState::Default:EUiState::Disabled)
                        .Reason(LOCTEXT("NoBuilding","This source has no world building."))
                        .OnClicked_Lambda([Weak=Model,bProducer,StableId=Relationship.StableId]{return Weak.IsValid() && Weak->RevealRelationshipIntent(bProducer,StableId)?FReply::Handled():FReply::Unhandled();})]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Relationship.Label).TextStyle(&CaptionStyle)]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::Format(LOCTEXT("RelationshipDetail", "{0} · {1}"), Relationship.Detail, Relationship.Status)).TextStyle(&CaptionStyle).AutoWrapText(true)]];
                MapWidget(Id,Reveal);
			}
		};
		AddRelationships(Detail.Consumers, ConsumerList, false);
		AddRelationships(Detail.Producers, ProducerList, true);
	}

	void SHansaMarketTable::RebuildItems(const FHansaMarketTableSnapshot& Snapshot)
	{
		auto PreviousItems = MoveTemp(Items); Items.Reset(); PresentedRows = Snapshot.VisibleRows;
        for(const auto& Row:Snapshot.VisibleRows) {
            auto* Existing=PreviousItems.FindByPredicate([&Row](const auto& Item){return Item->GoodStableId==Row.GoodStableId;});
            auto Item=Existing?*Existing:MakeShared<FHansaMarketTableRowPresentation>();
            *Item=Row; Items.Add(Item);
        }
		ListView->RequestListRefresh();
#if WITH_DEV_AUTOMATION_TESTS
		++ListRefreshCount;
#endif
	}

	FReply SHansaMarketTable::InvokeCategory() { return Model.IsValid() && Model->CycleCategoryFilterIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeTrend() { return Model.IsValid() && Model->CycleTrendFilterIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeQuick() { return Model.IsValid() && Model->CycleQuickFilterIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeClear()
    {
        // A valid button activation is consumed even when the filters are already clear.
        if (Model.IsValid()) Model->ClearFiltersIntent();
        return FReply::Handled();
    }
	FReply SHansaMarketTable::InvokeSort(const EHansaMarketSortColumn Column) { return Model.IsValid() && Model->SortByIntent(Column) ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeRow(const FName GoodStableId)
    {
        // Reselecting a good is a harmless no-op, not an unhandled input event.
        if (Model.IsValid() && Model->GetSnapshot().VisibleRows.ContainsByPredicate(
            [GoodStableId](const auto& Row) { return Row.GoodStableId == GoodStableId; }))
        {
            Model->SelectGoodIntent(GoodStableId);
        }
        return FReply::Handled();
    }
	FReply SHansaMarketTable::InvokePin() { return Model.IsValid() && Model->TogglePinIntent() ? FReply::Handled() : FReply::Unhandled(); }
	FReply SHansaMarketTable::InvokeRoute() { return Model.IsValid() && Model->BeginRouteIntent() ? FReply::Handled() : FReply::Unhandled(); }
	void SHansaMarketTable::HandleSearchChanged(const FText& Text) { if (Model.IsValid()) Model->SetSearchTextIntent(Text); }
	void SHansaMarketTable::MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget) {
        SemanticWidgets.Add(SemanticId, Widget);
        if(SemanticId.StartsWith(TEXT("Market.Filter.")) || SemanticId.StartsWith(TEXT("Market.Header.")) ||
           SemanticId.StartsWith(TEXT("Market.Row.")) || SemanticId.StartsWith(TEXT("Market.Detail.Action.")) ||
           SemanticId.StartsWith(TEXT("Market.Detail.Consumer.")) || SemanticId.StartsWith(TEXT("Market.Detail.Producer.")))
            StaticCastSharedPtr<SHansaAction>(Widget)->SetFocusHandler(FSimpleDelegate::CreateLambda([Weak=Model,Id=FName(*SemanticId)]{if(Weak.IsValid())Weak->SetFocusedSemanticId(Id);}));
    }

    void SHansaMarketTable::Tick(const FGeometry& Geometry,double Time,float Delta) {
        SCompoundWidget::Tick(Geometry,Time,Delta);
        if(PendingDetailScroll.IsEmpty())return;
        const auto Widget=ResolveSemanticWidget(PendingDetailScroll);
        if(!Model.IsValid() || Model->GetSnapshot().FocusedSemanticId!=FName(*PendingDetailScroll) || !Widget || ++ScrollLayoutAttempts>8){PendingDetailScroll.Reset();return;}
        const auto Clip=DetailScroll->GetCachedGeometry();
        const auto Target=Widget->GetCachedGeometry();
        if(Target.GetAbsoluteSize().Y>0 && Target.GetAbsolutePosition().Y>=Clip.GetAbsolutePosition().Y &&
            Target.GetAbsolutePosition().Y+Target.GetAbsoluteSize().Y<=Clip.GetAbsolutePosition().Y+Clip.GetAbsoluteSize().Y) {PendingDetailScroll.Reset();return;}
        // Auto-wrapped rows can change height after the first layout pass.
        DetailScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::Center);
    }

    FReply SHansaMarketTable::OnKeyDown(const FGeometry&,const FKeyEvent& Event) {
        if(!Model.IsValid() || FocusOrder.IsEmpty())return FReply::Unhandled();
        const auto Key=Event.GetKey();
        if(Key!=EKeys::Tab && Key!=EKeys::Gamepad_DPad_Down && Key!=EKeys::Gamepad_DPad_Up)return FReply::Unhandled();
        const bool Forward=(Key==EKeys::Tab && !Event.IsShiftDown()) || Key==EKeys::Gamepad_DPad_Down;
        int32 Index=FocusOrder.IndexOfByKey(Model->GetSnapshot().FocusedSemanticId.ToString());
        Index=Index==INDEX_NONE?0:(Index+(Forward?1:FocusOrder.Num()-1))%FocusOrder.Num();
        return FocusSemanticId(FocusOrder[Index])?FReply::Handled():FReply::Unhandled();
    }

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
        for(const auto& R:Model->GetSnapshot().SelectedGood.Consumers) if(SemanticId==TEXT("Market.Detail.Consumer.")+MarketTableSafeId(R.StableId.ToString())) return Model->RevealRelationshipIntent(false,R.StableId);
        for(const auto& R:Model->GetSnapshot().SelectedGood.Producers) if(SemanticId==TEXT("Market.Detail.Producer.")+MarketTableSafeId(R.StableId.ToString())) return Model->RevealRelationshipIntent(true,R.StableId);
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
		for (const auto& Row : Model->GetSnapshot().VisibleRows) if (SemanticId == RowId(Row.GoodStableId)) return Model->SelectGoodIntent(Row.GoodStableId);
		return false;
	}

	bool SHansaMarketTable::FocusSemanticId(const FString& SemanticId)
	{
		if(!FocusOrder.Contains(SemanticId)) return false;
		if(SemanticId.StartsWith(TEXT("Market.Row."))) for(const auto& Item:Items)
            if(SemanticId==RowId(Item->GoodStableId)) ListView->RequestScrollIntoView(Item);
        const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found != nullptr ? Found->Pin() : nullptr;
		if (!Widget.IsValid() && SemanticId.StartsWith(TEXT("Market.Row.")))
		{
			for (const auto& Item : Items) if (Item.IsValid() && SemanticId == RowId(Item->GoodStableId))
			{
				ListView->RequestScrollIntoView(Item);
				if (Model.IsValid()) Model->SetFocusedSemanticId(FName(*SemanticId));
				if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(ListView, EFocusCause::Navigation);
				return true;
			}
		}
		if (!Widget.IsValid() || !Widget->IsEnabled()) return false;
        if(SemanticId.StartsWith(TEXT("Market.Detail."))) {
            PendingDetailScroll=SemanticId;ScrollLayoutAttempts=0;
            DetailScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::Center);
        }
		if (Model.IsValid()) Model->SetFocusedSemanticId(FName(*SemanticId));
		if(ControlScroll && (SemanticId.StartsWith(TEXT("Market.Header.")) || SemanticId.StartsWith(TEXT("Market.Filter."))))ControlScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);
        if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);
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
                const auto G=Widget->GetCachedGeometry(); const auto P=G.GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition(); const auto Size=G.GetDrawSize();
                Node.Bounds=FIntRect(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.X+Size.X),FMath::RoundToInt(P.Y+Size.Y));
                if(Id.StartsWith(TEXT("Market.Detail."))) {
                    const auto Clip=DetailScroll->GetCachedGeometry();
                    const auto Min=Clip.GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition(),Max=Min+Clip.GetDrawSize();
                    Node.State.bVisible &= Snapshot.SelectedGood.bHasSelection && P.Y+Size.Y>Min.Y && P.Y<Max.Y;
                }
			}
			if(Id.StartsWith(TEXT("Market.Detail.")) && Id!=TEXT("Market.Detail.Empty")) Node.State.bVisible &= Snapshot.SelectedGood.bHasSelection;
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
				{ TEXT("Demand"), &Row.Demand }, { TEXT("Price"), &Row.Price }, { TEXT("Trend"), &Row.Trend }, { TEXT("Incoming"), &Row.Incoming }, { TEXT("Status"), &Row.Status }, { TEXT("ReportAge"), &Row.ReportAge } })
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
			{ TEXT("CitizenDemand"), &Detail.CitizenDemand }, { TEXT("IndustrialDemand"), &Detail.IndustrialDemand }, { TEXT("IncomingSupply"), &Detail.IncomingSupply }, { TEXT("Production"), &Detail.Production }, { TEXT("Consumption"), &Detail.Consumption }, { TEXT("SupplyBalance"), &Detail.SupplyBalance } })
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
			Add(FString::Printf(TEXT("Market.Detail.Factor.%s"), *MarketTableSafeId(Factor.StableId.ToString())), TEXT("Market.Detail.Factors"), Factor.AccessibleLabel.ToString(),
				EHansaHudSemanticRole::ListItem, TEXT("basis-points"), LexToString(Factor.ContributionBasisPoints), false, false, false, Factor.ContributionBasisPoints > 0);
		}
		Add(TEXT("Market.Detail.Consumers"), TEXT("Market.Detail"), TEXT("Consumers"), EHansaHudSemanticRole::List, TEXT("count"), LexToString(Detail.Consumers.Num()));
		for (const auto& Item : Detail.Consumers) Add(FString::Printf(TEXT("Market.Detail.Consumer.%s"), *MarketTableSafeId(Item.StableId.ToString())), TEXT("Market.Detail.Consumers"), Item.AccessibleLabel.ToString(), EHansaHudSemanticRole::ListItem, TEXT("building-id"), LexToString(Item.BuildingValue), Item.BuildingValue>0, Item.BuildingValue>0, false, Item.bWarning);
		Add(TEXT("Market.Detail.Producers"), TEXT("Market.Detail"), TEXT("Producers"), EHansaHudSemanticRole::List, TEXT("count"), LexToString(Detail.Producers.Num()));
		for (const auto& Item : Detail.Producers) Add(FString::Printf(TEXT("Market.Detail.Producer.%s"), *MarketTableSafeId(Item.StableId.ToString())), TEXT("Market.Detail.Producers"), Item.AccessibleLabel.ToString(), EHansaHudSemanticRole::ListItem, TEXT("building-id"), LexToString(Item.BuildingValue), Item.BuildingValue>0, Item.BuildingValue>0, false, Item.bWarning);
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
