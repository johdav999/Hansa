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
		FHansaEconomicRegistry EconomicRegistry,
		TMap<uint64, FString> InCompatibleDefinitionMigrations)
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
		for (const TPair<uint64, FString>& Migration : InCompatibleDefinitionMigrations)
		{
			if (Migration.Key == 0 || Migration.Key == DefinitionHash || Migration.Value.TrimStartAndEnd().IsEmpty())
			{
				return THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidFormat);
			}
		}
		FHansaSimulationDefinitionContext Result(ScenarioId, DefinitionHash);
		Result.EconomicRegistry = MoveTemp(EconomicRegistry);
		Result.CompatibleDefinitionMigrations = MoveTemp(InCompatibleDefinitionMigrations);
		return THansaValueResult<FHansaSimulationDefinitionContext>::Success(MoveTemp(Result));
	}

	THansaValueResult<FHansaSimulationDefinitionContext> FHansaSimulationDefinitionContext::TryCreate(
		const FHansaScenarioId ScenarioId,
		const uint64 DefinitionHash,
		FHansaEconomicRegistry EconomicRegistry,
		FHansaPlacementTopology InPlacementTopology,
		TMap<uint64, FString> InCompatibleDefinitionMigrations)
	{
		THansaValueResult<FHansaSimulationDefinitionContext> Created = TryCreate(
			ScenarioId,
			DefinitionHash,
			MoveTemp(EconomicRegistry),
			MoveTemp(InCompatibleDefinitionMigrations));
		if (!Created || !InPlacementTopology.IsValid())
		{
			return Created
				? THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidFormat)
				: Created;
		}
		Created.Value.PlacementTopology = MakeShared<FHansaPlacementTopology>(MoveTemp(InPlacementTopology));
		return Created;
	}

	THansaValueResult<FHansaSimulationDefinitionContext> FHansaSimulationDefinitionContext::TryCreate(
		const FHansaScenarioId ScenarioId,
		const uint64 DefinitionHash,
		FHansaPlacementTopology InPlacementTopology)
	{
		THansaValueResult<FHansaSimulationDefinitionContext> Created = TryCreate(ScenarioId, DefinitionHash);
		if (!Created || !InPlacementTopology.IsValid())
		{
			return Created
				? THansaValueResult<FHansaSimulationDefinitionContext>::Failure(EHansaValueError::InvalidFormat)
				: Created;
		}
		Created.Value.PlacementTopology = MakeShared<FHansaPlacementTopology>(MoveTemp(InPlacementTopology));
		return Created;
	}

	bool FHansaSimulationDefinitionContext::IsValid() const
	{
		return ScenarioId.IsValid() && DefinitionHash != 0 &&
			(!EconomicRegistry.IsSet() || EconomicRegistry->GetRegistryHash() == DefinitionHash) &&
			(!PlacementTopology.IsValid() || PlacementTopology->IsValid());
	}

	const FHansaEconomicRegistry* FHansaSimulationDefinitionContext::GetEconomicRegistry() const
	{
		return EconomicRegistry.IsSet() ? &EconomicRegistry.GetValue() : nullptr;
	}

	TOptional<FString> FHansaSimulationDefinitionContext::FindCompatibleDefinitionMigration(
		const uint64 SavedDefinitionHash,
		const uint64 SavedRegistryHash) const
	{
		if (SavedDefinitionHash == 0 || SavedDefinitionHash != SavedRegistryHash)
		{
			return {};
		}
		if (const FString* Migration = CompatibleDefinitionMigrations.Find(SavedDefinitionHash))
		{
			return *Migration;
		}
		return {};
	}
}
