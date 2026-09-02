#pragma once

#include "Commands/HansaGameplayCommand.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Events/HansaDomainEvent.h"
#include "Queries/HansaSimulationReadOnly.h"

namespace Hansa::Simulation
{
	class FHansaSimulationDefinitionContext;
	class FHansaSimulationState;
	class FHansaSimulationTransientCache;

	/** Stable rejection causes suitable for transport, localization lookup and deterministic tests. */
	enum class EHansaCommandGatewayError : uint8
	{
		None = 0,
		UninitializedState,
		InvalidDefinitionContext,
		UnsupportedSchemaVersion,
		InvalidCommandIdentity,
		InvalidAuthorityContext,
		UnknownIssuingHouse,
		ExecutionTickMismatch,
		CommandOrderInvalid,
		CommandIdentityOrderInvalid,
		CommandCountOverflow,
		EventCountOverflow,
		InvalidPayload,
		TargetAlreadyExists,
		TargetNotFound,
		NotAuthorized,
		ClockOverflow
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaCommandGatewayError Error);

	/** Owning result. Its ordered event array is empty on every rejection. */
	class HANSASIMULATION_API FHansaCommandGatewayResult final
	{
	public:
		[[nodiscard]] bool IsSuccess() const { return Error == EHansaCommandGatewayError::None; }
		explicit operator bool() const { return IsSuccess(); }
		[[nodiscard]] EHansaCommandGatewayError GetError() const { return Error; }
		[[nodiscard]] int32 GetFailedCommandIndex() const { return FailedCommandIndex; }
		[[nodiscard]] FHansaCommandId GetFailedCommandId() const { return FailedCommandId; }
		[[nodiscard]] FHansaSimulationTick GetTickBefore() const { return TickBefore; }
		[[nodiscard]] FHansaSimulationTick GetTickAfter() const { return TickAfter; }
		[[nodiscard]] const FHansaDeterminismFingerprint& GetFingerprintAfter() const { return FingerprintAfter; }
		[[nodiscard]] TConstArrayView<FHansaDomainEvent> GetEvents() const { return Events; }

	private:
		friend class FHansaSimulationPipeline;

		EHansaCommandGatewayError Error = EHansaCommandGatewayError::None;
		int32 FailedCommandIndex = INDEX_NONE;
		FHansaCommandId FailedCommandId;
		FHansaSimulationTick TickBefore;
		FHansaSimulationTick TickAfter;
		FHansaDeterminismFingerprint FingerprintAfter;
		TArray<FHansaDomainEvent> Events;
	};

	/** The sole public state-mutation entry point shared by player, AI, RPC and controlled automation callers. */
	class HANSASIMULATION_API FHansaGameplayCommandGateway final
	{
	public:
		static FHansaCommandGatewayResult ExecuteTick(
			FHansaSimulationState& State,
			const FHansaSimulationDefinitionContext& Definitions,
			TConstArrayView<FHansaGameplayCommand> Commands,
			FHansaSimulationTransientCache& TransientCache);
	};
}
