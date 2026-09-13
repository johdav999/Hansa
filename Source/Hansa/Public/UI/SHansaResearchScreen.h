#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaResearchPresentationModel.h"
#include "Widgets/SCompoundWidget.h"
#include "UI/HansaUiComponents.h"

class SBox;
class SScrollBox;
class SButton;
class SVerticalBox;
class SHorizontalBox;

namespace Hansa::UI
{
	/** Native bounded three-branch runtime research surface. */
	class HANSA_API SHansaResearchScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaResearchScreen) : _Model(nullptr) {}
			SLATE_ARGUMENT(FUiPreferences, Preferences)
			SLATE_ARGUMENT(UHansaResearchPresentationModel*, Model)
		SLATE_END_ARGS()

		~SHansaResearchScreen();
		void Construct(const FArguments& Arguments);
		void SetPresentationSize(FIntPoint Size);
		TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const { const auto* W=Widgets.Find(Id); return W?W->Pin():nullptr; }
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		[[nodiscard]] TArray<FString> GetControllerFocusOrder() const;
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	private:
		void Refresh(const FHansaResearchPresentationSnapshot& Snapshot, uint64 Revision);
		void RebuildBranch(SVerticalBox& Box, Hansa::Simulation::EHansaResearchBranch Branch,
			const FHansaResearchPresentationSnapshot& Snapshot);
		FReply HandleClose();
		FReply HandleQueue();
		FReply HandleSelect(FString TechnologyId);
		static FString SafeId(const FString& StableId);

		TWeakObjectPtr<UHansaResearchPresentationModel> Model;
		FDelegateHandle ChangedHandle;
		FUiPreferences Preferences;
		FString PresentedContentKey;
		FSlateBrush PanelBrush;
		FSlateBrush HeaderBrush;
		FSlateBrush InnerBrush;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FTextBlockStyle HeadingStyle;
		FTextBlockStyle BodyStyle;
		FTextBlockStyle CaptionStyle;
        bool bCompact = false;
        Hansa::Simulation::EHansaResearchBranch ActiveBranch = Hansa::Simulation::EHansaResearchBranch::Commerce;
        TSharedPtr<SHorizontalBox> BranchTabs;
        TSharedPtr<SBox> Root;
		TSharedPtr<SScrollBox> CommerceScroll,ProductionScroll,LogisticsScroll,DetailScroll;
		TSharedPtr<SButton> CloseButton;
		TSharedPtr<SVerticalBox> Commerce;
		TSharedPtr<SVerticalBox> Production;
		TSharedPtr<SVerticalBox> Logistics;
		TSharedPtr<SVerticalBox> Detail;
        TSharedPtr<SVerticalBox> Actions;
		TSharedPtr<SVerticalBox> Queue;
		TMap<FString, TWeakPtr<SWidget>> Widgets;
	};
}
