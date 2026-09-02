#include "Diagnostics/HansaDeterminismTrace.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint64 FnvOffset = 14695981039346656037ULL;
		constexpr uint64 FnvPrime = 1099511628211ULL;

		void AddByte(uint64& Hash, const uint8 Value)
		{
			Hash ^= Value;
			Hash *= FnvPrime;
		}

		void AddUInt32(uint64& Hash, const uint32 Value)
		{
			for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
			{
				AddByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddUInt64(uint64& Hash, const uint64 Value)
		{
			for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
			{
				AddByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		FHansaDeterminismComparison CompareStateReports(
			const FHansaStateHashReport& Left,
			const FHansaStateHashReport& Right,
			const FHansaSimulationTick DivergentTick)
		{
			if (Left == Right)
			{
				return FHansaDeterminismComparison();
			}

			const TConstArrayView<FHansaSubsystemStateHash> LeftSubsystems = Left.GetSubsystems();
			const TConstArrayView<FHansaSubsystemStateHash> RightSubsystems = Right.GetSubsystems();
			const int32 SharedCount = FMath::Min(LeftSubsystems.Num(), RightSubsystems.Num());
			for (int32 Index = 0; Index < SharedCount; ++Index)
			{
				if (!(LeftSubsystems[Index] == RightSubsystems[Index]))
				{
					return FHansaDeterminismComparison::MakeDivergence(
						EHansaDeterminismDivergenceKind::StateSubsystem,
						DivergentTick,
						LeftSubsystems[Index].Subsystem,
						LeftSubsystems[Index].Value,
						RightSubsystems[Index].Value);
				}
			}

			return FHansaDeterminismComparison::MakeDivergence(
				EHansaDeterminismDivergenceKind::TraceStructure,
				DivergentTick,
				EHansaStateHashSubsystem::NotApplicable,
				Left.GetOverallHash(),
				Right.GetOverallHash());
		}
	}

	THansaValueResult<FHansaDeterminismTrace> FHansaDeterminismTrace::TryCreate(
		FString FixtureId,
		const uint32 FixtureSchemaVersion,
		const uint64 Seed,
		const uint64 DefinitionHash,
		FHansaStateHashReport InitialState,
		TArray<FHansaDeterminismTickRecord> Ticks)
	{
		if (FixtureId.IsEmpty() || FixtureSchemaVersion == 0 || DefinitionHash == 0)
		{
			return THansaValueResult<FHansaDeterminismTrace>::Failure(EHansaValueError::InvalidFormat);
		}
		int64 ExpectedTick = InitialState.GetTick().GetValue();
		for (const FHansaDeterminismTickRecord& Tick : Ticks)
		{
			if (Tick.ProcessedTick.GetValue() != ExpectedTick ||
				Tick.StateAfterTick.GetTick().GetValue() != ExpectedTick + 1 ||
				Tick.PipelineOrderHash == 0 || Tick.DomainEventOrderHash == 0)
			{
				return THansaValueResult<FHansaDeterminismTrace>::Failure(EHansaValueError::InvalidFormat);
			}
			++ExpectedTick;
		}

		FHansaDeterminismTrace Trace;
		Trace.FixtureId = MoveTemp(FixtureId);
		Trace.FixtureSchemaVersion = FixtureSchemaVersion;
		Trace.Seed = Seed;
		Trace.DefinitionHash = DefinitionHash;
		Trace.InitialState = MoveTemp(InitialState);
		Trace.Ticks = MoveTemp(Ticks);
		return THansaValueResult<FHansaDeterminismTrace>::Success(Trace);
	}

	const TCHAR* LexToString(const EHansaDeterminismDivergenceKind Kind)
	{
		switch (Kind)
		{
		case EHansaDeterminismDivergenceKind::None: return TEXT("None");
		case EHansaDeterminismDivergenceKind::TraceStructure: return TEXT("TraceStructure");
		case EHansaDeterminismDivergenceKind::PipelineOrder: return TEXT("PipelineOrder");
		case EHansaDeterminismDivergenceKind::DomainEventOrder: return TEXT("DomainEventOrder");
		case EHansaDeterminismDivergenceKind::StateSubsystem: return TEXT("StateSubsystem");
		default: return TEXT("UnknownDeterminismDivergence");
		}
	}

	FHansaDeterminismComparison FHansaDeterminismComparison::MakeDivergence(
		const EHansaDeterminismDivergenceKind Kind,
		const FHansaSimulationTick FirstDivergentTick,
		const EHansaStateHashSubsystem Subsystem,
		const uint64 LeftValue,
		const uint64 RightValue)
	{
		FHansaDeterminismComparison Result;
		Result.Kind = Kind;
		Result.FirstDivergentTick = FirstDivergentTick;
		Result.Subsystem = Subsystem;
		Result.LeftValue = LeftValue;
		Result.RightValue = RightValue;
		return Result;
	}

	FString FHansaDeterminismComparison::ToCompactDebugString() const
	{
		if (IsEqual())
		{
			return TEXT("DeterminismComparison[equal]");
		}
		return FString::Printf(
			TEXT("DeterminismComparison[firstTick=%lld;kind=%s;subsystem=%s;left=%016llX;right=%016llX]"),
			static_cast<long long>(FirstDivergentTick.GetValue()),
			LexToString(Kind),
			LexToString(Subsystem),
			static_cast<unsigned long long>(LeftValue),
			static_cast<unsigned long long>(RightValue));
	}

	uint64 FHansaDeterminismDiagnostics::ComputePipelineOrderHash(
		const TConstArrayView<EHansaSimulationPhase> Phases)
	{
		uint64 Hash = FnvOffset;
		AddUInt32(Hash, FHansaSimulationState::CurrentSystemPipelineVersion);
		AddUInt32(Hash, static_cast<uint32>(Phases.Num()));
		for (const EHansaSimulationPhase Phase : Phases)
		{
			AddByte(Hash, static_cast<uint8>(Phase));
		}
		return Hash;
	}

	uint64 FHansaDeterminismDiagnostics::ComputeDomainEventOrderHash(
		const TConstArrayView<FHansaDomainEvent> Events)
	{
		uint64 Hash = FnvOffset;
		AddUInt32(Hash, static_cast<uint32>(Events.Num()));
		for (const FHansaDomainEvent& Event : Events)
		{
			AddByte(Hash, static_cast<uint8>(Event.GetType()));
			AddUInt64(Hash, Event.GetGlobalSequence());
			AddUInt64(Hash, static_cast<uint64>(Event.GetTick().GetValue()));
			AddUInt64(Hash, Event.GetSourceCommandId().GetValue());
			AddUInt32(Hash, Event.GetSourceCommandId().GetGeneration());
			AddUInt64(Hash, Event.GetIssuingHouseId().GetValue());
			AddUInt32(Hash, Event.GetIssuingHouseId().GetGeneration());
			AddUInt64(Hash, Event.GetTestEntityId().GetValue());
			AddUInt32(Hash, Event.GetTestEntityId().GetGeneration());
			AddUInt64(Hash, static_cast<uint64>(Event.GetValue()));
		}
		return Hash;
	}

	FHansaDeterminismComparison FHansaDeterminismDiagnostics::Compare(
		const FHansaDeterminismTrace& Left,
		const FHansaDeterminismTrace& Right)
	{
		if (Left.GetFixtureId() != Right.GetFixtureId() ||
			Left.GetFixtureSchemaVersion() != Right.GetFixtureSchemaVersion() ||
			Left.GetSeed() != Right.GetSeed() || Left.GetDefinitionHash() != Right.GetDefinitionHash())
		{
			FHansaDeterminismComparison Result;
			Result.Kind = EHansaDeterminismDivergenceKind::TraceStructure;
			Result.FirstDivergentTick = Left.GetInitialState().GetTick();
			Result.LeftValue = Left.GetDefinitionHash();
			Result.RightValue = Right.GetDefinitionHash();
			return Result;
		}

		FHansaDeterminismComparison InitialComparison = CompareStateReports(
			Left.GetInitialState(), Right.GetInitialState(), Left.GetInitialState().GetTick());
		if (!InitialComparison.IsEqual())
		{
			return InitialComparison;
		}

		const TConstArrayView<FHansaDeterminismTickRecord> LeftTicks = Left.GetTicks();
		const TConstArrayView<FHansaDeterminismTickRecord> RightTicks = Right.GetTicks();
		const int32 SharedCount = FMath::Min(LeftTicks.Num(), RightTicks.Num());
		for (int32 Index = 0; Index < SharedCount; ++Index)
		{
			if (LeftTicks[Index].ProcessedTick != RightTicks[Index].ProcessedTick)
			{
				FHansaDeterminismComparison Result;
				Result.Kind = EHansaDeterminismDivergenceKind::TraceStructure;
				Result.FirstDivergentTick = LeftTicks[Index].ProcessedTick;
				Result.LeftValue = static_cast<uint64>(LeftTicks[Index].ProcessedTick.GetValue());
				Result.RightValue = static_cast<uint64>(RightTicks[Index].ProcessedTick.GetValue());
				return Result;
			}
			if (LeftTicks[Index].PipelineOrderHash != RightTicks[Index].PipelineOrderHash)
			{
				FHansaDeterminismComparison Result;
				Result.Kind = EHansaDeterminismDivergenceKind::PipelineOrder;
				Result.FirstDivergentTick = LeftTicks[Index].ProcessedTick;
				Result.LeftValue = LeftTicks[Index].PipelineOrderHash;
				Result.RightValue = RightTicks[Index].PipelineOrderHash;
				return Result;
			}
			if (LeftTicks[Index].DomainEventOrderHash != RightTicks[Index].DomainEventOrderHash)
			{
				FHansaDeterminismComparison Result;
				Result.Kind = EHansaDeterminismDivergenceKind::DomainEventOrder;
				Result.FirstDivergentTick = LeftTicks[Index].ProcessedTick;
				Result.LeftValue = LeftTicks[Index].DomainEventOrderHash;
				Result.RightValue = RightTicks[Index].DomainEventOrderHash;
				return Result;
			}
			FHansaDeterminismComparison StateComparison = CompareStateReports(
				LeftTicks[Index].StateAfterTick,
				RightTicks[Index].StateAfterTick,
				LeftTicks[Index].ProcessedTick);
			if (!StateComparison.IsEqual())
			{
				return StateComparison;
			}
		}

		if (LeftTicks.Num() != RightTicks.Num())
		{
			FHansaDeterminismComparison Result;
			Result.Kind = EHansaDeterminismDivergenceKind::TraceStructure;
			Result.FirstDivergentTick = SharedCount > 0
				? LeftTicks[SharedCount - 1].StateAfterTick.GetTick()
				: Left.GetInitialState().GetTick();
			Result.LeftValue = static_cast<uint64>(LeftTicks.Num());
			Result.RightValue = static_cast<uint64>(RightTicks.Num());
			return Result;
		}
		return FHansaDeterminismComparison();
	}
}
