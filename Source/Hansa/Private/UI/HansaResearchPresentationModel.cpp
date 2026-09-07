#include "UI/HansaResearchPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"

namespace
{
	FText EffectSummary(const TArray<Hansa::Simulation::FHansaCompiledResearchEffect>& Effects)
	{
		if (Effects.IsEmpty()) return FText::GetEmpty();
		const auto& Effect = Effects[0];
		const FString Kind = Effect.Kind == Hansa::Simulation::EHansaResearchEffectKind::UnlockStableId
			? TEXT("Unlock") : TEXT("Efficiency / capability");
		return FText::FromString(FString::Printf(TEXT("%s: %s%s"), *Kind, *Effect.TargetStableId,
			Effect.Magnitude > 1 ? *FString::Printf(TEXT(" +%d"), Effect.Magnitude) : TEXT("")));
	}
}

void UHansaResearchPresentationModel::Initialize(const Hansa::Simulation::FHansaEconomicRegistry& Registry,
	const Hansa::Simulation::FHansaHouseResearchState& State)
{
	Snapshot = {};
	Refresh(Registry, State);
}

void UHansaResearchPresentationModel::Refresh(const Hansa::Simulation::FHansaEconomicRegistry& Registry,
	const Hansa::Simulation::FHansaHouseResearchState& State)
{
	const bool bWasOpen = Snapshot.bOpen;
	const FString Selected = Snapshot.SelectedTechnologyId;
	const FName Focused = Snapshot.FocusedSemanticId;
	Snapshot = {};
	Snapshot.bOpen = bWasOpen;
	Snapshot.AvailableResearchPoints = State.AvailableResearchPoints;
	Snapshot.SelectedTechnologyId = Selected;
	Snapshot.FocusedSemanticId = Focused;
	for (const Hansa::Simulation::FHansaCompiledTechnologyDefinition& Technology : Registry.GetTechnologies())
	{
		FHansaResearchNodePresentation Node;
		Node.StableId = Technology.StableId;
		Node.DisplayName = FText::FromString(Technology.DisplayName);
		Node.Branch = Technology.Branch;
		Node.PrerequisiteIds = Technology.PrerequisiteTechnologyIds;
		Node.UnlockExplanation = FText::FromString(Technology.UnlockExplanation);
		Node.EffectSummary = EffectSummary(Technology.Effects);
		Node.CostResearchPoints = Technology.CostResearchPoints;
		Node.DurationTicks = Technology.DurationTicks;
		Node.ProgressTicks = State.ActiveTechnologyId == Technology.StableId ? State.ProgressTicks : 0;
		for (const FString& PrerequisiteId : Technology.PrerequisiteTechnologyIds)
		{
			if (!State.IsCompleted(PrerequisiteId)) Node.MissingPrerequisiteIds.Add(PrerequisiteId);
		}
		Node.State = State.IsCompleted(Technology.StableId) ? EHansaResearchNodePresentationState::Completed :
			State.ActiveTechnologyId == Technology.StableId ? EHansaResearchNodePresentationState::Queued :
			Node.MissingPrerequisiteIds.IsEmpty() && State.ActiveTechnologyId.IsEmpty() &&
				State.AvailableResearchPoints >= Technology.CostResearchPoints ? EHansaResearchNodePresentationState::Available :
			EHansaResearchNodePresentationState::Locked;
		Snapshot.Nodes.Add(MoveTemp(Node));
	}
	if (Snapshot.SelectedTechnologyId.IsEmpty() && !Snapshot.Nodes.IsEmpty()) Snapshot.SelectedTechnologyId = Snapshot.Nodes[0].StableId;
	Broadcast();
}

void UHansaResearchPresentationModel::Open(const FName OpenerSemanticId) { Opener = OpenerSemanticId; Snapshot.bOpen = true; Broadcast(); }
void UHansaResearchPresentationModel::Close() { Snapshot.bOpen = false; Broadcast(); FocusRestoreRequested.Broadcast(Opener); }

bool UHansaResearchPresentationModel::SelectTechnology(const FString& TechnologyId)
{
	if (!Snapshot.Nodes.ContainsByPredicate([&TechnologyId](const auto& Node) { return Node.StableId == TechnologyId; })) return false;
	Snapshot.SelectedTechnologyId = TechnologyId;
	Broadcast();
	return true;
}

bool UHansaResearchPresentationModel::RequestQueueSelected()
{
	const FHansaResearchNodePresentation* Selected = Snapshot.Nodes.FindByPredicate([this](const auto& Node)
	{
		return Node.StableId == Snapshot.SelectedTechnologyId;
	});
	return Selected != nullptr && Selected->State == EHansaResearchNodePresentationState::Available &&
		QueueIntent && QueueIntent(Selected->StableId);
}

void UHansaResearchPresentationModel::SetFocusedSemanticId(const FName SemanticId) { Snapshot.FocusedSemanticId = SemanticId; Broadcast(); }
void UHansaResearchPresentationModel::Broadcast() { ++Revision; Changed.Broadcast(Snapshot, Revision); }

