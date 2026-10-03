#pragma once

#include "CoreMinimal.h"

#include "UI/HansaTradeEstablishment.h"
#include "UI/HansaTradeConstruction.h"
#include "UI/HansaTradeLedger.h"
#include "UI/HansaTradeRecovery.h"
#include "UI/HansaTradeViewTypes.h"
#include "UI/HansaTradeCityInspector.h"
#include "UI/HansaTradeSchedulePresentation.h"
#include "UI/HansaTradeSpecialization.h"
#include "UI/HansaTradeDecisions.h"
#include "UI/HansaVisitingTrade.h"
#include "HansaMultiplayerTypes.generated.h"

UENUM(BlueprintType)
enum class EHansaClientIntentType : uint8
{
	PlaceBuilding = 0,
	SetRouteActive,
	QueueResearch,
	CancelConstruction,
	RemoveBuilding,
	UpgradeResidence,
	SetProductionActive,
	SetProductionMode,
	UpgradeProduction,
	SetHeatingReserve,
	SetHouseholdAvailability,
	CreateRoute,
	EditRoute,
	CancelRoute,
	MoveShip,
	SpotTrade,
	ProposeTradeStation,
	FundTradeStation,
	CloseTradeStation,
    ManageStationOrder,
	RequestPresenceUpgrade,
	FundPresenceUpgrade,
	ApplyPresenceSpecialization,
 ManageCityPrivilege, FundCityProject, TransitionCityAuthority
};

UENUM(BlueprintType)
enum class EHansaClientCommandRejection : uint8
{
	None = 0,
	AuthorityUnavailable,
	ClientNotRegistered,
	DuplicateCommand,
	CommandOrderInvalid,
	InvalidPayload,
	NotAuthorized,
	StaleProjection,
	GatewayRejected
};

UENUM(BlueprintType)
enum class EHansaClientCommandState : uint8
{
	Pending = 0,
	Accepted,
	Rejected
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientPlacementIntent
{
	GENERATED_BODY()
	UPROPERTY() FString CityId;
	UPROPERTY() FString BuildingDefinitionId;
	UPROPERTY() int32 AnchorX = 0;
	UPROPERTY() int32 AnchorY = 0;
	UPROPERTY() uint8 Rotation = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientRouteActionIntent
{
	GENERATED_BODY()
	/** 0 = load, 1 = unload. Conditions are server-owned and currently always. */
	UPROPERTY() uint8 Kind = 0;
	UPROPERTY() FString GoodId;
	UPROPERTY() int64 QuantityMilliUnits = 0;
	UPROPERTY() int64 MinimumSourceReserveMilliUnits = 0;
	UPROPERTY() int32 CargoSlotIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientRouteStopIntent
{
	GENERATED_BODY()
	UPROPERTY() FString CityId;
	UPROPERTY() TArray<FHansaClientRouteActionIntent> Actions;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientInterest
{
	GENERATED_BODY()

	static constexpr int32 MaximumCityCount = 4;
	static constexpr int32 MaximumHistoryPageSize = 64;

	UPROPERTY()
	TArray<FString> CityIds;
	UPROPERTY() int32 HistoryOffset = 0;
	UPROPERTY() int32 HistoryPageSize = 16;
};

/**
 * Untrusted client intent. The server derives house, principal, command identity,
 * authoritative tick, global order, prices, costs, and all resolved outcomes.
 */
USTRUCT(BlueprintType)
struct HANSA_API FHansaClientCommandIntent
{
	GENERATED_BODY()

	static constexpr int32 CurrentSchemaVersion = 10;
    UPROPERTY() FString ExpectedRoutePlanKey;
	static constexpr int32 MaximumPlacementCount = 256;
	static constexpr int32 MaximumRouteStopCount = 16;
	static constexpr int32 MaximumActionsPerStop = 16;

 UPROPERTY() FString DecisionId;
 UPROPERTY() uint8 DecisionAction=0;
 UPROPERTY() int64 AuthorityRevision=0;
    UPROPERTY() int64 StationOrderId = 0;
    UPROPERTY() uint8 StationOrderAction = 0;
    UPROPERTY() uint8 StationOrderSide = 0;
    UPROPERTY() int64 StationOrderTarget = 0;
    UPROPERTY() int64 StationOrderCap = 1000;
    UPROPERTY() int64 StationOrderBudget = 0;
    UPROPERTY() int64 StationOrderLimitUnitPriceMilliMarks = 0;
    UPROPERTY() int64 StationOrderReviewedMarketUpdateTick = -1;
    UPROPERTY() int64 StationOrderReviewedUnitPriceMilliMarks = 0;
	UPROPERTY()
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int64 ClientSequence = 0;

	UPROPERTY()
	int64 ClientNonce = 0;

	/** Optional optimistic-concurrency check. Zero accepts the current authoritative tick. */
	UPROPERTY()
	int64 ExpectedServerTick = 0;

	UPROPERTY()
	EHansaClientIntentType Type = EHansaClientIntentType::PlaceBuilding;

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString BuildingDefinitionId;

	UPROPERTY()
	int32 AnchorX = 0;

	UPROPERTY()
	int32 AnchorY = 0;

	UPROPERTY()
	uint8 Rotation = 0;

	/** Empty preserves the schema-1 single-placement fields above. */
	UPROPERTY() TArray<FHansaClientPlacementIntent> Placements;
	UPROPERTY() int64 BuildingId = 0;
	UPROPERTY() int64 ProductionId = 0;
	UPROPERTY() int64 VehicleId = 0;

	UPROPERTY()
	int64 RouteId = 0;

	UPROPERTY()
	bool bActive = false;

	UPROPERTY() bool bFallback = false;
	UPROPERTY() bool bAvailable = true;
	UPROPERTY() bool bReleaseProtection = false;
	UPROPERTY() bool bReassignStoppedVehicle = false;
	UPROPERTY() int32 ReserveDays = 0;
	UPROPERTY() FString RecipeId;
	UPROPERTY() FString GoodId;
	UPROPERTY() FString RouteName;
	UPROPERTY() TArray<FHansaClientRouteStopIntent> RouteStops;
	UPROPERTY() int32 TargetX = 0;
	UPROPERTY() int32 TargetY = 0;
	UPROPERTY() int64 QuantityMilliUnits = 0;
	UPROPERTY() FString RecoveryReviewKey;
 UPROPERTY() int64 ReviewedMarketUpdateTick = -1;
	UPROPERTY() int64 ReviewedUnitPriceMilliMarks = 0;
	UPROPERTY() bool bSpotTradeBuy = true;
	UPROPERTY() int64 TradeStationId = 0;
	UPROPERTY() int64 FundingInventoryId = 0;
    UPROPERTY() uint8 ConstructionDeliveryMode=0;
	UPROPERTY() FString TradeStationSiteId;
	UPROPERTY() FString PresenceStageId;
	UPROPERTY() FString PresenceSpecializationId;
	UPROPERTY() uint8 PresenceSpecializationAction = 0;
	UPROPERTY() int64 PresenceSpecializationRevision = 0;

	UPROPERTY()
	FString TechnologyId;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientCommandFeedback
{
	GENERATED_BODY()

	UPROPERTY()
	bool bAccepted = false;

	UPROPERTY()
	EHansaClientCommandState State = EHansaClientCommandState::Pending;

	UPROPERTY()
	int64 ClientSequence = 0;

	UPROPERTY()
	int64 ClientNonce = 0;

	UPROPERTY()
	EHansaClientCommandRejection Rejection = EHansaClientCommandRejection::None;

	UPROPERTY()
	FString GatewayError;

	UPROPERTY()
	FString Message;

	UPROPERTY()
	FString Remedy;

	UPROPERTY()
	int64 ServerTick = 0;

	UPROPERTY()
	int64 AcceptedGlobalSequence = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedPlacement
{
	GENERATED_BODY()

	UPROPERTY()
	int64 BuildingId = 0;

	UPROPERTY()
	int64 OwnerHouseId = 0;

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString BuildingDefinitionId;

	UPROPERTY()
	int32 AnchorX = 0;

	UPROPERTY()
	int32 AnchorY = 0;

	UPROPERTY()
	FString Status;

	UPROPERTY()
	int64 ProgressPartsPerMillion = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedMarket
{
	GENERATED_BODY()

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString GoodId;

	UPROPERTY()
	int64 StockMilliUnits = 0;

	UPROPERTY()
	int64 DesiredReserveMilliUnits = 0;

	UPROPERTY()
	int64 CurrentPriceMilliMarks = 0;

	UPROPERTY()
	int64 ReportAgeTicks = 0;
	UPROPERTY() bool bStale = false;
	UPROPERTY() int64 RecentAveragePriceMilliMarks = 0;
	UPROPERTY() int64 CitizenDemandMilliUnits = 0;
	UPROPERTY() int64 IndustrialDemandMilliUnits = 0;
	UPROPERTY() int64 RecentLocalProductionMilliUnits = 0;
	UPROPERTY() int64 ExpectedIncomingSupplyMilliUnits = 0;
	UPROPERTY() int64 UnmetDemandMilliUnits = 0;
	UPROPERTY() TArray<int64> HistoryPriceMilliMarks;
	UPROPERTY() TArray<int64> HistoryTicks;
	UPROPERTY() int32 HistoryOffset = 0;
	UPROPERTY() int32 HistoryTotalCount = 0;
	UPROPERTY() bool bHistoryHasMore = false;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedRoute
{
 GENERATED_BODY()
 /** Stop instructions and labels are owner-only. Public routes carry no plan. */
 UPROPERTY() FString DefinitionId;
 UPROPERTY() FString Label;
 UPROPERTY() FString PlanKey;
 UPROPERTY() TArray<FHansaClientRouteStopIntent> Stops;

	UPROPERTY()
	int64 RouteId = 0;

	UPROPERTY()
	int64 OwnerHouseId = 0;

	UPROPERTY()
	int64 VehicleId = 0;

	UPROPERTY()
	FString Mode;

	UPROPERTY()
	FString Lifecycle;

	UPROPERTY()
	FString CurrentCityId;

	UPROPERTY()
	int32 RemainingTravelTicks = 0;

	UPROPERTY()
	bool bCargoVisible = false;

	UPROPERTY()
	int64 CargoMilliUnits = 0;
	UPROPERTY() int64 CapacityMilliUnits = 0;
	UPROPERTY() int64 FreeCapacityMilliUnits = 0;
	UPROPERTY() int32 TotalTravelTicks = 0;
	UPROPERTY() int64 ProgressPartsPerMillion = 0;
	UPROPERTY() int64 CompletedLegCount = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedInventoryStock
{
	GENERATED_BODY()
	UPROPERTY() FString GoodId;
	UPROPERTY() int64 QuantityMilliUnits = 0;
	UPROPERTY() int64 ReservedMilliUnits = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedInventory
{
	GENERATED_BODY()
	UPROPERTY() int64 InventoryId = 0;
	UPROPERTY() int64 OwnerHouseId = 0;
	UPROPERTY() FString OwnerKind;
	UPROPERTY() FString CityId;
	UPROPERTY() int64 BuildingId = 0;
	UPROPERTY() int64 VehicleId = 0;
	UPROPERTY() int64 CapacityMilliUnits = 0;
	UPROPERTY() int64 UsedCapacityMilliUnits = 0;
	UPROPERTY() int64 ReservedMilliUnits = 0;
	UPROPERTY() TArray<FHansaReplicatedInventoryStock> Stocks;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedProduction
{
	GENERATED_BODY()
	UPROPERTY() int64 ProductionId = 0;
	UPROPERTY() int64 OwnerHouseId = 0;
	UPROPERTY() int64 BuildingId = 0;
	UPROPERTY() FString CityId;
	UPROPERTY() FString RecipeId;
	UPROPERTY() bool bActive = false;
	UPROPERTY() int32 ProgressTicks = 0;
	UPROPERTY() int32 CycleTicks = 0;
	UPROPERTY() int64 CompletedCycles = 0;
	UPROPERTY() FString Blocker;
	UPROPERTY() FString BlockingGoodId;
	UPROPERTY() int64 BlockingRequiredMilliUnits = 0;
	UPROPERTY() int64 BlockingAvailableMilliUnits = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedPopulationNeed
{
	GENERATED_BODY()
	UPROPERTY() FString NeedId;
	UPROPERTY() int32 SatisfactionBasisPoints = 0;
	UPROPERTY() bool bSatisfied = false;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedPopulationCohort
{
	GENERATED_BODY()
	UPROPERTY() int64 CohortId = 0;
	UPROPERTY() int64 OwnerHouseId = 0;
	UPROPERTY() int64 ResidenceBuildingId = 0;
	UPROPERTY() FString CityId;
	UPROPERTY() FString TierId;
	UPROPERTY() int32 Residents = 0;
	UPROPERTY() int32 ResidenceCapacity = 0;
	UPROPERTY() int32 WorkforceSupply = 0;
	UPROPERTY() int32 SatisfactionBasisPoints = 0;
	UPROPERTY() TArray<FHansaReplicatedPopulationNeed> Needs;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedCitySummary
{
	GENERATED_BODY()
	UPROPERTY() FString CityId;
	UPROPERTY() int32 TotalResidents = 0;
	UPROPERTY() int32 HousingCapacity = 0;
	UPROPERTY() int32 LaborerResidents = 0;
	UPROPERTY() int32 ArtisanResidents = 0;
	UPROPERTY() int32 WorkforceAvailable = 0;
	UPROPERTY() int32 SatisfactionBasisPoints = 0;
	UPROPERTY() int64 StapleReserveMilliDays = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedCargoSlot
{
 GENERATED_BODY()
 UPROPERTY() FString GoodId;
 UPROPERTY() int64 QuantityMilliUnits = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedVehicle
{
	GENERATED_BODY()
	UPROPERTY() int64 VehicleId = 0;
	UPROPERTY() int64 OwnerHouseId = 0;
	UPROPERTY() FString DefinitionId;
	UPROPERTY() FString Mode;
	UPROPERTY() FString CurrentCityId;
	UPROPERTY() bool bPrivateDetailsVisible = false;
	UPROPERTY() TArray<FHansaReplicatedCargoSlot> CargoSlots;
	UPROPERTY() int64 CargoMilliUnits = 0;
	UPROPERTY() int64 CapacityMilliUnits = 0;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedLogisticsJob
{
	GENERATED_BODY()
	UPROPERTY() int64 JobId = 0;
	UPROPERTY() int64 OwnerHouseId = 0;
	UPROPERTY() FString CityId;
	UPROPERTY() FString GoodId;
	UPROPERTY() int64 QuantityMilliUnits = 0;
	UPROPERTY() int64 CargoMilliUnits = 0;
	UPROPERTY() int32 RemainingTravelTicks = 0;
	UPROPERTY() FString Status;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaProjectionRemoval
{
	GENERATED_BODY()
	UPROPERTY() FString Collection;
	UPROPERTY() FString StableId;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedResearch
{
	GENERATED_BODY()

	UPROPERTY()
	int64 HouseId = 0;

	UPROPERTY()
	int32 AvailableResearchPoints = 0;

	UPROPERTY()
	FString ActiveTechnologyId;

	UPROPERTY()
	int32 ProgressTicks = 0;

	UPROPERTY()
	TArray<FString> CompletedTechnologyIds;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedVictoryObjective
{
	GENERATED_BODY()

	UPROPERTY()
	FString VictoryId;

	UPROPERTY()
	FString ObjectiveId;

	UPROPERTY()
	int64 CurrentValue = 0;

	UPROPERTY()
	int64 TargetValue = 0;

	UPROPERTY()
	bool bMet = false;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaReplicatedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	int64 GlobalSequence = 0;

	UPROPERTY()
	int64 Tick = 0;

	UPROPERTY()
	FString Type;

	UPROPERTY()
	int64 IssuingHouseId = 0;

	UPROPERTY()
	int64 BuildingId = 0;

	UPROPERTY()
	int64 RouteId = 0;

	UPROPERTY()
	FString CityId;

	UPROPERTY()
	FString GoodId;

	UPROPERTY()
	FString TechnologyId;
};

/** Only the owning house receives station order terms and execution history. */
USTRUCT()
struct HANSA_API FHansaReplicatedStationOrderExecution {
 GENERATED_BODY()
 UPROPERTY() int64 Tick=-1;
 UPROPERTY() int64 RequestedMilliUnits=0;
 UPROPERTY() int64 AppliedMilliUnits=0;
 UPROPERTY() int64 MoneyDelta=0;
 UPROPERTY() uint8 Outcome=0;
 UPROPERTY() uint8 Blocker=0;
};
USTRUCT()
struct HANSA_API FHansaReplicatedStationOrder {
 GENERATED_BODY()
 UPROPERTY() int64 Id=0;
 UPROPERTY() FString GoodId;
 UPROPERTY() uint8 Side=0;
 UPROPERTY() int64 TargetMilliUnits=0;
 UPROPERTY() int64 CapMilliUnits=1000;
 UPROPERTY() int64 BudgetPfennig=0;
 UPROPERTY() int64 LimitUnitPriceMilliMarks=0;
 UPROPERTY() int64 ReviewedMarketUpdateTick=-1;
 UPROPERTY() int64 ReviewedUnitPriceMilliMarks=0;
 UPROPERTY() int64 SpentPfennig=0;
 UPROPERTY() int64 NextUpdateTick=0;
 UPROPERTY() bool bPaused=false;
 UPROPERTY() bool bCancelled=false;
 UPROPERTY() TArray<FHansaReplicatedStationOrderExecution> History;
};
USTRUCT()
struct HANSA_API FHansaReplicatedStationOrders {
 GENERATED_BODY()
 UPROPERTY() FName City;
 UPROPERTY() int64 StationId=0;
 UPROPERTY() int64 CapacityMilliUnits=0;
 UPROPERTY() int64 MaximumCapMilliUnits=0;
 UPROPERTY() int64 MaximumBudgetPfennig=0;
 UPROPERTY() bool bOperational=false;
 UPROPERTY() TArray<FString> GoodIds;
 UPROPERTY() TArray<FHansaReplicatedStationOrder> Orders;
};

USTRUCT()
struct HANSA_API FHansaReplicatedPresenceRequirement {
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FString Description;
 UPROPERTY() int64 Current=0;
 UPROPERTY() int64 Required=0;
 UPROPERTY() bool bMet=false;
};
USTRUCT()
struct HANSA_API FHansaReplicatedPresence {
 GENERATED_BODY()
 UPROPERTY() FName City;
 UPROPERTY() FString CurrentStage;
 UPROPERTY() FString NextStageId;
 UPROPERTY() FString NextStage;
 UPROPERTY() FString Status;
 UPROPERTY() FString Unlocks;
 UPROPERTY() FString History;
 UPROPERTY() uint8 UpgradeStatus=0;
 UPROPERTY() int64 CompletionTick=0;
 UPROPERTY() FString ConstructionDelivery;
 UPROPERTY() bool bProgressMet=false;
 UPROPERTY() bool bOfficeVisual=false;
 /** Completed office capability; preview art also appears before construction. */
 UPROPERTY() bool bOfficeBuilt=false;
 UPROPERTY() FHansaTradeSpecialization Specialization;
 UPROPERTY() FHansaTradeDecisions Decisions;
 UPROPERTY() TArray<FHansaTradeConstruction> ConstructionReports;
 UPROPERTY() TArray<FHansaReplicatedPresenceRequirement> Requirements;
 UPROPERTY() TArray<FHansaEstablishmentChoice> Sources;
};

/**
 * Purpose-built immutable client read model. It intentionally has no authoritative
 * simulation container, mutable inventory, command queue, RNG, or save state.
 */
/** Fully replaced viewer-scoped trade presentation, including unassigned owned vessels. */
USTRUCT()
struct HANSA_API FHansaReplicatedTradeWorkspace
{
 GENERATED_BODY()
 UPROPERTY() TArray<FHansaTradeMapCityPresentation> Cities;
 UPROPERTY() TArray<FHansaTradeCityInspector> Inspectors;
 UPROPERTY() TArray<FHansaTradeMapRoutePresentation> Routes;
 UPROPERTY() TArray<FHansaTradeDirectoryEntry> RouteDirectory;
 UPROPERTY() TArray<FHansaTradeDirectoryEntry> FleetDirectory;
 UPROPERTY() TArray<FHansaTradeSchedulePresentation> Schedules;
 UPROPERTY() TArray<FString> GoodIds;
};

class UPackageMap;

USTRUCT(BlueprintType)
struct HANSA_API FHansaClientProjectionSnapshot
{
	GENERATED_BODY()
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);


	static constexpr int32 CurrentSchemaVersion = 22;
	static constexpr int32 MaximumCollectionEntries = 2048;

	UPROPERTY()
	int32 SchemaVersion = CurrentSchemaVersion;

	UPROPERTY()
	int64 Revision = 0;

	UPROPERTY()
	int64 ServerTick = 0;

	UPROPERTY()
	bool bFullRefresh = true;

	UPROPERTY()
	int64 StartingEventSequence = 0;

	UPROPERTY()
	int64 LastEventSequence = 0;

	UPROPERTY()
	FString AuthoritativeHash;

	UPROPERTY()
	FString ProjectionDigest;
	UPROPERTY() FString AuthorizedViewDigest;
	UPROPERTY() int64 SerializedBytes = 0;
	UPROPERTY() int64 BuildMicroseconds = 0;

	UPROPERTY()
	int64 OwnerHouseId = 0;

	UPROPERTY()
	int64 OwnerMoneyPfennig = 0;

	UPROPERTY()
	FString ScenarioOutcome;

	UPROPERTY()
	FString WinningVictoryId;

	UPROPERTY()
	TArray<FHansaReplicatedPlacement> Placements;

	UPROPERTY()
	TArray<FHansaReplicatedMarket> Markets;
 UPROPERTY() TArray<FHansaVisitingTradeOffer> VisitingTrade;

	UPROPERTY()
	TArray<FHansaReplicatedRoute> Routes;
 /** Viewer-scoped establishment reviews, replaced in full on each snapshot. */
 UPROPERTY() TArray<FHansaTradeEstablishment> StationEstablishments;
 UPROPERTY() TArray<FHansaReplicatedPresence> Presences;
 UPROPERTY() TArray<FHansaTradeLedger> StationLedgers;
 UPROPERTY() TArray<FHansaTradeRecovery> Recoveries;
 UPROPERTY() FHansaReplicatedTradeWorkspace TradeWorkspace;
 UPROPERTY() bool bTradeWorkspaceIncluded=true;
 UPROPERTY() TArray<FHansaReplicatedStationOrders> StationOrders;
	UPROPERTY() TArray<FHansaReplicatedInventory> Inventories;
	UPROPERTY() TArray<FHansaReplicatedProduction> Productions;
	UPROPERTY() TArray<FHansaReplicatedPopulationCohort> PopulationCohorts;
	UPROPERTY() TArray<FHansaReplicatedCitySummary> CitySummaries;
	UPROPERTY() TArray<FHansaReplicatedVehicle> Vehicles;
	UPROPERTY() TArray<FHansaReplicatedLogisticsJob> LogisticsJobs;

	UPROPERTY()
	FHansaReplicatedResearch Research;
	UPROPERTY() TArray<FHansaReplicatedResearch> AuthorizedResearchReports;

	UPROPERTY()
	TArray<FHansaReplicatedVictoryObjective> VictoryObjectives;

	UPROPERTY()
	TArray<FHansaReplicatedEvent> Events;
	UPROPERTY() TArray<FHansaProjectionRemoval> Removed;
};

template<> struct TStructOpsTypeTraits<FHansaClientProjectionSnapshot> : TStructOpsTypeTraitsBase2<FHansaClientProjectionSnapshot> { enum { WithNetSerializer = true }; };
