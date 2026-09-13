#pragma once

#include "CoreMinimal.h"
#include "Events/HansaDomainEvent.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"

#include "HansaBuildingWorldProjection.generated.h"

class AHansaLubeckWorldFoundation;
class UHansaDefinitionBase;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UChildActorComponent;
class UTextRenderComponent;
enum class EHansaPlacementFeedback : uint8;
struct FHansaRoadPreviewCell;

namespace Hansa::Game
{
	struct HANSA_API FHansaPlacementProjectionDelta final
	{
		TArray<Hansa::Simulation::FHansaBuildingId> Created;
		TArray<Hansa::Simulation::FHansaBuildingId> Updated;
		TArray<Hansa::Simulation::FHansaBuildingId> Removed;
	};

	/**
	 * Presentation-only identity registry. Reconciliation is transactional and canonical, so a malformed
	 * projection cannot partially mutate the visible entity mapping.
	 */
	class HANSA_API FHansaPlacementProjectionRegistry final
	{
	public:
		bool Reconcile(
			TConstArrayView<Hansa::Simulation::FHansaBuildingWorldProjection> Projections,
			FHansaPlacementProjectionDelta& OutDelta);
		void Reset();

		[[nodiscard]] int32 Num() const { return Entries.Num(); }
		[[nodiscard]] const Hansa::Simulation::FHansaBuildingWorldProjection* Find(
			Hansa::Simulation::FHansaBuildingId BuildingId) const;
		[[nodiscard]] TArray<Hansa::Simulation::FHansaBuildingId> GetCanonicalIds() const;

	private:
		TMap<Hansa::Simulation::FHansaBuildingId, Hansa::Simulation::FHansaBuildingWorldProjection> Entries;
	};
}

/** Exact production state behind the bounded role meshes; no cosmetic stock counts. */
USTRUCT(BlueprintType)
struct HANSA_API FHansaProductionWorldObservation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
    UPROPERTY(BlueprintReadOnly) bool bWorking = false;
    UPROPERTY(BlueprintReadOnly) int32 ProgressTicks = 0;
    UPROPERTY(BlueprintReadOnly) int32 CycleTicks = 0;
    UPROPERTY(BlueprintReadOnly) int64 CompletedCycles = 0;
    UPROPERTY(BlueprintReadOnly) FName Blocker;
    UPROPERTY(BlueprintReadOnly) FString PresentationFailure;
};

/** One managed, non-authoritative world representation of a placed building or road. */
UCLASS(NotBlueprintable)
class HANSA_API AHansaBuildingWorldProjectionActor final : public AActor
{
	GENERATED_BODY()

public:
	AHansaBuildingWorldProjectionActor();
	virtual void Tick(float DeltaSeconds) override;

	void ApplyProjection(
		const Hansa::Simulation::FHansaBuildingWorldProjection& Projection,
		const AHansaLubeckWorldFoundation& Foundation, uint8 RoadNeighborMask = 0);
	void SetSelected(bool bInSelected);
    void ApplyProduction(const Hansa::Simulation::FHansaProductionProjection* Production,
        TConstArrayView<Hansa::Simulation::FHansaInventoryProjection> Inventories);
    void SampleProduction(double TickFraction);
    UFUNCTION(BlueprintPure, Category="Hansa|World|Projection") FHansaProductionWorldObservation QueryProduction() const { return ProductionObservation; }

	[[nodiscard]] Hansa::Simulation::FHansaBuildingId GetBuildingId() const { return BuildingId; }
	[[nodiscard]] const FString& GetBuildingDefinitionId() const { return BuildingDefinitionId; }
	[[nodiscard]] Hansa::Simulation::EHansaBuildingWorldStatus GetWorldStatus() const { return WorldStatus; }
	[[nodiscard]] Hansa::Simulation::EHansaProductionBlocker GetProductionBlocker() const { return ProductionBlocker; }
	[[nodiscard]] bool IsSelected() const { return bSelected; }
	[[nodiscard]] bool IsRoad() const { return bRoad; }
	[[nodiscard]] bool IsRoadDisconnectedIndicatorVisible() const { return bRoadDisconnected; }

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Projection")
	int64 GetStableBuildingValue() const { return static_cast<int64>(BuildingId.GetValue()); }

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Projection")
	int32 GetStableBuildingGeneration() const { return static_cast<int32>(BuildingId.GetGeneration()); }

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Projection")
	FString GetStableBuildingDefinitionId() const { return BuildingDefinitionId; }

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Projection")
	FName GetStatusName() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UStaticMeshComponent> BuildingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UChildActorComponent> BuildingPresentation;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UStaticMeshComponent> ConstructionPlaceholder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UStaticMeshComponent> SelectionOutline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UStaticMeshComponent> StatusMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UStaticMeshComponent> RoadDisconnectedMarker;

private:
	void EnsureMaterials();
	void ApplyVisualState();

	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Projection")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UHansaDefinitionBase> PresentationDefinition;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	Hansa::Simulation::FHansaBuildingId BuildingId;
	FString BuildingDefinitionId;
	Hansa::Simulation::EHansaBuildingWorldStatus WorldStatus =
		Hansa::Simulation::EHansaBuildingWorldStatus::UnderConstruction;
	Hansa::Simulation::EHansaProductionBlocker ProductionBlocker = Hansa::Simulation::EHansaProductionBlocker::None;
    FHansaProductionWorldObservation ProductionObservation;
    TMap<TWeakObjectPtr<USceneComponent>, FRotator> MechanicalRestRotations;
	bool bSelected = false;
	bool bRoad = false;
	bool bRoadDisconnected = false;
	UPROPERTY(EditDefaultsOnly, Category = "Hansa|World|Projection", meta = (ClampMin = "0.0"))
	float RoadDisconnectedRotationDegreesPerSecond = 45.0f;
	FBox PresentationBounds = FBox(ForceInit);
};

/** Non-authoritative world-space preview for a construction placement session. */
UCLASS(NotBlueprintable)
class HANSA_API AHansaBuildingPlacementGhost final : public AActor
{
	GENERATED_BODY()

public:
	AHansaBuildingPlacementGhost();

	void ApplyPreview(
		FName BuildingDefinitionId,
		FIntPoint AnchorCell,
		int32 RotationQuarterTurns,
		TConstArrayView<FIntPoint> FootprintCells,
		EHansaPlacementFeedback Feedback,
		const FText& Reason,
		const AHansaLubeckWorldFoundation& Foundation);
	void ApplyRoadPreview(
		TConstArrayView<FHansaRoadPreviewCell> Cells,
		EHansaPlacementFeedback Feedback,
		const FText& Reason,
		const AHansaLubeckWorldFoundation& Foundation);
	void HidePreview();

	[[nodiscard]] bool IsPreviewVisible() const { return !IsHidden(); }
	[[nodiscard]] FName GetPreviewBuildingId() const { return PreviewBuildingId; }
	[[nodiscard]] int32 GetPreviewCellCount() const { return ActiveFootprintCellCount; }
	[[nodiscard]] int32 GetRoadPieceCount() const { return ActiveRoadPieceCount; }

private:
	UStaticMeshComponent* AcquireFootprintCell(int32 Index);
	UStaticMeshComponent* AcquireRoadPiece(int32 Index);
	void ApplyFeedbackVisuals(EHansaPlacementFeedback Feedback, const FText& Reason);

	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Placement") TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Placement") TObjectPtr<UStaticMeshComponent> BuildingMesh;
	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Placement") TObjectPtr<UChildActorComponent> BuildingPresentation;
	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Placement") TArray<TObjectPtr<UStaticMeshComponent>> FootprintCellMeshes;
	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Placement") TArray<TObjectPtr<UStaticMeshComponent>> RoadPieceMeshes;
	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Placement") TArray<TObjectPtr<UStaticMeshComponent>> OutlineMeshes;
	UPROPERTY(VisibleAnywhere, Category = "Hansa|World|Placement") TObjectPtr<UTextRenderComponent> StatusText;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
	UPROPERTY() TObjectPtr<UHansaDefinitionBase> PresentationDefinition;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FeedbackMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RoadValidMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RoadExistingMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RoadInvalidMaterial;
	FName PreviewBuildingId;
	FBox PresentationBounds = FBox(ForceInit);
	int32 ActiveFootprintCellCount = 0;
	int32 ActiveRoadPieceCount = 0;
};

/** Managed Actor projection layer; authoritative state remains exclusively in HansaSimulation. */
UCLASS(NotBlueprintable)
class HANSA_API AHansaPlacementProjectionManager final : public AActor
{
	GENERATED_BODY()

public:
	AHansaPlacementProjectionManager();

	bool Synchronize(
		const Hansa::Simulation::FHansaSimulationProjection& Projection,
		AHansaLubeckWorldFoundation& Foundation);
	bool ConsumeEvents(
		TConstArrayView<Hansa::Simulation::FHansaDomainEvent> Events,
		const Hansa::Simulation::FHansaSimulationProjection& Projection,
		AHansaLubeckWorldFoundation& Foundation);
	bool RebuildFromProjection(
		const Hansa::Simulation::FHansaSimulationProjection& Projection,
		AHansaLubeckWorldFoundation& Foundation);
	void TearDownProjections();
	void SelectBuilding(Hansa::Simulation::FHansaBuildingId BuildingId);
	void ClearSelection();

	[[nodiscard]] int32 GetProjectionCount() const { return ProjectionActors.Num(); }
	const TSet<FIntPoint>& GetRoadCells() const { return RoadCells; }
	bool IsBoundTo(const AHansaLubeckWorldFoundation& Foundation) const { return BoundFoundation.Get() == &Foundation; }
	[[nodiscard]] AHansaBuildingWorldProjectionActor* FindProjectionActor(
		Hansa::Simulation::FHansaBuildingId BuildingId) const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool SpawnOrUpdate(
		Hansa::Simulation::FHansaBuildingId BuildingId,
		AHansaLubeckWorldFoundation& Foundation);
	void RefreshWarehouseInventories(const Hansa::Simulation::FHansaSimulationProjection& Projection);
	void RemoveActor(Hansa::Simulation::FHansaBuildingId BuildingId);

	Hansa::Game::FHansaPlacementProjectionRegistry Registry;
	TSet<FIntPoint> RoadCells;
    FTransform LastRoadFoundationTransform;
    TArray<TArray<FIntPoint>> RoadRuns;
    TMap<FIntPoint,uint8> RoadRunMasks;
	TMap<Hansa::Simulation::FHansaBuildingId, TWeakObjectPtr<AHansaBuildingWorldProjectionActor>> ProjectionActors;
	TWeakObjectPtr<AHansaLubeckWorldFoundation> BoundFoundation;
	Hansa::Simulation::FHansaBuildingId SelectedBuildingId;
};
