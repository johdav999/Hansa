#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	class FHansaSimulationDefinitionContext;
	class FHansaSimulationState;

	enum class EHansaStateHashSubsystem : uint8
	{
		Contract = 0,
		SimulationMetadata,
		RandomStreams,
		Houses,
		Cities,
		Buildings,
		Vehicles,
		Routes,
		Inventories,
		Productions,
		TestEntities,
		Population,
		Market,
		Placement,
		Logistics,
		Research,
		NotApplicable = 255
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaStateHashSubsystem Subsystem);

	struct FHansaSubsystemStateHash
	{
		EHansaStateHashSubsystem Subsystem = EHansaStateHashSubsystem::Contract;
		uint64 Value = 0;
		uint32 RecordCount = 0;

		friend bool operator==(const FHansaSubsystemStateHash& Left, const FHansaSubsystemStateHash& Right)
		{
			return Left.Subsystem == Right.Subsystem && Left.Value == Right.Value &&
				Left.RecordCount == Right.RecordCount;
		}
	};

	/** Versioned, normalized diagnostic hash report over authoritative state only. */
	class HANSASIMULATION_API FHansaStateHashReport final
	{
	public:
		static constexpr uint32 CurrentHashFormatVersion = 1;
		static constexpr uint32 CurrentNormalizationVersion = 1;

		[[nodiscard]] uint32 GetHashFormatVersion() const { return HashFormatVersion; }
		[[nodiscard]] uint32 GetNormalizationVersion() const { return NormalizationVersion; }
		[[nodiscard]] uint32 GetSystemPipelineVersion() const { return SystemPipelineVersion; }
		[[nodiscard]] FHansaSimulationTick GetTick() const { return Tick; }
		[[nodiscard]] uint64 GetOverallHash() const { return OverallHash; }
		/** Diagnostic only: number of subsystem hashes recomputed instead of served by the state cache. */
		[[nodiscard]] uint32 GetRecomputedSubsystemCount() const { return RecomputedSubsystemCount; }
		[[nodiscard]] TConstArrayView<FHansaSubsystemStateHash> GetSubsystems() const { return Subsystems; }
		[[nodiscard]] const FHansaSubsystemStateHash* Find(EHansaStateHashSubsystem Subsystem) const;
		[[nodiscard]] FString ToCompactDebugString() const;

		friend bool operator==(const FHansaStateHashReport& Left, const FHansaStateHashReport& Right)
		{
			return Left.HashFormatVersion == Right.HashFormatVersion &&
				Left.NormalizationVersion == Right.NormalizationVersion &&
				Left.SystemPipelineVersion == Right.SystemPipelineVersion &&
				Left.Tick == Right.Tick && Left.OverallHash == Right.OverallHash &&
				Left.Subsystems == Right.Subsystems;
		}

	private:
		friend class FHansaStateHasher;

		uint32 HashFormatVersion = CurrentHashFormatVersion;
		uint32 NormalizationVersion = CurrentNormalizationVersion;
		uint32 SystemPipelineVersion = 0;
		FHansaSimulationTick Tick;
		uint64 OverallHash = 0;
		uint32 RecomputedSubsystemCount = 0;
		TArray<FHansaSubsystemStateHash> Subsystems;
	};

	class HANSASIMULATION_API FHansaStateHasher final
	{
	public:
		[[nodiscard]] static FHansaStateHashReport Compute(
			const FHansaSimulationState& State,
			const FHansaSimulationDefinitionContext& Definitions);
        /** Verify old save integrity before the explicit consumption-history migration. */
        static FHansaStateHashReport ComputeLegacyV16(const FHansaSimulationState& State,
            const FHansaSimulationDefinitionContext& Definitions);
        static FHansaStateHashReport ComputeLegacyV17(const FHansaSimulationState& State,
            const FHansaSimulationDefinitionContext& Definitions);
        static FHansaStateHashReport ComputeLegacyV18(const FHansaSimulationState& State,
            const FHansaSimulationDefinitionContext& Definitions);
		static FHansaStateHashReport ComputeLegacyV19(const FHansaSimulationState& State,
			const FHansaSimulationDefinitionContext& Definitions);
    private:
        static FHansaStateHashReport ComputeVersion(const FHansaSimulationState& State,
            const FHansaSimulationDefinitionContext& Definitions, uint32 FingerprintVersion);
	};
}
