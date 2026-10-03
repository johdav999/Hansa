#pragma once
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaUiComponents.h"
#include "Widgets/SCompoundWidget.h"
class SVerticalBox;
class STextBlock;
class SScrollBox;
class SProgressBar;
struct FSlateDynamicImageBrush;

namespace Hansa::UI
{
class HANSA_API SHansaShipInspector final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHansaShipInspector) : _Model(nullptr) {}
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
private:
    TSharedRef<STextBlock> Text(FText Value, EHansaUiTypographyToken Token = EHansaUiTypographyToken::SerifBody, bool Dark = false);
    TSharedRef<SHansaAction> Action(FName Id, FText Label, EHansaUiButtonStyle Kind = EHansaUiButtonStyle::Secondary);
    TSharedRef<SWidget> Section(FText Label, const TSharedRef<SWidget>& Content);
    TWeakObjectPtr<UHansaInspectorPresentationModel> Model;
    FUiPreferences Preferences;
    FHansaInspectorSnapshot Presented;
    FSlateBrush Paper, Navy, Rule, Card;
    FProgressBarStyle BarStyle;
    TSharedPtr<FSlateDynamicImageBrush> Portrait, EmptyHold, Linen;
    TSharedPtr<STextBlock> Identity, State, Voyage, Location, Cargo, Upkeep, History, Cause, Result, Overflow;
    TSharedPtr<SProgressBar> Progress, Capacity;
    TSharedPtr<SScrollBox> Scroll;
    TSharedPtr<SVerticalBox> Details, ExtraActions;
    TArray<TSharedPtr<STextBlock>> SlotLabels, SlotAmounts;
    TArray<TSharedPtr<SHansaGlyph>> SlotGlyphs;
    TArray<TSharedPtr<SWidget>> EmptyImages;
    TMap<FString,TSharedPtr<SWidget>> Targets;
    TMap<FString,TSharedPtr<SHansaAction>> Buttons;
    TArray<FString> FocusOrder;
};
}
