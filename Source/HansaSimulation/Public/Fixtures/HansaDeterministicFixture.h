#pragma once

#include "Commands/HansaGameplayCommandGateway.h"
#include "Diagnostics/HansaDeterminismTrace.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"

namespace Hansa::Simulation
{
	class HANSASIMULATION_API FHansaDeterministicFixtureDescriptor final
	{
	public:
		static THansaValueResult<FHansaDeterministicFixtureDescriptor> TryCreate(
			FString FixtureId,
			uint32 SchemaVersion,
			FString Owner,
			const FHansaSimulationDefinitionContext& Definitions,
			FHansaSimulationInitialization Initialization);

		[[nodiscard]] const FString& GetFixtureId() const { return FixtureId; }
		[[nodiscard]] uint32 GetSchemaVersion() const { return SchemaVersion; }
		[[nodiscard]] const FString& GetOwner() const { return Owner; }
		[[nodiscard]] uint64 GetSeed() const { return InitialState.CreateReadOnlyAccess(Definitions).GetCampaignSeed(); }
		[[nodiscard]] uint64 GetDefinitionHash() const { return Definitions.GetDefinitionHash(); }
		[[nodiscard]] FHansaSimulationTick GetInitialTick() const
		{
			return InitialState.CreateReadOnlyAccess(Definitions).GetClock().GetTick();
		}

	private:
		friend class FHansaDeterministicFixtureHarness;

		FString FixtureId;
		uint32 SchemaVersion = 0;
		FString Owner;
		FHansaSimulationDefinitionContext Definitions;
		FHansaSimulationState InitialState;
	};

	enum class EHansaFixtureRunError : uint8
	{
		None = 0,
		NegativeTickCount,
		TickRangeOverflow,
		CommandOutsideTickRange,
		CommandScheduleOrderInvalid,
		GatewayRejected,
		ProjectionFailed,
		TraceInvalid
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaFixtureRunError Error);

	class HANSASIMULATION_API FHansaFixtureRunResult final
	{
	public:
		[[nodiscard]] bool IsSuccess() const { return Error == EHansaFixtureRunError::None; }
		explicit operator bool() const { return IsSuccess(); }
		[[nodiscard]] EHansaFixtureRunError GetError() const { return Error; }
		[[nodiscard]] EHansaCommandGatewayError GetGatewayError() const { return GatewayError; }
		[[nodiscard]] FHansaSimulationTick GetFailedTick() const { return FailedTick; }
		[[nodiscard]] int64 GetRequestedTickCount() const { return RequestedTickCount; }
		[[nodiscard]] const FString& GetOwner() const { return Owner; }
		[[nodiscard]] const FHansaDeterminismTrace& GetTrace() const { return Trace; }
		[[nodiscard]] const FHansaSimulationProjection& GetFinalProjection() const { return FinalProjection; }

	private:
		friend class FHansaDeterministicFixtureHarness;

		EHansaFixtureRunError Error = EHansaFixtureRunError::None;
		EHansaCommandGatewayError GatewayError = EHansaCommandGatewayError::None;
		FHansaSimulationTick FailedTick;
		int64 RequestedTickCount = 0;
		FString Owner;
		FHansaDeterminismTrace Trace;
		FHansaSimulationProjection FinalProjection;
	};

	class HANSASIMULATION_API FHansaDeterministicFixtureHarness final
	{
	public:
		[[nodiscard]] static FHansaFixtureRunResult RunExactTicks(
			const FHansaDeterministicFixtureDescriptor& Descriptor,
			int64 TickCount,
			TConstArrayView<FHansaGameplayCommand> Commands = {});
	};
}
