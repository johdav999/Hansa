#include "UI/SHansaContextInspector.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
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
		Model = Arguments._Model;
		WorkingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		DecisionBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		CriticalBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Critical);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		LightHeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading2, false);
		LightBodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		LightCaptionStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Caption, false);

		auto Section = [this](const FText& Heading, TSharedPtr<SWidget>& OutWidget, const TSharedRef<SWidget>& Content)
		{
			return SAssignNew(OutWidget, SBorder).BorderImage(&WorkingBrush).Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Heading).TextStyle(&LightCaptionStyle)]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[Content]
			];
		};

		ChildSlot
		[
			SAssignNew(RootWidget, SBorder).BorderImage(&WorkingBrush).Padding(12.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)[SAssignNew(IdentityText, STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
					+ SHorizontalBox::Slot().AutoWidth()[SAssignNew(CloseButton, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Close", "Close [Esc/B]")).ToolTipText(LOCTEXT("CloseTip", "Close and restore focus to the originating object or alert.")).OnClicked(this, &SHansaContextInspector::Invoke, FName(TEXT("Inspector.Close")))]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)[SAssignNew(StateText, STextBlock).TextStyle(&LightBodyStyle)]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("IdentitySection", "1 · Identity and state"), IdentityWidget, SNew(STextBlock).Text(LOCTEXT("IdentityHelp", "Selected object and authoritative state")).TextStyle(&LightCaptionStyle))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("ResultSection", "2 · Most important result"), ResultWidget, SAssignNew(ResultText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("FlowsSection", "3 · Inputs, outputs or needs"), FlowsWidget, SAssignNew(FlowRows, SVerticalBox))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
						[
							SAssignNew(ProblemWidget, SBorder).BorderImage(&DecisionBrush).Padding(8.0f)
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ProblemSection", "4 · Current problem and cause")).TextStyle(&LightCaptionStyle)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SAssignNew(ProblemText, STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
								+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
								[
									SAssignNew(CauseWidget, SVerticalBox)
									+ SVerticalBox::Slot().AutoHeight()[SAssignNew(CauseText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SAssignNew(EvidenceText, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
									+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)[SAssignNew(RemedyText, STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
								]
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("ActionsSection", "5 · Actions and automation"), ActionsWidget, SAssignNew(ActionRows, SVerticalBox))]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)[Section(LOCTEXT("HistorySection", "6 · History"), HistoryWidget, SAssignNew(HistoryRows, SVerticalBox))]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[SAssignNew(ResultStatusText, STextBlock).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
			]
		];

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
		else RootWidget->SetVisibility(EVisibility::Collapsed);
	}

	FString SHansaContextInspector::InstanceId(const TCHAR* Prefix, const FName StableId)
	{
		FString Suffix = StableId.ToString(); Suffix.ReplaceInline(TEXT("."), TEXT("_")); Suffix.ReplaceInline(TEXT("#"), TEXT("_")); Suffix.ReplaceInline(TEXT("@"), TEXT("_"));
		return FString::Printf(TEXT("%s.%s"), Prefix, *Suffix);
	}

	void SHansaContextInspector::RebuildFlows(const FHansaInspectorSnapshot& Snapshot)
	{
		FlowRows->ClearChildren();
		for (const FHansaInspectorFlowPresentation& Flow : Snapshot.Flows)
		{
			const FString Id = InstanceId(TEXT("Inspector.Flows.Item"), Flow.StableId);
			TSharedPtr<SBorder> Row;
			FlowRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SAssignNew(Row, SBorder).BorderImage(&WorkingBrush)
				.BorderBackgroundColor(Flow.bProblem ? UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::WarningAmber).CopyWithNewOpacity(0.18f) : FLinearColor::Transparent)
				.Padding(4.0f)
				[
					SNew(STextBlock).Text(FText::Format(LOCTEXT("FlowRow", "{0} · {1} · {2}"), Flow.Label, Flow.Value, Flow.State)).TextStyle(&LightBodyStyle).AutoWrapText(true)
				]
			];
			MapWidget(Id, Row);
		}
		if (Snapshot.Flows.IsEmpty()) FlowRows->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("NoFlows", "No inputs, outputs or needs for this selection.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)];
		PresentedFlows = Snapshot.Flows;
#if WITH_DEV_AUTOMATION_TESTS
		++StructureRebuildCount;
#endif
	}

	void SHansaContextInspector::RebuildActions(const FHansaInspectorSnapshot& Snapshot)
	{
		ActionRows->ClearChildren(); ActionButtons.Reset();
		FocusOrder = { TEXT("Inspector.Close"), TEXT("Inspector.Action.OpenCause") };
		for (const FHansaInspectorActionPresentation& Action : Snapshot.Actions)
		{
			const FString Id = Action.StableId.ToString();
			TSharedPtr<SButton> Button;
			const FText Tip = Action.bEnabled ? Action.ToolTip : Action.DisabledReason;
			ActionRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SAssignNew(Button, SButton).ButtonStyle(Action.bSelected ? &PrimaryButtonStyle : &SecondaryButtonStyle)
				.Text(Action.Label).ToolTipText(Tip).IsEnabled(Action.bEnabled).OnClicked(this, &SHansaContextInspector::Invoke, Action.StableId)
			];
			ActionButtons.Add(Id, Button); MapWidget(Id, Button); if (Action.bEnabled) FocusOrder.Add(Id);
		}
		PresentedActions = Snapshot.Actions;
#if WITH_DEV_AUTOMATION_TESTS
		++StructureRebuildCount;
#endif
	}

	void SHansaContextInspector::RebuildHistory(const FHansaInspectorSnapshot& Snapshot)
	{
		HistoryRows->ClearChildren();
		for (const FHansaInspectorHistoryPresentation& Entry : Snapshot.History)
		{
			const FString Id = InstanceId(TEXT("Inspector.History.Item"), Entry.StableId);
			TSharedPtr<STextBlock> Row;
			HistoryRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)[SAssignNew(Row, STextBlock).Text(FText::Format(LOCTEXT("HistoryRow", "◷ {0} · {1}"), Entry.Label, Entry.Age)).TextStyle(&LightCaptionStyle).AutoWrapText(true)];
			MapWidget(Id, Row);
		}
		PresentedHistory = Snapshot.History;
#if WITH_DEV_AUTOMATION_TESTS
		++StructureRebuildCount;
#endif
	}

	void SHansaContextInspector::Refresh(const FHansaInspectorSnapshot& Snapshot, const uint64 Revision)
	{
		PresentedRevision = Revision; RootWidget->SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		IdentityText->SetText(Snapshot.Identity); StateText->SetText(Snapshot.State); ResultText->SetText(Snapshot.PrimaryResult);
		ProblemText->SetText(Snapshot.Causal.Problem); CauseText->SetText(FText::Format(LOCTEXT("Cause", "Cause · {0}"), Snapshot.Causal.Cause));
		EvidenceText->SetText(FText::Format(LOCTEXT("Evidence", "Evidence · {0}"), Snapshot.Causal.Evidence));
		RemedyText->SetText(FText::Format(LOCTEXT("Remedy", "Remedy · {0}"), Snapshot.Causal.Remedy));
		CauseWidget->SetVisibility(Snapshot.bCauseExpanded ? EVisibility::Visible : EVisibility::Collapsed);
		const EHansaUiSeverity Severity = Snapshot.Causal.Severity == EHansaCausalSeverity::Critical ? EHansaUiSeverity::Critical :
			(Snapshot.Causal.Severity == EHansaCausalSeverity::Warning ? EHansaUiSeverity::Warning : EHansaUiSeverity::Notice);
		ProblemWidget->SetBorderImage(Snapshot.Causal.Severity == EHansaCausalSeverity::Critical ? &CriticalBrush : &DecisionBrush);
		ProblemWidget->SetBorderBackgroundColor(UHansaUiStyleLibrary::GetSeverityStyle(Severity).AccentColor.CopyWithNewOpacity(0.20f));
		ResultStatusText->SetText(Snapshot.LastActionResult);
		if (!bStructureInitialized || PresentedFlows != Snapshot.Flows) RebuildFlows(Snapshot);
		if (!bStructureInitialized || PresentedActions != Snapshot.Actions) RebuildActions(Snapshot);
		if (!bStructureInitialized || PresentedHistory != Snapshot.History) RebuildHistory(Snapshot);
		bStructureInitialized = true;
	}

	FReply SHansaContextInspector::Invoke(const FName SemanticId)
	{
		return Model.IsValid() && Model->ActivateAction(SemanticId) ? FReply::Handled() : FReply::Unhandled();
	}

	void SHansaContextInspector::MapWidget(const FString& SemanticId, const TSharedPtr<SWidget>& Widget)
	{
		SemanticWidgets.Add(SemanticId, Widget);
	}

	bool SHansaContextInspector::ActivateSemanticId(const FString& SemanticId)
	{
		return Model.IsValid() && Model->ActivateAction(FName(*SemanticId));
	}

	bool SHansaContextInspector::FocusSemanticId(const FString& SemanticId)
	{
		const TWeakPtr<SWidget>* Found = SemanticWidgets.Find(SemanticId);
		const TSharedPtr<SWidget> Widget = Found ? Found->Pin() : nullptr;
		if (!Widget.IsValid() || !Widget->IsEnabled()) return false;
		if (UHansaInspectorPresentationModel* Pinned = Model.Get()) Pinned->SetFocusedSemanticId(FName(*SemanticId));
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
		return true;
	}

	FReply SHansaContextInspector::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) return Invoke(TEXT("Inspector.Close"));
		if (Key == EKeys::F) return Invoke(TEXT("Inspector.Action.Frame"));
		if (Key == EKeys::C) return Invoke(TEXT("Inspector.Action.OpenRelated"));
		if (Key == EKeys::P) return Invoke(TEXT("Inspector.Action.Pin"));
		return FReply::Unhandled();
	}

	TArray<FHansaHudSemanticNode> SHansaContextInspector::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Nodes;
		const UHansaInspectorPresentationModel* Pinned = Model.Get(); if (Pinned == nullptr) return Nodes;
		const FHansaInspectorSnapshot& S = Pinned->GetSnapshot();
		auto Add = [this, &Nodes, &S](const FString& Id, const TCHAR* Parent, const FString& Label, const EHansaHudSemanticRole Role,
			const bool bActivate = false, const bool bFocus = false, const FString& Type = FString(), const FString& Value = FString(),
			const bool bSelected = false, const bool bEnabled = true, const bool bWarning = false, const bool bError = false)
		{
			FHansaHudSemanticNode N; N.Id = Id; N.ParentId = Parent; N.Label = Label; N.Role = Role; N.bCanActivate = bActivate; N.bCanFocus = bFocus;
			N.State.bVisible = S.bOpen; N.State.bEnabled = bEnabled; N.State.bSelected = bSelected; N.State.bFocused = S.FocusedSemanticId == FName(*Id);
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
		Add(TEXT("Inspector.Identity"), TEXT("Inspector.Root"), S.Identity.ToString(), EHansaHudSemanticRole::Status, false, false, TEXT("state"), S.State.ToString());
		Add(TEXT("Inspector.Result"), TEXT("Inspector.Root"), TEXT("Most important result"), EHansaHudSemanticRole::Status, false, false, TEXT("text"), S.PrimaryResult.ToString());
		Add(TEXT("Inspector.Flows"), TEXT("Inspector.Root"), S.Kind == EHansaInspectorObjectKind::Residence ? TEXT("Needs") : TEXT("Inputs and outputs"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(S.Flows.Num()));
		for (const FHansaInspectorFlowPresentation& Flow : S.Flows) Add(InstanceId(TEXT("Inspector.Flows.Item"), Flow.StableId), TEXT("Inspector.Flows"), Flow.Label.ToString(), EHansaHudSemanticRole::Status, false, false, TEXT("flow"), FString::Printf(TEXT("value=%s;state=%s"), *Flow.Value.ToString(), *Flow.State.ToString()), false, true, Flow.bProblem);
		Add(TEXT("Inspector.Problem"), TEXT("Inspector.Root"), S.Causal.Problem.ToString(), EHansaHudSemanticRole::Alert, false, false, TEXT("cause-code"), S.Causal.StableCode.ToString(), false, true, bWarning, bCritical);
		Add(TEXT("Inspector.Problem.Cause"), TEXT("Inspector.Problem"), S.Causal.Cause.ToString(), EHansaHudSemanticRole::Text, false, true, TEXT("causal-explanation"), FString::Printf(TEXT("cause=%s;evidence=%s;remedy=%s;related=%s"), *S.Causal.Cause.ToString(), *S.Causal.Evidence.ToString(), *S.Causal.Remedy.ToString(), *S.Causal.RelatedSemanticId.ToString()), S.bCauseExpanded, true, bWarning, bCritical);
		Add(TEXT("Inspector.Actions"), TEXT("Inspector.Root"), TEXT("Actions and automation"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(S.Actions.Num()));
		Add(TEXT("Inspector.Action.OpenCause"), TEXT("Inspector.Actions"), TEXT("Open cause"), EHansaHudSemanticRole::Button, true, true, TEXT("expanded"), S.bCauseExpanded ? TEXT("true") : TEXT("false"), S.bCauseExpanded);
		for (const FHansaInspectorActionPresentation& Action : S.Actions) Add(Action.StableId.ToString(), TEXT("Inspector.Actions"), Action.Label.ToString(), EHansaHudSemanticRole::Button, true, true, TEXT("action-state"), Action.bEnabled ? TEXT("available") : Action.DisabledReason.ToString(), Action.bSelected, Action.bEnabled);
		Add(TEXT("Inspector.History"), TEXT("Inspector.Root"), TEXT("History"), EHansaHudSemanticRole::Panel, false, false, TEXT("count"), FString::FromInt(S.History.Num()));
		for (const FHansaInspectorHistoryPresentation& Entry : S.History) Add(InstanceId(TEXT("Inspector.History.Item"), Entry.StableId), TEXT("Inspector.History"), Entry.Label.ToString(), EHansaHudSemanticRole::Status, false, false, TEXT("age"), Entry.Age.ToString());
		Add(TEXT("Inspector.Close"), TEXT("Inspector.Root"), TEXT("Close inspector"), EHansaHudSemanticRole::Button, true, true);
		Add(TEXT("Inspector.Tooltip.OpenCause"), TEXT("HUD.TooltipLayer"), TEXT("Open cause tooltip"), EHansaHudSemanticRole::Status, false, false, TEXT("tooltip"), FString::Printf(TEXT("shortcut=C;cause=%s;remedy=%s"), *S.Causal.Cause.ToString(), *S.Causal.Remedy.ToString()));
		return Nodes;
	}
}

#undef LOCTEXT_NAMESPACE
