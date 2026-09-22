#include "Definitions/HansaPresenceDefinitions.h"

#include "Model/HansaIds.h"

namespace
{
	void AddPresenceIssue(TArray<FHansaDefinitionValidationIssue>& OutIssues, FName Code,
		const TCHAR* Path, const FText& Cause, const FText& Remedy)
	{
		OutIssues.Add({EHansaDefinitionValidationSeverity::Error, Code, Path, Cause, Remedy});
	}

	bool HasDuplicates(TArray<FString> Values)
	{
		Values.Sort();
		for (int32 Index = 1; Index < Values.Num(); ++Index)
			if (Values[Index - 1] == Values[Index]) return true;
		return false;
	}

	bool HasDuplicates(TArray<FName> Values)
	{
		Values.Sort([](FName Left, FName Right){ return Left.ToString() < Right.ToString(); });
		for (int32 Index = 1; Index < Values.Num(); ++Index)
			if (Values[Index - 1] == Values[Index]) return true;
		return false;
	}

	FString JoinSorted(TArray<FString> Values)
	{
		Values.Sort();
		return FString::Join(Values, TEXT(","));
	}

	FString JoinSortedNames(TArray<FName> Values)
	{
		TArray<FString> Strings;
		for (FName Value : Values) Strings.Add(Value.ToString());
		return JoinSorted(MoveTemp(Strings));
	}
}

UHansaPresenceCapabilityDefinition::UHansaPresenceCapabilityDefinition()
{
	SchemaVersion = 1;
	DefinitionCategory = TEXT("Trade Presence Capabilities");
}

void UHansaPresenceCapabilityDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	if (!Hansa::Simulation::FHansaPresenceCapabilityId::TryParse(StableDefinitionId) || Semantics.IsEmpty() || CapabilitySchemaVersion != 1)
		AddPresenceIssue(OutIssues, TEXT("HSA-PRESENCE-CAPABILITY-001"), TEXT("StableDefinitionId"),
			NSLOCTEXT("HansaPresence", "InvalidCapability", "A capability requires a PresenceCapability.* identity, explicit semantics, and schema version 1."),
			NSLOCTEXT("HansaPresence", "InvalidCapabilityRemedy", "Assign a canonical identity, describe its single meaning, and retain schema version 1."));
}

void UHansaPresenceCapabilityDefinition::AppendDefinitionHashData(FString& Data) const
{
	Super::AppendDefinitionHashData(Data);
	Data += FString::Printf(TEXT("capabilitySchema=%d\nsemantics=%s\n"), CapabilitySchemaVersion, *Semantics.ToString());
}

UHansaForeignPresenceStageDefinition::UHansaForeignPresenceStageDefinition()
{
	SchemaVersion = 1;
	DefinitionCategory = TEXT("Trade Presence Stages");
}

void UHansaForeignPresenceStageDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	bool bInvalid = !Hansa::Simulation::FHansaPresenceStageId::TryParse(StableDefinitionId) || Ordinal < 0 ||
		UpgradeCostPfennig < 0 || RequiredLawfulTradeVolumeMilliUnits < 0 || RequiredCompletedDeliveries < 0 ||
		RequiredInvestedPfennig < 0 || RequiredTransactionValuePfennig < 0 || RequiredFulfilledShortageMilliUnits < 0 ||
		RequiredReliableOperatingTicks < 0 || RequiredSolventOperatingTicks < 0 || UpgradeConstructionTicks <= 0 ||
		HasDuplicates(PrerequisiteStageIds) || HasDuplicates(GrantedCapabilityIds) ||
		HasDuplicates(PermittedPlotCategories) || HasDuplicates(PermittedBuildingCategories);
	for (const FString& Id : PrerequisiteStageIds) bInvalid |= !Hansa::Simulation::FHansaPresenceStageId::TryParse(Id) || Id == StableDefinitionId;
	for (const FString& Id : GrantedCapabilityIds) bInvalid |= !Hansa::Simulation::FHansaPresenceCapabilityId::TryParse(Id);
	TArray<FString> CostGoods;
	for (const FHansaPresenceUpgradeGoodCost& Cost : UpgradeGoods)
	{
		bInvalid |= !Hansa::Simulation::FHansaGoodId::TryParse(Cost.GoodId) || Cost.QuantityMilliUnits < 0;
		CostGoods.Add(Cost.GoodId);
	}
	bInvalid |= HasDuplicates(CostGoods);
	for (FName Value : PermittedPlotCategories) bInvalid |= Value != TEXT("Commercial") && Value != TEXT("Industrial") && Value != TEXT("Civic");
	for (FName Value : PermittedBuildingCategories) bInvalid |= Value != TEXT("Storage") && Value != TEXT("Commercial") && Value != TEXT("Production") && Value != TEXT("Civic");
	if (bInvalid)
		AddPresenceIssue(OutIssues, TEXT("HSA-PRESENCE-STAGE-001"), TEXT("PresenceStage"),
			NSLOCTEXT("HansaPresence", "InvalidStage", "The presence stage contains an invalid identity, duplicate, self prerequisite, or negative requirement/cost."),
			NSLOCTEXT("HansaPresence", "InvalidStageRemedy", "Use unique typed references and non-negative authored costs and requirements."));
}

void UHansaForeignPresenceStageDefinition::AppendDefinitionHashData(FString& Data) const
{
	Super::AppendDefinitionHashData(Data);
	Data += FString::Printf(TEXT("ordinal=%d\nprerequisites=%s\ncapabilities=%s\nplots=%s\nbuildings=%s\nmoney=%lld\ntrade=%lld\ndeliveries=%lld\ninvested=%lld\ntransactionValue=%lld\nshortageRelief=%lld\nreliableTicks=%lld\nsolventTicks=%lld\nconstructionTicks=%d\n"),
		Ordinal, *JoinSorted(PrerequisiteStageIds), *JoinSorted(GrantedCapabilityIds), *JoinSortedNames(PermittedPlotCategories),
		*JoinSortedNames(PermittedBuildingCategories), static_cast<long long>(UpgradeCostPfennig),
		static_cast<long long>(RequiredLawfulTradeVolumeMilliUnits), static_cast<long long>(RequiredCompletedDeliveries),
		static_cast<long long>(RequiredInvestedPfennig), static_cast<long long>(RequiredTransactionValuePfennig),
		static_cast<long long>(RequiredFulfilledShortageMilliUnits), static_cast<long long>(RequiredReliableOperatingTicks),
		static_cast<long long>(RequiredSolventOperatingTicks), UpgradeConstructionTicks);
	TArray<FHansaPresenceUpgradeGoodCost> Costs = UpgradeGoods;
	Costs.Sort([](const auto& Left, const auto& Right){ return Left.GoodId < Right.GoodId; });
	for (const auto& Cost : Costs) Data += FString::Printf(TEXT("cost=%s|%lld\n"), *Cost.GoodId, static_cast<long long>(Cost.QuantityMilliUnits));
}

UHansaCityTradePolicyDefinition::UHansaCityTradePolicyDefinition()
{
	SchemaVersion = 1;
	DefinitionCategory = TEXT("City Trade Policies");
}

void UHansaCityTradePolicyDefinition::ValidateDefinition(TArray<FHansaDefinitionValidationIssue>& OutIssues) const
{
	Super::ValidateDefinition(OutIssues);
	bool bInvalid = !Hansa::Simulation::FHansaCityTradePolicyId::TryParse(StableDefinitionId) ||
		!Hansa::Simulation::FHansaCityDefinitionId::TryParse(CityId) || AllowedStageIds.IsEmpty() ||
		HasDuplicates(AllowedStageIds) || HasDuplicates(DeniedCapabilityIds) || HasDuplicates(AllowedPlotCategories) ||
		HasDuplicates(AllowedBuildingCategories) || (!InitialStageId.IsEmpty() && !AllowedStageIds.Contains(InitialStageId));
	for (const FString& Id : AllowedStageIds) bInvalid |= !Hansa::Simulation::FHansaPresenceStageId::TryParse(Id);
	for (const FString& Id : DeniedCapabilityIds) bInvalid |= !Hansa::Simulation::FHansaPresenceCapabilityId::TryParse(Id);
	for (FName Value : AllowedPlotCategories) bInvalid |= Value != TEXT("Commercial") && Value != TEXT("Industrial") && Value != TEXT("Civic");
	for (FName Value : AllowedBuildingCategories) bInvalid |= Value != TEXT("Storage") && Value != TEXT("Commercial") && Value != TEXT("Production") && Value != TEXT("Civic");
	 bInvalid |= MaximumStationOrders < 1 || MaximumStationOrders > 64 || MaximumOrderCapMilliUnits < 1 || MaximumOrderCapMilliUnits > 1000000000 || MaximumOrderBudgetPfennig < 1 || MaximumOrderBudgetPfennig > 1000000000000LL ||
		MerchantOfficeStorageBonusMilliUnits < 0 || MerchantOfficeStorageBonusMilliUnits > 1000000000 || MerchantOfficeAdditionalOrderSlots < 0 || MerchantOfficeAdditionalOrderSlots > 64;
	TArray<FName> SiteIds;
	for (const FHansaTradeStationSiteDefinition& Site : TradeStationSites)
	{
		bInvalid |= Site.SiteId.IsNone() || Site.PlotCategory != TEXT("Commercial") || Site.StorageCapacityMilliUnits <= 0 ||
			Site.ConstructionTicks <= 0 || Site.UpkeepPfennigPerTick < 0 || Site.CancellationRefundBasisPoints < 0 ||
			Site.CancellationRefundBasisPoints > 10000 || Site.PresentationClass.IsNull() || Site.LeaseBoundsMin.X > Site.LeaseBoundsMax.X || Site.LeaseBoundsMin.Y > Site.LeaseBoundsMax.Y || Site.PermittedBuildingCategories.IsEmpty() || HasDuplicates(Site.PermittedBuildingCategories);
		SiteIds.Add(Site.SiteId);
	}
	bInvalid |= HasDuplicates(SiteIds);
	TArray<FName> SpecializationIds;
	for (const FHansaPresenceSpecializationDefinition& Branch : Specializations)
	{
		bInvalid |= Branch.SpecializationId.IsNone() || Branch.DisplayName.IsEmpty() || Branch.RequiredStageId.IsEmpty() ||
			Branch.ExclusiveGroupId.IsNone() || Branch.GrantedCapabilityIds.IsEmpty() || Branch.InvestmentCostPfennig < 0 ||
			Branch.RespecRefundBasisPoints < 0 || Branch.RespecRefundBasisPoints > 10000 || Branch.StorageCapacityBonusMilliUnits < 0 ||
			Branch.AdditionalOrderSlots < 0 || Branch.AdditionalOrderSlots > 64 || Branch.StationTransferCapBonusMilliUnits < 0;
		bInvalid |= !AllowedStageIds.Contains(Branch.RequiredStageId) || !Hansa::Simulation::FHansaPresenceStageId::TryParse(Branch.RequiredStageId) || HasDuplicates(Branch.GrantedCapabilityIds);
		for (const FString& CapabilityId : Branch.GrantedCapabilityIds) bInvalid |= !Hansa::Simulation::FHansaPresenceCapabilityId::TryParse(CapabilityId);
		for (const FHansaPresenceUpgradeGoodCost& Cost : Branch.InvestmentGoods) bInvalid |= !Hansa::Simulation::FHansaGoodId::TryParse(Cost.GoodId) || Cost.QuantityMilliUnits <= 0;
		SpecializationIds.Add(Branch.SpecializationId);
	}
	bInvalid |= HasDuplicates(SpecializationIds);
	TArray<FName> PrivilegeIds;for(const auto& V:Privileges){PrivilegeIds.Add(V.PrivilegeId);bInvalid|=V.PrivilegeId.IsNone()||V.DisplayName.IsEmpty()||!AllowedStageIds.Contains(V.RequiredStageId)||V.CostPfennig<0||V.DurationTicks<0||V.LeaseBoundsMin.X>V.LeaseBoundsMax.X||V.LeaseBoundsMin.Y>V.LeaseBoundsMax.Y||V.PermittedBuildingCategories.IsEmpty();for(const auto& C:V.CostGoods)bInvalid|=!Hansa::Simulation::FHansaGoodId::TryParse(C.GoodId)||C.QuantityMilliUnits<=0;}bInvalid|=HasDuplicates(PrivilegeIds);
	TArray<FName> ProjectIds;for(const auto& V:CityProjects){ProjectIds.Add(V.ProjectId);bInvalid|=V.ProjectId.IsNone()||V.DisplayName.IsEmpty()||!AllowedStageIds.Contains(V.RequiredStageId)||V.CostPfennig<0||V.ConstructionTicks<=0||!Hansa::Simulation::FHansaGoodId::TryParse(V.SharedReserveGoodId)||V.SharedReserveBonusMilliUnits<=0;for(const auto& C:V.CostGoods)bInvalid|=!Hansa::Simulation::FHansaGoodId::TryParse(C.GoodId)||C.QuantityMilliUnits<=0;}bInvalid|=HasDuplicates(ProjectIds)||HasDuplicates(GovernanceScenarioIds)||(bExceptionalGovernanceAllowed&&(GovernanceCharterId.IsNone()||GovernanceScenarioIds.IsEmpty()));
	if (!InitialStageId.IsEmpty()) bInvalid |= !Hansa::Simulation::FHansaPresenceStageId::TryParse(InitialStageId);
	if (bInvalid)
		AddPresenceIssue(OutIssues, TEXT("HSA-PRESENCE-POLICY-001"), TEXT("CityTradePolicy"),
			NSLOCTEXT("HansaPresence", "InvalidPolicy", "The city policy has invalid, duplicate, empty, or contradictory stage/capability references."),
			NSLOCTEXT("HansaPresence", "InvalidPolicyRemedy", "Use one City.* target, unique typed references, and include the initial stage in the allowed ladder."));
}

void UHansaCityTradePolicyDefinition::AppendDefinitionHashData(FString& Data) const
{
	Super::AppendDefinitionHashData(Data);
	Data += FString::Printf(TEXT("city=%s\ninitial=%s\nstages=%s\ndenied=%s\nplots=%s\nbuildings=%s\npublic=%d\ngovernance=%d\n"),
		*CityId, *InitialStageId, *JoinSorted(AllowedStageIds), *JoinSorted(DeniedCapabilityIds),
		*JoinSortedNames(AllowedPlotCategories), *JoinSortedNames(AllowedBuildingCategories),
		bPublicMarketAccess ? 1 : 0, bExceptionalGovernanceAllowed ? 1 : 0);
    if(MaximumStationOrders != 8 || MaximumOrderCapMilliUnits != 50000 || MaximumOrderBudgetPfennig != 1000000)
        Data += FString::Printf(TEXT("orderLimits=%d|%lld|%lld\n"), MaximumStationOrders, MaximumOrderCapMilliUnits, MaximumOrderBudgetPfennig);
	Data += FString::Printf(TEXT("merchantOffice=%lld|%d\n"), static_cast<long long>(MerchantOfficeStorageBonusMilliUnits), MerchantOfficeAdditionalOrderSlots);
	for(const auto& V:Privileges)Data+=FString::Printf(TEXT("privilege=%s|%s|%lld|%lld|%d|%d|%d|%d|%d|%s\n"),*V.PrivilegeId.ToString(),*V.RequiredStageId,(long long)V.CostPfennig,(long long)V.DurationTicks,V.bReversible?1:0,V.LeaseBoundsMin.X,V.LeaseBoundsMin.Y,V.LeaseBoundsMax.X,V.LeaseBoundsMax.Y,*JoinSortedNames(V.PermittedBuildingCategories));
	for(const auto& V:CityProjects)Data+=FString::Printf(TEXT("project=%s|%s|%lld|%d|%s|%lld\n"),*V.ProjectId.ToString(),*V.RequiredStageId,(long long)V.CostPfennig,V.ConstructionTicks,*V.SharedReserveGoodId,(long long)V.SharedReserveBonusMilliUnits);
	Data+=FString::Printf(TEXT("governance=%s|%s\n"),*GovernanceCharterId.ToString(),*JoinSorted(GovernanceScenarioIds));
	TArray<FHansaTradeStationSiteDefinition> Sites = TradeStationSites;
	Sites.Sort([](const auto& Left, const auto& Right){ return Left.SiteId.ToString() < Right.SiteId.ToString(); });
	for (const auto& Site : Sites)
	{
		Data += FString::Printf(TEXT("stationSite=%s|%s|%lld|%d|%lld|%d|%s\n"), *Site.SiteId.ToString(), *Site.PlotCategory.ToString(),
			static_cast<long long>(Site.StorageCapacityMilliUnits), Site.ConstructionTicks, static_cast<long long>(Site.UpkeepPfennigPerTick),
			Site.CancellationRefundBasisPoints, *Site.PresentationClass.ToString());
		Data += FString::Printf(TEXT("leaseBounds=%d|%d|%d|%d|%s\n"), Site.LeaseBoundsMin.X, Site.LeaseBoundsMin.Y,
			Site.LeaseBoundsMax.X, Site.LeaseBoundsMax.Y, *JoinSortedNames(Site.PermittedBuildingCategories));
	}
	TArray<FHansaPresenceSpecializationDefinition> Branches = Specializations;
	Branches.Sort([](const auto& Left, const auto& Right){ return Left.SpecializationId.LexicalLess(Right.SpecializationId); });
	for (const auto& Branch : Branches)
	{
		Data += FString::Printf(TEXT("specialization=%s|%s|%s|%lld|%d|%lld|%d|%lld\n"), *Branch.SpecializationId.ToString(), *Branch.RequiredStageId,
			*Branch.ExclusiveGroupId.ToString(), static_cast<long long>(Branch.InvestmentCostPfennig), Branch.RespecRefundBasisPoints,
			static_cast<long long>(Branch.StorageCapacityBonusMilliUnits), Branch.AdditionalOrderSlots, static_cast<long long>(Branch.StationTransferCapBonusMilliUnits));
		Data += TEXT("specializationCapabilities=") + JoinSorted(Branch.GrantedCapabilityIds) + TEXT("\n");
		TArray<FHansaPresenceUpgradeGoodCost> Costs = Branch.InvestmentGoods; Costs.Sort([](const auto& L,const auto& R){return L.GoodId<R.GoodId;});
		for(const auto& Cost:Costs) Data += FString::Printf(TEXT("specializationCost=%s|%lld\n"),*Cost.GoodId,static_cast<long long>(Cost.QuantityMilliUnits));
	}
}
