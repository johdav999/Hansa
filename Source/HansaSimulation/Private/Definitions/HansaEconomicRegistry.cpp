#include "Definitions/HansaEconomicRegistry.h"

namespace Hansa::Simulation
{
	FHansaEconomicRegistry::FHansaEconomicRegistry(
		TArray<FHansaCompiledGoodDefinition> InGoods,
		TArray<FHansaCompiledRecipeDefinition> InRecipes,
		TArray<FHansaCompiledBuildingDefinition> InBuildings,
		const uint64 InRegistryHash,
		TArray<FHansaCompiledNeedDefinition> InNeeds,
		TArray<FHansaCompiledPopulationTierDefinition> InPopulationTiers,
		TArray<FHansaCompiledCityMarketProfileDefinition> InCityMarkets)
		: Goods(MoveTemp(InGoods))
		, Recipes(MoveTemp(InRecipes))
		, Buildings(MoveTemp(InBuildings))
		, Needs(MoveTemp(InNeeds))
		, PopulationTiers(MoveTemp(InPopulationTiers))
		, CityMarkets(MoveTemp(InCityMarkets))
		, RegistryHash(InRegistryHash)
	{
		for (int32 Index = 0; Index < Goods.Num(); ++Index)
		{
			GoodIndexes.Add(Goods[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < Recipes.Num(); ++Index)
		{
			RecipeIndexes.Add(Recipes[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < Buildings.Num(); ++Index)
		{
			BuildingIndexes.Add(Buildings[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < Needs.Num(); ++Index)
		{
			NeedIndexes.Add(Needs[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < PopulationTiers.Num(); ++Index)
		{
			PopulationTierIndexes.Add(PopulationTiers[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < CityMarkets.Num(); ++Index)
		{
			CityMarketIndexes.Add(CityMarkets[Index].StableId, Index);
		}
	}

	const FHansaCompiledGoodDefinition* FHansaEconomicRegistry::FindGood(const FString& StableId) const
	{
		const int32* Index = GoodIndexes.Find(StableId);
		return Index != nullptr ? &Goods[*Index] : nullptr;
	}

	const FHansaCompiledRecipeDefinition* FHansaEconomicRegistry::FindRecipe(const FString& StableId) const
	{
		const int32* Index = RecipeIndexes.Find(StableId);
		return Index != nullptr ? &Recipes[*Index] : nullptr;
	}

	const FHansaCompiledBuildingDefinition* FHansaEconomicRegistry::FindBuilding(const FString& StableId) const
	{
		const int32* Index = BuildingIndexes.Find(StableId);
		return Index != nullptr ? &Buildings[*Index] : nullptr;
	}

	const FHansaCompiledNeedDefinition* FHansaEconomicRegistry::FindNeed(const FString& StableId) const
	{
		const int32* Index = NeedIndexes.Find(StableId);
		return Index != nullptr ? &Needs[*Index] : nullptr;
	}

	const FHansaCompiledPopulationTierDefinition* FHansaEconomicRegistry::FindPopulationTier(const FString& StableId) const
	{
		const int32* Index = PopulationTierIndexes.Find(StableId);
		return Index != nullptr ? &PopulationTiers[*Index] : nullptr;
	}

	const FHansaCompiledCityMarketProfileDefinition* FHansaEconomicRegistry::FindCityMarket(const FString& StableId) const
	{
		const int32* Index = CityMarketIndexes.Find(StableId);
		return Index != nullptr ? &CityMarkets[*Index] : nullptr;
	}
}
