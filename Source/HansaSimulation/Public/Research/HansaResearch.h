#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	enum class EHansaResearchBranch : uint8
	{
		Commerce = 0,
		Production,
		Logistics
	};

	enum class EHansaResearchEffectKind : uint8
	{
		UnlockStableId = 0,
		MarketReportAgeReductionTicks,
		TransactionFrictionReductionBasisPoints,
		ProductionThroughputBasisPoints,
		WarehouseHandlingBasisPoints,
		VehicleCapacityBasisPoints,
		ReserveAutomation,
		RouteScheduling
	};

	struct HANSASIMULATION_API FHansaCompiledResearchEffect final
	{
		EHansaResearchEffectKind Kind = EHansaResearchEffectKind::UnlockStableId;
		FString TargetStableId;
		int32 Magnitude = 0;

		friend bool operator==(const FHansaCompiledResearchEffect& Left, const FHansaCompiledResearchEffect& Right) = default;
	};

	struct HANSASIMULATION_API FHansaCompiledTechnologyDefinition final
	{
		FString StableId;
		FString DisplayName;
		EHansaResearchBranch Branch = EHansaResearchBranch::Commerce;
		TArray<FString> PrerequisiteTechnologyIds;
		int32 CostResearchPoints = 0;
		int32 DurationTicks = 1;
		FString UnlockExplanation;
		TArray<FHansaCompiledResearchEffect> Effects;
		uint64 ContentHash = 0;
	};

	enum class EHansaResearchGraphIssue : uint8
	{
		MissingNode = 0,
		Cycle,
		Unreachable,
		InvalidEffectReference,
		MissingBranchRoot,
		InvalidEffectMagnitude,
		IncompatibleEffectTarget,
		UnsupportedEffectKind
	};

	struct HANSASIMULATION_API FHansaResearchGraphDiagnostic final
	{
		EHansaResearchGraphIssue Issue = EHansaResearchGraphIssue::MissingNode;
		FString TechnologyId;
		FString RelatedStableId;
		FString Cause;
		FString Remedy;
	};

	/** Deterministic graph checks shared by content compilation, Authoring Studio and CI. */
	class HANSASIMULATION_API FHansaResearchGraphValidator final
	{
	public:
		[[nodiscard]] static TArray<FHansaResearchGraphDiagnostic> Validate(
			TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies,
			TConstArrayView<FString> DeclaredRootTechnologyIds,
			const TSet<FString>& KnownStableIds);
	};

	struct HANSASIMULATION_API FHansaAppliedResearchEffect final
	{
		FString SourceTechnologyId;
		EHansaResearchEffectKind Kind = EHansaResearchEffectKind::UnlockStableId;
		FString TargetStableId;
		int32 Magnitude = 0;

		friend bool operator==(const FHansaAppliedResearchEffect& Left, const FHansaAppliedResearchEffect& Right) = default;
	};

	struct HANSASIMULATION_API FHansaHouseResearchInitialization final
	{
		FHansaHouseId HouseId;
		int32 AvailableResearchPoints = 0;
		FString ActiveTechnologyId;
		int32 ProgressTicks = 0;
		TArray<FString> CompletedTechnologyIds;
		TArray<FHansaAppliedResearchEffect> AppliedEffects;
	};

	struct HANSASIMULATION_API FHansaHouseResearchState final
	{
		FHansaHouseId HouseId;
		int32 AvailableResearchPoints = 0;
		FString ActiveTechnologyId;
		int32 ProgressTicks = 0;
		TArray<FString> CompletedTechnologyIds;
		TArray<FHansaAppliedResearchEffect> AppliedEffects;

		[[nodiscard]] bool IsCompleted(const FString& TechnologyId) const;
		[[nodiscard]] bool IsUnlocked(const FString& StableId) const;
		[[nodiscard]] int32 GetEffectMagnitude(EHansaResearchEffectKind Kind, const FString& TargetStableId = FString()) const;
	};

	/** Shared deterministic entry point for authoritative research-effect consumers. */
	class HANSASIMULATION_API FHansaResearchEffectResolver final
	{
	public:
		[[nodiscard]] static const FHansaHouseResearchState* FindHouse(TConstArrayView<FHansaHouseResearchState> States, FHansaHouseId HouseId);
		[[nodiscard]] static int32 GetMagnitude(TConstArrayView<FHansaHouseResearchState> States, FHansaHouseId HouseId, EHansaResearchEffectKind Kind, const FString& TargetStableId);
		[[nodiscard]] static int32 GetBasisPoints(TConstArrayView<FHansaHouseResearchState> States, FHansaHouseId HouseId, EHansaResearchEffectKind Kind, const FString& TargetStableId);
		[[nodiscard]] static bool IsEnabled(TConstArrayView<FHansaHouseResearchState> States, FHansaHouseId HouseId, EHansaResearchEffectKind Kind, const FString& TargetStableId);
		[[nodiscard]] static bool IsAuthoredForTarget(TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies, EHansaResearchEffectKind Kind, const FString& TargetStableId);
		[[nodiscard]] static int32 WorkUnitsForTick(int32 BonusBasisPoints, FHansaSimulationTick Tick);
	};

	enum class EHansaResearchQueueError : uint8
	{
		None = 0,
		UnknownHouse,
		UnknownTechnology,
		QueueFull,
		AlreadyCompleted,
		PrerequisiteMissing,
		InsufficientResearchPoints
	};

	struct HANSASIMULATION_API FHansaResearchQueueResult final
	{
		EHansaResearchQueueError Error = EHansaResearchQueueError::None;
		FString MissingPrerequisiteId;

		[[nodiscard]] bool IsSuccess() const { return Error == EHansaResearchQueueError::None; }
		explicit operator bool() const { return IsSuccess(); }
	};

	struct HANSASIMULATION_API FHansaResearchCompletion final
	{
		FHansaHouseId HouseId;
		FString TechnologyId;
		TArray<FHansaAppliedResearchEffect> AppliedEffects;
	};

	/** Authoritative one-slot MVP research executor. Mutation occurs only from the normal command/tick path. */
	class HANSASIMULATION_API FHansaResearchExecutor final
	{
	public:
        /** Read-only eligibility uses exactly the same rules as command execution. */
        [[nodiscard]] static FHansaResearchQueueResult CanQueue(
            const FHansaHouseResearchState& State, const FString& TechnologyId,
            TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies);

        [[nodiscard]] static FHansaResearchQueueResult TryQueue(
			TArray<FHansaHouseResearchState>& States,
			FHansaHouseId HouseId,
			const FString& TechnologyId,
			TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies);

		static void AdvanceOneTick(
			TArray<FHansaHouseResearchState>& States,
			TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies,
			TArray<FHansaResearchCompletion>& OutCompletions);

		[[nodiscard]] static bool RestoreCanonical(
			TArray<FHansaHouseResearchState>& OutStates,
			TConstArrayView<FHansaHouseResearchInitialization> Initializations,
			TConstArrayView<FHansaCompiledTechnologyDefinition> Technologies);
	};
}

