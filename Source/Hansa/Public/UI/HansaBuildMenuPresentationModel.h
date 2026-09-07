#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "HansaBuildMenuPresentationModel.generated.h"

class UWorld;
class UHansaRuntimeSimulationHost;
struct FHansaBuildRuntimeState;

UENUM(BlueprintType)
enum class EHansaBuildCategory : uint8
{
	Roads,
	Residences,
	Production,
	Storage,
	Harbor,
	Civic,
	Decoration
};

UENUM(BlueprintType)
enum class EHansaPlacementFeedback : uint8
{
	None,
	Valid,
	Warning,
	Invalid
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaBuildCardPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") EHansaBuildCategory Category = EHansaBuildCategory::Roads;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Tier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Cost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText WorkforceAndUpkeep;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Footprint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText InputOutput;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText LockedReason;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") bool bLocked = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") bool bFavorite = false;

	friend bool operator==(const FHansaBuildCardPresentation& Left, const FHansaBuildCardPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaBuildMenuSnapshot final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") TArray<FHansaBuildCardPresentation> Cards;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") EHansaBuildCategory SelectedCategory = EHansaBuildCategory::Roads;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FName SelectedBuildingId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText ValidationCause;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText ValidationRemedy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText PlacementSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText LastResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") EHansaPlacementFeedback Feedback = EHansaPlacementFeedback::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") int32 RotationQuarterTurns = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bHasTarget = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bCanConfirm = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bRepeat = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bGridOverlay = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bRoadOverlay = true;

	friend bool operator==(const FHansaBuildMenuSnapshot& Left, const FHansaBuildMenuSnapshot& Right);
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FHansaBuildMenuChanged, const FHansaBuildMenuSnapshot&, uint64);

/** Event-updated build presenter and normal player-intent adapter to the authoritative command gateway. */
UCLASS(BlueprintType)
class HANSA_API UHansaBuildMenuPresentationModel final : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UHansaBuildMenuPresentationModel() override;

	bool InitializeForLubeck(UWorld* World, FString& OutError);
	bool InitializeForLubeck(UWorld* World, UHansaRuntimeSimulationHost* SimulationHost, FString& OutError);
	void SetOpen(bool bOpen);
	bool SelectCategory(EHansaBuildCategory Category);
	bool SelectBuilding(FName BuildingId);
	bool TargetGridCell(int32 X, int32 Y);
	bool TargetRoadAdjacentIntent();
	bool TargetShorelineIntent();
	bool RotateIntent();
	bool ToggleRepeatIntent();
	bool ToggleGridOverlayIntent();
	bool ToggleRoadOverlayIntent();
	bool ToggleFavoriteIntent();
	bool CompareIntent();
	bool ConfirmIntent();
	bool CancelIntent();
	void SetFocusedSemanticId(FName SemanticId);

	[[nodiscard]] const FHansaBuildMenuSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] const FHansaBuildCardPresentation* FindCardPresentation(FName BuildingId) const { return FindCard(BuildingId); }
	[[nodiscard]] FIntPoint GetRoadAdjacentTarget() const;
	[[nodiscard]] TOptional<FIntPoint> FindValidShorelineTarget() const;
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	[[nodiscard]] int32 GetPlacedBuildingCount() const;
	[[nodiscard]] int64 GetSimulationTick() const;
	[[nodiscard]] FString GetBuildingWorldStatus(int64 BuildingValue) const;
	bool AdvanceSimulationTicks(int32 TickCount);
	FHansaBuildMenuChanged& OnChanged() { return Changed; }

private:
	[[nodiscard]] TOptional<TPair<FIntPoint, int32>> FindValidShorelinePlacement() const;
	[[nodiscard]] UHansaRuntimeSimulationHost* GetSimulationHost() const;
	void PublishIfChanged(const FHansaBuildMenuSnapshot& Previous);
	void RefreshValidation();
	const FHansaBuildCardPresentation* FindCard(FName BuildingId) const;

	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|Build") FHansaBuildMenuSnapshot Snapshot;
	UPROPERTY(Transient) TObjectPtr<UHansaRuntimeSimulationHost> OwnedSimulationHost;
	TSharedPtr<FHansaBuildRuntimeState> Runtime;
	uint64 Revision = 0;
	FHansaBuildMenuChanged Changed;
};

HANSA_API const TCHAR* LexToString(EHansaBuildCategory Category);
