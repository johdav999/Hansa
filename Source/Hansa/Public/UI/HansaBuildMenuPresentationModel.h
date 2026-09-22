#pragma once

#include "CoreMinimal.h"
#include "Placement/HansaPlacement.h"
#include "UObject/Object.h"

#include "HansaBuildMenuPresentationModel.generated.h"

class UWorld;
class UHansaRuntimeSimulationHost;
struct FHansaBuildRuntimeState;
namespace Hansa::Simulation
{
	class FHansaEconomicRegistry;
}

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

/** Construction browsing only; never changes a household's authoritative tier. */
UENUM(BlueprintType)
enum class EHansaBuildTier : uint8
{
 DayLaborers,
 Craftsmen,
 Merchants
};

UENUM(BlueprintType)
enum class EHansaPlacementFeedback : uint8
{
	None,
	Valid,
	Warning,
	Invalid
};

UENUM(BlueprintType)
enum class EHansaRoadPreviewCellState : uint8
{
	NewValid,
	ExistingRoad,
	Invalid
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaRoadPreviewCell final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FIntPoint Cell = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") EHansaRoadPreviewCellState State = EHansaRoadPreviewCellState::NewValid;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FName Failure;

	friend bool operator==(const FHansaRoadPreviewCell& Left, const FHansaRoadPreviewCell& Right)
	{
		return Left.Cell == Right.Cell && Left.State == Right.State && Left.Failure == Right.Failure;
	}
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaBuildCardPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FName StableId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") EHansaBuildCategory Category = EHansaBuildCategory::Roads;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Tier;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") int32 BrowsingTierMask = 7;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Cost;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText WorkforceAndUpkeep;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Footprint;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText InputOutput;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText LockedReason;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText AvailabilityReason;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FName ProductionChainOutputGoodId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") int32 MenuOrder = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") bool bLocked = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") bool bAvailable = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") bool bFavorite = false;

	friend bool operator==(const FHansaBuildCardPresentation& Left, const FHansaBuildCardPresentation& Right);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaBuildChainPresentation final
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FName OutputGoodId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") FText Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hansa|UI|Build") int32 StageCount = 0;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") int32 BrowsingTierMask = 7;

	friend bool operator==(const FHansaBuildChainPresentation& Left, const FHansaBuildChainPresentation& Right)
	{
		return Left.OutputGoodId == Right.OutputGoodId && Left.Name.EqualTo(Right.Name) && Left.StageCount == Right.StageCount && Left.BrowsingTierMask == Right.BrowsingTierMask;
	}
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaBuildMenuSnapshot final
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") TArray<FHansaBuildCardPresentation> Cards;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") TArray<EHansaBuildCategory> Categories;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") TArray<FHansaBuildChainPresentation> ProductionChains;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") EHansaBuildCategory SelectedCategory = EHansaBuildCategory::Roads;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") EHansaBuildTier SelectedTier = EHansaBuildTier::DayLaborers;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FName SelectedBuildingId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FName SelectedProductionChainOutputGoodId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FName FocusedSemanticId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText ValidationCause;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText ValidationRemedy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText PlacementSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText LastResult;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") EHansaPlacementFeedback Feedback = EHansaPlacementFeedback::None;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") int32 RotationQuarterTurns = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FIntPoint AnchorCell = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") TArray<FIntPoint> FootprintCells;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") TArray<FHansaRoadPreviewCell> RoadPreviewCells;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FIntPoint RoadStartCell = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FIntPoint RoadEndCell = FIntPoint::ZeroValue;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") FText RoadTotalCost;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") int32 RoadNewCellCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") int32 RoadExistingCellCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") int32 RoadInvalidCellCount = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bDemolitionMode = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bHasTarget = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bCanConfirm = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bDraggingCard = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bRoadDrawing = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|UI|Build") bool bPointerOverWorld = false;
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
 bool SelectTier(EHansaBuildTier Tier);
 bool IsChainVisible(FName OutputGoodId) const;
	bool SelectProductionChain(FName OutputGoodId);
	bool SelectBuilding(FName BuildingId);
	bool IsCardVisible(FName BuildingId) const;
	FName GetSelectedCardId() const;
	bool BeginCardDrag(FName BuildingId);
	bool UpdateCardDragTarget(int32 X, int32 Y);
	bool ClearCardDragTarget();
	bool EndCardDrag(bool bReleasedOverWorld);
	bool BeginRoadDraw(int32 X, int32 Y);
	bool UpdateRoadDraw(int32 X, int32 Y);
	bool EndRoadDraw(bool bReleasedOverWorld);
	bool TargetGridCell(int32 X, int32 Y);
	bool TargetRoadAdjacentIntent();
	bool TargetShorelineIntent();
	bool RotateIntent();
	bool ToggleRepeatIntent();
	bool ToggleGridOverlayIntent();
	bool ToggleRoadOverlayIntent();
	bool ToggleFavoriteIntent();
	bool CompareIntent();
	bool ConfirmIntent(bool bKeepSelection = false);
	bool BeginBuildingStroke(int32 X, int32 Y);
	bool UpdateBuildingStroke(int32 X, int32 Y);
	void EndBuildingStroke();
	bool IsBuildingStrokeActive() const { return bBuildingStroke; }
	bool ClearPointerTarget();
	bool CancelIntent();
	bool ToggleDemolitionIntent();
	bool DemolishBuildingIntent(int64 BuildingValue);
	void SetNetworkPlaceIntent(TFunction<bool(TConstArrayView<Hansa::Simulation::FHansaPlacementSpec>)> Intent)
	{ NetworkPlaceIntent = MoveTemp(Intent); }
	void SetNetworkDemolishIntent(TFunction<bool(int64)> Intent)
	{ NetworkDemolishIntent = MoveTemp(Intent); }
	void SetFocusedSemanticId(FName SemanticId);

	[[nodiscard]] const FHansaBuildMenuSnapshot& GetSnapshot() const { return Snapshot; }
	[[nodiscard]] const FHansaBuildCardPresentation* FindCardPresentation(FName BuildingId) const { return FindCard(BuildingId); }
	[[nodiscard]] FIntPoint GetRoadAdjacentTarget() const;
	[[nodiscard]] TOptional<FIntPoint> FindValidShorelineTarget() const;
	[[nodiscard]] uint64 GetRevision() const { return Revision; }
	[[nodiscard]] uint8 GetAdjacentRoadMaskForPreview() const;
 [[nodiscard]] uint64 GetParcelSeedForPreview() const;
	[[nodiscard]] int32 GetPlacedBuildingCount() const;
	[[nodiscard]] int64 GetSimulationTick() const;
	[[nodiscard]] FString GetBuildingWorldStatus(int64 BuildingValue) const;
	bool AdvanceSimulationTicks(int32 TickCount);
	bool ReloadCatalog(FString& OutError);
	static bool BuildCatalogFromDefinitions(
		const Hansa::Simulation::FHansaEconomicRegistry& Registry,
		const TSet<FString>& CompletedTechnologyIds,
		TArray<FHansaBuildCardPresentation>& OutCards,
		TArray<FHansaBuildChainPresentation>& OutChains,
		FString& OutError);
	FHansaBuildMenuChanged& OnChanged() { return Changed; }

    void SetConstructionAllowed(bool Allowed);
    bool IsConstructionAllowed() const { return bConstructionAllowed; }
private:
    bool bConstructionAllowed=true;
	bool bRandomLabourSelection = false;
	TArray<FName> RemainingLabourCompounds;
	FName PickRandomLabourCompound();
	bool bBuildingStroke=false;
	TOptional<FIntPoint> StrokePrevious;
	TSet<FIntPoint> StrokeVisited;
	[[nodiscard]] TOptional<TPair<FIntPoint, int32>> FindValidShorelinePlacement() const;
	[[nodiscard]] UHansaRuntimeSimulationHost* GetSimulationHost() const;
	void PublishIfChanged(const FHansaBuildMenuSnapshot& Previous);
	void RefreshValidation();
	void RefreshRoadValidation();
	[[nodiscard]] TArray<Hansa::Simulation::FHansaPlacementSpec> BuildRoadConstructionSpecs() const;
	void RefreshAvailability();
	const FHansaBuildCardPresentation* FindCard(FName BuildingId) const;

	UPROPERTY(VisibleAnywhere, Category = "Hansa|UI|Build") FHansaBuildMenuSnapshot Snapshot;
	UPROPERTY(Transient) TObjectPtr<UHansaRuntimeSimulationHost> OwnedSimulationHost;
	TSharedPtr<FHansaBuildRuntimeState> Runtime;
	uint64 Revision = 0;
	FHansaBuildMenuChanged Changed;
	TFunction<bool(TConstArrayView<Hansa::Simulation::FHansaPlacementSpec>)> NetworkPlaceIntent;
	TFunction<bool(int64)> NetworkDemolishIntent;
};

HANSA_API const TCHAR* LexToString(EHansaBuildCategory Category);
HANSA_API const TCHAR* LexToString(EHansaBuildTier Tier);
