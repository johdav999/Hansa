#include "UI/SHansaScenarioScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiNavigation.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SHansaScenarioScreen"

namespace Hansa::UI
{
	SHansaScenarioScreen::~SHansaScenarioScreen()
	{
		if (UHansaScenarioPresentationModel* Pinned = Model.Get()) Pinned->OnChanged().Remove(ChangedHandle);
	}

	void SHansaScenarioScreen::Construct(const FArguments& Arguments)
	{
		Model = Arguments._Model;
		ScrimBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::WorldOverlay);
		DecisionBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		WorkingBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working);
		SuccessBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Decision);
		CriticalBrush = UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Critical);
		PrimaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary);
		SecondaryButtonStyle = UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Secondary);
		HeadingStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1, false);
		BodyStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body, false);
		DataStyle = UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data, false);
		SetVisibility(EVisibility::Visible);
		ChildSlot
		[
			SAssignNew(RootWidget, SBorder).BorderImage(&ScrimBrush).Padding(24.0f)
			[
				SNew(SBox).WidthOverride(1120.0f).MaxDesiredHeight(760.0f)
				[
					SNew(SBorder).BorderImage(&DecisionBrush).Padding(24.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f)[SAssignNew(TitleText, STextBlock).TextStyle(&HeadingStyle)]
							+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(StateText, STextBlock).TextStyle(&DataStyle)]
							+ SHorizontalBox::Slot().AutoWidth().Padding(16.0f, 0.0f)[SAssignNew(CloseButton, SButton).ButtonStyle(&SecondaryButtonStyle).Text(LOCTEXT("Close", "Close")).OnClicked(this, &SHansaScenarioScreen::HandleClose)]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)[SAssignNew(BriefingText, STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)[SAssignNew(PathRows, SVerticalBox)]
						+ SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 8.0f)
						[
							SNew(SBorder).BorderImage(&WorkingBrush).Padding(16.0f)
							[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(ObjectiveRows, SVerticalBox)]]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)[SAssignNew(OutcomeText, STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]
						+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 12.0f, 0.0f, 0.0f)
						[SAssignNew(BeginButton, SButton).ButtonStyle(&PrimaryButtonStyle).Text(LOCTEXT("Begin", "Begin scenario")).OnClicked(this, &SHansaScenarioScreen::HandleBegin)]
					]
				]
			]
		];
		SemanticWidgets.Add(TEXT("Scenario.Root"), RootWidget);
		SemanticWidgets.Add(TEXT("Scenario.Close"), CloseButton);
		SemanticWidgets.Add(TEXT("Scenario.Begin"), BeginButton);
		if (UHansaScenarioPresentationModel* Pinned = Model.Get())
		{
			ChangedHandle = Pinned->OnChanged().AddSP(SharedThis(this), &SHansaScenarioScreen::Refresh);
			Refresh(Pinned->GetSnapshot(), Pinned->GetRevision());
		}
	}

	void SHansaScenarioScreen::Refresh(const FHansaScenarioPresentationSnapshot& Snapshot, const uint64 Revision)
	{
		PresentedRevision = Revision;
		SetVisibility(Snapshot.bOpen ? EVisibility::Visible : EVisibility::Collapsed);
		TitleText->SetText(Snapshot.Title);
		StateText->SetText(Snapshot.StateLabel);
		BriefingText->SetText(Snapshot.Briefing);
		const bool bOutcome = Snapshot.Phase == EHansaScenarioPresentationPhase::Victory || Snapshot.Phase == EHansaScenarioPresentationPhase::Failure;
		OutcomeText->SetText(bOutcome ? Snapshot.OutcomeExplanation : FText::GetEmpty());
		OutcomeText->SetVisibility(bOutcome ? EVisibility::Visible : EVisibility::Collapsed);
		BeginButton->SetVisibility(Snapshot.Phase == EHansaScenarioPresentationPhase::Briefing ? EVisibility::Visible : EVisibility::Collapsed);
		RebuildPaths(Snapshot);
		RebuildObjectives(Snapshot);
	}

	void SHansaScenarioScreen::RebuildPaths(const FHansaScenarioPresentationSnapshot& Snapshot)
	{
		PathRows->ClearChildren();
		for (const FHansaVictoryPathPresentation& Path : Snapshot.Paths)
		{
			FString Id = Path.StableId.ToString(); Id.ReplaceInline(TEXT("."), TEXT("_"));
			const FString SemanticId = TEXT("Scenario.Path.") + Id;
			TSharedPtr<SButton> Button;
			PathRows->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SAssignNew(Button, SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked(this, &SHansaScenarioScreen::HandleSelectPath, Path.StableId)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(Path.Label).TextStyle(&BodyStyle)]
					+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(Path.bVictorious ? LOCTEXT("Won", "✓ Victorious") : Path.bAllObjectivesMet ? LOCTEXT("Sustaining", "◉ Sustaining") : LOCTEXT("Available", "○ Available")).TextStyle(&DataStyle)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(16.0f, 0.0f)[SNew(STextBlock).Text(Path.SustainProgress).TextStyle(&DataStyle)]
				]
			];
			SemanticWidgets.Add(SemanticId, Button);
		}
	}

	void SHansaScenarioScreen::RebuildObjectives(const FHansaScenarioPresentationSnapshot& Snapshot)
	{
		ObjectiveRows->ClearChildren();
		const FHansaVictoryPathPresentation* Selected = Snapshot.Paths.FindByPredicate(
			[&Snapshot](const auto& Path) { return Path.StableId == Snapshot.SelectedVictoryId; });
		if (Selected == nullptr) return;
		ObjectiveRows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[SNew(STextBlock).Text(Selected->Summary).TextStyle(&BodyStyle).AutoWrapText(true)];
		for (const FHansaScenarioObjectivePresentation& Objective : Selected->Objectives)
		{
			FString Id = Objective.StableId.ToString(); Id.ReplaceInline(TEXT("."), TEXT("_"));
			const FString SemanticId = TEXT("Scenario.Objective.") + Id;
			TSharedPtr<SBorder> Row;
			ObjectiveRows->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
			[
				SAssignNew(Row, SBorder).BorderImage(&WorkingBrush).Padding(FMargin(10.0f, 8.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(STextBlock).Text(Objective.Label).TextStyle(&BodyStyle)]
						+ SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(Objective.Progress).TextStyle(&DataStyle)]
						+ SHorizontalBox::Slot().AutoWidth().Padding(16.0f, 0.0f)[SNew(STextBlock).Text(Objective.Status).TextStyle(&DataStyle)]]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SNew(SProgressBar).Percent(Objective.Ratio)]
				]
			];
			SemanticWidgets.Add(SemanticId, Row);
		}
	}

	FReply SHansaScenarioScreen::HandleClose() { if (UHansaScenarioPresentationModel* Pinned=Model.Get()) Pinned->Close(); return FReply::Handled(); }
	FReply SHansaScenarioScreen::HandleBegin() { if (UHansaScenarioPresentationModel* Pinned=Model.Get()) Pinned->AcknowledgeBriefing(); return FReply::Handled(); }
	FReply SHansaScenarioScreen::HandleSelectPath(const FName VictoryId) { if (UHansaScenarioPresentationModel* Pinned=Model.Get()) Pinned->SelectPath(VictoryId); return FReply::Handled(); }

	bool SHansaScenarioScreen::ActivateSemanticId(const FString& Id)
	{
		if (Id == TEXT("Scenario.Close")) return HandleClose().IsEventHandled();
		if (Id == TEXT("Scenario.Begin")) return HandleBegin().IsEventHandled();
		if (Id.StartsWith(TEXT("Scenario.Path.")))
		{
			if (UHansaScenarioPresentationModel* Pinned=Model.Get()) for (const auto& Path : Pinned->GetSnapshot().Paths)
			{
				FString Safe=Path.StableId.ToString(); Safe.ReplaceInline(TEXT("."),TEXT("_"));
				if (Id == TEXT("Scenario.Path.")+Safe) return HandleSelectPath(Path.StableId).IsEventHandled();
			}
		}
		return false;
	}

	bool SHansaScenarioScreen::FocusSemanticId(const FString& Id)
	{
		const TWeakPtr<SWidget>* Found=SemanticWidgets.Find(Id); const TSharedPtr<SWidget> Widget=Found?Found->Pin():nullptr;
		if(!Widget.IsValid() || !Widget->IsEnabled() || !Widget->GetVisibility().IsVisible()) return false;
		if(UHansaScenarioPresentationModel* Pinned=Model.Get()) Pinned->SetFocusedSemanticId(FName(*Id));
		if(FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);
		return true;
	}

	FReply SHansaScenarioScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
		const EHansaUiNavigationIntent Intent = ClassifyNavigationIntent(InKeyEvent);
		if (Intent == EHansaUiNavigationIntent::Back) return HandleClose();
		UHansaScenarioPresentationModel* Pinned = Model.Get();
		if (Pinned == nullptr) return FReply::Unhandled();
		if (Intent == EHansaUiNavigationIntent::Activate)
		{
			return ActivateSemanticId(Pinned->GetSnapshot().FocusedSemanticId.ToString()) ? FReply::Handled() : FReply::Unhandled();
		}
		if (Intent == EHansaUiNavigationIntent::Next || Intent == EHansaUiNavigationIntent::Previous)
		{
			const FString Target = FindWrappedFocusTarget(GetControllerFocusOrder(), Pinned->GetSnapshot().FocusedSemanticId.ToString(), Intent == EHansaUiNavigationIntent::Next);
			return !Target.IsEmpty() && FocusSemanticId(Target) ? FReply::Handled() : FReply::Unhandled();
		}
		return FReply::Unhandled();
	}

	TArray<FString> SHansaScenarioScreen::GetControllerFocusOrder() const
	{
		TArray<FString> Result={TEXT("Scenario.Close")}; const UHansaScenarioPresentationModel* Pinned=Model.Get(); if(!Pinned) return Result;
		if(Pinned->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing) Result.Add(TEXT("Scenario.Begin"));
		for(const auto& Path:Pinned->GetSnapshot().Paths){FString Safe=Path.StableId.ToString();Safe.ReplaceInline(TEXT("."),TEXT("_"));Result.Add(TEXT("Scenario.Path.")+Safe);}
		return Result;
	}

	TArray<FHansaHudSemanticNode> SHansaScenarioScreen::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Result; const UHansaScenarioPresentationModel* Pinned=Model.Get(); if(!Pinned) return Result; const auto& S=Pinned->GetSnapshot();
		auto Add=[&](FString Id,FString Parent,FString Label,EHansaHudSemanticRole Role,bool Activate,bool Focus,FString Type,FString Value,bool Selected,bool Warning,bool Error){FHansaHudSemanticNode N;N.Id=MoveTemp(Id);N.ParentId=MoveTemp(Parent);N.Label=MoveTemp(Label);N.Role=Role;N.bCanActivate=Activate;N.bCanFocus=Focus;N.State.bVisible=S.bOpen;N.State.bEnabled=true;N.State.bFocused=S.FocusedSemanticId==FName(*N.Id);N.State.bSelected=Selected;N.State.bWarning=Warning;N.State.bError=Error;N.State.ValueType=MoveTemp(Type);N.State.Value=MoveTemp(Value);Result.Add(MoveTemp(N));};
		Add(TEXT("Scenario.Root"),TEXT("HUD.Root"),S.Title.ToString(),EHansaHudSemanticRole::Screen,false,false,TEXT("scenario-state"),S.StateLabel.ToString(),false,S.Phase==EHansaScenarioPresentationPhase::Failure,S.Phase==EHansaScenarioPresentationPhase::Error);
		Add(TEXT("Scenario.Close"),TEXT("Scenario.Root"),TEXT("Close scenario"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("close"),false,false,false);
		if(S.Phase==EHansaScenarioPresentationPhase::Briefing) Add(TEXT("Scenario.Begin"),TEXT("Scenario.Root"),TEXT("Begin scenario"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("begin"),false,false,false);
		for(const auto& Path:S.Paths){FString Safe=Path.StableId.ToString();Safe.ReplaceInline(TEXT("."),TEXT("_"));const FString Pid=TEXT("Scenario.Path.")+Safe;Add(Pid,TEXT("Scenario.Root"),Path.Label.ToString(),EHansaHudSemanticRole::Button,true,true,TEXT("victory-path"),Path.SustainProgress.ToString(),Path.StableId==S.SelectedVictoryId,!Path.bAllObjectivesMet,false);if(Path.StableId==S.SelectedVictoryId)for(const auto& O:Path.Objectives){FString OSafe=O.StableId.ToString();OSafe.ReplaceInline(TEXT("."),TEXT("_"));Add(TEXT("Scenario.Objective.")+OSafe,Pid,O.Label.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("objective-progress"),O.Progress.ToString()+TEXT(";")+O.Status.ToString(),O.bMet,!O.bMet,false);}}
		Add(TEXT("Scenario.Outcome"),TEXT("Scenario.Root"),S.StateLabel.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("outcome"),S.OutcomeExplanation.ToString(),S.Phase==EHansaScenarioPresentationPhase::Victory,S.Phase==EHansaScenarioPresentationPhase::Failure,false);
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
