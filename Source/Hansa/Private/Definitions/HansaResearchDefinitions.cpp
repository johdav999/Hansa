#include "Definitions/HansaResearchDefinitions.h"

UHansaTechnologyDefinition::UHansaTechnologyDefinition()
{
	DefinitionCategory = TEXT("Technology");
}

void UHansaTechnologyDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	auto Add = [this, &OutIssues](const FName Code, const FString& Path, const FText& Cause, const FText& Remedy)
	{
		OutIssues.Add({EHansaDefinitionValidationSeverity::Error, Code, StableDefinitionId + TEXT(".") + Path, Cause, Remedy});
	};
	if (CostResearchPoints < 0 || DurationTicks <= 0)
	{
		Add(TEXT("HSA-TECH-001"), TEXT("CostResearchPoints"),
			NSLOCTEXT("HansaResearch", "InvalidCostDuration", "Technology cost must be non-negative and duration must be positive."),
			NSLOCTEXT("HansaResearch", "InvalidCostDurationRemedy", "Use a non-negative point cost and at least one simulation tick."));
	}
	if (UnlockExplanation.IsEmpty())
	{
		Add(TEXT("HSA-TECH-002"), TEXT("UnlockExplanation"),
			NSLOCTEXT("HansaResearch", "MissingExplanation", "Technology has no visible unlock explanation."),
			NSLOCTEXT("HansaResearch", "MissingExplanationRemedy", "Explain the unlock or efficiency effect in player-facing language."));
	}
	if (Effects.IsEmpty())
	{
		Add(TEXT("HSA-TECH-003"), TEXT("Effects"),
			NSLOCTEXT("HansaResearch", "MissingEffects", "Technology must apply at least one deterministic effect."),
			NSLOCTEXT("HansaResearch", "MissingEffectsRemedy", "Add a typed effect with a stable target where applicable."));
	}
	TSet<FString> SeenPrerequisites;
	for (const FString& Prerequisite : PrerequisiteTechnologyIds)
	{
		if (Prerequisite.IsEmpty() || Prerequisite == StableDefinitionId || SeenPrerequisites.Contains(Prerequisite))
		{
			Add(TEXT("HSA-TECH-004"), TEXT("PrerequisiteTechnologyIds"),
				NSLOCTEXT("HansaResearch", "InvalidPrerequisite", "Prerequisites contain an empty, duplicate, or self reference."),
				NSLOCTEXT("HansaResearch", "InvalidPrerequisiteRemedy", "Use unique stable Technology.* IDs and remove self references."));
		}
		SeenPrerequisites.Add(Prerequisite);
	}
	for (const FHansaResearchEffectDefinition& Effect : Effects)
	{
		if (Effect.Magnitude < 0 || (Effect.Kind == EHansaAuthoredResearchEffectKind::UnlockStableId && Effect.TargetStableId.IsEmpty()))
		{
			Add(TEXT("HSA-TECH-005"), TEXT("Effects"),
				NSLOCTEXT("HansaResearch", "InvalidEffect", "Research effect has an invalid magnitude or missing required stable target."),
				NSLOCTEXT("HansaResearch", "InvalidEffectRemedy", "Use a non-negative magnitude and assign a target for unlock effects."));
		}
	}
}

void UHansaTechnologyDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	TArray<FString> Prerequisites = PrerequisiteTechnologyIds;
	Prerequisites.Sort();
	InOutCanonicalData += FString::Printf(TEXT("|branch=%d|cost=%d|duration=%d|explanation=%s"),
		static_cast<int32>(Branch), CostResearchPoints, DurationTicks, *UnlockExplanation.ToString());
	for (const FString& Prerequisite : Prerequisites) InOutCanonicalData += TEXT("|prereq=") + Prerequisite;
	TArray<FString> EffectRows;
	for (const FHansaResearchEffectDefinition& Effect : Effects)
	{
		EffectRows.Add(FString::Printf(TEXT("%d:%s:%d"), static_cast<int32>(Effect.Kind), *Effect.TargetStableId, Effect.Magnitude));
	}
	EffectRows.Sort();
	for (const FString& Effect : EffectRows) InOutCanonicalData += TEXT("|effect=") + Effect;
}
