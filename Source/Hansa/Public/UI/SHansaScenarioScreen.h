#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaHudSemantics.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
class SProgressBar;
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
			SLATE_ARGUMENT(UHansaScenarioPresentationModel*, Model)
		SLATE_END_ARGS()
		~SHansaScenarioScreen();
		void Construct(const FArguments& Arguments);
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
		uint64 PresentedRevision = 0;
		FSlateBrush ScrimBrush;
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
		TSharedPtr<STextBlock> TitleText;
		TSharedPtr<STextBlock> StateText;
		TSharedPtr<STextBlock> BriefingText;
		TSharedPtr<STextBlock> OutcomeText;
		TSharedPtr<SVerticalBox> PathRows;
		TSharedPtr<SVerticalBox> ObjectiveRows;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
	};
}
