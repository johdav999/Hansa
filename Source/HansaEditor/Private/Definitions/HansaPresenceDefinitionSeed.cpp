#include "Definitions/HansaPresenceDefinitionSeed.h"

#include "Definitions/HansaPresenceDefinitions.h"

namespace Hansa::Editor::EconomicDefinitions
{
	namespace
	{
		void Configure(UHansaDefinitionBase& Definition, const FString& StableId, const FString& DisplayName)
		{
			Definition.StableDefinitionId = StableId;
			Definition.LocalizationKey = FName(*(TEXT("Game.") + StableId + TEXT(".Name")));
			Definition.DisplayName = FText::ChangeKey(TEXT("HansaDefinitions"), Definition.LocalizationKey.ToString(), FText::FromString(DisplayName));
			Definition.ContentSet = TEXT("MVP");
			Definition.AuthoredRevision = 1;
			Definition.Tags = {TEXT("MVP"), TEXT("TradePresence")};
		}

		template <typename T>
		T* Add(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer,
			const FString& StableId, const FString& DisplayName)
		{
			T* Value = NewObject<T>(Outer, MakeUniqueObjectName(Outer, T::StaticClass(), FName(*StableId.Replace(TEXT("."), TEXT("_")))), RF_Transactional);
			Configure(*Value, StableId, DisplayName);
			Definitions.Add(TStrongObjectPtr<UHansaDefinitionBase>(Value));
			return Value;
		}

		FHansaPresenceUpgradeGoodCost Cost(const TCHAR* GoodId, int64 Quantity)
		{
			FHansaPresenceUpgradeGoodCost Value; Value.GoodId = GoodId; Value.QuantityMilliUnits = Quantity; return Value;
		}
	}

	void AppendTradePresenceDefinitions(TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, UObject* Outer)
	{
		const auto Capability = [&](const TCHAR* Name, const TCHAR* Display, const TCHAR* Semantics)
		{
			const FString StableId = TEXT("PresenceCapability.") + FString(Name);
			auto* Value = Add<UHansaPresenceCapabilityDefinition>(Definitions, Outer, StableId, Display);
			Value->Semantics = FText::ChangeKey(TEXT("HansaDefinitions"), TEXT("Game.") + StableId + TEXT(".Semantics"), FText::FromString(Semantics));
			Value->RefreshContentHash();
		};
		Capability(TEXT("PublicMarketTrade"), TEXT("Public market trade"), TEXT("Permits public-market transactions only when a later command explicitly consumes this capability."));
		Capability(TEXT("MarketReports"), TEXT("Market reports"), TEXT("Permits city market reports only when a later reporting system explicitly consumes this capability."));
		Capability(TEXT("RouteAccess"), TEXT("Route access"), TEXT("Permits the city as a route endpoint only when a later command explicitly consumes this capability."));
		Capability(TEXT("TradeStation"), TEXT("Trade station"), TEXT("Permits one bounded station only when a later command explicitly consumes this capability."));
		Capability(TEXT("LocalStorage"), TEXT("Local storage"), TEXT("Permits local foreign-house stock only when a later inventory system explicitly consumes this capability."));
		Capability(TEXT("ResidentFactor"), TEXT("Resident factor"), TEXT("Permits a resident factor only when a later personnel system explicitly consumes this capability."));
		Capability(TEXT("StationOrders"), TEXT("Station orders"), TEXT("Permits bounded station orders only when a later command explicitly consumes this capability."));
		Capability(TEXT("MerchantOffice"), TEXT("Merchant office"), TEXT("Permits a merchant office only when a later construction command explicitly consumes this capability."));
		Capability(TEXT("CommercialPlots"), TEXT("Commercial plots"), TEXT("Permits commercial leases only when a later land command explicitly consumes this capability."));
		Capability(TEXT("MerchantQuarter"), TEXT("Merchant quarter"), TEXT("Permits a merchant quarter only when a later district command explicitly consumes this capability."));
		Capability(TEXT("IndustrialPlots"), TEXT("Industrial plots"), TEXT("Permits industrial leases only when a later land command explicitly consumes this capability."));
		Capability(TEXT("CityProjects"), TEXT("City projects"), TEXT("Permits city-project contributions only when a later command explicitly consumes this capability."));
		Capability(TEXT("Privileges"), TEXT("Commercial privileges"), TEXT("Permits explicitly authored privileges only when a later command consumes this capability."));
		Capability(TEXT("Governance"), TEXT("Exceptional governance"), TEXT("Permits scenario-gated governance only when an authored policy and later command explicitly consume it."));
		Capability(TEXT("WarehouseSpecialization"), TEXT("Warehouse specialization"), TEXT("Adds bounded local storage capacity to the active merchant office station."));
		Capability(TEXT("MarketSpecialization"), TEXT("Market specialization"), TEXT("Adds bounded order slots and enables reviewed price-limited station orders."));
		Capability(TEXT("HarborSpecialization"), TEXT("Harbor specialization"), TEXT("Adds bounded station handling throughput for route operations."));

		const auto Stage = [&](const TCHAR* Name, const TCHAR* Display, int32 Ordinal, TArray<FString> Prerequisites,
			TArray<FString> Capabilities, TArray<FName> Plots, TArray<FName> Buildings, int64 Money,
			TArray<FHansaPresenceUpgradeGoodCost> Goods, int64 Trade, int64 Deliveries, int64 Investment,
			int64 TransactionValue, int64 ShortageRelief, int64 ReliableTicks, int64 SolventTicks)
		{
			auto* Value = Add<UHansaForeignPresenceStageDefinition>(Definitions, Outer, TEXT("PresenceStage.") + FString(Name), Display);
			Value->Ordinal=Ordinal; Value->PrerequisiteStageIds=MoveTemp(Prerequisites); Value->GrantedCapabilityIds=MoveTemp(Capabilities);
			Value->PermittedPlotCategories=MoveTemp(Plots); Value->PermittedBuildingCategories=MoveTemp(Buildings);
			Value->UpgradeCostPfennig=Money; Value->UpgradeGoods=MoveTemp(Goods); Value->RequiredLawfulTradeVolumeMilliUnits=Trade;
			Value->RequiredCompletedDeliveries=Deliveries; Value->RequiredInvestedPfennig=Investment;
			Value->RequiredTransactionValuePfennig=TransactionValue; Value->RequiredFulfilledShortageMilliUnits=ShortageRelief;
			Value->RequiredReliableOperatingTicks=ReliableTicks; Value->RequiredSolventOperatingTicks=SolventTicks; Value->UpgradeConstructionTicks=3; Value->RefreshContentHash();
		};
		const TArray<FString> ContactCaps={TEXT("PresenceCapability.PublicMarketTrade"),TEXT("PresenceCapability.MarketReports"),TEXT("PresenceCapability.RouteAccess")};
		Stage(TEXT("VisitingContact"),TEXT("Visiting contact"),0,{},ContactCaps,{}, {},0,{},0,0,0,0,0,0,0);
		auto StationCaps=ContactCaps; StationCaps.Append({TEXT("PresenceCapability.TradeStation"),TEXT("PresenceCapability.LocalStorage"),TEXT("PresenceCapability.StationOrders")});
		Stage(TEXT("TradeStation"),TEXT("Trade station"),1,{TEXT("PresenceStage.VisitingContact")},StationCaps,{TEXT("Commercial")},{TEXT("Storage")},50000,{Cost(TEXT("Good.Timber"),8000),Cost(TEXT("Good.Planks"),4000)},50000,3,25000,50000,10000,3,3);
		auto OfficeCaps=StationCaps; OfficeCaps.Append({TEXT("PresenceCapability.ResidentFactor"),TEXT("PresenceCapability.MerchantOffice"),TEXT("PresenceCapability.CommercialPlots")});
		Stage(TEXT("MerchantOffice"),TEXT("Merchant office"),2,{TEXT("PresenceStage.TradeStation")},OfficeCaps,{TEXT("Commercial")},{TEXT("Storage"),TEXT("Commercial")},150000,{Cost(TEXT("Good.Planks"),12000),Cost(TEXT("Good.Tools"),2000)},250000,12,100000,250000,50000,12,8);
		auto QuarterCaps=OfficeCaps; QuarterCaps.Append({TEXT("PresenceCapability.MerchantQuarter"),TEXT("PresenceCapability.IndustrialPlots")});
		Stage(TEXT("MerchantQuarter"),TEXT("Merchant quarter"),3,{TEXT("PresenceStage.MerchantOffice")},QuarterCaps,{TEXT("Commercial"),TEXT("Industrial")},{TEXT("Storage"),TEXT("Commercial"),TEXT("Production")},500000,{Cost(TEXT("Good.Planks"),30000),Cost(TEXT("Good.Tools"),6000)},1000000,40,400000,1000000,250000,40,30);
		auto PrivilegedCaps=QuarterCaps; PrivilegedCaps.Append({TEXT("PresenceCapability.CityProjects"),TEXT("PresenceCapability.Privileges")});
		Stage(TEXT("PrivilegedPresence"),TEXT("Privileged presence"),4,{TEXT("PresenceStage.MerchantQuarter")},PrivilegedCaps,{TEXT("Commercial"),TEXT("Industrial")},{TEXT("Storage"),TEXT("Commercial"),TEXT("Production"),TEXT("Civic")},1250000,{Cost(TEXT("Good.Planks"),60000),Cost(TEXT("Good.Tools"),12000)},3000000,100,1000000,3000000,750000,100,80);
		auto GovernanceCaps=PrivilegedCaps; GovernanceCaps.Add(TEXT("PresenceCapability.Governance"));
		Stage(TEXT("ExceptionalGovernance"),TEXT("Exceptional charter and governance"),5,{TEXT("PresenceStage.PrivilegedPresence")},GovernanceCaps,{TEXT("Commercial"),TEXT("Industrial"),TEXT("Civic")},{TEXT("Storage"),TEXT("Commercial"),TEXT("Production"),TEXT("Civic")},5000000,{Cost(TEXT("Good.Planks"),120000),Cost(TEXT("Good.Tools"),25000)},10000000,250,4000000,10000000,2500000,250,200);

		const TArray<FString> StandardStages={TEXT("PresenceStage.VisitingContact"),TEXT("PresenceStage.TradeStation"),TEXT("PresenceStage.MerchantOffice"),TEXT("PresenceStage.MerchantQuarter"),TEXT("PresenceStage.PrivilegedPresence")};
		const auto Policy = [&](const TCHAR* City, const TCHAR* Initial)
		{
			auto* Value=Add<UHansaCityTradePolicyDefinition>(Definitions,Outer,TEXT("CityTradePolicy.")+FString(City),FString(City)+TEXT(" trade policy"));
			Value->CityId=TEXT("City.")+FString(City); Value->InitialStageId=Initial; Value->AllowedStageIds=StandardStages;
			Value->AllowedPlotCategories={TEXT("Commercial"),TEXT("Industrial")}; Value->AllowedBuildingCategories={TEXT("Storage"),TEXT("Commercial"),TEXT("Production"),TEXT("Civic")};
			Value->bPublicMarketAccess=true; Value->bExceptionalGovernanceAllowed=FString(City)==TEXT("Hamburg"); if(Value->bExceptionalGovernanceAllowed){Value->AllowedStageIds.Add(TEXT("PresenceStage.ExceptionalGovernance"));Value->GovernanceScenarioIds={TEXT("Scenario.LubeckGrainShortageV1")};Value->GovernanceCharterId=TEXT("Charter.Hamburg.FoundingCouncil");}
			const auto Branch=[&](const TCHAR* Id,const TCHAR* Display,const TCHAR* CapabilityId,int64 Money,TArray<FHansaPresenceUpgradeGoodCost> Goods,int64 Storage,int32 Slots,int64 Handling)
			{
				FHansaPresenceSpecializationDefinition B;B.SpecializationId=FName(Id);B.DisplayName=FText::FromString(Display);B.GrantedCapabilityIds={CapabilityId};
				B.InvestmentCostPfennig=Money;B.InvestmentGoods=MoveTemp(Goods);B.StorageCapacityBonusMilliUnits=Storage;B.AdditionalOrderSlots=Slots;B.StationTransferCapBonusMilliUnits=Handling;
				Value->Specializations.Add(MoveTemp(B));
			};
			Branch(TEXT("Warehouse"),TEXT("Warehouse"),TEXT("PresenceCapability.WarehouseSpecialization"),90000,{Cost(TEXT("Good.Planks"),8000),Cost(TEXT("Good.Tools"),1000)},75000,0,0);
			Branch(TEXT("Market"),TEXT("Market"),TEXT("PresenceCapability.MarketSpecialization"),75000,{Cost(TEXT("Good.Planks"),6000),Cost(TEXT("Good.Tools"),2000)},0,4,0);
			Branch(TEXT("Harbor"),TEXT("Harbor"),TEXT("PresenceCapability.HarborSpecialization"),110000,{Cost(TEXT("Good.Planks"),10000),Cost(TEXT("Good.Tools"),3000)},0,0,50000);
			FHansaCityPrivilegeDefinition Privilege;Privilege.PrivilegeId=TEXT("AdditionalCommercialPlot");Privilege.DisplayName=FText::FromString(TEXT("Additional Commercial Plot"));Privilege.CostPfennig=120000;Privilege.CostGoods={Cost(TEXT("Good.Planks"),8000)};Privilege.bReversible=true;Privilege.LeaseBoundsMin=FIntPoint(20,8);Privilege.LeaseBoundsMax=FIntPoint(27,19);Privilege.PermittedBuildingCategories={TEXT("Storage"),TEXT("Commercial")};Value->Privileges.Add(Privilege);
			FHansaCityProjectDefinition Project;Project.ProjectId=TEXT("PublicGranary");Project.DisplayName=FText::FromString(TEXT("Public Granary"));Project.CostPfennig=90000;Project.CostGoods={Cost(TEXT("Good.Planks"),6000)};Project.ConstructionTicks=3;Project.SharedReserveGoodId=TEXT("Good.Bread");Project.SharedReserveBonusMilliUnits=20000;Value->CityProjects.Add(Project);
			Value->RefreshContentHash();
		};
		Policy(TEXT("Lubeck"),TEXT("")); Policy(TEXT("Hamburg"),TEXT("")); Policy(TEXT("Luneburg"),TEXT(""));
		Policy(TEXT("Rostock"),TEXT("PresenceStage.VisitingContact"));
		auto* RostockPolicy = CastChecked<UHansaCityTradePolicyDefinition>(Definitions.Last().Get());
		FHansaTradeStationSiteDefinition Site;
		Site.SiteId=TEXT("TradeStationSite.Rostock.Harbor.01"); Site.PlotCategory=TEXT("Commercial");
		Site.StorageCapacityMilliUnits=50000; Site.ConstructionTicks=3; Site.UpkeepPfennigPerTick=25; Site.CancellationRefundBasisPoints=5000; Site.LeaseBoundsMin=FIntPoint(8,8); Site.LeaseBoundsMax=FIntPoint(19,19); Site.PermittedBuildingCategories={TEXT("Storage"),TEXT("Commercial"),TEXT("Production")};
		Site.PresentationClass=FSoftClassPath(TEXT("/Game/Mesh/hansa-harbor/BP_Harbor_Review.BP_Harbor_Review_C"));
		RostockPolicy->TradeStationSites.Add(Site); RostockPolicy->RefreshContentHash();
	}
}
