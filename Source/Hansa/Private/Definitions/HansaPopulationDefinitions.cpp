#include "Definitions/HansaPopulationDefinitions.h"

#include "Model/HansaIds.h"

namespace
{
	void AddPopulationIssue(TArray<FHansaDefinitionValidationIssue>& OutIssues, const FName Code,
		const FString& Path, const FText& Cause, const FText& Remedy)
	{
		OutIssues.Add({ EHansaDefinitionValidationSeverity::Error, Code, Path, Cause, Remedy });
	}

	bool HasPopulationDomain(const FString& StableId, const TCHAR* Domain)
	{
		const auto Parsed = Hansa::Simulation::FHansaDefinitionId::TryParse(StableId);
		return Parsed && Parsed.Value.GetDomain() == Domain;
	}
}

UHansaNeedDefinition::UHansaNeedDefinition()
{
	DefinitionCategory = TEXT("Population Needs");
	LocalizationKey = TEXT("Game.Need.Unnamed");
}

void UHansaNeedDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (DefaultReserveDays < 0 || DefaultReserveDays > 90 || SeasonDays < 1 || SeasonDays > 365 || FixedSeason < -1 || FixedSeason > 3 ||
		SeasonMultipliers.Num() != 4 || SeasonMultipliers.ContainsByPredicate([](int32 V) { return V < 0 || V > 10000; }) ||
		(bSeasonal && Kind != EHansaNeedKind::Good))
	{
		AddPopulationIssue(OutIssues, TEXT("HSA-NEED-SEASON"), TEXT("SeasonMultipliers"),
			NSLOCTEXT("HansaPopulationDefinition", "SeasonInvalid", "Seasonal need settings are invalid."),
			NSLOCTEXT("HansaPopulationDefinition", "SeasonRemedy", "Use a good need, 1-365 days, fixed season -1 to 3, and four multipliers from 0 to 10000."));
	}
	if (!HasPopulationDomain(StableDefinitionId, TEXT("Need")))
	{
		AddPopulationIssue(OutIssues, TEXT("HSA-NEED-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaPopulationDefinition", "NeedDomain", "A need requires a canonical Need.* stable ID."),
			NSLOCTEXT("HansaPopulationDefinition", "NeedDomainRemedy", "Assign a unique Need.* stable identity."));
	}
 TSet<FString> Seen;
 Seen.Add(GoodId);
 for (int32 Index = 0; Index < Alternatives.Num(); ++Index)
 {
  const auto& Alternative = Alternatives[Index];
  if (Kind != EHansaNeedKind::Good || !HasPopulationDomain(Alternative.GoodId, TEXT("Good")) ||
      Seen.Contains(Alternative.GoodId) || Alternative.FulfillmentBasisPoints < 1 || Alternative.FulfillmentBasisPoints > 100000)
   AddPopulationIssue(OutIssues, TEXT("HSA-NEED-003"), FString::Printf(TEXT("Alternatives[%d]"), Index),
    NSLOCTEXT("HansaPopulationDefinition", "AlternativeInvalid", "Need alternatives must be unique goods with positive bounded food value."),
    NSLOCTEXT("HansaPopulationDefinition", "AlternativeRemedy", "Use unique Good.* IDs and food values from 1 to 100000; remove alternatives from services."));
  Seen.Add(Alternative.GoodId);
 }
	if ((Kind == EHansaNeedKind::Good && !HasPopulationDomain(GoodId, TEXT("Good"))) ||
		(Kind == EHansaNeedKind::Service && !GoodId.IsEmpty()))
	{
		AddPopulationIssue(OutIssues, TEXT("HSA-NEED-002"), TEXT("GoodId"),
			NSLOCTEXT("HansaPopulationDefinition", "NeedGoodContract", "Good needs require a Good.* reference and service needs must not consume an inventory good."),
			NSLOCTEXT("HansaPopulationDefinition", "NeedGoodContractRemedy", "Select an existing Good.* ID for a good need, or clear the field for a service need."));
	}
}

void UHansaNeedDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("kind=%d\ngood=%s\n"), static_cast<int32>(Kind), *GoodId);
	// Default nonseasonal definitions retain their historical fingerprints.
	if (DefaultReserveDays != 3 || bSeasonal || SeasonDays != 90 || FixedSeason != -1 || SeasonMultipliers != TArray<int32>({0,4000,10000,4000}))
	{
		InOutCanonicalData += FString::Printf(TEXT("seasonal=%d;days=%d;fixed=%d;"), bSeasonal, SeasonDays, FixedSeason);
		InOutCanonicalData += FString::Printf(TEXT("reserveDays=%d;"), DefaultReserveDays);
		for (int32 Factor : SeasonMultipliers) InOutCanonicalData += FString::Printf(TEXT("%d,"), Factor);
	}

 // Preserve legacy hashes for empty alternatives. Order is consumption preference.
 for (const auto& Alternative : Alternatives)
  InOutCanonicalData += FString::Printf(TEXT("alternative=%s:%d\n"), *Alternative.GoodId, Alternative.FulfillmentBasisPoints);
}

UHansaPopulationTierDefinition::UHansaPopulationTierDefinition()
{
	DefinitionCategory = TEXT("Population Tiers");
	LocalizationKey = TEXT("Game.PopulationTier.Unnamed");
}

void UHansaPopulationTierDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (!HasPopulationDomain(StableDefinitionId, TEXT("PopulationTier")) ||
		(!PreviousTierId.IsEmpty() && !HasPopulationDomain(PreviousTierId, TEXT("PopulationTier"))))
	{
		AddPopulationIssue(OutIssues, TEXT("HSA-TIER-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaPopulationDefinition", "TierDomain", "Tier identities and progression references must use the PopulationTier.* domain."),
			NSLOCTEXT("HansaPopulationDefinition", "TierDomainRemedy", "Assign canonical PopulationTier.* identities or clear the base tier prerequisite."));
	}
	if (Needs.IsEmpty())
	{
		AddPopulationIssue(OutIssues, TEXT("HSA-TIER-002"), TEXT("Needs"),
			NSLOCTEXT("HansaPopulationDefinition", "TierNeedsEmpty", "A population tier must define at least one need."),
			NSLOCTEXT("HansaPopulationDefinition", "TierNeedsEmptyRemedy", "Add one or more unique Need.* requirements."));
	}
	TSet<FString> SeenNeeds;
	for (int32 Index = 0; Index < Needs.Num(); ++Index)
	{
		const FHansaPopulationTierNeed& Need = Needs[Index];
		if (!HasPopulationDomain(Need.NeedId, TEXT("Need")) || SeenNeeds.Contains(Need.NeedId) ||
			Need.ConsumptionMilliUnitsPerResidentPerTick < 0 ||
			Need.ImportanceBasisPoints <= 0 || Need.ImportanceBasisPoints > 10000)
		{
			AddPopulationIssue(OutIssues, TEXT("HSA-TIER-003"), FString::Printf(TEXT("Needs[%d]"), Index),
				NSLOCTEXT("HansaPopulationDefinition", "TierNeedInvalid", "A tier need has an invalid or duplicate identity, negative consumption, or out-of-range importance."),
				NSLOCTEXT("HansaPopulationDefinition", "TierNeedInvalidRemedy", "Use each Need.* identity once, non-negative consumption and importance from 1 to 10000."));
		}
		SeenNeeds.Add(Need.NeedId);
	}
	if (WorkforcePerResidentBasisPoints < 0 || WorkforcePerResidentBasisPoints > 10000 ||
		DeclineSatisfactionBasisPoints < 0 || DeclineSatisfactionBasisPoints > 10000 ||
		GrowthSatisfactionBasisPoints < 0 || GrowthSatisfactionBasisPoints > 10000 ||
		DeclineSatisfactionBasisPoints >= GrowthSatisfactionBasisPoints || EvaluationTicks <= 0 ||
		GrowthResidentsPerEvaluation <= 0 || DeclineResidentsPerEvaluation <= 0)
	{
		AddPopulationIssue(OutIssues, TEXT("HSA-TIER-004"), TEXT("GrowthSatisfactionBasisPoints"),
			NSLOCTEXT("HansaPopulationDefinition", "TierFactorsInvalid", "Workforce, migration thresholds or evaluation values are outside their bounded deterministic ranges."),
			NSLOCTEXT("HansaPopulationDefinition", "TierFactorsInvalidRemedy", "Keep basis points within 0–10000, decline below growth, and evaluation/change values positive."));
	}
}

void UHansaPopulationTierDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	TArray<FHansaPopulationTierNeed> SortedNeeds = Needs;
	SortedNeeds.Sort([](const FHansaPopulationTierNeed& Left, const FHansaPopulationTierNeed& Right)
	{
		return Left.NeedId.Compare(Right.NeedId, ESearchCase::CaseSensitive) < 0;
	});
	for (const FHansaPopulationTierNeed& Need : SortedNeeds)
	{
		InOutCanonicalData += FString::Printf(TEXT("need=%s:%d:%d\n"), *Need.NeedId,
			Need.ConsumptionMilliUnitsPerResidentPerTick, Need.ImportanceBasisPoints);
	}
	InOutCanonicalData += FString::Printf(TEXT("previous=%s\nworkforce=%d\ngrowth=%d\ndecline=%d\nevaluationTicks=%d\ngrowthResidents=%d\ndeclineResidents=%d\n"),
		*PreviousTierId, WorkforcePerResidentBasisPoints, GrowthSatisfactionBasisPoints,
		DeclineSatisfactionBasisPoints, EvaluationTicks, GrowthResidentsPerEvaluation, DeclineResidentsPerEvaluation);
}
