#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"

namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
	class FHansaSimulationReadOnlyAccess;

	enum class EHansaScenarioObjectiveMetric : uint8
	{
		HouseMoneyAtLeast = 0,
		CityPopulationAtLeast,
		ArtisanPopulationAtLeast,
		ActiveSeaRoutesAtLeast,
		ActiveLandRoutesAtLeast,
		CompletedTradeLegsAtLeast,
		CompletedTechnologiesAtLeast,
		ActiveProductionsAtLeast,
		CitySatisfactionAtLeast,
		MarketStockAtLeast,
		MarketPriceAtMost
	};

	struct HANSASIMULATION_API FHansaCompiledScenarioObjective final
	{
		FString StableId;
		FString DisplayName;
		EHansaScenarioObjectiveMetric Metric = EHansaScenarioObjectiveMetric::HouseMoneyAtLeast;
		int64 TargetValue = 1;
		FString CityId;
		FString GoodId;
		FString ProgressUnit;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledVictoryDefinition final
	{
		FString StableId;
		FString DisplayName;
		TArray<FString> ObjectiveIds;
		int32 SustainTicks = 1;
		int32 EndingPriority = 0;
		FString Summary;
		uint64 ContentHash = 0;
	};

	struct HANSASIMULATION_API FHansaCompiledScenarioDefinition final
	{
		FString StableId;
		FString DisplayName;
		FString HomeCityId;
		TArray<FString> VictoryIds;
		int64 InsolvencyThresholdPfennig = 0;
		int32 FailureSustainTicks = 1;
		FString Briefing;
		uint64 ContentHash = 0;
	};

	enum class EHansaScenarioOutcome : uint8
	{
		Active = 0,
		Victory,
		Failure
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaScenarioOutcome Outcome);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaScenarioObjectiveMetric Metric);

	struct HANSASIMULATION_API FHansaScenarioObjectiveProgress final
	{
		FString ObjectiveId;
		FString DisplayName;
		EHansaScenarioObjectiveMetric Metric = EHansaScenarioObjectiveMetric::HouseMoneyAtLeast;
		int64 CurrentValue = 0;
		int64 TargetValue = 0;
		FString ProgressUnit;
		bool bMet = false;
	};

	struct HANSASIMULATION_API FHansaVictoryPathProgress final
	{
		FString VictoryId;
		FString DisplayName;
		FString Summary;
		int32 EndingPriority = 0;
		int32 ConsecutiveSatisfiedTicks = 0;
		int32 RequiredSustainTicks = 1;
		bool bAllObjectivesMet = false;
		bool bVictorious = false;
		TArray<FHansaScenarioObjectiveProgress> Objectives;
	};

	struct HANSASIMULATION_API FHansaScenarioProgress final
	{
		FString ScenarioId;
		FString DisplayName;
		FString Briefing;
		EHansaScenarioOutcome Outcome = EHansaScenarioOutcome::Active;
		FString WinningVictoryId;
		FString FailureReason;
		int32 ConsecutiveFailureTicks = 0;
		int32 RequiredFailureTicks = 1;
		FHansaSimulationTick LastEvaluatedTick;
		TArray<FHansaVictoryPathProgress> VictoryPaths;
	};

	/** Deterministic, read-only scenario observer. Failure wins an exact-tick tie; victories then use priority and stable ID. */
	class HANSASIMULATION_API FHansaScenarioEvaluator final
	{
	public:
		[[nodiscard]] bool Initialize(
			const FHansaEconomicRegistry& Registry,
			const FString& ScenarioId,
			FHansaHouseId HouseId);
		[[nodiscard]] bool Evaluate(
			const FHansaSimulationReadOnlyAccess& ReadOnly,
			const FHansaEconomicRegistry& Registry);
		[[nodiscard]] const FHansaScenarioProgress& GetProgress() const { return Progress; }
		[[nodiscard]] bool IsInitialized() const { return HouseId.IsValid() && !Progress.ScenarioId.IsEmpty(); }

	private:
		friend class FHansaSaveEnvelope;
		FHansaHouseId HouseId;
		FHansaScenarioProgress Progress;
	};
}

