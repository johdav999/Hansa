// Included inside Hansa::UI after the native price chart definition.
// All values/controls are Slate widgets. Only the pictorial header and shared
// ornament/icon/linen assets are raster artwork.
namespace CommodityReport
{
static FText Quantity(int64 Raw)
{
    FNumberFormattingOptions Format; Format.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
    return FText::AsNumber(double(Raw)/1000., &Format);
}
static FText Percent(int32 BasisPoints)
{
    FNumberFormattingOptions Format; Format.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
    return FText::Format(LOCTEXT("ReportPercent", "{0}%"), FText::AsNumber(double(BasisPoints)/100., &Format));
}
static const FSlateBrush* Linen()
{
    static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeWorkspace/Linen.png"))), FVector2D(512), FLinearColor::White, ESlateBrushTileType::Both);
    return &Brush;
}
static const FSlateBrush* Hides()
{
    static FSlateDynamicImageBrush Brush(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/CommodityReport/RawHidesHeader.png"))), FVector2D(512,256));
    return &Brush;
}
}

class SHansaMarketReserveBar final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaMarketReserveBar) {} SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    void SetData(int64 InStock, int64 InReserve, bool InKnown)
    { Stock=FMath::Max<int64>(0,InStock); Reserve=FMath::Max<int64>(0,InReserve); bKnown=InKnown; Invalidate(EInvalidateWidgetReason::Paint); }
    FVector2D ComputeDesiredSize(float) const override { return FVector2D(100,22); }
    int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& O, int32 L, const FWidgetStyle& Style, bool) const override
    {
        const auto Z=G.GetLocalSize(); const auto* White=FCoreStyle::Get().GetBrush("WhiteBrush");
        auto Box=[&](float X,float Width,EHansaUiColorToken Color){ if(Width>0) FSlateDrawElement::MakeBox(O,L,G.ToPaintGeometry(FVector2D(Width,Z.Y-6),FSlateLayoutTransform(FVector2D(X,3))),White,ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(Color)*Style.GetColorAndOpacityTint()); };
        Box(0,Z.X,EHansaUiColorToken::Parchment);
        if(bKnown)
        {
            const double Extent=double(FMath::Max<int64>(1,FMath::Max(Stock,Reserve)));
            const float Marker=Z.X*float(Reserve/Extent), End=Z.X*float(Stock/Extent);
            Box(0,FMath::Min(Marker,End),EHansaUiColorToken::Brass);
            Box(Marker,FMath::Max(0.f,End-Marker),EHansaUiColorToken::ProsperityTeal);
            FSlateDrawElement::MakeLines(O,L+2,G.ToPaintGeometry(),TArray<FVector2D>{{Marker,0},{Marker,Z.Y}},ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink),true,2);
        }
        FSlateDrawElement::MakeLines(O,L+1,G.ToPaintGeometry(),TArray<FVector2D>{{0,3},{Z.X,3},{Z.X,Z.Y-3},{0,Z.Y-3},{0,3}},ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink),true,1);
        return L+2;
    }
private: int64 Stock=0,Reserve=0; bool bKnown=false;
};

TSharedRef<SWidget> SHansaMarketTable::BuildCommodityReport()
{
    const auto Ink=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink);
    const auto Chalk=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk);
    ReportTitleStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1,true);
    ReportTitleStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
    ReportPriceStyle=DataStyle;
    auto PriceFont=GetComponentFont(EHansaUiTypographyToken::Display,Preferences); PriceFont.Size*=4.f/3.f;
    ReportPriceStyle.SetFont(PriceFont);
    ReportBodyStyle=BodyStyle;
    ReportSmallStyle=BodyStyle; auto SmallFont=GetComponentFont(EHansaUiTypographyToken::SerifBody,Preferences); SmallFont.Size*=.875f; ReportSmallStyle.SetFont(SmallFont);
    ReportSectionStyle=HeadingStyle;

    auto Rule=[this](){return SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass)*.55f).Padding(0)];};
    auto Section=[&](FText Text){return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,3)[SNew(STextBlock).Text(Text).TextStyle(&ReportSectionStyle).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight()[Rule()];};
    auto Metric=[this](FText Label,TSharedPtr<STextBlock>& Value){return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Label).TextStyle(&ReportSmallStyle).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(Value,STextBlock).TextStyle(&DataStyle).AutoWrapText(true)];};
    auto StockMetric=[this](FText Label,TSharedPtr<STextBlock>& Value){return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(Value,STextBlock).TextStyle(&HeadingStyle).Justification(ETextJustify::Center).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Label).TextStyle(&ReportSmallStyle).Justification(ETextJustify::Center).AutoWrapText(true)];};
    auto Relationships=[this](bool Producer){return SNew(SHansaReferenceFrame).Dark(false).Padding(10)
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[SNew(SHansaGlyph).Glyph(Producer?EUiGlyph::Tools:EUiGlyph::People).Size(32)]
         +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Producer?LOCTEXT("ReportProducers","Producers"):LOCTEXT("ReportConsumers","Consumers")).TextStyle(&HeadingStyle).AutoWrapText(true)]
          +SVerticalBox::Slot().AutoHeight()[Producer?SAssignNew(ProducerList,SVerticalBox):SAssignNew(ConsumerList,SVerticalBox)]]];};

    TSharedRef<SVerticalBox> Quay=SNew(SVerticalBox);
    // Existing quay transaction workflow is assembled here without changing its intents.
    Quay->AddSlot().AutoHeight()[BuildQuayTrade()];

    auto Analysis=SAssignNew(MarketAnalysisPanel,SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,2,0,0)[SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(2).Padding(0,0,12,0)[SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ReportLocalPrice","Local price")).TextStyle(&ReportBodyStyle)]
          +SVerticalBox::Slot().AutoHeight()[SAssignNew(LocalPriceText,STextBlock).TextStyle(&ReportPriceStyle).AutoWrapText(true)]]
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Metric(LOCTEXT("ReportBase","Base value"),BaseValueText)]]
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ReportVsAverage","vs recent average")).TextStyle(&ReportSmallStyle)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,1,0,4)[SAssignNew(DifferenceText,STextBlock).TextStyle(&DataStyle).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("ReportStockReserve","Stock & reserve"))]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1)[StockMetric(LOCTEXT("ReportStock","Stock"),ReportStockText)]
         +SHorizontalBox::Slot().FillWidth(1)[StockMetric(LOCTEXT("ReportReserve","Desired reserve"),ReportReserveText)]
         +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight()[SAssignNew(ReportSurplusText,STextBlock).TextStyle(&HeadingStyle).Justification(ETextJustify::Center)]
          +SVerticalBox::Slot().AutoHeight()[SAssignNew(ReportBalanceLabel,STextBlock).TextStyle(&ReportSmallStyle).Justification(ETextJustify::Center).AutoWrapText(true)]]]
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(ReserveBar,SHansaMarketReserveBar)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ReserveDaysText,STextBlock).TextStyle(&ReportSmallStyle).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(StockReserveText,STextBlock).TextStyle(&ReportSmallStyle).Visibility(EVisibility::Collapsed)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SNew(SBorder).BorderImage(&DecisionBrush).Padding(8)
         [SNew(SHorizontalBox)
          +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Information).Size(20)]
          +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(SupplyBalanceText,STextBlock).TextStyle(&ReportSmallStyle).AutoWrapText(true)]]]
        +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("ReportSupplyDemand","Supply & demand"))]
        +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,3)[SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8,4))
         +SWrapBox::Slot().FillEmptySpace(true)[SNew(SBox).WidthOverride(Preferences.bLargeText?138:96)[Metric(LOCTEXT("ReportCitizenDemand","Citizen demand"),CitizenDemandText)]]
         +SWrapBox::Slot().FillEmptySpace(true)[SNew(SBox).WidthOverride(Preferences.bLargeText?138:96)[Metric(LOCTEXT("ReportIndustrialDemand","Industrial demand"),IndustrialDemandText)]]
         +SWrapBox::Slot().FillEmptySpace(true)[SNew(SBox).WidthOverride(Preferences.bLargeText?138:96)[Metric(LOCTEXT("ReportIncoming","Incoming supply"),IncomingSupplyText)]]
         +SWrapBox::Slot().FillEmptySpace(true)[SNew(SBox).WidthOverride(Preferences.bLargeText?138:96)[Metric(LOCTEXT("ReportProduction","Production / tick"),ProductionText)]]
         +SWrapBox::Slot().FillEmptySpace(true)[SNew(SBox).WidthOverride(Preferences.bLargeText?138:96)[Metric(LOCTEXT("ReportConsumption","Consumed / tick"),ConsumptionText)]]]
        +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("ReportPriceHistory","Price history"))]
        +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SAssignNew(ReportHistoryCount,STextBlock).TextStyle(&ReportSmallStyle).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(PriceChart,SHansaPriceHistoryChart).Preferences(Preferences)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[SAssignNew(ChartSummaryText,STextBlock).TextStyle(&ReportSmallStyle).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("ReportWhyPrice","Why this price?"))]
        +SVerticalBox::Slot().AutoHeight()[SAssignNew(ExplanationText,STextBlock).TextStyle(&ReportSmallStyle).AutoWrapText(true).Visibility(EVisibility::Collapsed)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SAssignNew(FactorList,SVerticalBox)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,4)[SNew(SHorizontalBox)
         +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Relationships(false)]
         +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Relationships(true)]];

    auto ReportButtonStyle=[&](bool Primary){
        auto Style=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
        const auto Navy=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy);
        const auto Slate=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::HarborSlate);
        const auto Brass=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
        const auto Fill=UHansaUiStyleLibrary::GetColor(Primary?EHansaUiColorToken::ProsperityTeal:EHansaUiColorToken::BalticNavy);
        Style.SetNormal(FSlateRoundedBoxBrush(Fill,3.f,Brass,1.f));
        Style.SetHovered(FSlateRoundedBoxBrush(Slate,3.f,Brass,2.f));
        Style.SetPressed(FSlateRoundedBoxBrush(Navy,3.f,Brass,2.f));
        Style.SetDisabled(FSlateRoundedBoxBrush(Slate,3.f,Brass*.5f,1.f));
        Style.SetNormalPadding(FMargin(10,8)).SetPressedPadding(FMargin(10,8));
        return Style;
    };
    PrimaryButtonStyle=ReportButtonStyle(true);SecondaryButtonStyle=ReportButtonStyle(false);
    ReportFooter=SNew(SHansaReferenceFrame).Dark(true).Padding(12)
        [SNew(SVerticalBox)
         +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
          +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,6,0)[SAssignNew(PinButton,SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Preferences).Compact(false).OnClicked(this,&SHansaMarketTable::InvokePin)
           [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Pin).OnDark(true).Size(26)]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(ReportPinLabel,STextBlock).TextStyle(&ReportBodyStyle).ColorAndOpacity(Chalk).AutoWrapText(true).Justification(ETextJustify::Center)]]]
          +SHorizontalBox::Slot().FillWidth(1).Padding(6,0,0,0)[SAssignNew(RouteButton,SHansaAction).Kind(EHansaUiButtonStyle::Primary).Preferences(Preferences).Compact(false).OnClicked(this,&SHansaMarketTable::InvokeRoute)
           [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Ship).OnDark(true).Size(32)]
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(ReportRouteLabel,STextBlock).TextStyle(&ReportBodyStyle).ColorAndOpacity(Chalk).AutoWrapText(true).Justification(ETextJustify::Center)]]]]
         +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(PinReasonText,STextBlock).TextStyle(&ReportSmallStyle).ColorAndOpacity(Chalk).AutoWrapText(true)]
         +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(RouteReasonText,STextBlock).TextStyle(&ReportSmallStyle).ColorAndOpacity(Chalk).AutoWrapText(true)]
         +SVerticalBox::Slot().AutoHeight()[SAssignNew(ActionResultText,STextBlock).TextStyle(&ReportSmallStyle).ColorAndOpacity(Chalk).AutoWrapText(true)]];

    PinButton->SetButtonStyle(&SecondaryButtonStyle);RouteButton->SetButtonStyle(&PrimaryButtonStyle);
    BaseValueText->SetTextStyle(&HeadingStyle);
    ReportArtwork=SNew(SBox).WidthOverride_Lambda([this]{return FMath::Clamp(float(GetCachedGeometry().GetLocalSize().X)*.4f*.39f,100.f,240.f);})
        .HeightOverride_Lambda([this]{return FMath::Clamp(float(GetCachedGeometry().GetLocalSize().X)*.4f*.39f,100.f,240.f)*.5f;})
        [SNew(SImage).Image(CommodityReport::Hides())];
    auto Header=SNew(SHansaReferenceFrame).Dark(true).Padding(FMargin(6))
        [SNew(SHorizontalBox)
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ReportArtwork.ToSharedRef()]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8)[SAssignNew(ReportGoodIcon,SHansaGlyph).OnDark(true).Size(48)]
         +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(8,8)[SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight()[SAssignNew(DetailTitle,STextBlock).TextStyle(&ReportTitleStyle).AutoWrapText(true)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SAssignNew(DetailConfidence,STextBlock).TextStyle(&ReportBodyStyle).ColorAndOpacity(Chalk).AutoWrapText(true)]
          +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ReportDescription,STextBlock).Text(LOCTEXT("HidesFlavor","A raw material for leatherworking and trade.")).TextStyle(&ReportSmallStyle).ColorAndOpacity(Chalk).AutoWrapText(true)]]
         +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[SAssignNew(ReportCloseButton,SHansaAction).Kind(EHansaUiButtonStyle::Icon).Preferences(Preferences).Compact(true).Label(FText::FromString(TEXT("×"))).ToolTipText(LOCTEXT("CloseReportTip","Close this report and return to the goods list.")).OnClicked(this,&SHansaMarketTable::CloseCommodityReport)]];
    MapWidget(TEXT("Market.Detail.Action.Close"),ReportCloseButton);
    MapWidget(TEXT("Market.Detail.Metric.Stock"),ReportStockText);
    MapWidget(TEXT("Market.Detail.Metric.Reserve"),ReportReserveText);
    MapWidget(TEXT("Market.Detail.Metric.Surplus"),ReportSurplusText);
    MapWidget(TEXT("Market.Detail.StockBar"),ReserveBar);

    return SAssignNew(DetailContentPanel,SBorder).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Padding(0)
        [SNew(SHansaReferenceFrame).Dark(false).Surface(Preferences.bHighContrast?nullptr:CommodityReport::Linen()).Padding(5)
         [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight()[Header]
          +SVerticalBox::Slot().FillHeight(1)[SAssignNew(DetailScroll,SScrollBox).AnimateWheelScrolling(false)
           +SScrollBox::Slot().Padding(FMargin(18,8))[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[Quay]
            +SVerticalBox::Slot().AutoHeight()[Analysis]]]
          +SVerticalBox::Slot().AutoHeight()[ReportFooter.ToSharedRef()]]];
}

FReply SHansaMarketTable::CloseCommodityReport()
{
    bReportDismissed=true;
    if(Model.IsValid())
    {
        const auto Id=RowId(Model->GetSnapshot().SelectedGoodStableId);
        Refresh(Model->GetSnapshot(),Model->GetRevision());
        if(!FocusSemanticId(Id)) FSlateApplication::Get().SetKeyboardFocus(SearchBox,EFocusCause::SetDirectly);
    }
    return FReply::Handled();
}

void SHansaMarketTable::RefreshCommodityReport(const FHansaMarketTableSnapshot& Snapshot)
{
    const auto& D=Snapshot.SelectedGood;
    if(PresentedDetail.GoodStableId!=D.GoodStableId) {bReportDismissed=false;DetailScroll->ScrollToStart();}
    const bool Visible=D.bHasSelection&&!bReportDismissed;
    DetailContentPanel->SetVisibility(Visible?EVisibility::Visible:EVisibility::Collapsed);
    DetailEmptyPanel->SetVisibility(Visible?EVisibility::Collapsed:EVisibility::Visible);
    const bool Hides=D.GoodStableId==TEXT("Good.RawHides")&&!Preferences.bLargeText;
    ReportArtwork->SetVisibility(Hides?EVisibility::HitTestInvisible:EVisibility::Collapsed);
    ReportGoodIcon->SetVisibility(Hides?EVisibility::Collapsed:EVisibility::HitTestInvisible);
    ReportGoodIcon->SetGlyph(GlyphForGood(D.GoodStableId));
    ReportDescription->SetVisibility(Hides?EVisibility::Visible:EVisibility::Collapsed);
    ReportFooter->SetVisibility(D.bSpotTradeVisible?EVisibility::Collapsed:EVisibility::Visible);
    ReportPinLabel->SetText(D.bPinned?LOCTEXT("ReportUnpin","Unpin price & stock"):D.PinActionLabel);
    ReportRouteLabel->SetText(D.RouteActionLabel);
    const auto* Row=Model.IsValid()?Model->FindRow(D.GoodStableId):nullptr;
    const bool Known=Row&&!Row->bUnknown;
    const auto Unknown=LOCTEXT("ReportUnavailable","Unavailable");
    ReportStockText->SetText(Known?Row->Stock:Unknown);
    ReportReserveText->SetText(Known?Row->Reserve:Unknown);
    ReportSurplusText->SetText(Known?CommodityReport::Quantity(FMath::Abs(Row->StockRaw-Row->ReserveRaw)):Unknown);
    ReportBalanceLabel->SetText(Known&&Row->StockRaw<Row->ReserveRaw?LOCTEXT("ReportBelowReserve","Below reserve"):LOCTEXT("ReportSurplus","Surplus above reserve"));
    ReserveBar->SetData(Known?Row->StockRaw:0,Known?Row->ReserveRaw:0,Known);
    ReserveBar->SetToolTipText(D.StockVersusReserve);
    ReserveDaysText->SetText(FText::Format(LOCTEXT("ReportCoverage","Reserve coverage: {0}"),D.ReserveDays.ToString()==TEXT("—")?Unknown:D.ReserveDays));
    ChartSummaryText->SetToolTipText(D.ChartSummary);
    if(!D.History.IsEmpty())
        ChartSummaryText->SetText(FText::Format(LOCTEXT("ReportChartCaption","Ticks {0}–{1} · {2}"),FText::AsNumber(D.History[0].Tick),FText::AsNumber(D.History.Last().Tick),D.MinimumHistoryPriceMilliMarks==D.MaximumHistoryPriceMilliMarks?LOCTEXT("ReportFlat","No price change"):D.Confidence));
    ExplanationText->SetToolTipText(D.Explanation);
    ReportHistoryCount->SetText(FText::Format(LOCTEXT("ReportHistoryLegend","{0} recorded prices · Local price: solid · Recent average: dashed"),FText::AsNumber(D.History.Num())));
}
