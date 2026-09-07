#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class SButton;
class SGridPanel;
class STextBlock;

namespace Hansa::UI
{
	/** Native build menu and placement palette. All dynamic state is pushed from the C++ model. */
	class HANSA_API SHansaBuildMenu final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaBuildMenu) : _Model(nullptr) {}
			SLATE_ARGUMENT(UHansaBuildMenuPresentationModel*, Model)
		SLATE_END_ARGS()

		~SHansaBuildMenu();
		void Construct(const FArguments& Arguments);
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		[[nodiscard]] const TArray<FString>& GetControllerFocusOrder() const { return FocusOrder; }

		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	private:
		void Refresh(const FHansaBuildMenuSnapshot& Snapshot, uint64 Revision);
		void RebuildCards(const FHansaBuildMenuSnapshot& Snapshot);
		FReply SelectCategory(EHansaBuildCategory Category);
		FReply SelectCard(FName BuildingId);
		FReply Invoke(TFunction<bool()> Intent);
		void MapWidget(const FString& Id, const TSharedPtr<SWidget>& Widget);
		FString CardSemanticId(FName BuildingId) const;

		TWeakObjectPtr<UHansaBuildMenuPresentationModel> Model;
		FDelegateHandle ChangedHandle;
		FSlateBrush PanelBrush;
		FSlateBrush WorkingBrush;
		FSlateBrush DecisionBrush;
		FButtonStyle PrimaryButtonStyle;
		FButtonStyle SecondaryButtonStyle;
		FButtonStyle IconButtonStyle;
		FTextBlockStyle DarkBodyStyle;
		FTextBlockStyle DarkCaptionStyle;
		FTextBlockStyle LightBodyStyle;
		FTextBlockStyle LightCaptionStyle;
		FTextBlockStyle LightHeadingStyle;
		TSharedPtr<SWidget> RootWidget;
		TSharedPtr<SWidget> CategoriesWidget;
		TSharedPtr<SGridPanel> CardsGrid;
		TSharedPtr<SWidget> CardsWidget;
		TSharedPtr<SWidget> DetailsWidget;
		TSharedPtr<STextBlock> DetailsText;
		TSharedPtr<SBorder> ValidationWidget;
		TSharedPtr<STextBlock> ValidationCauseText;
		TSharedPtr<STextBlock> ValidationRemedyText;
		TSharedPtr<SWidget> PreviewWidget;
		TSharedPtr<STextBlock> PreviewText;
		TSharedPtr<SBorder> FocusWidget;
		TSharedPtr<STextBlock> ResultText;
		TMap<FString, TWeakPtr<SWidget>> SemanticWidgets;
		TMap<EHansaBuildCategory, TSharedPtr<SButton>> CategoryButtons;
		TMap<FString, TSharedPtr<SButton>> CardButtons;
		TMap<FString, TSharedPtr<SButton>> ActionButtons;
		TArray<FString> FocusOrder;
	};
}
