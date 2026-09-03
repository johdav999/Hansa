#include "Session/HansaAutomationSessionService.h"

#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

namespace Hansa::Automation
{
	namespace
	{
		FHansaAutomationCapabilityDescriptor MakeCapability(
			const EHansaAutomationCapability Capability,
			const EHansaAutomationPermissionLevel Permission,
			const bool bMutating)
		{
			return FHansaAutomationCapabilityDescriptor { Capability, LexToString(Capability), Permission, bMutating };
		}

		bool IsBoundedIdentifier(const FString& Value)
		{
			if (Value.IsEmpty() || Value.Len() > 64)
			{
				return false;
			}
			for (const TCHAR Character : Value)
			{
				if (!(FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('_') ||
					Character == TEXT(':') || Character == TEXT('-')))
				{
					return false;
				}
			}
			return true;
		}

		bool IsKnownPermission(const EHansaAutomationPermissionLevel Permission)
		{
			return Permission >= EHansaAutomationPermissionLevel::ReadOnly &&
				Permission <= EHansaAutomationPermissionLevel::FixtureControl;
		}

		FHansaAutomationError MakeError(
			const EHansaAutomationErrorCode Code,
			const FHansaAutomationRequestContext& Context,
			const TCHAR* Message,
			const TCHAR* Remedy,
			const bool bRetryable = false)
		{
			return FHansaAutomationError { Code, Context.CorrelationId, Message, Remedy, bRetryable };
		}

		bool ResolveOperation(
			const EHansaAutomationOperation Operation,
			EHansaAutomationCapability& OutCapability,
			EHansaAutomationPermissionLevel& OutPermission)
		{
			switch (Operation)
			{
			case EHansaAutomationOperation::SessionGet:
				OutCapability = EHansaAutomationCapability::Session;
				OutPermission = EHansaAutomationPermissionLevel::ReadOnly;
				return true;
			case EHansaAutomationOperation::CapabilitiesGet:
				OutCapability = EHansaAutomationCapability::CapabilityDiscovery;
				OutPermission = EHansaAutomationPermissionLevel::ReadOnly;
				return true;
			case EHansaAutomationOperation::HealthGet:
				OutCapability = EHansaAutomationCapability::Health;
				OutPermission = EHansaAutomationPermissionLevel::ReadOnly;
				return true;
			case EHansaAutomationOperation::GameplayQuery:
				OutCapability = EHansaAutomationCapability::GameplayQueries;
				OutPermission = EHansaAutomationPermissionLevel::ReadOnly;
				return true;
			case EHansaAutomationOperation::ControlledCommand:
				OutCapability = EHansaAutomationCapability::ControlledCommands;
				OutPermission = EHansaAutomationPermissionLevel::ControlledActions;
				return true;
			case EHansaAutomationOperation::FixtureLoad:
				OutCapability = EHansaAutomationCapability::FixtureControl;
				OutPermission = EHansaAutomationPermissionLevel::FixtureControl;
				return true;
			case EHansaAutomationOperation::SemanticUiRead:
				OutCapability = EHansaAutomationCapability::SemanticUi;
				OutPermission = EHansaAutomationPermissionLevel::ReadOnly;
				return true;
			case EHansaAutomationOperation::SemanticUiAction:
				OutCapability = EHansaAutomationCapability::SemanticUi;
				OutPermission = EHansaAutomationPermissionLevel::ControlledActions;
				return true;
			case EHansaAutomationOperation::ScreenshotCapture:
				OutCapability = EHansaAutomationCapability::Screenshots;
				OutPermission = EHansaAutomationPermissionLevel::ReadOnly;
				return true;
			case EHansaAutomationOperation::WaitFor:
				OutCapability = EHansaAutomationCapability::WaitAssertions;
				OutPermission = EHansaAutomationPermissionLevel::ReadOnly;
				return true;
			default:
				return false;
			}
		}
	}

	FHansaAutomationSessionSettings FHansaAutomationSessionSettings::CreateDefault(
		const bool bInEnabled,
		const EHansaAutomationPermissionLevel InMaximumPermission,
		const FString& InRequiredAuthenticationToken)
	{
		FHansaAutomationSessionSettings Result;
		Result.bEnabled = bInEnabled;
		Result.MaximumPermission = InMaximumPermission;
		Result.RequiredAuthenticationToken = InRequiredAuthenticationToken;
		Result.SupportedCapabilities = {
			MakeCapability(EHansaAutomationCapability::Session, EHansaAutomationPermissionLevel::ReadOnly, false),
			MakeCapability(EHansaAutomationCapability::CapabilityDiscovery, EHansaAutomationPermissionLevel::ReadOnly, false),
			MakeCapability(EHansaAutomationCapability::Health, EHansaAutomationPermissionLevel::ReadOnly, false),
			MakeCapability(EHansaAutomationCapability::GameplayQueries, EHansaAutomationPermissionLevel::ReadOnly, false),
			MakeCapability(EHansaAutomationCapability::ControlledCommands, EHansaAutomationPermissionLevel::ControlledActions, true),
			MakeCapability(EHansaAutomationCapability::FixtureControl, EHansaAutomationPermissionLevel::FixtureControl, true),
			MakeCapability(EHansaAutomationCapability::SemanticUi, EHansaAutomationPermissionLevel::ReadOnly, true),
			MakeCapability(EHansaAutomationCapability::Screenshots, EHansaAutomationPermissionLevel::ReadOnly, false),
			MakeCapability(EHansaAutomationCapability::WaitAssertions, EHansaAutomationPermissionLevel::ReadOnly, false)
		};
		return Result;
	}

	FHansaAutomationSessionService::FHansaAutomationSessionService(
		FHansaAutomationSessionSettings InSettings,
		TFunction<int64()> InMonotonicClock)
		: Settings(MoveTemp(InSettings))
		, MonotonicClock(MoveTemp(InMonotonicClock))
	{
		if (!MonotonicClock)
		{
			MonotonicClock = []
			{
				return static_cast<int64>(FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64()));
			};
		}
		Settings.SupportedCapabilities.Sort([](
			const FHansaAutomationCapabilityDescriptor& Left,
			const FHansaAutomationCapabilityDescriptor& Right)
		{
			return static_cast<uint8>(Left.Capability) < static_cast<uint8>(Right.Capability);
		});
	}

	FHansaAutomationCapabilityResult FHansaAutomationSessionService::DiscoverCapabilities(
		const FHansaAutomationRequestContext& Context) const
	{
		if (const FHansaAutomationError Error = ValidateContext(Context); Error.IsError())
		{
			return FHansaAutomationCapabilityResult::Failure(Error);
		}

		FHansaAutomationCapabilityManifest Manifest;
		Manifest.ProtocolVersion = Settings.ProtocolVersion;
		Manifest.MaximumPermission = Settings.MaximumPermission;
		Manifest.Capabilities = Settings.SupportedCapabilities;
		return FHansaAutomationCapabilityResult::Success(Manifest);
	}

	FHansaAutomationOpenSessionResult FHansaAutomationSessionService::OpenSession(
		const FHansaAutomationOpenSessionRequest& Request,
		const FHansaAutomationRequestContext& Context)
	{
		if (const FHansaAutomationError Error = ValidateContext(Context); Error.IsError())
		{
			return FHansaAutomationOpenSessionResult::Failure(Error);
		}
		if (!Request.ProtocolVersion.IsValid() ||
			Request.ProtocolVersion.Major != Settings.ProtocolVersion.Major ||
			Request.ProtocolVersion.Minor > Settings.ProtocolVersion.Minor)
		{
			return FHansaAutomationOpenSessionResult::Failure(MakeError(
				EHansaAutomationErrorCode::IncompatibleProtocol,
				Context,
				TEXT("The requested automation protocol version is not compatible with this process."),
				TEXT("Discover capabilities and retry with a supported protocol major/minor.")));
		}
		if (!IsBoundedIdentifier(Request.ControllerId))
		{
			return FHansaAutomationOpenSessionResult::Failure(MakeError(
				EHansaAutomationErrorCode::InvalidController,
				Context,
				TEXT("Controller identity is empty, too long, or contains unsupported characters."),
				TEXT("Use a stable 1-64 character alphanumeric controller identity.")));
		}
		if (Settings.RequiredAuthenticationToken.IsEmpty() ||
			Request.AuthenticationToken != Settings.RequiredAuthenticationToken)
		{
			return FHansaAutomationOpenSessionResult::Failure(MakeError(
				EHansaAutomationErrorCode::AuthenticationFailed,
				Context,
				TEXT("Session authentication failed."),
				TEXT("Restart with an explicit short-lived token and provide the matching token.")));
		}
		if (!IsKnownPermission(Request.RequestedPermission) ||
			Request.RequestedPermission > Settings.MaximumPermission)
		{
			return FHansaAutomationOpenSessionResult::Failure(MakeError(
				EHansaAutomationErrorCode::PermissionDenied,
				Context,
				TEXT("Requested permission exceeds the startup-authorized ceiling."),
				TEXT("Request a lower permission or restart under an approved test profile.")));
		}
		if (HasActiveSession())
		{
			return FHansaAutomationOpenSessionResult::Failure(MakeError(
				ActiveSession.ControllerId == Request.ControllerId
					? EHansaAutomationErrorCode::SessionAlreadyOpen
					: EHansaAutomationErrorCode::ControllerConflict,
				Context,
				TEXT("This process already has an active automation controller."),
				TEXT("Close the active session before opening another one.")));
		}

		TArray<EHansaAutomationCapability> GrantedCapabilities;
		for (const EHansaAutomationCapability RequiredCapability : Request.RequiredCapabilities)
		{
			const FHansaAutomationCapabilityDescriptor* Descriptor = FindCapability(RequiredCapability);
			if (Descriptor == nullptr)
			{
				return FHansaAutomationOpenSessionResult::Failure(MakeError(
					EHansaAutomationErrorCode::MissingCapability,
					Context,
					TEXT("A required capability is unavailable in this process."),
					TEXT("Inspect capability discovery and remove or defer the unsupported operation.")));
			}
			if (Descriptor->MinimumPermission > Request.RequestedPermission)
			{
				return FHansaAutomationOpenSessionResult::Failure(MakeError(
					EHansaAutomationErrorCode::PermissionDenied,
					Context,
					TEXT("A required capability needs a higher permission level."),
					TEXT("Use a startup-approved profile and request the capability explicitly.")));
			}
			GrantedCapabilities.AddUnique(RequiredCapability);
		}
		GrantedCapabilities.Sort([](const EHansaAutomationCapability Left, const EHansaAutomationCapability Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});

		ActiveSession.SessionId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		ActiveSession.ControllerId = Request.ControllerId;
		ActiveSession.ProtocolVersion = Settings.ProtocolVersion;
		ActiveSession.Permission = Request.RequestedPermission;
		ActiveSession.GrantedCapabilities = MoveTemp(GrantedCapabilities);
		ActiveSession.OpenedAtMonotonicMilliseconds = MonotonicClock();
		return FHansaAutomationOpenSessionResult::Success(ActiveSession);
	}

	FHansaAutomationSessionResult FHansaAutomationSessionService::GetSession(
		const FString& SessionId,
		const FString& ControllerId,
		const FHansaAutomationRequestContext& Context) const
	{
		const FHansaAutomationOperationResult Authorization = AuthorizeOperation(
			SessionId,
			ControllerId,
			EHansaAutomationOperation::SessionGet,
			Context);
		if (!Authorization)
		{
			return FHansaAutomationSessionResult::Failure(Authorization.GetError());
		}
		return FHansaAutomationSessionResult::Success(ActiveSession);
	}

	FHansaAutomationOperationResult FHansaAutomationSessionService::CloseSession(
		const FString& SessionId,
		const FString& ControllerId,
		const FHansaAutomationRequestContext& Context)
	{
		if (const FHansaAutomationError Error = ValidateSessionIdentity(SessionId, ControllerId, Context); Error.IsError())
		{
			return FHansaAutomationOperationResult::Failure(Error);
		}
		ActiveSession = FHansaAutomationSessionSnapshot();
		return FHansaAutomationOperationResult::Success(FHansaAutomationEmptyPayload());
	}

	FHansaAutomationOperationResult FHansaAutomationSessionService::AuthorizeOperation(
		const FString& SessionId,
		const FString& ControllerId,
		const EHansaAutomationOperation Operation,
		const FHansaAutomationRequestContext& Context) const
	{
		if (const FHansaAutomationError Error = ValidateSessionIdentity(SessionId, ControllerId, Context); Error.IsError())
		{
			return FHansaAutomationOperationResult::Failure(Error);
		}

		EHansaAutomationCapability RequiredCapability;
		EHansaAutomationPermissionLevel RequiredPermission;
		if (!ResolveOperation(Operation, RequiredCapability, RequiredPermission))
		{
			return FHansaAutomationOperationResult::Failure(MakeError(
				EHansaAutomationErrorCode::OperationUnsupported,
				Context,
				TEXT("The requested operation is not part of the bounded automation protocol."),
				TEXT("Use an explicitly declared operation from capability discovery.")));
		}
		if (!ActiveSession.GrantedCapabilities.Contains(RequiredCapability))
		{
			return FHansaAutomationOperationResult::Failure(MakeError(
				EHansaAutomationErrorCode::MissingCapability,
				Context,
				TEXT("The active session was not granted the capability required by this operation."),
				TEXT("Open a new session that explicitly requests an available capability.")));
		}
		if (ActiveSession.Permission < RequiredPermission)
		{
			return FHansaAutomationOperationResult::Failure(MakeError(
				EHansaAutomationErrorCode::PermissionDenied,
				Context,
				TEXT("The active session permission does not authorize this operation."),
				TEXT("Use a startup-approved profile with the required permission.")));
		}
		return FHansaAutomationOperationResult::Success(FHansaAutomationEmptyPayload());
	}

	FHansaAutomationError FHansaAutomationSessionService::ValidateContext(
		const FHansaAutomationRequestContext& Context) const
	{
		if (!Settings.bEnabled)
		{
			return MakeError(
				EHansaAutomationErrorCode::Disabled,
				Context,
				TEXT("Automation was not explicitly enabled for this process."),
				TEXT("Restart a non-Shipping target with the dedicated enable flag and token."));
		}
		if (!IsBoundedIdentifier(Context.CorrelationId))
		{
			return MakeError(
				EHansaAutomationErrorCode::InvalidCorrelationId,
				Context,
				TEXT("Correlation identity is empty, too long, or contains unsupported characters."),
				TEXT("Use a stable 1-64 character alphanumeric correlation identity."));
		}
		if (Context.TimeoutMilliseconds <= 0 ||
			Context.TimeoutMilliseconds > Settings.MaximumTimeoutMilliseconds ||
			Context.EnqueuedAtMonotonicMilliseconds < 0)
		{
			return MakeError(
				EHansaAutomationErrorCode::InvalidTimeout,
				Context,
				TEXT("Request timeout is outside the allowed bounded range."),
				TEXT("Use a positive timeout no greater than the advertised process maximum."));
		}

		const int64 Now = MonotonicClock();
		if (Now < Context.EnqueuedAtMonotonicMilliseconds)
		{
			return MakeError(
				EHansaAutomationErrorCode::InvalidRequest,
				Context,
				TEXT("Request enqueue time is ahead of the process monotonic clock."),
				TEXT("Construct request context at the receiving endpoint."));
		}
		if (Now - Context.EnqueuedAtMonotonicMilliseconds >= Context.TimeoutMilliseconds)
		{
			return MakeError(
				EHansaAutomationErrorCode::TimedOut,
				Context,
				TEXT("Request expired before it could be authorized."),
				TEXT("Retry with a fresh correlation ID and bounded timeout."),
				true);
		}
		return FHansaAutomationError();
	}

	FHansaAutomationError FHansaAutomationSessionService::ValidateSessionIdentity(
		const FString& SessionId,
		const FString& ControllerId,
		const FHansaAutomationRequestContext& Context) const
	{
		if (const FHansaAutomationError Error = ValidateContext(Context); Error.IsError())
		{
			return Error;
		}
		if (!HasActiveSession())
		{
			return MakeError(
				EHansaAutomationErrorCode::NoActiveSession,
				Context,
				TEXT("No automation session is active."),
				TEXT("Open an authenticated compatible session first."));
		}
		if (SessionId != ActiveSession.SessionId || ControllerId != ActiveSession.ControllerId)
		{
			return MakeError(
				EHansaAutomationErrorCode::SessionMismatch,
				Context,
				TEXT("Session or controller identity does not match the active session."),
				TEXT("Use the session identity returned by session open."));
		}
		return FHansaAutomationError();
	}

	const FHansaAutomationCapabilityDescriptor* FHansaAutomationSessionService::FindCapability(
		const EHansaAutomationCapability Capability) const
	{
		return Settings.SupportedCapabilities.FindByPredicate([Capability](const FHansaAutomationCapabilityDescriptor& Descriptor)
		{
			return Descriptor.Capability == Capability;
		});
	}
}
