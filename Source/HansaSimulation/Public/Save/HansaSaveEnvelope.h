#pragma once

#include "Commands/HansaGameplayCommand.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Model/HansaSimulationState.h"
#include "Scenario/HansaScenario.h"

namespace Hansa::Simulation
{
	/** Stable session principal to simulation house binding; contains no platform credentials. */
	struct FHansaSavePlayerOwnership
	{
		uint64 PrincipalId = 0;
		FHansaHouseId HouseId;
	};

	struct FHansaSaveVictoryStreak
	{
		FString VictoryId;
		int32 ConsecutiveSatisfiedTicks = 0;
	};

	/** Only observer values that cannot be reconstructed from the current economy. */
	struct FHansaSaveScenarioState
	{
		FHansaHouseId HouseId;
		FString ScenarioId;
		EHansaScenarioOutcome Outcome = EHansaScenarioOutcome::Active;
		FString WinningVictoryId;
		int32 ConsecutiveFailureTicks = 0;
		FHansaSimulationTick LastEvaluatedTick;
		TArray<FHansaSaveVictoryStreak> VictoryStreaks;
	};

	/** Cosmetic player label; never used as gameplay identity. */
	struct FHansaSaveRouteLabel { uint64 RouteValue = 0; FString Label; };

	/** Owning tick-boundary copy. Safe to encode on a worker after capture on the authority thread. */
	struct FHansaSaveSnapshot
	{
		FString BuildVersion;
		FString SavedUtc;
		FString DisplayName;
		TArray<FString> MigrationHistory;
		uint64 NextCommandId = 0;
		uint64 NextBuildingId = 0;
		FHansaSimulationState State;
		TArray<FHansaSavePlayerOwnership> Players;
		TArray<FHansaGameplayCommand> PendingCommands;
		FHansaSaveScenarioState Scenario;
		TArray<FHansaSaveRouteLabel> RouteLabels;
	};

	enum class EHansaSaveError : uint8
	{
		None, InvalidSnapshot, CorruptData, UnsupportedFormat, IncompatibleSimulation,
		IncompatibleContent, IncompatibleScenario, SizeLimitExceeded
	};

	struct FHansaSaveMetadata
	{
		uint32 FormatVersion = 0;
		uint32 SimulationVersion = 0;
		uint32 PipelineVersion = 0;
		uint32 FingerprintVersion = 0;
		uint64 ContentHash = 0;
		uint64 RegistryHash = 0;
		uint64 PlacementTopologyHash = 0;
		FString ScenarioId;
		FString BuildVersion;
		FString SavedUtc;
		FString DisplayName;
		uint64 AuthoritativeHash = 0;
		uint64 CampaignHash = 0;
	};
	struct FHansaSaveResult
	{
		EHansaSaveError Error = EHansaSaveError::None;
		FString Message;
		uint32 SourceFormatVersion = 0;
		TArray<FString> AppliedMigrations;
		uint64 AuthoritativeHash = 0;
		uint64 CampaignHash = 0;
		[[nodiscard]] bool IsSuccess() const { return Error == EHansaSaveError::None; }
		explicit operator bool() const { return IsSuccess(); }
	};

	/** Versioned, bounded, little-endian archive. No filesystem, UObject, editor or UI dependencies. */
	class HANSASIMULATION_API FHansaSaveEnvelope final
	{
	public:
		static constexpr uint32 CurrentFormatVersion = 21;
		static constexpr int32 MaximumBytes = 64 * 1024 * 1024;
		static FHansaSaveResult Encode(const FHansaSaveSnapshot& Snapshot,
			const FHansaSimulationDefinitionContext& Definitions, TArray<uint8>& OutBytes);
		/** Failure leaves OutSnapshot unchanged. Compatibility is checked against the caller's registry. */
		/** Reads integrity-checked display metadata without requiring matching content. */
		static FHansaSaveResult InspectMetadata(TConstArrayView<uint8> Bytes, FHansaSaveMetadata& OutMetadata);
		static FHansaSaveResult Decode(TConstArrayView<uint8> Bytes,
			const FHansaSimulationDefinitionContext& Definitions, FHansaSaveSnapshot& OutSnapshot);
#if WITH_DEV_AUTOMATION_TESTS
		/** Writes the genuine historical field layout for migration fixtures; never used by runtime save slots. */
		static FHansaSaveResult EncodeHistoricalFixtureForTests(const FHansaSaveSnapshot& Snapshot,
			const FHansaSimulationDefinitionContext& Definitions, uint32 FormatVersion, uint32 FingerprintVersion, TArray<uint8>& OutBytes);
#endif
		static FHansaSaveScenarioState CaptureScenario(const FHansaScenarioEvaluator& Evaluator);
		/** Rebuilds labels/targets from definitions without advancing the saved victory streak. */
		static bool RestoreScenario(const FHansaSaveScenarioState& Saved,
			const FHansaSimulationState& State, const FHansaSimulationDefinitionContext& Definitions,
			FHansaScenarioEvaluator& OutEvaluator);
	};
}
