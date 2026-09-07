#include "UI/HansaScenarioPresentationModel.h"

#define LOCTEXT_NAMESPACE "HansaScenarioPresentationModel"

void UHansaScenarioPresentationModel::InitializeDefaults()
{
	Snapshot = {};
	Snapshot.Title = LOCTEXT("LoadingTitle", "Scenario");
	Snapshot.Briefing = LOCTEXT("LoadingBriefing", "Preparing the authored scenario objectives…");
	Snapshot.StateLabel = LOCTEXT("LoadingState", "Loading");
	Snapshot.Phase = EHansaScenarioPresentationPhase::Briefing;
	Snapshot.bOpen = true;
	Broadcast();
}

bool UHansaScenarioPresentationModel::ApplyProgress(const Hansa::Simulation::FHansaScenarioProgress& Progress)
{
	FHansaScenarioPresentationSnapshot Updated = Snapshot;
	Updated.ScenarioId = FName(*Progress.ScenarioId);
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
			Objective.Status = SourceObjective.bMet ? LOCTEXT("Met", "✓ Met") : LOCTEXT("InProgress", "△ In progress");
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
		Updated.StateLabel = LOCTEXT("Victory", "✓ Scenario victory");
		Updated.SelectedVictoryId = FName(*Progress.WinningVictoryId);
		const FHansaVictoryPathPresentation* Winner = Updated.Paths.FindByPredicate(
			[&Updated](const auto& Path) { return Path.StableId == Updated.SelectedVictoryId; });
		Updated.OutcomeExplanation = Winner != nullptr ? Winner->Summary : LOCTEXT("VictoryFallback", "The scenario objectives are complete.");
		Updated.bOpen = true;
	}
	else if (Progress.Outcome == Hansa::Simulation::EHansaScenarioOutcome::Failure)
	{
		Updated.Phase = EHansaScenarioPresentationPhase::Failure;
		Updated.StateLabel = LOCTEXT("Failure", "! Scenario failed");
		Updated.OutcomeExplanation = FText::FromString(Progress.FailureReason);
		Updated.bOpen = true;
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
	Broadcast();
}

void UHansaScenarioPresentationModel::AcknowledgeBriefing()
{
	if (Snapshot.Phase == EHansaScenarioPresentationPhase::Briefing) Snapshot.Phase = EHansaScenarioPresentationPhase::Active;
	Snapshot.StateLabel = LOCTEXT("Active", "Scenario active");
	Broadcast();
}

void UHansaScenarioPresentationModel::Close()
{
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

#undef LOCTEXT_NAMESPACE

