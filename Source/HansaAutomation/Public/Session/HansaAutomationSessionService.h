#pragma once

#include "Containers/Array.h"
#include "Protocol/HansaAutomationProtocol.h"
#include "Templates/Function.h"

namespace Hansa::Automation
{
	struct HANSAAUTOMATION_API FHansaAutomationSessionSettings final
	{
		bool bEnabled = false;
		FHansaAutomationProtocolVersion ProtocolVersion;
		EHansaAutomationPermissionLevel MaximumPermission = EHansaAutomationPermissionLevel::ReadOnly;
		FString RequiredAuthenticationToken;
		int64 MaximumTimeoutMilliseconds = 30000;
		TArray<FHansaAutomationCapabilityDescriptor> SupportedCapabilities;

		static FHansaAutomationSessionSettings CreateDefault(
			bool bInEnabled,
			EHansaAutomationPermissionLevel InMaximumPermission,
			const FString& InRequiredAuthenticationToken);
	};

	struct HANSAAUTOMATION_API FHansaAutomationOpenSessionRequest final
	{
		FHansaAutomationProtocolVersion ProtocolVersion;
		FString ControllerId;
		FString AuthenticationToken;
		EHansaAutomationPermissionLevel RequestedPermission = EHansaAutomationPermissionLevel::ReadOnly;
		TArray<EHansaAutomationCapability> RequiredCapabilities;
	};

	struct HANSAAUTOMATION_API FHansaAutomationSessionSnapshot final
	{
		FString SessionId;
		FString ControllerId;
		FHansaAutomationProtocolVersion ProtocolVersion;
		EHansaAutomationPermissionLevel Permission = EHansaAutomationPermissionLevel::None;
		TArray<EHansaAutomationCapability> GrantedCapabilities;
		int64 OpenedAtMonotonicMilliseconds = 0;
	};

	using FHansaAutomationOpenSessionResult = THansaAutomationResult<FHansaAutomationSessionSnapshot>;
	using FHansaAutomationSessionResult = THansaAutomationResult<FHansaAutomationSessionSnapshot>;

	/**
	 * In-process, transport-neutral automation boundary. It owns no endpoint, UObject,
	 * console bridge, filesystem surface, or mutable gameplay pointer.
	 */
	class HANSAAUTOMATION_API FHansaAutomationSessionService final
	{
	public:
		explicit FHansaAutomationSessionService(
			FHansaAutomationSessionSettings InSettings,
			TFunction<int64()> InMonotonicClock = TFunction<int64()>());

		[[nodiscard]] bool IsEnabled() const { return Settings.bEnabled; }
		[[nodiscard]] bool HasActiveSession() const { return ActiveSession.SessionId.Len() > 0; }
		[[nodiscard]] const FHansaAutomationProtocolVersion& GetProtocolVersion() const { return Settings.ProtocolVersion; }

		FHansaAutomationCapabilityResult DiscoverCapabilities(const FHansaAutomationRequestContext& Context) const;
		FHansaAutomationOpenSessionResult OpenSession(
			const FHansaAutomationOpenSessionRequest& Request,
			const FHansaAutomationRequestContext& Context);
		FHansaAutomationSessionResult GetSession(
			const FString& SessionId,
			const FString& ControllerId,
			const FHansaAutomationRequestContext& Context) const;
		FHansaAutomationOperationResult CloseSession(
			const FString& SessionId,
			const FString& ControllerId,
			const FHansaAutomationRequestContext& Context);
		FHansaAutomationOperationResult AuthorizeOperation(
			const FString& SessionId,
			const FString& ControllerId,
			EHansaAutomationOperation Operation,
			const FHansaAutomationRequestContext& Context) const;

	private:
		FHansaAutomationError ValidateContext(const FHansaAutomationRequestContext& Context) const;
		FHansaAutomationError ValidateSessionIdentity(
			const FString& SessionId,
			const FString& ControllerId,
			const FHansaAutomationRequestContext& Context) const;
		const FHansaAutomationCapabilityDescriptor* FindCapability(EHansaAutomationCapability Capability) const;

		FHansaAutomationSessionSettings Settings;
		TFunction<int64()> MonotonicClock;
		FHansaAutomationSessionSnapshot ActiveSession;
	};
}
