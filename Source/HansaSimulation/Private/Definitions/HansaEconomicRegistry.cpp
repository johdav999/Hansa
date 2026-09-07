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
		TArray<FHansaCompiledCityMarketProfileDefinition> InCityMarkets,
		TArray<FHansaCompiledVehicleDefinition> InVehicles,
		TArray<FHansaCompiledRouteDefinition> InRoutes,
		TArray<FHansaCompiledTechnologyDefinition> InTechnologies,
		TArray<FHansaCompiledMerchantAITuning> InMerchantAITunings,
		TArray<FHansaCompiledScenarioObjective> InScenarioObjectives,
		TArray<FHansaCompiledVictoryDefinition> InVictories,
		TArray<FHansaCompiledScenarioDefinition> InScenarios)
		: Goods(MoveTemp(InGoods))
		, Recipes(MoveTemp(InRecipes))
		, Buildings(MoveTemp(InBuildings))
		, Needs(MoveTemp(InNeeds))
		, PopulationTiers(MoveTemp(InPopulationTiers))
		, CityMarkets(MoveTemp(InCityMarkets))
		, Vehicles(MoveTemp(InVehicles))
		, Routes(MoveTemp(InRoutes))
		, Technologies(MoveTemp(InTechnologies))
		, MerchantAITunings(MoveTemp(InMerchantAITunings))
		, ScenarioObjectives(MoveTemp(InScenarioObjectives))
		, Victories(MoveTemp(InVictories))
		, Scenarios(MoveTemp(InScenarios))
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
		for (int32 Index = 0; Index < Vehicles.Num(); ++Index)
		{
			VehicleIndexes.Add(Vehicles[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < Routes.Num(); ++Index)
		{
			RouteIndexes.Add(Routes[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < Technologies.Num(); ++Index)
		{
			TechnologyIndexes.Add(Technologies[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < MerchantAITunings.Num(); ++Index)
		{
			MerchantAITuningIndexes.Add(MerchantAITunings[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < ScenarioObjectives.Num(); ++Index)
		{
			ScenarioObjectiveIndexes.Add(ScenarioObjectives[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < Victories.Num(); ++Index)
		{
			VictoryIndexes.Add(Victories[Index].StableId, Index);
		}
		for (int32 Index = 0; Index < Scenarios.Num(); ++Index)
		{
			ScenarioIndexes.Add(Scenarios[Index].StableId, Index);
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

	const FHansaCompiledVehicleDefinition* FHansaEconomicRegistry::FindVehicle(const FString& StableId) const
	{
		const int32* Index = VehicleIndexes.Find(StableId);
		return Index != nullptr ? &Vehicles[*Index] : nullptr;
	}

	const FHansaCompiledRouteDefinition* FHansaEconomicRegistry::FindRoute(const FString& StableId) const
	{
		const int32* Index = RouteIndexes.Find(StableId);
		return Index != nullptr ? &Routes[*Index] : nullptr;
	}

	const FHansaCompiledTechnologyDefinition* FHansaEconomicRegistry::FindTechnology(const FString& StableId) const
	{
		const int32* Index = TechnologyIndexes.Find(StableId);
		return Index != nullptr ? &Technologies[*Index] : nullptr;
	}

	const FHansaCompiledMerchantAITuning* FHansaEconomicRegistry::FindMerchantAITuning(const FString& StableId) const
	{
		const int32* Index = MerchantAITuningIndexes.Find(StableId);
		return Index != nullptr ? &MerchantAITunings[*Index] : nullptr;
	}

	const FHansaCompiledScenarioObjective* FHansaEconomicRegistry::FindScenarioObjective(const FString& StableId) const
	{
		const int32* Index = ScenarioObjectiveIndexes.Find(StableId);
		return Index != nullptr ? &ScenarioObjectives[*Index] : nullptr;
	}

	const FHansaCompiledVictoryDefinition* FHansaEconomicRegistry::FindVictory(const FString& StableId) const
	{
		const int32* Index = VictoryIndexes.Find(StableId);
		return Index != nullptr ? &Victories[*Index] : nullptr;
	}

	const FHansaCompiledScenarioDefinition* FHansaEconomicRegistry::FindScenario(const FString& StableId) const
	{
		const int32* Index = ScenarioIndexes.Find(StableId);
		return Index != nullptr ? &Scenarios[*Index] : nullptr;
	}
}
