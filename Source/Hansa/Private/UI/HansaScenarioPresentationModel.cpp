#include "UI/HansaScenarioPresentationModel.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "HansaScenarioPresentationModel"

void UHansaScenarioPresentationModel::InitializeDefaults()
{
	Snapshot = {};
	Snapshot.Title = LOCTEXT("LoadingTitle", "Scenario");
	Snapshot.Briefing = LOCTEXT("LoadingBriefing", "Preparing your city…");
	Snapshot.StateLabel = LOCTEXT("LoadingState", "Loading");
	Snapshot.Phase = EHansaScenarioPresentationPhase::Briefing;
	Snapshot.bOpen = true;
	Broadcast();
}

bool UHansaScenarioPresentationModel::ApplyProgress(const Hansa::Simulation::FHansaScenarioProgress& Progress)
{
	FHansaScenarioPresentationSnapshot Updated = Snapshot;
	Updated.ScenarioId = FName(*Progress.ScenarioId);
    Updated.bReady = !Progress.ScenarioId.IsEmpty();
	Updated.Title = FText::FromString(Progress.DisplayName);
	Updated.Briefing = FText::FromString(Progress.Briefing);
	Updated.Paths.Reset();
	for (const Hansa::Simulation::FHansaVictoryPathProgress& SourcePath : Progress.VictoryPaths)
	{
		FHansaVictoryPathPresentation Path;
		Path.StableId = FName(*SourcePath.VictoryId);
		Path.Label = FText::FromString(SourcePath.DisplayName);
		Path.Summary = FText::FromString(SourcePath.Summary);
		Path.SustainProgress = FText::Format(LOCTEXT("SustainProgress", "Sustained {0} / {1} ticks"),
			FText::AsNumber(SourcePath.ConsecutiveSatisfiedTicks), FText::AsNumber(SourcePath.RequiredSustainTicks));
		Path.SustainRatio = SourcePath.RequiredSustainTicks > 0
			? FMath::Clamp(static_cast<float>(SourcePath.ConsecutiveSatisfiedTicks) / SourcePath.RequiredSustainTicks, 0.0f, 1.0f) : 0.0f;
		Path.bAllObjectivesMet = SourcePath.bAllObjectivesMet;
		Path.bVictorious = SourcePath.bVictorious;
		for (const Hansa::Simulation::FHansaScenarioObjectiveProgress& SourceObjective : SourcePath.Objectives)
		{
			FHansaScenarioObjectivePresentation Objective;
			Objective.StableId = FName(*SourceObjective.ObjectiveId);
			Objective.Label = FText::FromString(SourceObjective.DisplayName);
			Objective.Progress = FText::Format(LOCTEXT("ObjectiveProgress", "{0} / {1} {2}"),
				FText::AsNumber(SourceObjective.CurrentValue), FText::AsNumber(SourceObjective.TargetValue),
				FText::FromString(SourceObjective.ProgressUnit));
			Objective.Status = SourceObjective.bMet ? LOCTEXT("Met", "Met") : LOCTEXT("InProgress", "In progress");
			Objective.Ratio = SourceObjective.TargetValue > 0
				? FMath::Clamp(static_cast<float>(SourceObjective.CurrentValue) / SourceObjective.TargetValue, 0.0f, 1.0f) : 0.0f;
			Objective.bMet = SourceObjective.bMet;
			Path.Objectives.Add(MoveTemp(Objective));
		}
		Updated.Paths.Add(MoveTemp(Path));
	}
	if (Updated.SelectedVictoryId.IsNone() && !Updated.Paths.IsEmpty()) Updated.SelectedVictoryId = Updated.Paths[0].StableId;
	if (Progress.Outcome == Hansa::Simulation::EHansaScenarioOutcome::Victory)
	{
		Updated.Phase = EHansaScenarioPresentationPhase::Victory;
		Updated.StateLabel = LOCTEXT("Victory", "Scenario victory");
		Updated.SelectedVictoryId = FName(*Progress.WinningVictoryId);
		const FHansaVictoryPathPresentation* Winner = Updated.Paths.FindByPredicate(
			[&Updated](const auto& Path) { return Path.StableId == Updated.SelectedVictoryId; });
		Updated.OutcomeExplanation = Winner != nullptr ? Winner->Summary : LOCTEXT("VictoryFallback", "The scenario objectives are complete.");
        if (Snapshot.Phase != Updated.Phase || Snapshot.ScenarioId != Updated.ScenarioId) { Updated.bOpen = true; Updated.bPauseMenu = false; }
	}
	else if (Progress.Outcome == Hansa::Simulation::EHansaScenarioOutcome::Failure)
	{
		Updated.Phase = EHansaScenarioPresentationPhase::Failure;
		Updated.StateLabel = LOCTEXT("Failure", "! Scenario failed");
		Updated.OutcomeExplanation = FText::FromString(Progress.FailureReason);
        if (Snapshot.Phase != Updated.Phase || Snapshot.ScenarioId != Updated.ScenarioId) { Updated.bOpen = true; Updated.bPauseMenu = false; }
	}
	else
	{
		if (Updated.Phase != EHansaScenarioPresentationPhase::Briefing) Updated.Phase = EHansaScenarioPresentationPhase::Active;
		Updated.StateLabel = Updated.Phase == EHansaScenarioPresentationPhase::Briefing
			? LOCTEXT("Briefing", "Scenario briefing") : LOCTEXT("Active", "Scenario active");
		Updated.OutcomeExplanation = FText::GetEmpty();
	}
	Snapshot = MoveTemp(Updated);
	Broadcast();
	return true;
}

void UHansaScenarioPresentationModel::Open(const FName RestoreFocus)
{
	RestoreFocusSemanticId = RestoreFocus;
	Snapshot.bOpen = true;
    Snapshot.bPauseMenu = false;
	Broadcast();
}

void UHansaScenarioPresentationModel::AcknowledgeBriefing()
{
    if (!Snapshot.bReady || Snapshot.Phase != EHansaScenarioPresentationPhase::Briefing) return;
    Snapshot.Phase = EHansaScenarioPresentationPhase::Active;
    Snapshot.StateLabel = LOCTEXT("Active", "Scenario active");
    Snapshot.bOpen = false;
    Snapshot.bPauseMenu = false;
    Broadcast();
    FocusRestoreRequested.Broadcast(TEXT("HUD.TopStatus.Session"));
    OfferHelp(EHansaSessionHelpTopic::Camera);
}

void UHansaScenarioPresentationModel::Close()
{
    if (Snapshot.Phase == EHansaScenarioPresentationPhase::Briefing) { AcknowledgeBriefing(); return; }
    Snapshot.bPauseMenu = false;
    Snapshot.bOpen = false;
	Broadcast();
	FocusRestoreRequested.Broadcast(RestoreFocusSemanticId);
}

void UHansaScenarioPresentationModel::SelectPath(const FName VictoryId)
{
	if (Snapshot.Paths.ContainsByPredicate([VictoryId](const auto& Path) { return Path.StableId == VictoryId; }))
	{
		Snapshot.SelectedVictoryId = VictoryId;
		Broadcast();
	}
}

void UHansaScenarioPresentationModel::SetFocusedSemanticId(const FName SemanticId)
{
	Snapshot.FocusedSemanticId = SemanticId;
	Broadcast();
}

void UHansaScenarioPresentationModel::Broadcast()
{
	++Revision;
	Changed.Broadcast(Snapshot, Revision);
}


void UHansaScenarioPresentationModel::OpenPause() { Snapshot.bPauseMenu=true;Snapshot.bOpen=true;RestoreFocusSemanticId=TEXT("HUD.TopStatus.Session");Broadcast(); }
void UHansaScenarioPresentationModel::ReviewProgress() { Snapshot.bPauseMenu=false;Broadcast(); }
void UHansaScenarioPresentationModel::RequestSaveLoad() { if(SessionIntent)SessionIntent(TEXT("SaveLoad")); }
void UHansaScenarioPresentationModel::SessionRestored() {
    Snapshot.bLoadedSession=true;
    if(Snapshot.Phase==EHansaScenarioPresentationPhase::Briefing)Snapshot.Phase=EHansaScenarioPresentationPhase::Active;
    Snapshot.bCoachVisible=false;
    OpenPause();
}
void UHansaScenarioPresentationModel::LoadHelpPreferences(const FString& File) {
    PreferenceFile=File;FConfigFile Config;Config.Read(File);
    Config.GetBool(TEXT("Hansa.SessionHelp"),TEXT("Enabled"),Snapshot.bHelpEnabled);
    int32 Mask=0;Config.GetInt(TEXT("Hansa.SessionHelp"),TEXT("Dismissed"),Mask);DismissedHelpMask=uint32(Mask)&63u;
    Snapshot.bCoachVisible=false;Broadcast();
}
void UHansaScenarioPresentationModel::PersistHelp() {
    if(PreferenceFile.IsEmpty())return;
    FConfigFile Config;Config.Read(PreferenceFile);
    Config.SetBool(TEXT("Hansa.SessionHelp"),TEXT("Enabled"),Snapshot.bHelpEnabled);
    Config.SetString(TEXT("Hansa.SessionHelp"),TEXT("Dismissed"),*FString::FromInt(int32(DismissedHelpMask)));IFileManager::Get().MakeDirectory(*FPaths::GetPath(PreferenceFile),true);Config.Write(PreferenceFile);
}
void UHansaScenarioPresentationModel::ToggleHelp() {Snapshot.bHelpEnabled=!Snapshot.bHelpEnabled;if(!Snapshot.bHelpEnabled)Snapshot.bCoachVisible=false;PersistHelp();Broadcast();}
void UHansaScenarioPresentationModel::ResetHelp() {DismissedHelpMask=0;Snapshot.bHelpEnabled=true;PersistHelp();Broadcast();}
void UHansaScenarioPresentationModel::DismissHelp() {DismissedHelpMask|=1u<<uint8(Snapshot.HelpTopic);Snapshot.bCoachVisible=false;PersistHelp();Broadcast();}
void UHansaScenarioPresentationModel::OfferHelp(EHansaSessionHelpTopic Topic) {
    if(!Snapshot.bHelpEnabled || Snapshot.bOpen || (DismissedHelpMask&(1u<<uint8(Topic))) || (Snapshot.bCoachVisible&&Snapshot.HelpTopic==Topic))return;
    Snapshot.HelpTopic=Topic;Snapshot.bCoachVisible=true;
    switch(Topic) {
    case EHansaSessionHelpTopic::Camera:
        Snapshot.HelpTitle=LOCTEXT("CameraTitle","Explore your city");
        Snapshot.HelpBody=LOCTEXT("CameraHelp","Pan with WASD or the left stick. Zoom with the mouse wheel or triggers; rotate with Q/E or the right stick. Escape or Menu opens the session menu. There is no single right way to build prosperity.");break;
    case EHansaSessionHelpTopic::Construction:
        Snapshot.HelpTitle=LOCTEXT("BuildTitle","Build where it matters");
        Snapshot.HelpBody=LOCTEXT("BuildHelp","Drag a construction card onto the city to preview placement. You can also select a card, use the placement target controls and confirm with Enter or A. Read the cost, access and inputs before placing; cancel with Escape or B.");break;
    case EHansaSessionHelpTopic::Roads:
        Snapshot.HelpTitle=LOCTEXT("RoadTitle","Connect the city");
        Snapshot.HelpBody=LOCTEXT("RoadHelp","Choose Roads, then drag through the city to preview a connection. The road controls also let you target adjacent cells without dragging. Review valid cells and total cost before confirming. Cancel a preview without spending money.");break;
    case EHansaSessionHelpTopic::Inspection:
        Snapshot.HelpTitle=LOCTEXT("InspectTitle","Read the cause before acting");
        Snapshot.HelpBody=LOCTEXT("InspectHelp","Select a building to inspect its inputs, output, workforce and current blocker. Explain cause follows the evidence; Frame returns the camera to the building. Use what you find to choose your own remedy.");break;
    case EHansaSessionHelpTopic::Market:
        Snapshot.HelpTitle=LOCTEXT("MarketTitle","Diagnose a shortage");
        Snapshot.HelpBody=LOCTEXT("MarketHelp","Open Market and select a good. Compare stock, demand, reserves and incoming cargo; check how old the report is. Producer and consumer links show where pressure comes from. Production and trade are alternatives, not prescribed steps.");break;
    case EHansaSessionHelpTopic::Routes:
        Snapshot.HelpTitle=LOCTEXT("RoutesTitle","Plan a useful voyage");
        Snapshot.HelpBody=LOCTEXT("RoutesHelp","Choose New route, select a Cog and add ordered ports. Set what to load or unload and the minimum reserve. Name the route, review capacity, upkeep and warnings, then activate. Cargo delivery does not automatically sell the goods.");break;
    }
    Broadcast();
}

#undef LOCTEXT_NAMESPACE

