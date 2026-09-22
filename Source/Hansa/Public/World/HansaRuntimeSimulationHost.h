#pragma once
#include "Population/HansaHeating.h"

#include "CoreMinimal.h"
#include "AI/HansaMerchantAI.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Placement/HansaPlacement.h"
#include "Multiplayer/HansaHouseControl.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Scenario/HansaScenario.h"
#include "Save/HansaSaveEnvelope.h"
#include "UObject/Object.h"
#include "World/HansaLubeckScenarioInitializer.h"

#include "HansaRuntimeSimulationHost.generated.h"

class UWorld;
namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
	struct FHansaCompiledBuildingDefinition;
	struct FHansaLogisticsRoadPathProjection;
}
struct FHansaRuntimeSimulationState;

UENUM(BlueprintType)
enum class EHansaRuntimeSimulationSpeed : uint8
{
	Paused,
	Normal,
	Fast,
	Fastest
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHansaRuntimeSimulationAdvanced, int64);

/**
 * World-owned authoritative runtime for the playable Lübeck slice. UI presenters submit intents and consume
 * projections; only this host owns the mutable simulation state and advances its clock.
 */
UCLASS(NotBlueprintable)
class HANSA_API UHansaRuntimeSimulationHost final : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UHansaRuntimeSimulationHost() override;

	bool InitializeForLubeck(
		UWorld* World,
		FString& OutError,
		EHansaRuntimeScenario Scenario = EHansaRuntimeScenario::LubeckGrainShortage,
		uint64 CampaignSeedOverride = 0,
        bool bEmptyPlayerCity = false);
	bool StartNewGame(FString& OutError);
    Hansa::Simulation::FHansaCommandGatewayResult CancelRoute(Hansa::Simulation::FHansaRouteId RouteId);
	[[nodiscard]] EHansaRuntimeScenario GetScenario() const;
	void SetSpeed(EHansaRuntimeSimulationSpeed NewSpeed);
	[[nodiscard]] EHansaRuntimeSimulationSpeed GetSpeed() const { return Speed; }
	[[nodiscard]] bool IsReady() const;

	/** Frame-time adapter used by the game mode. Runs at most one tick per frame and drops overload catch-up debt. */
	bool AdvanceRealTime(double DeltaSeconds);
    /** Fractional simulation time for read-only visual interpolation, frozen when paused. */
    double GetPresentationTickFraction() const { return FMath::Clamp(TickAccumulator, 0.0, 0.999999); }
	/** Deterministic explicit stepping seam for automation and focused presenter tests. */
	bool AdvanceTicks(int32 TickCount);

	[[nodiscard]] const Hansa::Simulation::FHansaCompiledBuildingDefinition* FindBuildingDefinition(
		const FString& StableId) const;
	[[nodiscard]] const Hansa::Simulation::FHansaPlacementMapInitialization* FindPlacementMap() const;
	[[nodiscard]] Hansa::Simulation::FHansaPlacementValidationResult ValidatePlacement(
		const Hansa::Simulation::FHansaPlacementSpec& Spec) const;
	[[nodiscard]] bool IsOwnedRoadCell(Hansa::Simulation::FHansaGridCoordinate Cell) const;
	[[nodiscard]] Hansa::Simulation::FHansaConstructionCostProjection QueryConstructionCost(
		const FString& BuildingStableId) const;
	[[nodiscard]] bool IsTechnologyCompleted(const FString& TechnologyId) const;
	Hansa::Simulation::FHansaCommandGatewayResult PlaceBuildings(
		TConstArrayView<Hansa::Simulation::FHansaPlacementSpec> Specs);
    /** Creates a new route; a stopped empty Cog may be reassigned atomically with explicit player consent. */
    Hansa::Simulation::FHansaCommandGatewayResult CreateTradeRoute(
        Hansa::Simulation::FHansaVehicleId VehicleId, TConstArrayView<Hansa::Simulation::FHansaRouteStop> Stops,
        const FString& Name, bool bReassignStopped, bool bPreview, uint64& OutRouteValue);
    Hansa::Simulation::FHansaCommandGatewayResult MoveShip(Hansa::Simulation::FHansaVehicleId VehicleId, Hansa::Simulation::FHansaGridCoordinate Target);
    FString GetRouteLabel(uint64 RouteValue) const;
	Hansa::Simulation::FHansaCommandGatewayResult EditRoute(
		Hansa::Simulation::FHansaRouteId RouteId,
		TConstArrayView<Hansa::Simulation::FHansaRouteStop> Stops);
	Hansa::Simulation::FHansaCommandGatewayResult SetRouteActive(
		Hansa::Simulation::FHansaRouteId RouteId,
		bool bActive);
    Hansa::Simulation::FHansaCommandGatewayResult SetProductionMode(Hansa::Simulation::FHansaProductionId Id, Hansa::Simulation::FHansaRecipeId Recipe, bool Fallback);
    Hansa::Simulation::FHansaCommandGatewayResult UpgradeProduction(Hansa::Simulation::FHansaProductionId Id);
    Hansa::Simulation::FHansaCommandGatewayResult PreviewUpgradeProduction(Hansa::Simulation::FHansaProductionId Id) const;
    Hansa::Simulation::FHansaCommandGatewayResult SetHouseholdAvailability(Hansa::Simulation::FHansaBuildingId Market, bool Available);
	Hansa::Simulation::FHansaCommandGatewayResult SetProductionActive(
		Hansa::Simulation::FHansaProductionId ProductionId,
		bool bActive);
	/** Read-only command preflights use the same gateway on an isolated state copy. */
	[[nodiscard]] Hansa::Simulation::FHansaCommandGatewayResult PreviewCancelConstruction(
		Hansa::Simulation::FHansaBuildingId BuildingId) const;
	[[nodiscard]] Hansa::Simulation::FHansaCommandGatewayResult PreviewRemoveBuilding(
		Hansa::Simulation::FHansaBuildingId BuildingId) const;
	[[nodiscard]] Hansa::Simulation::FHansaCommandGatewayResult PreviewUpgradeResidence(
		Hansa::Simulation::FHansaBuildingId BuildingId) const;
	Hansa::Simulation::FHansaCommandGatewayResult CancelConstruction(
		Hansa::Simulation::FHansaBuildingId BuildingId);
	Hansa::Simulation::FHansaCommandGatewayResult RemoveBuilding(
		Hansa::Simulation::FHansaBuildingId BuildingId);
	Hansa::Simulation::FHansaCommandGatewayResult UpgradeResidence(
		Hansa::Simulation::FHansaBuildingId BuildingId);
	Hansa::Simulation::FHansaCommandGatewayResult QueueResearch(const FString& TechnologyId);
	Hansa::Simulation::FHansaCommandGatewayResult ExecuteSpotTrade(const Hansa::Simulation::FHansaSpotTradeCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult ExecuteSpotTradeForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		const Hansa::Simulation::FHansaSpotTradeCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult ProposeTradeStation(Hansa::Simulation::FHansaCityDefinitionId CityId, const FString& SiteId, Hansa::Simulation::FHansaTradeStationId& OutStationId);
	Hansa::Simulation::FHansaCommandGatewayResult ProposeTradeStationForAuthority(const Hansa::Simulation::FHansaCommandAuthorityContext& Authority, Hansa::Simulation::FHansaCityDefinitionId CityId, const FString& SiteId, Hansa::Simulation::FHansaTradeStationId& OutStationId);
	Hansa::Simulation::FHansaCommandGatewayResult FundTradeStation(Hansa::Simulation::FHansaTradeStationId StationId, Hansa::Simulation::FHansaInventoryId FundingInventoryId);
	Hansa::Simulation::FHansaCommandGatewayResult FundTradeStationForAuthority(const Hansa::Simulation::FHansaCommandAuthorityContext& Authority, Hansa::Simulation::FHansaTradeStationId StationId, Hansa::Simulation::FHansaInventoryId FundingInventoryId);
	Hansa::Simulation::FHansaCommandGatewayResult ManageStationOrder(const Hansa::Simulation::FHansaManageStationOrderCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult ManageStationOrderForAuthority(const Hansa::Simulation::FHansaCommandAuthorityContext& Authority, const Hansa::Simulation::FHansaManageStationOrderCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult RequestPresenceUpgrade(const Hansa::Simulation::FHansaRequestPresenceUpgradeCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult RequestPresenceUpgradeForAuthority(const Hansa::Simulation::FHansaCommandAuthorityContext& Authority, const Hansa::Simulation::FHansaRequestPresenceUpgradeCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult FundPresenceUpgrade(const Hansa::Simulation::FHansaFundPresenceUpgradeCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult FundPresenceUpgradeForAuthority(const Hansa::Simulation::FHansaCommandAuthorityContext& Authority, const Hansa::Simulation::FHansaFundPresenceUpgradeCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult ApplyPresenceSpecialization(const Hansa::Simulation::FHansaApplyPresenceSpecializationCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult ApplyPresenceSpecializationForAuthority(const Hansa::Simulation::FHansaCommandAuthorityContext& Authority, const Hansa::Simulation::FHansaApplyPresenceSpecializationCommand& Payload);
	Hansa::Simulation::FHansaCommandGatewayResult CloseTradeStation(Hansa::Simulation::FHansaTradeStationId StationId);
	Hansa::Simulation::FHansaCommandGatewayResult CloseTradeStationForAuthority(const Hansa::Simulation::FHansaCommandAuthorityContext& Authority, Hansa::Simulation::FHansaTradeStationId StationId);

	/** Server-only seams that derive command identity, tick and global ordering after principal authorization. */
	Hansa::Simulation::FHansaCommandGatewayResult PlaceBuildingsForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		TConstArrayView<Hansa::Simulation::FHansaPlacementSpec> Specs);
	Hansa::Simulation::FHansaCommandGatewayResult CancelConstructionForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaBuildingId BuildingId);
	Hansa::Simulation::FHansaCommandGatewayResult RemoveBuildingForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaBuildingId BuildingId);
	Hansa::Simulation::FHansaCommandGatewayResult UpgradeResidenceForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaBuildingId BuildingId);
	Hansa::Simulation::FHansaCommandGatewayResult SetProductionActiveForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaProductionId ProductionId, bool bActive);
	Hansa::Simulation::FHansaCommandGatewayResult SetProductionModeForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaProductionId ProductionId,
		Hansa::Simulation::FHansaRecipeId RecipeId, bool bFallback);
	Hansa::Simulation::FHansaCommandGatewayResult UpgradeProductionForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaProductionId ProductionId);
	Hansa::Simulation::FHansaCommandGatewayResult SetHeatingReserveForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaBuildingId MarketId, int32 Days, bool bReleaseProtection);
	Hansa::Simulation::FHansaCommandGatewayResult SetHouseholdAvailabilityForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaBuildingId MarketId,
		Hansa::Simulation::FHansaGoodId GoodId, bool bAvailable);
	Hansa::Simulation::FHansaCommandGatewayResult CreateTradeRouteForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaVehicleId VehicleId,
		TConstArrayView<Hansa::Simulation::FHansaRouteStop> Stops,
		const FString& Name, bool bReassignStopped, uint64& OutRouteValue);
	Hansa::Simulation::FHansaCommandGatewayResult EditRouteForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaRouteId RouteId,
		TConstArrayView<Hansa::Simulation::FHansaRouteStop> Stops);
	Hansa::Simulation::FHansaCommandGatewayResult SetRouteActiveForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaRouteId RouteId,
		bool bActive);
	Hansa::Simulation::FHansaCommandGatewayResult CancelRouteForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaRouteId RouteId);
	Hansa::Simulation::FHansaCommandGatewayResult MoveShipForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		Hansa::Simulation::FHansaVehicleId VehicleId,
		Hansa::Simulation::FHansaGridCoordinate Target);
	Hansa::Simulation::FHansaCommandGatewayResult QueueResearchForAuthority(
		const Hansa::Simulation::FHansaCommandAuthorityContext& Authority,
		const FString& TechnologyId);

	[[nodiscard]] Hansa::Simulation::FHansaHouseId GetHouseId() const;
	[[nodiscard]] TConstArrayView<Hansa::Simulation::FHansaHouseId> GetHouseIds() const;
	[[nodiscard]] TConstArrayView<FHansaHouseStartOpportunity> GetStartingOpportunities() const;
	bool ClaimHouseForHuman(Hansa::Simulation::FHansaHouseId HouseId,
		Hansa::Simulation::FHansaParticipantId ParticipantId, FString& OutError);
	bool ReleaseHumanHouse(Hansa::Simulation::FHansaParticipantId ParticipantId, FString& OutError);
	[[nodiscard]] bool IsHouseAIControlled(Hansa::Simulation::FHansaHouseId HouseId) const;
	[[nodiscard]] int32 GetAIControlledHouseCount() const;
	[[nodiscard]] Hansa::Simulation::FHansaCityDefinitionId GetCityId() const;
	[[nodiscard]] int32 GetPlacedBuildingCount() const;
	[[nodiscard]] int64 GetSimulationTick() const;
	[[nodiscard]] Hansa::Simulation::FHansaHeatingProjection QueryHeating() const;
	Hansa::Simulation::FHansaCommandGatewayResult SetHeatingReserve(Hansa::Simulation::FHansaBuildingId MarketId, int32 Days, bool bOverride);
	/** Authoritative calendar plus the fractional visual tick; read-only and excluded from deterministic state. */
	[[nodiscard]] bool TryGetPresentationCalendar(
		Hansa::Simulation::FHansaCalendarProjection& OutCalendar,
		double& OutTickFraction,
		uint16& OutMinutesPerTick) const;
	[[nodiscard]] uint64 GetLastProcessedCommandSequence() const;
	[[nodiscard]] FString GetBuildingWorldStatus(int64 BuildingValue) const;
	[[nodiscard]] Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaSimulationProjection> BuildProjection() const;
	[[nodiscard]] Hansa::Simulation::FHansaLogisticsRoadPathProjection QueryLocalRoadPath(
		Hansa::Simulation::FHansaInventoryId SourceInventoryId,
		Hansa::Simulation::FHansaInventoryId DestinationInventoryId) const;
    Hansa::Simulation::FHansaSpotTradeQuoteProjection QuerySpotTradeQuote(Hansa::Simulation::FHansaVehicleId Vehicle, Hansa::Simulation::FHansaCityDefinitionId City, Hansa::Simulation::FHansaGoodId Good, Hansa::Simulation::EHansaSpotTradeSide Side, Hansa::Simulation::FHansaQuantity Quantity) const;
    TOptional<Hansa::Simulation::FHansaKnownMarketPriceProjection> QueryKnownMarketPrice(Hansa::Simulation::FHansaCityDefinitionId City, Hansa::Simulation::FHansaGoodId Good) const;
    TOptional<Hansa::Simulation::FHansaKnownMarketSupplyDemandProjection> QueryKnownMarketSupply(Hansa::Simulation::FHansaCityDefinitionId City, Hansa::Simulation::FHansaGoodId Good) const;
	[[nodiscard]] const Hansa::Simulation::FHansaEconomicRegistry* GetEconomicRegistry() const;
	[[nodiscard]] Hansa::Simulation::FHansaHouseId GetRivalHouseId() const;
	void SetMerchantAIEnabled(bool bEnabled);
	[[nodiscard]] bool IsMerchantAIEnabled() const;
	[[nodiscard]] const Hansa::Simulation::FHansaMerchantAIDecisionTrace* GetLastMerchantAIDecision() const;
	[[nodiscard]] TConstArrayView<Hansa::Simulation::FHansaMerchantAIDecisionTrace> GetMerchantAIDecisionHistory() const;
	[[nodiscard]] TArray<Hansa::Simulation::FHansaMerchantAIExplanationProjection> GetMerchantAIExplanationProjection() const;
	[[nodiscard]] const Hansa::Simulation::FHansaScenarioProgress* GetScenarioProgress() const;
	[[nodiscard]] uint64 GetCampaignSeed() const;
 [[nodiscard]] uint64 GetNextParcelSeed() const;
	[[nodiscard]] TConstArrayView<Hansa::Simulation::FHansaDomainEvent> GetEventHistory() const;
	bool SynchronizeWorldProjection();
	/** Authority-only tick-boundary save seam; slot/filesystem/UI orchestration is separate. */
	Hansa::Simulation::FHansaSaveResult CaptureSaveBytes(TArray<uint8>& OutBytes,
		const FString& DisplayName, const FString& SavedUtc) const;
	/** Read-only compatibility/metadata inspection used by the bounded save-slot service. */
	Hansa::Simulation::FHansaSaveResult InspectSaveBytes(TConstArrayView<uint8> Bytes,
		Hansa::Simulation::FHansaSaveSnapshot& OutSnapshot, int64& OutSimulationTick) const;
	Hansa::Simulation::FHansaSaveResult RestoreSaveBytes(TConstArrayView<uint8> Bytes);

	FHansaRuntimeSimulationAdvanced& OnSimulationAdvanced() { return SimulationAdvanced; }

private:
 bool IsCompoundTerrainBuildable(const Hansa::Simulation::FHansaPlacementSpec& Spec, int32 BatchOffset = 0) const;
	void LogEconomyDiagnostics();
	bool PublishStateChange(TConstArrayView<Hansa::Simulation::FHansaDomainEvent> Events, bool bPreserveNavigationPosition = false);
	double TicksPerSecond() const;

	TSharedPtr<FHansaRuntimeSimulationState> Runtime;
	TWeakObjectPtr<UWorld> BoundWorld;
	EHansaRuntimeSimulationSpeed Speed = EHansaRuntimeSimulationSpeed::Normal;
	double TickAccumulator = 0.0;
	FHansaRuntimeSimulationAdvanced SimulationAdvanced;
};
