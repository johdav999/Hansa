#pragma once

#include "CoreMinimal.h"
#include "Model/HansaIds.h"
#include "Network/HansaMultiplayerTypes.h"
#include "Trade/HansaTrade.h"
#include "UObject/Object.h"

#include "HansaMarketTablePresentationModel.generated.h"

class UHansaRuntimeSimulationHost;

namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
	class FHansaSimulationProjection;
}

UENUM(BlueprintType)
enum class EHansaMarketGoodCategory : uint8
{
	All,
	Food,
	Material,
	Manufactured
};

UENUM(BlueprintType)
enum class EHansaMarketTrendFilter : uint8
{
	All,
	Rising,
	Stable,
	Falling,
	Unknown
};

UENUM(BlueprintType)
enum class EHansaMarketQuickFilter : uint8
{
	All,
	Shortage,
	OwnedStock,
	Incoming,
	Opportunity
};

UENUM(BlueprintType)
enum class EHansaMarketSortColumn : uint8
{
	Good,
	Stock,
	Reserve,
	Demand,
	Price,
	Trend,
	Incoming,
	Status
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaMarketTableRowPresentation final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FName GoodStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText GoodLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText GoodGlyph;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText CategoryLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Stock;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Reserve;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Demand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Price;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Trend;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Sparkline;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Incoming;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Status;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText ReportAge;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText AccessibleLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") EHansaMarketGoodCategory Category = EHansaMarketGoodCategory::Food;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") EHansaMarketTrendFilter TrendKind = EHansaMarketTrendFilter::Unknown;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 StockRaw = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 ReserveRaw = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 DemandRaw = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 PriceRaw = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 PriceDifferenceRaw = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int32 PriceDifferenceBasisPoints = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 IncomingRaw = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 ReportAgeTicks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bUnknown = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bStale = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bEstimated = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bShortage = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bOpportunity = false;

	friend bool operator==(const FHansaMarketTableRowPresentation& Left, const FHansaMarketTableRowPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaMarketChartPointPresentation final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 Tick = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 PriceMilliMarks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") float NormalizedPrice = 0.5f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText AccessibleLabel;

	friend bool operator==(const FHansaMarketChartPointPresentation& Left, const FHansaMarketChartPointPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaMarketFactorPresentation final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FName StableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Contribution;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText AccessibleLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int32 ContributionBasisPoints = 0;

	friend bool operator==(const FHansaMarketFactorPresentation& Left, const FHansaMarketFactorPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaMarketRelationshipPresentation final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FName StableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Detail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Status;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText AccessibleLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bWarning = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 BuildingValue = 0;

	friend bool operator==(const FHansaMarketRelationshipPresentation& Left, const FHansaMarketRelationshipPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaSelectedGoodPresentation final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FName GoodStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText GoodLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText GoodGlyph;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Confidence;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText BaseValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText LocalPrice;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText RecentAverageDifference;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText StockVersusReserve;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText ReserveDays;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText CitizenDemand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText IndustrialDemand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText IncomingSupply;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Production;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Consumption;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SupplyBalance;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText Explanation;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText ChartSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText PinActionLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText PinDisabledReason;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText RouteActionLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText RouteDisabledReason;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText LastActionResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SpotTradeHeading;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SpotTradeVehicle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SpotTradeQuantity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SpotTradeQuote;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SpotTradeRemedy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SpotTradeResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SpotTradeConfirmLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 SpotTradeVehicleValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 SpotTradeQuantityRaw = 5000;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 SpotTradeReviewedMarketUpdateTick = -1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 SpotTradeReviewedUnitPrice = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bSpotTradeVisible = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bSpotTradeBuy = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bSpotTradeCanSubmit = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") TArray<FHansaMarketChartPointPresentation> History;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") TArray<FHansaMarketFactorPresentation> Factors;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") TArray<FHansaMarketRelationshipPresentation> Consumers;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") TArray<FHansaMarketRelationshipPresentation> Producers;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 CurrentPriceMilliMarks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 RecentAveragePriceMilliMarks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 MinimumHistoryPriceMilliMarks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") int64 MaximumHistoryPriceMilliMarks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bHasSelection = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bHasReport = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bStale = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bPinned = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bPinEnabled = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bRouteEnabled = false;

	friend bool operator==(const FHansaSelectedGoodPresentation& Left, const FHansaSelectedGoodPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaMarketTableSnapshot final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") TArray<FHansaMarketTableRowPresentation> AllRows;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") TArray<FHansaMarketTableRowPresentation> VisibleRows;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText SearchText;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText ResultSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText EmptyTitle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FText EmptyDetail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FName SelectedGoodStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") EHansaMarketGoodCategory CategoryFilter = EHansaMarketGoodCategory::All;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") EHansaMarketTrendFilter TrendFilter = EHansaMarketTrendFilter::All;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") EHansaMarketQuickFilter QuickFilter = EHansaMarketQuickFilter::All;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") EHansaMarketSortColumn SortColumn = EHansaMarketSortColumn::Good;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") bool bSortAscending = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Market") FHansaSelectedGoodPresentation SelectedGood;

	friend bool operator==(const FHansaMarketTableSnapshot& Left, const FHansaMarketTableSnapshot& Right);
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaMarketTableChanged, const FHansaMarketTableSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaMarketRouteRequested, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaMarketBuildingRequested, FName, int64);

/** Immutable, event-driven ten-good market table model. */
UCLASS(BlueprintType)
class HANSA_API UHansaMarketTablePresentationModel final : public UObject
{
	GENERATED_BODY()

public:
	void InitializeDefaults();
	void BindRuntime(UHansaRuntimeSimulationHost* RuntimeHost);
	void SetNetworkCommandIntent(TFunction<bool(const FHansaClientCommandIntent&)> InIntent) { NetworkCommandIntent = MoveTemp(InIntent); }
#if WITH_DEV_AUTOMATION_TESTS
	void SetSpotTradeTestContext(Hansa::Simulation::FHansaCityDefinitionId CityId, Hansa::Simulation::FHansaVehicleId VehicleId, TFunction<Hansa::Simulation::FHansaSpotTradeQuoteProjection(Hansa::Simulation::EHansaSpotTradeSide,Hansa::Simulation::FHansaQuantity)> QuoteProvider);
#endif
	bool ApplyProjection(
		const Hansa::Simulation::FHansaSimulationProjection& Projection,
		const Hansa::Simulation::FHansaEconomicRegistry& Registry,
		Hansa::Simulation::FHansaCityDefinitionId CityId);
	bool SetSearchTextIntent(FText SearchText);
	bool CycleCategoryFilterIntent();
	bool CycleTrendFilterIntent();
	bool CycleQuickFilterIntent();
	bool ClearFiltersIntent();
	bool SortByIntent(EHansaMarketSortColumn Column);
	bool SelectGoodIntent(FName GoodStableId);
	bool TogglePinIntent();
	bool BeginRouteIntent();
	bool CycleSpotTradeSideIntent();
	bool AdjustSpotTradeQuantityIntent(int64 DeltaMilliUnits);
	bool ConfirmSpotTradeIntent();
	void SetFocusedSemanticId(FName SemanticId);
	bool RevealRelationshipIntent(bool bProducer, FName StableId);
	FHansaMarketBuildingRequested& OnBuildingRequested() { return BuildingRequested; }

	[[nodiscard]] const FHansaMarketTableSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] const FHansaMarketTableRowPresentation* FindRow(FName GoodStableId) const;
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaMarketTableChanged& OnChanged() { return Changed; }
	FHansaMarketRouteRequested& OnRouteRequested() { return RouteRequested; }

private:
	void RebuildVisibleRows();
	void RebuildSelectedGood();
	void RebuildSpotTrade();
	void PublishIfChanged(const FHansaMarketTableSnapshot& Previous);

	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|Market")
	FHansaMarketTableSnapshot Snapshot;

	TMap<FName, FHansaSelectedGoodPresentation> DetailByGood;
	TSet<FName> PinnedGoods;
	TWeakObjectPtr<UHansaRuntimeSimulationHost> Runtime;
	TFunction<bool(const FHansaClientCommandIntent&)> NetworkCommandIntent;
	Hansa::Simulation::FHansaCityDefinitionId CurrentCityId;
	Hansa::Simulation::FHansaVehicleId SpotTradeVehicleId;
	int64 SpotTradeQuantityRaw = 5000;
	bool bSpotTradeBuy = true;
	FText SpotTradeResult;
#if WITH_DEV_AUTOMATION_TESTS
	TFunction<Hansa::Simulation::FHansaSpotTradeQuoteProjection(Hansa::Simulation::EHansaSpotTradeSide,Hansa::Simulation::FHansaQuantity)> SpotTradeQuoteForTesting;
#endif

	uint64 Revision = 0;
	FHansaMarketTableChanged Changed;
	FHansaMarketRouteRequested RouteRequested;
	FHansaMarketBuildingRequested BuildingRequested;
};
