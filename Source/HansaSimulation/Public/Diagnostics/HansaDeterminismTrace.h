#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Diagnostics/HansaStateHash.h"
#include "Events/HansaDomainEvent.h"
#include "Systems/HansaSimulationPipeline.h"

namespace Hansa::Simulation
{
	struct FHansaDeterminismTickRecord
	{
		FHansaSimulationTick ProcessedTick;
		uint64 PipelineOrderHash = 0;
		uint64 DomainEventOrderHash = 0;
		FHansaStateHashReport StateAfterTick;
	};

	class HANSASIMULATION_API FHansaDeterminismTrace final
	{
	public:
		static THansaValueResult<FHansaDeterminismTrace> TryCreate(
			FString FixtureId,
			uint32 FixtureSchemaVersion,
			uint64 Seed,
			uint64 DefinitionHash,
			FHansaStateHashReport InitialState,
			TArray<FHansaDeterminismTickRecord> Ticks);

		[[nodiscard]] const FString& GetFixtureId() const { return FixtureId; }
		[[nodiscard]] uint32 GetFixtureSchemaVersion() const { return FixtureSchemaVersion; }
		[[nodiscard]] uint64 GetSeed() const { return Seed; }
		[[nodiscard]] uint64 GetDefinitionHash() const { return DefinitionHash; }
		[[nodiscard]] const FHansaStateHashReport& GetInitialState() const { return InitialState; }
		[[nodiscard]] TConstArrayView<FHansaDeterminismTickRecord> GetTicks() const { return Ticks; }

	private:
		FString FixtureId;
		uint32 FixtureSchemaVersion = 0;
		uint64 Seed = 0;
		uint64 DefinitionHash = 0;
		FHansaStateHashReport InitialState;
		TArray<FHansaDeterminismTickRecord> Ticks;
	};

	enum class EHansaDeterminismDivergenceKind : uint8
	{
		None = 0,
		TraceStructure,
		PipelineOrder,
		DomainEventOrder,
		StateSubsystem
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaDeterminismDivergenceKind Kind);

	class HANSASIMULATION_API FHansaDeterminismComparison final
	{
	public:
		[[nodiscard]] static FHansaDeterminismComparison MakeDivergence(
			EHansaDeterminismDivergenceKind Kind,
			FHansaSimulationTick FirstDivergentTick,
			EHansaStateHashSubsystem Subsystem,
			uint64 LeftValue,
			uint64 RightValue);

		[[nodiscard]] bool IsEqual() const { return Kind == EHansaDeterminismDivergenceKind::None; }
		[[nodiscard]] EHansaDeterminismDivergenceKind GetKind() const { return Kind; }
		[[nodiscard]] FHansaSimulationTick GetFirstDivergentTick() const { return FirstDivergentTick; }
		[[nodiscard]] EHansaStateHashSubsystem GetSubsystem() const { return Subsystem; }
		[[nodiscard]] uint64 GetLeftValue() const { return LeftValue; }
		[[nodiscard]] uint64 GetRightValue() const { return RightValue; }
		[[nodiscard]] FString ToCompactDebugString() const;

	private:
		friend class FHansaDeterminismDiagnostics;

		EHansaDeterminismDivergenceKind Kind = EHansaDeterminismDivergenceKind::None;
		FHansaSimulationTick FirstDivergentTick;
		EHansaStateHashSubsystem Subsystem = EHansaStateHashSubsystem::NotApplicable;
		uint64 LeftValue = 0;
		uint64 RightValue = 0;
	};

	class HANSASIMULATION_API FHansaDeterminismDiagnostics final
	{
	public:
		[[nodiscard]] static uint64 ComputePipelineOrderHash(TConstArrayView<EHansaSimulationPhase> Phases);
		[[nodiscard]] static uint64 ComputeDomainEventOrderHash(TConstArrayView<FHansaDomainEvent> Events);
		[[nodiscard]] static FHansaDeterminismComparison Compare(
			const FHansaDeterminismTrace& Left,
			const FHansaDeterminismTrace& Right);
	};
}
