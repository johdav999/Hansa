#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HansaInspectorPresentationModel.generated.h"

namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
	class FHansaDomainEvent;
	struct FHansaPopulationCohortProjection;
	struct FHansaProductionProjection;
}

UENUM(BlueprintType)
enum class EHansaInspectorObjectKind : uint8
{
	None,
	ProductionBuilding,
	Residence
};

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FName ObjectStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText Identity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText State;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText PrimaryResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") TArray<FHansaInspectorFlowPresentation> Flows;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FHansaCausalPresentation Causal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") TArray<FHansaInspectorActionPresentation> Actions;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") TArray<FHansaInspectorHistoryPresentation> History;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FText LastActionResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FName FocusOriginSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Inspector") FName FocusedSemanticId;
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
	void InitializeDefaults();
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

	bool CloseIntent();
	bool TogglePinIntent();
	bool FrameIntent();
	bool OpenCauseIntent();
	bool OpenRelatedIntent();
	bool ActivateAction(FName SemanticId);
	void SetFocusedSemanticId(FName SemanticId);

	[[nodiscard]] const FHansaInspectorSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	FHansaInspectorChanged& OnChanged() { return Changed; }
	FHansaInspectorFocusRestoreRequested& OnFocusRestoreRequested() { return FocusRestoreRequested; }
	FHansaInspectorFrameRequested& OnFrameRequested() { return FrameRequested; }
	FHansaInspectorRelatedTargetRequested& OnRelatedTargetRequested() { return RelatedTargetRequested; }

private:
	void PublishIfChanged(const FHansaInspectorSnapshot& Previous);
	void SetCommonActions();

	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|Inspector") FHansaInspectorSnapshot Snapshot;
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
