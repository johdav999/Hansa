#pragma once
#include "CoreMinimal.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaUiComponents.h"
#include "Widgets/SCompoundWidget.h"
class SVerticalBox;
class STextBlock;
class SScrollBox;
class SProgressBar;
struct FSlateDynamicImageBrush;
namespace Hansa::UI {
class HANSA_API SHansaResidenceInspector final:public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SHansaResidenceInspector):_Model(nullptr){}
  SLATE_ARGUMENT(UHansaInspectorPresentationModel*,Model)
  SLATE_ARGUMENT(FUiPreferences,Preferences)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 void Refresh(const FHansaInspectorSnapshot& Snapshot);
 TSharedPtr<SWidget> Resolve(const FString& Id) const;
 bool Focus(const FString& Id);
 void Reveal(const FString& Id);
 const TArray<FString>& GetFocusOrder()const{return FocusOrder;}
 TArray<FHansaHudSemanticNode> GetSemanticSnapshot()const;
private:
 TSharedRef<STextBlock> Text(FText Value,EHansaUiTypographyToken Token=EHansaUiTypographyToken::Body);
 TSharedRef<SHansaAction> Action(FName Id,FText Label);
 TWeakObjectPtr<UHansaInspectorPresentationModel> Model;
 FUiPreferences Preferences;
 FHansaInspectorSnapshot Presented;
 FSlateBrush Paper,Navy,Arch,Tip;
 FProgressBarStyle BarStyle;
 TSharedPtr<FSlateDynamicImageBrush> PortraitBrush;
 TSharedPtr<STextBlock> NeedsHeading;
 TSharedPtr<STextBlock> Identity,Occupancy,State,Cause,Result;
 TSharedPtr<SProgressBar> OccupancyBar;
 TSharedPtr<SScrollBox> Scroll;
 TSharedPtr<SVerticalBox> Needs,Details,Actions;
 TSharedPtr<SHansaAction> DetailsButton;
 TArray<FName> NeedIds;
 TArray<FString> FocusOrder;
 TMap<FString,TSharedPtr<SWidget>> Targets;
 TMap<FString,TSharedPtr<SHansaAction>> Buttons;
 TMap<FName,TSharedPtr<STextBlock>> NeedPercents,NeedHints,NeedAmounts;
 TMap<FName,TSharedPtr<SProgressBar>> NeedBars;
};
}
