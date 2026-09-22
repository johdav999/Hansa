#pragma once
#include "Population/HansaHeating.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Diagnostics/HansaStateHash.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	struct FHansaDeterminismFingerprint
	{
		uint32 Version = FHansaSimulationState::DeterminismFingerprintVersion;
		uint32 SystemPipelineVersion = FHansaSimulationState::CurrentSystemPipelineVersion;
		uint64 Value = 0;

		[[nodiscard]] FString ToDebugString() const;

		friend bool operator==(const FHansaDeterminismFingerprint& Left, const FHansaDeterminismFingerprint& Right)
		{
			return Left.Version == Right.Version &&
				Left.SystemPipelineVersion == Right.SystemPipelineVersion &&
				Left.Value == Right.Value;
		}
	};

	/** Owning immutable copy suitable for asynchronous save, UI, networking or automation work. */
	class HANSASIMULATION_API FHansaSimulationSnapshot final
	{
	public:
		[[nodiscard]] const FHansaSimulationClock& GetClock() const { return Clock; }
		[[nodiscard]] uint64 GetCampaignSeed() const { return CampaignSeed; }
		[[nodiscard]] uint64 GetProcessedCommandCount() const { return ProcessedCommandCount; }
		[[nodiscard]] uint64 GetLastProcessedCommandSequence() const { return LastProcessedCommandSequence; }
		[[nodiscard]] FHansaCommandId GetLastProcessedCommandId() const { return LastProcessedCommandId; }
		[[nodiscard]] uint64 GetCommandHistoryFingerprint() const { return CommandHistoryFingerprint; }
		[[nodiscard]] uint64 GetPublishedDomainEventCount() const { return PublishedDomainEventCount; }
		[[nodiscard]] const FHansaDeterminismFingerprint& GetFingerprint() const { return Fingerprint; }
		[[nodiscard]] TConstArrayView<FHansaRandomStream> GetRandomStreams() const { return RandomStreams; }
		[[nodiscard]] TConstArrayView<FHansaHouseState> GetHouses() const { return Houses; }
		[[nodiscard]] TConstArrayView<FHansaCityState> GetCities() const { return Cities; }
		[[nodiscard]] TConstArrayView<FHansaBuildingState> GetBuildings() const { return Buildings; }
		[[nodiscard]] TConstArrayView<FHansaVehicleState> GetVehicles() const { return Vehicles; }
		[[nodiscard]] TConstArrayView<FHansaRouteState> GetRoutes() const { return Routes; }
		[[nodiscard]] TConstArrayView<FHansaForeignPresenceState> GetForeignPresences() const { return ForeignPresences; }
		[[nodiscard]] TConstArrayView<FHansaTradeStationState> GetTradeStations() const { return TradeStations; }
		[[nodiscard]] TConstArrayView<FHansaLeasedPlotState> GetLeasedPlots() const { return LeasedPlots; }
		[[nodiscard]] TConstArrayView<FHansaHouseResearchState> GetResearch() const { return Research; }
		[[nodiscard]] TConstArrayView<FHansaTestEntityState> GetTestEntities() const { return TestEntities; }
		[[nodiscard]] const FHansaInventorySnapshot& GetInventories() const { return Inventories; }
		[[nodiscard]] const FHansaProductionSnapshot& GetProductions() const { return Productions; }
		[[nodiscard]] const FHansaPopulationSnapshot& GetPopulation() const { return Population; }
		[[nodiscard]] const FHansaMarketSnapshot& GetMarket() const { return Market; }
		[[nodiscard]] const FHansaPlacementState& GetPlacement() const { return Placement; }
		[[nodiscard]] const FHansaLocalLogisticsSnapshot& GetLocalLogistics() const { return LocalLogistics; }

	private:
		friend class FHansaSimulationReadOnlyAccess;

		FHansaSimulationClock Clock;
		uint64 CampaignSeed = 0;
		uint64 ProcessedCommandCount = 0;
		uint64 LastProcessedCommandSequence = 0;
		FHansaCommandId LastProcessedCommandId;
		uint64 CommandHistoryFingerprint = FHansaSimulationState::EmptyCommandHistoryFingerprint;
		uint64 PublishedDomainEventCount = 0;
		FHansaDeterminismFingerprint Fingerprint;
		TArray<FHansaRandomStream> RandomStreams;
		TArray<FHansaHouseState> Houses;
		TArray<FHansaCityState> Cities;
		TArray<FHansaBuildingState> Buildings;
		TArray<FHansaVehicleState> Vehicles;
		TArray<FHansaRouteState> Routes;
		TArray<FHansaForeignPresenceState> ForeignPresences;
		TArray<FHansaTradeStationState> TradeStations;
		TArray<FHansaLeasedPlotState> LeasedPlots;
		TArray<FHansaHouseResearchState> Research;
		TArray<FHansaTestEntityState> TestEntities;
		FHansaInventorySnapshot Inventories;
		FHansaProductionSnapshot Productions;
		FHansaPopulationSnapshot Population;
		FHansaMarketSnapshot Market;
		FHansaPlacementState Placement;
		FHansaLocalLogisticsSnapshot LocalLogistics;
	};

	struct FHansaHouseProjection
	{
		FHansaHouseId Id;
		FHansaMoney Money;
	};

	enum class EHansaBuildingWorldStatus : uint8
	{
		UnderConstruction = 0,
		Ready,
		Blocked
	};

	/** Immutable, presentation-specific join of building, placement and production state. */
	struct HANSASIMULATION_API FHansaBuildingWorldProjection final
	{
        uint8 AdjacentRoadMask = 0;
        bool bMapEdge = false;
		FHansaBuildingId BuildingId;
		FHansaHouseId OwnerId;
		FHansaPlacementSpec Placement;
		TArray<FHansaGridCoordinate> OccupiedCells;
		int32 FootprintWidthCells = 1;
		int32 FootprintHeightCells = 1;
		FHansaRate ConstructionProgress;
		EHansaBuildingWorldStatus Status = EHansaBuildingWorldStatus::UnderConstruction;
		EHansaProductionBlocker ProductionBlocker = EHansaProductionBlocker::None;
		bool bRequiresRoad = false;
		bool bHasRoadAccess = false;
		EHansaLogisticsRoadPathFailure RoadAccessFailure = EHansaLogisticsRoadPathFailure::None;
		bool bHasMarketAccess = false;
		EHansaLogisticsRoadPathFailure MarketAccessFailure = EHansaLogisticsRoadPathFailure::None;
		FName MarketAccessMessageKey;
		FName MarketAccessRemedyKey;
		FHansaBuildingId SelectedMarketBuildingId;
		int32 MarketRoadDistanceCells = 0;
		bool bDeliveryBlocked = false;
		int32 BlockedDeliveryCount = 0;
		EHansaLogisticsRoadPathFailure DeliveryFailure = EHansaLogisticsRoadPathFailure::None;

		friend bool operator==(const FHansaBuildingWorldProjection& Left, const FHansaBuildingWorldProjection& Right)
		{
			return Left.AdjacentRoadMask == Right.AdjacentRoadMask && Left.bMapEdge == Right.bMapEdge &&
                Left.BuildingId == Right.BuildingId &&
				Left.OwnerId == Right.OwnerId &&
				Left.Placement.CityId == Right.Placement.CityId &&
				Left.Placement.BuildingDefinitionId == Right.Placement.BuildingDefinitionId &&
				Left.Placement.Anchor == Right.Placement.Anchor &&
				Left.Placement.Rotation == Right.Placement.Rotation &&
				Left.OccupiedCells == Right.OccupiedCells &&
				Left.FootprintWidthCells == Right.FootprintWidthCells &&
				Left.FootprintHeightCells == Right.FootprintHeightCells &&
				Left.ConstructionProgress == Right.ConstructionProgress &&
				Left.Status == Right.Status &&
				Left.ProductionBlocker == Right.ProductionBlocker &&
				Left.bRequiresRoad == Right.bRequiresRoad &&
				Left.bHasRoadAccess == Right.bHasRoadAccess &&
				Left.RoadAccessFailure == Right.RoadAccessFailure &&
				Left.bHasMarketAccess == Right.bHasMarketAccess &&
				Left.MarketAccessFailure == Right.MarketAccessFailure &&
				Left.MarketAccessMessageKey == Right.MarketAccessMessageKey &&
				Left.MarketAccessRemedyKey == Right.MarketAccessRemedyKey &&
				Left.SelectedMarketBuildingId == Right.SelectedMarketBuildingId &&
				Left.MarketRoadDistanceCells == Right.MarketRoadDistanceCells &&
				Left.bDeliveryBlocked == Right.bDeliveryBlocked &&
				Left.BlockedDeliveryCount == Right.BlockedDeliveryCount &&
				Left.DeliveryFailure == Right.DeliveryFailure;
		}

		friend bool operator!=(const FHansaBuildingWorldProjection& Left, const FHansaBuildingWorldProjection& Right)
		{
			return !(Left == Right);
		}
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaBuildingWorldStatus Status);

	/** Purpose-built copy for UI and automation; it never exposes authoritative containers. */
	class HANSASIMULATION_API FHansaSimulationProjection final
	{
	public:
		[[nodiscard]] const FHansaSimulationClock& GetClock() const { return Clock; }
		[[nodiscard]] const FHansaCalendarProjection& GetCalendar() const { return Calendar; }
		[[nodiscard]] const FHansaDeterminismFingerprint& GetFingerprint() const { return Fingerprint; }
		[[nodiscard]] uint64 GetProcessedCommandCount() const { return ProcessedCommandCount; }
		[[nodiscard]] uint64 GetPublishedDomainEventCount() const { return PublishedDomainEventCount; }
		[[nodiscard]] int32 GetCityCount() const { return CityCount; }
		[[nodiscard]] int32 GetBuildingCount() const { return BuildingCount; }
		[[nodiscard]] int32 GetVehicleCount() const { return VehicleCount; }
		[[nodiscard]] int32 GetRouteCount() const { return RouteCount; }
		[[nodiscard]] int32 GetTestEntityCount() const { return TestEntityCount; }
		[[nodiscard]] int32 GetPlacedBuildingCount() const { return Placements.Num(); }
		[[nodiscard]] TConstArrayView<FHansaHouseProjection> GetHouses() const { return Houses; }
		[[nodiscard]] TConstArrayView<FHansaVehicleProjection> GetVehicles() const { return Vehicles; }
		[[nodiscard]] TConstArrayView<FHansaRouteProjection> GetRoutes() const { return Routes; }
		[[nodiscard]] TConstArrayView<FHansaForeignPresenceProjection> GetForeignPresences() const { return ForeignPresences; }
		[[nodiscard]] TConstArrayView<FHansaTradeStationProjection> GetTradeStations() const { return TradeStations; }
		[[nodiscard]] TConstArrayView<FHansaHouseResearchState> GetResearch() const { return Research; }
		[[nodiscard]] TConstArrayView<FHansaInventoryProjection> GetInventories() const { return Inventories; }
        [[nodiscard]] TConstArrayView<FHansaSpoilageRecord> GetSpoilage() const { return Spoilage; }
		[[nodiscard]] TConstArrayView<FHansaProductionProjection> GetProductions() const { return Productions; }
		[[nodiscard]] TConstArrayView<FHansaPopulationCohortProjection> GetPopulationCohorts() const { return PopulationCohorts; }
		[[nodiscard]] TConstArrayView<FHansaCityPopulationProjection> GetCityPopulations() const { return CityPopulations; }
		[[nodiscard]] const FHansaConsumptionProjection& GetCitizenConsumption() const { return CitizenConsumption; }
		[[nodiscard]] int32 GetTotalResidents() const { return TotalResidents; }
		[[nodiscard]] int32 GetTotalWorkforceSupply() const { return TotalWorkforceSupply; }
		[[nodiscard]] TConstArrayView<FHansaCityMarketProjection> GetMarkets() const { return Markets; }
		[[nodiscard]] TConstArrayView<FHansaRemoteIndustryProjection> GetRemoteIndustries() const { return RemoteIndustries; }
		[[nodiscard]] TConstArrayView<FHansaRegionalShipmentState> GetRegionalShipments() const { return RegionalShipments; }
		[[nodiscard]] TConstArrayView<FHansaMarketReserveProjection> GetMarketReserves() const { return MarketReserves; }
		[[nodiscard]] TConstArrayView<FHansaMarketExplanationProjection> GetMarketExplanations() const { return MarketExplanations; }
		[[nodiscard]] TConstArrayView<FHansaMarketConsumerProjection> GetMarketConsumers() const { return MarketConsumers; }
		[[nodiscard]] TConstArrayView<FHansaMarketProducerProjection> GetMarketProducers() const { return MarketProducers; }
		[[nodiscard]] TConstArrayView<FHansaMarketAlertProjection> GetActiveMarketAlerts() const { return ActiveMarketAlerts; }
		[[nodiscard]] TConstArrayView<FHansaPlacedBuildingRecord> GetPlacements() const { return Placements; }
		[[nodiscard]] TConstArrayView<FHansaBuildingWorldProjection> GetBuildingWorldProjections() const
		{
			return BuildingWorldProjections;
		}
		[[nodiscard]] TConstArrayView<FHansaConstructionProjection> GetConstructions() const { return Constructions; }
		[[nodiscard]] TConstArrayView<FHansaLogisticsRequestProjection> GetLogisticsRequests() const { return LogisticsRequests; }
		[[nodiscard]] TConstArrayView<FHansaLogisticsJobProjection> GetLogisticsJobs() const { return LogisticsJobs; }

	private:
		friend class FHansaSimulationReadOnlyAccess;

		FHansaSimulationClock Clock;
		FHansaCalendarProjection Calendar;
		FHansaDeterminismFingerprint Fingerprint;
		uint64 ProcessedCommandCount = 0;
		uint64 PublishedDomainEventCount = 0;
		int32 CityCount = 0;
		int32 BuildingCount = 0;
		int32 VehicleCount = 0;
		int32 RouteCount = 0;
		int32 TestEntityCount = 0;
		TArray<FHansaHouseProjection> Houses;
		TArray<FHansaVehicleProjection> Vehicles;
		TArray<FHansaRouteProjection> Routes;
		TArray<FHansaForeignPresenceProjection> ForeignPresences;
		TArray<FHansaTradeStationProjection> TradeStations;
		TArray<FHansaHouseResearchState> Research;
		TArray<FHansaInventoryProjection> Inventories;
        TArray<FHansaSpoilageRecord> Spoilage;
		TArray<FHansaProductionProjection> Productions;
		TArray<FHansaPopulationCohortProjection> PopulationCohorts;
		FHansaConsumptionProjection CitizenConsumption;
		TArray<FHansaCityPopulationProjection> CityPopulations;
		int32 TotalResidents = 0;
		int32 TotalWorkforceSupply = 0;
		TArray<FHansaCityMarketProjection> Markets;
		TArray<FHansaRemoteIndustryProjection> RemoteIndustries;
		TArray<FHansaRegionalShipmentState> RegionalShipments;
		TArray<FHansaMarketReserveProjection> MarketReserves;
		TArray<FHansaMarketExplanationProjection> MarketExplanations;
		TArray<FHansaMarketConsumerProjection> MarketConsumers;
		TArray<FHansaMarketProducerProjection> MarketProducers;
		TArray<FHansaMarketAlertProjection> ActiveMarketAlerts;
		TArray<FHansaPlacedBuildingRecord> Placements;
		TArray<FHansaBuildingWorldProjection> BuildingWorldProjections;
		TArray<FHansaConstructionProjection> Constructions;
		TArray<FHansaLogisticsRequestProjection> LogisticsRequests;
		TArray<FHansaLogisticsJobProjection> LogisticsJobs;
	};

	/** Borrowed const-only view over the live state plus its immutable definition context. */
	class HANSASIMULATION_API FHansaSimulationReadOnlyAccess final
	{
	public:
		[[nodiscard]] const FHansaSimulationClock& GetClock() const;
		[[nodiscard]] uint64 GetCampaignSeed() const;
		[[nodiscard]] uint64 GetProcessedCommandCount() const;
		[[nodiscard]] uint64 GetLastProcessedCommandSequence() const;
		[[nodiscard]] FHansaCommandId GetLastProcessedCommandId() const;
		[[nodiscard]] uint64 GetPublishedDomainEventCount() const;
		[[nodiscard]] FHansaDeterminismFingerprint GetFingerprint() const;
		[[nodiscard]] FHansaStateHashReport BuildStateHashReport() const;
		[[nodiscard]] TConstArrayView<FHansaHouseState> GetHouses() const;
		[[nodiscard]] TConstArrayView<FHansaCityState> GetCities() const;
		[[nodiscard]] TConstArrayView<FHansaBuildingState> GetBuildings() const;
		[[nodiscard]] TConstArrayView<FHansaVehicleState> GetVehicles() const;
		[[nodiscard]] TConstArrayView<FHansaRouteState> GetRoutes() const;
		[[nodiscard]] TConstArrayView<FHansaForeignPresenceState> GetForeignPresences() const;
		[[nodiscard]] TOptional<FHansaForeignPresenceProjection> QueryForeignPresence(FHansaHouseId HouseId, FHansaCityDefinitionId CityId) const;
		[[nodiscard]] TArray<FHansaForeignPresenceProjection> BuildForeignPresenceProjection(FHansaHouseId HouseId = FHansaHouseId()) const;
		[[nodiscard]] TConstArrayView<FHansaTradeStationState> GetTradeStations() const;
		[[nodiscard]] TConstArrayView<FHansaLeasedPlotState> GetLeasedPlots() const;
		[[nodiscard]] TOptional<FHansaTradeStationProjection> QueryTradeStation(FHansaTradeStationId StationId) const;
		[[nodiscard]] TArray<FHansaTradeStationProjection> BuildTradeStationProjection(FHansaHouseId HouseId = FHansaHouseId()) const;
		[[nodiscard]] TArray<FHansaLeasedPlotOverlayProjection> BuildLeasedPlotOverlayProjection(FHansaHouseId HouseId = FHansaHouseId(), FHansaCityDefinitionId CityId = FHansaCityDefinitionId()) const;
		[[nodiscard]] TArray<FHansaForeignConstructionOptionProjection> BuildForeignConstructionBrowser(FHansaHouseId HouseId, FHansaCityDefinitionId CityId) const;
		[[nodiscard]] TConstArrayView<FHansaHouseResearchState> GetResearch() const;
		[[nodiscard]] TOptional<FHansaVehicleProjection> QueryVehicle(FHansaVehicleId VehicleId) const;
		[[nodiscard]] TArray<FHansaVehicleProjection> BuildVehicleProjection() const;
		[[nodiscard]] TOptional<FHansaRouteProjection> QueryRoute(FHansaRouteId RouteId) const;
		[[nodiscard]] TArray<FHansaRouteProjection> BuildRouteProjection() const;
		[[nodiscard]] FHansaSpotTradeQuoteProjection QuerySpotTradeQuote(FHansaHouseId HouseId, FHansaVehicleId VehicleId,
			FHansaCityDefinitionId CityId, FHansaGoodId GoodId, EHansaSpotTradeSide Side, FHansaQuantity Quantity) const;
		[[nodiscard]] TConstArrayView<FHansaTestEntityState> GetTestEntities() const;
		[[nodiscard]] const FHansaPlacementState& GetPlacement() const;
		[[nodiscard]] FHansaPlacementValidationResult ValidatePlacement(
			FHansaHouseId IssuingHouseId,
			const FHansaPlacementSpec& Spec) const;
		[[nodiscard]] FHansaConstructionCostProjection QueryConstructionCost(
			FHansaHouseId HouseId,
			FHansaCityDefinitionId CityId,
			FHansaBuildingTypeId BuildingDefinitionId) const;
		[[nodiscard]] TOptional<FHansaConstructionProjection> QueryConstruction(FHansaBuildingId BuildingId) const;
		[[nodiscard]] TArray<FHansaConstructionProjection> BuildConstructionProjection() const;
		[[nodiscard]] FHansaLogisticsRoadPathProjection QueryLogisticsRoadPath(
			FHansaInventoryId SourceInventoryId,
			FHansaInventoryId DestinationInventoryId) const;
		[[nodiscard]] TOptional<FHansaLogisticsRequestProjection> QueryLogisticsRequest(
			FHansaLogisticsRequestId RequestId) const;
		[[nodiscard]] TOptional<FHansaLogisticsJobProjection> QueryLogisticsJob(
			FHansaLogisticsJobId JobId) const;
		[[nodiscard]] TArray<FHansaLogisticsRequestProjection> BuildLogisticsRequestProjection() const;
		[[nodiscard]] TArray<FHansaLogisticsJobProjection> BuildLogisticsJobProjection() const;
		[[nodiscard]] FHansaInventoryReadOnlyAccess GetInventories() const;
		[[nodiscard]] TOptional<FHansaProductionProjection> QueryProduction(FHansaProductionId ProductionId) const;
		[[nodiscard]] TArray<FHansaProductionProjection> BuildProductionProjection() const;
		[[nodiscard]] TOptional<FHansaPopulationCohortProjection> QueryPopulationCohort(FHansaPopulationCohortId CohortId) const;
		[[nodiscard]] TArray<FHansaPopulationCohortProjection> BuildPopulationProjection() const;
		[[nodiscard]] FHansaHeatingProjection QueryHeating(FHansaCityDefinitionId CityId) const;
		[[nodiscard]] TOptional<FHansaCityPopulationProjection> QueryCityPopulation(FHansaCityDefinitionId CityId) const;
		[[nodiscard]] TArray<FHansaCityPopulationProjection> BuildCityPopulationProjection() const;
		[[nodiscard]] TOptional<FHansaCityMarketProjection> QueryMarket(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TArray<FHansaCityMarketProjection> BuildMarketProjection() const;
		[[nodiscard]] TOptional<FString> QueryRegionForCity(FHansaCityDefinitionId CityId) const;
		[[nodiscard]] TArray<FHansaRemoteIndustryProjection> QueryCityIndustries(FHansaCityDefinitionId CityId) const;
		[[nodiscard]] TOptional<FHansaCompiledProductionChainDefinition> QueryProductionChain(const FString& ProductionChainId) const;
		[[nodiscard]] TArray<FHansaRegionalShipmentState> QueryIncomingRegionalShipments(FHansaCityDefinitionId CityId) const;
		[[nodiscard]] TOptional<FHansaMarketPriceProjection> QueryMarketPrice(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TOptional<FHansaMarketReportAgeProjection> QueryMarketReportAge(FHansaCityDefinitionId CityId, FHansaGoodId GoodId, FHansaHouseId ViewingHouseId = FHansaHouseId()) const;
		[[nodiscard]] TOptional<FHansaKnownMarketPriceProjection> QueryKnownMarketPrice(FHansaCityDefinitionId CityId, FHansaGoodId GoodId, FHansaHouseId ViewingHouseId = FHansaHouseId()) const;
		[[nodiscard]] TOptional<FHansaKnownMarketSupplyDemandProjection> QueryKnownMarketSupplyDemand(FHansaCityDefinitionId CityId, FHansaGoodId GoodId, FHansaHouseId ViewingHouseId = FHansaHouseId()) const;
		[[nodiscard]] TOptional<FHansaMarketOpportunityComparisonProjection> CompareMarketOpportunity(
			FHansaCityDefinitionId SourceCityId,
			FHansaCityDefinitionId DestinationCityId,
			FHansaGoodId GoodId,
			FHansaHouseId ViewingHouseId = FHansaHouseId()) const;
		[[nodiscard]] TArray<FHansaMarketPriceHistoryEntry> QueryMarketPriceHistory(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TOptional<FHansaMarketSupplyDemandProjection> QueryMarketSupplyDemand(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TOptional<FHansaMarketReserveProjection> QueryMarketReserveDays(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TOptional<FHansaMarketExplanationProjection> QueryMarketExplanation(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TArray<FHansaMarketConsumerProjection> QueryMarketConsumers(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TArray<FHansaMarketProducerProjection> QueryMarketProducers(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TArray<FHansaMarketAlertProjection> QueryMarketAlerts(FHansaCityDefinitionId CityId, FHansaGoodId GoodId) const;
		[[nodiscard]] TArray<FHansaMarketAlertProjection> BuildActiveMarketAlerts() const;

		[[nodiscard]] FHansaSimulationSnapshot CaptureSnapshot() const;
		[[nodiscard]] THansaValueResult<FHansaSimulationProjection> BuildProjection() const;

	private:
		friend class FHansaSimulationState;

		FHansaSimulationReadOnlyAccess(
			const FHansaSimulationState& InState,
			const FHansaSimulationDefinitionContext& InDefinitions)
			: State(&InState)
			, Definitions(&InDefinitions)
		{
		}

		const FHansaSimulationState* State = nullptr;
		const FHansaSimulationDefinitionContext* Definitions = nullptr;
	};
}
