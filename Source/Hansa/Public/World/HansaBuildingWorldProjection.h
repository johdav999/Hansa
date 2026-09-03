#pragma once

#include "CoreMinimal.h"
#include "Events/HansaDomainEvent.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"

#include "HansaBuildingWorldProjection.generated.h"

class AHansaLubeckWorldFoundation;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

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

/** One managed, non-authoritative world representation of a placed building or road. */
UCLASS(NotBlueprintable)
class HANSA_API AHansaBuildingWorldProjectionActor final : public AActor
{
	GENERATED_BODY()

public:
	AHansaBuildingWorldProjectionActor();

	void ApplyProjection(
		const Hansa::Simulation::FHansaBuildingWorldProjection& Projection,
		const AHansaLubeckWorldFoundation& Foundation);
	void SetSelected(bool bInSelected);

	[[nodiscard]] Hansa::Simulation::FHansaBuildingId GetBuildingId() const { return BuildingId; }
	[[nodiscard]] const FString& GetBuildingDefinitionId() const { return BuildingDefinitionId; }
	[[nodiscard]] Hansa::Simulation::EHansaBuildingWorldStatus GetWorldStatus() const { return WorldStatus; }
	[[nodiscard]] bool IsSelected() const { return bSelected; }
	[[nodiscard]] bool IsRoad() const { return bRoad; }

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
	TObjectPtr<UStaticMeshComponent> ConstructionPlaceholder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UStaticMeshComponent> SelectionOutline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Projection")
	TObjectPtr<UStaticMeshComponent> StatusMarker;

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
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	Hansa::Simulation::FHansaBuildingId BuildingId;
	FString BuildingDefinitionId;
	Hansa::Simulation::EHansaBuildingWorldStatus WorldStatus =
		Hansa::Simulation::EHansaBuildingWorldStatus::UnderConstruction;
	bool bSelected = false;
	bool bRoad = false;
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
	[[nodiscard]] AHansaBuildingWorldProjectionActor* FindProjectionActor(
		Hansa::Simulation::FHansaBuildingId BuildingId) const;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool SpawnOrUpdate(
		Hansa::Simulation::FHansaBuildingId BuildingId,
		AHansaLubeckWorldFoundation& Foundation);
	void RemoveActor(Hansa::Simulation::FHansaBuildingId BuildingId);

	Hansa::Game::FHansaPlacementProjectionRegistry Registry;
	TMap<Hansa::Simulation::FHansaBuildingId, TWeakObjectPtr<AHansaBuildingWorldProjectionActor>> ProjectionActors;
	TWeakObjectPtr<AHansaLubeckWorldFoundation> BoundFoundation;
	Hansa::Simulation::FHansaBuildingId SelectedBuildingId;
};
