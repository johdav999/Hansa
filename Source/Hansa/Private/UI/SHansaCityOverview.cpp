#include "UI/SHansaCityOverview.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/SHansaMarketTable.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SHansaCityOverview"

namespace Hansa::UI
{
	namespace
	{
		FString SafeId(FString Value)
		{
			Value.ReplaceInline(TEXT("."), TEXT("_"));
			Value.ReplaceInline(TEXT("#"), TEXT("_"));
			Value.ReplaceInline(TEXT("@"), TEXT("_"));
			return Value;
		}

		FString LoadStateValue(const EHansaCityOverviewLoadState State)
		{
			switch (State)
			{
			case EHansaCityOverviewLoadState::Ready: return TEXT("ready");
			case EHansaCityOverviewLoadState::Loading: return TEXT("loading");
			case EHansaCityOverviewLoadState::Error: return TEXT("error");
			default: return TEXT("empty");
			}
		}
	}

	SHansaCityOverview::~SHansaCityOverview()
	{
		if (UHansaCityOverviewPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaCityOverview::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model;
		MarketTableModel = Arguments._MarketTableModel;
		PresentationSize = Arguments._InitialViewportSize;
		WorldOverlayBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::WorldOverlay);
		WorkingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		DecisionBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		CriticalBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Critical);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		DarkHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1, true);
		DarkBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, true);
		LightHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);
		LightBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		LightDataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, false);
		LightCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);

		ChildSlot
		[
			SAssignNew(PresentationBox, SBox)
			.WidthOverride(static_cast<float>(PresentationSize.X))
			.HeightOverride(static_cast<float>(PresentationSize.Y))
			[
				SAssignNew(RootWidget, SBorder).BorderImage(&WorldOverlayBrush).Padding(24.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SAssignNew(TitleText, STextBlock).TextStyle(&DarkHeadingStyle).AutoWrapText(true)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SAssignNew(CloseButton, SButton).ButtonStyle(&PrimaryButtonStyle)
							.Text(LOCTEXT("Close", "Close [Esc/B]")).ToolTipText(LOCTEXT("CloseTip", "Close the City Overview and restore focus."))
							.OnClicked(this, &SHansaCityOverview::InvokeClose)
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 12.0f)
					[
						SAssignNew(SummaryBox, SHorizontalBox)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SAssignNew(PopulationTab, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("PopulationTab", "Population"))
							.OnClicked(this, &SHansaCityOverview::InvokeTab, EHansaCityOverviewTab::Population)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SAssignNew(ProductionTab, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("ProductionTab", "Production"))
							.OnClicked(this, &SHansaCityOverview::InvokeTab, EHansaCityOverviewTab::Production)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SAssignNew(MarketTab, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("MarketTab", "Market"))
							.OnClicked(this, &SHansaCityOverview::InvokeTab, EHansaCityOverviewTab::Market)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SAssignNew(AdministrationTab, SButton).ButtonStyle(&SecondaryButtonStyle)
							.Text(LOCTEXT("AdministrationFuture", "▣ Administration · Future")).IsEnabled(false)
							.ToolTipText(LOCTEXT("AdministrationFutureTip", "Administration is planned after the MVP and is not interactive."))
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SAssignNew(MarketTableWidget, SHansaMarketTable).Model(MarketTableModel.Get())
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SAssignNew(ListPanel, SBorder).BorderImage(&WorkingBrush).Padding(8.0f)
						[
							SAssignNew(ListView, SListView<TSharedPtr<FHansaCityOverviewRowPresentation>>)
							.ListItemsSource(&ListItems)
							.SelectionMode(ESelectionMode::Single)
							.OnGenerateRow(this, &SHansaCityOverview::GenerateRow)
							.OnMouseButtonDoubleClick_Lambda([this](TSharedPtr<FHansaCityOverviewRowPresentation> Item)
							{
								if (Item.IsValid()) InvokeCausal(Item->StableId);
							})
						]
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SAssignNew(StatePanel, SBorder).BorderImage(&DecisionBrush).Padding(24.0f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
							[
								SAssignNew(StateTitleText, STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true).Justification(ETextJustify::Center)
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)
							[
								SAssignNew(StateDetailText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).Justification(ETextJustify::Center)
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)
							[
								SAssignNew(RetryButton, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Retry", "Try again"))
								.OnClicked(this, &SHansaCityOverview::InvokeRetry)
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SAssignNew(LastActionText, STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)
					]
				]
			]
		];

		for (int32 Index = 0; Index < 6; ++Index)
		{
			TSharedPtr<SBorder> Card;
			TSharedPtr<STextBlock> Label;
			TSharedPtr<STextBlock> Value;
			TSharedPtr<STextBlock> Detail;
			SummaryBox->AddSlot().FillWidth(1.0f).Padding(2.0f)
			[
				SAssignNew(Card, SBorder).BorderImage(&WorkingBrush).Padding(8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SAssignNew(Label, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(Value, STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
					+ SVerticalBox::Slot().AutoHeight()[SAssignNew(Detail, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
				]
			];
			SummaryCards.Add(Card); SummaryLabels.Add(Label); SummaryValues.Add(Value); SummaryDetails.Add(Detail);
		}

		MapWidget(TEXT("CityOverview.Root"), RootWidget);
		MapWidget(TEXT("CityOverview.Close"), CloseButton);
		MapWidget(TEXT("CityOverview.Header"), SummaryBox);
		MapWidget(TEXT("CityOverview.Tab.Population"), PopulationTab);
		MapWidget(TEXT("CityOverview.Tab.Production"), ProductionTab);
		MapWidget(TEXT("CityOverview.Tab.Market"), MarketTab);
		MapWidget(TEXT("CityOverview.Tab.Administration"), AdministrationTab);
		MapWidget(TEXT("CityOverview.List"), ListPanel);
		MapWidget(TEXT("CityOverview.State"), StatePanel);
		MapWidget(TEXT("CityOverview.State.Retry"), RetryButton);

		if (UHansaCityOverviewPresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaCityOverview::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
		else RootWidget->SetVisibility(EVisibility::Collapsed);
	}

	void SHansaCityOverview::SetPresentationSize(const FIntPoint Size)
	{
		PresentationSize = Size;
		if (PresentationBox.IsValid())
		{
			PresentationBox->SetWidthOverride(static_cast<float>(Size.X));
			PresentationBox->SetHeightOverride(static_cast<float>(Size.Y));
		}
	}

	FString SHansaCityOverview::RowSemanticId(const FName StableId)
	{
		return FString::Printf(TEXT("CityOverview.Row.%s"), *SafeId(StableId.ToString()));
	}

	FString SHansaCityOverview::RevealSemanticId(const FName StableId)
	{
		return RowSemanticId(StableId) + TEXT(".Reveal");
	}

	FString SHansaCityOverview::TabSemanticId(const EHansaCityOverviewTab Tab)
	{
		switch (Tab)
		{
		case EHansaCityOverviewTab::Production: return TEXT("CityOverview.Tab.Production");
		case EHansaCityOverviewTab::Market: return TEXT("CityOverview.Tab.Market");
		default: return TEXT("CityOverview.Tab.Population");
		}
	}

	TSharedRef<ITableRow> SHansaCityOverview::GenerateRow(
		TSharedPtr<FHansaCityOverviewRowPresentation> Item,
		const TSharedRef<STableViewBase>& OwnerTable)
	{
		TSharedPtr<SBorder> RowBorder;
		TSharedPtr<SButton> RowButton;
		TSharedPtr<SButton> CausalButton;
		TSharedPtr<SHorizontalBox> FieldsBox;
		const FString RowId = Item.IsValid() ? RowSemanticId(Item->StableId) : FString();
		const FString RevealId = Item.IsValid() ? RevealSemanticId(Item->StableId) : FString();
		TSharedRef<STableRow<TSharedPtr<FHansaCityOverviewRowPresentation>>> TableRow =
			SNew(STableRow<TSharedPtr<FHansaCityOverviewRowPresentation>>, OwnerTable).Padding(2.0f)
			[
				SAssignNew(RowBorder, SBorder).BorderImage(Item.IsValid() && Item->bError ? &CriticalBrush : (Item.IsValid() && Item->bWarning ? &DecisionBrush : &WorkingBrush)).Padding(8.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.22f).VAlign(VAlign_Center)
					[
						SAssignNew(RowButton, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaCityOverview::InvokeRow, Item.IsValid() ? Item->StableId : NAME_None)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Item.IsValid() ? Item->Title : FText()).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
							+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Item.IsValid() ? Item->Subtitle : FText()).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
						]
					]
					+ SHorizontalBox::Slot().FillWidth(0.54f).VAlign(VAlign_Center).Padding(8.0f, 0.0f)
					[
						SAssignNew(FieldsBox, SHorizontalBox)
					]
					+ SHorizontalBox::Slot().FillWidth(0.12f).VAlign(VAlign_Center).Padding(4.0f)
					[
						SNew(STextBlock).Text(Item.IsValid() ? Item->Status : FText()).TextStyle(&LightBodyStyle).AutoWrapText(true)
					]
					+ SHorizontalBox::Slot().FillWidth(0.16f).VAlign(VAlign_Center)
					[
						SAssignNew(CausalButton, SButton).ButtonStyle(&PrimaryButtonStyle)
						.Text(Item.IsValid() ? Item->CausalActionLabel : FText())
						.IsEnabled(Item.IsValid() && Item->bCausalActionEnabled)
						.ToolTipText(Item.IsValid() && Item->bCausalActionEnabled
							? FText::Format(LOCTEXT("CausalTip", "Open related cause: {0}"), FText::FromName(Item->RelatedSemanticId))
							: (Item.IsValid() ? Item->CausalActionDisabledReason : FText()))
						.OnClicked(this, &SHansaCityOverview::InvokeCausal, Item.IsValid() ? Item->StableId : NAME_None)
					]
				]
			];
		if (Item.IsValid())
		{
			for (const FHansaCityOverviewFieldPresentation& Field : Item->Fields)
			{
				FieldsBox->AddSlot().FillWidth(1.0f).Padding(3.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Field.Label).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
					+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Field.Value).TextStyle(&LightDataStyle).AutoWrapText(true)]
				];
			}
			MapWidget(RowId, RowButton);
			MapWidget(RevealId, CausalButton);
		}
		return TableRow;
	}

	void SHansaCityOverview::RebuildListItems(const FHansaCityOverviewSnapshot& Snapshot)
	{
		ListItems.Reset();
		PresentedRows.Reset();
		if (const UHansaCityOverviewPresentationModel* Pinned = Model.Get())
		{
			for (const FHansaCityOverviewRowPresentation& Row : Pinned->GetActiveRows())
			{
				PresentedRows.Add(Row);
				ListItems.Add(MakeShared<FHansaCityOverviewRowPresentation>(Row));
			}
		}
		ListView->RequestListRefresh();
#if WITH_DEV_AUTOMATION_TESTS
		++ListRefreshCount;
#endif
		RebuildFocusOrder(Snapshot);
	}

	void SHansaCityOverview::RebuildFocusOrder(const FHansaCityOverviewSnapshot& Snapshot)
	{
		FocusOrder = { TEXT("CityOverview.Close"), TEXT("CityOverview.Tab.Population"), TEXT("CityOverview.Tab.Production"), TEXT("CityOverview.Tab.Market") };
		if (Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableModel.IsValid() && MarketTableWidget.IsValid())
		{
			FocusOrder.Append(MarketTableWidget->GetControllerFocusOrder());
			return;
		}
		for (const FHansaCityOverviewRowPresentation& Row : PresentedRows)
		{
			FocusOrder.Add(RowSemanticId(Row.StableId));
			if (Row.bCausalActionEnabled) FocusOrder.Add(RevealSemanticId(Row.StableId));
		}
		if (Snapshot.LoadState == EHansaCityOverviewLoadState::Error) FocusOrder.Add(TEXT("CityOverview.State.Retry"));
	}

	void SHansaCityOverview::Refresh(const FHansaCityOverviewSnapshot& Snapshot, const uint64 Revision)
	{
		PresentedRevision = Revision;
		RootWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		TitleText->SetText(Snapshot.CityTitle);
		for (int32 Index = 0; Index < SummaryCards.Num(); ++Index)
		{
			const bool bHasSummary = Snapshot.HeaderSummaries.IsValidIndex(Index);
			const FHansaCityOverviewSummaryPresentation* Summary = bHasSummary ? &Snapshot.HeaderSummaries[Index] : nullptr;
			SummaryLabels[Index]->SetText(Summary != nullptr ? Summary->Label : FText());
			SummaryValues[Index]->SetText(Summary != nullptr ? Summary->Value : FText());
			SummaryDetails[Index]->SetText(Summary != nullptr ? Summary->Detail : FText());
			SummaryCards[Index]->SetBorderImage(Summary != nullptr && Summary->bError ? &CriticalBrush : (Summary != nullptr && Summary->bWarning ? &DecisionBrush : &WorkingBrush));
			if (Summary != nullptr) MapWidget(FString::Printf(TEXT("CityOverview.Header.%s"), *Summary->StableId.ToString()), SummaryCards[Index]);
		}
		PopulationTab->SetButtonStyle(Snapshot.ActiveTab == EHansaCityOverviewTab::Population ? &PrimaryButtonStyle : &SecondaryButtonStyle);
		ProductionTab->SetButtonStyle(Snapshot.ActiveTab == EHansaCityOverviewTab::Production ? &PrimaryButtonStyle : &SecondaryButtonStyle);
		MarketTab->SetButtonStyle(Snapshot.ActiveTab == EHansaCityOverviewTab::Market ? &PrimaryButtonStyle : &SecondaryButtonStyle);
		StateTitleText->SetText(Snapshot.StateTitle);
		StateDetailText->SetText(Snapshot.StateDetail);
		LastActionText->SetText(Snapshot.LastActionResult);
		const bool bReady = Snapshot.LoadState == EHansaCityOverviewLoadState::Ready;
		const bool bShowMarket = bReady && Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableModel.IsValid();
		ListPanel->SetVisibility(bReady && !bShowMarket ? EVisibility::Visible : EVisibility::Collapsed);
		if (MarketTableWidget.IsValid()) MarketTableWidget->SetVisibility(bShowMarket ? EVisibility::Visible : EVisibility::Collapsed);
		StatePanel->SetVisibility(bReady ? EVisibility::Collapsed : EVisibility::Visible);
		StatePanel->SetBorderImage(Snapshot.LoadState == EHansaCityOverviewLoadState::Error ? &CriticalBrush : &DecisionBrush);
		RetryButton->SetVisibility(Snapshot.LoadState == EHansaCityOverviewLoadState::Error ? EVisibility::Visible : EVisibility::Collapsed);
		const UHansaCityOverviewPresentationModel* Pinned = Model.Get();
		const TConstArrayView<FHansaCityOverviewRowPresentation> ActiveRows = Pinned != nullptr ? Pinned->GetActiveRows() : TConstArrayView<FHansaCityOverviewRowPresentation>();
		bool bRowsChanged = PresentedRows.Num() != ActiveRows.Num();
		for (int32 Index = 0; !bRowsChanged && Index < PresentedRows.Num(); ++Index) bRowsChanged = !(PresentedRows[Index] == ActiveRows[Index]);
		if (bRowsChanged) RebuildListItems(Snapshot); else RebuildFocusOrder(Snapshot);
	}

	FReply SHansaCityOverview::InvokeTab(const EHansaCityOverviewTab Tab)
	{
		return Model.IsValid() && Model->SelectTabIntent(Tab) ? FReply::Handled() : FReply::Unhandled();
	}

	FReply SHansaCityOverview::InvokeClose()
	{
		return Model.IsValid() && Model->CloseIntent() ? FReply::Handled() : FReply::Unhandled();
	}

	FReply SHansaCityOverview::InvokeRetry()
	{
		return Model.IsValid() && Model->RetryIntent() ? FReply::Handled() : FReply::Unhandled();
	}

	FReply SHansaCityOverview::InvokeRow(const FName RowStableId)
	{
		return Model.IsValid() && Model->SelectRowIntent(RowStableId) ? FReply::Handled() : FReply::Unhandled();
	}

	FReply SHansaCityOverview::InvokeCausal(const FName RowStableId)
	{
		return Model.IsValid() && Model->ActivateCausalIntent(RowStableId) ? FReply::Handled() : FReply::Unhandled();
	}

	void SHansaCityOverview::MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget)
	{
		SemanticWidgets.Add(SemanticId, Widget);
	}

	bool SHansaCityOverview::ActivateSemanticId(const FString& SemanticId)
	{
		if (!Model.IsValid()) return false;
		if (MarketTableWidget.IsValid() && SemanticId.StartsWith(TEXT("Market."))) return MarketTableWidget->ActivateSemanticId(SemanticId);
		if (SemanticId == TEXT("CityOverview.Close")) return Model->CloseIntent();
		if (SemanticId == TEXT("CityOverview.Tab.Population")) return Model->SelectTabIntent(EHansaCityOverviewTab::Population);
		if (SemanticId == TEXT("CityOverview.Tab.Production")) return Model->SelectTabIntent(EHansaCityOverviewTab::Production);
		if (SemanticId == TEXT("CityOverview.Tab.Market")) return Model->SelectTabIntent(EHansaCityOverviewTab::Market);
		if (SemanticId == TEXT("CityOverview.State.Retry")) return Model->RetryIntent();
		for (const FHansaCityOverviewRowPresentation& Row : Model->GetActiveRows())
		{
			if (SemanticId == RowSemanticId(Row.StableId)) return Model->SelectRowIntent(Row.StableId);
			if (SemanticId == RevealSemanticId(Row.StableId)) return Model->ActivateCausalIntent(Row.StableId);
		}
		return false;
	}

	bool SHansaCityOverview::FocusSemanticId(const FString& SemanticId)
	{
		if (MarketTableWidget.IsValid() && SemanticId.StartsWith(TEXT("Market.")))
		{
			const bool bFocused = MarketTableWidget->FocusSemanticId(SemanticId);
			if (bFocused && Model.IsValid()) Model->SetFocusedSemanticId(FName(*SemanticId));
			return bFocused;
		}
		const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found != nullptr ? Found->Pin() : nullptr;
		if (!Widget.IsValid() && (SemanticId.StartsWith(TEXT("CityOverview.Row."))))
		{
			for (const TSharedPtr<FHansaCityOverviewRowPresentation>& Item : ListItems)
			{
				if (!Item.IsValid()) continue;
				if (SemanticId != RowSemanticId(Item->StableId) && SemanticId != RevealSemanticId(Item->StableId)) continue;
				ListView->SetSelection(Item, ESelectInfo::OnNavigation);
				ListView->RequestScrollIntoView(Item);
				if (UHansaCityOverviewPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
				if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(ListView, EFocusCause::Navigation);
				return true;
			}
		}
		if (!Widget.IsValid() || !Widget->IsEnabled()) return false;
		if (UHansaCityOverviewPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
		return true;
	}

	TArray<FString> SHansaCityOverview::GetControllerFocusOrder() const
	{
		if (Model.IsValid() && Model->GetSnapshot().ActiveTab == EHansaCityOverviewTab::Market && MarketTableWidget.IsValid())
		{
			TArray<FString> Current = { TEXT("CityOverview.Close"), TEXT("CityOverview.Tab.Population"), TEXT("CityOverview.Tab.Production"), TEXT("CityOverview.Tab.Market") };
			Current.Append(MarketTableWidget->GetControllerFocusOrder());
			return Current;
		}
		return FocusOrder;
	}

	FReply SHansaCityOverview::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		if (!Model.IsValid()) return FReply::Unhandled();
		if (Model->GetSnapshot().ActiveTab == EHansaCityOverviewTab::Market) RebuildFocusOrder(Model->GetSnapshot());
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) return InvokeClose();
		if (Key == EKeys::Left || Key == EKeys::Gamepad_LeftShoulder) return Model->CycleTabIntent(-1) ? FReply::Handled() : FReply::Unhandled();
		if (Key == EKeys::Right || Key == EKeys::Gamepad_RightShoulder) return Model->CycleTabIntent(1) ? FReply::Handled() : FReply::Unhandled();
		if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			return ActivateSemanticId(Model->GetSnapshot().FocusedSemanticId.ToString()) ? FReply::Handled() : FReply::Unhandled();
		}
		if ((Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down) && !FocusOrder.IsEmpty())
		{
			const FString Current = Model->GetSnapshot().FocusedSemanticId.ToString();
			int32 Index = FocusOrder.IndexOfByKey(Current);
			const bool bForward = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down;
			Index = Index == INDEX_NONE ? 0 : (Index + (bForward ? 1 : FocusOrder.Num() - 1)) % FocusOrder.Num();
			return FocusSemanticId(FocusOrder[Index]) ? FReply::Handled() : FReply::Unhandled();
		}
		return FReply::Unhandled();
	}

	TArray<FHansaHudSemanticNode> SHansaCityOverview::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Nodes;
		const UHansaCityOverviewPresentationModel* Pinned = Model.Get();
		if (Pinned == nullptr) return Nodes;
		const FHansaCityOverviewSnapshot& Snapshot = Pinned->GetSnapshot();
		auto Add = [this, &Nodes, &Snapshot](const FString& Id, const TCHAR* ParentId, const FString& Label,
			const EHansaHudSemanticRole Role, const bool bActivate = false, const bool bFocus = false,
			const FString& ValueType = FString(), const FString& Value = FString(), const bool bSelected = false,
			const bool bEnabled = true, const bool bWarning = false, const bool bError = false)
		{
			FHansaHudSemanticNode Node;
			Node.Id = Id; Node.ParentId = ParentId; Node.Label = Label; Node.Role = Role;
			Node.bCanActivate = bActivate; Node.bCanFocus = bFocus;
			Node.State.bVisible = Snapshot.bOpen; Node.State.bEnabled = bEnabled;
			Node.State.bFocused = Snapshot.FocusedSemanticId == FName(*Id); Node.State.bSelected = bSelected;
			Node.State.bWarning = bWarning; Node.State.bError = bError; Node.State.ValueType = ValueType; Node.State.Value = Value;
			if (const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(Id))
			{
				if (const TSharedPtr<SWidget> Widget = Found->Pin())
				{
					Node.State.bVisible = Snapshot.bOpen && Widget->GetVisibility().IsVisible();
					Node.State.bEnabled = bEnabled && Widget->IsEnabled();
					const FGeometry& Geometry = Widget->GetCachedGeometry();
					const FVector2f Origin = RootWidget->GetCachedGeometry().GetAbsolutePosition();
					const FVector2f Position = Geometry.GetAbsolutePosition() - Origin;
					const FVector2f Size = Geometry.GetDrawSize();
					Node.Bounds = FIntRect(FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y), FMath::RoundToInt(Position.X + Size.X), FMath::RoundToInt(Position.Y + Size.Y));
				}
			}
			Nodes.Add(MoveTemp(Node));
		};

		Add(TEXT("CityOverview.Root"), TEXT("HUD.Root"), Snapshot.CityTitle.ToString(), EHansaHudSemanticRole::Screen, false, false,
			TEXT("load-state"), LoadStateValue(Snapshot.LoadState), Snapshot.bOpen, true, false, Snapshot.LoadState == EHansaCityOverviewLoadState::Error);
		Add(TEXT("CityOverview.Close"), TEXT("CityOverview.Root"), TEXT("Close City Overview"), EHansaHudSemanticRole::Button, true, true);
		Add(TEXT("CityOverview.Header"), TEXT("CityOverview.Root"), TEXT("City summaries"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(Snapshot.HeaderSummaries.Num()));
		for (const FHansaCityOverviewSummaryPresentation& Summary : Snapshot.HeaderSummaries)
		{
			Add(FString::Printf(TEXT("CityOverview.Header.%s"), *Summary.StableId.ToString()), TEXT("CityOverview.Header"), Summary.Label.ToString(),
				EHansaHudSemanticRole::Status, false, false, TEXT("summary"), FString::Printf(TEXT("value=%s;detail=%s"), *Summary.Value.ToString(), *Summary.Detail.ToString()), false, true, Summary.bWarning, Summary.bError);
		}
		Add(TEXT("CityOverview.Tab.Population"), TEXT("CityOverview.Root"), TEXT("Population"), EHansaHudSemanticRole::Tab, true, true, TEXT("selected"), Snapshot.ActiveTab == EHansaCityOverviewTab::Population ? TEXT("true") : TEXT("false"), Snapshot.ActiveTab == EHansaCityOverviewTab::Population);
		Add(TEXT("CityOverview.Tab.Production"), TEXT("CityOverview.Root"), TEXT("Production"), EHansaHudSemanticRole::Tab, true, true, TEXT("selected"), Snapshot.ActiveTab == EHansaCityOverviewTab::Production ? TEXT("true") : TEXT("false"), Snapshot.ActiveTab == EHansaCityOverviewTab::Production);
		Add(TEXT("CityOverview.Tab.Market"), TEXT("CityOverview.Root"), TEXT("Market"), EHansaHudSemanticRole::Tab, true, true, TEXT("selected"), Snapshot.ActiveTab == EHansaCityOverviewTab::Market ? TEXT("true") : TEXT("false"), Snapshot.ActiveTab == EHansaCityOverviewTab::Market);
		Add(TEXT("CityOverview.Tab.Administration"), TEXT("CityOverview.Root"), TEXT("Administration · Future"), EHansaHudSemanticRole::Tab, false, false, TEXT("availability"), TEXT("future-placeholder"), false, false);
		const bool bNativeMarket = Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableModel.IsValid() && MarketTableWidget.IsValid();
		Add(TEXT("CityOverview.List"), TEXT("CityOverview.Root"), TEXT("Active tab rows"), EHansaHudSemanticRole::List, false, false, TEXT("count"),
			FString::FromInt(bNativeMarket ? 0 : Pinned->GetActiveRows().Num()));
		if (!bNativeMarket) for (const FHansaCityOverviewRowPresentation& Row : Pinned->GetActiveRows())
		{
			const FString Id = RowSemanticId(Row.StableId);
			Add(Id, TEXT("CityOverview.List"), Row.Title.ToString(), EHansaHudSemanticRole::ListItem, true, true, TEXT("row"),
				FString::Printf(TEXT("subtitle=%s;status=%s;fields=%d"), *Row.Subtitle.ToString(), *Row.Status.ToString(), Row.Fields.Num()),
				Snapshot.SelectedRowStableId == Row.StableId, true, Row.bWarning, Row.bError);
			for (const FHansaCityOverviewFieldPresentation& Field : Row.Fields)
			{
				Add(FString::Printf(TEXT("%s.Field.%s"), *Id, *Field.StableId.ToString()), *Id, Field.Label.ToString(), EHansaHudSemanticRole::Status,
					false, false, TEXT("value"), Field.Value.ToString());
			}
			Add(RevealSemanticId(Row.StableId), *Id, Row.CausalActionLabel.ToString(), EHansaHudSemanticRole::Button, true, true,
				TEXT("related-target"), Row.bCausalActionEnabled ? Row.RelatedSemanticId.ToString() : Row.CausalActionDisabledReason.ToString(),
				false, Row.bCausalActionEnabled, Row.bWarning, Row.bError);
		}
		const bool bError = Snapshot.LoadState == EHansaCityOverviewLoadState::Error;
		Add(TEXT("CityOverview.State"), TEXT("CityOverview.Root"), Snapshot.StateTitle.ToString(), bError ? EHansaHudSemanticRole::Alert : EHansaHudSemanticRole::Status,
			false, false, TEXT("state-detail"), Snapshot.StateDetail.ToString(), false, true, Snapshot.LoadState == EHansaCityOverviewLoadState::Loading, bError);
		Add(TEXT("CityOverview.State.Retry"), TEXT("CityOverview.State"), TEXT("Try again"), EHansaHudSemanticRole::Button, true, true,
			TEXT("availability"), bError ? TEXT("available") : TEXT("not-error"), false, bError, false, bError);
		if (Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableWidget.IsValid()) Nodes.Append(MarketTableWidget->GetSemanticSnapshot());
		return Nodes;
	}
}

#undef LOCTEXT_NAMESPACE
