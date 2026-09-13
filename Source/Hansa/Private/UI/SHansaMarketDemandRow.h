#pragma once
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaUiComponents.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace Hansa::UI
{
/** Native, focusable information row. No raster, tick binding, or simulation ownership. */
class SHansaMarketDemandRow final : public SBorder
{
public:
    SLATE_BEGIN_ARGS(SHansaMarketDemandRow) {}
        SLATE_ARGUMENT(FUiPreferences, Preferences)
        SLATE_EVENT(FSimpleDelegate, OnFocused)
    SLATE_END_ARGS()
    void Construct(const FArguments& A)
    {
        Focused = A._OnFocused;
        auto Color = [](EHansaUiColorToken T) { return UHansaUiStyleLibrary::GetColor(T); };
        Frame = FSlateRoundedBoxBrush(FLinearColor::Transparent, 3.f, Color(EHansaUiColorToken::Ink), 2.f);
        Plain = FSlateRoundedBoxBrush(FLinearColor::Transparent, 3.f);
        BarStyle.SetBackgroundImage(FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Parchment), 4.f, Color(EHansaUiColorToken::Ink), 1.f));
        BarStyle.SetFillImage(FSlateRoundedBoxBrush(Color(EHansaUiColorToken::ProsperityTeal), 4.f, Color(EHansaUiColorToken::Ink), 1.f));
        BarStyle.EnableFillAnimation = false;
        auto Text = [&](EHansaUiTypographyToken T) {
            return SNew(STextBlock).Font(GetComponentFont(T, A._Preferences)).ColorAndOpacity(Color(EHansaUiColorToken::Ink)).AutoWrapText(true);
        };
        Label = Text(EHansaUiTypographyToken::Body);
        Percent = Text(EHansaUiTypographyToken::Data); Percent->SetAutoWrapText(false);
        Amount = Text(EHansaUiTypographyToken::Caption);
        SBorder::Construct(SBorder::FArguments().BorderImage(&Plain).Padding(4)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SAssignNew(Glyph, SHansaGlyph).Size(32)]
            + SHorizontalBox::Slot().FillWidth(1)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Label.ToSharedRef()]
                    + SHorizontalBox::Slot().AutoWidth()[Percent.ToSharedRef()]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0,4)
                [SNew(SBox).HeightOverride(8)[SAssignNew(Bar,SProgressBar).Style(&BarStyle).BarFillStyle(EProgressBarFillStyle::Scale).Percent(0.f).FillColorAndOpacity(FLinearColor::White)]]
                + SVerticalBox::Slot().AutoHeight()[Amount.ToSharedRef()]
            ]
        ]);
    }
    void Refresh(const FHansaInspectorFlowPresentation& D)
    {
        Label->SetText(D.Label); Percent->SetText(D.State); Amount->SetText(D.Value);
        Glyph->SetGlyph(D.DemandGoodId==TEXT("Good.Bread") ? EUiGlyph::Bread :
            D.DemandGoodId==TEXT("Good.Fish") ? EUiGlyph::Fish :
            D.DemandGoodId==TEXT("Good.Beer") ? EUiGlyph::Beer :
            D.DemandGoodId==TEXT("Good.Tools") ? EUiGlyph::Production : EUiGlyph::Information);
        Bar->SetPercent(D.bDemandKnown && D.DemandRequired>0 ? FMath::Clamp(float(double(D.DemandSupplied)/D.DemandRequired),0.f,1.f) : 0.f);
        SetToolTipText(FText::Format(NSLOCTEXT("HansaMarketDemandRow","RollingTooltip","{0}\n{1}\nDemand fulfilled: {2}\nCity-wide · {3}. Supply counts goods actually consumed; access and affordability can also limit consumption."),D.Label,D.Value,D.State,D.DemandPeriod));
    }
    bool SupportsKeyboardFocus() const override { return true; }
    FReply OnFocusReceived(const FGeometry&,const FFocusEvent&) override { Focused.ExecuteIfBound(); return FReply::Handled(); }
    int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& O,int32 L,const FWidgetStyle& S,bool E)const override
    {
        int32 Top=SBorder::OnPaint(A,G,R,O,L,S,E);
        if(HasKeyboardFocus()||HasUserFocus(0)) FSlateDrawElement::MakeBox(O,++Top,G.ToPaintGeometry(),&Frame,ESlateDrawEffect::None,Frame.GetTint(S));
        return Top;
    }
private:
    FSimpleDelegate Focused;
    FSlateBrush Frame, Plain;
    FProgressBarStyle BarStyle;
    TSharedPtr<STextBlock> Label, Percent, Amount;
    TSharedPtr<SHansaGlyph> Glyph;
    TSharedPtr<SProgressBar> Bar;
};
}
