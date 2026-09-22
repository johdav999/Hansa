#include "Definitions/HansaEconomicRegistry.h"

namespace Hansa::Simulation
{
	void FHansaEconomicRegistry::SetPresenceDefinitions(
		TArray<FHansaCompiledPresenceCapabilityDefinition> InCapabilities,
		TArray<FHansaCompiledForeignPresenceStageDefinition> InStages,
		TArray<FHansaCompiledCityTradePolicyDefinition> InPolicies)
	{
		PresenceCapabilities = MoveTemp(InCapabilities);
		PresenceStages = MoveTemp(InStages);
		CityTradePolicies = MoveTemp(InPolicies);
		PresenceCapabilityIndexes.Reset();
		PresenceStageIndexes.Reset();
		CityTradePolicyIndexes.Reset();
		CityTradePolicyByCityIndexes.Reset();
		for (int32 Index = 0; Index < PresenceCapabilities.Num(); ++Index)
			PresenceCapabilityIndexes.Add(PresenceCapabilities[Index].StableId, Index);
		for (int32 Index = 0; Index < PresenceStages.Num(); ++Index)
			PresenceStageIndexes.Add(PresenceStages[Index].StableId, Index);
		for (int32 Index = 0; Index < CityTradePolicies.Num(); ++Index)
		{
			CityTradePolicyIndexes.Add(CityTradePolicies[Index].StableId, Index);
			CityTradePolicyByCityIndexes.Add(CityTradePolicies[Index].CityId, Index);
		}
	}

	const FHansaCompiledPresenceCapabilityDefinition* FHansaEconomicRegistry::FindPresenceCapability(const FString& StableId) const
	{
		const int32* Index = PresenceCapabilityIndexes.Find(StableId);
		return Index ? &PresenceCapabilities[*Index] : nullptr;
	}

	const FHansaCompiledForeignPresenceStageDefinition* FHansaEconomicRegistry::FindPresenceStage(const FString& StableId) const
	{
		const int32* Index = PresenceStageIndexes.Find(StableId);
		return Index ? &PresenceStages[*Index] : nullptr;
	}

	const FHansaCompiledCityTradePolicyDefinition* FHansaEconomicRegistry::FindCityTradePolicy(const FString& StableId) const
	{
		const int32* Index = CityTradePolicyIndexes.Find(StableId);
		return Index ? &CityTradePolicies[*Index] : nullptr;
	}

	const FHansaCompiledCityTradePolicyDefinition* FHansaEconomicRegistry::FindCityTradePolicyForCity(const FString& CityId) const
	{
		const int32* Index = CityTradePolicyByCityIndexes.Find(CityId);
		return Index ? &CityTradePolicies[*Index] : nullptr;
	}
	bool FHansaEconomicRegistry::IsValidPresenceTransition(const FString& CityId, const FString& CurrentStageId, const FString& NextStageId) const
	{
		const auto* Policy = FindCityTradePolicyForCity(CityId);
		const auto* Current = FindPresenceStage(CurrentStageId);
		const auto* Next = FindPresenceStage(NextStageId);
		return Policy && Current && Next && Next->Ordinal > Current->Ordinal &&
			Policy->AllowedStageIds.Contains(NextStageId) && Next->PrerequisiteStageIds.Contains(CurrentStageId);
	}

}
