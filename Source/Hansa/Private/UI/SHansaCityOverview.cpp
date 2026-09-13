#include "UI/SHansaCityOverview.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaUiStyle.h"
#include "UI/SHansaMarketTable.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
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
        Preferences=Arguments._Preferences;RowStyle=GetLedgerRowStyle(Preferences.bHighContrast);
		MarketTableModel = Arguments._MarketTableModel;
		PresentationSize = Arguments._InitialViewportSize;
		WorldOverlayBrush = GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
		WorkingBrush = GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
        ListStyle.SetBackgroundBrush(WorkingBrush);
		DecisionBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		CriticalBrush = GetComponentStyle(EUiSurface::Notification,EUiState::Error,Preferences).Brush;
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		DarkHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1, true);
		DarkBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, true);
		LightHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);
		LightBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		LightDataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, false);
		LightCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);

        DarkHeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
        DarkBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        LightHeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences));
        LightBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        LightDataStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
        LightCaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences));

		ChildSlot
		[
			SAssignNew(PresentationBox, SBox)
			.WidthOverride(1600.f)
			[
				SAssignNew(RootWidget, SBorder).BorderImage(&WorldOverlayBrush).Padding(24.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SAssignNew(TitleText, STextBlock).TextStyle(&DarkHeadingStyle).OverflowPolicy(ETextOverflowPolicy::Ellipsis).Clipping(EWidgetClipping::ClipToBounds)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox).WidthOverride(112)[SAssignNew(CloseButton, SHansaAction).Preferences(Preferences).Compact(true)
                            .Label(LOCTEXT("Close", "Close")).ToolTipText(LOCTEXT("CloseTip", "Close the City Overview and restore focus."))
							.OnClicked(this, &SHansaCityOverview::InvokeClose)]
						]
					]
                    + SVerticalBox::Slot().AutoHeight().Padding(0,8)
                    [SNew(SHorizontalBox)
                     + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SBox).WidthOverride(128)[SAssignNew(LubeckButton,SHansaAction).Preferences(Preferences).Compact(true).Label(LOCTEXT("Lubeck","Lübeck")).OnClicked_Lambda([this]{if (Model.IsValid()) { Model->SelectCityIntent(TEXT("City.Lubeck")); } return FReply::Handled();})]]
                     + SHorizontalBox::Slot().AutoWidth().Padding(0,0,16,0)[SNew(SBox).WidthOverride(128)[SAssignNew(RostockButton,SHansaAction).Preferences(Preferences).Compact(true).Label(LOCTEXT("Rostock","Rostock")).OnClicked_Lambda([this]{if (Model.IsValid()) { Model->SelectCityIntent(TEXT("City.Rostock")); } return FReply::Handled();})]]
                     + SHorizontalBox::Slot().AutoWidth().Padding(0,0,16,0)[SNew(SBox).WidthOverride(128)[SAssignNew(VisitButton,SHansaAction).Preferences(Preferences).Compact(true).Label(LOCTEXT("VisitCity","Visit city")).OnClicked_Lambda([this]{if (Model.IsValid()) { Model->VisitCityIntent(); } return FReply::Handled();})]]
                     + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(ReportText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true)]]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 12.0f)
					[
						SAssignNew(SummaryBox, SHorizontalBox)
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SAssignNew(PopulationTab, SHansaAction).Preferences(Preferences).Compact(true).Label(LOCTEXT("PopulationTab", "Population"))
							.OnClicked(this, &SHansaCityOverview::InvokeTab, EHansaCityOverviewTab::Population)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SAssignNew(ProductionTab, SHansaAction).Preferences(Preferences).Compact(true).Label(LOCTEXT("ProductionTab", "Production"))
							.OnClicked(this, &SHansaCityOverview::InvokeTab, EHansaCityOverviewTab::Production)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2.0f)
						[
							SAssignNew(MarketTab, SHansaAction).Preferences(Preferences).Compact(true).Label(LOCTEXT("MarketTab", "Market"))
							.OnClicked(this, &SHansaCityOverview::InvokeTab, EHansaCityOverviewTab::Market)
						]
                    + SHorizontalBox::Slot().FillWidth(1.2f).Padding(2)
                    [SAssignNew(MarketDetailToggle,SHansaAction).Preferences(Preferences).Compact(false).Label(LOCTEXT("FullMarket","Open full market"))
                     .OnClicked_Lambda([this]{ActivateSemanticId(TEXT("CityOverview.Market.Details")); return FReply::Handled();})]
					]

					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SAssignNew(MarketTableWidget, SHansaMarketTable).Model(MarketTableModel.Get()).Preferences(Preferences)
					]
					+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 12.0f, 0.0f, 0.0f)
					[
						SAssignNew(ListPanel, SBorder).BorderImage(&WorkingBrush).Padding(8.0f)
						[
							SAssignNew(ListView, SListView<TSharedPtr<FHansaCityOverviewRowPresentation>>)
							.ScrollIntoViewAlignment(EScrollIntoViewAlignment::TopOrLeft).ListViewStyle(&ListStyle).ListItemsSource(&ListItems)
							.SelectionMode(ESelectionMode::Single)
							.OnGenerateRow(this, &SHansaCityOverview::GenerateRow)
                            .OnSelectionChanged_Lambda([this](auto Item,ESelectInfo::Type){if(Item.IsValid() && Model.IsValid() && Model->GetSnapshot().SelectedRowStableId!=Item->StableId)Model->SelectRowIntent(Item->StableId);})
                            .OnItemScrolledIntoView_Lambda([this](auto, auto){if(!PendingFocus.IsEmpty()){const FString Id=PendingFocus;PendingFocus.Reset();FocusSemanticId(Id);}})
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
								SAssignNew(StateTitleText, STextBlock).TextStyle(&LightHeadingStyle).WrapTextAt_Lambda([this]{return FMath::Min(600.f,FMath::Max(160.f,float(PresentationSize.X)-96.f));}).AutoWrapText(false).Justification(ETextJustify::Center)
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)
							[
								SAssignNew(StateDetailText, STextBlock).TextStyle(&LightBodyStyle).WrapTextAt_Lambda([this]{return FMath::Min(600.f,FMath::Max(160.f,float(PresentationSize.X)-96.f));}).AutoWrapText(false).Justification(ETextJustify::Center)
							]
							+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)
							[
								SAssignNew(RetryButton, SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Preferences).Compact(false).Label(LOCTEXT("Retry", "Try again"))
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
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(Value, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
					+ SVerticalBox::Slot().AutoHeight()[SAssignNew(Detail, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true).Visibility(EVisibility::Collapsed)]
				]
			];
			Card->SetToolTipText(TAttribute<FText>::CreateLambda([Detail]{return Detail->GetText();}));
			SummaryCards.Add(Card); SummaryLabels.Add(Label); SummaryValues.Add(Value); SummaryDetails.Add(Detail);
		}

		MapWidget(TEXT("CityOverview.Root"), RootWidget);
		MapWidget(TEXT("CityOverview.Close"), CloseButton);
		MapWidget(TEXT("CityOverview.Header"), SummaryBox);
		MapWidget(TEXT("CityOverview.Tab.Population"), PopulationTab);
		MapWidget(TEXT("CityOverview.Tab.Production"), ProductionTab);
		MapWidget(TEXT("CityOverview.Tab.Market"), MarketTab);
		MapWidget(TEXT("CityOverview.City.Lubeck"),LubeckButton);
        MapWidget(TEXT("CityOverview.City.Rostock"),RostockButton);
        MapWidget(TEXT("CityOverview.Visit"),VisitButton);
        MapWidget(TEXT("CityOverview.Report"),ReportText);
        MapWidget(TEXT("CityOverview.Market.Details"),MarketDetailToggle);
		MapWidget(TEXT("CityOverview.List"), ListPanel);
		MapWidget(TEXT("CityOverview.State"), StatePanel);
        MapWidget(TEXT("CityOverview.State.Title"),StateTitleText);
        MapWidget(TEXT("CityOverview.State.Detail"),StateDetailText);
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
			PresentationBox->SetWidthOverride(FMath::Min(1600.f,float(Size.X)));
			PresentationBox->SetHeightOverride(FOptionalSize());
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

    TSharedRef<ITableRow> SHansaCityOverview::GenerateRow(TSharedPtr<FHansaCityOverviewRowPresentation> Item,const TSharedRef<STableViewBase>& OwnerTable)
    {
        TSharedPtr<SHansaAction> Select,Reveal;
        TSharedPtr<SUniformGridPanel> Fields;
        auto Row=SNew(STableRow<TSharedPtr<FHansaCityOverviewRowPresentation>>,OwnerTable).Style(&RowStyle).Padding(4)
        [SNew(SBorder).BorderImage(&WorkingBrush).Padding(16)
         [SNew(SVerticalBox)
          + SVerticalBox::Slot().AutoHeight()
          [SNew(SHorizontalBox)
           + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0,0,16,0)
           [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Item]{return Item->Title;}).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Item]{return Item->Subtitle;}).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
           + SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SBox).WidthOverride(112)[SAssignNew(Select,SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Preferences).Compact(true).Label(LOCTEXT("Select","Select")).OnClicked(this,&SHansaCityOverview::InvokeRow,Item->StableId)]]
           + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(240)[SAssignNew(Reveal,SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Preferences(Preferences).Compact(true).Label(Item->CausalActionLabel).OnClicked(this,&SHansaCityOverview::InvokeCausal,Item->StableId)]]]
          + SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Fields,SUniformGridPanel).SlotPadding(FMargin(0,4,16,4))]
          + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([Item]{return Item->Status;}).TextStyle(&LightBodyStyle).AutoWrapText(true)]]];
        // Two native columns reduce scan distance; every field wraps within its cell.
        for(int32 I=0;I<Item->Fields.Num();++I){
            TSharedPtr<STextBlock> Value;
            Fields->AddSlot(I%2,I/2).HAlign(HAlign_Fill)
            [SAssignNew(Value,STextBlock).Text_Lambda([Item,I]{return Item->Fields.IsValidIndex(I)?FText::Format(LOCTEXT("FieldLine","{0}: {1}"),Item->Fields[I].Label,Item->Fields[I].Value):FText();}).TextStyle(&LightBodyStyle).AutoWrapText(true)];
            MapWidget(RowSemanticId(Item->StableId)+TEXT(".Field.")+Item->Fields[I].StableId.ToString(),Value);
        }
        Select->SetState(Model.IsValid() && Model->GetSnapshot().SelectedRowStableId==Item->StableId?EUiState::Selected:EUiState::Default,FText());
        Reveal->SetState(Item->bCausalActionEnabled?EUiState::Default:EUiState::Disabled,Item->bCausalActionEnabled?Item->Status:Item->CausalActionDisabledReason);
        MapWidget(RowSemanticId(Item->StableId),Select);MapWidget(RevealSemanticId(Item->StableId),Reveal);
        return Row;
    }

	void SHansaCityOverview::RebuildListItems(const FHansaCityOverviewSnapshot& Snapshot)
	{
        const auto OldItems=ListItems;
        ListItems.Reset();PresentedRows.Reset();
        if(const auto* Pinned=Model.Get())for(const auto& Row:Pinned->GetActiveRows()){
            PresentedRows.Add(Row);
            const auto* Existing=OldItems.FindByPredicate([&](const auto& I){if(I->StableId!=Row.StableId || I->Fields.Num()!=Row.Fields.Num())return false;
                for(int32 Index=0;Index<Row.Fields.Num();++Index)if(I->Fields[Index].StableId!=Row.Fields[Index].StableId)return false;
                return true;});
            auto Item=Existing?*Existing:MakeShared<FHansaCityOverviewRowPresentation>(Row);
            *Item=Row;ListItems.Add(Item);
        }
		ListView->RequestListRefresh();
#if WITH_DEV_AUTOMATION_TESTS
		++ListRefreshCount;
#endif
		RebuildFocusOrder(Snapshot);
	}

	void SHansaCityOverview::RebuildFocusOrder(const FHansaCityOverviewSnapshot& Snapshot)
	{
		FocusOrder = { TEXT("CityOverview.Close"), TEXT("CityOverview.City.Lubeck"), TEXT("CityOverview.City.Rostock"), TEXT("CityOverview.Visit"), TEXT("CityOverview.Tab.Population"), TEXT("CityOverview.Tab.Production"), TEXT("CityOverview.Tab.Market") };
		if(Snapshot.LoadState==EHansaCityOverviewLoadState::Ready && Snapshot.CityStableId==TEXT("City.Lubeck") && Snapshot.ActiveTab==EHansaCityOverviewTab::Market && MarketTableModel.IsValid())FocusOrder.Add(TEXT("CityOverview.Market.Details"));
		if (bFullMarket && Snapshot.LoadState==EHansaCityOverviewLoadState::Ready && Snapshot.CityStableId==TEXT("City.Lubeck") && Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableModel.IsValid() && MarketTableWidget.IsValid())
		{
			FocusOrder.Append(MarketTableWidget->GetControllerFocusOrder());
			return;
		}
		if(Snapshot.LoadState==EHansaCityOverviewLoadState::Ready) for (const FHansaCityOverviewRowPresentation& Row : PresentedRows)
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
		TitleText->SetText(Snapshot.CityTitle);TitleText->SetToolTipText(Snapshot.CityTitle);
        const bool Remote=Snapshot.CityStableId==TEXT("City.Rostock");
        ReportText->SetText(Remote?LOCTEXT("RemoteContext","Trade reports only · civic data unavailable · no construction"):LOCTEXT("LocalContext","Inspect needs, production and trade"));
        LubeckButton->SetState(Remote?EUiState::Default:EUiState::Selected,FText());
        RostockButton->SetState(Remote?EUiState::Selected:EUiState::Default,FText());
		for (int32 Index = 0; Index < SummaryCards.Num(); ++Index)
		{
			const bool bHasSummary = Snapshot.HeaderSummaries.IsValidIndex(Index);
			const FHansaCityOverviewSummaryPresentation* Summary = bHasSummary ? &Snapshot.HeaderSummaries[Index] : nullptr;
			SummaryLabels[Index]->SetText(Summary != nullptr ? Summary->Label : FText());
			SummaryValues[Index]->SetText(Summary != nullptr ? Summary->Value : FText());
			SummaryDetails[Index]->SetText(Summary != nullptr ? Summary->Detail : FText());
			SummaryCards[Index]->SetBorderImage(Summary != nullptr && Summary->bError ? &CriticalBrush : (Summary != nullptr && Summary->bWarning ? &DecisionBrush : &WorkingBrush));
            const bool OnDark=Summary && Summary->bError;
            SummaryLabels[Index]->SetColorAndOpacity(UHansaUiStyleLibrary::GetColor(OnDark?EHansaUiColorToken::Chalk:EHansaUiColorToken::MutedInk));
            SummaryValues[Index]->SetColorAndOpacity(UHansaUiStyleLibrary::GetColor(OnDark?EHansaUiColorToken::Chalk:EHansaUiColorToken::Ink));
			if (Summary != nullptr) MapWidget(FString::Printf(TEXT("CityOverview.Header.%s"), *Summary->StableId.ToString()), SummaryCards[Index]);
		}
		StaticCastSharedPtr<SHansaAction>(PopulationTab)->SetState(Snapshot.ActiveTab == EHansaCityOverviewTab::Population?EUiState::Selected:EUiState::Default,FText());
		StaticCastSharedPtr<SHansaAction>(ProductionTab)->SetState(Snapshot.ActiveTab == EHansaCityOverviewTab::Production?EUiState::Selected:EUiState::Default,FText());
		StaticCastSharedPtr<SHansaAction>(MarketTab)->SetState(Snapshot.ActiveTab == EHansaCityOverviewTab::Market?EUiState::Selected:EUiState::Default,FText());
		StateTitleText->SetText(Snapshot.StateTitle);
		StateDetailText->SetText(Snapshot.StateDetail);
        const auto StateForeground=UHansaUiStyleLibrary::GetColor(Snapshot.LoadState==EHansaCityOverviewLoadState::Error?EHansaUiColorToken::Chalk:EHansaUiColorToken::Ink);
        StateTitleText->SetColorAndOpacity(StateForeground);
        StateDetailText->SetColorAndOpacity(StateForeground);
		LastActionText->SetText(Snapshot.LastActionResult);
        LastActionText->SetVisibility(Snapshot.LastActionResult.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
		const bool bReady = Snapshot.LoadState == EHansaCityOverviewLoadState::Ready;
        const bool MarketAvailable=bReady && !Remote && Snapshot.ActiveTab==EHansaCityOverviewTab::Market && MarketTableModel.IsValid();
        MarketDetailToggle->SetVisibility(MarketAvailable?EVisibility::Visible:EVisibility::Collapsed);
        MarketDetailToggle->SetLabel(bFullMarket?LOCTEXT("MarketSummary","Market summary"):LOCTEXT("FullMarket","Open full market"));
		const bool bShowMarket = bReady && bFullMarket && Snapshot.LoadState==EHansaCityOverviewLoadState::Ready && Snapshot.CityStableId==TEXT("City.Lubeck") && Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableModel.IsValid();
		ListPanel->SetVisibility(bReady && !bShowMarket ? EVisibility::Visible : EVisibility::Collapsed);
		SummaryBox->SetVisibility(bShowMarket?EVisibility::Collapsed:EVisibility::Visible);
		if (MarketTableWidget.IsValid()) MarketTableWidget->SetVisibility(bShowMarket ? EVisibility::Visible : EVisibility::Collapsed);
		StatePanel->SetVisibility(bReady ? EVisibility::Collapsed : EVisibility::Visible);
		StatePanel->SetBorderImage(Snapshot.LoadState == EHansaCityOverviewLoadState::Error ? &CriticalBrush : &DecisionBrush);
		RetryButton->SetVisibility(Snapshot.LoadState == EHansaCityOverviewLoadState::Error ? EVisibility::Visible : EVisibility::Collapsed);
		const UHansaCityOverviewPresentationModel* Pinned = Model.Get();
		const TConstArrayView<FHansaCityOverviewRowPresentation> ActiveRows = Pinned != nullptr ? Pinned->GetActiveRows() : TConstArrayView<FHansaCityOverviewRowPresentation>();
		bool bRowsChanged = PresentedRows.Num() != ActiveRows.Num();
		for (int32 Index = 0; !bRowsChanged && Index < PresentedRows.Num(); ++Index) bRowsChanged = !(PresentedRows[Index] == ActiveRows[Index]);
		if (bRowsChanged) RebuildListItems(Snapshot); else RebuildFocusOrder(Snapshot);
        if(bRowsChanged && Snapshot.FocusedSemanticId.ToString().StartsWith(TEXT("CityOverview.Row.")))FocusSemanticId(Snapshot.FocusedSemanticId.ToString());
        for(const auto& Item:ListItems){
            if(auto W=ResolveSemanticWidget(RowSemanticId(Item->StableId)))StaticCastSharedPtr<SHansaAction>(W)->SetState(Snapshot.SelectedRowStableId==Item->StableId?EUiState::Selected:EUiState::Default,FText());
            if(auto W=ResolveSemanticWidget(RevealSemanticId(Item->StableId))){auto Action=StaticCastSharedPtr<SHansaAction>(W);Action->SetLabel(Item->CausalActionLabel);Action->SetState(Item->bCausalActionEnabled?EUiState::Default:EUiState::Disabled,Item->bCausalActionEnabled?Item->Status:Item->CausalActionDisabledReason);}
        }
	}

	FReply SHansaCityOverview::InvokeTab(const EHansaCityOverviewTab Tab)
	{
		if (Model.IsValid()) { Model->SelectTabIntent(Tab); } return FReply::Handled();
	}

	FReply SHansaCityOverview::InvokeClose()
	{
		if (Model.IsValid()) { Model->CloseIntent(); } return FReply::Handled();
	}

	FReply SHansaCityOverview::InvokeRetry()
	{
		if (Model.IsValid()) { Model->RetryIntent(); } return FReply::Handled();
	}

	FReply SHansaCityOverview::InvokeRow(const FName RowStableId)
	{
		if (Model.IsValid()) { Model->SelectRowIntent(RowStableId); } return FReply::Handled();
	}

	FReply SHansaCityOverview::InvokeCausal(const FName RowStableId)
	{
		if (Model.IsValid()) { Model->ActivateCausalIntent(RowStableId); } return FReply::Handled();
	}

	void SHansaCityOverview::MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget)
	{
		SemanticWidgets.Add(SemanticId, Widget);
        if(Widget.IsValid() && Widget->GetType()==TEXT("SHansaAction"))StaticCastSharedPtr<SHansaAction>(Widget)->SetFocusHandler(FSimpleDelegate::CreateLambda([this,SemanticId]{if(Model.IsValid())Model->SetFocusedSemanticId(FName(*SemanticId));}));
	}

	bool SHansaCityOverview::ActivateSemanticId(const FString& SemanticId)
	{
		if (!Model.IsValid() || !Model->GetSnapshot().bOpen) return false;
		if (bFullMarket && Model->GetSnapshot().CityStableId==TEXT("City.Lubeck") && Model->GetSnapshot().ActiveTab==EHansaCityOverviewTab::Market && Model->GetSnapshot().LoadState==EHansaCityOverviewLoadState::Ready && MarketTableWidget.IsValid() && SemanticId.StartsWith(TEXT("Market."))) return MarketTableWidget->ActivateSemanticId(SemanticId);
		if(SemanticId==TEXT("CityOverview.Market.Details")){
            const auto& S=Model->GetSnapshot();if(S.CityStableId!=TEXT("City.Lubeck") || S.ActiveTab!=EHansaCityOverviewTab::Market || S.LoadState!=EHansaCityOverviewLoadState::Ready || !MarketTableModel.IsValid())return false;
            bFullMarket=!bFullMarket;Refresh(S,Model->GetRevision());return true;
        }
        if (SemanticId == TEXT("CityOverview.City.Lubeck"))return Model->SelectCityIntent(TEXT("City.Lubeck"));
        if (SemanticId == TEXT("CityOverview.Visit")) return Model->VisitCityIntent();
        if (SemanticId == TEXT("CityOverview.City.Rostock"))return Model->SelectCityIntent(TEXT("City.Rostock"));
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

	TSharedPtr<SWidget> SHansaCityOverview::ResolveSemanticWidget(const FString& Id) const {const auto* Found=SemanticWidgets.Find(Id);return Found?Found->Pin():nullptr;}

    bool SHansaCityOverview::FocusSemanticId(const FString& SemanticId)
    {
        if(!Model.IsValid() || !Model->GetSnapshot().bOpen || !GetControllerFocusOrder().Contains(SemanticId))return false;
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
				PendingFocus=SemanticId;
                ListView->RequestScrollIntoView(Item);
				if (UHansaCityOverviewPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
				if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(ListView, EFocusCause::Navigation);
				return true;
			}
		}
		if (!Widget.IsValid() || !Widget->IsEnabled()) return false;
        for(const auto& Item:ListItems)if(SemanticId==RowSemanticId(Item->StableId) || SemanticId==RevealSemanticId(Item->StableId))ListView->RequestScrollIntoView(Item);
		if (UHansaCityOverviewPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
		return true;
	}

	TArray<FString> SHansaCityOverview::GetControllerFocusOrder() const
	{
		if (Model.IsValid() && bFullMarket && Model->GetSnapshot().CityStableId==TEXT("City.Lubeck") && Model->GetSnapshot().LoadState==EHansaCityOverviewLoadState::Ready && Model->GetSnapshot().ActiveTab == EHansaCityOverviewTab::Market && MarketTableWidget.IsValid())
		{
			TArray<FString> Current = { TEXT("CityOverview.Close"), TEXT("CityOverview.City.Lubeck"), TEXT("CityOverview.City.Rostock"), TEXT("CityOverview.Visit"), TEXT("CityOverview.Tab.Population"), TEXT("CityOverview.Tab.Production"), TEXT("CityOverview.Tab.Market") };
			Current.Add(TEXT("CityOverview.Market.Details"));
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
		if ((Key == EKeys::Tab || Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down) && !FocusOrder.IsEmpty())
		{
			const FString Current = Model->GetSnapshot().FocusedSemanticId.ToString();
			int32 Index = FocusOrder.IndexOfByKey(Current);
			const bool bForward = (Key == EKeys::Tab && !InKeyEvent.IsShiftDown()) || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down;
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
			if(Id.StartsWith(TEXT("CityOverview.Header.")) || Id==TEXT("CityOverview.Header")) Node.State.bVisible &= !bFullMarket || Snapshot.ActiveTab!=EHansaCityOverviewTab::Market;
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
		const bool bNativeMarket = bFullMarket && Snapshot.LoadState==EHansaCityOverviewLoadState::Ready && Snapshot.CityStableId==TEXT("City.Lubeck") && Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableModel.IsValid() && MarketTableWidget.IsValid();
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
		if (bFullMarket && Snapshot.CityStableId==TEXT("City.Lubeck") && Snapshot.LoadState==EHansaCityOverviewLoadState::Ready && Snapshot.ActiveTab == EHansaCityOverviewTab::Market && MarketTableWidget.IsValid()) {
            auto MarketNodes=MarketTableWidget->GetSemanticSnapshot();
            const auto Offset=MarketTableWidget->GetCachedGeometry().GetAbsolutePosition()-RootWidget->GetCachedGeometry().GetAbsolutePosition();
            for(auto& Node:MarketNodes) if(Node.Bounds.Width()>0 && Node.Bounds.Height()>0) Node.Bounds+=FIntPoint(FMath::RoundToInt(Offset.X),FMath::RoundToInt(Offset.Y));
            Nodes.Append(MarketNodes);
        }
        Add(TEXT("CityOverview.City.Lubeck"),TEXT("CityOverview.Root"),TEXT("Lübeck"),EHansaHudSemanticRole::Button,true,true);
        Add(TEXT("CityOverview.City.Rostock"),TEXT("CityOverview.Root"),TEXT("Rostock"),EHansaHudSemanticRole::Button,true,true);
        Add(TEXT("CityOverview.Visit"),TEXT("CityOverview.Root"),TEXT("Visit city"),EHansaHudSemanticRole::Button,true,true);
        Add(TEXT("CityOverview.Visit.Status"),TEXT("CityOverview.Root"),TEXT("City visit status"),EHansaHudSemanticRole::Status,false,false,TEXT("visit"),Snapshot.LastActionResult.ToString());
        Add(TEXT("CityOverview.Market.Details"),TEXT("CityOverview.Root"),TEXT("Toggle full market"),EHansaHudSemanticRole::Button,true,true);
        Add(TEXT("CityOverview.Report"),TEXT("CityOverview.Root"),TEXT("Report context"),EHansaHudSemanticRole::Status,false,false,TEXT("context"),ReportText->GetText().ToString());
        for(auto& Node:Nodes){
            if(Node.Id==TEXT("CityOverview.Market.Details"))Node.State.bVisible=Snapshot.bOpen && Snapshot.LoadState==EHansaCityOverviewLoadState::Ready && Snapshot.CityStableId==TEXT("City.Lubeck") && Snapshot.ActiveTab==EHansaCityOverviewTab::Market;
            if(Node.Id==TEXT("CityOverview.Tab.Administration"))Node.State.bVisible=false;
            if(Node.Id.StartsWith(TEXT("CityOverview.Row.")) && Snapshot.LoadState!=EHansaCityOverviewLoadState::Ready)Node.State.bVisible=false;
            if(Node.Id==TEXT("CityOverview.State.Retry") && Snapshot.LoadState!=EHansaCityOverviewLoadState::Error)Node.State.bVisible=false;
        }
        return Nodes;
    }
}

#undef LOCTEXT_NAMESPACE
