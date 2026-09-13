#pragma once
#include "CoreMinimal.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaUiComponents.h"
#include "Widgets/SCompoundWidget.h"

class SBox;
class SScrollBox;
class SToolTip;
class SVerticalBox;
class STextBlock;
class SBorder;
class SProgressBar;
struct FSlateDynamicImageBrush;

namespace Hansa::UI
{
class SHansaBatchRing;
class SHansaBatchToolTip;

/** Native reconstruction of the approved bakery reference, shared by all recipes. */
class HANSA_API SHansaProductionInspector final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaProductionInspector) : _Model(nullptr) {}
        SLATE_ARGUMENT(UHansaInspectorPresentationModel*, Model)
        SLATE_ARGUMENT(FUiPreferences, Preferences)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Refresh(const FHansaInspectorSnapshot& Snapshot);
    TSharedPtr<SWidget> Resolve(const FString& Id) const;
    bool Focus(const FString& Id);
    void Reveal(const FString& Id);
    const TArray<FString>& GetFocusOrder() const { return FocusOrder; }
    TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
private:
    TSharedRef<STextBlock> Text(FText Value, EHansaUiTypographyToken Token = EHansaUiTypographyToken::Data);
    TSharedRef<SWidget> Rule();
    TSharedRef<SHansaAction> Action(const TCHAR* Id, FText Label, bool Primary = false);
    TSharedRef<SWidget> Ledger(const FString& Id, FText Label, FText Value);
    void BuildPorts(const FHansaInspectorProductionData& Data);
    FReply Invoke(FName Id);
    void RecordFocus(FName Id);
    TWeakObjectPtr<UHansaInspectorPresentationModel> Model;
    FUiPreferences Preferences;
    FHansaInspectorSnapshot Presented;
    TSharedPtr<SScrollBox> Scroll;
    TSharedPtr<SBox> FlowHost;
    TSharedPtr<SVerticalBox> InputPorts, OutputPorts, Stocks, Records, ExtraActions, DetailsContent;
    TSharedPtr<STextBlock> Identity, State, Percent, Remaining, RecipeText, StatusTitle, StatusDetail, ActionResult;
    TSharedPtr<STextBlock> LaborerCount, ArtisanCount, CostValue, LaborSummary, TooltipTime, TooltipPercent;
    TSharedPtr<STextBlock> ProcessLabel, ProcessHeading, TooltipHeading, TooltipDurationLabel, RecordHeading;
    TSharedPtr<SWidget> InputArrow, OutputArrow, Footer;
    TSharedPtr<SVerticalBox> ConstructionDetails;
    TSharedPtr<SWidget> LaborerRow, ArtisanRow, Workforce, LaborFooter, BatchSurface;
    TSharedPtr<SHansaBatchToolTip> BatchTooltip;
    TMap<FString, TSharedPtr<SHansaBatchToolTip>> ProductTooltips;
    TMap<FString, TSharedPtr<STextBlock>> ProductTooltipValues;
    TSharedPtr<SProgressBar> LaborerBar, ArtisanBar;
    TSharedPtr<SHansaBatchRing> Ring;
    TSharedPtr<SHansaGlyph> StateGlyph, StatusGlyph;
    TSharedPtr<SBorder> StatusCard;
    FProgressBarStyle WorkforceStyle;
    FSlateBrush PaperBrush, NavyBrush, RecipeBrush, StatusBrush, PortraitFrame, TooltipBrush;
    TMap<FString, TSharedPtr<SWidget>> Targets;
    TMap<FString, TSharedPtr<STextBlock>> Values;
    TMap<FString, TSharedPtr<SHansaAction>> Buttons;
    TArray<TSharedPtr<FSlateDynamicImageBrush>> ArtBrushes, StaticArtBrushes;
    TArray<FString> FocusOrder;
};
}
