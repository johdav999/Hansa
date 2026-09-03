#pragma once

#include "Containers/Array.h"
#include "Construction/HansaConstruction.h"
#include "Inventory/HansaInventory.h"
#include "Logistics/HansaLocalLogistics.h"
#include "Math/HansaDeterministicRandom.h"
#include "Math/HansaFixedPoint.h"
#include "Market/HansaMarket.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Production/HansaProduction.h"
#include "Population/HansaPopulation.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Simulation
{
	class FHansaSimulationDefinitionContext;
	class FHansaSimulationPipeline;
	class FHansaSimulationReadOnlyAccess;
	class FHansaStateHasher;

	struct FHansaHouseState
	{
		FHansaHouseId Id;
		FHansaMoney Money;
	};

	struct FHansaCityState
	{
		FHansaCityDefinitionId DefinitionId;
		FHansaQuantity AggregateStock;
	};

	struct FHansaBuildingState
	{
		FHansaBuildingId Id;
		FHansaBuildingTypeId DefinitionId;
		FHansaHouseId OwnerId;
		FHansaRate ConstructionProgress;
		EHansaConstructionState ConstructionState = EHansaConstructionState::UnderConstruction;
		FHansaSimulationTick ConstructionStartedTick;
		int32 ConstructionElapsedTicks = 0;
	};

	struct FHansaVehicleState
	{
		FHansaVehicleId Id;
		FHansaVehicleDefinitionId DefinitionId;
		FHansaHouseId OwnerId;
		FHansaQuantity Cargo;
	};

	struct FHansaRouteState
	{
		FHansaRouteId Id;
		FHansaHouseId OwnerId;
		FHansaVehicleId VehicleId;
		FHansaRate Progress;
	};

	/** Minimal lifecycle record used to prove the command/event contract before gameplay feature commands exist. */
	struct FHansaTestEntityState
	{
		FHansaTestEntityId Id;
		FHansaHouseId OwnerId;
		int64 Value = 0;
	};

	/**
	 * Authoritative initialization/restore data. Discovery order is accepted here and canonicalized by TryCreate.
	 */
	struct FHansaSimulationInitialization
	{
		FHansaSimulationClock Clock;
		uint64 CampaignSeed = 0;
		uint64 ProcessedCommandCount = 0;
		uint64 LastProcessedCommandSequence = 0;
		FHansaCommandId LastProcessedCommandId;
		uint64 CommandHistoryFingerprint = 14695981039346656037ULL;
		uint64 PublishedDomainEventCount = 0;
		TArray<FHansaRandomStream> RandomStreams;
		TArray<FHansaHouseState> Houses;
		TArray<FHansaCityState> Cities;
		TArray<FHansaBuildingState> Buildings;
		TArray<FHansaVehicleState> Vehicles;
		TArray<FHansaRouteState> Routes;
		TArray<FHansaTestEntityState> TestEntities;
		FHansaPlacementInitialization Placement;
		TArray<FHansaInventoryInitialization> Inventories;
		int32 InventoryMovementHistoryCapacity = 64;
		TArray<FHansaProductionInitialization> Productions;
		TArray<FHansaPopulationCohortInitialization> PopulationCohorts;
		FHansaMarketSettings MarketSettings;
		TArray<FHansaCityMarketInitialization> Markets;
		FHansaLocalLogisticsSettings LocalLogisticsSettings;
		TArray<FHansaLogisticsRequestInitialization> LocalLogisticsRequests;
	};

	/**
	 * Plain authoritative campaign state. Public callers receive only read-only access/snapshots;
	 * the fixed-step pipeline and its short-lived mutation context own mutation.
	 */
	class HANSASIMULATION_API FHansaSimulationState final
	{
	public:
		static constexpr uint32 DeterminismFingerprintVersion = 13;
		static constexpr uint32 CurrentSystemPipelineVersion = 1;
		static constexpr uint64 EmptyCommandHistoryFingerprint = 14695981039346656037ULL;

		FHansaSimulationState() = default;

		static THansaValueResult<FHansaSimulationState> TryCreate(FHansaSimulationInitialization Initialization);

		[[nodiscard]] bool IsInitialized() const { return bInitialized; }
		[[nodiscard]] FHansaSimulationReadOnlyAccess CreateReadOnlyAccess(
			const FHansaSimulationDefinitionContext& Definitions) const;

	private:
		friend class FHansaSimulationPipeline;
		friend class FHansaSimulationReadOnlyAccess;
		friend class FHansaStateHasher;

		[[nodiscard]] uint64 ComputeDeterminismFingerprint(
			const FHansaSimulationDefinitionContext& Definitions) const;

		bool bInitialized = false;
		FHansaSimulationClock Clock;
		uint64 CampaignSeed = 0;
		uint64 ProcessedCommandCount = 0;
		uint64 LastProcessedCommandSequence = 0;
		FHansaCommandId LastProcessedCommandId;
		uint64 CommandHistoryFingerprint = EmptyCommandHistoryFingerprint;
		uint64 PublishedDomainEventCount = 0;
		TArray<FHansaRandomStream> RandomStreams;
		TArray<FHansaHouseState> Houses;
		TArray<FHansaCityState> Cities;
		TArray<FHansaBuildingState> Buildings;
		TArray<FHansaVehicleState> Vehicles;
		TArray<FHansaRouteState> Routes;
		TArray<FHansaTestEntityState> TestEntities;
		FHansaPlacementState Placement;
		FHansaInventoryLedger InventoryLedger;
		uint64 NextProductionReservationValue = 1;
		TArray<FHansaProductionState> Productions;
		TArray<FHansaPopulationCohortState> PopulationCohorts;
		FHansaMarketSettings MarketSettings;
		TArray<FHansaCityMarketState> Markets;
		FHansaLocalLogisticsSettings LocalLogisticsSettings;
		uint64 NextLogisticsJobValue = 1;
		uint64 NextLogisticsReservationValue = 0x8000000000000000ULL;
		TArray<FHansaLogisticsRequestState> LocalLogisticsRequests;
		TArray<FHansaLogisticsJobState> LocalLogisticsJobs;
	};
}
