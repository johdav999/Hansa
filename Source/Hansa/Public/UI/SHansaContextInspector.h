#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class SButton;
class STextBlock;
class SVerticalBox;
class UHansaInspectorPresentationModel;

namespace Hansa::UI
{
	/** Reusable, stable-order contextual inspector for buildings and residences. */
	class HANSA_API SHansaContextInspector final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaContextInspector) : _Model(nullptr) {}
			SLATE_ARGUMENT(UHansaInspectorPresentationModel*, Model)
		SLATE_END_ARGS()

		~SHansaContextInspector();
		void Construct(const FArguments& Arguments);
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		[[nodiscard]] const TArray<FString>& GetControllerFocusOrder() const { return FocusOrder; }
#if WITH_DEV_AUTOMATION_TESTS
		[[nodiscard]] int32 GetStructureRebuildCountForTesting() const { return StructureRebuildCount; }
#endif

		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	private:
		void Refresh(const FHansaInspectorSnapshot& Snapshot, uint64 Revision);
		void RebuildFlows(const FHansaInspectorSnapshot& Snapshot);
		void RebuildActions(const FHansaInspectorSnapshot& Snapshot);
		void RebuildHistory(const FHansaInspectorSnapshot& Snapshot);
		FReply Invoke(FName SemanticId);
		void MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget);
		static FString InstanceId(const TCHAR* Prefix, FName StableId);

		TWeakObjectPtr<UHansaInspectorPresentationModel> Model;
		FDelegateHandle ChangedHandle;
		uint64 PresentedRevision = 0;
		FSlateBrush WorkingBrush;
		FSlateBrush DecisionBrush;
		FSlateBrush CriticalBrush;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FTextBlockStyle LightHeadingStyle;
		FTextBlockStyle LightBodyStyle;
		FTextBlockStyle LightCaptionStyle;
		TSharedPtr<SBorder> RootWidget;
		TSharedPtr<SWidget> IdentityWidget;
		TSharedPtr<STextBlock> IdentityText;
		TSharedPtr<STextBlock> StateText;
		TSharedPtr<SWidget> ResultWidget;
		TSharedPtr<STextBlock> ResultText;
		TSharedPtr<SWidget> FlowsWidget;
		TSharedPtr<SVerticalBox> FlowRows;
		TSharedPtr<SBorder> ProblemWidget;
		TSharedPtr<STextBlock> ProblemText;
		TSharedPtr<SWidget> CauseWidget;
		TSharedPtr<STextBlock> CauseText;
		TSharedPtr<STextBlock> EvidenceText;
		TSharedPtr<STextBlock> RemedyText;
		TSharedPtr<SWidget> ActionsWidget;
		TSharedPtr<SVerticalBox> ActionRows;
		TSharedPtr<SWidget> HistoryWidget;
		TSharedPtr<SVerticalBox> HistoryRows;
		TSharedPtr<SButton> CloseButton;
		TSharedPtr<STextBlock> ResultStatusText;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
		TMap<FString, TSharedPtr<SButton>> ActionButtons;
		TArray<FString> FocusOrder;
		TArray<FHansaInspectorFlowPresentation> PresentedFlows;
		TArray<FHansaInspectorActionPresentation> PresentedActions;
		TArray<FHansaInspectorHistoryPresentation> PresentedHistory;
		bool bStructureInitialized = false;
#if WITH_DEV_AUTOMATION_TESTS
		int32 StructureRebuildCount = 0;
#endif
	};
}
