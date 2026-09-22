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

UHansaProductionChainDefinition::UHansaProductionChainDefinition()
{
	DefinitionCategory = TEXT("Production Chains");
	LocalizationKey = TEXT("Game.ProductionChain.Unnamed");
}

void UHansaProductionChainDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (!HasDomain(StableDefinitionId, TEXT("ProductionChain")) || Stages.IsEmpty())
		AddMarketIssue(OutIssues, TEXT("HSA-CHAIN-001"), TEXT("Stages"),
			NSLOCTEXT("HansaMarketDefinition", "InvalidChain", "A ProductionChain.* definition requires at least one stage."),
			NSLOCTEXT("HansaMarketDefinition", "InvalidChainFix", "Assign a stable chain identity and one or more recipe stages."));
	TSet<FString> Keys;
	for (int32 Index=0; Index<Stages.Num(); ++Index)
	{
		const auto& Stage=Stages[Index];
		if (Stage.StageKey.IsEmpty() || !HasDomain(Stage.RecipeId, TEXT("Recipe")) || Keys.Contains(Stage.StageKey))
			AddMarketIssue(OutIssues, TEXT("HSA-CHAIN-002"), FString::Printf(TEXT("Stages[%d]"),Index),
				NSLOCTEXT("HansaMarketDefinition", "InvalidStage", "A production stage has an empty/duplicate key or invalid recipe reference."),
				NSLOCTEXT("HansaMarketDefinition", "InvalidStageFix", "Use a unique stage key and Recipe.* reference."));
		for(const FString& Required:Stage.PrerequisiteStageKeys) if(!Keys.Contains(Required))
			AddMarketIssue(OutIssues, TEXT("HSA-CHAIN-003"), FString::Printf(TEXT("Stages[%d].PrerequisiteStageKeys"),Index),
				NSLOCTEXT("HansaMarketDefinition", "StageOrder", "A prerequisite must name an earlier stage in the same ordered chain."),
				NSLOCTEXT("HansaMarketDefinition", "StageOrderFix", "Reorder stages or correct the prerequisite key."));
		Keys.Add(Stage.StageKey);
	}
}

void UHansaProductionChainDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	for(const auto& Stage:Stages)
	{
		TArray<FString> Prerequisites=Stage.PrerequisiteStageKeys; Prerequisites.Sort();
		InOutCanonicalData+=FString::Printf(TEXT("stage=%s:%s:%d:%s:%s\n"),*Stage.StageKey,*Stage.RecipeId,
			static_cast<int32>(Stage.Role),*Stage.IntendedConstructionTier,*FString::Join(Prerequisites,TEXT(",")));
	}
}

UHansaRegionEconomicProfileDefinition::UHansaRegionEconomicProfileDefinition()
{
	DefinitionCategory = TEXT("Regional Economies");
	LocalizationKey = TEXT("Game.Region.Unnamed.Economy");
}

void UHansaRegionEconomicProfileDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if(!HasDomain(StableDefinitionId,TEXT("Region")) || MemberCityIds.Num()<5 || MemberCityIds.Num()>6 || ExchangeCapacityMilliUnitsPerUpdate<0 || ExchangeDelayUpdates<1 || TransportLossBasisPoints<0 || TransportLossBasisPoints>10000)
		AddMarketIssue(OutIssues,TEXT("HSA-REGION-001"),TEXT("MemberCityIds"),NSLOCTEXT("HansaMarketDefinition","InvalidRegion","A Region.* economy requires five or six cities and bounded exchange settings."),NSLOCTEXT("HansaMarketDefinition","InvalidRegionFix","Use five or six unique City.* members and bounded exchange settings."));
	TSet<FString> Cities; for(const FString& City:MemberCityIds){if(!HasDomain(City,TEXT("City"))||Cities.Contains(City)) AddMarketIssue(OutIssues,TEXT("HSA-REGION-002"),TEXT("MemberCityIds"),NSLOCTEXT("HansaMarketDefinition","RegionCity","Region members must be unique City.* references."),NSLOCTEXT("HansaMarketDefinition","RegionCityFix","Correct the duplicate or invalid city identity.")); Cities.Add(City);}
	TSet<FString> Goods; for(const auto& E:ResourceEndowments){if(!HasDomain(E.GoodId,TEXT("Good"))||Goods.Contains(E.GoodId)||E.SourceCapacityMilliUnitsPerUpdate<0) AddMarketIssue(OutIssues,TEXT("HSA-REGION-003"),TEXT("ResourceEndowments"),NSLOCTEXT("HansaMarketDefinition","RegionResource","Region resource rows must uniquely reference Good.* with non-negative capacity."),NSLOCTEXT("HansaMarketDefinition","RegionResourceFix","Correct the good or capacity.")); Goods.Add(E.GoodId);}
}

void UHansaRegionEconomicProfileDefinition::AppendDefinitionHashData(FString& InOutCanonicalData) const
{
	Super::AppendDefinitionHashData(InOutCanonicalData);
	TArray<FString> Cities=MemberCityIds; Cities.Sort();
	InOutCanonicalData+=FString::Printf(TEXT("cities=%s\nexchange=%lld:%d:%d:%lld\n"),*FString::Join(Cities,TEXT(",")),static_cast<long long>(ExchangeCapacityMilliUnitsPerUpdate),ExchangeDelayUpdates,TransportLossBasisPoints,static_cast<long long>(TransportCostMilliMarksPerUnit));
	for(const auto& P:PermittedStages){TArray<FString> Keys=P.StageKeys;Keys.Sort();InOutCanonicalData+=TEXT("permit=")+P.ProductionChainId+TEXT(":")+FString::Join(Keys,TEXT(","))+TEXT("\n");}
	TArray<FHansaRegionResourceEndowmentDefinition> Resources=ResourceEndowments;Resources.Sort([](const auto& L,const auto& R){return L.GoodId<R.GoodId;});
	for(const auto& E:Resources)InOutCanonicalData+=FString::Printf(TEXT("resource=%s:%d:%lld\n"),*E.GoodId,static_cast<int32>(E.Endowment),static_cast<long long>(E.SourceCapacityMilliUnitsPerUpdate));
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
	if ((!RegionId.IsEmpty() && !HasDomain(RegionId,TEXT("Region"))) || (!IndustryBindings.IsEmpty() && RegionId.IsEmpty()))
		AddMarketIssue(OutIssues,TEXT("HSA-MARKET-REGION-001"),TEXT("RegionId"),NSLOCTEXT("HansaMarketDefinition","CityRegion","Industry bindings require a Region.* reference."),NSLOCTEXT("HansaMarketDefinition","CityRegionFix","Assign the city's regional economic profile."));
	TSet<FString> BindingKeys; int32 SignatureCount=0;
	for(int32 Index=0;Index<IndustryBindings.Num();++Index)
	{
		const auto& B=IndustryBindings[Index]; TArray<FString> OrderedKeys=B.EnabledStageKeys; OrderedKeys.Sort();
		const FString Key=B.ProductionChainId+TEXT("|")+FString::Join(OrderedKeys,TEXT(",")); SignatureCount+=B.SignatureRank>0?1:0;
		if(!HasDomain(B.ProductionChainId,TEXT("ProductionChain"))||B.EnabledStageKeys.IsEmpty()||BindingKeys.Contains(Key)||B.CyclesPerMarketUpdate<=0||B.EfficiencyBasisPoints<=0||B.EfficiencyBasisPoints>50000||B.InputReserveMilliUnits<0||B.OutputReserveMilliUnits<0||B.SignatureRank<0||B.SignatureRank>2)
			AddMarketIssue(OutIssues,TEXT("HSA-MARKET-REGION-002"),FString::Printf(TEXT("IndustryBindings[%d]"),Index),NSLOCTEXT("HansaMarketDefinition","CityIndustry","A city industry binding is invalid or duplicated."),NSLOCTEXT("HansaMarketDefinition","CityIndustryFix","Use a valid chain, non-empty stages, positive cycles/efficiency, non-negative reserves and signature rank 0-2."));
		BindingKeys.Add(Key);
	}
	if(SignatureCount>2) AddMarketIssue(OutIssues,TEXT("HSA-MARKET-REGION-003"),TEXT("IndustryBindings"),NSLOCTEXT("HansaMarketDefinition","TooManySignatures","A city may have at most two signature industry families."),NSLOCTEXT("HansaMarketDefinition","TooManySignaturesFix","Reduce signature rankings while retaining ordinary industry bindings."));
	if (PresentationClass != EHansaCityPresentationClass::Unspecified &&
		(MapLongitudeMilliDegrees < -180000 || MapLongitudeMilliDegrees > 180000 ||
		 MapLatitudeMilliDegrees < -90000 || MapLatitudeMilliDegrees > 90000 ||
		 (MapLongitudeMilliDegrees == 0 && MapLatitudeMilliDegrees == 0)))
	{
		AddMarketIssue(OutIssues, TEXT("HSA-MARKET-PRESENTATION-001"), TEXT("MapLongitudeMilliDegrees"),
			NSLOCTEXT("HansaMarketDefinition", "CityMapCoordinate", "An explicitly classified city requires reviewed non-zero map coordinates inside geographic bounds."),
			NSLOCTEXT("HansaMarketDefinition", "CityMapCoordinateFix", "Author approximate longitude/latitude in milli-degrees from the reviewed regional catalog."));
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
	InOutCanonicalData+=TEXT("region=")+RegionId+TEXT("\n");
	if (PresentationClass != EHansaCityPresentationClass::Unspecified || MapLongitudeMilliDegrees != 0 || MapLatitudeMilliDegrees != 0)
	{
		InOutCanonicalData += FString::Printf(TEXT("presentation=%d:%d:%d\n"),
			static_cast<int32>(PresentationClass), MapLongitudeMilliDegrees, MapLatitudeMilliDegrees);
	}
	TArray<FHansaCityIndustryBindingDefinition> Bindings=IndustryBindings;
	Bindings.Sort([](const auto& L,const auto& R){return L.ProductionChainId<R.ProductionChainId;});
	for(const auto& B:Bindings)
	{
		TArray<FString> Keys=B.EnabledStageKeys; Keys.Sort();
		InOutCanonicalData+=FString::Printf(TEXT("industry=%s:%s:%d:%d:%lld:%lld:%d:%d\n"),*B.ProductionChainId,*FString::Join(Keys,TEXT(",")),B.CyclesPerMarketUpdate,B.EfficiencyBasisPoints,static_cast<long long>(B.InputReserveMilliUnits),static_cast<long long>(B.OutputReserveMilliUnits),B.bEnabled?1:0,B.SignatureRank);
	}
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
