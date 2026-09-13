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
	};

	struct HANSASIMULATION_API FHansaCompiledBuildingDefinition final
	{
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
		bool bRequiresShoreline = false;
		uint64 ContentHash = 0;
		FString DisplayName;
		bool bShowInConstructionMenu = false;
		FString ConstructionMenuCategory;
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

	struct HANSASIMULATION_API FHansaCompiledNeedDefinition final
	{
		FString StableId;
		EHansaCompiledNeedKind Kind = EHansaCompiledNeedKind::Good;
		FString GoodId;
		uint64 ContentHash = 0;
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

	struct HANSASIMULATION_API FHansaCompiledCityMarketProfileDefinition final
	{
		FString StableId;
		int32 UpdateCadenceTicks = 0;
		int32 PriceHistoryCapacity = 0;
		int32 TargetSmoothingBasisPoints = 0;
		int32 MaximumMovementBasisPointsPerUpdate = 0;
		int32 StaleAfterTicks = 0;
		bool bMarketOnly = false;
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
		uint64 RegistryHash = 0;
	};
}
