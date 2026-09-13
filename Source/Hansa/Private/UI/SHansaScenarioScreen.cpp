#include "UI/SHansaScenarioScreen.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "UI/HansaUiComponents.h"
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

    void SHansaScenarioScreen::SetPresentationSize(FIntPoint Size){
        AvailableSize=Size;if(!PanelSize)return;
        const bool Intro=Model.IsValid()&&Model->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing;
        const bool Pause=Model.IsValid()&&Model->GetSnapshot().bPauseMenu;
        const float Width=Intro||Pause?800.f:1120.f;
        const float Height=Intro?(Preferences.bLargeText?500.f:420.f):Pause?(Preferences.bLargeText?560.f:480.f):680.f;
        PanelSize->SetWidthOverride(FMath::Min(Width,float(Size.X)));PanelSize->SetHeightOverride(FMath::Min(Height,float(Size.Y)));
    }
    void SHansaScenarioScreen::Construct(const FArguments& Arguments)
    {
        Model=Arguments._Model;Preferences=Arguments._Preferences;
        HeaderBrush=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
        DecisionBrush=GetComponentStyle(EUiSurface::Modal,EUiState::Default,Preferences).Brush;
        WorkingBrush=GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
        HeadingStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Heading1,false);HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences));
        BodyStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Body,false);BodyStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Body,Preferences));
        DataStyle=UHansaUiStyleLibrary::GetTextStyle(EHansaUiTypographyToken::Data,false);DataStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
        ProgressStyle.SetBackgroundImage(*FCoreStyle::Get().GetBrush("WhiteBrush"));ProgressStyle.BackgroundImage.TintColor=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment);
        ProgressStyle.SetFillImage(*FCoreStyle::Get().GetBrush("WhiteBrush"));ProgressStyle.FillImage.TintColor=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::ProsperityTeal);ProgressStyle.EnableFillAnimation=false;
        ChildSlot[SAssignNew(PanelSize,SBox).WidthOverride(1120).HeightOverride(680)
        [SAssignNew(RootWidget,SBorder).BorderImage(&DecisionBrush).Padding(16)
        [SNew(SVerticalBox)
         +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&HeaderBrush).Padding(12)[SNew(SHorizontalBox)
           +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(TitleText,STextBlock).TextStyle(&HeadingStyle).AutoWrapText(true).ColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk))]
           +SHorizontalBox::Slot().AutoWidth().Padding(12,0,0,0)[SAssignNew(CloseButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Icon).Label(LOCTEXT("Close","Close")).OnClicked(this,&SHansaScenarioScreen::HandleClose)]]]
         +SVerticalBox::Slot().FillHeight(1).Padding(0,12)[SAssignNew(ContentScroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
           +SVerticalBox::Slot().AutoHeight()[SAssignNew(StateText,STextBlock).TextStyle(&DataStyle).AutoWrapText(true)]
           +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(BriefingText,STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]
           +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(OutcomeText,STextBlock).TextStyle(&BodyStyle).AutoWrapText(true)]
           +SVerticalBox::Slot().AutoHeight()[SAssignNew(MenuRows,SVerticalBox)]
           +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(PathRows,SHorizontalBox)]
           +SVerticalBox::Slot().AutoHeight()[SAssignNew(ObjectiveRows,SVerticalBox)]]]
         +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
           +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(BeginButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Primary).Label(LOCTEXT("Begin","Begin")).OnClicked(this,&SHansaScenarioScreen::HandleBegin)]
           +SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0)[SAssignNew(SaveButton,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(LOCTEXT("SaveLoad","Save / load")).OnClicked_Lambda([this]{if(auto* P=Model.Get())P->RequestSaveLoad();return FReply::Handled();})]]]]];
        SemanticWidgets.Add(TEXT("Scenario.Root"),RootWidget);SemanticWidgets.Add(TEXT("Scenario.Content"),ContentScroll);
        SemanticWidgets.Add(TEXT("Scenario.Close"),CloseButton);SemanticWidgets.Add(TEXT("Scenario.Begin"),BeginButton);SemanticWidgets.Add(TEXT("Scenario.Resume"),BeginButton);SemanticWidgets.Add(TEXT("Scenario.SaveLoad"),SaveButton);SemanticWidgets.Add(TEXT("Scenario.Outcome"),OutcomeText);
        if(auto* P=Model.Get()){ChangedHandle=P->OnChanged().AddSP(SharedThis(this),&SHansaScenarioScreen::Refresh);Refresh(P->GetSnapshot(),P->GetRevision());}
    }

    void SHansaScenarioScreen::Refresh(const FHansaScenarioPresentationSnapshot& S,uint64 Revision)
    {
        PresentedRevision=Revision;SetVisibility(S.bOpen?EVisibility::Visible:EVisibility::Collapsed);
        FString Key=S.SelectedVictoryId.ToString()+FString::FromInt(int32(S.Phase))+FString::FromInt(S.bPauseMenu)+FString::FromInt(S.bHelpEnabled)+FString::FromInt(S.bReady)+FString::FromInt(S.bLoadedSession)+S.OutcomeExplanation.ToString();
        for(const auto& P:S.Paths){Key+=P.StableId.ToString()+P.Label.ToString()+P.Summary.ToString()+P.SustainProgress.ToString();for(const auto& O:P.Objectives)Key+=O.Label.ToString()+O.Progress.ToString()+O.Status.ToString();}
        if(Key==PresentedContentKey)return;PresentedContentKey=MoveTemp(Key);SetPresentationSize(AvailableSize);
        const auto Previous=ResolveSemanticWidget(S.FocusedSemanticId.ToString());const bool Restore=Previous&&Previous->HasKeyboardFocus();
        const bool Intro=S.Phase==EHansaScenarioPresentationPhase::Briefing;
        const bool Outcome=S.Phase==EHansaScenarioPresentationPhase::Victory||S.Phase==EHansaScenarioPresentationPhase::Failure;
        TitleText->SetText(S.bPauseMenu?LOCTEXT("PausedTitle","Game paused"):Intro?LOCTEXT("Welcome","Welcome to Lübeck"):S.Title);
        StateText->SetText(S.bPauseMenu?(S.bLoadedSession?LOCTEXT("Loaded","Game loaded — paused. Resume when you are ready."):LOCTEXT("Paused","Your city is waiting.")):S.StateLabel);
        BriefingText->SetText(Intro?LOCTEXT("Opening","Your merchant house has a foothold in Lübeck. Build reliable supply, understand the market and connect the Baltic ports. Choose your own balance of production, trade and research. You can pause at any time; optional tips explain the tools without choosing an economic solution for you."):S.bPauseMenu?LOCTEXT("PauseContext","Resume your session, review prosperity, or save and return later. Contextual help can be hidden at any time."):LOCTEXT("ProgressContext","These are your house’s current goals and sustained progress. Select a path to inspect its measures; you decide how to reach them. Scroll with the wheel, Page Up/Down or controller shoulder buttons."));
        OutcomeText->SetText(Outcome?FText::Format(LOCTEXT("OutcomeRecovery","{0}\n{1}"),S.OutcomeExplanation,S.Phase==EHansaScenarioPresentationPhase::Failure?LOCTEXT("FailureRecovery","Review the city to understand what happened, or load a compatible earlier save. Loading replaces the current session only after confirmation."):LOCTEXT("VictoryRecovery","Your house has achieved this path. Review the final measures, save the result, or return to the city.")):FText::GetEmpty());
        OutcomeText->SetVisibility(Outcome&&!S.bPauseMenu?EVisibility::Visible:EVisibility::Collapsed);
        StaticCastSharedPtr<SHansaAction>(BeginButton)->SetLabel(Intro?LOCTEXT("BeginCity","Begin your city"):S.bPauseMenu?LOCTEXT("Resume","Resume"):LOCTEXT("ReturnCity","Return to city"));
        StaticCastSharedPtr<SHansaAction>(BeginButton)->SetState(S.bReady?EUiState::Default:EUiState::Disabled);
        MenuRows->ClearChildren();
        for(auto It=SemanticWidgets.CreateIterator();It;++It)if(It.Key().StartsWith(TEXT("Scenario.Help."))||It.Key()==TEXT("Scenario.Progress"))It.RemoveCurrent();
        auto Action=[this](const TCHAR* Id,FText Label,TFunction<void()> Invoke){TSharedPtr<SHansaAction> B;MenuRows->AddSlot().AutoHeight().Padding(0,4)[SAssignNew(B,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary).Label(Label).OnClicked_Lambda([Invoke]{Invoke();return FReply::Handled();})];SemanticWidgets.Add(Id,B);};
        if(S.bPauseMenu){Action(TEXT("Scenario.Settings"),LOCTEXT("Settings","Settings"),[this]{if(auto* P=Model.Get())P->RequestSystemAction(TEXT("Settings"));});Action(TEXT("Scenario.ReturnTitle"),LOCTEXT("ReturnTitle","Return to title"),[this]{if(auto* P=Model.Get())P->RequestSystemAction(TEXT("ReturnTitle"));});}
        if(S.bPauseMenu)Action(TEXT("Scenario.Progress"),LOCTEXT("Review","Review prosperity"),[this]{if(auto* P=Model.Get())P->ReviewProgress();});
        if(Intro||S.bPauseMenu){
            Action(TEXT("Scenario.Help.Toggle"),S.bHelpEnabled?LOCTEXT("HelpOn","Contextual help: on"):LOCTEXT("HelpOff","Contextual help: off"),[this]{if(auto* P=Model.Get())P->ToggleHelp();});
            if(S.bPauseMenu)Action(TEXT("Scenario.Help.Reset"),LOCTEXT("ResetHelp","Show dismissed tips again"),[this]{if(auto* P=Model.Get())P->ResetHelp();});
        }
        const bool ShowProgress=!Intro&&!S.bPauseMenu;
        PathRows->SetVisibility(ShowProgress?EVisibility::Visible:EVisibility::Collapsed);ObjectiveRows->SetVisibility(ShowProgress?EVisibility::Visible:EVisibility::Collapsed);
        RebuildPaths(S);RebuildObjectives(S);
        for(const auto& Pair:SemanticWidgets)if(Pair.Key!=TEXT("Scenario.Root")&&Pair.Key!=TEXT("Scenario.Content")&&Pair.Key!=TEXT("Scenario.Outcome")&&!Pair.Key.StartsWith(TEXT("Scenario.Objective."))){
            const FName Id(Pair.Key==TEXT("Scenario.Begin")||Pair.Key==TEXT("Scenario.Resume")?(Intro?TEXT("Scenario.Begin"):TEXT("Scenario.Resume")):*Pair.Key);
            if(auto W=Pair.Value.Pin())StaticCastSharedPtr<SHansaAction>(W)->SetFocusHandler(FSimpleDelegate::CreateLambda([Weak=Model,Id]{if(auto* P=Weak.Get())P->SetFocusedSemanticId(Id);}));
        }
        if(Restore&&!FocusSemanticId(S.FocusedSemanticId.ToString()))FocusSemanticId(Intro?TEXT("Scenario.Begin"):TEXT("Scenario.Resume"));
    }

	void SHansaScenarioScreen::RebuildPaths(const FHansaScenarioPresentationSnapshot& Snapshot)
	{
		PathRows->ClearChildren();
		for (const FHansaVictoryPathPresentation& Path : Snapshot.Paths)
		{
			FString Id = Path.StableId.ToString(); Id.ReplaceInline(TEXT("."), TEXT("_"));
			const FString SemanticId = TEXT("Scenario.Path.") + Id;
			TSharedPtr<SButton> Button;
            PathRows->AddSlot().FillWidth(1).Padding(4,0)
            [SAssignNew(Button,SHansaAction).Preferences(Preferences).Kind(EHansaUiButtonStyle::Secondary)
             .State(Path.StableId==Snapshot.SelectedVictoryId?EUiState::Selected:EUiState::Default)
             .OnClicked(this,&SHansaScenarioScreen::HandleSelectPath,Path.StableId)
             [SNew(SVerticalBox)
              +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Path.Label).TextStyle(&BodyStyle).AutoWrapText(true)]
              +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(STextBlock).Text(Path.bVictorious?LOCTEXT("Won","Victorious"):Path.bAllObjectivesMet?LOCTEXT("Sustaining","Sustaining"):LOCTEXT("Available","Available")).TextStyle(&DataStyle).AutoWrapText(true)]
              +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Path.SustainProgress).TextStyle(&DataStyle).AutoWrapText(true)]]];

			SemanticWidgets.Add(SemanticId, Button);
			StaticCastSharedPtr<SHansaAction>(Button)->SetFocusHandler(FSimpleDelegate::CreateLambda([WeakModel=Model,SemanticId]{if(auto* P=WeakModel.Get())P->SetFocusedSemanticId(FName(*SemanticId));}));
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
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Objective.Label).TextStyle(&BodyStyle).AutoWrapText(true)]
                    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::Format(LOCTEXT("Measure","{0} · {1}"),Objective.Progress,Objective.Status)).TextStyle(&DataStyle).AutoWrapText(true)]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)[SNew(SBox).HeightOverride(6)[SNew(SProgressBar).Style(&ProgressStyle).Percent(Objective.Ratio).FillColorAndOpacity(FLinearColor::White)]]
				]
			];
			SemanticWidgets.Add(SemanticId, Row);
		}
	}

	FReply SHansaScenarioScreen::HandleClose() { if (UHansaScenarioPresentationModel* Pinned=Model.Get()) Pinned->Close(); return FReply::Handled(); }
	FReply SHansaScenarioScreen::HandleBegin() { if (auto* P=Model.Get()){if(P->GetSnapshot().Phase==EHansaScenarioPresentationPhase::Briefing)P->AcknowledgeBriefing();else P->Close();} return FReply::Handled(); }
	FReply SHansaScenarioScreen::HandleSelectPath(const FName VictoryId) { if (UHansaScenarioPresentationModel* Pinned=Model.Get()) Pinned->SelectPath(VictoryId); return FReply::Handled(); }

	bool SHansaScenarioScreen::ActivateSemanticId(const FString& Id)
	{
		if(Id==TEXT("Scenario.Settings")||Id==TEXT("Scenario.ReturnTitle")){if(auto* P=Model.Get())P->RequestSystemAction(Id.EndsWith(TEXT("Settings"))?TEXT("Settings"):TEXT("ReturnTitle"));return true;}
		if (Id == TEXT("Scenario.Close")) return HandleClose().IsEventHandled();
		if (Id == TEXT("Scenario.Begin")||Id==TEXT("Scenario.Resume")) return HandleBegin().IsEventHandled();
        if(auto* P=Model.Get()){
            if(Id==TEXT("Scenario.SaveLoad")){P->RequestSaveLoad();return true;}
            if(Id==TEXT("Scenario.Progress")){P->ReviewProgress();return true;}
            if(Id==TEXT("Scenario.Help.Toggle")){P->ToggleHelp();return true;}
            if(Id==TEXT("Scenario.Help.Reset")){P->ResetHelp();return true;}
        }
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
		if(ContentScroll && (Id.StartsWith(TEXT("Scenario.Path."))||Id.StartsWith(TEXT("Scenario.Help."))||Id==TEXT("Scenario.Progress")))ContentScroll->ScrollDescendantIntoView(Widget,false,EDescendantScrollDestination::IntoView);
		if(FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);
		return true;
	}

	FReply SHansaScenarioScreen::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		(void)MyGeometry;
        const FKey Key=InKeyEvent.GetKey();
        if(Key==EKeys::PageDown||Key==EKeys::Gamepad_RightShoulder||Key==EKeys::PageUp||Key==EKeys::Gamepad_LeftShoulder){ContentScroll->SetScrollOffset(FMath::Max(0.f,ContentScroll->GetScrollOffset()+((Key==EKeys::PageDown||Key==EKeys::Gamepad_RightShoulder)?220.f:-220.f)));return FReply::Handled();}
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
        TArray<FString> R={TEXT("Scenario.Close")};if(!Model.IsValid())return R;
        const auto& S=Model->GetSnapshot();
        if(S.bReady)R.Add(S.Phase==EHansaScenarioPresentationPhase::Briefing?TEXT("Scenario.Begin"):TEXT("Scenario.Resume"));
        R.Add(TEXT("Scenario.SaveLoad"));
        if(S.bPauseMenu){R.Add(TEXT("Scenario.Progress"));R.Add(TEXT("Scenario.Settings"));R.Add(TEXT("Scenario.ReturnTitle"));}
        if(S.bPauseMenu||S.Phase==EHansaScenarioPresentationPhase::Briefing){R.Add(TEXT("Scenario.Help.Toggle"));if(S.bPauseMenu)R.Add(TEXT("Scenario.Help.Reset"));}
        else for(const auto& P:S.Paths){FString Id=P.StableId.ToString();Id.ReplaceInline(TEXT("."),TEXT("_"));R.Add(TEXT("Scenario.Path.")+Id);}
        return R;
    }

	TArray<FHansaHudSemanticNode> SHansaScenarioScreen::GetSemanticSnapshot() const
	{
		TArray<FHansaHudSemanticNode> Result; const UHansaScenarioPresentationModel* Pinned=Model.Get(); if(!Pinned) return Result; const auto& S=Pinned->GetSnapshot();
		auto Add=[&](FString Id,FString Parent,FString Label,EHansaHudSemanticRole Role,bool Activate,bool Focus,FString Type,FString Value,bool Selected,bool Warning,bool Error){FHansaHudSemanticNode N;N.Id=MoveTemp(Id);N.ParentId=MoveTemp(Parent);N.Label=MoveTemp(Label);N.Role=Role;N.bCanActivate=Activate;N.bCanFocus=Focus;N.State.bVisible=S.bOpen;N.State.bEnabled=true;N.State.bFocused=S.FocusedSemanticId==FName(*N.Id);N.State.bSelected=Selected;N.State.bWarning=Warning;N.State.bError=Error;N.State.ValueType=MoveTemp(Type);N.State.Value=MoveTemp(Value);Result.Add(MoveTemp(N));};
		Add(TEXT("Scenario.Root"),TEXT("HUD.Root"),S.Title.ToString(),EHansaHudSemanticRole::Screen,false,false,TEXT("scenario-state"),S.StateLabel.ToString(),false,S.Phase==EHansaScenarioPresentationPhase::Failure,S.Phase==EHansaScenarioPresentationPhase::Error);
		Add(TEXT("Scenario.Close"),TEXT("Scenario.Root"),TEXT("Close scenario"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("close"),false,false,false);
		if(S.Phase==EHansaScenarioPresentationPhase::Briefing) Add(TEXT("Scenario.Begin"),TEXT("Scenario.Root"),TEXT("Begin scenario"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("begin"),false,false,false);
        if(S.Phase!=EHansaScenarioPresentationPhase::Briefing)Add(TEXT("Scenario.Resume"),TEXT("Scenario.Root"),TEXT("Resume or return to city"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("resume"),false,false,false);
        Add(TEXT("Scenario.SaveLoad"),TEXT("Scenario.Root"),TEXT("Save / load"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("save-load"),false,false,false);
        if(S.bPauseMenu){Add(TEXT("Scenario.Settings"),TEXT("Scenario.Root"),TEXT("Settings"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("settings"),false,false,false);Add(TEXT("Scenario.ReturnTitle"),TEXT("Scenario.Root"),TEXT("Return to title"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("return-title"),false,false,false);}
        if(S.bPauseMenu)Add(TEXT("Scenario.Progress"),TEXT("Scenario.Root"),TEXT("Review prosperity"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("progress"),false,false,false);
        if(S.bPauseMenu||S.Phase==EHansaScenarioPresentationPhase::Briefing){Add(TEXT("Scenario.Help.Toggle"),TEXT("Scenario.Root"),TEXT("Contextual help"),EHansaHudSemanticRole::Button,true,true,TEXT("enabled"),S.bHelpEnabled?TEXT("on"):TEXT("off"),S.bHelpEnabled,false,false);if(S.bPauseMenu)Add(TEXT("Scenario.Help.Reset"),TEXT("Scenario.Root"),TEXT("Show dismissed tips again"),EHansaHudSemanticRole::Button,true,true,TEXT("action"),TEXT("reset-tips"),false,false,false);}
		for(const auto& Path:S.Paths){FString Safe=Path.StableId.ToString();Safe.ReplaceInline(TEXT("."),TEXT("_"));const FString Pid=TEXT("Scenario.Path.")+Safe;Add(Pid,TEXT("Scenario.Root"),Path.Label.ToString(),EHansaHudSemanticRole::Button,true,true,TEXT("victory-path"),Path.SustainProgress.ToString(),Path.StableId==S.SelectedVictoryId,!Path.bAllObjectivesMet,false);if(Path.StableId==S.SelectedVictoryId)for(const auto& O:Path.Objectives){FString OSafe=O.StableId.ToString();OSafe.ReplaceInline(TEXT("."),TEXT("_"));Add(TEXT("Scenario.Objective.")+OSafe,Pid,O.Label.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("objective-progress"),O.Progress.ToString()+TEXT(";")+O.Status.ToString(),O.bMet,!O.bMet,false);}}
		Add(TEXT("Scenario.Outcome"),TEXT("Scenario.Root"),S.StateLabel.ToString(),EHansaHudSemanticRole::Status,false,false,TEXT("outcome"),S.OutcomeExplanation.ToString(),S.Phase==EHansaScenarioPresentationPhase::Victory,S.Phase==EHansaScenarioPresentationPhase::Failure,false);
        for(auto& N:Result){
            if(N.Id.StartsWith(TEXT("Scenario.Path."))||N.Id.StartsWith(TEXT("Scenario.Objective.")))N.State.bVisible&=!S.bPauseMenu&&S.Phase!=EHansaScenarioPresentationPhase::Briefing;
            if(auto W=ResolveSemanticWidget(N.Id)){
                N.State.bEnabled=W->IsEnabled();N.bCanActivate&=N.State.bEnabled;N.bCanFocus&=N.State.bEnabled;
                const auto G=W->GetCachedGeometry();FVector2D A=G.GetAbsolutePosition(),B=A+G.GetAbsoluteSize();
                if(N.Id.StartsWith(TEXT("Scenario.Path."))||N.Id.StartsWith(TEXT("Scenario.Objective."))||N.Id.StartsWith(TEXT("Scenario.Help."))||N.Id==TEXT("Scenario.Progress")||N.Id==TEXT("Scenario.Outcome")){
                    const auto C=ContentScroll->GetCachedGeometry();const auto Min=C.GetAbsolutePosition(),Max=Min+C.GetAbsoluteSize();A.X=FMath::Max(A.X,Min.X);A.Y=FMath::Max(A.Y,Min.Y);B.X=FMath::Min(B.X,Max.X);B.Y=FMath::Min(B.Y,Max.Y);
                }
                N.State.bVisible&=W->GetVisibility().IsVisible()&&B.X>A.X&&B.Y>A.Y;
                if(N.State.bVisible)N.Bounds=FIntRect(FMath::RoundToInt(A.X),FMath::RoundToInt(A.Y),FMath::RoundToInt(B.X),FMath::RoundToInt(B.Y));
            }
        }
		return Result;
	}
}

#undef LOCTEXT_NAMESPACE
