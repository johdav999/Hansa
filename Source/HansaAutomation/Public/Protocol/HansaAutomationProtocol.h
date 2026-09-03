#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"

namespace Hansa::Automation
{
	struct HANSAAUTOMATION_API FHansaAutomationProtocolVersion final
	{
		int32 Major = 1;
		int32 Minor = 0;

		[[nodiscard]] bool IsValid() const { return Major > 0 && Minor >= 0; }

		friend bool operator==(const FHansaAutomationProtocolVersion& Left, const FHansaAutomationProtocolVersion& Right)
		{
			return Left.Major == Right.Major && Left.Minor == Right.Minor;
		}
	};

	enum class EHansaAutomationPermissionLevel : uint8
	{
		None = 0,
		ReadOnly = 1,
		ControlledActions = 2,
		FixtureControl = 3
	};

	enum class EHansaAutomationCapability : uint8
	{
		Session = 0,
		CapabilityDiscovery,
		Health,
		GameplayQueries,
		ControlledCommands,
		FixtureControl,
		SemanticUi,
		Screenshots,
		WaitAssertions
	};

	enum class EHansaAutomationOperation : uint8
	{
		SessionGet = 0,
		CapabilitiesGet,
		HealthGet,
		GameplayQuery,
		ControlledCommand,
		FixtureLoad,
		SemanticUiRead,
		SemanticUiAction,
		ScreenshotCapture,
		WaitFor
	};

	enum class EHansaAutomationErrorCode : uint8
	{
		None = 0,
		Disabled,
		InvalidRequest,
		InvalidCorrelationId,
		InvalidTimeout,
		TimedOut,
		IncompatibleProtocol,
		AuthenticationFailed,
		InvalidController,
		PermissionDenied,
		MissingCapability,
		SessionAlreadyOpen,
		ControllerConflict,
		NoActiveSession,
		SessionMismatch,
		OperationUnsupported,
		SemanticNodeNotFound,
		SemanticActionUnsupported,
		CaptureUnavailable,
		InvalidCaptureSize,
		EvidenceWriteFailed
	};

	HANSAAUTOMATION_API const TCHAR* LexToString(EHansaAutomationPermissionLevel Permission);
	HANSAAUTOMATION_API const TCHAR* LexToString(EHansaAutomationCapability Capability);
	HANSAAUTOMATION_API const TCHAR* LexToString(EHansaAutomationOperation Operation);
	HANSAAUTOMATION_API const TCHAR* LexToString(EHansaAutomationErrorCode Error);
	HANSAAUTOMATION_API bool TryParsePermissionLevel(const FString& Text, EHansaAutomationPermissionLevel& OutPermission);
	HANSAAUTOMATION_API bool TryParseCapability(const FString& Text, EHansaAutomationCapability& OutCapability);

	struct HANSAAUTOMATION_API FHansaAutomationRequestContext final
	{
		FString CorrelationId;
		int64 EnqueuedAtMonotonicMilliseconds = 0;
		int64 TimeoutMilliseconds = 0;
	};

	struct HANSAAUTOMATION_API FHansaAutomationError final
	{
		EHansaAutomationErrorCode Code = EHansaAutomationErrorCode::None;
		FString CorrelationId;
		FString Message;
		FString Remedy;
		bool bRetryable = false;

		[[nodiscard]] bool IsError() const { return Code != EHansaAutomationErrorCode::None; }
	};

	struct HANSAAUTOMATION_API FHansaAutomationCapabilityDescriptor final
	{
		EHansaAutomationCapability Capability = EHansaAutomationCapability::Session;
		FString StableName;
		EHansaAutomationPermissionLevel MinimumPermission = EHansaAutomationPermissionLevel::ReadOnly;
		bool bMutating = false;
	};

	struct HANSAAUTOMATION_API FHansaAutomationCapabilityManifest final
	{
		FHansaAutomationProtocolVersion ProtocolVersion;
		EHansaAutomationPermissionLevel MaximumPermission = EHansaAutomationPermissionLevel::None;
		TArray<FHansaAutomationCapabilityDescriptor> Capabilities;
	};

	template <typename TPayload>
	class THansaAutomationResult final
	{
	public:
		static THansaAutomationResult Success(const TPayload& InPayload)
		{
			THansaAutomationResult Result;
			Result.Payload = InPayload;
			return Result;
		}

		static THansaAutomationResult Failure(const FHansaAutomationError& InError)
		{
			THansaAutomationResult Result;
			Result.Error = InError;
			return Result;
		}

		[[nodiscard]] bool IsSuccess() const { return !Error.IsError(); }
		explicit operator bool() const { return IsSuccess(); }
		[[nodiscard]] const FHansaAutomationError& GetError() const { return Error; }
		[[nodiscard]] const TPayload& GetPayload() const { return Payload; }

	private:
		FHansaAutomationError Error;
		TPayload Payload;
	};

	struct HANSAAUTOMATION_API FHansaAutomationEmptyPayload final
	{
	};

	using FHansaAutomationOperationResult = THansaAutomationResult<FHansaAutomationEmptyPayload>;
	using FHansaAutomationCapabilityResult = THansaAutomationResult<FHansaAutomationCapabilityManifest>;
}
