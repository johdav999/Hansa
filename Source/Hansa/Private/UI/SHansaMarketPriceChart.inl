class SHansaPriceHistoryChart final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaPriceHistoryChart) {} SLATE_ARGUMENT(FUiPreferences,Preferences) SLATE_END_ARGS()
    void Construct(const FArguments& A) { Preferences=A._Preferences; }
    void SetData(const TArray<FHansaMarketChartPointPresentation>& InPoints,int64 Average,int64 Minimum,int64 Maximum,bool Stale)
    {
        Points=InPoints; AveragePrice=Average; bStale=Stale;
        const double Low=double(FMath::Min(Minimum,Average)), High=double(FMath::Max(Maximum,Average));
        const double Margin=FMath::Max(100.,(High-Low)*.1);
        AxisLow=FMath::Max(0.,Low-Margin); AxisHigh=High+Margin;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    FVector2D ComputeDesiredSize(float) const override {return FVector2D(240,Preferences.bLargeText?164:136);}
    int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& O,int32 L,const FWidgetStyle& Style,bool) const override
    {
        const auto Z=G.GetLocalSize();
        auto Font=GetComponentFont(EHansaUiTypographyToken::SerifBody,Preferences); Font.Size=Preferences.bLargeText?16:12;
        const float Left=Preferences.bLargeText?86:68, Right=FMath::Max(Left+1,float(Z.X)-14), Top=12, Bottom=Z.Y-30;
        const auto Ink=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink)*Style.GetColorAndOpacityTint();
        const auto Brass=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass)*Style.GetColorAndOpacityTint();
        auto Grid=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk); Grid.A=.23f;
        auto Line=[&](TArray<FVector2D> P,int Layer,FLinearColor Color,float Thickness=1.f){FSlateDrawElement::MakeLines(O,Layer,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,Thickness);};
        auto Text=[&](FText T,FVector2D P){FSlateDrawElement::MakeText(O,L+4,G.ToPaintGeometry(FVector2D(100,24),FSlateLayoutTransform(P)),T,Font,ESlateDrawEffect::None,Ink);};
        auto PriceText=[](double Milli){FNumberFormattingOptions F;F.SetMinimumFractionalDigits(3).SetMaximumFractionalDigits(3);return FText::Format(LOCTEXT("ReportChartMarks","{0} mk"),FText::AsNumber(Milli/1000.,&F));};
        auto Y=[&](double Price){return Bottom-(Bottom-Top)*float((Price-AxisLow)/FMath::Max(1.,AxisHigh-AxisLow));};
        if(Points.IsEmpty())
        {
            Text(LOCTEXT("ReportChartNoHistory","No recorded price history"),FVector2D(8,Top+20));return L+4;
        }
        for(int I=0;I<3;++I)
        {
            const double Price=AxisHigh-(AxisHigh-AxisLow)*I/2.; const float YY=Y(Price);
            Line({{Left,YY},{Right,YY}},L,Grid); Text(PriceText(Price),FVector2D(0,YY-8));
        }
        Line({{Left,Top-3},{Left,Bottom},{Right,Bottom}},L+1,Ink);
        const int64 First=Points[0].Tick,Last=Points.Last().Tick;
        const int Steps=Right-Left<260?2:4;
        FNumberFormattingOptions TickFormat;TickFormat.SetUseGrouping(false);
        for(int I=0;I<=Steps;++I)
        {
            if(First==Last&&I>0)break;
            const int64 Tick=First+FMath::RoundToInt64(double(Last-First)*I/Steps);
            const float X=First==Last?(Left+Right)*.5f:Left+(Right-Left)*float(double(Tick-First)/double(Last-First));
            Line({{X,Top},{X,Bottom}},L,Grid);Line({{X,Bottom},{X,Bottom+4}},L+1,Ink);
            Text(FText::AsNumber(Tick,&TickFormat),FVector2D(FMath::Clamp(X-16,Left-8,Right-32),Bottom+7));
        }
        TArray<FVector2D> P;
        for(const auto& Point:Points)
        {
            const float X=First==Last?(Left+Right)*.5f:Left+(Right-Left)*float(double(Point.Tick-First)/double(Last-First));
            P.Add(FVector2D(X,Y(Point.PriceMilliMarks)));
        }
        auto PriceLine=[&](const TArray<FVector2D>& Segment){Line(Segment,L+2,Ink,3);Line(Segment,L+3,Brass,1.5f);};
        if(P.Num()==1)PriceLine({P[0]-FVector2D(3,0),P[0]+FVector2D(3,0)});
        else if(!bStale)PriceLine(P);
        else for(int I=1;I<P.Num();++I)
        {
            const double Length=(P[I]-P[I-1]).Size();
            for(double D=0;D<Length;D+=12)PriceLine({FMath::Lerp(P[I-1],P[I],D/Length),FMath::Lerp(P[I-1],P[I],FMath::Min(D+6,Length)/Length)});
        }
        const float AY=Y(AveragePrice);
        // Draw the average last: equal current/average prices remain two readable
        // series without displacing either price to invent movement.
        for(float X=Left;X<Right;X+=12)Line({{X,AY},{FMath::Min(X+5,Right),AY}},L+4,Ink,1);
        return L+4;
    }
private:
    FUiPreferences Preferences;
    TArray<FHansaMarketChartPointPresentation> Points;
    int64 AveragePrice=0;
    double AxisLow=0,AxisHigh=1;
    bool bStale=false;
};
