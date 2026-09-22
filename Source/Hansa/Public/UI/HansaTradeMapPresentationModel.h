#pragma once

#include "CoreMinimal.h"
#include "Network/HansaMultiplayerTypes.h"
#include "Trade/HansaTrade.h"
#include "UObject/Object.h"

#include "Presence/HansaStationOrders.h"
#include "Presence/HansaForeignPresence.h"
#include "HansaTradeMapPresentationModel.generated.h"

class UHansaRuntimeSimulationHost;
namespace Hansa::Simulation { class FHansaEconomicRegistry; class FHansaSimulationProjection; }

UENUM(BlueprintType)
enum class EHansaTradeMapModeFilter : uint8 { All, Sea, Land };

UENUM(BlueprintType)
enum class EHansaTradeMapCityFilter : uint8 { All, Presence, Routes };

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapCityPresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName StableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FVector2D NormalizedPosition = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Information;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bOwned = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bStale = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bUnknown = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bRendered = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bVisitable = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bBuildable = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bMarketOnly = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bHasPresence = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bHasRoute = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 ReportAgeTicks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText CapabilitySummary;
	friend bool operator==(const FHansaTradeMapCityPresentation&, const FHansaTradeMapCityPresentation&);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapStopPresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int32 Index = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName CityStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText CityLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName GoodStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ActionLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Quantity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText MinimumReserve;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText AccessibleLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bReserveRisk = false;
	friend bool operator==(const FHansaTradeMapStopPresentation&, const FHansaTradeMapStopPresentation&);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapRoutePresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 RouteValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 VehicleValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName DefinitionStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Mode;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText State;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Ownership;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StateHeading;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StateDetail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ToggleActionLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ToggleActionHint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StopSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Capacity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Upkeep;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText RoundTripTime;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ExpectedProfitRange;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Uncertainty;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bSea = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bActive = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bOwnedByPlayer = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCanToggleActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCanCancel = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bTraveling = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bReserveRisk = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bProfitKnown = false;
	friend bool operator==(const FHansaTradeMapRoutePresentation&, const FHansaTradeMapRoutePresentation&);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapSnapshot final
{
	GENERATED_BODY()
    bool bCreating = false;
    bool bShipInTransit = false;
    FVector2D ShipPosition = FVector2D::ZeroVector;
    bool bReview = false;
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
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StationOrderFeedback;
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

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaTradeMapChanged, const FHansaTradeMapSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaTradeMapFocusRestoreRequested, FName);

/** Event-driven Simple route editor. Drafts are committed only through the typed runtime command gateway. */
UCLASS(BlueprintType)
class HANSA_API UHansaTradeMapPresentationModel final : public UObject
{
	GENERATED_BODY()
public:
	void InitializeDefaults();
	void BindRuntime(UHansaRuntimeSimulationHost* RuntimeHost);
	void SetNetworkCommandIntent(TFunction<bool(const FHansaClientCommandIntent&)> InIntent) { NetworkCommandIntent = MoveTemp(InIntent); }
	bool ApplyProjection(const Hansa::Simulation::FHansaSimulationProjection& Projection,
		const Hansa::Simulation::FHansaEconomicRegistry& Registry);
	bool Open(FName FocusOrigin = TEXT("HUD.TopStatus.TradeMap"), FName PreferredGood = NAME_None, FName SourceCity = TEXT("City.Lubeck"));
	bool CloseIntent();
	bool CycleModeFilterIntent();
	bool CycleCityFilterIntent();
	bool SetCitySearchIntent(const FString& SearchText);
	bool CycleSelectedGoodIntent();
	bool MoveRouteWindowIntent(int32 Direction);
	bool SelectRouteIntent(int64 RouteValue);
	bool SelectCityIntent(FName CityId);
	bool CycleCityIntent(int32 Direction);
	bool SelectStopIntent(int32 StopIndex);
	bool CycleCargoActionIntent();
	bool AdjustQuantityIntent(int32 DeltaMilliUnits);
	bool AdjustMinimumReserveIntent(int32 DeltaMilliUnits);
	bool MoveStopIntent(int32 Direction);
    TFunction<bool(FName)> VisitRequested;
    bool VisitSelectedStopIntent();
    bool BeginCreateIntent(FName Good = NAME_None, FName SourceCity = TEXT("City.Lubeck"));
    bool DiscardCreateIntent();
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
	bool ToggleActiveIntent();
    bool TradeStationActionIntent();
	bool PresenceUpgradeActionIntent();
	bool PresenceSpecializationIntent(const FString& Action);
	bool CanPresenceSpecializationIntent(const FString& Action) const;
    bool StationOrderIntent(const FString& Action);
    bool CanStationOrderAction(const FString& Action) const;
    void RefreshStationOrderText();
    bool CancelRouteIntent();
	void SetCompact(bool bCompact);
	void SetFocusedSemanticId(FName SemanticId);
	[[nodiscard]] const FHansaTradeMapSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] const TArray<Hansa::Simulation::FHansaRouteStop>& GetDraftStops() const { return DraftStops; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaTradeMapChanged& OnChanged() { return Changed; }
	FHansaTradeMapFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
private:
	void RebuildStops();
	void RebuildFilteredProjection();
    void UpdateCreatorReview();
	void PublishIfChanged(const FHansaTradeMapSnapshot& Previous);
	const FHansaTradeMapRoutePresentation* FindSelectedRoute() const;
	UPROPERTY(VisibleAnywhere, Category="Hansa|UI|Trade") FHansaTradeMapSnapshot Snapshot;
    Hansa::Simulation::FHansaStationOrderTerms OrderDraft;
    TArray<Hansa::Simulation::FHansaStationOrderState> StationOrders;
    TArray<Hansa::Simulation::FHansaGoodId> OrderGoods;
    uint64 SelectedStationOrder = 0;
    int64 StationOrderCapacity = 0;
    int64 StationOrderMaxCap = 50000;
    int64 StationOrderMaxBudget = 1000000;
	FString PresenceUpgradeStageId;
	FString SelectedStationSiteId;
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
	TWeakObjectPtr<UHansaRuntimeSimulationHost> Runtime;
	TFunction<bool(const FHansaClientCommandIntent&)> NetworkCommandIntent;
	FName FocusOriginSemanticId;
	uint64 Revision = 0;
	FHansaTradeMapChanged Changed;
	FHansaTradeMapFocusRestoreRequested FocusRestoreRequested;
};
