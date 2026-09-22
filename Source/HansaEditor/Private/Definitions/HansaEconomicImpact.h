#pragma once
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Definitions/HansaMarketDefinitions.h"
#include "Definitions/HansaPresenceDefinitions.h"

namespace Hansa::Editor::EconomicDefinitions
{
// Editor-only direct reverse references, sorted independently of asset discovery order.
inline TArray<FString> DescribeEconomicImpact(const FString& StableId, TConstArrayView<const UHansaDefinitionBase*> Definitions)
{
    TArray<FString> Result;
    for (const auto* Definition : Definitions)
    {
        if (!Definition) continue;
        if (const auto* Policy = Cast<UHansaCityTradePolicyDefinition>(Definition); Policy && Policy->StableDefinitionId == StableId)
            {
            Result.AddUnique(Policy->CityId + TEXT(".StationOrders (cap, budget, slot limits; active saves require migration review)"));
            Result.AddUnique(Policy->CityId + TEXT(".StationRoutes (RouteAccess/LocalStorage denial blocks transfers; physical cargo is preserved)"));
            }
        if (Definition->StableDefinitionId == StableId) continue;
        const auto Add = [&](const TCHAR* Field) { Result.AddUnique(Definition->StableDefinitionId + TEXT(".") + Field); };
        if (const auto* Need = Cast<UHansaNeedDefinition>(Definition))
        {
            if (Need->GoodId == StableId) Add(TEXT("GoodId"));
            if (Need->Alternatives.ContainsByPredicate([&](const auto& V){return V.GoodId == StableId;})) Add(TEXT("Alternatives"));
        }
        if (const auto* Recipe = Cast<UHansaRecipeDefinition>(Definition))
        {
            if (Recipe->InternalCatchRecipeId == StableId) Add(TEXT("InternalCatchRecipeId"));
            if (Recipe->Inputs.ContainsByPredicate([&](const auto& V){return V.GoodId == StableId;})) Add(TEXT("Inputs"));
            if (Recipe->Outputs.ContainsByPredicate([&](const auto& V){return V.GoodId == StableId;})) Add(TEXT("Outputs"));
        }
        if (const auto* Building = Cast<UHansaBuildingDefinition>(Definition))
        {
            if (Building->ConstructionChainOutputGoodId == StableId) Add(TEXT("ConstructionChainOutputGoodId"));
            if (Building->UpgradeTargetBuildingId == StableId) Add(TEXT("UpgradeTargetBuildingId"));
            if (Building->RecipeIds.Contains(StableId)) Add(TEXT("RecipeIds"));
            if (Building->ConstructionCosts.ContainsByPredicate([&](const auto& V){return V.GoodId == StableId;})) Add(TEXT("ConstructionCosts"));
        }
        if (const auto* Tier = Cast<UHansaPopulationTierDefinition>(Definition))
            if (Tier->Needs.ContainsByPredicate([&](const auto& V){return V.NeedId == StableId;})) Add(TEXT("Needs"));
        if (const auto* City = Cast<UHansaCityMarketProfileDefinition>(Definition))
            if (City->Goods.ContainsByPredicate([&](const auto& V){return V.GoodId == StableId;})) Add(TEXT("Goods"));
        if (const auto* Stage = Cast<UHansaForeignPresenceStageDefinition>(Definition))
        {
            if (Stage->PrerequisiteStageIds.Contains(StableId)) Add(TEXT("PrerequisiteStageIds"));
            if (Stage->GrantedCapabilityIds.Contains(StableId)) Add(TEXT("GrantedCapabilityIds"));
            if (Stage->UpgradeGoods.ContainsByPredicate([&](const auto& V){return V.GoodId == StableId;})) Add(TEXT("UpgradeGoods"));
        }
        if (const auto* Policy = Cast<UHansaCityTradePolicyDefinition>(Definition))
        {
            if (Policy->CityId == StableId) Add(TEXT("CityId"));
            if (Policy->InitialStageId == StableId) Add(TEXT("InitialStageId"));
            if (Policy->AllowedStageIds.Contains(StableId)) Add(TEXT("AllowedStageIds"));
            if (Policy->DeniedCapabilityIds.Contains(StableId)) Add(TEXT("DeniedCapabilityIds"));
            if (Policy->Specializations.ContainsByPredicate([&](const auto& V){ return V.GrantedCapabilityIds.Contains(StableId); })) Add(TEXT("Specializations.GrantedCapabilityIds"));
            if (Policy->Specializations.ContainsByPredicate([&](const auto& V){ return V.RequiredStageId == StableId; })) Add(TEXT("Specializations.RequiredStageId"));
            if (Policy->Specializations.ContainsByPredicate([&](const auto& V){ return V.InvestmentGoods.ContainsByPredicate([&](const auto& Cost){ return Cost.GoodId == StableId; }); })) Add(TEXT("Specializations.InvestmentGoods"));
        }
    }
    Result.Sort();
    return Result;
}
}
