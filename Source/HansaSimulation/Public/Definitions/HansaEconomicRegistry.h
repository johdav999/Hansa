#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"

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
		bool bRequiresShoreline = false;
		uint64 ContentHash = 0;
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
		TArray<FHansaCompiledMarketGoodProfile> Goods;
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
			TArray<FHansaCompiledCityMarketProfileDefinition> InCityMarkets = {});

		[[nodiscard]] const TArray<FHansaCompiledGoodDefinition>& GetGoods() const { return Goods; }
		[[nodiscard]] const TArray<FHansaCompiledRecipeDefinition>& GetRecipes() const { return Recipes; }
		[[nodiscard]] const TArray<FHansaCompiledBuildingDefinition>& GetBuildings() const { return Buildings; }
		[[nodiscard]] const TArray<FHansaCompiledNeedDefinition>& GetNeeds() const { return Needs; }
		[[nodiscard]] const TArray<FHansaCompiledPopulationTierDefinition>& GetPopulationTiers() const { return PopulationTiers; }
		[[nodiscard]] const TArray<FHansaCompiledCityMarketProfileDefinition>& GetCityMarkets() const { return CityMarkets; }
		[[nodiscard]] uint64 GetRegistryHash() const { return RegistryHash; }

		[[nodiscard]] const FHansaCompiledGoodDefinition* FindGood(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledRecipeDefinition* FindRecipe(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledBuildingDefinition* FindBuilding(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledNeedDefinition* FindNeed(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledPopulationTierDefinition* FindPopulationTier(const FString& StableId) const;
		[[nodiscard]] const FHansaCompiledCityMarketProfileDefinition* FindCityMarket(const FString& StableId) const;

	private:
		TArray<FHansaCompiledGoodDefinition> Goods;
		TArray<FHansaCompiledRecipeDefinition> Recipes;
		TArray<FHansaCompiledBuildingDefinition> Buildings;
		TArray<FHansaCompiledNeedDefinition> Needs;
		TArray<FHansaCompiledPopulationTierDefinition> PopulationTiers;
		TArray<FHansaCompiledCityMarketProfileDefinition> CityMarkets;
		TMap<FString, int32> GoodIndexes;
		TMap<FString, int32> RecipeIndexes;
		TMap<FString, int32> BuildingIndexes;
		TMap<FString, int32> NeedIndexes;
		TMap<FString, int32> PopulationTierIndexes;
		TMap<FString, int32> CityMarketIndexes;
		uint64 RegistryHash = 0;
	};
}
