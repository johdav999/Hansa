#include "Definitions/HansaSimulationDefinitionContext.h"

namespace Hansa::Simulation
{
	THansaValueResult<FHansaSimulationDefinitionContext> FHansaSimulationDefinitionContext::TryCreate(
		const FHansaScenarioId ScenarioId,
		const uint64 DefinitionHash)
	{
		if (!ScenarioId.IsValid())
		{
			return THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidFormat);
		}
		if (DefinitionHash == 0)
		{
			return THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidZero);
		}
		return THansaValueResult<FHansaSimulationDefinitionContext>::Success(
			FHansaSimulationDefinitionContext(ScenarioId, DefinitionHash));
	}

	THansaValueResult<FHansaSimulationDefinitionContext> FHansaSimulationDefinitionContext::TryCreate(
		const FHansaScenarioId ScenarioId,
		const uint64 DefinitionHash,
		FHansaEconomicRegistry EconomicRegistry)
	{
		if (!ScenarioId.IsValid())
		{
			return THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidFormat);
		}
		if (DefinitionHash == 0 || EconomicRegistry.GetRegistryHash() == 0)
		{
			return THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidZero);
		}
		if (DefinitionHash != EconomicRegistry.GetRegistryHash())
		{
			return THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidFormat);
		}
		FHansaSimulationDefinitionContext Result(ScenarioId, DefinitionHash);
		Result.EconomicRegistry = MoveTemp(EconomicRegistry);
		return THansaValueResult<FHansaSimulationDefinitionContext>::Success(MoveTemp(Result));
	}

	bool FHansaSimulationDefinitionContext::IsValid() const
	{
		return ScenarioId.IsValid() && DefinitionHash != 0 &&
			(!EconomicRegistry.IsSet() || EconomicRegistry->GetRegistryHash() == DefinitionHash);
	}

	const FHansaEconomicRegistry* FHansaSimulationDefinitionContext::GetEconomicRegistry() const
	{
		return EconomicRegistry.IsSet() ? &EconomicRegistry.GetValue() : nullptr;
	}
}
