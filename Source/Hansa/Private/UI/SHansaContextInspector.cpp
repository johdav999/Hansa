#include "UI/SHansaContextInspector.h"
#include "UI/SHansaProductionInspector.h"
#include "UI/SHansaResidenceInspector.h"
#include "Widgets/SOverlay.h"
#include "UI/SHansaMarketDemandRow.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaContextInspector"

namespace Hansa::UI
{
	SHansaContextInspector::~SHansaContextInspector()
	{
		if (UHansaInspectorPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaContextInspector::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model;Preferences=Arguments._Preferences;
		HeaderBrush = GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
		HeaderTextStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1,true);
		HeaderTextStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
		QuietDestructiveStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		const auto DestructiveInk=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oxblood);
        QuietDestructiveStyle.SetNormalForeground(DestructiveInk).SetHoveredForeground(DestructiveInk).SetPressedForeground(DestructiveInk).SetDisabledForeground(DestructiveInk);
		WorkingBrush = GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
		DecisionBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		CriticalBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Critical);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		LightHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);
		LightBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		LightCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);
		LightHeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences));
		LightBodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
		LightCaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences));

		auto Section = [this](const FText& Heading, TSharedPtr<SWidget>& OutWidget, const TSharedRef<SWidget>& Content, TSharedPtr<STextBlock>* HeadingTarget = nullptr)
		{
            const auto HeadingText=SNew(STextBlock).Text(Heading).TextStyle(&LightCaptionStyle);
            if(HeadingTarget)*HeadingTarget=HeadingText;
			return SAssignNew(OutWidget, SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder"))).Padding(0,8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[HeadingText]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[Content]
			];
		};

		ChildSlot
		[
			SAssignNew(RootWidget, SBorder).BorderImage(&WorkingBrush).Padding(0)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBorder).BorderImage(&HeaderBrush).Padding(12,8)[SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SAssignNew(IdentityGlyph,SHansaGlyph).Glyph(EUiGlyph::Building).OnDark(true).Size(24)]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)[SAssignNew(IdentityText, STextBlock).TextStyle(&HeaderTextStyle).AutoWrapText(true)]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(80)[SAssignNew(CloseButton, SHansaAction).Kind(EHansaUiButtonStyle::Icon).Compact(true).Preferences(Preferences).Label(LOCTEXT("Close", "Close")).Reason(LOCTEXT("CloseTip", "Close and restore focus to the originating object or alert.")).OnClicked(this, &SHansaContextInspector::Invoke, FName(TEXT("Inspector.Close")))]]]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 12.0f, 16.0f, 4.0f)[SAssignNew(StateText, STextBlock).TextStyle(&LightBodyStyle)]
				+ SVerticalBox::Slot().FillHeight(1.0f).Padding(16,0)
				[
					SAssignNew(Scroll,SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("ResultSection", "Production summary"), ResultWidget, SAssignNew(ResultText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true), &ResultSectionHeading)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("FlowsSection", "Inputs and outputs"), FlowsWidget, SAssignNew(FlowRows, SVerticalBox), &FlowSectionHeading)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
						[
							SAssignNew(ProblemWidget, SBorder).BorderImage(&DecisionBrush).Padding(8.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SAssignNew(ProblemHeadingWidget,SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SAssignNew(ProblemGlyph,SHansaGlyph).Size(24)]+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(LOCTEXT("ProblemSection", "Details")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(ProblemText, STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
								+ SVerticalBox::Slot().AutoHeight()[SAssignNew(CauseButton,SHansaAction).Kind(EHansaUiButtonStyle::Secondary).Compact(true).Preferences(Preferences).Label(LOCTEXT("Explain","Explain cause")).Reason(LOCTEXT("ExplainTip","Show or hide the cause, evidence and remedy.")).OnClicked(this,&SHansaContextInspector::Invoke,FName(TEXT("Inspector.Action.OpenCause")))]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
								[
									SAssignNew(CauseWidget, SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(CauseText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SAssignNew(EvidenceText, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SAssignNew(RemedyText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("ActionsSection", "Actions"), ActionsWidget, SAssignNew(ActionRows, SVerticalBox))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("HistorySection", "Recent history"), HistoryWidget, SAssignNew(HistoryRows, SVerticalBox))]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 6.0f, 16.0f, 8.0f)[SAssignNew(ResultStatusText, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
			]
		];

        const auto LegacyContent = ChildSlot.GetWidget();
        ChildSlot[SNew(SOverlay)
            + SOverlay::Slot()[LegacyContent]
            + SOverlay::Slot()[SAssignNew(ProductionPanel, SHansaProductionInspector).Model(Model.Get()).Preferences(Preferences)]
            + SOverlay::Slot()[SAssignNew(ResidencePanel,SHansaResidenceInspector).Model(Model.Get()).Preferences(Preferences)]];
        ProductionPanel->SetVisibility(EVisibility::Collapsed);
        ResidencePanel->SetVisibility(EVisibility::Collapsed);
		IdentityWidget=IdentityText;MapWidget(TEXT("Inspector.Action.OpenCause"),CauseButton);
		MapWidget(TEXT("Inspector.Root"), RootWidget); MapWidget(TEXT("Inspector.Identity"), IdentityWidget);
		MapWidget(TEXT("Inspector.Result"), ResultWidget); MapWidget(TEXT("Inspector.Flows"), FlowsWidget);
		MapWidget(TEXT("Inspector.Problem"), ProblemWidget); MapWidget(TEXT("Inspector.Problem.Cause"), CauseWidget);
		MapWidget(TEXT("Inspector.Actions"), ActionsWidget); MapWidget(TEXT("Inspector.History"), HistoryWidget);
		MapWidget(TEXT("Inspector.Close"), CloseButton);

		if (UHansaInspectorPresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaContextInspector::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
		else {IdentityText->SetText(LOCTEXT("Loading","Loading selection…"));ResultText->SetText(LOCTEXT("LoadingDetail","Waiting for the city state."));RootWidget->SetVisibility(EVisibility::Visible);CloseButton->SetEnabled(false);CauseButton->SetEnabled(false);}
	}

	FString SHansaContextInspector::InstanceId(const TCHAR* Prefix, const FName StableId)
	{
		FString Suffix = StableId.ToString(); Suffix.ReplaceInline(TEXT("."), TEXT("_")); Suffix.ReplaceInline(TEXT("#"), TEXT("_")); Suffix.ReplaceInline(TEXT("@"), TEXT("_"));
		return FString::Printf(TEXT("%s.%s"), Prefix, *Suffix);
	}

	void SHansaContextInspector::RebuildFlows(const FHansaInspectorSnapshot& Snapshot)
	{
        const bool Market = Snapshot.Kind == EHansaInspectorObjectKind::Market;
        // Keep rows alive through simulation updates so pointer/controller focus is stable.
        bool SameRows = Market && PresentedFlows.Num() == Snapshot.Flows.Num() && !Snapshot.Flows.IsEmpty();
        for (const auto& Flow : Snapshot.Flows)
        {
            const auto W = ResolveSemanticWidget(InstanceId(TEXT("Inspector.Flows.Item"), Flow.StableId));
            SameRows &= W.IsValid() && W->GetType() == TEXT("SHansaMarketDemandRow");
        }
        if (SameRows)
        {
            for (const auto& Flow : Snapshot.Flows)
                StaticCastSharedPtr<SHansaMarketDemandRow>(ResolveSemanticWidget(InstanceId(TEXT("Inspector.Flows.Item"), Flow.StableId)))->Refresh(Flow);
            PresentedFlows = Snapshot.Flows;
            return;
        }
		for(const auto& Old:PresentedFlows)SemanticWidgets.Remove(InstanceId(TEXT("Inspector.Flows.Item"),Old.StableId));
		FlowRows->ClearChildren();
		for (const FHansaInspectorFlowPresentation& Flow : Snapshot.Flows)
		{
			const FString Id = InstanceId(TEXT("Inspector.Flows.Item"), Flow.StableId);
            if (Market)
            {
                const auto DemandRow = SNew(SHansaMarketDemandRow).Preferences(Preferences)
                    .OnFocused_Lambda([this, Id] { RecordNativeFocus(FName(*Id)); });
                DemandRow->Refresh(Flow);
                FlowRows->AddSlot().AutoHeight().Padding(0, 4)[DemandRow];
                MapWidget(Id, DemandRow);
                continue;
            }
			TSharedPtr<SBorder> Row;
			FlowRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SAssignNew(Row, SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder")))
				.BorderBackgroundColor(Flow.bProblem ? UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::WarningAmber).CopyWithNewOpacity(0.18f) : FLinearColor::Transparent)
				.Padding(4.0f)
				[
					SNew(STextBlock).Text(FText::Format(LOCTEXT("FlowRow", "{0} · {1} · {2}"), Flow.Label, Flow.Value, Flow.State)).TextStyle(&LightBodyStyle).AutoWrapText(true)
				]
			];
			MapWidget(Id, Row);
		}
		if (Snapshot.Flows.IsEmpty()) FlowRows->AddSlot().AutoHeight()[SNew(STextBlock).Text((Market ? LOCTEXT("NoCitizenDemand", "No citizen demand recorded in this city.") : LOCTEXT("NoFlows", "No inputs, outputs or needs for this selection."))).TextStyle(&LightCaptionStyle).AutoWrapText(true)];
		PresentedFlows = Snapshot.Flows;
#if WITH_DEV_AUTOMATION_TESTS
		++StructureRebuildCount;
#endif
	}

	void SHansaContextInspector::RebuildActions(const FHansaInspectorSnapshot& Snapshot)
	{
        TArray<FName> OldIds,NewIds;
        for(const auto& A:PresentedActions)OldIds.Add(A.StableId);
        for(const auto& A:Snapshot.Actions)NewIds.Add(A.StableId);
        if(!bStructureInitialized || OldIds!=NewIds){
            for(const auto& Pair:ActionButtons)SemanticWidgets.Remove(Pair.Key);
            ActionRows->ClearChildren();ActionButtons.Reset();
            auto IsPrimary=[](FName Id){return Id==TEXT("Inspector.Action.ToggleProduction") || Id==TEXT("Inspector.Action.UpgradeResidence");};
            auto IsDestructive=[](FName Id){return Id==TEXT("Inspector.Action.RemoveBuilding") || Id==TEXT("Inspector.Action.CancelConstruction");};
            auto MakeAction=[&](const FHansaInspectorActionPresentation& A){
                auto Button=SNew(SHansaAction).Preferences(Preferences)
                  .Kind(IsPrimary(A.StableId)?EHansaUiButtonStyle::Primary:EHansaUiButtonStyle::Secondary)
                  .Label(A.Label).OnClicked(this,&SHansaContextInspector::Invoke,A.StableId);
                if(IsDestructive(A.StableId))Button->SetButtonStyle(&QuietDestructiveStyle);
                ActionButtons.Add(A.StableId.ToString(),Button);MapWidget(A.StableId.ToString(),Button);
                return Button;
            };
            for(const auto& A:Snapshot.Actions)if(IsPrimary(A.StableId))ActionRows->AddSlot().AutoHeight().Padding(0,4)[MakeAction(A)];
            TSharedPtr<SUniformGridPanel> Secondary;
            ActionRows->AddSlot().AutoHeight()[SAssignNew(Secondary,SUniformGridPanel).SlotPadding(FMargin(4))];
            int32 Index=0;
            for(const auto& A:Snapshot.Actions)if(!IsPrimary(A.StableId)&&!IsDestructive(A.StableId)&&A.StableId!=TEXT("Inspector.Action.OpenRelated")){
                Secondary->AddSlot(Index%2,Index/2)[MakeAction(A)];++Index;
            }
            for(const auto& A:Snapshot.Actions)if(A.StableId==TEXT("Inspector.Action.OpenRelated"))ActionRows->AddSlot().AutoHeight().Padding(4,0,4,4)[MakeAction(A)];
            for(const auto& A:Snapshot.Actions)if(IsDestructive(A.StableId))ActionRows->AddSlot().AutoHeight().Padding(0,12,0,4)[MakeAction(A)];
        }
        FocusOrder={TEXT("Inspector.Close"),TEXT("Inspector.Action.OpenCause")};
        for(const auto& A:Snapshot.Actions){
            auto Button=StaticCastSharedPtr<SHansaAction>(ActionButtons[A.StableId.ToString()]);
            Button->SetLabel(A.Label);Button->SetState(!A.bEnabled?EUiState::Disabled:A.bSelected?EUiState::Selected:EUiState::Default,A.bEnabled?A.ToolTip:A.DisabledReason);
            if(A.bEnabled)FocusOrder.Add(A.StableId.ToString());
        }
		PresentedActions = Snapshot.Actions;
#if WITH_DEV_AUTOMATION_TESTS
		++StructureRebuildCount;
#endif
	}

	void SHansaContextInspector::RebuildHistory(const FHansaInspectorSnapshot& Snapshot)
	{
		for(const auto& Old:PresentedHistory)SemanticWidgets.Remove(InstanceId(TEXT("Inspector.History.Item"),Old.StableId));
		HistoryRows->ClearChildren();
		for (const FHansaInspectorHistoryPresentation& Entry : Snapshot.History)
		{
			const FString Id = InstanceId(TEXT("Inspector.History.Item"), Entry.StableId);
			TSharedPtr<STextBlock> Row;
			HistoryRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SAssignNew(Row, STextBlock).Text(Entry.Age.IsEmpty()?Entry.Label:FText::Format(LOCTEXT("HistoryRow", "{0} · {1}"), Entry.Label, Entry.Age)).TextStyle(&LightCaptionStyle).AutoWrapText(true)];
			MapWidget(Id, Row);
		}
		if(Snapshot.History.IsEmpty())HistoryRows->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("NoHistory","No recent state changes.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)];
		PresentedHistory = Snapshot.History;
#if WITH_DEV_AUTOMATION_TESTS
		++StructureRebuildCount;
#endif
	}

	void SHansaContextInspector::Refresh(const FHansaInspectorSnapshot& Snapshot, const uint64 Revision)
	{
        bProductionMode = Snapshot.Production.bValid && Snapshot.DataState == EHansaInspectorDataState::Ready;
        ProductionPanel->SetVisibility(bProductionMode && Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
        bResidenceMode=Snapshot.Residence.bValid && Snapshot.DataState==EHansaInspectorDataState::Ready;
        ResidencePanel->SetVisibility(bResidenceMode && Snapshot.bOpen?EVisibility::Visible:EVisibility::Collapsed);
        if(bResidenceMode){RootWidget->SetVisibility(EVisibility::Collapsed);ResidencePanel->Refresh(Snapshot);FocusOrder=ResidencePanel->GetFocusOrder();PresentedRevision=Revision;return;}
        if (bProductionMode)
        {
            RootWidget->SetVisibility(EVisibility::Collapsed);
            ProductionPanel->Refresh(Snapshot);
            FocusOrder = ProductionPanel->GetFocusOrder();
            PresentedRevision = Revision;
            return;
        }
        const bool Cargo=Snapshot.Kind==EHansaInspectorObjectKind::Cargo;
        const bool Market=Snapshot.Kind==EHansaInspectorObjectKind::Market;
        ResultSectionHeading->SetText(Market?LOCTEXT("MarketSection","Market supply"):Cargo?LOCTEXT("CargoSection","Cargo aboard"):LOCTEXT("ResultSection", "Production summary"));
        FlowSectionHeading->SetText(Market?LOCTEXT("DemandSection","Citizen demand"):Cargo?LOCTEXT("JourneySection","Journey status"):LOCTEXT("FlowsSection", "Inputs and outputs"));
        IdentityGlyph->SetGlyph(Market?EUiGlyph::Civic:Cargo?EUiGlyph::Information:EUiGlyph::Building);
		PresentedRevision = Revision; RootWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		IdentityText->SetText(Snapshot.Identity.IsEmpty()?LOCTEXT("EmptyIdentity","No object selected"):Snapshot.Identity); StateText->SetText(Snapshot.State); ResultText->SetText(Snapshot.PrimaryResult.IsEmpty()?LOCTEXT("EmptyResult","Select an object in the city to inspect it."):Snapshot.PrimaryResult);
		ProblemText->SetText(Snapshot.Causal.Problem); CauseText->SetText(FText::Format(LOCTEXT("Cause", "Cause · {0}"), Snapshot.Causal.Cause));
		EvidenceText->SetText(FText::Format(LOCTEXT("Evidence", "Evidence · {0}"), Snapshot.Causal.Evidence));
		RemedyText->SetText(FText::Format(LOCTEXT("Remedy", "Remedy · {0}"), Snapshot.Causal.Remedy));
		CauseButton->SetState(Snapshot.DataState!=EHansaInspectorDataState::Ready?EUiState::Disabled:Snapshot.bCauseExpanded?EUiState::Selected:EUiState::Default,LOCTEXT("CauseTip","Show or hide the cause, evidence and remedy."));
		ProblemGlyph->SetGlyph(Snapshot.DataState==EHansaInspectorDataState::Loading?EUiGlyph::Loading:Snapshot.DataState==EHansaInspectorDataState::Error?EUiGlyph::Error:Snapshot.Causal.Severity==EHansaCausalSeverity::Critical?EUiGlyph::Error:Snapshot.Causal.Severity==EHansaCausalSeverity::Warning?EUiGlyph::Warning:EUiGlyph::Information);
		CauseWidget->SetVisibility(Snapshot.bCauseExpanded ? EVisibility::Visible : EVisibility::Collapsed);
		const bool bNeedsAttention = Snapshot.Causal.Severity == EHansaCausalSeverity::Warning || Snapshot.Causal.Severity == EHansaCausalSeverity::Critical;
		ProblemText->SetVisibility(bNeedsAttention || Snapshot.bCauseExpanded ? EVisibility::Visible : EVisibility::Collapsed);
		CauseButton->SetLabel(Snapshot.bCauseExpanded ? LOCTEXT("HideDetails", "Hide details") : bNeedsAttention ? LOCTEXT("Explain", "Explain cause") : LOCTEXT("ShowDetails", "Show details"));
		const EHansaUiSeverity Severity = Snapshot.Causal.Severity == EHansaCausalSeverity::Critical ? EHansaUiSeverity::Critical :
			(Snapshot.Causal.Severity == EHansaCausalSeverity::Warning ? EHansaUiSeverity::Warning : EHansaUiSeverity::Notice);
		ProblemWidget->SetBorderImage(bNeedsAttention ? &DecisionBrush : FCoreStyle::Get().GetBrush(TEXT("NoBorder")));
		ProblemWidget->SetBorderBackgroundColor(UHansaUiStyleLibrary::GetSeverityStyle(Severity).AccentColor.CopyWithNewOpacity(0.20f));
		ResultStatusText->SetText(Snapshot.LastActionResult);
		if (!bStructureInitialized || PresentedFlows != Snapshot.Flows) RebuildFlows(Snapshot);
		if (!bStructureInitialized || PresentedActions != Snapshot.Actions) RebuildActions(Snapshot);
		if (!bStructureInitialized || PresentedHistory != Snapshot.History) RebuildHistory(Snapshot);
        if (Market)
        {
            FocusOrder = {TEXT("Inspector.Close")};
            for (const auto& Flow : Snapshot.Flows) FocusOrder.Add(InstanceId(TEXT("Inspector.Flows.Item"), Flow.StableId));
            FocusOrder.Add(TEXT("Inspector.Action.OpenCause"));
            if (Snapshot.bCauseExpanded) for (const auto& A : Snapshot.Actions) if (A.bEnabled) FocusOrder.AddUnique(A.StableId.ToString());
        }
		bStructureInitialized = true;
        const bool Ready=Snapshot.DataState==EHansaInspectorDataState::Ready;
        CauseText->SetVisibility(Ready?EVisibility::Visible:EVisibility::Collapsed);
        EvidenceText->SetVisibility(Ready && !Snapshot.Causal.Evidence.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed);
        for(const auto& Section:{FlowsWidget,ActionsWidget,HistoryWidget})Section->SetVisibility(Ready?EVisibility::Visible:EVisibility::Collapsed);
        if(!Ready){FocusOrder={TEXT("Inspector.Close")};CauseButton->SetVisibility(EVisibility::Collapsed);CauseWidget->SetVisibility(EVisibility::Visible);ProblemText->SetText(Snapshot.State);CauseText->SetText(FText());EvidenceText->SetText(FText());}
        else CauseButton->SetVisibility(EVisibility::Visible);
        ResultSectionHeading->SetVisibility(Market ? EVisibility::Collapsed : EVisibility::Visible);
        StateText->SetVisibility(Market ? EVisibility::Collapsed : EVisibility::Visible);
        ProblemHeadingWidget->SetVisibility(Market && !Snapshot.bCauseExpanded ? EVisibility::Collapsed : EVisibility::Visible);
        if (Market && Ready)
        {
            for (const auto& Section : {ActionsWidget, HistoryWidget})
                Section->SetVisibility(Snapshot.bCauseExpanded ? EVisibility::Visible : EVisibility::Collapsed);
            CauseButton->SetLabel(Snapshot.bCauseExpanded ? LOCTEXT("LessMarket", "Less") : LOCTEXT("MarketDetails", "Details"));
        }
	}

	FReply SHansaContextInspector::Invoke(const FName SemanticId)
	{
		return Model.IsValid() && Model->ActivateAction(SemanticId) ? FReply::Handled() : FReply::Unhandled();
	}

	void SHansaContextInspector::RecordNativeFocus(FName Id){if(Model.IsValid())Model->SetFocusedSemanticId(Id);}

	void SHansaContextInspector::MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget)
	{
		SemanticWidgets.Add(SemanticId, Widget);
        if(Widget.IsValid() && Widget->GetType()==TEXT("SHansaAction"))StaticCastSharedPtr<SHansaAction>(Widget)->SetFocusHandler(FSimpleDelegate::CreateSP(this,&SHansaContextInspector::RecordNativeFocus,FName(*SemanticId)));
	}

	bool SHansaContextInspector::ActivateSemanticId(const FString& SemanticId)
	{
		return Model.IsValid() && Model->GetSnapshot().bOpen && (Model->GetSnapshot().DataState==EHansaInspectorDataState::Ready || SemanticId==TEXT("Inspector.Close")) && Model->ActivateAction(FName(*SemanticId));
	}

	bool SHansaContextInspector::FocusSemanticId(const FString& SemanticId)
	{
		if(bResidenceMode)return ResidencePanel->Focus(SemanticId);
		if (bProductionMode) return ProductionPanel->Focus(SemanticId);
		const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found ? Found->Pin() : nullptr;
		if (!Model.IsValid() || !Model->GetSnapshot().bOpen || !FocusOrder.Contains(SemanticId) || !Widget.IsValid() || !Widget->IsEnabled()) return false;
		if(SemanticId!=TEXT("Inspector.Close"))Scroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);
		if (UHansaInspectorPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
		return true;
	}

	FReply SHansaContextInspector::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		const FKey Key = InKeyEvent.GetKey();
        if(Key==EKeys::Tab || Key==EKeys::Gamepad_DPad_Down || Key==EKeys::Gamepad_DPad_Up || Key==EKeys::Gamepad_DPad_Left || Key==EKeys::Gamepad_DPad_Right){
            int32 Index=INDEX_NONE;for(int32 I=0;I<FocusOrder.Num();++I){auto W=ResolveSemanticWidget(FocusOrder[I]);if(W.IsValid() && W->HasKeyboardFocus())Index=I;}
            const bool Back=InKeyEvent.IsShiftDown() || Key==EKeys::Gamepad_DPad_Up || Key==EKeys::Gamepad_DPad_Left;
            for(int32 I=0;I<FocusOrder.Num();++I){Index=(Index+(Back?-1:1)+FocusOrder.Num())%FocusOrder.Num();if(FocusSemanticId(FocusOrder[Index]))return FReply::Handled();}
        }
		if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) return Invoke(TEXT("Inspector.Close"));
		if (Key == EKeys::F) return Invoke(TEXT("Inspector.Action.Frame"));
		if (Key == EKeys::C) return Invoke(TEXT("Inspector.Action.OpenRelated"));
		if (Key == EKeys::P) return Invoke(TEXT("Inspector.Action.Pin"));
		if (Key == EKeys::O) return Invoke(TEXT("Inspector.Action.ToggleProduction"));
		if (Key == EKeys::U) return Invoke(TEXT("Inspector.Action.UpgradeResidence"));
		if (Key == EKeys::X || Key == EKeys::Delete)
		{
			const UHansaInspectorPresentationModel* Pinned = Model.Get();
			if (Pinned != nullptr && Pinned->GetSnapshot().Actions.ContainsByPredicate([](const FHansaInspectorActionPresentation& Action)
				{ return Action.StableId == TEXT("Inspector.Action.CancelConstruction") && Action.bEnabled; }))
			{
				return Invoke(TEXT("Inspector.Action.CancelConstruction"));
			}
			return Invoke(TEXT("Inspector.Action.RemoveBuilding"));
		}
		return FReply::Unhandled();
	}

	void SHansaContextInspector::RevealSemanticWidget(const FString& Id){if(bResidenceMode){ResidencePanel->Reveal(Id);return;}if(bProductionMode){ProductionPanel->Reveal(Id);return;}if(auto W=ResolveSemanticWidget(Id))Scroll->ScrollDescendantIntoView(W,false,EDescendantScrollDestination::IntoView);}

	TSharedPtr<SWidget> SHansaContextInspector::ResolveSemanticWidget(const FString& Id) const {if(bResidenceMode)return ResidencePanel->Resolve(Id);if(bProductionMode)return ProductionPanel->Resolve(Id);const auto* W=SemanticWidgets.Find(Id);return W?W->Pin():nullptr;}

	TArray<FHansaHudSemanticNode> SHansaContextInspector::GetSemanticSnapshot() const
	{
		if(bResidenceMode)return ResidencePanel->GetSemanticSnapshot();
		if (bProductionMode) return ProductionPanel->GetSemanticSnapshot();
		TArray<FHansaHudSemanticNode> Nodes;
		const UHansaInspectorPresentationModel* Pinned = Model.Get(); if (Pinned == nullptr) return Nodes;
		const FHansaInspectorSnapshot& S = Pinned->GetSnapshot();
		auto Add = [this, &Nodes, &S](const FString& Id, const TCHAR* Parent, const FString& Label, const EHansaHudSemanticRole Role,
			const bool bActivate = false, const bool bFocus = false, const FString& Type = FString(), const FString& Value = FString(),
			const bool bSelected = false, const bool bEnabled = true, const bool bWarning = false, const bool bError = false)
		{
			FHansaHudSemanticNode N; N.Id = Id; N.ParentId = Parent; N.Label = Label; N.Role = Role; N.bCanActivate = bActivate; N.bCanFocus = bFocus;
			N.State.bVisible = S.bOpen && (Id==TEXT("Inspector.DataStatus") || S.DataState==EHansaInspectorDataState::Ready || Id==TEXT("Inspector.Root") || Id==TEXT("Inspector.Identity") || Id==TEXT("Inspector.Result") || Id.StartsWith(TEXT("Inspector.Problem")) || Id==TEXT("Inspector.Close")) && (Id!=TEXT("Inspector.Problem.Cause") || S.bCauseExpanded); N.State.bEnabled = bEnabled; N.State.bSelected = bSelected; N.State.bFocused = S.FocusedSemanticId == FName(*Id);
			N.State.bWarning = bWarning; N.State.bError = bError; N.State.ValueType = Type; N.State.Value = Value;
			if (const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(Id)) if (const TSharedPtr<SWidget> W = Found->Pin())
			{
				const FGeometry& G = W->GetCachedGeometry(); const FVector2f O = RootWidget->GetCachedGeometry().GetAbsolutePosition(); const FVector2f P = G.GetAbsolutePosition() - O; const FVector2f Z = G.GetDrawSize();
				N.Bounds = FIntRect(FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y), FMath::RoundToInt(P.X + Z.X), FMath::RoundToInt(P.Y + Z.Y));
			}
			Nodes.Add(MoveTemp(N));
		};
		const bool bWarning = S.Causal.Severity == EHansaCausalSeverity::Warning; const bool bCritical = S.Causal.Severity == EHansaCausalSeverity::Critical;
		Add(TEXT("Inspector.Root"), TEXT("HUD.InspectorHost"), TEXT("Context inspector"), EHansaHudSemanticRole::Panel, false, false, TEXT("object-id"), S.ObjectStableId.ToString(), S.bOpen);
		Add(TEXT("Inspector.DataStatus"),TEXT("Inspector.Root"),TEXT("Selection data status"),EHansaHudSemanticRole::Status,false,false,TEXT("data-state"),UEnum::GetValueAsString(S.DataState),false,true,false,S.DataState==EHansaInspectorDataState::Error);
		Add(TEXT("Inspector.Identity"), TEXT("Inspector.Root"), S.Identity.ToString(), EHansaHudSemanticRole::Status, false, false, TEXT("state"), S.State.ToString());
		Add(TEXT("Inspector.Result"), TEXT("Inspector.Root"), TEXT("Most important result"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), S.PrimaryResult.ToString());
		Add(TEXT("Inspector.Flows"), TEXT("Inspector.Root"), S.Kind == EHansaInspectorObjectKind::Market ? TEXT("Citizen demand") : S.Kind == EHansaInspectorObjectKind::Residence ? TEXT("Needs") : TEXT("Inputs and outputs"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(S.Flows.Num()));
        for (const auto& Flow : S.Flows)
        {
            const bool Market = S.Kind == EHansaInspectorObjectKind::Market;
            Add(InstanceId(TEXT("Inspector.Flows.Item"), Flow.StableId), TEXT("Inspector.Flows"), Flow.Label.ToString(),
                EHansaHudSemanticRole::Status, false, Market, Market ? TEXT("citizen-supply") : TEXT("flow"),
                Market ? FString::Printf(TEXT("known=%s;requiredMilliUnits=%lld;suppliedMilliUnits=%lld;percent=%s;period=%s"),
                    Flow.bDemandKnown ? TEXT("true") : TEXT("false"), Flow.DemandRequired, Flow.DemandSupplied, *Flow.State.ToString(), *Flow.DemandPeriod.ToString())
                    : FString::Printf(TEXT("value=%s;state=%s"), *Flow.Value.ToString(), *Flow.State.ToString()), false, true, Flow.bProblem);
        }
		Add(TEXT("Inspector.Problem"), TEXT("Inspector.Root"), S.Causal.Problem.ToString(), EHansaHudSemanticRole::Alert, false, false, TEXT("cause-code"), S.Causal.StableCode.ToString(), false, true, bWarning, bCritical);
		Add(TEXT("Inspector.Problem.Cause"), TEXT("Inspector.Problem"), S.Causal.Cause.ToString(), EHansaHudSemanticRole::Text, false, true, TEXT("causal-explanation"), FString::Printf(TEXT("cause=%s;evidence=%s;remedy=%s;related=%s"), *S.Causal.Cause.ToString(), *S.Causal.Evidence.ToString(), *S.Causal.Remedy.ToString(), *S.Causal.RelatedSemanticId.ToString()), S.bCauseExpanded, true, bWarning, bCritical);
		Add(TEXT("Inspector.Actions"), TEXT("Inspector.Root"), TEXT("Actions and automation"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(S.Actions.Num()));
		Add(TEXT("Inspector.Action.OpenCause"), TEXT("Inspector.Actions"), TEXT("Open cause"), EHansaHudSemanticRole::Button, true, true, TEXT("expanded"), S.bCauseExpanded ? TEXT("true") : TEXT("false"), S.bCauseExpanded);
		for (const FHansaInspectorActionPresentation& Action : S.Actions) Add(Action.StableId.ToString(), TEXT("Inspector.Actions"), Action.Label.ToString(), EHansaHudSemanticRole::Button, true, true, TEXT("action-state"), Action.bEnabled ? TEXT("available") : Action.DisabledReason.ToString(), Action.bSelected, Action.bEnabled);
		Add(TEXT("Inspector.History"), TEXT("Inspector.Root"), TEXT("History"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(S.History.Num()));
		for (const FHansaInspectorHistoryPresentation& Entry : S.History) Add(InstanceId(TEXT("Inspector.History.Item"), Entry.StableId), TEXT("Inspector.History"), Entry.Label.ToString(), EHansaHudSemanticRole::Status, false, false, TEXT("age"), Entry.Age.ToString());
		Add(TEXT("Inspector.Close"), TEXT("Inspector.Root"), TEXT("Close inspector"), EHansaHudSemanticRole::Button, true, true);
		Add(TEXT("Inspector.Tooltip.OpenCause"), TEXT("HUD.TooltipLayer"), TEXT("Open cause tooltip"), EHansaHudSemanticRole::Status, false, false, TEXT("tooltip"), FString::Printf(TEXT("shortcut=C;cause=%s;remedy=%s"), *S.Causal.Cause.ToString(), *S.Causal.Remedy.ToString()));
        if (S.Kind == EHansaInspectorObjectKind::Market)
        {
            for (auto& N : Nodes)
            {
                if (N.Id.StartsWith(TEXT("Inspector.History")) || N.Id == TEXT("Inspector.Actions") ||
                    (N.Id.StartsWith(TEXT("Inspector.Action.")) && N.Id != TEXT("Inspector.Action.OpenCause")) ||
                    N.Id == TEXT("Inspector.Problem.Cause"))
                    N.State.bVisible &= S.bCauseExpanded;
                N.bCanFocus &= N.State.bVisible;
                N.bCanActivate &= N.State.bVisible && N.State.bEnabled;
            }
        }
		return Nodes;
	}
}

#undef LOCTEXT_NAMESPACE
