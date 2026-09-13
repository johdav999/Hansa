#include "UI/HansaResearchPresentationModel.h"

#include "Definitions/HansaEconomicRegistry.h"

namespace
{
	FText EffectSummary(const TArray<Hansa::Simulation::FHansaCompiledResearchEffect>& Effects)
	{
		if (Effects.IsEmpty()) return FText::GetEmpty();
		TArray<FString> Lines;
        for(const auto& Effect:Effects){
            using Hansa::Simulation::EHansaResearchEffectKind;
            const FText Percent=FText::AsPercent(double(Effect.Magnitude)/10000.);
            FText Line;
            switch(Effect.Kind){
            case EHansaResearchEffectKind::MarketReportAgeReductionTicks:Line=FText::Format(NSLOCTEXT("Research","ReportAge","Reports stay current {0} ticks longer"),FText::AsNumber(Effect.Magnitude));break;
            case EHansaResearchEffectKind::TransactionFrictionReductionBasisPoints:Line=FText::Format(NSLOCTEXT("Research","Friction","Transaction friction reduced by {0}"),Percent);break;
            case EHansaResearchEffectKind::ProductionThroughputBasisPoints:Line=FText::Format(NSLOCTEXT("Research","Throughput","Production throughput bonus: {0}"),Percent);break;
            case EHansaResearchEffectKind::WarehouseHandlingBasisPoints:Line=FText::Format(NSLOCTEXT("Research","Handling","Warehouse handling bonus: {0}"),Percent);break;
            case EHansaResearchEffectKind::VehicleCapacityBasisPoints:Line=FText::Format(NSLOCTEXT("Research","Capacity","Vehicle capacity bonus: {0}"),Percent);break;
            case EHansaResearchEffectKind::ReserveAutomation:Line=NSLOCTEXT("Research","Reserves","Enables reserve automation");break;
            case EHansaResearchEffectKind::RouteScheduling:Line=NSLOCTEXT("Research","Scheduling","Enables route scheduling");break;
            default:{FString Target=Effect.TargetStableId;int32 Dot;if(Target.FindLastChar(TEXT('.'),Dot))Target.RightChopInline(Dot+1);Line=FText::Format(NSLOCTEXT("Research","Unlock","Unlocks {0}"),FText::FromString(Target));break;}
            }
            Lines.Add(Line.ToString());
        }
        return FText::FromString(FString::Join(Lines,TEXT("\n")));
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
    Snapshot.bLoading = false;
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
        Node.Effects = Technology.Effects;
        TArray<Hansa::Simulation::FHansaCompiledResearchEffect> Applied;
        for (const auto& Effect : State.AppliedEffects)
            if (Effect.SourceTechnologyId == Technology.StableId) Applied.Add({Effect.Kind, Effect.TargetStableId, Effect.Magnitude});
        Node.AppliedEffectSummary = EffectSummary(Applied);
        const auto Eligibility = Hansa::Simulation::FHansaResearchExecutor::CanQueue(State, Technology.StableId, Registry.GetTechnologies());
        Node.QueueError = Eligibility.Error;
        using Hansa::Simulation::EHansaResearchQueueError;
        switch (Eligibility.Error) {
        case EHansaResearchQueueError::QueueFull: Node.LockedReason = NSLOCTEXT("Research", "QueueFull", "Research slot occupied. Wait for completion."); break;
        case EHansaResearchQueueError::PrerequisiteMissing: Node.LockedReason = NSLOCTEXT("Research", "Prerequisite", "Complete prerequisites first."); break;
        case EHansaResearchQueueError::InsufficientResearchPoints: Node.LockedReason = FText::Format(NSLOCTEXT("Research", "Points", "Requires {0} research points. You have {1}."), FText::AsNumber(Technology.CostResearchPoints), FText::AsNumber(State.AvailableResearchPoints)); break;
        default: break;
        }
		Node.CostResearchPoints = Technology.CostResearchPoints;
		Node.DurationTicks = Technology.DurationTicks;
		Node.ProgressTicks = State.ActiveTechnologyId == Technology.StableId ? State.ProgressTicks : 0;
		for (const FString& PrerequisiteId : Technology.PrerequisiteTechnologyIds)
		{
			if (!State.IsCompleted(PrerequisiteId)) Node.MissingPrerequisiteIds.Add(PrerequisiteId);
		}
		Node.State = State.IsCompleted(Technology.StableId) ? EHansaResearchNodePresentationState::Completed :
			State.ActiveTechnologyId == Technology.StableId ? EHansaResearchNodePresentationState::Queued :
            Eligibility.IsSuccess() ? EHansaResearchNodePresentationState::Available : EHansaResearchNodePresentationState::Locked;
		Snapshot.Nodes.Add(MoveTemp(Node));
	}
    // Deterministic topological presentation order; dependencies always precede children.
    TArray<FHansaResearchNodePresentation> Ordered;
    TSet<FString> Added;
    for(int32 Pass=0;Pass<Snapshot.Nodes.Num();++Pass) {
        bool Progress=false;
        for(const auto& Node:Snapshot.Nodes) {
            if(Added.Contains(Node.StableId))continue;
            bool Ready=true;
            for(const auto& Id:Node.PrerequisiteIds)if(!Added.Contains(Id))Ready=false;
            if(Ready){Ordered.Add(Node);Added.Add(Node.StableId);Progress=true;}
        }
        if(!Progress)break;
    }
    for(const auto& Node:Snapshot.Nodes)if(!Added.Contains(Node.StableId))Ordered.Add(Node);
    Ordered.StableSort([](const auto& A,const auto& B){return A.Branch<B.Branch;});
    Snapshot.Nodes=MoveTemp(Ordered);
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
    if (!Selected || Snapshot.bLoading || Snapshot.bSubmitting || Selected->State != EHansaResearchNodePresentationState::Available) return false;
    const FString Id = Selected->StableId;
    Snapshot.bSubmitting = true; Snapshot.Feedback = FText::GetEmpty(); Broadcast();
    const bool Accepted = QueueIntent && QueueIntent(Id);
    Snapshot.bSubmitting = false;
    Snapshot.Feedback = Accepted ? FText::GetEmpty() : NSLOCTEXT("Research", "Rejected", "Research could not start. Check the current requirements and try again.");
    Broadcast();
    return Accepted;
}

void UHansaResearchPresentationModel::SetFocusedSemanticId(const FName SemanticId) { Snapshot.FocusedSemanticId = SemanticId; Broadcast(); }
void UHansaResearchPresentationModel::Broadcast() { ++Revision; Changed.Broadcast(Snapshot, Revision); }


void UHansaResearchPresentationModel::SetLoading(bool bLoading) { Snapshot.bLoading = bLoading; Broadcast(); }
void UHansaResearchPresentationModel::SetError(const FText& Message) { Snapshot.bLoading = false; Snapshot.Feedback = Message; Broadcast(); }
bool UHansaResearchPresentationModel::RequestEffect(int32 Index)
{
    const auto* Node = Snapshot.Nodes.FindByPredicate([this](const auto& N){return N.StableId == Snapshot.SelectedTechnologyId;});
    if (!Node || !Node->Effects.IsValidIndex(Index) || !EffectIntent) return false;
    const auto Effect = Node->Effects[Index];
    if (EffectIntent(Effect)) return true;
    SetError(NSLOCTEXT("Research", "NoTarget", "No affected building or route is present in this city yet."));
    return false;
}
