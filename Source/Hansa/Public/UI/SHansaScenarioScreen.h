#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaHudSemantics.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaUiComponents.h"

class SBox;
class SButton;
class SProgressBar;
class SScrollBox;
class SHorizontalBox;
class STextBlock;
class SVerticalBox;
class UHansaScenarioPresentationModel;
struct FHansaScenarioPresentationSnapshot;

namespace Hansa::UI
{
	/** Native, responsive scenario briefing/progress/outcome dossier. */
	class HANSA_API SHansaScenarioScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaScenarioScreen) : _Model(nullptr) {}
			SLATE_ARGUMENT(FUiPreferences, Preferences)
			SLATE_ARGUMENT(UHansaScenarioPresentationModel*, Model)
		SLATE_END_ARGS()
		~SHansaScenarioScreen();
		void Construct(const FArguments& Arguments);
		void SetPresentationSize(FIntPoint Size);
		TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const { const auto* W=SemanticWidgets.Find(Id); return W?W->Pin():nullptr; }
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	private:
		void Refresh(const FHansaScenarioPresentationSnapshot& Snapshot, uint64 Revision);
		FReply HandleClose();
		FReply HandleBegin();
		FReply HandleSelectPath(FName VictoryId);
		void RebuildPaths(const FHansaScenarioPresentationSnapshot& Snapshot);
		void RebuildObjectives(const FHansaScenarioPresentationSnapshot& Snapshot);
		TWeakObjectPtr<UHansaScenarioPresentationModel> Model;
		FDelegateHandle ChangedHandle;
		TSharedPtr<SBox> PanelSize;
        FIntPoint AvailableSize=FIntPoint(1120,680);
		FUiPreferences Preferences;
		FString PresentedContentKey;
		uint64 PresentedRevision = 0;
		FSlateBrush ScrimBrush;
		FSlateBrush HeaderBrush;
		FSlateBrush DecisionBrush;
		FSlateBrush WorkingBrush;
		FSlateBrush SuccessBrush;
		FSlateBrush CriticalBrush;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FTextBlockStyle HeadingStyle;
		FTextBlockStyle BodyStyle;
		FTextBlockStyle DataStyle;
		TSharedPtr<SWidget> RootWidget;
		TSharedPtr<SButton> CloseButton;
		TSharedPtr<SButton> BeginButton;
        TSharedPtr<SButton> SaveButton;
        TSharedPtr<SVerticalBox> MenuRows;
		TSharedPtr<STextBlock> TitleText;
		TSharedPtr<STextBlock> StateText;
		TSharedPtr<STextBlock> BriefingText;
		TSharedPtr<STextBlock> OutcomeText;
		TSharedPtr<SHorizontalBox> PathRows;
		TSharedPtr<SScrollBox> ContentScroll;
		FProgressBarStyle ProgressStyle;
		TSharedPtr<SVerticalBox> ObjectiveRows;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
	};
}
