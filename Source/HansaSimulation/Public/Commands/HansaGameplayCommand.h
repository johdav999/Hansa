#pragma once

#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Placement/HansaPlacement.h"
#include "Trade/HansaTrade.h"
#include "Presence/HansaStationOrders.h"

namespace Hansa::Simulation
{
	enum class EHansaCommandOrigin : uint8
	{
		PlayerInput = 0,
		ArtificialIntelligence,
		MultiplayerRpc,
		ControlledAutomation
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaCommandOrigin Origin);

	/** Transport-neutral authority claim. Every origin is subject to the same authoritative validation. */
	struct FHansaCommandAuthorityContext
	{
		FHansaHouseId IssuingHouseId;
		uint64 PrincipalId = 0;
		EHansaCommandOrigin Origin = EHansaCommandOrigin::PlayerInput;
	};

	struct FHansaCommandHeader
	{
		static constexpr uint16 CurrentSchemaVersion = 14;

		FHansaCommandId CommandId;
		FHansaCommandAuthorityContext Authority;
		FHansaSimulationTick RequestedExecutionTick;
		uint64 GlobalSequence = 0;
		uint16 SchemaVersion = CurrentSchemaVersion;
	};

	struct FHansaCreateTestEntityCommand
	{
		FHansaTestEntityId EntityId;
		int64 InitialValue = 0;
	};

	struct FHansaCancelTestEntityCommand
	{
		FHansaTestEntityId EntityId;
	};

	struct FHansaNoOpTestCommand
	{
		int64 CorrelationValue = 0;
	};

	/** Authoritative activation change; stock and output remain consequences of normal simulation systems. */
	struct FHansaSetProductionActiveCommand
	{
		FHansaProductionId ProductionId;
		bool bActive = true;
	};

	/** Normal authoritative build command. Preview and automation submit this same payload. */
	struct FHansaPlaceBuildingCommand
	{
		FHansaBuildingId BuildingId;
		FHansaPlacementSpec Placement;
	};

	struct FHansaCancelConstructionCommand
	{
		FHansaBuildingId BuildingId;
	};

	struct FHansaRemoveBuildingCommand
	{
		FHansaBuildingId BuildingId;
	};

	/** Manual MVP residence progression; the authored upgrade target determines the next tier. */
	struct FHansaUpgradeResidenceCommand
	{
		FHansaBuildingId BuildingId;
	};

	struct FHansaCreateRouteCommand
	{
		FHansaRouteId RouteId;
		FHansaVehicleId VehicleId;
		FHansaRouteDefinitionId RouteDefinitionId;
		TArray<FHansaRouteStop> Stops;
		bool bActivate = false;
	};

	/** Replaces the ordered stop/action plan while an owned route is inactive at a stop. */
	struct FHansaEditRouteCommand
	{
		FHansaRouteId RouteId;
		TArray<FHansaRouteStop> Stops;
	};

	struct FHansaSetRouteActiveCommand
	{
		FHansaRouteId RouteId;
		bool bActive = true;
	};

	struct FHansaCancelRouteCommand
	{
		FHansaRouteId RouteId;
	};

	/** Explicit visiting-market commerce; reviewed values make stale confirmation detectable. */
	struct FHansaSpotTradeCommand
	{
		FHansaVehicleId VehicleId;
		FHansaCityDefinitionId CityId;
		FHansaGoodId GoodId;
		EHansaSpotTradeSide Side = EHansaSpotTradeSide::BuyFromCity;
		FHansaQuantity Quantity;
		int64 ReviewedMarketUpdateTick = -1;
		int64 ReviewedUnitPriceMilliMarks = 0;
	};
	struct FHansaProposeTradeStationCommand
	{
		FHansaTradeStationId StationId; FHansaFactorId FactorId; FHansaLeasedPlotId LeasedPlotId; FHansaInventoryId InventoryId;
		FHansaCityDefinitionId CityId; FString SiteId;
	};
	struct FHansaFundTradeStationCommand { FHansaTradeStationId StationId; FHansaInventoryId FundingInventoryId; };
	struct FHansaCloseTradeStationCommand { FHansaTradeStationId StationId; };
	struct FHansaRequestPresenceUpgradeCommand { FHansaCityDefinitionId CityId; FString TargetStageId; };
	struct FHansaFundPresenceUpgradeCommand { FHansaCityDefinitionId CityId; FString TargetStageId; FHansaInventoryId FundingInventoryId; };
	 enum class EHansaPresenceSpecializationAction : uint8 { Select = 0, Respec };
	 struct FHansaApplyPresenceSpecializationCommand { FHansaCityDefinitionId CityId; FString SpecializationId; FHansaInventoryId FundingInventoryId; EHansaPresenceSpecializationAction Action = EHansaPresenceSpecializationAction::Select; int64 ReviewedRevision = 0; };
	/** Enqueues one stable technology in the issuing house's authoritative MVP research slot. */
	enum class EHansaCityPrivilegeAction : uint8 { Acquire = 0, Revoke };
 struct FHansaManageCityPrivilegeCommand { FHansaCityDefinitionId CityId; FString PrivilegeId; FHansaInventoryId FundingInventoryId; FHansaLeasedPlotId GrantedLeaseId; EHansaCityPrivilegeAction Action=EHansaCityPrivilegeAction::Acquire; int64 ReviewedRevision=0; };
 struct FHansaFundCityProjectCommand { FHansaCityDefinitionId CityId; FString ProjectId; FHansaInventoryId FundingInventoryId; int64 ReviewedRevision=0; };
 struct FHansaTransitionCityAuthorityCommand { FHansaCityDefinitionId CityId; FString CharterId; int64 ReviewedRevision=0; };
struct FHansaQueueResearchCommand
	{
		FString TechnologyId;
	};

	struct FHansaSetHeatingReserveCommand
	{
		FHansaBuildingId MarketBuildingId;
		int32 ReserveDays = 3;
		bool bReleaseProtection = false;
	};

struct FHansaSetProductionModeCommand
{
    FHansaProductionId ProductionId;
    FHansaRecipeId RecipeId;
    bool bFallbackToFresh = false;
};

struct FHansaUpgradeProductionCommand
{
    FHansaProductionId ProductionId;
};

struct FHansaSetHouseholdAvailabilityCommand
{
    FHansaBuildingId MarketBuildingId;
    FHansaGoodId GoodId;
    bool bAvailable = true;
};

    struct FHansaMoveShipCommand
    {
        FHansaVehicleId VehicleId;
        FHansaGridCoordinate Target;
    };
	enum class EHansaGameplayCommandType : uint8
	{
		CreateTestEntity = 0,
		CancelTestEntity,
		NoOpTest,
		SetProductionActive,
		PlaceBuilding,
		CancelConstruction,
		RemoveBuilding,
		UpgradeResidence,
		CreateRoute,
		EditRoute,
		SetRouteActive,
		CancelRoute,
		QueueResearch,
		SetHeatingReserve,
		SetProductionMode,
		UpgradeProduction,
		SetHouseholdAvailability,
        MoveShip,
		SpotTrade,
		ProposeTradeStation,
		FundTradeStation,
		CloseTradeStation,
		ManageStationOrder,
		RequestPresenceUpgrade,
		FundPresenceUpgrade,
		ApplyPresenceSpecialization,
		ManageCityPrivilege,
		FundCityProject,
		TransitionCityAuthority
	};

	HANSASIMULATION_API const TCHAR* LexToString(EHansaGameplayCommandType Type);

	/**
	 * Closed typed command envelope for the current protocol version. Callers cannot provide a trusted fingerprint;
	 * it is derived from the complete header and active payload by deterministic code.
	 */
	class HANSASIMULATION_API FHansaGameplayCommand final
	{
	public:
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaCreateTestEntityCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaCancelTestEntityCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaNoOpTestCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaSetProductionActiveCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaPlaceBuildingCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaCancelConstructionCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaRemoveBuildingCommand& Payload);
		static FHansaGameplayCommand Create(
			const FHansaCommandHeader& Header,
			const FHansaUpgradeResidenceCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaCreateRouteCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaEditRouteCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaSetRouteActiveCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaCancelRouteCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaQueueResearchCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaSpotTradeCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaProposeTradeStationCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaFundTradeStationCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaCloseTradeStationCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaRequestPresenceUpgradeCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaFundPresenceUpgradeCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaApplyPresenceSpecializationCommand& Payload);
		static FHansaGameplayCommand Create(const FHansaCommandHeader&, const FHansaManageCityPrivilegeCommand&);
		static FHansaGameplayCommand Create(const FHansaCommandHeader&, const FHansaFundCityProjectCommand&);
		static FHansaGameplayCommand Create(const FHansaCommandHeader&, const FHansaTransitionCityAuthorityCommand&);

		[[nodiscard]] const FHansaCommandHeader& GetHeader() const { return Header; }
		[[nodiscard]] EHansaGameplayCommandType GetType() const { return Type; }
		[[nodiscard]] const FHansaCreateTestEntityCommand& GetCreateTestEntity() const;
		[[nodiscard]] const FHansaCancelTestEntityCommand& GetCancelTestEntity() const;
		[[nodiscard]] const FHansaNoOpTestCommand& GetNoOpTest() const;
		[[nodiscard]] const FHansaSetProductionActiveCommand& GetSetProductionActive() const;
		[[nodiscard]] const FHansaPlaceBuildingCommand& GetPlaceBuilding() const;
		[[nodiscard]] const FHansaCancelConstructionCommand& GetCancelConstruction() const;
		[[nodiscard]] const FHansaRemoveBuildingCommand& GetRemoveBuilding() const;
		[[nodiscard]] const FHansaUpgradeResidenceCommand& GetUpgradeResidence() const;
		[[nodiscard]] const FHansaCreateRouteCommand& GetCreateRoute() const;
		[[nodiscard]] const FHansaEditRouteCommand& GetEditRoute() const;
		[[nodiscard]] const FHansaSetRouteActiveCommand& GetSetRouteActive() const;
		[[nodiscard]] const FHansaCancelRouteCommand& GetCancelRoute() const;
		[[nodiscard]] const FHansaQueueResearchCommand& GetQueueResearch() const;
		[[nodiscard]] const FHansaSpotTradeCommand& GetSpotTrade() const;
		[[nodiscard]] const FHansaProposeTradeStationCommand& GetProposeTradeStation() const;
		[[nodiscard]] const FHansaFundTradeStationCommand& GetFundTradeStation() const;
		[[nodiscard]] const FHansaCloseTradeStationCommand& GetCloseTradeStation() const;
		[[nodiscard]] const FHansaRequestPresenceUpgradeCommand& GetRequestPresenceUpgrade() const;
		[[nodiscard]] const FHansaFundPresenceUpgradeCommand& GetFundPresenceUpgrade() const;
		[[nodiscard]] const FHansaApplyPresenceSpecializationCommand& GetApplyPresenceSpecialization() const;
		const FHansaManageCityPrivilegeCommand& GetManageCityPrivilege() const;
		const FHansaFundCityProjectCommand& GetFundCityProject() const;
		const FHansaTransitionCityAuthorityCommand& GetTransitionCityAuthority() const;
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaManageStationOrderCommand& Payload);
		const FHansaManageStationOrderCommand& GetManageStationOrder() const;
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaSetHeatingReserveCommand& Payload);
		[[nodiscard]] const FHansaSetHeatingReserveCommand& GetSetHeatingReserve() const;
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaSetProductionModeCommand& Payload);
		[[nodiscard]] const FHansaSetProductionModeCommand& GetSetProductionMode() const;
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaUpgradeProductionCommand& Payload);
		[[nodiscard]] const FHansaUpgradeProductionCommand& GetUpgradeProduction() const;
		static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaSetHouseholdAvailabilityCommand& Payload);
		[[nodiscard]] const FHansaSetHouseholdAvailabilityCommand& GetSetHouseholdAvailability() const;
        static FHansaGameplayCommand Create(const FHansaCommandHeader& Header, const FHansaMoveShipCommand& Payload);
        const FHansaMoveShipCommand& GetMoveShip() const;
		[[nodiscard]] uint64 ComputeStableFingerprint() const;

	private:
		friend class FHansaSaveCodec;
		friend class FHansaSaveEnvelope;
		FHansaCommandHeader Header;
		EHansaGameplayCommandType Type = EHansaGameplayCommandType::NoOpTest;
		FHansaCreateTestEntityCommand CreateTestEntity;
		FHansaCancelTestEntityCommand CancelTestEntity;
		FHansaNoOpTestCommand NoOpTest;
		FHansaSetProductionActiveCommand SetProductionActive;
		FHansaPlaceBuildingCommand PlaceBuilding;
		FHansaCancelConstructionCommand CancelConstruction;
		FHansaRemoveBuildingCommand RemoveBuilding;
		FHansaUpgradeResidenceCommand UpgradeResidence;
		FHansaCreateRouteCommand CreateRoute;
		FHansaEditRouteCommand EditRoute;
		FHansaSetRouteActiveCommand SetRouteActive;
		FHansaCancelRouteCommand CancelRoute;
		FHansaQueueResearchCommand QueueResearch;
		FHansaSpotTradeCommand SpotTrade;
		FHansaProposeTradeStationCommand ProposeTradeStation;
		FHansaFundTradeStationCommand FundTradeStation;
		FHansaCloseTradeStationCommand CloseTradeStation;
		FHansaManageStationOrderCommand ManageStationOrder;
		FHansaRequestPresenceUpgradeCommand RequestPresenceUpgrade;
		FHansaFundPresenceUpgradeCommand FundPresenceUpgrade;
		FHansaApplyPresenceSpecializationCommand ApplyPresenceSpecialization;
		FHansaManageCityPrivilegeCommand ManageCityPrivilege;
		FHansaFundCityProjectCommand FundCityProject;
		FHansaTransitionCityAuthorityCommand TransitionCityAuthority;
		FHansaSetHeatingReserveCommand SetHeatingReserve;
		FHansaSetProductionModeCommand SetProductionMode;
		FHansaUpgradeProductionCommand UpgradeProduction;
		FHansaSetHouseholdAvailabilityCommand SetHouseholdAvailability;
        FHansaMoveShipCommand MoveShip;

	};
}
