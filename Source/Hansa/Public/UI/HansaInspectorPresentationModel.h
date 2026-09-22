#pragma once

#include "CoreMinimal.h"
#include "Network/HansaMultiplayerTypes.h"
#include "UObject/Object.h"

#include "HansaInspectorPresentationModel.generated.h"

namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
	class FHansaDomainEvent;
	struct FHansaPopulationCohortProjection;
	struct FHansaConsumptionProjection;
	struct FHansaProductionProjection;
	struct FHansaForeignPresenceProjection;
	struct FHansaTradeStationProjection;
}
class UHansaRuntimeSimulationHost;
struct FHansaCargoWorldObservation;

/** Presentation-only data; authored recipe and save schemas are unchanged. */
struct HANSA_API FHansaInspectorProductionPort
{
    FName GoodId;
    FText Label;
    int64 PerBatch = 0, Stock = 0, Available = 0, Reserved = 0, ProducedTotal = 0;
    bool bStockKnown = false;
    int64 BuildingStock = 0, MarketStock = 0;
    bool bBuildingStockKnown = false, bMarketStockKnown = false;
    bool operator==(const FHansaInspectorProductionPort& R) const
    {
        return GoodId == R.GoodId && Label.EqualTo(R.Label) && PerBatch == R.PerBatch && ProducedTotal == R.ProducedTotal &&
            Stock == R.Stock && Available == R.Available && Reserved == R.Reserved && bStockKnown == R.bStockKnown &&
            BuildingStock == R.BuildingStock && MarketStock == R.MarketStock &&
            bBuildingStockKnown == R.bBuildingStockKnown && bMarketStockKnown == R.bMarketStockKnown;
    }
};

struct HANSA_API FHansaInspectorProductionData
{
    bool bValid = false, bActive = false, bCanProgress = false, bSimulationPaused = false;
    bool bConstruction = false;
    bool bRoadRequired = false, bHasMarketAccess = false, bDeliveryBlocked = false;
    int32 ProgressTicks = 0, CycleTicks = 0;
    int32 MarketRoadDistanceCells = 0, BlockedDeliveryCount = 0;
    int64 SelectedMarketBuildingValue = 0;
    FName MarketAccessCode;
    uint64 CompletedBatches = 0;
    int32 Laborers = 0, RequiredLaborers = 0, Artisans = 0, RequiredArtisans = 0;
    TArray<FHansaInspectorProductionPort> Inputs, Outputs;
    bool operator==(const FHansaInspectorProductionData& R) const
    {
        return bConstruction == R.bConstruction && bValid == R.bValid && bActive == R.bActive && bCanProgress == R.bCanProgress &&
            bSimulationPaused == R.bSimulationPaused && ProgressTicks == R.ProgressTicks && CycleTicks == R.CycleTicks &&
            CompletedBatches == R.CompletedBatches && Laborers == R.Laborers && RequiredLaborers == R.RequiredLaborers &&
            Artisans == R.Artisans && RequiredArtisans == R.RequiredArtisans && Inputs == R.Inputs && Outputs == R.Outputs &&
            bRoadRequired == R.bRoadRequired && bHasMarketAccess == R.bHasMarketAccess &&
            bDeliveryBlocked == R.bDeliveryBlocked && MarketRoadDistanceCells == R.MarketRoadDistanceCells &&
            BlockedDeliveryCount == R.BlockedDeliveryCount && SelectedMarketBuildingValue == R.SelectedMarketBuildingValue &&
            MarketAccessCode == R.MarketAccessCode;
    }
};

/** Read-only residence presentation, derived from the existing population projection. */
struct HANSA_API FHansaInspectorNeedData
{
 FName NeedId,GoodId; FText Label,Percent,Amount,SupplyDetail;
 int64 Required=0,Consumed=0;
 int32 Fulfillment=0,Access=0,Affordability=0,Reliability=0;
 bool bKnown=false,bService=false;
 bool operator==(const FHansaInspectorNeedData& R) const {return SupplyDetail.EqualTo(R.SupplyDetail) && Required==R.Required && Consumed==R.Consumed && Percent.EqualTo(R.Percent) && Amount.EqualTo(R.Amount) && NeedId==R.NeedId && GoodId==R.GoodId && Label.EqualTo(R.Label) && Fulfillment==R.Fulfillment && Access==R.Access && Affordability==R.Affordability && Reliability==R.Reliability && bKnown==R.bKnown && bService==R.bService;}
};
struct HANSA_API FHansaInspectorResidenceData
{
 bool bValid=false; int32 Residents=0,Capacity=0,Workforce=0;
 FText ConsumptionPeriod; int64 CoveredMinutes=0; bool bFullWindow=false;
 TArray<FHansaInspectorNeedData> Needs;
 bool operator==(const FHansaInspectorResidenceData& R) const {return ConsumptionPeriod.EqualTo(R.ConsumptionPeriod) && CoveredMinutes==R.CoveredMinutes && bFullWindow==R.bFullWindow && bValid==R.bValid && Residents==R.Residents && Capacity==R.Capacity && Workforce==R.Workforce && Needs==R.Needs;}
};

UENUM(BlueprintType)
enum class EHansaInspectorObjectKind : uint8
{
	None,
	ProductionBuilding,
	Residence,
    Cargo,
    Market,
	TradeStation
};

UENUM(BlueprintType)
enum class EHansaInspectorDataState : uint8 { Ready, Loading, Empty, Error };

UENUM(BlueprintType)
enum class EHansaCausalSeverity : uint8
{
	None,
	Notice,
	Warning,
	Critical
};

/** Shared UI-ready causal payload. Widgets render this; they never reproduce simulation formulas. */
USTRUCT(BlueprintType)
struct HANSA_API FHansaCausalPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Cause") FName StableCode;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Cause") FText Problem;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Cause") FText Cause;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Cause") FText Evidence;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Cause") FText Remedy;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Cause") FName RelatedSemanticId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Cause") EHansaCausalSeverity Severity = EHansaCausalSeverity::None;

	friend bool operator==(const FHansaCausalPresentation& Left, const FHansaCausalPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaInspectorFlowPresentation final
{
	GENERATED_BODY()

	// Read-only citizen consumption presentation, in simulation milli-units.
	FName DemandGoodId;
	int64 DemandRequired = 0, DemandSupplied = 0;
	FText DemandPeriod;
	bool bDemandKnown = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText Value;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText State;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") bool bProblem = false;

	friend bool operator==(const FHansaInspectorFlowPresentation& Left, const FHansaInspectorFlowPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaInspectorActionPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText ToolTip;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText DisabledReason;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") bool bSelected = false;

	friend bool operator==(const FHansaInspectorActionPresentation& Left, const FHansaInspectorActionPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaInspectorHistoryPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Inspector") FText Age;

	friend bool operator==(const FHansaInspectorHistoryPresentation& Left, const FHansaInspectorHistoryPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaInspectorSnapshot final
{
	GENERATED_BODY()

	FHansaInspectorProductionData Production;
	FHansaInspectorResidenceData Residence;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FName ObjectStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") EHansaInspectorDataState DataState = EHansaInspectorDataState::Ready;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText Identity;
    FText PreservationSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText State;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText PrimaryResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") TArray<FHansaInspectorFlowPresentation> Flows;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FHansaCausalPresentation Causal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") TArray<FHansaInspectorActionPresentation> Actions;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") TArray<FHansaInspectorHistoryPresentation> History;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText LastActionResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FName FocusOriginSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FName PendingConfirmationAction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") EHansaInspectorObjectKind Kind = EHansaInspectorObjectKind::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") int64 BuildingValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") bool bOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") bool bPinned = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") bool bCauseExpanded = true;

	friend bool operator==(const FHansaInspectorSnapshot& Left, const FHansaInspectorSnapshot& Right);
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaInspectorChanged, const FHansaInspectorSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaInspectorFocusRestoreRequested, FName);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaInspectorFrameRequested, int64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaInspectorRelatedTargetRequested, FName);

UCLASS(BlueprintType)
class HANSA_API UHansaInspectorPresentationModel final : public UObject
{
	GENERATED_BODY()

public:
	void BindRuntime(UHansaRuntimeSimulationHost* InRuntimeHost);
	void SetNetworkCommandIntent(TFunction<bool(const FHansaClientCommandIntent&)> InIntent) { NetworkCommandIntent = MoveTemp(InIntent); }
	void InitializeDefaults();
	void ShowStatus(EHansaInspectorDataState State, FText Detail, FText Remedy, FName FocusOrigin);
	bool ShowProduction(
		const Hansa::Simulation::FHansaProductionProjection& Production,
		const Hansa::Simulation::FHansaEconomicRegistry& Registry,
		TConstArrayView<Hansa::Simulation::FHansaDomainEvent> Events,
		FName FocusOriginSemanticId);
	bool ShowResidence(
		const Hansa::Simulation::FHansaPopulationCohortProjection& Residence,
		const Hansa::Simulation::FHansaEconomicRegistry& Registry,
		FName FocusOriginSemanticId);
	void ShowWorldBuilding(
		const FString& BuildingDefinitionId,
		FText DisplayName,
		FText DefinitionFlow,
		int64 BuildingValue,
		const FString& WorldState,
		const FString& ProductionBlocker,
		FName FocusOriginSemanticId);
	void OpenFromAlert(
		FName AlertId,
		FText AffectedObject,
		FText Age,
		int64 BuildingValue,
		const FHansaCausalPresentation& Causal,
		FName FocusOriginSemanticId);

    void ShowCargo(const FHansaCargoWorldObservation& Cargo, FName FocusOrigin);
	void ShowTradeStation(
		const Hansa::Simulation::FHansaTradeStationProjection& Station,
		const Hansa::Simulation::FHansaForeignPresenceProjection& Presence,
		FName FocusOrigin);
	bool CloseIntent();
	bool TogglePinIntent();
	bool FrameIntent();
	bool OpenCauseIntent();
	bool OpenRelatedIntent();
	bool ToggleProductionIntent();
    bool PreservationIntent(FName SemanticId);
	bool RecipeIntent(FName SemanticId);
	bool UpgradeResidenceIntent();
	bool CancelConstructionIntent();
	bool RemoveBuildingIntent();
	bool ActivateAction(FName SemanticId);
	void SetFocusedSemanticId(FName SemanticId);

	[[nodiscard]] const FHansaInspectorSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	[[nodiscard]] float GetBatchVisualFraction() const;
	void RefreshProductionClock();
	FHansaInspectorChanged& OnChanged() { return Changed; }
	FHansaInspectorFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
	FHansaInspectorFrameRequested& OnFrameRequested() { return FrameRequested; }
	FHansaInspectorRelatedTargetRequested& OnRelatedTargetRequested() { return RelatedTargetRequested; }

private:
	void BuildProductionDetail(const Hansa::Simulation::FHansaProductionProjection& Production, const Hansa::Simulation::FHansaEconomicRegistry& Registry);
	void ApplyProductionConnectivity();
	void PublishIfChanged(const FHansaInspectorSnapshot& Previous);
	void SetCommonActions();
	void AppendWorldActions();
	bool IsActionEnabled(FName SemanticId) const;
	bool ArmDestructiveAction(FName SemanticId, const FText& Confirmation);

	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|Inspector") FHansaInspectorSnapshot Snapshot;
	UPROPERTY(Transient) TWeakObjectPtr<UHansaRuntimeSimulationHost> RuntimeHost;
	TFunction<bool(const FHansaClientCommandIntent&)> NetworkCommandIntent;
	FString SelectedBuildingDefinitionId;
	uint64 Revision = 0;
	FHansaInspectorChanged Changed;
	FHansaInspectorFocusRestoreRequested FocusRestoreRequested;
	FHansaInspectorFrameRequested FrameRequested;
	FHansaInspectorRelatedTargetRequested RelatedTargetRequested;
};

HANSA_API FHansaCausalPresentation MakeProductionCausalPresentation(
	const Hansa::Simulation::FHansaProductionProjection& Production);
HANSA_API FHansaCausalPresentation MakeResidenceCausalPresentation(
	const Hansa::Simulation::FHansaPopulationCohortProjection& Residence);

/** Present authoritative rolling consumption totals for one city. */
HANSA_API TArray<FHansaInspectorFlowPresentation> BuildMarketDemandFlows(
    TConstArrayView<Hansa::Simulation::FHansaPopulationCohortProjection> Cohorts,
    const Hansa::Simulation::FHansaEconomicRegistry& Registry, const FString& CityId,
    const Hansa::Simulation::FHansaConsumptionProjection& Consumption);
HANSA_API FText FormatMarketConsumptionPeriod(const Hansa::Simulation::FHansaConsumptionProjection& Consumption);
