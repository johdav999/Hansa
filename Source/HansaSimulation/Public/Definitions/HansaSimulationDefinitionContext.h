#pragma once

#include "Definitions/HansaEconomicRegistry.h"
#include "Misc/Optional.h"
#include "Model/HansaIds.h"
#include "Model/HansaValueResult.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Simulation
{
	/**
	 * Immutable identity for the compiled definitions consumed by one simulation.
	 * The real compiled definition registry will extend this boundary without becoming mutable campaign state.
	 */
	class HANSASIMULATION_API FHansaSimulationDefinitionContext final
	{
	public:
		FHansaSimulationDefinitionContext() = default;

		static THansaValueResult<FHansaSimulationDefinitionContext> TryCreate(
			FHansaScenarioId ScenarioId,
			uint64 DefinitionHash);
		static THansaValueResult<FHansaSimulationDefinitionContext> TryCreate(
			FHansaScenarioId ScenarioId,
			uint64 DefinitionHash,
			FHansaEconomicRegistry EconomicRegistry,
			TMap<uint64, FString> InCompatibleDefinitionMigrations = {});
		static THansaValueResult<FHansaSimulationDefinitionContext> TryCreate(
			FHansaScenarioId ScenarioId,
			uint64 DefinitionHash,
			FHansaEconomicRegistry EconomicRegistry,
			FHansaPlacementTopology PlacementTopology,
			TMap<uint64, FString> InCompatibleDefinitionMigrations = {});
		static THansaValueResult<FHansaSimulationDefinitionContext> TryCreate(
			FHansaScenarioId ScenarioId,
			uint64 DefinitionHash,
			FHansaPlacementTopology PlacementTopology);

		[[nodiscard]] bool IsValid() const;
		[[nodiscard]] const FHansaScenarioId& GetScenarioId() const { return ScenarioId; }
		[[nodiscard]] uint64 GetDefinitionHash() const { return DefinitionHash; }
		[[nodiscard]] const FHansaEconomicRegistry* GetEconomicRegistry() const;
		[[nodiscard]] const FHansaPlacementTopology* GetPlacementTopology() const { return PlacementTopology.Get(); }
		[[nodiscard]] TSharedPtr<const FHansaPlacementTopology> GetPlacementTopologyShared() const { return PlacementTopology; }
		[[nodiscard]] uint64 GetPlacementTopologyHash() const
		{
			return PlacementTopology.IsValid() ? PlacementTopology->GetTopologyHash() : 0;
		}
		[[nodiscard]] TOptional<FString> FindCompatibleDefinitionMigration(
			uint64 SavedDefinitionHash,
			uint64 SavedRegistryHash) const;

		friend bool operator==(
			const FHansaSimulationDefinitionContext& Left,
			const FHansaSimulationDefinitionContext& Right)
		{
			return Left.ScenarioId == Right.ScenarioId && Left.DefinitionHash == Right.DefinitionHash &&
				Left.EconomicRegistry.IsSet() == Right.EconomicRegistry.IsSet() &&
				Left.GetPlacementTopologyHash() == Right.GetPlacementTopologyHash();
		}

	private:
		FHansaSimulationDefinitionContext(const FHansaScenarioId InScenarioId, const uint64 InDefinitionHash)
			: ScenarioId(InScenarioId)
			, DefinitionHash(InDefinitionHash)
		{
		}

		FHansaScenarioId ScenarioId;
		uint64 DefinitionHash = 0;
		TOptional<FHansaEconomicRegistry> EconomicRegistry;
		TSharedPtr<const FHansaPlacementTopology> PlacementTopology;
		TMap<uint64, FString> CompatibleDefinitionMigrations;
	};
}
