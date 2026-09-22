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
		case EHansaCommandGatewayError::CityPrivilegeUnavailable: return TEXT("CityPrivilegeUnavailable");
		case EHansaCommandGatewayError::CityPrivilegeCostUnavailable: return TEXT("CityPrivilegeCostUnavailable");
		case EHansaCommandGatewayError::CityPrivilegeStateInvalid: return TEXT("CityPrivilegeStateInvalid");
		case EHansaCommandGatewayError::CityProjectUnavailable: return TEXT("CityProjectUnavailable");
		case EHansaCommandGatewayError::CityProjectCostUnavailable: return TEXT("CityProjectCostUnavailable");
		case EHansaCommandGatewayError::GovernanceTransitionUnavailable: return TEXT("GovernanceTransitionUnavailable");
		case EHansaCommandGatewayError::GovernanceScenarioGateRejected: return TEXT("GovernanceScenarioGateRejected");
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
		case EHansaCommandGatewayError::PlacementRejected: return TEXT("PlacementRejected");
		case EHansaCommandGatewayError::ConstructionCostUnavailable: return TEXT("ConstructionCostUnavailable");
		case EHansaCommandGatewayError::ConstructionStateInvalid: return TEXT("ConstructionStateInvalid");
		case EHansaCommandGatewayError::ConstructionRefundUnavailable: return TEXT("ConstructionRefundUnavailable");
		case EHansaCommandGatewayError::TargetHasDependents: return TEXT("TargetHasDependents");
		case EHansaCommandGatewayError::ResidenceProgressionUnavailable: return TEXT("ResidenceProgressionUnavailable");
		case EHansaCommandGatewayError::RouteRejected: return TEXT("RouteRejected");
		case EHansaCommandGatewayError::RouteStateInvalid: return TEXT("RouteStateInvalid");
		case EHansaCommandGatewayError::VehicleAlreadyAssigned: return TEXT("VehicleAlreadyAssigned");
		case EHansaCommandGatewayError::ResearchRejected: return TEXT("ResearchRejected");
		case EHansaCommandGatewayError::TargetHasCargoObligations: return TEXT("TargetHasCargoObligations");
		case EHansaCommandGatewayError::ResearchEffectRequired: return TEXT("ResearchEffectRequired");
		case EHansaCommandGatewayError::SpotTradeAccessUnavailable: return TEXT("SpotTradeAccessUnavailable");
		case EHansaCommandGatewayError::SpotTradeNotBerthed: return TEXT("SpotTradeNotBerthed");
		case EHansaCommandGatewayError::SpotTradeStaleReview: return TEXT("SpotTradeStaleReview");
		case EHansaCommandGatewayError::SpotTradeRejected: return TEXT("SpotTradeRejected");
		case EHansaCommandGatewayError::TradeStationAccessUnavailable: return TEXT("TradeStationAccessUnavailable");
		case EHansaCommandGatewayError::TradeStationSiteUnavailable: return TEXT("TradeStationSiteUnavailable");
		case EHansaCommandGatewayError::TradeStationCostUnavailable: return TEXT("TradeStationCostUnavailable");
		case EHansaCommandGatewayError::TradeStationStateInvalid: return TEXT("TradeStationStateInvalid");
		case EHansaCommandGatewayError::TradeStationHasCargo: return TEXT("TradeStationHasCargo");
		case EHansaCommandGatewayError::StationOrderStaleReview: return TEXT("StationOrderStaleReview");
		case EHansaCommandGatewayError::PresenceUpgradeUnavailable: return TEXT("PresenceUpgradeUnavailable");
		case EHansaCommandGatewayError::PresenceUpgradeRequirementsUnmet: return TEXT("PresenceUpgradeRequirementsUnmet");
		case EHansaCommandGatewayError::PresenceUpgradeCostUnavailable: return TEXT("PresenceUpgradeCostUnavailable");
		case EHansaCommandGatewayError::PresenceUpgradeStateInvalid: return TEXT("PresenceUpgradeStateInvalid");
		case EHansaCommandGatewayError::PresenceSpecializationUnavailable: return TEXT("PresenceSpecializationUnavailable");
		case EHansaCommandGatewayError::PresenceSpecializationStaleReview: return TEXT("PresenceSpecializationStaleReview");
		case EHansaCommandGatewayError::PresenceSpecializationCostUnavailable: return TEXT("PresenceSpecializationCostUnavailable");
		case EHansaCommandGatewayError::PresenceSpecializationStateInvalid: return TEXT("PresenceSpecializationStateInvalid");
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
