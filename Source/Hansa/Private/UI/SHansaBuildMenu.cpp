#include "UI/SHansaBuildMenu.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaBuildMenu"

namespace Hansa::UI
{
	namespace
	{
		const EHansaBuildCategory Categories[] = {
			EHansaBuildCategory::Roads, EHansaBuildCategory::Residences, EHansaBuildCategory::Production,
			EHansaBuildCategory::Storage, EHansaBuildCategory::Harbor, EHansaBuildCategory::Civic,
			EHansaBuildCategory::Decoration
		};

		FString CategorySemanticId(const EHansaBuildCategory Category)
		{
			return FString::Printf(TEXT("BuildMenu.Category.%s"), ::LexToString(Category));
		}
	}

	SHansaBuildMenu::~SHansaBuildMenu()
	{
		if (UHansaBuildMenuPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaBuildMenu::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model;
		PanelBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::WorldOverlay);
		WorkingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		DecisionBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		IconButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Icon);
		DarkBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, true);
		DarkCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, true);
		LightBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		LightCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);
		LightHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);

		TSharedPtr<SHorizontalBox> CategoryRow;
		TSharedPtr<SUniformGridPanel> TargetRow;
		TSharedPtr<SUniformGridPanel> ActionRow;
		ChildSlot
		[
			SAssignNew(RootWidget, SBorder).BorderImage(&PanelBrush).Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(LOCTEXT("BuildModeHeading", "Build mode · Lübeck")).TextStyle(&DarkBodyStyle)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Text(LOCTEXT("InputHelp", "Choose · click/A   Back · Esc/B")).TextStyle(&DarkCaptionStyle)
					]
				]
				+ SVerticalBox::Slot().AutoHeight()[SAssignNew(CategoriesWidget, SBorder).BorderImage(&PanelBrush).Padding(0.0f)[SAssignNew(CategoryRow, SHorizontalBox)]]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 6.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SAssignNew(CardsWidget, SBorder).BorderImage(&WorkingBrush).Padding(8.0f)[SAssignNew(CardsGrid, SGridPanel)]
						]
						+ SHorizontalBox::Slot().FillWidth(0.72f).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SAssignNew(DetailsWidget, SBorder).BorderImage(&WorkingBrush).Padding(10.0f)
							[
								SAssignNew(DetailsText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)
							]
						]
						+ SHorizontalBox::Slot().FillWidth(0.62f).Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SAssignNew(ValidationWidget, SBorder).BorderImage(&DecisionBrush).Padding(10.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SAssignNew(ValidationCauseText, STextBlock).TextStyle(&LightHeadingStyle)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(ValidationRemedyText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(PreviewWidget, SBorder).BorderImage(&WorkingBrush).Padding(4.0f)[SAssignNew(PreviewText, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
							]
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[SAssignNew(TargetRow, SUniformGridPanel).SlotPadding(FMargin(2.0f))]
				+ SVerticalBox::Slot().AutoHeight()[SAssignNew(ActionRow, SUniformGridPanel).SlotPadding(FMargin(2.0f))]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SAssignNew(FocusWidget, SBorder).BorderImage(&PanelBrush).Padding(4.0f)
					[
						SAssignNew(ResultText, STextBlock).TextStyle(&DarkCaptionStyle)
					]
				]
			]
		];

		MapWidget(TEXT("BuildMenu.Root"), RootWidget);
		MapWidget(TEXT("BuildMenu.Categories"), CategoriesWidget);
		MapWidget(TEXT("BuildMenu.Cards"), CardsWidget);
		MapWidget(TEXT("BuildMenu.Selection"), DetailsWidget);
		MapWidget(TEXT("Placement.Root"), RootWidget);
		MapWidget(TEXT("Placement.Validation"), ValidationWidget);
		MapWidget(TEXT("Placement.Validation.Cause"), ValidationCauseText);
		MapWidget(TEXT("Placement.Validation.Remedy"), ValidationRemedyText);
		MapWidget(TEXT("Placement.Preview"), PreviewWidget);
		MapWidget(TEXT("Placement.Footprint"), PreviewWidget);

		int32 CategoryIndex = 1;
		for (const EHansaBuildCategory Category : Categories)
		{
			const FString Id = CategorySemanticId(Category);
			TSharedPtr<SButton> Button;
			CategoryRow->AddSlot().FillWidth(1.0f).Padding(2.0f)
			[
				SAssignNew(Button, SButton).ButtonStyle(&IconButtonStyle)
				.Text(FText::FromString(FString::Printf(TEXT("[%d] %s"), CategoryIndex++, ::LexToString(Category))))
				.OnClicked(this, &SHansaBuildMenu::SelectCategory, Category)
			];
			CategoryButtons.Add(Category, Button); MapWidget(Id, Button);
		}

		auto AddAction = [this](const TSharedPtr<SUniformGridPanel>& Row, const int32 SlotIndex, const int32 ColumnCount,
			const TCHAR* Id, const FText& Label, TFunction<bool()> Intent, const FButtonStyle* Style)
		{
			TSharedPtr<SButton> Button;
			Row->AddSlot(SlotIndex % ColumnCount, SlotIndex / ColumnCount).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
			[
				SNew(SBox).MinDesiredHeight(40.0f)
				[
					SAssignNew(Button, SButton).ButtonStyle(Style).Text(Label).HAlign(HAlign_Center)
					.OnClicked(this, &SHansaBuildMenu::Invoke, MoveTemp(Intent))
				]
			];
			ActionButtons.Add(Id, Button); MapWidget(Id, Button);
		};

		AddAction(TargetRow, 0, 4, TEXT("Placement.Target.Road"), LOCTEXT("RoadTarget", "Road target 18,16"), [this] { return Model->TargetGridCell(18, 16); }, &SecondaryButtonStyle);
		AddAction(TargetRow, 1, 4, TEXT("Placement.Target.Disconnected"), LOCTEXT("InvalidTarget", "Disconnected 10,10"), [this] { return Model->TargetGridCell(10, 10); }, &SecondaryButtonStyle);
		AddAction(TargetRow, 2, 4, TEXT("Placement.Target.Adjacent"), LOCTEXT("ValidTarget", "Fit beside road"), [this] { return Model->TargetRoadAdjacentIntent(); }, &SecondaryButtonStyle);
		AddAction(TargetRow, 3, 4, TEXT("Placement.Target.Shore"), LOCTEXT("ShoreTarget", "Find shoreline"), [this] { return Model->TargetShorelineIntent(); }, &SecondaryButtonStyle);
		AddAction(ActionRow, 0, 4, TEXT("Placement.Overlay.Grid"), LOCTEXT("GridOverlay", "Grid [G]"), [this] { return Model->ToggleGridOverlayIntent(); }, &IconButtonStyle);
		AddAction(ActionRow, 1, 4, TEXT("Placement.Overlay.Road"), LOCTEXT("RoadOverlay", "Road overlay [O]"), [this] { return Model->ToggleRoadOverlayIntent(); }, &IconButtonStyle);
		AddAction(ActionRow, 2, 4, TEXT("BuildMenu.CardAction.Favorite"), LOCTEXT("Favorite", "★ Favorite [F]"), [this] { return Model->ToggleFavoriteIntent(); }, &IconButtonStyle);
		AddAction(ActionRow, 3, 4, TEXT("BuildMenu.CardAction.Compare"), LOCTEXT("Compare", "Compare [C]"), [this] { return Model->CompareIntent(); }, &IconButtonStyle);
		AddAction(ActionRow, 4, 4, TEXT("Placement.Action.Rotate"), LOCTEXT("Rotate", "↻ Rotate [R]"), [this] { return Model->RotateIntent(); }, &IconButtonStyle);
		AddAction(ActionRow, 5, 4, TEXT("Placement.Action.Repeat"), LOCTEXT("Repeat", "⟳ Repeat [T]"), [this] { return Model->ToggleRepeatIntent(); }, &IconButtonStyle);
		AddAction(ActionRow, 6, 4, TEXT("Placement.Action.Confirm"), LOCTEXT("Confirm", "✓ Confirm [Enter/A]"), [this] { return Model->ConfirmIntent(); }, &PrimaryButtonStyle);
		AddAction(ActionRow, 7, 4, TEXT("Placement.Action.Cancel"), LOCTEXT("Cancel", "✕ Cancel [Esc/B]"), [this] { return Model->CancelIntent(); }, &SecondaryButtonStyle);

		if (UHansaBuildMenuPresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaBuildMenu::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
		else
		{
			RootWidget->SetVisibility(EVisibility::Collapsed);
		}
	}

	FString SHansaBuildMenu::CardSemanticId(const FName BuildingId) const
	{
		FString Suffix = BuildingId.ToString(); Suffix.ReplaceInline(TEXT("."), TEXT("_"));
		return FString::Printf(TEXT("BuildMenu.Card.%s"), *Suffix);
	}

	void SHansaBuildMenu::RebuildCards(const FHansaBuildMenuSnapshot& Snapshot)
	{
		CardsGrid->ClearChildren(); CardButtons.Reset();
		int32 Index = 0;
		for (const FHansaBuildCardPresentation& Card : Snapshot.Cards)
		{
			if (Card.Category != Snapshot.SelectedCategory) continue;
			const FString Id = CardSemanticId(Card.StableId);
			const FString Lock = Card.bLocked ? FString::Printf(TEXT("\n◉ Locked · %s"), *Card.LockedReason.ToString()) : TEXT("");
			const FText Label = FText::FromString(FString::Printf(TEXT("%s\n%s · %s\n%s%s"),
				*Card.Name.ToString(), *Card.Cost.ToString(), *Card.Footprint.ToString(), *Card.InputOutput.ToString(), *Lock));
			TSharedPtr<SButton> Button;
			CardsGrid->AddSlot(Index % 4, Index / 4).Padding(3.0f)
			[
				SAssignNew(Button, SButton).ButtonStyle(&SecondaryButtonStyle).Text(Label).IsEnabled(!Card.bLocked)
				.OnClicked(this, &SHansaBuildMenu::SelectCard, Card.StableId)
			];
			CardButtons.Add(Id, Button); MapWidget(Id, Button); ++Index;
		}
	}

	void SHansaBuildMenu::Refresh(const FHansaBuildMenuSnapshot& Snapshot, const uint64 Revision)
	{
		(void)Revision;
		RootWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		RebuildCards(Snapshot);
		FocusOrder.Reset();
		for (const EHansaBuildCategory Category : Categories) FocusOrder.Add(CategorySemanticId(Category));
		for (const FHansaBuildCardPresentation& Card : Snapshot.Cards)
		{
			if (Card.Category == Snapshot.SelectedCategory && !Card.bLocked) FocusOrder.Add(CardSemanticId(Card.StableId));
		}
		for (const TCHAR* Id : {
			TEXT("Placement.Target.Road"), TEXT("Placement.Target.Disconnected"), TEXT("Placement.Target.Adjacent"), TEXT("Placement.Target.Shore"),
			TEXT("Placement.Overlay.Grid"), TEXT("Placement.Overlay.Road"), TEXT("BuildMenu.CardAction.Favorite"), TEXT("BuildMenu.CardAction.Compare"),
			TEXT("Placement.Action.Rotate"), TEXT("Placement.Action.Repeat"), TEXT("Placement.Action.Confirm"), TEXT("Placement.Action.Cancel") })
		{
			FocusOrder.Add(Id);
		}
		for (const TPair<EHansaBuildCategory, TSharedPtr<SButton>>& Pair : CategoryButtons)
		{
			Pair.Value->SetBorderBackgroundColor(Pair.Key == Snapshot.SelectedCategory
				? UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass)
				: UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::HarborSlate));
		}
		const FHansaBuildCardPresentation* Selected = Snapshot.Cards.FindByPredicate(
			[&Snapshot](const FHansaBuildCardPresentation& Card) { return Card.StableId == Snapshot.SelectedBuildingId; });
		DetailsText->SetText(Selected == nullptr ? LOCTEXT("ChooseCard", "Choose a building card. Cost, workforce, upkeep, footprint, chain and prerequisites appear here.")
			: FText::FromString(FString::Printf(TEXT("%s · %s\nCost  %s\n%s\nFootprint  %s\nChain  %s%s"),
				*Selected->Name.ToString(), *Selected->Tier.ToString(), *Selected->Cost.ToString(),
				*Selected->WorkforceAndUpkeep.ToString(), *Selected->Footprint.ToString(), *Selected->InputOutput.ToString(),
				Selected->bFavorite ? TEXT("\n★ Favorite") : TEXT(""))));
		ValidationWidget->SetVisibility(Snapshot.bHasTarget ? EVisibility::Visible : EVisibility::Collapsed);
		ValidationWidget->SetBorderBackgroundColor(Snapshot.Feedback == EHansaPlacementFeedback::Invalid
			? UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oxblood).CopyWithNewOpacity(0.24f)
			: UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal).CopyWithNewOpacity(0.20f));
		ValidationCauseText->SetText(Snapshot.ValidationCause); ValidationRemedyText->SetText(Snapshot.ValidationRemedy);
		PreviewText->SetText(Snapshot.PlacementSummary);
		ResultText->SetText(Snapshot.LastResult.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("Focus · %s"), *Snapshot.FocusedSemanticId.ToString())) : Snapshot.LastResult);
		FocusWidget->SetBorderBackgroundColor(Snapshot.FocusedSemanticId.IsNone()
			? UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy)
			: UHansaUiStyleLibrary::GetFocusStyle().Color);
		auto Enable = [this](const TCHAR* Id, const bool bEnabled) { if (const TSharedPtr<SButton>* B = ActionButtons.Find(Id)) (*B)->SetEnabled(bEnabled); };
		const bool bSelected = !Snapshot.SelectedBuildingId.IsNone();
		Enable(TEXT("Placement.Action.Rotate"), bSelected); Enable(TEXT("Placement.Action.Repeat"), bSelected);
		Enable(TEXT("Placement.Action.Confirm"), Snapshot.bCanConfirm); Enable(TEXT("Placement.Action.Cancel"), bSelected);
		Enable(TEXT("BuildMenu.CardAction.Favorite"), bSelected); Enable(TEXT("BuildMenu.CardAction.Compare"), bSelected);
		Enable(TEXT("Placement.Target.Road"), bSelected); Enable(TEXT("Placement.Target.Disconnected"), bSelected);
		Enable(TEXT("Placement.Target.Adjacent"), bSelected);
		Enable(TEXT("Placement.Target.Shore"), Model.IsValid() && Model->FindValidShorelineTarget().IsSet());
	}

	FReply SHansaBuildMenu::SelectCategory(const EHansaBuildCategory Category)
	{
		return Invoke([this, Category] { return Model->SelectCategory(Category); });
	}

	FReply SHansaBuildMenu::SelectCard(const FName BuildingId)
	{
		return Invoke([this, BuildingId] { return Model->SelectBuilding(BuildingId); });
	}

	FReply SHansaBuildMenu::Invoke(TFunction<bool()> Intent)
	{
		return Intent && Intent() ? FReply::Handled() : FReply::Unhandled();
	}

	void SHansaBuildMenu::MapWidget(const FString& Id, const TSharedPtr<SWidget>& Widget)
	{
		SemanticWidgets.Add(Id, Widget);
	}

	bool SHansaBuildMenu::ActivateSemanticId(const FString& SemanticId)
	{
		if (UHansaBuildMenuPresentationModel* Pinned = Model.Get())
		{
			for (const EHansaBuildCategory Category : Categories) if (SemanticId == CategorySemanticId(Category)) return Pinned->SelectCategory(Category);
			for (const FHansaBuildCardPresentation& Card : Pinned->GetSnapshot().Cards) if (SemanticId == CardSemanticId(Card.StableId)) return Pinned->SelectBuilding(Card.StableId);
			if (SemanticId == TEXT("Placement.Target.Road")) return Pinned->TargetGridCell(18, 16);
			if (SemanticId == TEXT("Placement.Target.Disconnected")) return Pinned->TargetGridCell(10, 10);
			if (SemanticId == TEXT("Placement.Target.Adjacent")) return Pinned->TargetRoadAdjacentIntent();
			if (SemanticId == TEXT("Placement.Target.Shore")) return Pinned->TargetShorelineIntent();
			if (SemanticId == TEXT("Placement.Overlay.Grid")) return Pinned->ToggleGridOverlayIntent();
			if (SemanticId == TEXT("Placement.Overlay.Road")) return Pinned->ToggleRoadOverlayIntent();
			if (SemanticId == TEXT("BuildMenu.CardAction.Favorite")) return Pinned->ToggleFavoriteIntent();
			if (SemanticId == TEXT("BuildMenu.CardAction.Compare")) return Pinned->CompareIntent();
			if (SemanticId == TEXT("Placement.Action.Rotate")) return Pinned->RotateIntent();
			if (SemanticId == TEXT("Placement.Action.Repeat")) return Pinned->ToggleRepeatIntent();
			if (SemanticId == TEXT("Placement.Action.Confirm")) return Pinned->ConfirmIntent();
			if (SemanticId == TEXT("Placement.Action.Cancel")) return Pinned->CancelIntent();
		}
		return false;
	}

	bool SHansaBuildMenu::FocusSemanticId(const FString& SemanticId)
	{
		const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found ? Found->Pin() : nullptr;
		if (!Widget.IsValid() || !Widget->IsEnabled()) return false;
		if (UHansaBuildMenuPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
		return true;
	}

	FReply SHansaBuildMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		if (!Model.IsValid()) return FReply::Unhandled();
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::One) return SelectCategory(EHansaBuildCategory::Roads);
		if (Key == EKeys::Two) return SelectCategory(EHansaBuildCategory::Residences);
		if (Key == EKeys::Three) return SelectCategory(EHansaBuildCategory::Production);
		if (Key == EKeys::Four) return SelectCategory(EHansaBuildCategory::Storage);
		if (Key == EKeys::Five) return SelectCategory(EHansaBuildCategory::Harbor);
		if (Key == EKeys::Six) return SelectCategory(EHansaBuildCategory::Civic);
		if (Key == EKeys::Seven) return SelectCategory(EHansaBuildCategory::Decoration);
		if (Key == EKeys::R) return Invoke([this] { return Model->RotateIntent(); });
		if (Key == EKeys::T) return Invoke([this] { return Model->ToggleRepeatIntent(); });
		if (Key == EKeys::G) return Invoke([this] { return Model->ToggleGridOverlayIntent(); });
		if (Key == EKeys::O) return Invoke([this] { return Model->ToggleRoadOverlayIntent(); });
		if (Key == EKeys::F) return Invoke([this] { return Model->ToggleFavoriteIntent(); });
		if (Key == EKeys::C) return Invoke([this] { return Model->CompareIntent(); });
		if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom) return Invoke([this] { return Model->ConfirmIntent(); });
		if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) return Invoke([this] { return Model->CancelIntent(); });
		return FReply::Unhandled();
	}

	TArray<FHansaHudSemanticNode> SHansaBuildMenu::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Nodes;
		const UHansaBuildMenuPresentationModel* Pinned = Model.Get();
		if (Pinned == nullptr) return Nodes;
		const FHansaBuildMenuSnapshot& S = Pinned->GetSnapshot();
		auto Add = [this, &Nodes, &S](const FString& Id, const TCHAR* Parent, const FString& Label, const EHansaHudSemanticRole Role,
			const bool bActivate = false, const bool bFocus = false, const FString& Type = FString(), const FString& Value = FString(),
			const bool bSelected = false, const bool bEnabled = true, const bool bError = false, const bool bWarning = false)
		{
			FHansaHudSemanticNode N; N.Id = Id; N.ParentId = Parent; N.Label = Label; N.Role = Role;
			N.bCanActivate = bActivate; N.bCanFocus = bFocus; N.State.bVisible = S.bOpen; N.State.bEnabled = bEnabled;
			N.State.bSelected = bSelected; N.State.bFocused = S.FocusedSemanticId == FName(*Id); N.State.bError = bError; N.State.bWarning = bWarning;
			N.State.ValueType = Type; N.State.Value = Value;
			if (const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(Id)) if (const TSharedPtr<SWidget> W = Found->Pin())
			{
				const FGeometry& G = W->GetCachedGeometry(); const FVector2f O = RootWidget->GetCachedGeometry().GetAbsolutePosition();
				const FVector2f P = G.GetAbsolutePosition() - O; const FVector2f Z = G.GetDrawSize();
				N.Bounds = FIntRect(FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y), FMath::RoundToInt(P.X + Z.X), FMath::RoundToInt(P.Y + Z.Y));
			}
			Nodes.Add(MoveTemp(N));
		};
		Add(TEXT("BuildMenu.Root"), TEXT("HUD.BottomArea"), TEXT("Build menu"), EHansaHudSemanticRole::Panel, false, false, TEXT("open"), S.bOpen ? TEXT("true") : TEXT("false"), S.bOpen);
		Add(TEXT("BuildMenu.Categories"), TEXT("BuildMenu.Root"), TEXT("Build categories"), EHansaHudSemanticRole::Panel);
		for (const EHansaBuildCategory Category : Categories) Add(CategorySemanticId(Category), TEXT("BuildMenu.Categories"), ::LexToString(Category), EHansaHudSemanticRole::Button, true, true, TEXT("build-category"), ::LexToString(Category), S.SelectedCategory == Category);
		Add(TEXT("BuildMenu.Cards"), TEXT("BuildMenu.Root"), TEXT("Building cards"), EHansaHudSemanticRole::Panel);
		for (const FHansaBuildCardPresentation& Card : S.Cards) if (Card.Category == S.SelectedCategory)
		{
			Add(CardSemanticId(Card.StableId), TEXT("BuildMenu.Cards"), Card.Name.ToString(), EHansaHudSemanticRole::Button, true, true,
				TEXT("building-card"), FString::Printf(TEXT("id=%s;cost=%s;workforce=%s;footprint=%s;flow=%s;lockedReason=%s"),
					*Card.StableId.ToString(), *Card.Cost.ToString(), *Card.WorkforceAndUpkeep.ToString(), *Card.Footprint.ToString(), *Card.InputOutput.ToString(), *Card.LockedReason.ToString()),
				S.SelectedBuildingId == Card.StableId, !Card.bLocked, false, Card.bLocked);
		}
		Add(TEXT("BuildMenu.Recent"), TEXT("BuildMenu.Root"), TEXT("Recently used"), EHansaHudSemanticRole::Panel, false, false, TEXT("building-definition-id"), S.SelectedBuildingId.ToString());
		int32 FavoriteCount = 0;
		for (const FHansaBuildCardPresentation& Card : S.Cards) FavoriteCount += Card.bFavorite ? 1 : 0;
		Add(TEXT("BuildMenu.Favorites"), TEXT("BuildMenu.Root"), TEXT("Favorites"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(FavoriteCount));
		Add(TEXT("BuildMenu.CardAction.Favorite"), TEXT("BuildMenu.Root"), TEXT("Toggle favorite"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), TEXT(""), false, !S.SelectedBuildingId.IsNone());
		Add(TEXT("BuildMenu.CardAction.Compare"), TEXT("BuildMenu.Root"), TEXT("Compare building"), EHansaHudSemanticRole::Button, true, true, FString(), FString(), false, !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Root"), TEXT("HUD.Root"), TEXT("Placement"), EHansaHudSemanticRole::Panel, false, false, TEXT("building-definition-id"), S.SelectedBuildingId.ToString(), !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Preview"), TEXT("Placement.Root"), TEXT("Placement preview"), EHansaHudSemanticRole::Status, false, false, TEXT("placement-preview"), S.PlacementSummary.ToString(), S.bHasTarget);
		Add(TEXT("Placement.Footprint"), TEXT("Placement.Preview"), TEXT("Footprint"), EHansaHudSemanticRole::Status, false, false, TEXT("placement-feedback"), S.Feedback == EHansaPlacementFeedback::Invalid ? TEXT("striped-error") : TEXT("outlined-valid"), S.bHasTarget, true, S.Feedback == EHansaPlacementFeedback::Invalid);
		Add(TEXT("Placement.Validation"), TEXT("Placement.Root"), TEXT("Placement validation"), EHansaHudSemanticRole::Alert, false, false, TEXT("placement-feedback"), UEnum::GetValueAsString(S.Feedback), S.bCanConfirm, true, S.Feedback == EHansaPlacementFeedback::Invalid);
		Add(TEXT("Placement.Validation.Cause"), TEXT("Placement.Validation"), S.ValidationCause.ToString(), EHansaHudSemanticRole::Text, false, false, TEXT("text"), S.ValidationCause.ToString(), false, true, S.Feedback == EHansaPlacementFeedback::Invalid);
		Add(TEXT("Placement.Validation.Remedy"), TEXT("Placement.Validation"), S.ValidationRemedy.ToString(), EHansaHudSemanticRole::Text, false, false, TEXT("text"), S.ValidationRemedy.ToString());
		Add(TEXT("Placement.Target.Road"), TEXT("Placement.Root"), TEXT("18,16"), EHansaHudSemanticRole::Button, true, true, TEXT("grid-coordinate"), TEXT("18,16"), false, !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Target.Disconnected"), TEXT("Placement.Root"), TEXT("10,10"), EHansaHudSemanticRole::Button, true, true, TEXT("grid-coordinate"), TEXT("10,10"), false, !S.SelectedBuildingId.IsNone());
		const FIntPoint AdjacentTarget = Pinned->GetRoadAdjacentTarget();
		const FString AdjacentValue = FString::Printf(TEXT("%d,%d"), AdjacentTarget.X, AdjacentTarget.Y);
		Add(TEXT("Placement.Target.Adjacent"), TEXT("Placement.Root"), TEXT("Fit beside road"), EHansaHudSemanticRole::Button, true, true, TEXT("grid-coordinate"), AdjacentValue, false, !S.SelectedBuildingId.IsNone());
		const TOptional<FIntPoint> ShoreTarget = Pinned->FindValidShorelineTarget();
		const FString ShoreValue = ShoreTarget.IsSet() ? FString::Printf(TEXT("%d,%d"), ShoreTarget->X, ShoreTarget->Y) : FString();
		Add(TEXT("Placement.Target.Shore"), TEXT("Placement.Root"), TEXT("Find shoreline"), EHansaHudSemanticRole::Button, true, true, TEXT("grid-coordinate"), ShoreValue, false, ShoreTarget.IsSet());
		Add(TEXT("Placement.Overlay.Grid"), TEXT("Placement.Root"), TEXT("Grid overlay"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), S.bGridOverlay ? TEXT("true") : TEXT("false"), S.bGridOverlay);
		Add(TEXT("Placement.Overlay.Road"), TEXT("Placement.Root"), TEXT("Road overlay"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), S.bRoadOverlay ? TEXT("true") : TEXT("false"), S.bRoadOverlay);
		Add(TEXT("Placement.Action.Rotate"), TEXT("Placement.Root"), TEXT("Rotate"), EHansaHudSemanticRole::Button, true, true, TEXT("degrees"), FString::FromInt(S.RotationQuarterTurns * 90), false, !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Action.Repeat"), TEXT("Placement.Root"), TEXT("Repeat"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), S.bRepeat ? TEXT("true") : TEXT("false"), S.bRepeat, !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Action.Confirm"), TEXT("Placement.Root"), TEXT("Confirm"), EHansaHudSemanticRole::Button, true, true, FString(), FString(), false, S.bCanConfirm);
		Add(TEXT("Placement.Action.Cancel"), TEXT("Placement.Root"), TEXT("Cancel"), EHansaHudSemanticRole::Button, true, true, FString(), FString(), false, !S.SelectedBuildingId.IsNone());
		return Nodes;
	}
}

#undef LOCTEXT_NAMESPACE
