#pragma once

#include "Containers/Array.h"
#include "Presence/HansaStationOrders.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Math/HansaFixedPoint.h"
#include "Model/HansaIds.h"
#include "Model/HansaSimulationTime.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Simulation
{
	enum class EHansaForeignPresenceStatus : uint8 { Active = 0, Suspended, Revoked };
	enum class EHansaTradeStationStatus : uint8 { Proposed = 0, UnderConstruction, Active, Suspended, Closed };
	/** Orthogonal operating condition. Lifecycle answers what the station is; this answers what it may do and how to recover it. */
	enum class EHansaTradeStationOperationalState : uint8
	{
		Active = 0,
		Underfunded,
		StorageBlocked,
		OrderSuspended,
		RightsSuspended,
		VoluntarilyClosed,
		Revoked
	};
	enum class EHansaPresenceUpgradeStatus : uint8 { None = 0, Requested, Funded };
	enum class EHansaPresenceHistoryKind : uint8 { Contribution = 0, UpgradeRequested, UpgradeFunded, UpgradeCompleted, SpecializationApplied, SpecializationReversed };

	struct HANSASIMULATION_API FHansaForeignPresenceContributions final
	{
		int64 LawfulTradeVolumeMilliUnits = 0;
		int64 CompletedDeliveryCount = 0;
		int64 InvestedPfennig = 0;
		int64 TransactionValuePfennig = 0;
		int64 FulfilledShortageMilliUnits = 0;
		int64 ReliableOperatingTicks = 0;
		int64 SolventOperatingTicks = 0;
	};

	struct HANSASIMULATION_API FHansaPresenceHistoryEntry final
	{
		EHansaPresenceHistoryKind Kind = EHansaPresenceHistoryKind::Contribution;
		FHansaSimulationTick Tick;
		uint64 SourceEventSequence = 0;
		FString StageId;
		int64 QuantityMilliUnits = 0;
		int64 MoneyPfennig = 0;
	};

	struct HANSASIMULATION_API FHansaPresenceUpgradeState final
	{
		EHansaPresenceUpgradeStatus Status = EHansaPresenceUpgradeStatus::None;
		FString TargetStageId;
		FHansaInventoryId FundingInventoryId;
		FHansaSimulationTick RequestedTick;
		FHansaSimulationTick FundedTick;
		FHansaSimulationTick CompletionTick;
		int64 SpentMoneyPfennig = 0;
	};

	/** One authoritative record, canonically keyed by (HouseId, CityId). */
	enum class EHansaCityPrivilegeStatus : uint8 { Active = 0, Suspended, Revoked };
enum class EHansaCityProjectStatus : uint8 { Funded = 0, Completed };
struct HANSASIMULATION_API FHansaCityPrivilegeState final { FString PrivilegeId; FHansaLeasedPlotId GrantedLeaseId; EHansaCityPrivilegeStatus Status=EHansaCityPrivilegeStatus::Active; FHansaSimulationTick GrantedTick; FHansaSimulationTick ExpiryTick; int64 SpentMoneyPfennig=0; };
struct HANSASIMULATION_API FHansaCityProjectState final { FString ProjectId; EHansaCityProjectStatus Status=EHansaCityProjectStatus::Funded; FHansaSimulationTick FundedTick; FHansaSimulationTick CompletionTick; int64 SpentMoneyPfennig=0; bool bSharedEffectApplied=false; };
struct HANSASIMULATION_API FHansaForeignPresenceState final
	{
		FHansaHouseId HouseId;
		FHansaCityDefinitionId CityId;
		FString CurrentStageId;
		TArray<FString> GrantedCapabilityIds;
		FHansaForeignPresenceContributions Contributions;
		EHansaForeignPresenceStatus Status = EHansaForeignPresenceStatus::Active;
		FHansaSimulationTick EstablishedTick;
		FHansaSimulationTick LastUpgradeTick;
		FHansaTradeStationId StationId;
		FHansaLeasedPlotId LeasedPlotId;
		uint64 LastAcceptedContributionEventSequence = 0;
		FHansaPresenceUpgradeState Upgrade;
		TArray<FString> ActiveSpecializationIds;
		int64 SpecializationRevision = 0;
		TArray<FHansaPresenceHistoryEntry> History;
		TArray<FHansaCityPrivilegeState> Privileges;
		TArray<FHansaCityProjectState> CityProjects;
		bool bGovernanceAuthority=false;
		FString GovernanceCharterId;
		FHansaSimulationTick GovernanceGrantedTick;
		int64 AuthorityRevision=0;
	};

	struct HANSASIMULATION_API FHansaPresenceSpecializationProjection final
	{
		FString SpecializationId; FString DisplayName; FString ExclusiveGroupId;
		TArray<FString> GrantedCapabilityIds; int64 InvestmentCostPfennig = 0;
		TArray<FHansaCompiledPresenceUpgradeGoodCost> InvestmentGoods;
		int32 RespecRefundBasisPoints = 0; int64 StorageCapacityBonusMilliUnits = 0;
		int32 AdditionalOrderSlots = 0; int64 StationTransferCapBonusMilliUnits = 0;
		bool bSelected = false; bool bAvailable = false; FString Blocker;
	};
	struct HANSASIMULATION_API FHansaPresenceRequirementProjection final
	{
		FString RequirementId;
		FString Description;
		int64 CurrentValue = 0;
		int64 RequiredValue = 0;
		bool bMet = false;
	};

	struct HANSASIMULATION_API FHansaPresenceStageOptionProjection final
	{
		FString StageId;
		FString DisplayName;
		int32 Ordinal = 0;
		int64 UpgradeCostPfennig = 0;
		TArray<FHansaPresenceRequirementProjection> Requirements;
		bool bProgressRequirementsMet = false;
		bool bFundingAvailable = false;
		bool bAvailable = false;
	};

	struct HANSASIMULATION_API FHansaPresenceCapabilityProjection final
	{
		FString CapabilityId;
		FString DisplayName;
		FString Reason;
		bool bGranted = false;
	};

	struct HANSASIMULATION_API FHansaForeignPresenceProjection final
	{
		FHansaHouseId HouseId;
		FHansaCityDefinitionId CityId;
		FString CurrentStageId;
		FString CurrentStageDisplayName;
		EHansaForeignPresenceStatus Status = EHansaForeignPresenceStatus::Active;
		FHansaForeignPresenceContributions Contributions;
		FHansaSimulationTick EstablishedTick;
		FHansaSimulationTick LastUpgradeTick;
		FHansaTradeStationId StationId;
		FHansaLeasedPlotId LeasedPlotId;
		TArray<FHansaPresenceCapabilityProjection> Capabilities;
		TArray<FHansaPresenceStageOptionProjection> NextStages;
		FHansaPresenceUpgradeState Upgrade;
		int64 SpecializationRevision = 0;
		TArray<FHansaPresenceSpecializationProjection> Specializations;
		TArray<FHansaPresenceHistoryEntry> History;
		TArray<FHansaCityPrivilegeState> Privileges;
		TArray<FHansaCityProjectState> CityProjects;
		bool bGovernanceAuthority=false;
		FString GovernanceCharterId;
		FHansaSimulationTick GovernanceGrantedTick;
		int64 AuthorityRevision=0;
	};
	struct HANSASIMULATION_API FHansaTradeStationSpentGood final { FHansaGoodId GoodId; FHansaQuantity Quantity; };

	struct HANSASIMULATION_API FHansaTradeStationState final
	{
		FHansaTradeStationId Id;
		FHansaHouseId OwnerId;
		FHansaCityDefinitionId CityId;
		FString SiteId;
		FHansaInventoryId InventoryId;
		FHansaFactorId FactorId;
		FHansaLeasedPlotId LeasedPlotId;
		EHansaTradeStationStatus Status = EHansaTradeStationStatus::Proposed;
		EHansaTradeStationOperationalState OperationalState = EHansaTradeStationOperationalState::Active;
		FHansaSimulationTick OperationalStateChangedTick;
		int64 OutstandingUpkeepPfennig = 0;
		FHansaSimulationTick ProposedTick;
		FHansaSimulationTick FundedTick;
		FHansaSimulationTick CompletionTick;
		FHansaSimulationTick CompletedTick;
		int64 UpkeepPfennigPerTick = 0;
		int64 SpentMoneyRaw = 0;
		FHansaInventoryId FundingInventoryId;
		TArray<FHansaTradeStationSpentGood> SpentGoods;
		TArray<FHansaStationOrderState> Orders;
	};

	struct HANSASIMULATION_API FHansaLeasedPlotState final
	{
		FHansaLeasedPlotId Id;
		FHansaTradeStationId StationId;
		FHansaHouseId OwnerId;
		FHansaCityDefinitionId CityId;
		FString SiteId;
		FString PlotCategory;
		FHansaGridCoordinate BoundsMin;
		FHansaGridCoordinate BoundsMax;
		TArray<FString> PermittedBuildingCategories;
		TArray<FHansaBuildingId> OccupyingBuildingIds;
		bool bActive = false;
		bool bOccupied = false;
	};

	enum class EHansaLeasedPlotOverlayKind : uint8 { Owned = 0, LeasedEmpty, FutureAvailable, Prohibited, Warning, Invalid };
	struct HANSASIMULATION_API FHansaLeasedPlotOverlayProjection final
	{
		FHansaLeasedPlotId LeaseId; FHansaHouseId OwnerId; FHansaCityDefinitionId CityId;
		FHansaGridCoordinate BoundsMin; FHansaGridCoordinate BoundsMax;
		EHansaLeasedPlotOverlayKind Kind = EHansaLeasedPlotOverlayKind::Invalid;
		FString PatternKey; FString Label; FString Reason;
	};
	struct HANSASIMULATION_API FHansaForeignConstructionOptionProjection final
	{
		FHansaBuildingTypeId BuildingDefinitionId; FString DisplayName; FString Category;
		bool bPermitted = false; FString Reason;
	};

	struct HANSASIMULATION_API FHansaTradeStationProjection final
	{
		FHansaTradeStationState Station;
		FHansaLeasedPlotState Lease;
		FHansaQuantity StorageUsed;
		FHansaQuantity StorageCapacity;
		FHansaQuantity StorageReserved;
		FString PresentationClassPath;
		FString Blocker;
		FString NextStep;
		FString PreservedAssets;
		FString ContinuingCosts;
		FString RecoveryDiagnostic;
	};
}
