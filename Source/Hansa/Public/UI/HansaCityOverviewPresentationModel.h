#pragma once

#include "CoreMinimal.h"
#include "Model/HansaIds.h"
#include "UObject/Object.h"

#include "HansaCityOverviewPresentationModel.generated.h"

namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
	class FHansaSimulationProjection;
}

class UHansaRuntimeSimulationHost;

UENUM(BlueprintType)
enum class EHansaCityOverviewTab : uint8
{
	Population,
	Production,
	Market
};

UENUM(BlueprintType)
enum class EHansaCityOverviewLoadState : uint8
{
	Ready,
	Loading,
	Empty,
	Error
};

UENUM(BlueprintType)
enum class EHansaCityOverviewRowKind : uint8
{
	Population,
	Production,
	Market
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCityOverviewSummaryPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Value;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Detail;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") bool bWarning = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") bool bError = false;

	friend bool operator==(const FHansaCityOverviewSummaryPresentation& Left, const FHansaCityOverviewSummaryPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCityOverviewFieldPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Label;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Value;

	friend bool operator==(const FHansaCityOverviewFieldPresentation& Left, const FHansaCityOverviewFieldPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCityOverviewRowPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Subtitle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") TArray<FHansaCityOverviewFieldPresentation> Fields;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText Status;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText CausalActionLabel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FText CausalActionDisabledReason;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FName RelatedSemanticId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") FName GoodStableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") EHansaCityOverviewRowKind Kind = EHansaCityOverviewRowKind::Population;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") int64 BuildingValue = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") bool bCausalActionEnabled = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") bool bWarning = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|City Overview") bool bError = false;

	friend bool operator==(const FHansaCityOverviewRowPresentation& Left, const FHansaCityOverviewRowPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaCityOverviewSnapshot final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FName CityStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FText CityTitle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") TArray<FHansaCityOverviewSummaryPresentation> HeaderSummaries;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") TArray<FHansaCityOverviewRowPresentation> PopulationRows;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") TArray<FHansaCityOverviewRowPresentation> ProductionRows;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") TArray<FHansaCityOverviewRowPresentation> MarketRows;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FText StateTitle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FText StateDetail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FText LastActionResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FName FocusOriginSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") FName SelectedRowStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") EHansaCityOverviewTab ActiveTab = EHansaCityOverviewTab::Population;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") EHansaCityOverviewLoadState LoadState = EHansaCityOverviewLoadState::Empty;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|City Overview") bool bOpen = false;

	friend bool operator==(const FHansaCityOverviewSnapshot& Left, const FHansaCityOverviewSnapshot& Right);
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaCityOverviewChanged, const FHansaCityOverviewSnapshot&, uint64);
DECLARE_MULTICAST_DELEGATE_OneParam(FHansaCityOverviewFocusRestoreRequested, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaCityOverviewRelatedTargetRequested, FName, int64);

/** Event-fed, immutable-at-the-widget-boundary city dashboard presentation model. */
UCLASS(BlueprintType)
class HANSA_API UHansaCityOverviewPresentationModel final : public UObject
{
	GENERATED_BODY()

public:
	void InitializeDefaults();
	bool ApplyProjection(
		const Hansa::Simulation::FHansaSimulationProjection& Projection,
		const Hansa::Simulation::FHansaEconomicRegistry& Registry,
		Hansa::Simulation::FHansaCityDefinitionId CityId,
		FText CityDisplayName,
		int64 TreasuryContributionMilliMarks = 0,
        const UHansaRuntimeSimulationHost* KnowledgeSource = nullptr);

	bool Open(FName FocusOriginSemanticId = TEXT("HUD.TopStatus.CityOverview"));
	bool CloseIntent();
	bool SelectTabIntent(EHansaCityOverviewTab Tab);
	bool CycleTabIntent(int32 Direction);
	bool SelectRowIntent(FName RowStableId);
	bool ActivateCausalIntent(FName RowStableId);
	bool RetryIntent();
    bool SelectCityIntent(FName CityId);
    TFunction<bool(FName)> VisitRequested;
    bool VisitCityIntent();
    void SetVisitStatus(FText Status);
    FSimpleMulticastDelegate& OnRefreshRequested() { return RefreshRequested; }
	void SetFocusedSemanticId(FName SemanticId);
	void SetLoading(FText CityDisplayName = FText());
	void SetError(FText Cause, FText Remedy);

	[[nodiscard]] const FHansaCityOverviewSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] TConstArrayView<FHansaCityOverviewRowPresentation> GetActiveRows() const;
	[[nodiscard]] const FHansaCityOverviewRowPresentation* FindActiveRow(FName StableId) const;
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaCityOverviewChanged& OnChanged() { return Changed; }
	FHansaCityOverviewFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
	FHansaCityOverviewRelatedTargetRequested& OnRelatedTargetRequested() { return RelatedTargetRequested; }

private:
	void PublishIfChanged(const FHansaCityOverviewSnapshot& Previous);
	void UpdateStateForActiveRows();

	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|City Overview")
	FHansaCityOverviewSnapshot Snapshot;

	uint64 Revision = 0;
	FHansaCityOverviewChanged Changed;
    FSimpleMulticastDelegate RefreshRequested;
	FHansaCityOverviewFocusRestoreRequested FocusRestoreRequested;
	FHansaCityOverviewRelatedTargetRequested RelatedTargetRequested;
};
