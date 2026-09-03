#include "Protocol/HansaAutomationProtocol.h"

namespace Hansa::Automation
{
	const TCHAR* LexToString(const EHansaAutomationPermissionLevel Permission)
	{
		switch (Permission)
		{
		case EHansaAutomationPermissionLevel::None: return TEXT("None");
		case EHansaAutomationPermissionLevel::ReadOnly: return TEXT("ReadOnly");
		case EHansaAutomationPermissionLevel::ControlledActions: return TEXT("ControlledActions");
		case EHansaAutomationPermissionLevel::FixtureControl: return TEXT("FixtureControl");
		default: return TEXT("UnknownPermission");
		}
	}

	const TCHAR* LexToString(const EHansaAutomationCapability Capability)
	{
		switch (Capability)
		{
		case EHansaAutomationCapability::Session: return TEXT("session");
		case EHansaAutomationCapability::CapabilityDiscovery: return TEXT("capabilities");
		case EHansaAutomationCapability::Health: return TEXT("health");
		case EHansaAutomationCapability::GameplayQueries: return TEXT("gameplay.query");
		case EHansaAutomationCapability::ControlledCommands: return TEXT("gameplay.command");
		case EHansaAutomationCapability::FixtureControl: return TEXT("fixture.control");
		case EHansaAutomationCapability::SemanticUi: return TEXT("semantic-ui");
		case EHansaAutomationCapability::Screenshots: return TEXT("screenshots");
		case EHansaAutomationCapability::WaitAssertions: return TEXT("wait-assertions");
		default: return TEXT("unknown-capability");
		}
	}

	const TCHAR* LexToString(const EHansaAutomationOperation Operation)
	{
		switch (Operation)
		{
		case EHansaAutomationOperation::SessionGet: return TEXT("session_get");
		case EHansaAutomationOperation::CapabilitiesGet: return TEXT("capabilities_get");
		case EHansaAutomationOperation::HealthGet: return TEXT("health_get");
		case EHansaAutomationOperation::GameplayQuery: return TEXT("gameplay_query");
		case EHansaAutomationOperation::ControlledCommand: return TEXT("controlled_command");
		case EHansaAutomationOperation::FixtureLoad: return TEXT("fixture_load");
		case EHansaAutomationOperation::SemanticUiRead: return TEXT("semantic_ui_read");
		case EHansaAutomationOperation::SemanticUiAction: return TEXT("semantic_ui_action");
		case EHansaAutomationOperation::ScreenshotCapture: return TEXT("screenshot_capture");
		case EHansaAutomationOperation::WaitFor: return TEXT("wait_for");
		default: return TEXT("unknown_operation");
		}
	}

	const TCHAR* LexToString(const EHansaAutomationErrorCode Error)
	{
		switch (Error)
		{
		case EHansaAutomationErrorCode::None: return TEXT("None");
		case EHansaAutomationErrorCode::Disabled: return TEXT("Disabled");
		case EHansaAutomationErrorCode::InvalidRequest: return TEXT("InvalidRequest");
		case EHansaAutomationErrorCode::InvalidCorrelationId: return TEXT("InvalidCorrelationId");
		case EHansaAutomationErrorCode::InvalidTimeout: return TEXT("InvalidTimeout");
		case EHansaAutomationErrorCode::TimedOut: return TEXT("TimedOut");
		case EHansaAutomationErrorCode::IncompatibleProtocol: return TEXT("IncompatibleProtocol");
		case EHansaAutomationErrorCode::AuthenticationFailed: return TEXT("AuthenticationFailed");
		case EHansaAutomationErrorCode::InvalidController: return TEXT("InvalidController");
		case EHansaAutomationErrorCode::PermissionDenied: return TEXT("PermissionDenied");
		case EHansaAutomationErrorCode::MissingCapability: return TEXT("MissingCapability");
		case EHansaAutomationErrorCode::SessionAlreadyOpen: return TEXT("SessionAlreadyOpen");
		case EHansaAutomationErrorCode::ControllerConflict: return TEXT("ControllerConflict");
		case EHansaAutomationErrorCode::NoActiveSession: return TEXT("NoActiveSession");
		case EHansaAutomationErrorCode::SessionMismatch: return TEXT("SessionMismatch");
		case EHansaAutomationErrorCode::OperationUnsupported: return TEXT("OperationUnsupported");
		case EHansaAutomationErrorCode::SemanticNodeNotFound: return TEXT("SemanticNodeNotFound");
		case EHansaAutomationErrorCode::SemanticActionUnsupported: return TEXT("SemanticActionUnsupported");
		case EHansaAutomationErrorCode::CaptureUnavailable: return TEXT("CaptureUnavailable");
		case EHansaAutomationErrorCode::InvalidCaptureSize: return TEXT("InvalidCaptureSize");
		case EHansaAutomationErrorCode::EvidenceWriteFailed: return TEXT("EvidenceWriteFailed");
		default: return TEXT("UnknownAutomationError");
		}
	}

	bool TryParsePermissionLevel(const FString& Text, EHansaAutomationPermissionLevel& OutPermission)
	{
		for (const EHansaAutomationPermissionLevel Candidate : {
			EHansaAutomationPermissionLevel::ReadOnly,
			EHansaAutomationPermissionLevel::ControlledActions,
			EHansaAutomationPermissionLevel::FixtureControl })
		{
			if (Text.Equals(LexToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutPermission = Candidate;
				return true;
			}
		}
		return false;
	}

	bool TryParseCapability(const FString& Text, EHansaAutomationCapability& OutCapability)
	{
		for (const EHansaAutomationCapability Candidate : {
			EHansaAutomationCapability::Session,
			EHansaAutomationCapability::CapabilityDiscovery,
			EHansaAutomationCapability::Health,
			EHansaAutomationCapability::GameplayQueries,
			EHansaAutomationCapability::ControlledCommands,
			EHansaAutomationCapability::FixtureControl,
			EHansaAutomationCapability::SemanticUi,
			EHansaAutomationCapability::Screenshots,
			EHansaAutomationCapability::WaitAssertions })
		{
			if (Text.Equals(LexToString(Candidate), ESearchCase::IgnoreCase))
			{
				OutCapability = Candidate;
				return true;
			}
		}
		return false;
	}
}
