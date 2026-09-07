#include "Definitions/HansaMerchantAIDefinitions.h"

UHansaMerchantAITuningDefinition::UHansaMerchantAITuningDefinition()
{
	DefinitionCategory = TEXT("Merchant AI");
}

void UHansaMerchantAITuningDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	auto Add = [this, &OutIssues](const FName Code, const FString& Path, const FText& Cause, const FText& Remedy)
	{
		OutIssues.Add({EHansaDefinitionValidationSeverity::Error, Code, StableDefinitionId + TEXT(".") + Path, Cause, Remedy});
	};
	if (DecisionCadenceTicks <= 0 || DecisionHistoryCapacity <= 0 || DecisionHistoryCapacity > 256 ||
		MinimumDestinationDemandGapMilliUnits < 0 || ShortageUtilityPerUnit < 0 || MarginUtilityPerMilliMark < 0 ||
		TargetCompletedTradeLegs <= 0)
	{
		Add(TEXT("HSA-AI-001"), TEXT("DecisionCadenceTicks"),
			NSLOCTEXT("HansaMerchantAI", "InvalidBounds", "Merchant AI cadence, history, thresholds or utility weights are outside deterministic bounds."),
			NSLOCTEXT("HansaMerchantAI", "InvalidBoundsRemedy", "Use positive cadence/objective values, a history capacity from 1 through 256, and non-negative shortage inputs."));
	}
	if (TradePlans.IsEmpty())
	{
		Add(TEXT("HSA-AI-002"), TEXT("TradePlans"),
			NSLOCTEXT("HansaMerchantAI", "NoPlans", "Merchant AI tuning requires at least one bounded trade plan."),
			NSLOCTEXT("HansaMerchantAI", "NoPlansRemedy", "Add a stable route, vehicle, source, destination and good opportunity."));
	}
	TSet<FString> PlanIds;
	for (const FHansaMerchantAITradePlanDefinition& Plan : TradePlans)
	{
		if (Plan.StablePlanId.IsEmpty() || Plan.RouteDefinitionId.IsEmpty() || Plan.VehicleDefinitionId.IsEmpty() ||
			Plan.SourceCityId.IsEmpty() || Plan.DestinationCityId.IsEmpty() || Plan.SourceCityId == Plan.DestinationCityId ||
			Plan.GoodId.IsEmpty() || Plan.QuantityLimitMilliUnits <= 0 || Plan.MinimumSourceReserveMilliUnits < 0 ||
			PlanIds.Contains(Plan.StablePlanId))
		{
			Add(TEXT("HSA-AI-003"), TEXT("TradePlans"),
				NSLOCTEXT("HansaMerchantAI", "InvalidPlan", "A trade plan has missing or duplicate identity, invalid references, cities, quantity, or reserve."),
				NSLOCTEXT("HansaMerchantAI", "InvalidPlanRemedy", "Use a unique plan ID, distinct source/destination, valid stable references, positive quantity and non-negative reserve."));
		}
		PlanIds.Add(Plan.StablePlanId);
	}
}

void UHansaMerchantAITuningDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("|cadence=%d|history=%d|gap=%lld|margin=%lld|shortageWeight=%lld|marginWeight=%lld|research=%lld|production=%lld|legs=%lld"),
		DecisionCadenceTicks, DecisionHistoryCapacity, MinimumDestinationDemandGapMilliUnits, MinimumGrossMarginMilliMarks,
		ShortageUtilityPerUnit, MarginUtilityPerMilliMark, ResearchUtility, ProductionUtility, TargetCompletedTradeLegs);
	for (const FString& TechnologyId : PreferredResearchTechnologyIds) InOutCanonicalData += TEXT("|research=") + TechnologyId;
	TArray<FString> Rows;
	for (const FHansaMerchantAITradePlanDefinition& Plan : TradePlans)
	{
		Rows.Add(FString::Printf(TEXT("%s:%s:%s:%s:%s:%s:%lld:%lld:%lld"), *Plan.StablePlanId, *Plan.RouteDefinitionId,
			*Plan.VehicleDefinitionId, *Plan.SourceCityId, *Plan.DestinationCityId, *Plan.GoodId,
			Plan.QuantityLimitMilliUnits, Plan.MinimumSourceReserveMilliUnits, Plan.UtilityBias));
	}
	Rows.Sort();
	for (const FString& Row : Rows) InOutCanonicalData += TEXT("|plan=") + Row;
}
