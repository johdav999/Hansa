#include "Fixtures/HansaDeterministicFixture.h"

#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	namespace
	{
		bool IsValidFixtureId(const FString& FixtureId)
		{
			if (FixtureId.IsEmpty() || FixtureId[0] < TEXT('a') || FixtureId[0] > TEXT('z') ||
				FixtureId.EndsWith(TEXT("_")) || FixtureId.Contains(TEXT("__")))
			{
				return false;
			}
			for (const TCHAR Character : FixtureId)
			{
				if (!((Character >= TEXT('a') && Character <= TEXT('z')) ||
					(Character >= TEXT('0') && Character <= TEXT('9')) || Character == TEXT('_')))
				{
					return false;
				}
			}
			int32 VersionMarker = INDEX_NONE;
			if (!FixtureId.FindLastChar(TEXT('_'), VersionMarker) || VersionMarker + 2 >= FixtureId.Len() ||
				FixtureId[VersionMarker + 1] != TEXT('v') || FixtureId[VersionMarker + 2] < TEXT('1') ||
				FixtureId[VersionMarker + 2] > TEXT('9'))
			{
				return false;
			}
			for (int32 Index = VersionMarker + 3; Index < FixtureId.Len(); ++Index)
			{
				if (FixtureId[Index] < TEXT('0') || FixtureId[Index] > TEXT('9'))
				{
					return false;
				}
			}
			return true;
		}

		bool IsValidOwner(const FString& Owner)
		{
			if (Owner.IsEmpty() || Owner.Len() > 64)
			{
				return false;
			}
			for (const TCHAR Character : Owner)
			{
				if (Character < 32 || Character > 126)
				{
					return false;
				}
			}
			return true;
		}
	}

	THansaValueResult<FHansaDeterministicFixtureDescriptor> FHansaDeterministicFixtureDescriptor::TryCreate(
		FString FixtureId,
		const uint32 SchemaVersion,
		FString Owner,
		const FHansaSimulationDefinitionContext& Definitions,
		FHansaSimulationInitialization Initialization)
	{
		if (!IsValidFixtureId(FixtureId) || SchemaVersion == 0 || !IsValidOwner(Owner) || !Definitions.IsValid())
		{
			return THansaValueResult<FHansaDeterministicFixtureDescriptor>::Failure(EHansaValueError::InvalidFormat);
		}
		const THansaValueResult<FHansaSimulationState> InitialState =
			FHansaSimulationState::TryCreate(MoveTemp(Initialization));
		if (!InitialState)
		{
			return THansaValueResult<FHansaDeterministicFixtureDescriptor>::Failure(InitialState.Error);
		}

		FHansaDeterministicFixtureDescriptor Descriptor;
		Descriptor.FixtureId = MoveTemp(FixtureId);
		Descriptor.SchemaVersion = SchemaVersion;
		Descriptor.Owner = MoveTemp(Owner);
		Descriptor.Definitions = Definitions;
		Descriptor.InitialState = InitialState.Value;
		return THansaValueResult<FHansaDeterministicFixtureDescriptor>::Success(Descriptor);
	}

	const TCHAR* LexToString(const EHansaFixtureRunError Error)
	{
		switch (Error)
		{
		case EHansaFixtureRunError::None: return TEXT("None");
		case EHansaFixtureRunError::NegativeTickCount: return TEXT("NegativeTickCount");
		case EHansaFixtureRunError::TickRangeOverflow: return TEXT("TickRangeOverflow");
		case EHansaFixtureRunError::CommandOutsideTickRange: return TEXT("CommandOutsideTickRange");
		case EHansaFixtureRunError::CommandScheduleOrderInvalid: return TEXT("CommandScheduleOrderInvalid");
		case EHansaFixtureRunError::GatewayRejected: return TEXT("GatewayRejected");
		case EHansaFixtureRunError::ProjectionFailed: return TEXT("ProjectionFailed");
		case EHansaFixtureRunError::TraceInvalid: return TEXT("TraceInvalid");
		default: return TEXT("UnknownFixtureRunError");
		}
	}

	FHansaFixtureRunResult FHansaDeterministicFixtureHarness::RunExactTicks(
		const FHansaDeterministicFixtureDescriptor& Descriptor,
		const int64 TickCount,
		const TConstArrayView<FHansaGameplayCommand> Commands)
	{
		FHansaFixtureRunResult Result;
		Result.RequestedTickCount = TickCount;
		Result.Owner = Descriptor.Owner;
		const int64 InitialTick = Descriptor.GetInitialTick().GetValue();
		if (TickCount < 0)
		{
			Result.Error = EHansaFixtureRunError::NegativeTickCount;
			return Result;
		}
		if (TickCount > MAX_int32 || TickCount > TNumericLimits<int64>::Max() - InitialTick)
		{
			Result.Error = EHansaFixtureRunError::TickRangeOverflow;
			return Result;
		}
		const int64 FinalTickExclusive = InitialTick + TickCount;

		int64 PreviousScheduledTick = InitialTick;
		uint64 PreviousSequence = 0;
		for (int32 Index = 0; Index < Commands.Num(); ++Index)
		{
			const FHansaCommandHeader& Header = Commands[Index].GetHeader();
			const int64 ScheduledTick = Header.RequestedExecutionTick.GetValue();
			if (ScheduledTick < InitialTick || ScheduledTick >= FinalTickExclusive)
			{
				Result.Error = EHansaFixtureRunError::CommandOutsideTickRange;
				Result.FailedTick = Header.RequestedExecutionTick;
				return Result;
			}
			if ((Index > 0 && ScheduledTick < PreviousScheduledTick) ||
				Header.GlobalSequence == 0 || Header.GlobalSequence <= PreviousSequence)
			{
				Result.Error = EHansaFixtureRunError::CommandScheduleOrderInvalid;
				Result.FailedTick = Header.RequestedExecutionTick;
				return Result;
			}
			PreviousScheduledTick = ScheduledTick;
			PreviousSequence = Header.GlobalSequence;
		}

		FHansaSimulationState State = Descriptor.InitialState;
		const FHansaStateHashReport InitialHashes =
			State.CreateReadOnlyAccess(Descriptor.Definitions).BuildStateHashReport();
		FHansaSimulationTransientCache Cache;
		TArray<FHansaDeterminismTickRecord> TickRecords;
		TickRecords.Reserve(static_cast<int32>(FMath::Min<int64>(TickCount, MAX_int32)));
		int32 CommandIndex = 0;
		for (int64 TickValue = InitialTick; TickValue < FinalTickExclusive; ++TickValue)
		{
			const int32 FirstCommandIndex = CommandIndex;
			while (CommandIndex < Commands.Num() &&
				Commands[CommandIndex].GetHeader().RequestedExecutionTick.GetValue() == TickValue)
			{
				++CommandIndex;
			}
			const int32 CommandsThisTick = CommandIndex - FirstCommandIndex;
			const TConstArrayView<FHansaGameplayCommand> TickCommands = CommandsThisTick > 0
				? TConstArrayView<FHansaGameplayCommand>(&Commands[FirstCommandIndex], CommandsThisTick)
				: TConstArrayView<FHansaGameplayCommand>();

			const FHansaCommandGatewayResult GatewayResult = FHansaGameplayCommandGateway::ExecuteTick(
				State, Descriptor.Definitions, TickCommands, Cache);
			if (!GatewayResult)
			{
				Result.Error = EHansaFixtureRunError::GatewayRejected;
				Result.GatewayError = GatewayResult.GetError();
				Result.FailedTick = GatewayResult.GetTickBefore();
				return Result;
			}

			FHansaDeterminismTickRecord TickRecord;
			TickRecord.ProcessedTick = GatewayResult.GetTickBefore();
			TickRecord.PipelineOrderHash =
				FHansaDeterminismDiagnostics::ComputePipelineOrderHash(Cache.GetLastPhaseOrder());
			TickRecord.DomainEventOrderHash =
				FHansaDeterminismDiagnostics::ComputeDomainEventOrderHash(GatewayResult.GetEvents());
			TickRecord.StateAfterTick = State.CreateReadOnlyAccess(Descriptor.Definitions).BuildStateHashReport();
			TickRecords.Add(MoveTemp(TickRecord));
		}

		const THansaValueResult<FHansaSimulationProjection> Projection =
			State.CreateReadOnlyAccess(Descriptor.Definitions).BuildProjection();
		if (!Projection)
		{
			Result.Error = EHansaFixtureRunError::ProjectionFailed;
			return Result;
		}
		const THansaValueResult<FHansaDeterminismTrace> Trace = FHansaDeterminismTrace::TryCreate(
			Descriptor.FixtureId,
			Descriptor.SchemaVersion,
			Descriptor.GetSeed(),
			Descriptor.GetDefinitionHash(),
			InitialHashes,
			MoveTemp(TickRecords));
		if (!Trace)
		{
			Result.Error = EHansaFixtureRunError::TraceInvalid;
			return Result;
		}

		Result.Trace = Trace.Value;
		Result.FinalProjection = Projection.Value;
		return Result;
	}
}
