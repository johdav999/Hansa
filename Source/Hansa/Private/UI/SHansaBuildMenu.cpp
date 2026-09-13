#include "UI/SHansaBuildMenu.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SViewport.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiComponents.h"
#include "World/HansaStrategyPlayerController.h"
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
		FString CategorySemanticId(const EHansaBuildCategory Category)
		{
			return FString::Printf(TEXT("BuildMenu.Category.%s"), ::LexToString(Category));
		}

		FText CategoryLabel(EHansaBuildCategory Category)
		{
		FText Label;
			switch(Category)
			{
			case EHansaBuildCategory::Roads: Label=LOCTEXT("CategoryRoads","Roads");break;
			case EHansaBuildCategory::Residences: Label=LOCTEXT("CategoryHomes","Homes");break;
			case EHansaBuildCategory::Production: Label=LOCTEXT("CategoryProduction","Production");break;
			case EHansaBuildCategory::Storage: Label=LOCTEXT("CategoryStorage","Storage");break;
			case EHansaBuildCategory::Harbor: Label=LOCTEXT("CategoryHarbor","Harbor");break;
			case EHansaBuildCategory::Civic: Label=LOCTEXT("CategoryCivic","Civic");break;
			default: Label=LOCTEXT("CategoryDecoration","Decoration");break;
			}
		return Label;
		}

		class SHansaBuildCardButton final : public SHansaAction
		{
		public:
			SLATE_BEGIN_ARGS(SHansaBuildCardButton) : _Model(nullptr), _Controller(nullptr) {}
				SLATE_ARGUMENT(FHansaBuildCardPresentation, Card)
				SLATE_ARGUMENT(FUiPreferences, Preferences)
				SLATE_ARGUMENT(UHansaBuildMenuPresentationModel*, Model)
				SLATE_ARGUMENT(AHansaStrategyPlayerController*, Controller)
				SLATE_ARGUMENT(TWeakPtr<SHansaBuildMenu>, Menu)
			SLATE_END_ARGS()

			void Construct(const FArguments& Args)
			{
				BuildingId=Args._Card.StableId; Model=Args._Model; PlacementController=Args._Controller; Menu=Args._Menu;

                const EUiGlyph StageGlyph=BuildingId==TEXT("Building.GrainFarm")?EUiGlyph::Farm:
                    BuildingId==TEXT("Building.Mill")?EUiGlyph::Mill:BuildingId==TEXT("Building.Bakery")?EUiGlyph::Bakery:
                    BuildingId==TEXT("Building.Fishery")?EUiGlyph::Fish:
                    BuildingId==TEXT("Building.LumberCamp")?EUiGlyph::LumberCamp:
                    BuildingId==TEXT("Building.HopFarm")?EUiGlyph::HopFarm:
                    BuildingId==TEXT("Building.MaltHouse")?EUiGlyph::MaltHouse:
                    BuildingId==TEXT("Building.Cooperage")?EUiGlyph::Cooperage:
                    BuildingId==TEXT("Building.Brewery")?EUiGlyph::Brewery:
                    BuildingId==TEXT("Building.Road")?EUiGlyph::Road:
                    BuildingId==TEXT("Building.Warehouse")?EUiGlyph::Warehouse:
                    BuildingId==TEXT("Building.Dock")?EUiGlyph::Dock:BuildingId==TEXT("Building.Market")?EUiGlyph::Market:EUiGlyph::Building;
                SHansaAction::Construct(SHansaAction::FArguments().Kind(EHansaUiButtonStyle::Primary).Preferences(Args._Preferences)
                    .OnClicked_Lambda([this] {
                        auto* Pinned=Model.Get();
                        if (!Pinned || !Pinned->SelectBuilding(BuildingId))return FReply::Unhandled();
                        FReply Reply=FReply::Handled();
                        // A selected construction tool belongs to the scene. Leaving
                        // focus on the menu can swallow the first world input action.
                        if (const auto* Controller=PlacementController.Get())
                            if (const auto* Player=Controller->GetLocalPlayer();Player && Player->ViewportClient)
                                if (auto Viewport=Player->ViewportClient->GetGameViewportWidget())
                                    Reply.SetUserFocus(Viewport.ToSharedRef(),EFocusCause::SetDirectly);
                        return Reply;
                    })
                    [SNew(SBox).WidthOverride(48).HeightOverride(48)
                    [SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SHansaGlyph).Glyph(StageGlyph).OnDark(true).Size(48)]
                    +SVerticalBox::Slot().AutoHeight().Padding(0)[SAssignNew(NameText,STextBlock).Visibility(EVisibility::Collapsed).Justification(ETextJustify::Center)
                        .Font(GetComponentFont(EHansaUiTypographyToken::Body,Args._Preferences)).AutoWrapText(true)]]]);
				Update(Args._Card,FHansaBuildMenuSnapshot());
                IconButtonStyle=UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
                IconButtonStyle.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
                SetButtonStyle(&IconButtonStyle);
			}

			void Update(const FHansaBuildCardPresentation& Card,const FHansaBuildMenuSnapshot& Snapshot)
			{
				BuildingName=Card.Name; NameText->SetText(BuildingId==TEXT("Building.GrainFarm")?LOCTEXT("FarmStage","Farm"):Card.Name);
				FString Input,Output;const FString Flow=Card.InputOutput.ToString();
				const FText NativeFlow=Flow.Split(TEXT(" → "),&Input,&Output)?FText::Format(LOCTEXT("CardFlow","{0} produces {1}"),FText::FromString(Input),FText::FromString(Output)):Card.InputOutput;
				const FText Data=FText::Format(LOCTEXT("CardData","Cost: {0}\nFootprint: {1}\n{2}\n{3}"),Card.Cost,Card.Footprint,Card.WorkforceAndUpkeep,NativeFlow);
				const bool Selected=Snapshot.SelectedBuildingId==Card.StableId;
				const FText Reason=Card.bLocked?Card.LockedReason:!Card.bAvailable?Card.AvailabilityReason:
					Selected&&Snapshot.bDraggingCard?LOCTEXT("CardDragging","Dragging — release on a valid site"):
					Selected&&Snapshot.bHasTarget?Snapshot.ValidationCause:LOCTEXT("CardReady","Select, then click a site. Hold and move to build a line.");
				
				const EUiState CardState=Card.bLocked||!Card.bAvailable?EUiState::Disabled:
					Selected&&Snapshot.Feedback==EHansaPlacementFeedback::Invalid?EUiState::Error:
					Selected&&Snapshot.Feedback==EHansaPlacementFeedback::Warning?EUiState::Warning:
					Selected?EUiState::Selected:EUiState::Default;
				SetState(CardState,FText::Format(LOCTEXT("BuildCardTooltip","{0}\n{1}\n{2}"),Card.Name,Data,Reason));
			}

		private:
			FButtonStyle IconButtonStyle;
            FName BuildingId;
			FText BuildingName;
			TWeakObjectPtr<UHansaBuildMenuPresentationModel> Model;
			TWeakObjectPtr<AHansaStrategyPlayerController> PlacementController;
			TWeakPtr<SHansaBuildMenu> Menu;
			TSharedPtr<STextBlock> NameText,DataText,ReasonText;
		};

	}

	SHansaBuildMenu::~SHansaBuildMenu()
	{
		if (UHansaBuildMenuPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaBuildMenu::SetPreferences(FUiPreferences InPreferences)
	{
		const FName Focus=Model.IsValid()?Model->GetSnapshot().FocusedSemanticId:NAME_None;
		if(Model.IsValid()) Model->OnChanged().Remove(ChangedHandle);
		CachedCategories.Reset();CachedChains.Reset();CachedCardIds.Reset();ConnectorIds.Reset();
		CategoryButtons.Reset();ChainButtons.Reset();CardButtons.Reset();SemanticWidgets.Reset();
		Construct(FArguments().Model(Model.Get()).PlacementController(PlacementController.Get()).Preferences(InPreferences));
		if(!Focus.IsNone()) FocusSemanticId(Focus.ToString());
	}

	void SHansaBuildMenu::Construct(const FArguments& Arguments)
	{
		Model=Arguments._Model; PlacementController=Arguments._PlacementController; Preferences=Arguments._Preferences;
		PanelBrush=GetComponentStyle(EUiSurface::BottomTray,EUiState::Default,Preferences).Brush;
		WorkingBrush=UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		DecisionBrush=UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		DarkBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,true);
		DarkBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
		DarkCaptionStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption,true);
		LightBodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,false);
		LightHeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2,false);
		
		ChildSlot[SAssignNew(RootWidget,SVerticalBox)
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[SAssignNew(ExpansionWidget,SVerticalBox)
			+SVerticalBox::Slot().AutoHeight().Padding(0,4)
			[SNew(SBox).MaxDesiredHeight(320)
				[SAssignNew(CardScroll,SScrollBox)+SScrollBox::Slot()
					[SAssignNew(CardsWidget,SBorder).BorderImage(&PanelBrush).Padding(8)[SAssignNew(CardsGrid,SGridPanel)]]]]
			+SVerticalBox::Slot().AutoHeight()[SAssignNew(ValidationWidget,SBorder).BorderImage(&PanelBrush).Padding(8)
				[SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SAssignNew(ValidationIcon,SHansaGlyph).OnDark(true)]
				+SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight()[SAssignNew(ValidationCauseText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]
					+SVerticalBox::Slot().AutoHeight()[SAssignNew(ValidationRemedyText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]]]]
			]
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(ChainsGrid,SUniformGridPanel).SlotPadding(FMargin(4))]
		+SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(CategoriesWidget,SBorder).BorderImage(&PanelBrush).Padding(8)[SAssignNew(CategoryRow,SHorizontalBox)]]];

		PreviewWidget=ValidationWidget;
		MapWidget(TEXT("BuildMenu.Root"),RootWidget); MapWidget(TEXT("BuildMenu.Categories"),CategoriesWidget);
		MapWidget(TEXT("BuildMenu.Chains"),ChainsGrid); MapWidget(TEXT("BuildMenu.Cards"),CardsWidget);
		MapWidget(TEXT("Placement.Root"),RootWidget);
		MapWidget(TEXT("Placement.Validation"),ValidationWidget); MapWidget(TEXT("Placement.Validation.Cause"),ValidationCauseText);
		MapWidget(TEXT("Placement.Validation.Remedy"),ValidationRemedyText); MapWidget(TEXT("Placement.Preview"),PreviewWidget);
		MapWidget(TEXT("Placement.Footprint"),PreviewWidget);
		if(auto* Pinned=Model.Get())
		{
			ChangedHandle=Pinned->OnChanged().AddSP(SharedThis(this),&SHansaBuildMenu::Refresh);
			Refresh(Pinned->GetSnapshot(),Pinned->GetRevision());
		}
		else RootWidget->SetVisibility(EVisibility::Collapsed);
	}

	FString SHansaBuildMenu::CardSemanticId(const FName BuildingId) const
	{
		FString Suffix = BuildingId.ToString(); Suffix.ReplaceInline(TEXT("."), TEXT("_"));
		return FString::Printf(TEXT("BuildMenu.Card.%s"), *Suffix);
	}

	FString SHansaBuildMenu::ChainSemanticId(const FName OutputGoodId) const
	{
		FString Suffix = OutputGoodId.ToString(); Suffix.ReplaceInline(TEXT("."), TEXT("_"));
		return FString::Printf(TEXT("BuildMenu.Chain.%s"), *Suffix);
	}

	void SHansaBuildMenu::RebuildCategories(const FHansaBuildMenuSnapshot& Snapshot)
	{
		if(CachedCategories==Snapshot.Categories && CategoryButtons.Num()>0) return;
		for(const auto& Pair:CategoryButtons) SemanticWidgets.Remove(CategorySemanticId(Pair.Key));
		CategoryRow->ClearChildren();CategoryButtons.Reset();CachedCategories=Snapshot.Categories;
		for(const auto Category:Snapshot.Categories)
		{
			const FText Label=CategoryLabel(Category);
			TSharedPtr<SHansaAction> Button;
			const EUiGlyph Glyph=Category==EHansaBuildCategory::Roads?EUiGlyph::Road:Category==EHansaBuildCategory::Residences?EUiGlyph::Building:
                Category==EHansaBuildCategory::Production?EUiGlyph::Production:Category==EHansaBuildCategory::Storage?EUiGlyph::Storage:
                Category==EHansaBuildCategory::Harbor?EUiGlyph::Harbor:Category==EHansaBuildCategory::Civic?EUiGlyph::Civic:EUiGlyph::Decoration;
            CategoryRow->AddSlot().AutoWidth().Padding(4)[SAssignNew(Button,SHansaAction).Compact(true).Preferences(Preferences)
                .Reason(Label).OnClicked(this,&SHansaBuildMenu::SelectCategory,Category)
                [SNew(SHansaGlyph).Glyph(Glyph).OnDark(true).Size(32)]];
			CategoryButtons.Add(Category,Button);MapWidget(CategorySemanticId(Category),Button);
		}
	}

	void SHansaBuildMenu::RebuildChains(const FHansaBuildMenuSnapshot& Snapshot)
	{
		if(CachedChains==Snapshot.ProductionChains && ChainButtons.Num()>0) return;
		for(const auto& Pair:ChainButtons) SemanticWidgets.Remove(Pair.Key);
		ChainsGrid->ClearChildren();ChainButtons.Reset();CachedChains=Snapshot.ProductionChains;
		for(int32 Index=0;Index<Snapshot.ProductionChains.Num();++Index)
		{
			const auto& Chain=Snapshot.ProductionChains[Index];const FString Id=ChainSemanticId(Chain.OutputGoodId);
			const EUiGlyph Glyph=GlyphForGood(Chain.OutputGoodId);
			TSharedPtr<SHansaAction> Button;
            ChainsGrid->AddSlot(Index,0)[SAssignNew(Button,SHansaAction).Compact(true).Preferences(Preferences)
                .OnClicked(this,&SHansaBuildMenu::SelectChain,Chain.OutputGoodId)
                [SNew(SBox).MinDesiredWidth(64)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SHansaGlyph).Glyph(Glyph).OnDark(true).Size(40)]
                +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[SNew(STextBlock).Text(Chain.Name).Justification(ETextJustify::Center)
                    .Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)).AutoWrapText(true)]]]];
			ChainButtons.Add(Id,Button);MapWidget(Id,Button);
		}
	}

	void SHansaBuildMenu::RebuildCards(const FHansaBuildMenuSnapshot& Snapshot)
	{
		TArray<FName> VisibleIds;
		for(const auto& Card:Snapshot.Cards)
			if(Card.Category==Snapshot.SelectedCategory && (Card.Category!=EHansaBuildCategory::Production ||
				Card.ProductionChainOutputGoodId==Snapshot.SelectedProductionChainOutputGoodId)) VisibleIds.Add(Card.StableId);
		if(VisibleIds!=CachedCardIds)
		{
			for(const auto& Pair:CardButtons) SemanticWidgets.Remove(Pair.Key);
			for(const auto& Id:ConnectorIds) SemanticWidgets.Remove(Id);
			CardsGrid->ClearChildren();CardButtons.Reset();ConnectorIds.Reset();CachedCardIds=VisibleIds;
			for(int32 Index=0;Index<VisibleIds.Num();++Index)
			{
				const auto& Card=*Snapshot.Cards.FindByPredicate([&](const auto& Value){return Value.StableId==VisibleIds[Index];});
				const FString Id=CardSemanticId(Card.StableId);
				TSharedPtr<SHansaBuildCardButton> Button;
				CardsGrid->SetColumnFill(Index*2,1.f);
				CardsGrid->AddSlot(Index*2,0).Padding(4).HAlign(HAlign_Center).VAlign(VAlign_Fill)
					[SAssignNew(Button,SHansaBuildCardButton).Card(Card).Preferences(Preferences)
						.Model(Model.Get()).Controller(PlacementController.Get()).Menu(SharedThis(this))];
				CardButtons.Add(Id,Button);MapWidget(Id,Button);
				if(Index+1<VisibleIds.Num() && Snapshot.SelectedCategory==EHansaBuildCategory::Production)
				{
					TSharedPtr<SWidget> Connector;
					CardsGrid->AddSlot(Index*2+1,0).VAlign(VAlign_Center)[SAssignNew(Connector,SBox).WidthOverride(32).HeightOverride(32)
						[SNew(SHansaGlyph).Glyph(EUiGlyph::Arrow).OnDark(true)]];
					const FString Link=FString::Printf(TEXT("BuildMenu.Connector.%d"),Index);
					ConnectorIds.Add(Link);MapWidget(Link,Connector);
				}
			}
			CardScroll->SetScrollOffset(0);
		}
		for(const auto& Card:Snapshot.Cards)
			if(const auto* Found=CardButtons.Find(CardSemanticId(Card.StableId)))
				StaticCastSharedPtr<SHansaBuildCardButton>(*Found)->Update(Card,Snapshot);
	}

	void SHansaBuildMenu::Refresh(const FHansaBuildMenuSnapshot& Snapshot,const uint64 Revision)
	{
		(void)Revision;
		RootWidget->SetVisibility(EVisibility::SelfHitTestInvisible);
		const bool Expanded=Snapshot.bOpen && (Snapshot.SelectedCategory!=EHansaBuildCategory::Production || !Snapshot.SelectedProductionChainOutputGoodId.IsNone());
		ExpansionWidget->SetVisibility(Expanded?EVisibility::Visible:EVisibility::Collapsed);
		ChainsGrid->SetVisibility(Snapshot.bOpen && Snapshot.SelectedCategory==EHansaBuildCategory::Production?EVisibility::Visible:EVisibility::Collapsed);
		RebuildCategories(Snapshot);RebuildChains(Snapshot);RebuildCards(Snapshot);
		FocusOrder.Reset();
		for(const auto Category:Snapshot.Categories)
		{
			FocusOrder.Add(CategorySemanticId(Category));
			StaticCastSharedPtr<SHansaAction>(CategoryButtons[Category])->SetState(Snapshot.bOpen && Category==Snapshot.SelectedCategory?EUiState::Selected:EUiState::Default,
                CategoryLabel(Category));
		}
		for(const auto& Chain:Snapshot.ProductionChains)
		{
			const FString Id=ChainSemanticId(Chain.OutputGoodId);
			StaticCastSharedPtr<SHansaAction>(ChainButtons[Id])->SetState(Chain.OutputGoodId==Snapshot.SelectedProductionChainOutputGoodId?EUiState::Selected:EUiState::Default,Chain.Name);
			if(Snapshot.bOpen && Snapshot.SelectedCategory==EHansaBuildCategory::Production) FocusOrder.Add(Id);
		}
		if(Snapshot.bOpen) for(const auto& Id:CachedCardIds)
		{
			const FString SemanticId=CardSemanticId(Id);
			if(CardButtons[SemanticId]->IsEnabled()) FocusOrder.Add(SemanticId);
		}
		const bool Selected=!Snapshot.SelectedBuildingId.IsNone();
		ValidationWidget->SetVisibility(Selected && Snapshot.bHasTarget?EVisibility::Visible:EVisibility::Collapsed);
		ValidationCauseText->SetText(Snapshot.ValidationCause);ValidationRemedyText->SetText(Snapshot.ValidationRemedy);
		ValidationIcon->SetGlyph(Snapshot.Feedback==EHansaPlacementFeedback::Invalid?EUiGlyph::Error:
			Snapshot.Feedback==EHansaPlacementFeedback::Warning?EUiGlyph::Warning:EUiGlyph::Check);

	}

	FReply SHansaBuildMenu::SelectCategory(const EHansaBuildCategory Category)
	{
		return Invoke([this, Category] { return Model->SelectCategory(Category); });
	}

	FReply SHansaBuildMenu::SelectChain(const FName OutputGoodId)
	{
		return Invoke([this, OutputGoodId] { return Model->SelectProductionChain(OutputGoodId); });
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
			for (const EHansaBuildCategory Category : Pinned->GetSnapshot().Categories)
				if (SemanticId == CategorySemanticId(Category)) return Pinned->SelectCategory(Category);
			const FHansaBuildMenuSnapshot& Snapshot = Pinned->GetSnapshot();
			if (!Snapshot.bOpen) return false;
			if (Snapshot.SelectedCategory == EHansaBuildCategory::Production)
			{
				for (const FHansaBuildChainPresentation& Chain : Snapshot.ProductionChains)
					if (SemanticId == ChainSemanticId(Chain.OutputGoodId)) return Pinned->SelectProductionChain(Chain.OutputGoodId);
			}
			for (const FHansaBuildCardPresentation& Card : Snapshot.Cards)
			{
				const bool bVisible = Card.Category == Snapshot.SelectedCategory &&
					(Card.Category != EHansaBuildCategory::Production ||
					 Card.ProductionChainOutputGoodId == Snapshot.SelectedProductionChainOutputGoodId);
				if (bVisible && SemanticId == CardSemanticId(Card.StableId)) return Pinned->SelectBuilding(Card.StableId);
			}
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
		if (!Widget.IsValid() || !Widget->IsEnabled() || !FocusOrder.Contains(SemanticId)) return false;
		if (UHansaBuildMenuPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
		return true;
	}

	FReply SHansaBuildMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		if (!Model.IsValid()) return FReply::Unhandled();
		const FKey Key = InKeyEvent.GetKey();
		if(Key==EKeys::Tab || Key==EKeys::Gamepad_DPad_Right || Key==EKeys::Gamepad_DPad_Down || Key==EKeys::Gamepad_DPad_Left || Key==EKeys::Gamepad_DPad_Up)
		{
			const bool Back=InKeyEvent.IsShiftDown() || Key==EKeys::Gamepad_DPad_Left || Key==EKeys::Gamepad_DPad_Up;
			int32 Index=FocusOrder.IndexOfByKey(Model->GetSnapshot().FocusedSemanticId.ToString());
			for(int32 Try=0;Try<FocusOrder.Num();++Try)
			{
				Index=(Index+(Back?-1:1)+FocusOrder.Num())%FocusOrder.Num();
				if(FocusSemanticId(FocusOrder[Index])) return FReply::Handled();
			}
		}
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
		if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
		{
			if(Model->GetSnapshot().SelectedBuildingId.IsNone()) { Model->SetOpen(false); return FReply::Handled(); }
			return Invoke([this] { return Model->CancelIntent(); });
		}
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
			N.bCanActivate = bActivate; N.bCanFocus = bFocus; N.State.bVisible = S.bOpen || Id == TEXT("BuildMenu.Root") || Id == TEXT("BuildMenu.Categories") || Id.StartsWith(TEXT("BuildMenu.Category.")); N.State.bEnabled = bEnabled;
			if(Id==TEXT("BuildMenu.Cards")) N.State.bVisible &= S.SelectedCategory!=EHansaBuildCategory::Production || !S.SelectedProductionChainOutputGoodId.IsNone();
			if(Id.StartsWith(TEXT("Placement.")) || Id.StartsWith(TEXT("BuildMenu.CardAction."))) N.State.bVisible &= !S.SelectedBuildingId.IsNone();
			if(Id.StartsWith(TEXT("Placement.Validation")) || Id==TEXT("Placement.Preview")) N.State.bVisible &= S.bHasTarget;
			if(Id.StartsWith(TEXT("Placement.Action.")) || Id.StartsWith(TEXT("Placement.Overlay.")) || Id.StartsWith(TEXT("BuildMenu.CardAction.")))
            {
                N.State.bVisible=false; N.bCanActivate=false; N.bCanFocus=false;
            }
            if(Id==TEXT("BuildMenu.Recent") || Id==TEXT("BuildMenu.Favorites")) N.State.bVisible=false;
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
		for (const EHansaBuildCategory Category : S.Categories) Add(CategorySemanticId(Category), TEXT("BuildMenu.Categories"), CategoryLabel(Category).ToString(), EHansaHudSemanticRole::Button, true, true, TEXT("build-category"), ::LexToString(Category), S.SelectedCategory == Category);
		Add(TEXT("BuildMenu.Cards"), TEXT("BuildMenu.Root"), TEXT("Building cards"), EHansaHudSemanticRole::Panel);
		if (S.bOpen && S.SelectedCategory == EHansaBuildCategory::Production)
		{
			Add(TEXT("BuildMenu.Chains"), TEXT("BuildMenu.Root"), TEXT("Production chains"), EHansaHudSemanticRole::Panel);
			for (const FHansaBuildChainPresentation& Chain : S.ProductionChains)
			{
				Add(ChainSemanticId(Chain.OutputGoodId), TEXT("BuildMenu.Chains"), Chain.Name.ToString(), EHansaHudSemanticRole::Button,
					true, true, TEXT("production-chain-output"), Chain.OutputGoodId.ToString(),
					S.SelectedProductionChainOutputGoodId == Chain.OutputGoodId);
			}
		}
		for (const FHansaBuildCardPresentation& Card : S.Cards) if (S.bOpen &&
			Card.Category == S.SelectedCategory &&
			(Card.Category != EHansaBuildCategory::Production ||
			 Card.ProductionChainOutputGoodId == S.SelectedProductionChainOutputGoodId))
		{
			Add(CardSemanticId(Card.StableId), TEXT("BuildMenu.Cards"), Card.Name.ToString(), EHansaHudSemanticRole::Button, true, true,
				TEXT("building-card"), FString::Printf(TEXT("id=%s;cost=%s;workforce=%s;footprint=%s;flow=%s;lockedReason=%s"),
					*Card.StableId.ToString(), *Card.Cost.ToString(), *Card.WorkforceAndUpkeep.ToString(), *Card.Footprint.ToString(), *Card.InputOutput.ToString(),
					*FString::Printf(TEXT("%s%s%s"), *Card.LockedReason.ToString(), Card.AvailabilityReason.IsEmpty() ? TEXT("") : TEXT("; availability="), *Card.AvailabilityReason.ToString())),
				S.SelectedBuildingId == Card.StableId, !Card.bLocked && Card.bAvailable, false, Card.bLocked || !Card.bAvailable);
		}
		if(S.bOpen) for(int32 Index=0;Index<ConnectorIds.Num();++Index)
			Add(ConnectorIds[Index],TEXT("BuildMenu.Cards"),TEXT("Production flows to next stage"),EHansaHudSemanticRole::Status,false,false,TEXT("chain-edge"),FString::FromInt(Index));
		Add(TEXT("BuildMenu.Recent"), TEXT("BuildMenu.Root"), TEXT("Recently used"), EHansaHudSemanticRole::Panel, false, false, TEXT("building-definition-id"), S.SelectedBuildingId.ToString());
		int32 FavoriteCount = 0;
		for (const FHansaBuildCardPresentation& Card : S.Cards) FavoriteCount += Card.bFavorite ? 1 : 0;
		Add(TEXT("BuildMenu.Favorites"), TEXT("BuildMenu.Root"), TEXT("Favorites"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(FavoriteCount));
		Add(TEXT("BuildMenu.CardAction.Favorite"), TEXT("BuildMenu.Root"), TEXT("Toggle favorite"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), TEXT(""), false, !S.SelectedBuildingId.IsNone());
		Add(TEXT("BuildMenu.CardAction.Compare"), TEXT("BuildMenu.Root"), TEXT("Compare building"), EHansaHudSemanticRole::Button, true, true, FString(), FString(), false, !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Root"), TEXT("HUD.Root"), TEXT("Placement"), EHansaHudSemanticRole::Panel, false, false, TEXT("building-definition-id"), S.SelectedBuildingId.ToString(), !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Preview"), TEXT("Placement.Root"), TEXT("Placement preview"), EHansaHudSemanticRole::Status, false, false, TEXT("placement-preview"), S.PlacementSummary.ToString(), S.bHasTarget);
		const FString FootprintPattern = S.Feedback == EHansaPlacementFeedback::Invalid ? TEXT("raised-stripes+x")
			: S.Feedback == EHansaPlacementFeedback::Warning ? TEXT("alternating-cells+warning") : TEXT("solid-cells+outline+check");
		Add(TEXT("Placement.Footprint"), TEXT("Placement.Preview"), TEXT("Footprint"), EHansaHudSemanticRole::Status, false, false, TEXT("placement-feedback"), FootprintPattern, S.bHasTarget, true, S.Feedback == EHansaPlacementFeedback::Invalid, S.Feedback == EHansaPlacementFeedback::Warning);
		Add(TEXT("Placement.Viewport"), TEXT("Placement.Root"), TEXT("World viewport placement target"), EHansaHudSemanticRole::Status, false, false,
			TEXT("viewport-placement"), FString::Printf(TEXT("dragging=%s;roadDrawing=%s;overWorld=%s;anchor=%d,%d;cells=%d;rotation=%d;buildingStroke=%s"),
				S.bDraggingCard ? TEXT("true") : TEXT("false"), S.bRoadDrawing ? TEXT("true") : TEXT("false"),
				S.bPointerOverWorld ? TEXT("true") : TEXT("false"),
				S.AnchorCell.X, S.AnchorCell.Y, S.FootprintCells.Num(), S.RotationQuarterTurns * 90, Pinned->IsBuildingStrokeActive()?TEXT("true"):TEXT("false")), S.bHasTarget);
		TArray<FString> RoadCells;
		for (const FHansaRoadPreviewCell& Cell : S.RoadPreviewCells)
		{
			RoadCells.Add(FString::Printf(TEXT("%d,%d:%s%s"), Cell.Cell.X, Cell.Cell.Y,
				Cell.State == EHansaRoadPreviewCellState::NewValid ? TEXT("new")
					: Cell.State == EHansaRoadPreviewCellState::ExistingRoad ? TEXT("existing") : TEXT("invalid"),
				Cell.Failure.IsNone() ? TEXT("") : *FString::Printf(TEXT("(%s)"), *Cell.Failure.ToString())));
		}
		Add(TEXT("Placement.RoadPath"), TEXT("Placement.Preview"), TEXT("Connected road path"), EHansaHudSemanticRole::Status,
			false, false, TEXT("road-path"), FString::Printf(
				TEXT("active=%s;start=%d,%d;end=%d,%d;tieBreak=horizontal-first;new=%d;existing=%d;invalid=%d;cost=%s;cells=%s"),
				S.bRoadDrawing ? TEXT("true") : TEXT("false"), S.RoadStartCell.X, S.RoadStartCell.Y,
				S.RoadEndCell.X, S.RoadEndCell.Y, S.RoadNewCellCount, S.RoadExistingCellCount,
				S.RoadInvalidCellCount, *S.RoadTotalCost.ToString(), *FString::Join(RoadCells, TEXT("|"))),
			S.bRoadDrawing, true, S.RoadInvalidCellCount > 0,
			S.bRoadDrawing && S.RoadExistingCellCount > 0);
		Add(TEXT("Placement.Validation"), TEXT("Placement.Root"), TEXT("Placement validation"), EHansaHudSemanticRole::Alert, false, false, TEXT("placement-feedback"), UEnum::GetValueAsString(S.Feedback), S.bCanConfirm, true, S.Feedback == EHansaPlacementFeedback::Invalid, S.Feedback == EHansaPlacementFeedback::Warning);
		Add(TEXT("Placement.Validation.Cause"), TEXT("Placement.Validation"), S.ValidationCause.ToString(), EHansaHudSemanticRole::Text, false, false, TEXT("text"), S.ValidationCause.ToString(), false, true, S.Feedback == EHansaPlacementFeedback::Invalid, S.Feedback == EHansaPlacementFeedback::Warning);
		Add(TEXT("Placement.Validation.Remedy"), TEXT("Placement.Validation"), S.ValidationRemedy.ToString(), EHansaHudSemanticRole::Text, false, false, TEXT("text"), S.ValidationRemedy.ToString());
		Add(TEXT("Placement.Overlay.Grid"), TEXT("Placement.Root"), TEXT("Grid overlay"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), S.bGridOverlay ? TEXT("true") : TEXT("false"), S.bGridOverlay);
		Add(TEXT("Placement.Overlay.Road"), TEXT("Placement.Root"), TEXT("Road overlay"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), S.bRoadOverlay ? TEXT("true") : TEXT("false"), S.bRoadOverlay);
		Add(TEXT("Placement.Action.Rotate"), TEXT("Placement.Root"), TEXT("Rotate"), EHansaHudSemanticRole::Button, true, true, TEXT("degrees"), FString::FromInt(S.RotationQuarterTurns * 90), false, !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Action.Repeat"), TEXT("Placement.Root"), TEXT("Repeat"), EHansaHudSemanticRole::Button, true, true, TEXT("boolean"), S.bRepeat ? TEXT("true") : TEXT("false"), S.bRepeat, !S.SelectedBuildingId.IsNone());
		Add(TEXT("Placement.Action.Confirm"), TEXT("Placement.Root"), TEXT("Confirm"), EHansaHudSemanticRole::Button, true, true, FString(), FString(), false, S.bCanConfirm);
		Add(TEXT("Placement.Action.Cancel"), TEXT("Placement.Root"), TEXT("Cancel"), EHansaHudSemanticRole::Button, true, true, FString(), FString(), false, !S.SelectedBuildingId.IsNone());
		return Nodes;
	}

	bool SHansaBuildMenu::IsScreenPositionOverMenu(const FVector2D AbsoluteScreenPosition) const
	{
        for(const auto& Surface:{CategoriesWidget, TSharedPtr<SWidget>(ChainsGrid), ExpansionWidget})
            if(Surface.IsValid() && Surface->GetVisibility().IsVisible() && Surface->GetCachedGeometry().IsUnderLocation(AbsoluteScreenPosition))return true;
        return false;
	}
}

#undef LOCTEXT_NAMESPACE
