#include "Definitions/HansaMarketDefinitions.h"

#include "Model/HansaIds.h"

namespace
{
	void AddMarketIssue(TArray<FHansaDefinitionValidationIssue>& OutIssues, const FName Code,
		const FString& Path, const FText& Cause, const FText& Remedy)
	{
		OutIssues.Add({ EHansaDefinitionValidationSeverity::Error, Code, Path, Cause, Remedy });
	}

	bool HasDomain(const FString& StableId, const TCHAR* Domain)
	{
		const auto Parsed = Hansa::Simulation::FHansaDefinitionId::TryParse(StableId);
		return Parsed && Parsed.Value.GetDomain() == Domain;
	}
}

UHansaCityMarketProfileDefinition::UHansaCityMarketProfileDefinition()
{
	DefinitionCategory = TEXT("City Markets");
	LocalizationKey = TEXT("Game.City.Unnamed.MarketProfile");
}

void UHansaCityMarketProfileDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (!HasDomain(StableDefinitionId, TEXT("City")))
	{
		AddMarketIssue(OutIssues, TEXT("HSA-MARKET-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaMarketDefinition", "CityDomain", "A city market profile requires a canonical City.* stable ID."),
			NSLOCTEXT("HansaMarketDefinition", "CityDomainRemedy", "Assign the stable identity of the city whose market this profile configures."));
	}
	if (UpdateCadenceTicks <= 0 || PriceHistoryCapacity <= 0 || PriceHistoryCapacity > 4096 ||
		TargetSmoothingBasisPoints <= 0 || TargetSmoothingBasisPoints > 10000 ||
		MaximumMovementBasisPointsPerUpdate <= 0 || MaximumMovementBasisPointsPerUpdate > 10000 ||
		StaleAfterTicks < UpdateCadenceTicks || ReportCadenceTicks <= 0 ||
		ReportCadenceTicks % FMath::Max(1, UpdateCadenceTicks) != 0 ||
		CurrentReportMaxAgeTicks < 0 || RecentReportMaxAgeTicks < CurrentReportMaxAgeTicks ||
		StaleReportMaxAgeTicks < RecentReportMaxAgeTicks ||
		EstimatedReportMaxAgeTicks < StaleReportMaxAgeTicks)
	{
		AddMarketIssue(OutIssues, TEXT("HSA-MARKET-002"), TEXT("UpdateCadenceTicks"),
			NSLOCTEXT("HansaMarketDefinition", "SettingsRange", "Market cadence, history, smoothing, movement or report-age settings are outside deterministic bounds."),
			NSLOCTEXT("HansaMarketDefinition", "SettingsRangeRemedy", "Use positive cadence/history, 1–10000 basis-point controls, a report cadence divisible by market cadence, and ordered current/recent/stale/estimated age limits."));
	}
	if (Goods.IsEmpty())
	{
		AddMarketIssue(OutIssues, TEXT("HSA-MARKET-003"), TEXT("Goods"),
			NSLOCTEXT("HansaMarketDefinition", "EmptyGoods", "A city market profile must configure at least one good."),
			NSLOCTEXT("HansaMarketDefinition", "EmptyGoodsRemedy", "Add a reviewed Good.* market row."));
	}
	TSet<FString> SeenGoods;
	for (int32 Index = 0; Index < Goods.Num(); ++Index)
	{
		const FHansaMarketGoodProfile& Good = Goods[Index];
		if (!HasDomain(Good.GoodId, TEXT("Good")) || SeenGoods.Contains(Good.GoodId) ||
			Good.DesiredReserveMilliUnits < 0 || Good.ConfirmedIncomingSupplyMilliUnits < 0 ||
			Good.InitialStockMilliUnits < 0 || Good.BackgroundProductionMilliUnitsPerUpdate < 0 ||
			Good.BackgroundCitizenDemandMilliUnitsPerUpdate < 0 ||
			Good.BackgroundIndustrialDemandMilliUnitsPerUpdate < 0 ||
			Good.SeasonModifierBasisPoints < -5000 || Good.SeasonModifierBasisPoints > 5000 ||
			Good.CityModifierBasisPoints < -5000 || Good.CityModifierBasisPoints > 5000 ||
			Good.MinimumPriceMilliMarks <= 0 || Good.MaximumPriceMilliMarks < Good.MinimumPriceMilliMarks ||
			Good.MaximumPriceMilliMarks > 1'000'000'000'000'000LL ||
			Good.InitialPriceMilliMarks < Good.MinimumPriceMilliMarks || Good.InitialPriceMilliMarks > Good.MaximumPriceMilliMarks)
		{
			AddMarketIssue(OutIssues, TEXT("HSA-MARKET-004"), FString::Printf(TEXT("Goods[%d]"), Index),
				NSLOCTEXT("HansaMarketDefinition", "GoodRowInvalid", "A market good row has an invalid/duplicate reference, quantity, modifier or price bound."),
				NSLOCTEXT("HansaMarketDefinition", "GoodRowInvalidRemedy", "Use each existing Good.* once, non-negative quantities, ±5000 modifiers and an initial price inside positive bounds."));
		}
		if (Good.BackgroundProductionMilliUnitsPerUpdate > 0 && Good.DesiredReserveMilliUnits <= 0)
		{
			AddMarketIssue(OutIssues, TEXT("HSA-MARKET-007"), FString::Printf(TEXT("Goods[%d].DesiredReserveMilliUnits"), Index),
				NSLOCTEXT("HansaMarketDefinition", "ProductionCeiling", "Background production requires a positive reserve to define its stock ceiling."),
				NSLOCTEXT("HansaMarketDefinition", "ProductionCeilingRemedy", "Set a positive reserve or disable background production."));
		}
		SeenGoods.Add(Good.GoodId);
	}
	if (bMarketOnly && Goods.ContainsByPredicate([](const FHansaMarketGoodProfile& Good)
	{
		return Good.InitialStockMilliUnits == 0 && Good.BackgroundProductionMilliUnitsPerUpdate == 0 &&
			Good.BackgroundCitizenDemandMilliUnitsPerUpdate == 0 &&
			Good.BackgroundIndustrialDemandMilliUnitsPerUpdate == 0;
	}))
	{
		AddMarketIssue(OutIssues, TEXT("HSA-MARKET-005"), TEXT("Goods"),
			NSLOCTEXT("HansaMarketDefinition", "EmptyMarketOnlyRow", "Every good in a market-only city requires authored starting stock or background activity."),
			NSLOCTEXT("HansaMarketDefinition", "EmptyMarketOnlyRowRemedy", "Set starting stock, background production, citizen demand or industrial demand for each simulated good."));
	}
}

void UHansaCityMarketProfileDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	InOutCanonicalData += FString::Printf(TEXT("cadence=%d\nhistory=%d\nsmoothing=%d\nmaxMovement=%d\nstaleAfter=%d\nmarketOnly=%d\nreportCadence=%d\nreportAges=%d:%d:%d:%d\n"),
		UpdateCadenceTicks, PriceHistoryCapacity, TargetSmoothingBasisPoints,
		MaximumMovementBasisPointsPerUpdate, StaleAfterTicks, bMarketOnly ? 1 : 0, ReportCadenceTicks,
		CurrentReportMaxAgeTicks, RecentReportMaxAgeTicks, StaleReportMaxAgeTicks, EstimatedReportMaxAgeTicks);
	TArray<FHansaMarketGoodProfile> Sorted = Goods;
	Sorted.Sort([](const FHansaMarketGoodProfile& Left, const FHansaMarketGoodProfile& Right)
	{
		return Left.GoodId.Compare(Right.GoodId, ESearchCase::CaseSensitive) < 0;
	});
	for (const FHansaMarketGoodProfile& Good : Sorted)
	{
		InOutCanonicalData += FString::Printf(TEXT("good=%s:%lld:%lld:%lld:%lld:%lld:%lld:%d:%d:%lld:%lld:%lld\n"), *Good.GoodId,
			static_cast<long long>(Good.DesiredReserveMilliUnits),
			static_cast<long long>(Good.ConfirmedIncomingSupplyMilliUnits),
			static_cast<long long>(Good.InitialStockMilliUnits),
			static_cast<long long>(Good.BackgroundProductionMilliUnitsPerUpdate),
			static_cast<long long>(Good.BackgroundCitizenDemandMilliUnitsPerUpdate),
			static_cast<long long>(Good.BackgroundIndustrialDemandMilliUnitsPerUpdate), Good.SeasonModifierBasisPoints,
			Good.CityModifierBasisPoints, static_cast<long long>(Good.MinimumPriceMilliMarks),
			static_cast<long long>(Good.MaximumPriceMilliMarks), static_cast<long long>(Good.InitialPriceMilliMarks));
	}
}
