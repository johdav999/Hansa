#pragma once

#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Production/HansaProduction.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Simulation
{
	class FHansaConstructionExecutor;

	enum class EHansaDomainEventType : uint8
	{
		TestEntityCreated = 0,
		TestEntityCancelled,
		NoOpCommandAccepted,
		ProductionCycleCompleted,
		ProductionBlockerChanged,
		ProductionActiveChanged,
		BuildingPlaced,
		ConstructionProgressed,
		ConstructionCompleted,
		ConstructionCancelled,
		BuildingRemoved,
		ResidenceUpgraded
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaDomainEventType Type);

	/** Immutable event published only after the enclosing tick transaction succeeds. */
	class HANSASIMULATION_API FHansaDomainEvent final
	{
	public:
		[[nodiscard]] EHansaDomainEventType GetType() const { return Type; }
		[[nodiscard]] uint64 GetGlobalSequence() const { return GlobalSequence; }
		[[nodiscard]] FHansaSimulationTick GetTick() const { return Tick; }
		[[nodiscard]] FHansaCommandId GetSourceCommandId() const { return SourceCommandId; }
		[[nodiscard]] FHansaHouseId GetIssuingHouseId() const { return IssuingHouseId; }
		[[nodiscard]] FHansaTestEntityId GetTestEntityId() const { return TestEntityId; }
		[[nodiscard]] FHansaProductionId GetProductionId() const { return ProductionId; }
		[[nodiscard]] FHansaBuildingId GetBuildingId() const { return BuildingId; }
		[[nodiscard]] const FHansaRecipeId& GetRecipeId() const { return RecipeId; }
		[[nodiscard]] EHansaProductionBlocker GetProductionBlocker() const { return ProductionBlocker; }
		[[nodiscard]] const FHansaPlacementSpec& GetPlacement() const { return Placement; }
		[[nodiscard]] int64 GetValue() const { return Value; }
		[[nodiscard]] FString ToDebugString() const;

	private:
		friend class FHansaConstructionExecutor;
		friend class FHansaSimulationPipeline;

		EHansaDomainEventType Type = EHansaDomainEventType::NoOpCommandAccepted;
		uint64 GlobalSequence = 0;
		FHansaSimulationTick Tick;
		FHansaCommandId SourceCommandId;
		FHansaHouseId IssuingHouseId;
		FHansaTestEntityId TestEntityId;
		FHansaProductionId ProductionId;
		FHansaBuildingId BuildingId;
		FHansaRecipeId RecipeId;
		EHansaProductionBlocker ProductionBlocker = EHansaProductionBlocker::None;
		FHansaPlacementSpec Placement;
		int64 Value = 0;
	};
}
