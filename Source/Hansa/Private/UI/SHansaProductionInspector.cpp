#include "UI/SHansaProductionInspector.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "HansaProductionInspector"
namespace Hansa::UI
{
namespace
{
FLinearColor Color(EHansaUiColorToken Token) { return UHansaUiStyleLibrary::GetColor(Token); }
FText Quantity(int64 Milli)
{
    FNumberFormattingOptions Format; Format.SetMinimumFractionalDigits(0).SetMaximumFractionalDigits(1);
    return FText::AsNumber(static_cast<double>(Milli) / 1000., &Format);
}
FString PortId(bool Input, FName Good)
{
    return FString(Input ? TEXT("Inspector.Production.Input.") : TEXT("Inspector.Production.Output.")) + Good.ToString();
}
}

/** Only this leaf paints each animation frame; the inspector tree is event driven. */
class SHansaBatchToolTip final : public SToolTip
{
public:
    void OnOpening() override { bOpened=true; }
    void OnClosed() override { bOpened=false; }
    bool IsOpen() const { return bOpened; }
    void SetKeyboardOpened(bool Value) { bKeyboardOpened=Value; }
    bool IsInteractive() const override { return bKeyboardOpened; }
private:
    bool bOpened=false, bKeyboardOpened=false;
};

/** Product information remains reachable through keyboard and controller navigation. */
class SHansaProductPort final : public SBorder
{
public:
    SLATE_BEGIN_ARGS(SHansaProductPort) {} SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_EVENT(FSimpleDelegate, OnFocused)
    SLATE_END_ARGS()
    void Construct(const FArguments& A)
    {
        Focused=A._OnFocused;
        Frame=FSlateRoundedBoxBrush(FLinearColor::Transparent,3.f,Color(EHansaUiColorToken::Ink),2.f);
        SBorder::Construct(SBorder::FArguments().BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder"))).Padding(0)[A._Content.Widget]);
    }
    void SetProductTooltip(TSharedRef<SHansaBatchToolTip> Tip) { ProductTooltip=Tip; SetToolTip(Tip); }
    bool SupportsKeyboardFocus() const override { return true; }
    FReply OnFocusReceived(const FGeometry& G,const FFocusEvent&) override
    {
        Focused.ExecuteIfBound();
        if(ProductTooltip)ProductTooltip->SetKeyboardOpened(true);
        if(GetToolTip().IsValid() && FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().SpawnToolTip(GetToolTip().ToSharedRef(),G.GetAbsolutePosition()+FVector2D(G.GetAbsoluteSize().X,0));
            GetToolTip()->OnOpening(); // Manual spawning bypasses Slate's hover opening notification.
        }
        return FReply::Handled();
    }
    void OnFocusLost(const FFocusEvent& E) override
    {
        if(ProductTooltip)ProductTooltip->SetKeyboardOpened(false);
        if(FSlateApplication::IsInitialized())FSlateApplication::Get().CloseToolTip();
        SBorder::OnFocusLost(E);
    }
    int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& O,int32 L,const FWidgetStyle& S,bool E)const override
    {
        int32 Top=SBorder::OnPaint(A,G,R,O,L,S,E);
        if(HasKeyboardFocus()||HasUserFocus(0))FSlateDrawElement::MakeBox(O,++Top,G.ToPaintGeometry(),&Frame,ESlateDrawEffect::None,Frame.GetTint(S));
        return Top;
    }
private:
    FSimpleDelegate Focused;
    TSharedPtr<SHansaBatchToolTip> ProductTooltip;
    FSlateBrush Frame;
};

class SHansaBatchRing final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaBatchRing) {} SLATE_END_ARGS()
    void Construct(const FArguments&) { SetCanTick(false); }
    void SetProduction(UHansaInspectorPresentationModel* InModel, const FUiPreferences& InPreferences)
    {
        Model = InModel; Preferences = InPreferences;
        const auto& D = Model->GetSnapshot().Production;
        if (!Preferences.bReducedMotion && D.bCanProgress && !D.bSimulationPaused && !Timer.IsValid())
            Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SHansaBatchRing::Animate));
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    FVector2D ComputeDesiredSize(float) const override { return FVector2D(100,100); }
    int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
        int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const float Fraction = Model.IsValid() ? (Preferences.bReducedMotion
            ? float(Model->GetSnapshot().Production.ProgressTicks) / FMath::Max(1, Model->GetSnapshot().Production.CycleTicks)
            : Model->GetBatchVisualFraction()) : 0.f;
        const FVector2f Center = G.GetLocalSize() * .5f;
        const float Radius = FMath::Min(Center.X, Center.Y) - 4.f;
        auto Arc = [&](float End, FLinearColor Ink, int32 L)
        {
            TArray<FVector2f> Points; const int32 Steps = FMath::Max(1, FMath::CeilToInt(128 * End));
            for (int32 I=0; I<=Steps; ++I)
            {
                const float A = -HALF_PI + 2*PI*End*I/Steps;
                Points.Add(Center + FVector2f(FMath::Cos(A),FMath::Sin(A))*Radius);
            }
            FSlateDrawElement::MakeLines(Out,L,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,
                Ink * Style.GetColorAndOpacityTint(),true,6.f);
        };
        Arc(1.f,Color(EHansaUiColorToken::Parchment),Layer);
        if (Fraction>0) Arc(Fraction,Color(Preferences.bHighContrast?EHansaUiColorToken::Ink:EHansaUiColorToken::Brass),Layer+1);
        return Layer+1;
    }
private:
    EActiveTimerReturnType Animate(double,float)
    {
        if (!Model.IsValid() || !Model->GetSnapshot().bOpen || !Model->GetSnapshot().Production.bCanProgress ||
            Model->GetSnapshot().Production.bSimulationPaused || Preferences.bReducedMotion)
        { Timer.Reset(); return EActiveTimerReturnType::Stop; }
        Invalidate(EInvalidateWidgetReason::Paint); return EActiveTimerReturnType::Continue;
    }
    TWeakObjectPtr<UHansaInspectorPresentationModel> Model;
    FUiPreferences Preferences;
    TWeakPtr<FActiveTimerHandle> Timer;
};

TSharedRef<STextBlock> SHansaProductionInspector::Text(FText Value, EHansaUiTypographyToken Token)
{
    return SNew(STextBlock).Text(Value).Font(GetComponentFont(Token,Preferences))
        .ColorAndOpacity(Color(EHansaUiColorToken::Ink)).AutoWrapText(true);
}
TSharedRef<SWidget> SHansaProductionInspector::Rule()
{
    return SNew(SBox).HeightOverride(1)[SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .ColorAndOpacity(Color(Preferences.bHighContrast?EHansaUiColorToken::Ink:EHansaUiColorToken::Brass))];
}
TSharedRef<SHansaAction> SHansaProductionInspector::Action(const TCHAR* Id,FText Label,bool Primary)
{
    if (auto* Existing = Buttons.Find(Id))
    {
        (*Existing)->SetLabel(Label);
        Targets.Add(Id, *Existing);
        return Existing->ToSharedRef();
    }
    auto Button = SNew(SHansaAction).Preferences(Preferences).Compact(!Preferences.bLargeText)
        .Kind(Primary?EHansaUiButtonStyle::Primary:EHansaUiButtonStyle::Secondary)
        .Label(Label).OnClicked(this,&SHansaProductionInspector::Invoke,FName(Id));
    Button->SetFocusHandler(FSimpleDelegate::CreateSP(this,&SHansaProductionInspector::RecordFocus,FName(Id)));
    Buttons.Add(Id,Button); Targets.Add(Id,Button); return Button;
}
TSharedRef<SWidget> SHansaProductionInspector::Ledger(const FString& Id,FText Label,FText Value)
{
    auto Number = Text(Value); Values.Add(Id,Number);
    auto Row = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[Text(Label)]
            + SHorizontalBox::Slot().AutoWidth().Padding(8,0)[Number]]
        + SVerticalBox::Slot().AutoHeight()[SNew(SBox).HeightOverride(1)
            [SNew(SImage).Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).ColorAndOpacity(Color(EHansaUiColorToken::Parchment))]];
    Targets.Add(Id,Row); return Row;
}

void SHansaProductionInspector::Construct(const FArguments& Args)
{
    Model=Args._Model; Preferences=Args._Preferences;
    PaperBrush=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Linen),3.f,Color(EHansaUiColorToken::Brass),1.f);
    NavyBrush=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::BalticNavy),2.f);
    TooltipBrush=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::BalticNavy),3.f,Color(EHansaUiColorToken::Brass),1.f);
    PortraitFrame=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Linen),FVector4(Preferences.bLargeText?40:64,Preferences.bLargeText?40:64,3,3),Color(EHansaUiColorToken::Brass),2.f);
    RecipeBrush=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Parchment),0.f);
    StatusBrush=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::ProsperityTeal).CopyWithNewOpacity(.08f),3.f,Color(EHansaUiColorToken::ProsperityTeal),1.f);
    Identity=Text(FText(),EHansaUiTypographyToken::Heading1);Identity->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));
    State=Text(FText(),EHansaUiTypographyToken::Body);
    Percent=Text(FText(),EHansaUiTypographyToken::Display);Percent->SetAutoWrapText(false);
    Remaining=Text(FText(),EHansaUiTypographyToken::Caption);
    RecipeText=Text(FText(),EHansaUiTypographyToken::Data);
    StatusTitle=Text(FText(),EHansaUiTypographyToken::Heading2);StatusDetail=Text(FText(),EHansaUiTypographyToken::Body);
    ActionResult=Text(FText(),EHansaUiTypographyToken::Caption);
    CostValue=Text(FText());LaborSummary=Text(FText());
    TooltipTime=Text(FText(),EHansaUiTypographyToken::Body);TooltipPercent=Text(FText(),EHansaUiTypographyToken::Body);
    TooltipTime->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));TooltipPercent->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));
    ProcessLabel=Text(LOCTEXT("Batch","Batch"),EHansaUiTypographyToken::Caption);
    ProcessHeading=Text(LOCTEXT("Production","Production"),EHansaUiTypographyToken::Caption);
    ProcessHeading->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));
    TooltipHeading=Text(LOCTEXT("BatchProcess","Batch process"),EHansaUiTypographyToken::Heading2);
    TooltipDurationLabel=Text(LOCTEXT("BatchTime","Batch time (1×)"),EHansaUiTypographyToken::Body);
    TooltipHeading->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));
    TooltipDurationLabel->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));
    RecordHeading=Text(LOCTEXT("Record","Production record"),EHansaUiTypographyToken::Heading2);
    auto TipLabel=[&](FText T,EHansaUiTypographyToken Token){auto W=Text(T,Token);W->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));return W;};
    BatchTooltip=SNew(SHansaBatchToolTip).BorderImage(&TooltipBrush).TextMargin(16)
        [SNew(SBox).MinDesiredWidth(248)[SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[TooltipHeading.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)[TooltipDurationLabel.ToSharedRef()]
                + SHorizontalBox::Slot().AutoWidth().Padding(16,0)[TooltipTime.ToSharedRef()]]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)[TipLabel(LOCTEXT("CompletedPercent","% completed"),EHansaUiTypographyToken::Body)]
                + SHorizontalBox::Slot().AutoWidth().Padding(16,0)[TooltipPercent.ToSharedRef()]]]];
    Targets.Add(TEXT("Inspector.Production.Batch.Tooltip"),BatchTooltip);
    Targets.Add(TEXT("Inspector.Production.Batch.Time"),TooltipTime);Targets.Add(TEXT("Inspector.Production.Batch.Percent"),TooltipPercent);
    auto Icon=[&](const TCHAR* Name,float Size)->TSharedRef<SWidget>
    {
        if(FCString::Strcmp(Name,TEXT("worker"))!=0)
            return SNew(SHansaGlyph).Glyph(FCString::Strcmp(Name,TEXT("cost"))==0?EUiGlyph::Coin:EUiGlyph::People).Size(Size);
        auto Brush=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/Production/")+Name+TEXT("--160.png"))),FVector2D(Size,Size));
        StaticArtBrushes.Add(Brush);return SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(&Brush.Get())];
    };
    auto Portrait=SNew(SBox).WidthOverride(Preferences.bLargeText?80:136).HeightOverride(Preferences.bLargeText?64:120)
        [SNew(SBorder).BorderImage(&PortraitFrame).Padding(8,4)[Icon(TEXT("worker"),Preferences.bLargeText?56:112)]];
    Targets.Add(TEXT("Inspector.Production.Portrait"),Portrait);
    WorkforceStyle.SetBackgroundImage(FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Parchment),0.f));
    WorkforceStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White,0.f));WorkforceStyle.EnableFillAnimation=false;
    auto WorkforceRow=[&](const TCHAR* Id,FText Label,TSharedPtr<STextBlock>& Count,TSharedPtr<SProgressBar>& Bar)
    {
        Count=Text(FText());
        auto Row=SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Label)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4,0)[Count.ToSharedRef()]
            + SHorizontalBox::Slot().FillWidth(.6f).VAlign(VAlign_Center)[SNew(SBox).HeightOverride(8)
                [SAssignNew(Bar,SProgressBar).Style(&WorkforceStyle).Percent(0.f).FillColorAndOpacity(Color(EHansaUiColorToken::ProsperityTeal))]];
        Targets.Add(Id,Row);return Row;
    };
    LaborerRow=WorkforceRow(TEXT("Inspector.Production.Laborers"),LOCTEXT("Laborers","Laborers"),LaborerCount,LaborerBar);
    ArtisanRow=WorkforceRow(TEXT("Inspector.Production.Artisans"),LOCTEXT("Artisans","Artisans"),ArtisanCount,ArtisanBar);
    auto Flow=SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(InputPorts,SVerticalBox)]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(InputArrow,SHansaGlyph).Glyph(EUiGlyph::Arrow).Size(12)]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(BatchSurface,SOverlay)
                + SOverlay::Slot()[SAssignNew(Ring,SHansaBatchRing)]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Percent.ToSharedRef()]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ProcessLabel.ToSharedRef()]]]
            ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(OutputArrow,SHansaGlyph).Glyph(EUiGlyph::Arrow).Size(12)]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(OutputPorts,SVerticalBox)];
    BatchSurface->SetToolTip(BatchTooltip);
    Targets.Add(TEXT("Inspector.Production.Batch.Hover"),BatchSurface);
    Targets.Add(TEXT("Inspector.Flows"),Flow);
    auto Details=SAssignNew(DetailsContent,SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SAssignNew(ConstructionDetails,SVerticalBox)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8)[RecipeText.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight()[Remaining.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight()[SAssignNew(Stocks,SVerticalBox)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Workforce,SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[Rule()]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4)[LaborerRow.ToSharedRef()]
            + SVerticalBox::Slot().AutoHeight().Padding(0,4)[ArtisanRow.ToSharedRef()]]
        + SVerticalBox::Slot().AutoHeight()[RecordHeading.ToSharedRef()]
        + SVerticalBox::Slot().AutoHeight()[SAssignNew(Records,SVerticalBox)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(StatusCard,SBorder).BorderImage(&StatusBrush).Padding(8)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)[SAssignNew(StatusGlyph,SHansaGlyph).Glyph(EUiGlyph::Check).Size(24)]
                    + SHorizontalBox::Slot().FillWidth(1)[StatusTitle.ToSharedRef()]]
                + SVerticalBox::Slot().AutoHeight()[StatusDetail.ToSharedRef()]]]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[Action(TEXT("Inspector.Action.ViewStorage"),LOCTEXT("Storage","View storage"))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[Action(TEXT("Inspector.Action.OpenRelated"),LOCTEXT("Chain","Production chain"))]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Action(TEXT("Inspector.Action.Pin"),LOCTEXT("Pin","Pin"))]
            + SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Action(TEXT("Inspector.Action.Frame"),LOCTEXT("Frame","Frame"))]]
        + SVerticalBox::Slot().AutoHeight()[SAssignNew(ExtraActions,SVerticalBox)]
        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[ActionResult.ToSharedRef()];
    auto FooterCell=[&](const TCHAR* Id,const TCHAR* Glyph,FText Caption,TSharedPtr<STextBlock> Value)
    {
        auto Cell=SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[Icon(Glyph,28)]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Value.ToSharedRef()]
                + SVerticalBox::Slot().AutoHeight()[Text(Caption,EHansaUiTypographyToken::Caption)]];
        Targets.Add(Id,Cell);return Cell;
    };
    auto Cost=FooterCell(TEXT("Inspector.Production.Cost"),TEXT("cost"),LOCTEXT("BatchCost","Cost / batch"),CostValue);
    Cost->SetToolTipText(LOCTEXT("NoOperatingCost","No operating cost is defined for this production. The cost value is blank."));
    auto Labor=FooterCell(TEXT("Inspector.Production.Labor"),TEXT("labor"),LOCTEXT("WorkersNeeded","Workers needed"),LaborSummary);
    LaborFooter=Labor;
    ChildSlot[SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Portrait]
        + SVerticalBox::Slot().FillHeight(1)[SNew(SBorder).BorderImage(&PaperBrush).Padding(1)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&NavyBrush).Padding(12,4)
                    [SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()[Identity.ToSharedRef()]
                            + SVerticalBox::Slot().AutoHeight()[ProcessHeading.ToSharedRef()]]
                        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(48)[Action(TEXT("Inspector.Close"),LOCTEXT("Close","×"),true)]]]]
                + SVerticalBox::Slot().FillHeight(1)[SAssignNew(Scroll,SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll).NavigationDestination(EDescendantScrollDestination::Center).NavigationScrollPadding(8)
                    + SScrollBox::Slot().Padding(16,0)[SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)[SAssignNew(StateGlyph,SHansaGlyph).Glyph(EUiGlyph::Check).Size(20)]
                            + SHorizontalBox::Slot().FillWidth(1)[State.ToSharedRef()]]
                        + SVerticalBox::Slot().AutoHeight()[SAssignNew(PreservationControls,SVerticalBox)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(FlowHost,SBox)[Flow]]
                        + SVerticalBox::Slot().AutoHeight()[Details]]]
                + SVerticalBox::Slot().AutoHeight().Padding(16,4)[SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Action(TEXT("Inspector.Action.ToggleProduction"),LOCTEXT("CompactPause","Pause"))]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Action(TEXT("Inspector.Action.OpenCause"),LOCTEXT("Details","Details"))]]
                + SVerticalBox::Slot().AutoHeight().Padding(16,4)[Rule()]
                + SVerticalBox::Slot().AutoHeight().Padding(16,4,16,12)[SAssignNew(Footer,SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Cost]
                    + SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Labor]]]]];
    Targets.Add(TEXT("Inspector.Identity"),Identity);Targets.Add(TEXT("Inspector.Result"),Ring);
    Targets.Add(TEXT("Inspector.Production.Batch"),Ring);Targets.Add(TEXT("Inspector.History"),Records);
    Targets.Add(TEXT("Inspector.Problem"),State);Targets.Add(TEXT("Inspector.Problem.Cause"),StatusDetail);
}

void SHansaProductionInspector::BuildPorts(const FHansaInspectorProductionData& D)
{
    for(auto It=Targets.CreateIterator();It;++It) if(It.Key().StartsWith(TEXT("Inspector.Production.Input.")) ||
        It.Key().StartsWith(TEXT("Inspector.Production.Output.")) || It.Key().StartsWith(TEXT("Inspector.Production.Record."))) It.RemoveCurrent();
    InputPorts->ClearChildren(); OutputPorts->ClearChildren(); Stocks->ClearChildren(); Records->ClearChildren();
    Values.Reset(); ArtBrushes.Reset(); ProductTooltips.Reset(); ProductTooltipValues.Reset();
    auto Add=[&](const FHansaInspectorProductionPort& P,bool Input)
    {
        TSharedRef<SWidget> Art=SNew(SHansaGlyph).Glyph(GlyphForGood(P.GoodId)).Size(64);
        const FString Id=PortId(Input,P.GoodId);
        auto Port=SNew(SHansaProductPort).OnFocused(FSimpleDelegate::CreateSP(this,&SHansaProductionInspector::RecordFocus,FName(*Id)))
            [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Art]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,4)[Text(FText::Format(LOCTEXT("PortQuantity","{0} {1}"),Quantity(P.PerBatch),P.Label),EHansaUiTypographyToken::Data)]];
        (Input?InputPorts:OutputPorts)->AddSlot().AutoHeight()[Port];
        Targets.Add(Id,Port);
        auto Summary=Text(FText(),EHansaUiTypographyToken::Body);
        Summary->SetColorAndOpacity(Color(EHansaUiColorToken::Chalk));
        auto Tip=SNew(SHansaBatchToolTip).BorderImage(&TooltipBrush).TextMargin(16)
            [SNew(SBox).MinDesiredWidth(160).MaxDesiredWidth(320)[Summary]];
        Port->SetProductTooltip(Tip);
        ProductTooltips.Add(Id,Tip); ProductTooltipValues.Add(Id,Summary);
        Targets.Add(Id+TEXT(".Tooltip"),Tip);

        Stocks->AddSlot().AutoHeight()[Ledger(Id+TEXT(".Stock"),FText::Format(Input?LOCTEXT("Available","{0} available"):LOCTEXT("OutputStock","{0} in output storage"),P.Label),FText())];
        if(Input)Stocks->AddSlot().AutoHeight()[Ledger(Id+TEXT(".Reserved"),D.Inputs.Num()==1?LOCTEXT("Reserved","Reserved for this batch"):FText::Format(LOCTEXT("ReservedGood","{0} reserved for this batch"),P.Label),FText())];
    };
    for(const auto& P:D.Inputs)Add(P,true);
    if(D.Inputs.IsEmpty()) InputPorts->AddSlot().AutoHeight().HAlign(HAlign_Center)[Text(LOCTEXT("NoInput","No input\nrequired"),EHansaUiTypographyToken::Caption)];
    for(const auto& P:D.Outputs)Add(P,false);
    Records->AddSlot().AutoHeight()[Ledger(TEXT("Inspector.Production.Record.Batches"),LOCTEXT("Batches","Completed batches"),FText())];
    for(const auto& P:D.Outputs) Records->AddSlot().AutoHeight()[Ledger(TEXT("Inspector.Production.Record.")+P.GoodId.ToString(),
        D.Outputs.Num()==1?LOCTEXT("Produced","Produced so far"):FText::Format(LOCTEXT("ProducedGood","{0} produced"),P.Label),FText())];
    Stocks->AddSlot().AutoHeight()[Ledger(TEXT("Inspector.Logistics.MarketAccess"),LOCTEXT("MarketAccess","Market access"),FText())];
}

void SHansaProductionInspector::Refresh(const FHansaInspectorSnapshot& S)
{
    const auto& D=S.Production;
    auto SameIds=[](const auto& A,const auto& B){if(A.Num()!=B.Num())return false;for(int32 I=0;I<A.Num();++I)if(A[I].GoodId!=B[I].GoodId || !A[I].Label.EqualTo(B[I].Label) || A[I].PerBatch!=B[I].PerBatch)return false;return true;};
    if(!Presented.Production.bValid || D.bConstruction!=Presented.Production.bConstruction || !SameIds(D.Inputs,Presented.Production.Inputs) || !SameIds(D.Outputs,Presented.Production.Outputs)) BuildPorts(D);
    Identity->SetText(S.Identity);
    DetailsContent->SetVisibility(S.bCauseExpanded?EVisibility::Visible:EVisibility::Collapsed);
    LaborSummary->SetText(FText::AsNumber(D.RequiredLaborers+D.RequiredArtisans));
    LaborFooter->SetToolTipText(FText::Format(LOCTEXT("LaborBreakdown","Laborers: {0} / {1}\nArtisans: {2} / {3}"),FText::AsNumber(D.Laborers),FText::AsNumber(D.RequiredLaborers),FText::AsNumber(D.Artisans),FText::AsNumber(D.RequiredArtisans)));
    const bool Warning=(!D.bCanProgress && D.bActive) || (D.bRoadRequired && !D.bHasMarketAccess);
    State->SetText(D.bRoadRequired && !D.bHasMarketAccess ? LOCTEXT("MarketNotInRange","Market not in range") : !D.bActive?LOCTEXT("Paused","Paused"):Warning?LOCTEXT("Blocked","Production blocked"):D.bSimulationPaused?LOCTEXT("SimulationPaused","Simulation paused"):D.Outputs.ContainsByPredicate([](const auto& P){return P.GoodId==TEXT("Good.Bread");})?LOCTEXT("Baking","Baking"):LOCTEXT("Working","Producing"));
    StateGlyph->SetGlyph(Warning?EUiGlyph::Warning:D.bActive?EUiGlyph::Check:EUiGlyph::Information);
    State->SetColorAndOpacity(Color(Warning?EHansaUiColorToken::Ink:EHansaUiColorToken::ProsperityTeal));
    Percent->SetText(FText::Format(LOCTEXT("Percent","{0}%"),FText::AsNumber(FMath::FloorToInt(100.f*D.ProgressTicks/FMath::Max(1,D.CycleTicks)))));
    const int32 SecondsRemaining=FMath::Max(0,D.CycleTicks-D.ProgressTicks);
    Remaining->SetText(FText::Format(LOCTEXT("RemainingTime","{0} remaining"),FText::FromString(FString::Printf(TEXT("%02d:%02d"),SecondsRemaining/60,SecondsRemaining%60))));
    Ring->SetProduction(Model.Get(),Preferences);
    auto RecipePart=[](const auto& Ports){TArray<FText> Parts;for(const auto& P:Ports)Parts.Add(FText::Format(LOCTEXT("AmountGood","{0} {1}"),Quantity(P.PerBatch),P.Label));return FText::Join(LOCTEXT("Plus"," + "),Parts);};
    RecipeText->SetText(D.Inputs.IsEmpty()?FText::Format(LOCTEXT("SourceRecipe","{0} per batch"),RecipePart(D.Outputs)):
        FText::Format(LOCTEXT("Recipe","{0} → {1} per batch"),RecipePart(D.Inputs),RecipePart(D.Outputs)));
    for(bool Input:{true,false})for(const auto& P:Input?D.Inputs:D.Outputs)
    {
        const FString Id=PortId(Input,P.GoodId);
        ProductTooltipValues[Id]->SetText(FText::Format(LOCTEXT("ProductStockSummary","{0} in storage\n{1} in markets"),
            P.bBuildingStockKnown?Quantity(P.BuildingStock):LOCTEXT("Unknown","Unavailable"),
            P.bMarketStockKnown?Quantity(P.MarketStock):LOCTEXT("Unknown","Unavailable")));
        Values[Id+TEXT(".Stock")]->SetText(P.bStockKnown?Quantity(Input?P.Available:P.Stock):LOCTEXT("Unknown","Unavailable"));
        if(Input)Values[Id+TEXT(".Reserved")]->SetText(Quantity(P.Reserved));
    }
    Values[TEXT("Inspector.Production.Record.Batches")]->SetText(FText::AsNumber(D.CompletedBatches));
    for(const auto& P:D.Outputs)
    {
        const FText Total=Quantity(P.ProducedTotal);
        Values[TEXT("Inspector.Production.Record.")+P.GoodId.ToString()]->SetText(FText::Format(LOCTEXT("TotalGood","{0} {1}"),Total,P.Label));
    }
    LaborerCount->SetText(FText::Format(LOCTEXT("StaffCount","{0} / {1}"),FText::AsNumber(D.Laborers),FText::AsNumber(D.RequiredLaborers)));
    ArtisanCount->SetText(FText::Format(LOCTEXT("StaffCount","{0} / {1}"),FText::AsNumber(D.Artisans),FText::AsNumber(D.RequiredArtisans)));
    LaborerBar->SetPercent(float(D.Laborers)/FMath::Max(1,D.RequiredLaborers)); ArtisanBar->SetPercent(float(D.Artisans)/FMath::Max(1,D.RequiredArtisans));
    LaborerRow->SetVisibility(D.RequiredLaborers?EVisibility::Visible:EVisibility::Collapsed);
    ArtisanRow->SetVisibility(D.RequiredArtisans?EVisibility::Visible:EVisibility::Collapsed);
    Workforce->SetVisibility(D.RequiredLaborers||D.RequiredArtisans?EVisibility::Visible:EVisibility::Collapsed);
    StatusGlyph->SetGlyph(Warning?EUiGlyph::Warning:EUiGlyph::Check);
    StatusTitle->SetText(Warning?S.Causal.Problem:!D.bActive?LOCTEXT("PausedTitle","Production paused"):D.Inputs.IsEmpty()?LOCTEXT("SourceReady","No ingredients required"):D.ProgressTicks>0?LOCTEXT("Secured","Inputs secured"):LOCTEXT("Ready","Ready for next batch"));
    StatusDetail->SetText(Warning?FText::Format(LOCTEXT("ProblemDetail","{0}\n{1}"),S.Causal.Evidence,S.Causal.Remedy):!D.bActive?LOCTEXT("ResumeHint","Resume production to continue this batch."):D.Inputs.IsEmpty()?LOCTEXT("SourceDetail","This recipe produces directly from its workforce and site."):D.ProgressTicks>0?LOCTEXT("ReservedDetail","Ingredients are reserved for the current batch."):LOCTEXT("NextDetail","Ingredients are checked when the next batch starts."));
    StatusCard->SetToolTipText(FText::Format(LOCTEXT("StatusTip","{0}\n{1}\n{2}"),S.Causal.Cause,S.Causal.Evidence,S.Causal.Remedy));
    State->SetToolTipText(FText::Format(LOCTEXT("CompactCause","{0}\n{1}\n{2}"),S.Causal.Cause,S.Causal.Evidence,S.Causal.Remedy));
    // Runtime normal speed is one simulation tick per real second.
    TooltipTime->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"),D.CycleTicks/60,D.CycleTicks%60)));
    TooltipPercent->SetText(Percent->GetText());
    ActionResult->SetText(S.LastActionResult);
    ActionResult->SetVisibility(S.LastActionResult.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
    for(auto& Pair:Buttons)
    {
        const auto* A=S.Actions.FindByPredicate([&](const auto& V){return V.StableId==FName(*Pair.Key);});
        const bool Special=Pair.Key==TEXT("Inspector.Close")||Pair.Key==TEXT("Inspector.Action.OpenCause");
        Pair.Value->SetState(!Special&&(!A||!A->bEnabled)?EUiState::Disabled:A&&A->bSelected?EUiState::Selected:EUiState::Default,
            A?(A->bEnabled?A->ToolTip:A->DisabledReason):Pair.Key==TEXT("Inspector.Close")?LOCTEXT("CloseLabel","Close production inspector"):LOCTEXT("DetailsLabel","Production details"));
    }
    FlowHost->SetHAlign(D.bConstruction?HAlign_Center:HAlign_Fill);
    const EVisibility ProductionOnly=D.bConstruction?EVisibility::Collapsed:EVisibility::Visible;
    for(const auto& W:{InputArrow,OutputArrow,Footer})W->SetVisibility(ProductionOnly);
    InputPorts->SetVisibility(ProductionOnly);OutputPorts->SetVisibility(ProductionOnly);
    Stocks->SetVisibility(ProductionOnly);Records->SetVisibility(ProductionOnly);RecordHeading->SetVisibility(ProductionOnly);
    RecipeText->SetVisibility(ProductionOnly);
    Buttons[TEXT("Inspector.Action.ToggleProduction")]->SetVisibility(ProductionOnly);
    Buttons[TEXT("Inspector.Action.ViewStorage")]->SetVisibility(ProductionOnly);
    Buttons[TEXT("Inspector.Action.OpenRelated")]->SetLabel(D.bConstruction?LOCTEXT("RelatedBuilding","Related view"):LOCTEXT("Chain","Production chain"));
    ProcessLabel->SetText(D.bConstruction?LOCTEXT("Build","Build"):LOCTEXT("Batch","Batch"));
    ProcessHeading->SetText(D.bConstruction?LOCTEXT("Construction","Construction"):LOCTEXT("Production","Production"));
    TooltipHeading->SetText(D.bConstruction?LOCTEXT("ConstructionProgress","Construction progress"):LOCTEXT("BatchProcess","Batch process"));
    TooltipDurationLabel->SetText(D.bConstruction?LOCTEXT("ConstructionTime","Build time (1×)"):LOCTEXT("BatchTime","Batch time (1×)"));
    ConstructionDetails->ClearChildren();
    if(D.bConstruction)
    {
        State->SetText(D.bSimulationPaused?LOCTEXT("ConstructionPaused","Construction paused"):LOCTEXT("UnderConstruction","Under construction"));
        StateGlyph->SetGlyph(EUiGlyph::Information);
        StatusTitle->SetText(S.Causal.Problem);
        StatusDetail->SetText(S.Causal.Remedy);
        ConstructionDetails->AddSlot().AutoHeight().Padding(0,8)[Text(S.PrimaryResult)];
        for(const auto& Row:S.Flows)if(Row.StableId!=TEXT("Construction.Progress"))
            ConstructionDetails->AddSlot().AutoHeight().Padding(0,4)[Text(FText::Format(LOCTEXT("RefundLine","{0}: {1} · {2}"),Row.Label,Row.Value,Row.State))];
    }
    Values[TEXT("Inspector.Logistics.MarketAccess")]->SetText(!D.bRoadRequired
        ? LOCTEXT("MarketAccessNotRequired","Not required")
        : D.bHasMarketAccess
            ? FText::Format(LOCTEXT("MarketAccessConnected","Market #{0} · {1} cells"),
                FText::AsNumber(D.SelectedMarketBuildingValue),FText::AsNumber(D.MarketRoadDistanceCells))
            : FText::Format(LOCTEXT("MarketAccessDisconnected","Disconnected · {0}"),FText::FromName(D.MarketAccessCode)));
    Buttons[TEXT("Inspector.Action.ToggleProduction")]->SetLabel(D.bActive?LOCTEXT("CompactPause","Pause"):LOCTEXT("CompactResume","Resume"));
    Buttons[TEXT("Inspector.Action.Pin")]->SetLabel(S.bPinned?LOCTEXT("Unpin","Unpin"):LOCTEXT("Pin","Pin"));
    Buttons[TEXT("Inspector.Action.OpenCause")]->SetLabel(S.bCauseExpanded?LOCTEXT("Less","Less"):LOCTEXT("Details","Details"));
    // Destructive controls and full history remain reachable below the reference's main actions.
    if(S.Actions!=Presented.Actions)
    {
        ExtraActions->ClearChildren();
        PreservationControls->ClearChildren();
        for(const auto Id:{TEXT("Inspector.Action.RemoveBuilding"),TEXT("Inspector.Action.CancelConstruction"),TEXT("Inspector.Preservation.Upgrade"),TEXT("Inspector.Preservation.Fresh"),TEXT("Inspector.Preservation.Salted"),TEXT("Inspector.Preservation.Fallback")}) { Buttons.Remove(Id);Targets.Remove(Id); }
		TArray<FString> OldRecipeButtons;
		for (const auto& Pair : Buttons) if (Pair.Key.StartsWith(TEXT("Inspector.Recipe."))) OldRecipeButtons.Add(Pair.Key);
		for (const FString& Id : OldRecipeButtons)
        {
            if (!S.Actions.ContainsByPredicate([&](const auto& A) { return A.StableId == FName(*Id); }))
            { Buttons.Remove(Id); Targets.Remove(Id); }
        }
        bool HasRecipeControls = false;
        TSharedPtr<SHorizontalBox> Modes;
        for(const auto& A:S.Actions)
		{
			const FString ActionId = A.StableId.ToString();
			if(ActionId.StartsWith(TEXT("Inspector.Preservation.")) || ActionId.StartsWith(TEXT("Inspector.Recipe.")))
        {

            HasRecipeControls = true;
            auto Button=Action(*A.StableId.ToString(),A.Label);
            Button->SetState(!A.bEnabled?EUiState::Disabled:A.bSelected?EUiState::Selected:EUiState::Default,A.bEnabled?A.ToolTip:A.DisabledReason);
            if(A.StableId==TEXT("Inspector.Preservation.Fresh") || A.StableId==TEXT("Inspector.Preservation.Salted") || ActionId.StartsWith(TEXT("Inspector.Recipe.")))
            {
                if(!Modes) PreservationControls->AddSlot().AutoHeight().Padding(0,2)[SAssignNew(Modes,SHorizontalBox)];
                Modes->AddSlot().FillWidth(1).Padding(2,0)[Button];
            }
            else PreservationControls->AddSlot().AutoHeight().Padding(0,2)[Button];
            if(A.StableId==TEXT("Inspector.Preservation.Upgrade"))
                PreservationControls->AddSlot().AutoHeight().Padding(0,2)[Text(A.bEnabled?A.ToolTip:A.DisabledReason)];
        }
		}
        if(!S.PreservationSummary.IsEmpty()) PreservationControls->AddSlot().AutoHeight().Padding(0,2)[Text(S.PreservationSummary,EHansaUiTypographyToken::Body)];
        if(auto Portrait=Resolve(TEXT("Inspector.Production.Portrait"))) Portrait->SetVisibility(HasRecipeControls?EVisibility::Collapsed:EVisibility::Visible);
        ProcessHeading->SetVisibility(HasRecipeControls?EVisibility::Collapsed:EVisibility::Visible);
        PreservationControls->SetVisibility(HasRecipeControls?EVisibility::Visible:EVisibility::Collapsed);
        for(const auto& A:S.Actions)if(A.StableId==TEXT("Inspector.Action.RemoveBuilding")||A.StableId==TEXT("Inspector.Action.CancelConstruction"))
        {
            auto Button=Action(*A.StableId.ToString(),A.Label);Button->SetState(A.bEnabled?EUiState::Default:EUiState::Disabled,A.bEnabled?A.ToolTip:A.DisabledReason);
            ExtraActions->AddSlot().AutoHeight().Padding(0,8)[Button];
        }
    }
    ExtraActions->SetVisibility(S.bCauseExpanded?EVisibility::Visible:EVisibility::Collapsed);
    FocusOrder={TEXT("Inspector.Close")};
    for(bool Input:{true,false})for(const auto& P:Input?D.Inputs:D.Outputs)FocusOrder.Add(PortId(Input,P.GoodId));
    FocusOrder.Append({TEXT("Inspector.Action.ToggleProduction"),TEXT("Inspector.Action.OpenCause")});
    if(S.bCauseExpanded)FocusOrder.Append({TEXT("Inspector.Action.ViewStorage"),TEXT("Inspector.Action.OpenRelated"),TEXT("Inspector.Action.Pin"),TEXT("Inspector.Action.Frame")});
    for(const auto& A:S.Actions)if(A.StableId.ToString().StartsWith(TEXT("Inspector.Preservation.")) || A.StableId.ToString().StartsWith(TEXT("Inspector.Recipe.")) || (S.bCauseExpanded && (A.StableId==TEXT("Inspector.Action.RemoveBuilding")||A.StableId==TEXT("Inspector.Action.CancelConstruction"))))FocusOrder.Add(A.StableId.ToString());
    FocusOrder.RemoveAll([&](const auto& Id){const auto W=Resolve(Id);return !W.IsValid() || !W->IsEnabled() || W->GetVisibility()==EVisibility::Collapsed;});
    Presented=S;
}

FReply SHansaProductionInspector::Invoke(FName Id){return Model.IsValid()&&Model->ActivateAction(Id)?FReply::Handled():FReply::Unhandled();}
void SHansaProductionInspector::RecordFocus(FName Id){if(Model.IsValid())Model->SetFocusedSemanticId(Id);}
TSharedPtr<SWidget> SHansaProductionInspector::Resolve(const FString& Id)const
{
    if(Id==TEXT("Inspector.Root"))return ConstCastSharedRef<SHansaProductionInspector>(SharedThis(this));
    const auto* W=Targets.Find(Id);return W?*W:nullptr;
}
void SHansaProductionInspector::Reveal(const FString& Id){if(auto W=Resolve(Id))Scroll->ScrollDescendantIntoView(W,false,EDescendantScrollDestination::Center,8);}
bool SHansaProductionInspector::Focus(const FString& Id)
{
    auto W=Resolve(Id);if(!Presented.bOpen||!FocusOrder.Contains(Id)||!W.IsValid()||!W->IsEnabled())return false;
    if(Id!=TEXT("Inspector.Close") && Id!=TEXT("Inspector.Action.OpenCause") && Id!=TEXT("Inspector.Action.ToggleProduction"))
    {
        Reveal(Id);
        // Newly rebuilt accessibility layouts need a paint before their final
        // wrapped heights are available to the scroll request.
        RegisterActiveTimer(.05f,FWidgetActiveTimerDelegate::CreateLambda([Weak=TWeakPtr<SHansaProductionInspector>(SharedThis(this)),Id](double,float)
        {
            if(auto Self=Weak.Pin()) if(Self->Model.IsValid() && Self->Model->GetSnapshot().FocusedSemanticId==FName(*Id))Self->Reveal(Id);
            return EActiveTimerReturnType::Stop;
        }));
    }
    if(Model.IsValid())Model->SetFocusedSemanticId(FName(*Id));
    if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);return true;
}
FReply SHansaProductionInspector::OnKeyDown(const FGeometry&,const FKeyEvent& Event)
{
    const FKey K=Event.GetKey();
    if(K==EKeys::Tab||K==EKeys::Gamepad_DPad_Down||K==EKeys::Gamepad_DPad_Up||K==EKeys::Gamepad_DPad_Left||K==EKeys::Gamepad_DPad_Right)
    {
        int32 Index=FocusOrder.IndexOfByKey(Model.IsValid()?Model->GetSnapshot().FocusedSemanticId.ToString():FString());
        const bool Back=Event.IsShiftDown()||K==EKeys::Gamepad_DPad_Up||K==EKeys::Gamepad_DPad_Left;
        if(!FocusOrder.IsEmpty())Focus(FocusOrder[(Index+(Back?-1:1)+FocusOrder.Num())%FocusOrder.Num()]);return FReply::Handled();
    }
    if(K==EKeys::Escape||K==EKeys::Gamepad_FaceButton_Right)return Invoke(TEXT("Inspector.Close"));
    if(K==EKeys::X)return Invoke(TEXT("Inspector.Action.CancelConstruction"));
    if(K==EKeys::O && !Presented.Production.bConstruction)return Invoke(TEXT("Inspector.Action.ToggleProduction"));
    if(K==EKeys::P)return Invoke(TEXT("Inspector.Action.Pin"));
    if(K==EKeys::F)return Invoke(TEXT("Inspector.Action.Frame"));
    if(K==EKeys::C)return Invoke(TEXT("Inspector.Action.OpenRelated"));
    return FReply::Unhandled();
}
TArray<FHansaHudSemanticNode> SHansaProductionInspector::GetSemanticSnapshot()const
{
    TArray<FHansaHudSemanticNode> Nodes;
    auto Add=[&](FString Id,FString Label,FString Value,EHansaHudSemanticRole Role=EHansaHudSemanticRole::Status)
    {
        FHansaHudSemanticNode N;N.Id=Id;N.ParentId=Id==TEXT("Inspector.Root")?TEXT("HUD.InspectorHost"):TEXT("Inspector.Root");N.Label=Label;N.Role=Role;
        N.State.bVisible=Presented.bOpen;N.State.bEnabled=true;N.State.Value=Value;N.State.ValueType=TEXT("production");
        N.bCanActivate=Buttons.Contains(Id);N.bCanFocus=FocusOrder.Contains(Id);N.State.bFocused=Presented.FocusedSemanticId==FName(*Id);
        if(auto W=Resolve(Id))
        {
            const auto P=W->GetCachedGeometry().GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition();const auto Z=W->GetCachedGeometry().GetAbsoluteSize();
            N.Bounds=FIntRect(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.X+Z.X),FMath::RoundToInt(P.Y+Z.Y));
            N.State.bEnabled=W->IsEnabled();N.State.bVisible&=W->GetVisibility()!=EVisibility::Collapsed;
            if(Id==TEXT("Inspector.Action.RemoveBuilding") || Id==TEXT("Inspector.Action.CancelConstruction"))N.State.bVisible &= Presented.bCauseExpanded;
            const bool Detail=Values.Contains(Id)||Id==TEXT("Inspector.History")||Id==TEXT("Inspector.Problem.Cause")||Id==TEXT("Inspector.Production.Laborers")||Id==TEXT("Inspector.Production.Artisans")||Id==TEXT("Inspector.Action.ViewStorage")||Id==TEXT("Inspector.Action.OpenRelated")||Id==TEXT("Inspector.Action.Pin")||Id==TEXT("Inspector.Action.Frame");
            if(Detail)N.State.bVisible &= Presented.bCauseExpanded;
            if(Presented.Production.bConstruction && (Values.Contains(Id)||Id==TEXT("Inspector.History")||
                Id==TEXT("Inspector.Production.Cost")||Id==TEXT("Inspector.Production.Labor")||
                Id==TEXT("Inspector.Production.Laborers")||Id==TEXT("Inspector.Production.Artisans")))N.State.bVisible=false;
            if(Id.EndsWith(TEXT(".Tooltip")))
                if(const auto* Tip=ProductTooltips.Find(Id.LeftChop(8)))N.State.bVisible &= (*Tip)->IsOpen();
            if(Id.StartsWith(TEXT("Inspector.Production.Batch.")))N.State.bVisible &= BatchTooltip->IsOpen();
            N.bCanActivate &= N.State.bVisible && N.State.bEnabled;
        }
        Nodes.Add(MoveTemp(N));
    };
    Add(TEXT("Inspector.Root"),Presented.Production.bConstruction?TEXT("Building inspector"):TEXT("Production inspector"),Presented.ObjectStableId.ToString(),EHansaHudSemanticRole::Panel);
    Add(TEXT("Inspector.Identity"),Presented.Identity.ToString(),State->GetText().ToString());
    Add(TEXT("Inspector.Result"),Presented.Production.bConstruction?TEXT("Construction progress"):TEXT("Batch progress"),Percent->GetText().ToString());
    Add(TEXT("Inspector.Production.Batch"),Presented.Production.bConstruction?TEXT("Construction progress"):TEXT("Batch progress"),FString::Printf(TEXT("progressTicks=%d;cycleTicks=%d;completedBatches=%llu"),Presented.Production.ProgressTicks,Presented.Production.CycleTicks,Presented.Production.CompletedBatches));
    Add(TEXT("Inspector.Production.Batch.Tooltip"),TooltipHeading->GetText().ToString(),TEXT("hover"));
    Add(TEXT("Inspector.Production.Batch.Time"),TooltipDurationLabel->GetText().ToString(),TooltipTime->GetText().ToString());
    Add(TEXT("Inspector.Production.Batch.Percent"),TEXT("Process completed"),TooltipPercent->GetText().ToString());
    Add(TEXT("Inspector.Production.Portrait"),TEXT("Hansa worker portrait"),TEXT("Worker"));
    Add(TEXT("Inspector.Production.Cost"),TEXT("Cost per batch"),TEXT(""));
    Add(TEXT("Inspector.Production.Labor"),TEXT("Workers needed"),LaborSummary->GetText().ToString());
    for(bool Input:{true,false})for(const auto& P:Input?Presented.Production.Inputs:Presented.Production.Outputs)
    {
        const auto Id=PortId(Input,P.GoodId);
        Add(Id,P.Label.ToString(),Quantity(P.PerBatch).ToString());
        Add(Id+TEXT(".Tooltip"),P.Label.ToString(),ProductTooltipValues[Id]->GetText().ToString());
    }
    Add(TEXT("Inspector.Flows"),TEXT("Inputs and outputs"),RecipeText->GetText().ToString(),EHansaHudSemanticRole::Panel);
	for(const auto& Pair:Values)if(Pair.Key!=TEXT("Inspector.Logistics.MarketAccess"))Add(Pair.Key,Pair.Key,Pair.Value->GetText().ToString());
    Add(TEXT("Inspector.Production.Laborers"),TEXT("Laborers"),LaborerCount->GetText().ToString());
    Add(TEXT("Inspector.Production.Artisans"),TEXT("Artisans"),ArtisanCount->GetText().ToString());
    Add(TEXT("Inspector.Logistics.MarketAccess"),TEXT("Market access"),
        FString::Printf(TEXT("required=%s;connected=%s;selectedMarketBuildingId=%lld;roadDistanceCells=%d;deliveryBlocked=%s;blockedDeliveryCount=%d;failure=%s"),
            Presented.Production.bRoadRequired?TEXT("true"):TEXT("false"),
            Presented.Production.bHasMarketAccess?TEXT("true"):TEXT("false"),
            static_cast<long long>(Presented.Production.SelectedMarketBuildingValue),
            Presented.Production.MarketRoadDistanceCells,
            Presented.Production.bDeliveryBlocked?TEXT("true"):TEXT("false"),
            Presented.Production.BlockedDeliveryCount,
            *Presented.Production.MarketAccessCode.ToString()));
    Add(TEXT("Inspector.Problem"),StatusTitle->GetText().ToString(),StatusDetail->GetText().ToString(),EHansaHudSemanticRole::Alert);
    Add(TEXT("Inspector.Problem.Cause"),Presented.Causal.Cause.ToString(),Presented.Causal.Evidence.ToString());
    Add(TEXT("Inspector.History"),TEXT("Production record"),FString::Printf(TEXT("%llu batches"),Presented.Production.CompletedBatches));
    for(const auto& Pair:Buttons)
    {
        const auto* A=Presented.Actions.FindByPredicate([&](const auto& V){return V.StableId==FName(*Pair.Key);});
        const FString Label=Pair.Key==TEXT("Inspector.Close")?LOCTEXT("CloseLabel","Close production inspector").ToString():
            Pair.Key==TEXT("Inspector.Action.OpenCause")?LOCTEXT("DetailsLabel","Production details").ToString():A?A->Label.ToString():Pair.Key;
        Add(Pair.Key,Label,TEXT("action"),EHansaHudSemanticRole::Button);
    }
    return Nodes;
}
}
#undef LOCTEXT_NAMESPACE

