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
}
