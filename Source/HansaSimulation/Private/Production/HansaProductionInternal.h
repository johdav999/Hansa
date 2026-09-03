#pragma once

#include "Containers/Array.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	enum class EHansaProductionStepEventKind : uint8
	{
		CycleCompleted = 0,
		BlockerChanged
	};

	struct FHansaProductionStepEvent final
	{
		EHansaProductionStepEventKind Kind = EHansaProductionStepEventKind::CycleCompleted;
		FHansaProductionId ProductionId;
		FHansaBuildingId BuildingId;
		FHansaRecipeId RecipeId;
		EHansaProductionBlocker Blocker = EHansaProductionBlocker::None;
		uint64 CompletedCycles = 0;
	};

	class FHansaProductionExecutor final
	{
	public:
		static void AdvanceOneTick(
			TArray<FHansaProductionState>& Productions,
			uint64& NextReservationValue,
			const TArray<FHansaBuildingState>& Buildings,
			FHansaInventoryLedger& InventoryLedger,
			const FHansaEconomicRegistry* EconomicRegistry,
			FHansaSimulationTick Tick,
			TArray<FHansaProductionStepEvent>& OutEvents);
	};
}
