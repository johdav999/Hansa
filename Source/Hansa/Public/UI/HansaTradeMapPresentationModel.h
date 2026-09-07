#pragma once

#include "CoreMinimal.h"
#include "Trade/HansaTrade.h"
#include "UObject/Object.h"

#include "HansaTradeMapPresentationModel.generated.h"

class UHansaRuntimeSimulationHost;
namespace Hansa::Simulation { class FHansaEconomicRegistry; class FHansaSimulationProjection; }

UENUM(BlueprintType)
enum class EHansaTradeMapModeFilter : uint8 { All, Sea, Land };

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bTraveling = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bReserveRisk = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bProfitKnown = false;
	friend bool operator==(const FHansaTradeMapRoutePresentation&, const FHansaTradeMapRoutePresentation&);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapSnapshot final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") TArray<FHansaTradeMapCityPresentation> Cities;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") TArray<FHansaTradeMapRoutePresentation> Routes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") TArray<FHansaTradeMapStopPresentation> Stops;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName PreferredGoodStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Title;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText EditorStatus;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ReserveRisk;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 SelectedRouteValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int32 SelectedStopIndex = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") EHansaTradeMapModeFilter ModeFilter = EHansaTradeMapModeFilter::All;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCompact = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bDirty = false;
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
	bool ApplyProjection(const Hansa::Simulation::FHansaSimulationProjection& Projection,
		const Hansa::Simulation::FHansaEconomicRegistry& Registry);
	bool Open(FName FocusOrigin = TEXT("HUD.TopStatus.TradeMap"), FName PreferredGood = NAME_None);
	bool CloseIntent();
	bool CycleModeFilterIntent();
	bool SelectRouteIntent(int64 RouteValue);
	bool SelectStopIntent(int32 StopIndex);
	bool CycleCargoActionIntent();
	bool AdjustQuantityIntent(int32 DeltaMilliUnits);
	bool AdjustMinimumReserveIntent(int32 DeltaMilliUnits);
	bool MoveStopIntent(int32 Direction);
	bool CommitIntent();
	bool ToggleActiveIntent();
	void SetCompact(bool bCompact);
	void SetFocusedSemanticId(FName SemanticId);
	[[nodiscard]] const FHansaTradeMapSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] const TArray<Hansa::Simulation::FHansaRouteStop>& GetDraftStops() const { return DraftStops; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaTradeMapChanged& OnChanged() { return Changed; }
	FHansaTradeMapFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
private:
	void RebuildStops();
	void PublishIfChanged(const FHansaTradeMapSnapshot& Previous);
	const FHansaTradeMapRoutePresentation* FindSelectedRoute() const;
	UPROPERTY(VisibleAnywhere, Category="Hansa|UI|Trade") FHansaTradeMapSnapshot Snapshot;
	TArray<Hansa::Simulation::FHansaRouteStop> DraftStops;
	TArray<FHansaTradeMapRoutePresentation> AllRoutes;
	TWeakObjectPtr<UHansaRuntimeSimulationHost> Runtime;
	FName FocusOriginSemanticId;
	uint64 Revision = 0;
	FHansaTradeMapChanged Changed;
	FHansaTradeMapFocusRestoreRequested FocusRestoreRequested;
};
