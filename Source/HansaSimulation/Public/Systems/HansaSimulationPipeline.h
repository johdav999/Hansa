#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"

namespace Hansa::Simulation
{
	enum class EHansaSimulationPhase : uint8
	{
		ApplyCommands = 0,
		CalendarAndWorldEvents,
		VehicleMovementAndTransfers,
		WarehousesAndStorage,
		ConstructionAndProduction,
		WorkforceAndNeeds,
		MarketClearing,
		PricesAndHistory,
		FinanceAndContracts,
		ResearchPoliticsAndVictory,
		PublishAndChecksum
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaSimulationPhase Phase);

	struct FHansaSimulationStepInput
	{
		TConstArrayView<FHansaGameplayCommand> Commands;
	};

	/**
	 * Rebuildable, nonauthoritative data. It is deliberately absent from snapshots, projections and hashes.
	 */
	class HANSASIMULATION_API FHansaSimulationTransientCache final
	{
	public:
		[[nodiscard]] FHansaSimulationTick GetPreparedForTick() const { return PreparedForTick; }
		[[nodiscard]] uint64 GetRebuildCount() const { return RebuildCount; }
		[[nodiscard]] int64 GetCachedEntityCount() const { return CachedEntityCount; }
		[[nodiscard]] TConstArrayView<EHansaSimulationPhase> GetLastPhaseOrder() const { return LastPhaseOrder; }

		void Discard();

	private:
		friend class FHansaSimulationPipeline;

		void BeginStep(FHansaSimulationTick Tick, int64 EntityCount);
		void RecordPhase(EHansaSimulationPhase Phase);

		FHansaSimulationTick PreparedForTick;
		uint64 RebuildCount = 0;
		int64 CachedEntityCount = 0;
		TArray<EHansaSimulationPhase> LastPhaseOrder;
	};

	/** Stateless fixed-step executor. Mutation access is private to the normal gameplay command gateway. */
	class HANSASIMULATION_API FHansaSimulationPipeline final
	{
	public:
		static constexpr uint32 CurrentPipelineVersion = FHansaSimulationState::CurrentSystemPipelineVersion;

		[[nodiscard]] static TConstArrayView<EHansaSimulationPhase> GetOrderedPhases();

	private:
		friend class FHansaGameplayCommandGateway;

		static FHansaCommandGatewayResult AdvanceOneTick(
			FHansaSimulationState& State,
			const FHansaSimulationDefinitionContext& Definitions,
			const FHansaSimulationStepInput& Input,
			FHansaSimulationTransientCache& TransientCache);
	};
}
