#pragma once

#include "Model/HansaIds.h"
#include "Model/HansaValueResult.h"

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

		[[nodiscard]] bool IsValid() const { return ScenarioId.IsValid() && DefinitionHash != 0; }
		[[nodiscard]] const FHansaScenarioId& GetScenarioId() const { return ScenarioId; }
		[[nodiscard]] uint64 GetDefinitionHash() const { return DefinitionHash; }

		friend bool operator==(
			const FHansaSimulationDefinitionContext& Left,
			const FHansaSimulationDefinitionContext& Right)
		{
			return Left.ScenarioId == Right.ScenarioId && Left.DefinitionHash == Right.DefinitionHash;
		}

	private:
		FHansaSimulationDefinitionContext(const FHansaScenarioId InScenarioId, const uint64 InDefinitionHash)
			: ScenarioId(InScenarioId)
			, DefinitionHash(InDefinitionHash)
		{
		}

		FHansaScenarioId ScenarioId;
		uint64 DefinitionHash = 0;
	};
}
