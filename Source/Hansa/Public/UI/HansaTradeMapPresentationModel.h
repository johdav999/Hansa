#pragma once

#include "CoreMinimal.h"
#include "Network/HansaMultiplayerTypes.h"
#include "UI/HansaTradeViewTypes.h"
#include "Trade/HansaTrade.h"
#include "UI/HansaCargoCellEditor.h"
#include "UI/HansaTradeSchedulePresentation.h"
#include "UI/HansaTradeCityInspector.h"
#include "UI/HansaTradeEstablishment.h"
#include "UObject/Object.h"

#include "Presence/HansaStationOrders.h"
#include "Presence/HansaForeignPresence.h"
#include "UI/HansaTradeLedger.h"
#include "UI/HansaTradeConstruction.h"
#include "HansaTradeMapPresentationModel.generated.h"

class UHansaRuntimeSimulationHost;
namespace Hansa::Simulation { class FHansaEconomicRegistry; class FHansaSimulationProjection; }

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapSnapshot final
{
	GENERATED_BODY()
    FHansaTradeCityInspector CityInspector;
 Hansa::UI::FHansaTradeEstablishment Establishment;
    FString ScheduleKey;
 FString LedgerKey;
    TArray<Hansa::Simulation::FHansaPresenceRequirementProjection> PresenceRequirements;
    FString PresenceRequirementsKey;
    FHansaTradeRecovery Recovery;
    FString RecoveryKey,RecoveryItem,RecoveryFeedback;
    bool bRecoveryReview=false,bRecoveryPending=false;
    FHansaTradeDecisions Decisions;
    FString DecisionsKey,DecisionId,DecisionSource,DecisionFeedback;
    bool bDecisionReview=false,bDecisionPending=false;
    FHansaTradeSpecialization Specialization;
    FString SpecializationKey,SpecializationSourceId;
    FText SpecializationReview;
    bool bSpecializationReview=false;
    bool bSpecializationPending=false;
    TArray<FHansaEstablishmentChoice> PresenceSources;
    FString PresenceSourceId;
    FText PresenceStageSummary, PresenceConsequences, PresenceFundingDetail, PresenceHistory, PresenceReview, PresenceStatus;
    bool bPresenceReview=false;
    bool bPresenceOfficeVisual=false;
    TArray<FHansaTradeDirectoryEntry> Directory;
    bool bFleetView=false;
    int32 DirectoryFilter=0; // All, active, paused, draft, attention, available, presence, selected good.
    int64 SelectedVehicleValue=0;
    FString ActiveSection = TEXT("Route");
    FString ConstructionKey;
    FString WorkspacePage = TEXT("Map");
    bool bHasProjection = false;
    bool bCreating = false;
    bool bShipInTransit = false;
    FVector2D ShipPosition = FVector2D::ZeroVector;
    bool bReview = false;
    bool bDiscardConfirmation = false;
    bool bCommandPending = false;
    bool bAnyCommandPending = false;
    FName ValidationTarget;
    bool bCanCreate = false;
    bool bReassignCog = false;
    int64 CogValue = 0;
    FString DraftName;
    FText CogLabel;
    FText CreatorReview;
    FText Validation;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") TArray<FHansaTradeMapCityPresentation> Cities;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") TArray<FHansaTradeMapRoutePresentation> Routes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") TArray<FHansaTradeMapStopPresentation> Stops;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName PreferredGoodStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName SelectedCityStableId = TEXT("City.Rostock");
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Title;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText EditorStatus;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ReserveRisk;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 SelectedRouteValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int32 SelectedStopIndex = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") EHansaTradeMapModeFilter ModeFilter = EHansaTradeMapModeFilter::All;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") EHansaTradeMapCityFilter CityFilter = EHansaTradeMapCityFilter::All;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FString CitySearchText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int32 MatchingCityCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int32 MatchingRouteCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int32 RouteWindowStart = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCompact = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bDirty = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 TradeStationValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText TradeStationState;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText TradeStationDetail;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StationOrderText;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StationOrderList;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StationOrderListCompact;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bConfirmStationOrderCancel = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 SelectedStationOrderId = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FString StationOrderTargetInput;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FString StationOrderCapInput;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 StationOrderBudgetPfennig = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StationOrderFeedback;
    FString StationOrderEditorKey;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText TradeStationAction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCanTradeStationAction = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText PresenceProgress;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText PresenceUpgradeAction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCanPresenceUpgradeAction = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText PresenceSpecializationComparison;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText PresenceSpecializationAction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText PresenceSpecializationFeedback;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FString SelectedPresenceSpecializationId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCanPresenceSpecializationAction = false;
	friend bool operator==(const FHansaTradeMapSnapshot&, const FHansaTradeMapSnapshot&);
};

struct FHansaStationOrderRowPresentation {
    uint64 Id=0;
    FName GoodId;
    FText Good, Side, Target, Cap, Budget, State, ReportAge, LastResult, Remedy;
};

/** Derived from existing authorized ledgers/orders/schedules; never serialized gameplay state. */
struct FHansaStationOverviewRow {
    FString Id;
    FText Label, Detail;
    int64 Tick=-1;
};
struct FHansaStationOverviewPresentation {
    FHansaTradeLedger Ledger;
    FText Storage, UpkeepState, Trading, Blocker, UpgradeStatus;
    TArray<FHansaStationOverviewRow> Transport, Activity;
    int32 Running=0, Paused=0, Blocked=0, Completed=0;
    float StorageFraction=0;
};

/** Viewer-scoped, display-only order editor data. No economic state lives in widgets. */
struct FHansaStationOrderEditorPresentation {
    FName Good;
    FText GoodLabel, Heading, State, Stock, ModeHint, TargetLabel, TargetHint, RateLabel, RateHint, RateUnit;
    FText ReportHeading, Price, ReportAge, Limit, Summary, Activity, FooterHint, Rights;
    bool bBuy=true, bStale=false, bReportAvailable=false, bPaused=false, bCancelled=false;
    float StockFraction=0;
    FString Key() const;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaTradeMapChanged, const FHansaTradeMapSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaTradeMapFocusRestoreRequested, FName);

/** Event-driven Simple route editor. Drafts are committed only through the typed runtime command gateway. */
UCLASS(BlueprintType)
class HANSA_API UHansaTradeMapPresentationModel final : public UObject
{
	GENERATED_BODY()
public:
	void InitializeDefaults();
    // Same station model and controls, presented as a world-side inspector.
    bool bWorldStationDetail = false;
    bool OpenWorldStation(FName City, int64 StationId);
    TFunction<bool(FName,int64)> StationMapRequested;
    FHansaCargoCellEditor CargoEditor;
    bool bShipDetailOpen = false;
    FString ShipDetailTab = TEXT("Route");
    bool OpenCargoCell(int32 Stop, int32 Slot, bool Load);
    bool CanOpenCargoCell(int32 Stop, int32 Slot, bool Load) const;
    bool SetCargoProduct(const FString& Good);
    bool SetCargoQuantity(int64 MilliUnits, bool Reserve = false);
    bool SetCargoSource(uint8 Kind);
    bool ConfirmCargoCell(bool Remove = false);
    void CancelCargoCell();
    TArray<Hansa::Simulation::FHansaRouteStop> GetSlotDraft() const;
    FText GetShipIdentity() const;
    FName GetShipTown() const;
    TArray<Hansa::Simulation::FHansaCargoSlot> GetShipCargo() const;
    void RefreshCargoChoices();
    bool SetStopTown(int32 Stop, FName Town);
    int64 GetShipCapacity() const;
	void BindRuntime(UHansaRuntimeSimulationHost* RuntimeHost);
	void SetNetworkCommandIntent(TFunction<bool(const FHansaClientCommandIntent&)> InIntent) { NetworkCommandIntent = MoveTemp(InIntent); }
	bool ApplyProjection(const Hansa::Simulation::FHansaSimulationProjection& Projection,
		const Hansa::Simulation::FHansaEconomicRegistry& Registry, bool bDirectoryOnly = false);
	bool Open(FName FocusOrigin = TEXT("HUD.TopStatus.TradeMap"), FName PreferredGood = NAME_None, FName SourceCity = TEXT("City.Lubeck"), bool bBeginRoute = true);
	bool CloseIntent();
	bool CycleModeFilterIntent();
	bool CycleCityFilterIntent();
	bool SetCitySearchIntent(const FString& SearchText);
	bool CycleSelectedGoodIntent();
	bool MoveRouteWindowIntent(int32 Direction);
	bool SelectRouteIntent(int64 RouteValue);
 bool DirectoryIntent(const FString& Action);
 bool SelectFleetIntent(int64 VehicleValue);
 void SetViewerHouse(Hansa::Simulation::FHansaHouseId House) { ViewerHouse=House; }
 void RebuildDirectory();
	bool SelectCityIntent(FName CityId);
    bool SelectSectionIntent(const FString& Section);
    FHansaTradeConstruction GetConstructionPresentation() const;
    bool ConstructionIntent(const FString& Action);
    bool SelectWorkspacePageIntent(const FString& Page);
	bool CycleCityIntent(int32 Direction);
	bool SelectStopIntent(int32 StopIndex);
 FHansaTradeSchedulePresentation GetSchedulePresentation() const;
 FHansaTradeLedger GetLedgerPresentation() const;
 FHansaStationOverviewPresentation GetStationOverviewPresentation() const;
 bool LedgerRelatedIntent(FName Good,bool Order);
 TArray<FHansaTradeLedger> RemoteLedgers;
 bool SelectScheduleRowIntent(const FString& Id);
	bool CycleCargoActionIntent();
	bool AdjustQuantityIntent(int32 DeltaMilliUnits);
	bool AdjustMinimumReserveIntent(int32 DeltaMilliUnits);
	bool MoveStopIntent(int32 Direction);
    TFunction<bool(FName)> VisitRequested;
    TFunction<void(FName)> RemoteInterestRequested;
    TFunction<bool(FName,FName)> MarketRequested;
    TFunction<bool(FName,uint64,FName)> ConstructionRequested;
    bool VisitSelectedStopIntent();
    bool BeginCreateIntent(FName Good = NAME_None, FName SourceCity = TEXT("City.Lubeck"));
    bool DiscardCreateIntent();
    bool KeepDraftIntent();
    void ReceiveCommandFeedback(const FHansaClientCommandFeedback& Feedback);
    bool CanEditStops() const;
    bool AddStopIntent();
    bool RemoveStopIntent();
    bool CycleStopCityIntent();
    bool CycleStopGoodIntent();
    bool CycleCogIntent();
    bool SetRouteNameIntent(const FString& Name);
    bool ReviewCreateIntent();
    bool EditCreateIntent();
    bool CreateAndActivateIntent();
	bool CommitIntent();
    bool DiscardRouteEditsIntent();
	bool ToggleActiveIntent();
    bool TradeStationActionIntent();
 bool EstablishmentIntent(const FString& Action);
 void ApplyRemoteEstablishment(const FHansaClientProjectionSnapshot& Projection);
 bool IsRemoteView() const { return bRemoteEstablishment; }
 bool IsAnyTradeCommandPending() const;
 FText RemoteActionReason(const FString& SemanticId) const;
 bool SelectEstablishmentSite(const FString& Id);
 bool SelectEstablishmentSource(const FString& Id);
 bool CanEstablishmentIntent(const FString& Action) const;
	bool PresenceUpgradeActionIntent();
    bool IsAutomaticMerchantOfficeUpgrade() const;
    const FText& GetPresenceActionFeedback() const { return PresenceFeedback; }
    bool PresenceSourceIntent();
    bool SelectPresenceSource(const FString& SourceId);
    bool PresenceCancelReviewIntent();
	bool DecisionIntent(const FString& Action);
    bool CanDecisionIntent(const FString& Action) const;
	bool PresenceSpecializationIntent(const FString& Action);
	bool CanPresenceSpecializationIntent(const FString& Action) const;
    bool StationOrderIntent(const FString& Action);
    bool SetStationOrderNumber(const FString& Field, const FString& Value);
    bool SelectStationOrder(uint64 Id);
    bool SelectStationOrderGood(FName Good);
    const TArray<Hansa::Simulation::FHansaGoodId>& GetStationOrderGoods() const { return OrderGoods; }
    const TArray<Hansa::Simulation::FHansaStationOrderState>& GetStationOrdersForView() const { return StationOrders; }
    const TArray<FHansaStationOrderRowPresentation>& GetStationOrderRows() const { return StationOrderRows; }
    const FHansaStationOrderEditorPresentation& GetStationOrderEditor() const { return StationOrderEditor; }
    void RefreshStationOrderEditor();
    FText GetOrderGoodLabel(Hansa::Simulation::FHansaGoodId Good) const;
    bool CanStationOrderAction(const FString& Action) const;
    void RefreshStationOrderText();
    bool CancelRouteIntent();
	void SetCompact(bool bCompact);
	void SetFocusedSemanticId(FName SemanticId);
	const FHansaTradeMapCityPresentation* GetSelectedCityPresentation() const { return AllCities.FindByPredicate([this](const auto& C){return C.StableId==Snapshot.SelectedCityStableId;}); }
	const FHansaTradeMapRoutePresentation* GetSelectedRoutePresentation() const { return FindSelectedRoute(); }
	[[nodiscard]] const FHansaTradeMapSnapshot& GetSnapshot() const { return Snapshot; }
	/** Public city identities independent of trade-directory search and filters. */
	TConstArrayView<FHansaTradeMapCityPresentation> GetMapCities() const { return AllCities; }
	[[nodiscard]] const TArray<Hansa::Simulation::FHansaRouteStop>& GetDraftStops() const { return DraftStops; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaTradeMapChanged& OnChanged() { return Changed; }
	FHansaTradeMapFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
private:
    FHansaReplicatedTradeWorkspace RemoteWorkspace;
    TArray<FHansaReplicatedRoute> RemoteRoutes;
    TArray<FHansaReplicatedVehicle> RemoteVehicles;
    int64 RemoteRevision=0;
    FString RemoteDraftPlanKey;
    bool bOrderPending=false;
    int64 OrderSequence=0,OrderNonce=0;
    bool ReceiveOrderFeedback(const FHansaClientCommandFeedback& Feedback);
    void RefreshRemoteTradeSelection();
 void RefreshRemoteCityReports();
    void RebuildRemoteDirectory();
    void ResetRemoteOwnerState();
	void RebuildStops(bool bDraftChanged = false);
public:
    void RefreshRecovery();
    bool RecoveryIntent(const FString& Action);
    bool CanRecoveryIntent(const FString& Action) const;
    bool ReceiveRecoveryFeedback(const FHansaClientCommandFeedback& Feedback);
    bool OpenRecovery(FName City,int64 Station,FName Origin=NAME_None);
    TArray<FHansaTradeRecovery> Recoveries;
private:
    int64 RecoveryStation=0,RecoverySequence=0,RecoveryNonce=0;
    FString RecoveryReviewedKey;
    void RefreshDecisions();
    FString DecisionSignature() const;
    FString DecisionReviewedKey;
    int64 DecisionSequence=0,DecisionNonce=0;
    bool ReceiveDecisionFeedback(const FHansaClientCommandFeedback& F);
    void RefreshSpecialization();
    FString SpecializationReviewSignature() const;
    FString SpecializationReviewedKey;
    int64 SpecializationSequence=0,SpecializationNonce=0;
    bool ReceiveSpecializationFeedback(const FHansaClientCommandFeedback& Feedback);
    FString ReviewedVoyage;
    int64 PendingSequence=0, PendingNonce=0;
    bool bPendingCreate=false;
    bool bPendingRouteEdit=false;
    FString CreatorReviewKey() const;
	void RebuildFilteredProjection();
    void UpdateCreatorReview();
	void PublishIfChanged(const FHansaTradeMapSnapshot& Previous);
	const FHansaTradeMapRoutePresentation* FindSelectedRoute() const;
	UPROPERTY(VisibleAnywhere, Category="Hansa|UI|Trade") FHansaTradeMapSnapshot Snapshot;
    Hansa::Simulation::FHansaStationOrderTerms OrderDraft;
    TArray<Hansa::Simulation::FHansaStationOrderState> StationOrders;
    TArray<FHansaStationOrderRowPresentation> StationOrderRows;
    FHansaStationOrderEditorPresentation StationOrderEditor;
    TArray<Hansa::Simulation::FHansaGoodId> OrderGoods;
    uint64 SelectedStationOrder = 0;
    int64 StationOrderCapacity = 0;
    bool bStationOrdersWritable=false;
    int64 StationOrderMaxCap = 50000;
    int64 StationOrderMaxBudget = 1000000;
	FString PresenceUpgradeStageId;
    FString SelectedPresenceSource, PresenceReviewKey;
    FText PresenceFeedback;
    bool bPresencePending=false;
    int64 PresencePendingSequence=0,PresencePendingNonce=0;
    bool ReceivePresenceFeedback(const FHansaClientCommandFeedback& Feedback);
    FString PresenceReviewSignature() const;
    void ConfigureMerchantOfficeUpgrade(bool ProgressMet);
	FString SelectedStationSiteId;
 TArray<FHansaTradeEstablishment> RemoteEstablishments;
 TArray<FHansaReplicatedPresence> RemotePresences;
 void RefreshRemotePresence();
 TArray<FHansaReplicatedStationOrders> RemoteStationOrders;
 TArray<FHansaReplicatedMarket> RemoteMarkets;
 void RefreshRemoteStationOrders();
 bool bRemoteEstablishment=false;
 FString SelectedFundingSource, EstablishmentReviewKey;
 bool bConstructionPriorityReview=false;
 FText EstablishmentFeedback;
 bool bEstablishmentPending=false;
 int64 EstablishmentSequence=0, EstablishmentNonce=0;
 void RefreshEstablishment();
 bool ReceiveEstablishmentFeedback(const FHansaClientCommandFeedback& Feedback);
    FName ProjectedCityId;
    uint64 SelectedConstructionLease=0;
    FName SelectedConstructionBuilding;
    FName ConstructionReturnFocus;
    uint64 ProjectedViewerId=0;
	TSharedPtr<Hansa::Simulation::FHansaSimulationProjection> LastProjection;
	const Hansa::Simulation::FHansaEconomicRegistry* LastRegistry = nullptr;
	Hansa::Simulation::FHansaInventoryId PresenceFundingInventoryId;
	bool bPresenceUpgradeFunding = false;
	TArray<Hansa::Simulation::FHansaPresenceSpecializationProjection> PresenceSpecializations;
	int64 PresenceSpecializationRevision = 0;
	TArray<Hansa::Simulation::FHansaRouteStop> DraftStops;
	TArray<FHansaTradeMapCityPresentation> AllCities;
	TArray<FHansaTradeMapRoutePresentation> AllRoutes;
	TArray<FName> AvailableGoods;
 Hansa::Simulation::FHansaHouseId ViewerHouse;
	TWeakObjectPtr<UHansaRuntimeSimulationHost> Runtime;
	TFunction<bool(const FHansaClientCommandIntent&)> NetworkCommandIntent;
	FName FocusOriginSemanticId;
	uint64 Revision = 0;
	FHansaTradeMapChanged Changed;
	FHansaTradeMapFocusRestoreRequested FocusRestoreRequested;
};
