#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaHudSemantics.h"
#include "UI/HansaUiComponents.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class SButton;
class SGridPanel;
class SHorizontalBox;
class SUniformGridPanel;
class STextBlock;
class SScrollBox;
class AHansaStrategyPlayerController;

namespace Hansa::UI
{
	/** Native build menu and placement palette. All dynamic state is pushed from the C++ model. */
	class HANSA_API SHansaBuildMenu final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SHansaBuildMenu) : _Model(nullptr), _PlacementController(nullptr) {}
			SLATE_ARGUMENT(UHansaBuildMenuPresentationModel*, Model)
			SLATE_ARGUMENT(FUiPreferences, Preferences)
			SLATE_ARGUMENT(AHansaStrategyPlayerController*, PlacementController)
		SLATE_END_ARGS()

		~SHansaBuildMenu();
		void Construct(const FArguments& Arguments);
		[[nodiscard]] TArray<FHansaHudSemanticNode> GetSemanticSnapshot() const;
		TSharedPtr<SWidget> ResolveSemanticWidget(const FString& Id) const
		{ const auto* Value=SemanticWidgets.Find(Id); return Value?Value->Pin():nullptr; }
		void SetPreferences(FUiPreferences InPreferences);
		bool ActivateSemanticId(const FString& SemanticId);
		bool FocusSemanticId(const FString& SemanticId);
		[[nodiscard]] const TArray<FString>& GetControllerFocusOrder() const { return FocusOrder; }
		[[nodiscard]] bool IsScreenPositionOverMenu(FVector2D AbsoluteScreenPosition) const;

		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	private:
		void Refresh(const FHansaBuildMenuSnapshot& Snapshot, uint64 Revision);
		void RebuildCategories(const FHansaBuildMenuSnapshot& Snapshot);
		void RebuildChains(const FHansaBuildMenuSnapshot& Snapshot);
		void RebuildCards(const FHansaBuildMenuSnapshot& Snapshot);
		FReply ToggleDemolition();
		FReply SelectCategory(EHansaBuildCategory Category);
		FReply SelectChain(FName OutputGoodId);
  FReply SelectTier(EHansaBuildTier Tier);
		FReply SelectCard(FName BuildingId);
		FReply Invoke(TFunction<bool()> Intent);
		void MapWidget(const FString& Id, const TSharedPtr<SWidget>& Widget);
		FString CardSemanticId(FName BuildingId) const;
		FString ChainSemanticId(FName OutputGoodId) const;

		TWeakObjectPtr<UHansaBuildMenuPresentationModel> Model;
		TWeakObjectPtr<AHansaStrategyPlayerController> PlacementController;
		FDelegateHandle ChangedHandle;
		FUiPreferences Preferences;
		TArray<EHansaBuildCategory> CachedCategories;
		TArray<FHansaBuildChainPresentation> CachedChains;
  TOptional<EHansaBuildTier> LaidOutChainTier;
		TArray<FName> CachedCardIds;
		TArray<FString> ConnectorIds;
		TSharedPtr<SWidget> ExpansionWidget;
		TSharedPtr<SScrollBox> CardScroll;
		TSharedPtr<SHansaGlyph> ValidationIcon;
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
		TSharedPtr<SHorizontalBox> CategoryRow;
  TSharedPtr<SScrollBox> TierScroll;
  TSharedPtr<SHorizontalBox> TierRow;
  TSharedPtr<STextBlock> EmptyTierText;
  TMap<EHansaBuildTier, TSharedPtr<SHansaAction>> TierButtons;
		TSharedPtr<SHansaAction> DemolitionButton;
		TSharedPtr<SGridPanel> CardsGrid;
		TSharedPtr<SUniformGridPanel> ChainsGrid;
		TSharedPtr<SWidget> CardsWidget;
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
		TMap<FString, TSharedPtr<SButton>> ChainButtons;

		TArray<FString> FocusOrder;
	};
}
