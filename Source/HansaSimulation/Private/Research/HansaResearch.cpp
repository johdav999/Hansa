#include "Research/HansaResearch.h"

#include "Algo/Sort.h"

namespace Hansa::Simulation
{
	namespace
	{
		const FHansaCompiledTechnologyDefinition* FindTechnology(
			TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies, const FString& Id)
		{
			return Technologies.FindByPredicate([&Id](const FHansaCompiledTechnologyDefinition& Technology)
			{
				return Technology.StableId == Id;
			});
		}

		bool EffectLess(const FHansaAppliedResearchEffect& Left, const FHansaAppliedResearchEffect& Right)
		{
			if (Left.SourceTechnologyId != Right.SourceTechnologyId) return Left.SourceTechnologyId < Right.SourceTechnologyId;
			if (Left.Kind != Right.Kind) return static_cast<uint8>(Left.Kind) < static_cast<uint8>(Right.Kind);
			if (Left.TargetStableId != Right.TargetStableId) return Left.TargetStableId < Right.TargetStableId;
			return Left.Magnitude < Right.Magnitude;
		}
	}

	bool FHansaHouseResearchState::IsCompleted(const FString& TechnologyId) const
	{
		return CompletedTechnologyIds.Contains(TechnologyId);
	}

	bool FHansaHouseResearchState::IsUnlocked(const FString& StableId) const
	{
		return AppliedEffects.ContainsByPredicate([&StableId](const FHansaAppliedResearchEffect& Effect)
		{
			return Effect.Kind == EHansaResearchEffectKind::UnlockStableId && Effect.TargetStableId == StableId;
		});
	}

	int32 FHansaHouseResearchState::GetEffectMagnitude(
		const EHansaResearchEffectKind Kind, const FString& TargetStableId) const
	{
		int64 Total = 0;
		for (const FHansaAppliedResearchEffect& Effect : AppliedEffects)
		{
			if (Effect.Kind == Kind && (TargetStableId.IsEmpty() || Effect.TargetStableId == TargetStableId))
			{
				Total += Effect.Magnitude;
			}
		}
		return static_cast<int32>(FMath::Clamp<int64>(Total, MIN_int32, MAX_int32));
	}

	TArray<FHansaResearchGraphDiagnostic> FHansaResearchGraphValidator::Validate(
		TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies,
		TConstArrayView<FString> DeclaredRootTechnologyIds,
		const TSet<FString>& KnownStableIds)
	{
		TMap<FString, const FHansaCompiledTechnologyDefinition*> ById;
		for (const FHansaCompiledTechnologyDefinition& Technology : Technologies) ById.Add(Technology.StableId, &Technology);
		TArray<FHansaResearchGraphDiagnostic> Result;
		auto Add = [&Result](const EHansaResearchGraphIssue Issue, const FString& Id, const FString& Related,
			const TCHAR* Cause, const TCHAR* Remedy)
		{
			Result.Add({Issue, Id, Related, Cause, Remedy});
		};

		for (const FHansaCompiledTechnologyDefinition& Technology : Technologies)
		{
			for (const FString& PrerequisiteId : Technology.PrerequisiteTechnologyIds)
			{
				if (!ById.Contains(PrerequisiteId))
				{
					Add(EHansaResearchGraphIssue::MissingNode, Technology.StableId, PrerequisiteId,
						TEXT("Technology references a prerequisite that is not present in the graph."),
						TEXT("Add the missing technology or correct/remove the stable prerequisite reference."));
				}
			}
			for (const FHansaCompiledResearchEffect& Effect : Technology.Effects)
			{
				if (!Effect.TargetStableId.IsEmpty() && !KnownStableIds.Contains(Effect.TargetStableId))
				{
					Add(EHansaResearchGraphIssue::InvalidEffectReference, Technology.StableId, Effect.TargetStableId,
						TEXT("Technology effect references an unknown stable ID."),
						TEXT("Choose a stable ID from the compiled content set or add that definition."));
				}
			}
		}

		TMap<FString, uint8> VisitState;
		TFunction<void(const FString&)> Visit = [&](const FString& Id)
		{
			VisitState.Add(Id, 1);
			const FHansaCompiledTechnologyDefinition* Technology = ById.FindRef(Id);
			if (Technology != nullptr)
			{
				for (const FString& PrerequisiteId : Technology->PrerequisiteTechnologyIds)
				{
					if (!ById.Contains(PrerequisiteId)) continue;
					if (VisitState.FindRef(PrerequisiteId) == 0) Visit(PrerequisiteId);
					else if (VisitState.FindRef(PrerequisiteId) == 1)
					{
						Add(EHansaResearchGraphIssue::Cycle, Id, PrerequisiteId,
							TEXT("Research prerequisite graph contains a cycle."),
							TEXT("Remove the back-edge so prerequisites form an acyclic progression."));
					}
				}
			}
			VisitState.Add(Id, 2);
		};
		for (const FHansaCompiledTechnologyDefinition& Technology : Technologies)
		{
			if (VisitState.FindRef(Technology.StableId) == 0) Visit(Technology.StableId);
		}

		TSet<FString> Reachable;
		TArray<FString> Frontier;
		for (const FString& RootId : DeclaredRootTechnologyIds)
		{
			if (ById.Contains(RootId)) { Reachable.Add(RootId); Frontier.Add(RootId); }
			else Add(EHansaResearchGraphIssue::MissingBranchRoot, RootId, FString(),
				TEXT("A declared branch root is missing from the graph."), TEXT("Add the branch root or update the declared root stable ID."));
		}
		while (!Frontier.IsEmpty())
		{
			const FString SourceId = Frontier.Pop(EAllowShrinking::No);
			for (const FHansaCompiledTechnologyDefinition& Candidate : Technologies)
			{
				if (!Reachable.Contains(Candidate.StableId) && Candidate.PrerequisiteTechnologyIds.Contains(SourceId))
				{
					Reachable.Add(Candidate.StableId);
					Frontier.Add(Candidate.StableId);
				}
			}
		}
		for (const FHansaCompiledTechnologyDefinition& Technology : Technologies)
		{
			if (!Reachable.Contains(Technology.StableId))
			{
				Add(EHansaResearchGraphIssue::Unreachable, Technology.StableId, FString(),
					TEXT("Technology cannot be reached from any declared branch root."),
					TEXT("Connect the technology to a reachable prerequisite chain or declare the correct branch root."));
			}
		}
		Result.Sort([](const FHansaResearchGraphDiagnostic& Left, const FHansaResearchGraphDiagnostic& Right)
		{
			if (Left.TechnologyId != Right.TechnologyId) return Left.TechnologyId < Right.TechnologyId;
			if (Left.Issue != Right.Issue) return static_cast<uint8>(Left.Issue) < static_cast<uint8>(Right.Issue);
			return Left.RelatedStableId < Right.RelatedStableId;
		});
		return Result;
	}

	FHansaResearchQueueResult FHansaResearchExecutor::TryQueue(
		TArray<FHansaHouseResearchState>& States, const FHansaHouseId HouseId, const FString& TechnologyId,
		TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies)
	{
		FHansaHouseResearchState* State = States.FindByPredicate([HouseId](const FHansaHouseResearchState& Candidate)
		{
			return Candidate.HouseId == HouseId;
		});
		if (State == nullptr) return {EHansaResearchQueueError::UnknownHouse, {}};
		const FHansaCompiledTechnologyDefinition* Technology = FindTechnology(Technologies, TechnologyId);
		if (Technology == nullptr) return {EHansaResearchQueueError::UnknownTechnology, {}};
		if (!State->ActiveTechnologyId.IsEmpty()) return {EHansaResearchQueueError::QueueFull, {}};
		if (State->IsCompleted(TechnologyId)) return {EHansaResearchQueueError::AlreadyCompleted, {}};
		for (const FString& PrerequisiteId : Technology->PrerequisiteTechnologyIds)
		{
			if (!State->IsCompleted(PrerequisiteId)) return {EHansaResearchQueueError::PrerequisiteMissing, PrerequisiteId};
		}
		if (State->AvailableResearchPoints < Technology->CostResearchPoints)
		{
			return {EHansaResearchQueueError::InsufficientResearchPoints, {}};
		}
		State->AvailableResearchPoints -= Technology->CostResearchPoints;
		State->ActiveTechnologyId = TechnologyId;
		State->ProgressTicks = 0;
		return {};
	}

	void FHansaResearchExecutor::AdvanceOneTick(TArray<FHansaHouseResearchState>& States,
		TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies, TArray<FHansaResearchCompletion>& OutCompletions)
	{
		for (FHansaHouseResearchState& State : States)
		{
			if (State.ActiveTechnologyId.IsEmpty()) continue;
			const FHansaCompiledTechnologyDefinition* Technology = FindTechnology(Technologies, State.ActiveTechnologyId);
			if (Technology == nullptr) continue;
			++State.ProgressTicks;
			if (State.ProgressTicks < Technology->DurationTicks) continue;

			FHansaResearchCompletion Completion;
			Completion.HouseId = State.HouseId;
			Completion.TechnologyId = Technology->StableId;
			State.CompletedTechnologyIds.AddUnique(Technology->StableId);
			State.CompletedTechnologyIds.Sort();
			for (const FHansaCompiledResearchEffect& Effect : Technology->Effects)
			{
				FHansaAppliedResearchEffect Applied{Technology->StableId, Effect.Kind, Effect.TargetStableId, Effect.Magnitude};
				State.AppliedEffects.Add(Applied);
				Completion.AppliedEffects.Add(MoveTemp(Applied));
			}
			State.AppliedEffects.Sort(EffectLess);
			Completion.AppliedEffects.Sort(EffectLess);
			State.ActiveTechnologyId.Reset();
			State.ProgressTicks = 0;
			OutCompletions.Add(MoveTemp(Completion));
		}
	}

	bool FHansaResearchExecutor::RestoreCanonical(TArray<FHansaHouseResearchState>& OutStates,
		TConstArrayView<FHansaHouseResearchInitialization> Initializations,
		TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies)
	{
		TArray<FHansaHouseResearchState> Candidate;
		for (const FHansaHouseResearchInitialization& Initial : Initializations)
		{
			if (!Initial.HouseId.IsValid() || Initial.AvailableResearchPoints < 0 || Initial.ProgressTicks < 0 ||
				Candidate.ContainsByPredicate([&Initial](const FHansaHouseResearchState& Existing) { return Existing.HouseId == Initial.HouseId; })) return false;
			if (!Initial.ActiveTechnologyId.IsEmpty())
			{
				const FHansaCompiledTechnologyDefinition* Active = FindTechnology(Technologies, Initial.ActiveTechnologyId);
				if (Active == nullptr || Initial.ProgressTicks >= Active->DurationTicks) return false;
			}
			FHansaHouseResearchState State;
			State.HouseId = Initial.HouseId;
			State.AvailableResearchPoints = Initial.AvailableResearchPoints;
			State.ActiveTechnologyId = Initial.ActiveTechnologyId;
			State.ProgressTicks = Initial.ProgressTicks;
			State.CompletedTechnologyIds = Initial.CompletedTechnologyIds;
			State.CompletedTechnologyIds.Sort();
			if (State.CompletedTechnologyIds.ContainsByPredicate([&Technologies](const FString& Id) { return FindTechnology(Technologies, Id) == nullptr; })) return false;
			State.AppliedEffects = Initial.AppliedEffects;
			State.AppliedEffects.Sort(EffectLess);
			Candidate.Add(MoveTemp(State));
		}
		Candidate.Sort([](const FHansaHouseResearchState& Left, const FHansaHouseResearchState& Right) { return Left.HouseId < Right.HouseId; });
		OutStates = MoveTemp(Candidate);
		return true;
	}
}

