#pragma once

#include "CoreMinimal.h"
#include "Model/HansaIds.h"
#include "UObject/Object.h"
#include "UI/HansaInspectorPresentationModel.h"

#include "HansaHudPresentationModel.generated.h"

namespace Hansa::Simulation
{
	class FHansaSimulationProjection;
    struct FHansaCityMarketProjection;
}

UENUM(BlueprintType)
enum class EHansaHudGameSpeed : uint8
{
	Paused,
	Normal,
	Fast,
	Fastest
};

UENUM(BlueprintType)
enum class EHansaHudAlertAction : uint8
{
	Frame,
	OpenCause,
	Snooze,
	Pin
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaHudAlertPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FName StableId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD") FName GroupId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD") FText AffectedObject;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD") FText Age;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD") FHansaCausalPresentation Causal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD") int64 AffectedBuildingValue = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD") bool bSnoozed = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD") bool bPinned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	bool bWarning = false;

	friend bool operator==(const FHansaHudAlertPresentation& Left, const FHansaHudAlertPresentation& Right)
	{
		return Left.StableId == Right.StableId && Left.Label.EqualTo(Right.Label) && Left.GroupId == Right.GroupId &&
			Left.AffectedObject.EqualTo(Right.AffectedObject) && Left.Age.EqualTo(Right.Age) && Left.Causal == Right.Causal &&
			Left.AffectedBuildingValue == Right.AffectedBuildingValue && Left.bSnoozed == Right.bSnoozed &&
			Left.bPinned == Right.bPinned && Left.bWarning == Right.bWarning;
	}
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaHudNotificationPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FName StableId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText Label;

	friend bool operator==(const FHansaHudNotificationPresentation& Left, const FHansaHudNotificationPresentation& Right)
	{
		return Left.StableId == Right.StableId && Left.Label.EqualTo(Right.Label);
	}
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaHudProductSummary final
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|HUD") FName GoodId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|HUD") FText Value;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|HUD") FText Tooltip;
    friend bool operator==(const FHansaHudProductSummary& A, const FHansaHudProductSummary& B)
    { return A.GoodId==B.GoodId && A.Value.EqualTo(B.Value) && A.Tooltip.EqualTo(B.Tooltip); }
};

/** Immutable-at-the-widget-boundary state delivered to the root HUD after gameplay events. */
USTRUCT(BlueprintType)
struct HANSA_API FHansaHudPresentationSnapshot final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText Money;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText MoneyTrend;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText Population;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText Workforce;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|HUD") FText WealthyCitizens;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|HUD") FText MoneyTrendTooltip;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|HUD") TArray<FHansaHudProductSummary> TopProducts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText CityBreadcrumb;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|HUD") bool bRemoteCityView=false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText DateAndSeason;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText Research;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText Connection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	EHansaHudGameSpeed Speed = EHansaHudGameSpeed::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	TArray<FHansaHudAlertPresentation> Alerts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	TArray<FHansaHudNotificationPresentation> Notifications;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText SelectionSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText InspectorTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	FText InspectorSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	bool bAlertStackExpanded = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	bool bBottomAreaOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|HUD")
	bool bInspectorOpen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|HUD")
	FName FocusedSemanticId;

	friend bool operator==(const FHansaHudPresentationSnapshot& Left, const FHansaHudPresentationSnapshot& Right);
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FHansaHudPresentationChanged,
	const FHansaHudPresentationSnapshot&,
	uint64);

/** Event-fed presentation state. It never polls gameplay and never ticks. */
UCLASS(BlueprintType)
class HANSA_API UHansaHudPresentationModel final : public UObject
{
	GENERATED_BODY()

public:
	void InitializeDefaults();
    void ResetCashHistory();
	bool ApplySnapshot(const FHansaHudPresentationSnapshot& NewSnapshot);

	UFUNCTION(BlueprintCallable, Category = "Hansa|UI|HUD")
	void ToggleAlertStack();

	UFUNCTION(BlueprintCallable, Category = "Hansa|UI|HUD")
	void SetBottomAreaOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "Hansa|UI|HUD")
	void SetInspectorOpen(bool bOpen);

	UFUNCTION(BlueprintCallable, Category = "Hansa|UI|HUD")
	void SetSpeed(EHansaHudGameSpeed NewSpeed);

	void SetFocusedSemanticId(FName SemanticId);
	void SetSelection(FText SelectionSummary, FText InspectorTitle, FText InspectorSummary, bool bOpenInspector);
	void ApplyRuntimeStatus(const Hansa::Simulation::FHansaSimulationProjection& Projection,
        Hansa::Simulation::FHansaCityDefinitionId CityId,Hansa::Simulation::FHansaHouseId HouseId);
	bool ApplyMarketAlerts(
		const Hansa::Simulation::FHansaSimulationProjection& Projection,
		Hansa::Simulation::FHansaCityDefinitionId CityId,
		FText CityDisplayName);
	bool ApplyAlertAction(FName AlertId, EHansaHudAlertAction Action);
	[[nodiscard]] const FHansaHudAlertPresentation* FindAlert(FName AlertId) const;

	[[nodiscard]] const FHansaHudPresentationSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaHudPresentationChanged& OnChanged() { return PresentationChanged; }

    static FHansaHudProductSummary BuildBreadBalance(const Hansa::Simulation::FHansaCityMarketProjection* Market, uint16 MinutesPerTick);

private:
	void BroadcastChange();

	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|HUD")
	FHansaHudPresentationSnapshot Snapshot;

    struct FCashSample { int64 Minute; int64 MilliMarks; };
    TArray<FCashSample> CashHistory;
    Hansa::Simulation::FHansaHouseId CashHistoryHouse;
	uint64 Revision = 0;
	FHansaHudPresentationChanged PresentationChanged;
};
