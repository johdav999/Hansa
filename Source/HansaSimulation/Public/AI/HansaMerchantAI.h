#pragma once

#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"

namespace Hansa::Simulation
{
	enum class EHansaMerchantAIGoal : uint8
	{
		None = 0,
		RecoverShortage,
		OperateTradeRoute,
		Research,
		TradeObjectiveComplete
	};

	enum class EHansaMerchantAIOptionKind : uint8
	{
		ActivateProduction = 0,
		ActivateRoute,
		QueueResearch
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaMerchantAIGoal Goal);
	HANSASIMULATION_API const TCHAR* LexToString(EHansaMerchantAIOptionKind Kind);

	struct HANSASIMULATION_API FHansaMerchantAIObservedFact final
	{
		FString StableFactId;
		FString SubjectStableId;
		EHansaMarketInformationState InformationState = EHansaMarketInformationState::Unknown;
		int64 Value = 0;
		FString Unit;
	};

	struct HANSASIMULATION_API FHansaMerchantAIConsideredOption final
	{
		EHansaMerchantAIOptionKind Kind = EHansaMerchantAIOptionKind::ActivateRoute;
		FString StableOptionId;
		int64 Utility = 0;
		uint64 SeededTieBreak = 0;
		bool bEligible = false;
		FString Reason;
	};

	/** Read-only diagnostic record. It deliberately contains no mutable simulation references. */
	struct HANSASIMULATION_API FHansaMerchantAIDecisionTrace final
	{
		int64 DecisionTick = -1;
		uint64 DecisionOrdinal = 0;
		FString TuningStableId;
		EHansaMerchantAIGoal SelectedGoal = EHansaMerchantAIGoal::None;
		TArray<FHansaMerchantAIObservedFact> KnownFacts;
		TArray<FHansaMerchantAIConsideredOption> ConsideredOptions;
		FString ChosenOptionId;
		FString Reason;
		TOptional<EHansaGameplayCommandType> SubmittedCommandType;
		bool bCommandAccepted = false;
		EHansaCommandGatewayError GatewayError = EHansaCommandGatewayError::None;
	};

	struct HANSASIMULATION_API FHansaMerchantAIDecision final
	{
		TOptional<FHansaGameplayCommand> Command;
		FHansaMerchantAIDecisionTrace Trace;
	};

	/**
	 * Server-side merchant controller. It observes only immutable read models and emits the same closed typed
	 * commands as a player. Decision history is diagnostic controller state, never authoritative economy state.
	 */
	class HANSASIMULATION_API FHansaMerchantAIController final
	{
	public:
		[[nodiscard]] bool IsDue(const FHansaSimulationReadOnlyAccess& View,
			const FHansaCompiledMerchantAITuning& Tuning) const;
		[[nodiscard]] FHansaMerchantAIDecision Evaluate(
			const FHansaSimulationReadOnlyAccess& View,
			const FHansaEconomicRegistry& Registry,
			const FHansaCompiledMerchantAITuning& Tuning,
			FHansaHouseId HouseId,
			uint64 PrincipalId,
			FHansaCommandId CommandId);
		void RecordGatewayOutcome(const FHansaCommandGatewayResult& Result);

		[[nodiscard]] const FHansaMerchantAIDecisionTrace* GetLastDecision() const;
		[[nodiscard]] TConstArrayView<FHansaMerchantAIDecisionTrace> GetDecisionHistory() const { return History; }

	private:
		uint64 DecisionOrdinal = 0;
		TArray<FHansaMerchantAIDecisionTrace> History;
	};
}
