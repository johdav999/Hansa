#include "Commands/HansaGameplayCommandGateway.h"

#include "Systems/HansaSimulationPipeline.h"

namespace Hansa::Simulation
{
	const TCHAR* LexToString(const EHansaCommandGatewayError Error)
	{
		switch (Error)
		{
		case EHansaCommandGatewayError::None: return TEXT("None");
		case EHansaCommandGatewayError::UninitializedState: return TEXT("UninitializedState");
		case EHansaCommandGatewayError::InvalidDefinitionContext: return TEXT("InvalidDefinitionContext");
		case EHansaCommandGatewayError::UnsupportedSchemaVersion: return TEXT("UnsupportedSchemaVersion");
		case EHansaCommandGatewayError::InvalidCommandIdentity: return TEXT("InvalidCommandIdentity");
		case EHansaCommandGatewayError::InvalidAuthorityContext: return TEXT("InvalidAuthorityContext");
		case EHansaCommandGatewayError::UnknownIssuingHouse: return TEXT("UnknownIssuingHouse");
		case EHansaCommandGatewayError::ExecutionTickMismatch: return TEXT("ExecutionTickMismatch");
		case EHansaCommandGatewayError::CommandOrderInvalid: return TEXT("CommandOrderInvalid");
		case EHansaCommandGatewayError::CommandIdentityOrderInvalid: return TEXT("CommandIdentityOrderInvalid");
		case EHansaCommandGatewayError::CommandCountOverflow: return TEXT("CommandCountOverflow");
		case EHansaCommandGatewayError::EventCountOverflow: return TEXT("EventCountOverflow");
		case EHansaCommandGatewayError::InvalidPayload: return TEXT("InvalidPayload");
		case EHansaCommandGatewayError::TargetAlreadyExists: return TEXT("TargetAlreadyExists");
		case EHansaCommandGatewayError::TargetNotFound: return TEXT("TargetNotFound");
		case EHansaCommandGatewayError::NotAuthorized: return TEXT("NotAuthorized");
		case EHansaCommandGatewayError::ClockOverflow: return TEXT("ClockOverflow");
		default: return TEXT("UnknownCommandGatewayError");
		}
	}

	FHansaCommandGatewayResult FHansaGameplayCommandGateway::ExecuteTick(
		FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions,
		const TConstArrayView<FHansaGameplayCommand> Commands,
		FHansaSimulationTransientCache& TransientCache)
	{
		FHansaSimulationStepInput Input;
		Input.Commands = Commands;
		return FHansaSimulationPipeline::AdvanceOneTick(State, Definitions, Input, TransientCache);
	}
}
