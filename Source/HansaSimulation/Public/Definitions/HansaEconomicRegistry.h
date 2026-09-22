#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Research/HansaResearch.h"
#include "Scenario/HansaScenario.h"
#include "Trade/HansaTrade.h"

namespace Hansa::Simulation
{
	struct HANSASIMULATION_API FHansaCompiledGoodAmount final
	{
		FString GoodId;
		int64 QuantityMilliUnits = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledGoodDefinition final
	{
		FString StableId;
		FString Unit;
		int64 BaseValueMilliMarks = 0;
		int32 PriceElasticityBasisPoints = 0;
		int32 SpoilageBasisPointsPerDay = 0;
		uint64 ContentHash = 0;
		FString DisplayName;
        bool bSpoilageEnabled = false;
	};

	struct HANSASIMULATION_API FHansaCompiledRecipeDefinition final
	{
		FString StableId;
		TArray<FHansaCompiledGoodAmount> Inputs;
		TArray<FHansaCompiledGoodAmount> Outputs;
		int32 CycleTicks = 0;
		int32 LaborerWorkforce = 0;
		int32 ArtisanWorkforce = 0;
		bool bDeclaredSource = false;
		bool bDeclaredSink = false;
		uint64 ContentHash = 0;
        FString InternalCatchRecipeId;
	};

	struct HANSASIMULATION_API FHansaCompiledBuildingDefinition final
	{
        FString ResidentialCompoundId;
        int32 CompoundStage = 0;
        FString CompoundDistrictId;
        uint8 CompoundRoadFrontMask = 0;
        uint8 CompoundLayoutContextMask = 0; // Straight, left corner, right corner, map edge.
		FString StableId;
		int32 SchemaVersion = 0;
		TArray<FHansaCompiledGoodAmount> ConstructionCosts;
		int64 ConstructionCostPfennig = 0;
		int32 CancellationRefundBasisPoints = 0;
		TArray<FString> RecipeIds;
		FString UpgradeTargetBuildingId;
		int32 FootprintWidthCells = 0;
		int32 FootprintHeightCells = 0;
		int32 BuildTicks = 0;
		int32 StorageCapacityMilliUnits = 0;
		int32 ResidenceCapacity = 0;
		FString ResidentPopulationTierId;
		int32 LaborerWorkforce = 0;
		int32 ArtisanWorkforce = 0;
		bool bRequiresRoad = false;
		bool bProvidesMarketAccess = false;
		int32 MaximumMarketRoadDistanceCells = 40;
		bool bRequiresShoreline = false;
		uint64 ContentHash = 0;
		FString DisplayName;
		bool bShowInConstructionMenu = false;
		FString ConstructionMenuCategory;
        FString ConstructionTier;
		int32 ConstructionMenuOrder = 0;
		FString ConstructionChainOutputGoodId;
		int32 ConstructionChainStage = 0;
		int32 ConstructionChainStageCount = 0;
		FString RequiredConstructionTechnologyId;
		bool bUpgradeOnly = false;
		FString ConstructionPresentationPurpose;
	};

	enum class EHansaCompiledNeedKind : uint8
	{
		Good = 0,
		Service
	};

	struct HANSASIMULATION_API FHansaCompiledNeedAlternative final
 { FString GoodId; int32 FulfillmentBasisPoints = 10000; };

	struct HANSASIMULATION_API FHansaCompiledNeedDefinition final
	{
		FString StableId;
		EHansaCompiledNeedKind Kind = EHansaCompiledNeedKind::Good;
		FString GoodId;
		uint64 ContentHash = 0;
		bool bSeasonal = false;
		int32 SeasonDays = 90;
		int32 FixedSeason = -1;
		int32 DefaultReserveDays = 3;
		TArray<int32> SeasonMultipliers = { 0, 4000, 10000, 4000 };
		TArray<FHansaCompiledNeedAlternative> Alternatives;
	};

	struct HANSASIMULATION_API FHansaCompiledPopulationTierNeed final
	{
		FString NeedId;
		int32 ConsumptionMilliUnitsPerResidentPerTick = 0;
		int32 ImportanceBasisPoints = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledPopulationTierDefinition final
	{
		FString StableId;
		FString PreviousTierId;
		TArray<FHansaCompiledPopulationTierNeed> Needs;
		int32 WorkforcePerResidentBasisPoints = 0;
		int32 GrowthSatisfactionBasisPoints = 0;
		int32 DeclineSatisfactionBasisPoints = 0;
		int32 EvaluationTicks = 0;
		int32 GrowthResidentsPerEvaluation = 0;
		int32 DeclineResidentsPerEvaluation = 0;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledMarketGoodProfile final
	{
		FString GoodId;
		int64 DesiredReserveMilliUnits = 0;
		int64 ConfirmedIncomingSupplyMilliUnits = 0;
		int64 InitialStockMilliUnits = 0;
		int64 BackgroundProductionMilliUnitsPerUpdate = 0;
		int64 BackgroundCitizenDemandMilliUnitsPerUpdate = 0;
		int64 BackgroundIndustrialDemandMilliUnitsPerUpdate = 0;
		int32 SeasonModifierBasisPoints = 0;
		int32 CityModifierBasisPoints = 0;
		int64 MinimumPriceMilliMarks = 0;
		int64 MaximumPriceMilliMarks = 0;
		int64 InitialPriceMilliMarks = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledProductionChainStage final
	{
		FString StageKey;
		FString RecipeId;
		TArray<FString> PrerequisiteStageKeys;
		uint8 Role = 0;
		FString IntendedConstructionTier;
	};

	struct HANSASIMULATION_API FHansaCompiledProductionChainDefinition final
	{
		FString StableId;
		TArray<FHansaCompiledProductionChainStage> Stages;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledRegionResourceEndowment final
	{
		FString GoodId;
		uint8 Endowment = 0;
		int64 SourceCapacityMilliUnitsPerUpdate = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledRegionPermittedStage final
	{
		FString ProductionChainId;
		TArray<FString> StageKeys;
	};

	struct HANSASIMULATION_API FHansaCompiledRegionEconomicProfileDefinition final
	{
		FString StableId;
		TArray<FString> MemberCityIds;
		TArray<FHansaCompiledRegionPermittedStage> PermittedStages;
		TArray<FHansaCompiledRegionResourceEndowment> ResourceEndowments;
		int64 ExchangeCapacityMilliUnitsPerUpdate = 0;
		int32 ExchangeDelayUpdates = 1;
		int32 TransportLossBasisPoints = 0;
		int64 TransportCostMilliMarksPerUnit = 0;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledCityIndustryBinding final
	{
		FString ProductionChainId;
		TArray<FString> EnabledStageKeys;
		int32 CyclesPerMarketUpdate = 1;
		int32 EfficiencyBasisPoints = 10000;
		int64 InputReserveMilliUnits = 0;
		int64 OutputReserveMilliUnits = 0;
		bool bEnabled = true;
		int32 SignatureRank = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledCityMarketProfileDefinition final
	{
		FString StableId;
		int32 UpdateCadenceTicks = 0;
		int32 PriceHistoryCapacity = 0;
		int32 TargetSmoothingBasisPoints = 0;
		int32 MaximumMovementBasisPointsPerUpdate = 0;
		int32 StaleAfterTicks = 0;
		bool bMarketOnly = false;
		uint8 PresentationClass = 0;
		int32 MapLongitudeMilliDegrees = 0;
		int32 MapLatitudeMilliDegrees = 0;
		FString RegionId;
		TArray<FHansaCompiledCityIndustryBinding> IndustryBindings;
		int32 ReportCadenceTicks = 0;
		int32 CurrentReportMaxAgeTicks = 0;
		int32 RecentReportMaxAgeTicks = 0;
		int32 StaleReportMaxAgeTicks = 0;
		int32 EstimatedReportMaxAgeTicks = 0;
		TArray<FHansaCompiledMarketGoodProfile> Goods;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledVehicleDefinition final
	{
		FString StableId;
		EHansaRouteMode Mode = EHansaRouteMode::Sea;
		int64 CargoCapacityMilliUnits = 0;
		int64 UpkeepPfennigPerTravelTick = 0;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledRouteConnection final
	{
		FString SourceCityId;
		FString DestinationCityId;
		int32 TravelTicks = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledRouteDefinition final
	{
		FString StableId;
		EHansaRouteMode Mode = EHansaRouteMode::Sea;
		int32 CargoRuleSchemaVersion = 1;
		TArray<FHansaCompiledRouteConnection> Connections;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledPresenceCapabilityDefinition final
	{
		FString StableId;
		FString DisplayName;
		FString Semantics;
		int32 SchemaVersion = 1;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledPresenceUpgradeGoodCost final
	{
		FString GoodId;
		int64 QuantityMilliUnits = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledForeignPresenceStageDefinition final
	{
		FString StableId;
		FString DisplayName;
		int32 Ordinal = 0;
		TArray<FString> PrerequisiteStageIds;
		TArray<FString> GrantedCapabilityIds;
		TArray<FString> PermittedPlotCategories;
		TArray<FString> PermittedBuildingCategories;
		int64 UpgradeCostPfennig = 0;
		TArray<FHansaCompiledPresenceUpgradeGoodCost> UpgradeGoods;
		int64 RequiredLawfulTradeVolumeMilliUnits = 0;
		int64 RequiredCompletedDeliveries = 0;
		int64 RequiredInvestedPfennig = 0;
		int64 RequiredTransactionValuePfennig = 0;
		int64 RequiredFulfilledShortageMilliUnits = 0;
		int64 RequiredReliableOperatingTicks = 0;
		int64 RequiredSolventOperatingTicks = 0;
		int32 UpgradeConstructionTicks = 3;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledCityTradePolicyDefinition final
	{
		struct FPrivilege final { FString PrivilegeId; FString DisplayName; FString RequiredStageId; int64 CostPfennig=0; TArray<FHansaCompiledPresenceUpgradeGoodCost> CostGoods; int64 DurationTicks=0; bool bReversible=false; FHansaGridCoordinate LeaseBoundsMin; FHansaGridCoordinate LeaseBoundsMax; TArray<FString> PermittedBuildingCategories; };
		struct FCityProject final { FString ProjectId; FString DisplayName; FString RequiredStageId; int64 CostPfennig=0; TArray<FHansaCompiledPresenceUpgradeGoodCost> CostGoods; int32 ConstructionTicks=3; FString SharedReserveGoodId; int64 SharedReserveBonusMilliUnits=0; };
		struct FSpecialization final
		{
			FString SpecializationId;
			FString DisplayName;
			FString RequiredStageId;
			FString ExclusiveGroupId;
			TArray<FString> GrantedCapabilityIds;
			int64 InvestmentCostPfennig = 0;
			TArray<FHansaCompiledPresenceUpgradeGoodCost> InvestmentGoods;
			bool bAllowRespec = true;
			int32 RespecRefundBasisPoints = 2500;
			int64 StorageCapacityBonusMilliUnits = 0;
			int32 AdditionalOrderSlots = 0;
			int64 StationTransferCapBonusMilliUnits = 0;
		};

		struct FStationSite final
		{
			FString SiteId;
			FString PlotCategory;
			int64 StorageCapacityMilliUnits = 0;
			int32 ConstructionTicks = 0;
			int64 UpkeepPfennigPerTick = 0;
			int32 CancellationRefundBasisPoints = 0;
			FString PresentationClassPath;
			FHansaGridCoordinate LeaseBoundsMin;
			FHansaGridCoordinate LeaseBoundsMax;
			TArray<FString> PermittedBuildingCategories;
		};
		FString StableId;
		FString CityId;
		FString InitialStageId;
		TArray<FString> AllowedStageIds;
		TArray<FString> DeniedCapabilityIds;
		TArray<FString> AllowedPlotCategories;
		TArray<FString> AllowedBuildingCategories;
		bool bPublicMarketAccess = true;
		bool bExceptionalGovernanceAllowed = false;
		TArray<FStationSite> TradeStationSites;
		TArray<FSpecialization> Specializations;
		TArray<FPrivilege> Privileges;
		TArray<FCityProject> CityProjects;
		TArray<FString> GovernanceScenarioIds;
		FString GovernanceCharterId;
		int64 MerchantOfficeStorageBonusMilliUnits = 0;
		int32 MerchantOfficeAdditionalOrderSlots = 0;
        int32 MaximumStationOrders = 8;
        int64 MaximumOrderCapMilliUnits = 50000;
        int64 MaximumOrderBudgetPfennig = 1000000;
		uint64 ContentHash = 0;
	};
	struct HANSASIMULATION_API FHansaCompiledMerchantAITradePlan final
	{
		FString StablePlanId;
		FString RouteDefinitionId;
		FString VehicleDefinitionId;
		FString SourceCityId;
		FString DestinationCityId;
		FString GoodId;
		int64 QuantityLimitMilliUnits = 0;
		int64 MinimumSourceReserveMilliUnits = 0;
		int64 UtilityBias = 0;
	};

	/** Provider-neutral, authored runtime tuning for one bounded merchant controller. */
	struct HANSASIMULATION_API FHansaCompiledMerchantAITuning final
	{
		FString StableId;
		int32 DecisionCadenceTicks = 1;
		int32 DecisionHistoryCapacity = 32;
		int64 MinimumDestinationDemandGapMilliUnits = 0;
		int64 MinimumGrossMarginMilliMarks = 0;
		int64 ShortageUtilityPerUnit = 1;
		int64 MarginUtilityPerMilliMark = 1;
		int64 ResearchUtility = 0;
		int64 ProductionUtility = 0;
		int64 ProtectedCashReservePfennig = 0;
		int32 ActionCooldownTicks = 0;
		int64 DirectTradeQuantityMilliUnits = 1;
		int64 StationOrderTargetMilliUnits = 1;
		int64 StationOrderCapMilliUnits = 1;
		int64 StationOrderBudgetPfennig = 1;
		int64 PresenceUtility = 0;
		int64 TargetCompletedTradeLegs = 1;
		TArray<FString> PreferredResearchTechnologyIds;
		TArray<FHansaCompiledMerchantAITradePlan> TradePlans;
		uint64 ContentHash = 0;
	};

	/** Immutable, stable-ID keyed economic content consumed by deterministic systems. */
	class HANSASIMULATION_API FHansaEconomicRegistry final
	{
	public:
		FHansaEconomicRegistry() = default;
		FHansaEconomicRegistry(
			TArray<FHansaCompiledGoodDefinition> InGoods,
			TArray<FHansaCompiledRecipeDefinition> InRecipes,
			TArray<FHansaCompiledBuildingDefinition> InBuildings,
			uint64 InRegistryHash,
			TArray<FHansaCompiledNeedDefinition> InNeeds = {},
			TArray<FHansaCompiledPopulationTierDefinition> InPopulationTiers = {},
			TArray<FHansaCompiledCityMarketProfileDefinition> InCityMarkets = {},
			TArray<FHansaCompiledVehicleDefinition> InVehicles = {},
			TArray<FHansaCompiledRouteDefinition> InRoutes = {},
			TArray<FHansaCompiledTechnologyDefinition> InTechnologies = {},
			TArray<FHansaCompiledMerchantAITuning> InMerchantAITunings = {},
			TArray<FHansaCompiledScenarioObjective> InScenarioObjectives = {},
			TArray<FHansaCompiledVictoryDefinition> InVictories = {},
			TArray<FHansaCompiledScenarioDefinition> InScenarios = {});
		void SetPresenceDefinitions(TArray<FHansaCompiledPresenceCapabilityDefinition> InCapabilities,
			TArray<FHansaCompiledForeignPresenceStageDefinition> InStages,
			TArray<FHansaCompiledCityTradePolicyDefinition> InPolicies);
		void SetRegionalEconomy(TArray<FHansaCompiledProductionChainDefinition> InProductionChains,
			TArray<FHansaCompiledRegionEconomicProfileDefinition> InRegions);

		[[nodiscard]] const TArray<FHansaCompiledGoodDefinition>& GetGoods() const { return Goods; }
		[[nodiscard]] const TArray<FHansaCompiledRecipeDefinition>& GetRecipes() const { return Recipes; }
		[[nodiscard]] const TArray<FHansaCompiledBuildingDefinition>& GetBuildings() const { return Buildings; }
		[[nodiscard]] const TArray<FHansaCompiledNeedDefinition>& GetNeeds() const { return Needs; }
		[[nodiscard]] const TArray<FHansaCompiledPopulationTierDefinition>& GetPopulationTiers() const { return PopulationTiers; }
		[[nodiscard]] const TArray<FHansaCompiledCityMarketProfileDefinition>& GetCityMarkets() const { return CityMarkets; }
		[[nodiscard]] const TArray<FHansaCompiledVehicleDefinition>& GetVehicles() const { return Vehicles; }
		[[nodiscard]] const TArray<FHansaCompiledRouteDefinition>& GetRoutes() const { return Routes; }
		[[nodiscard]] const TArray<FHansaCompiledTechnologyDefinition>& GetTechnologies() const { return Technologies; }
		[[nodiscard]] const TArray<FHansaCompiledMerchantAITuning>& GetMerchantAITunings() const { return MerchantAITunings; }
		[[nodiscard]] const TArray<FHansaCompiledScenarioObjective>& GetScenarioObjectives() const { return ScenarioObjectives; }
		[[nodiscard]] const TArray<FHansaCompiledVictoryDefinition>& GetVictories() const { return Victories; }
		[[nodiscard]] const TArray<FHansaCompiledScenarioDefinition>& GetScenarios() const { return Scenarios; }
		[[nodiscard]] const TArray<FHansaCompiledPresenceCapabilityDefinition>& GetPresenceCapabilities() const { return PresenceCapabilities; }
		[[nodiscard]] const TArray<FHansaCompiledForeignPresenceStageDefinition>& GetPresenceStages() const { return PresenceStages; }
		[[nodiscard]] const TArray<FHansaCompiledCityTradePolicyDefinition>& GetCityTradePolicies() const { return CityTradePolicies; }
		[[nodiscard]] const TArray<FHansaCompiledProductionChainDefinition>& GetProductionChains() const { return ProductionChains; }
		[[nodiscard]] const TArray<FHansaCompiledRegionEconomicProfileDefinition>& GetRegions() const { return Regions; }
		[[nodiscard]] uint64 GetRegistryHash() const { return RegistryHash; }

		[[nodiscard]] const FHansaCompiledGoodDefinition* FindGood(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledRecipeDefinition* FindRecipe(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledBuildingDefinition* FindBuilding(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledNeedDefinition* FindNeed(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledPopulationTierDefinition* FindPopulationTier(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledCityMarketProfileDefinition* FindCityMarket(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledVehicleDefinition* FindVehicle(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledRouteDefinition* FindRoute(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledTechnologyDefinition* FindTechnology(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledMerchantAITuning* FindMerchantAITuning(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledScenarioObjective* FindScenarioObjective(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledVictoryDefinition* FindVictory(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledScenarioDefinition* FindScenario(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledPresenceCapabilityDefinition* FindPresenceCapability(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledForeignPresenceStageDefinition* FindPresenceStage(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledCityTradePolicyDefinition* FindCityTradePolicy(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledCityTradePolicyDefinition* FindCityTradePolicyForCity(const FString& CityId) const;
		[[nodiscard]] bool IsValidPresenceTransition(const FString& CityId, const FString& CurrentStageId, const FString& NextStageId) const;
		[[nodiscard]] const FHansaCompiledProductionChainDefinition* FindProductionChain(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledRegionEconomicProfileDefinition* FindRegion(const FString& StableId) const;

	private:
		TArray<FHansaCompiledGoodDefinition> Goods;
		TArray<FHansaCompiledRecipeDefinition> Recipes;
		TArray<FHansaCompiledBuildingDefinition> Buildings;
		TArray<FHansaCompiledNeedDefinition> Needs;
		TArray<FHansaCompiledPopulationTierDefinition> PopulationTiers;
		TArray<FHansaCompiledCityMarketProfileDefinition> CityMarkets;
		TArray<FHansaCompiledVehicleDefinition> Vehicles;
		TArray<FHansaCompiledRouteDefinition> Routes;
		TArray<FHansaCompiledTechnologyDefinition> Technologies;
		TArray<FHansaCompiledMerchantAITuning> MerchantAITunings;
		TArray<FHansaCompiledScenarioObjective> ScenarioObjectives;
		TArray<FHansaCompiledVictoryDefinition> Victories;
		TArray<FHansaCompiledScenarioDefinition> Scenarios;
		TArray<FHansaCompiledPresenceCapabilityDefinition> PresenceCapabilities;
		TArray<FHansaCompiledForeignPresenceStageDefinition> PresenceStages;
		TArray<FHansaCompiledCityTradePolicyDefinition> CityTradePolicies;
		TArray<FHansaCompiledProductionChainDefinition> ProductionChains;
		TArray<FHansaCompiledRegionEconomicProfileDefinition> Regions;
		TMap<FString, int32> GoodIndexes;
		TMap<FString, int32> RecipeIndexes;
		TMap<FString, int32> BuildingIndexes;
		TMap<FString, int32> NeedIndexes;
		TMap<FString, int32> PopulationTierIndexes;
		TMap<FString, int32> CityMarketIndexes;
		TMap<FString, int32> VehicleIndexes;
		TMap<FString, int32> RouteIndexes;
		TMap<FString, int32> TechnologyIndexes;
		TMap<FString, int32> MerchantAITuningIndexes;
		TMap<FString, int32> ScenarioObjectiveIndexes;
		TMap<FString, int32> VictoryIndexes;
		TMap<FString, int32> ScenarioIndexes;
		TMap<FString, int32> PresenceCapabilityIndexes;
		TMap<FString, int32> PresenceStageIndexes;
		TMap<FString, int32> CityTradePolicyIndexes;
		TMap<FString, int32> CityTradePolicyByCityIndexes;
		TMap<FString, int32> ProductionChainIndexes;
		TMap<FString, int32> RegionIndexes;
		uint64 RegistryHash = 0;
	};
}
