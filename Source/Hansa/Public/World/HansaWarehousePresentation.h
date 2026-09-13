#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaWarehousePresentation.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

/** Read-only storage presentation. Cargo groups indicate occupancy, never units or good identity.
 * Reviewed meshes are assigned by a presentation Blueprint; native defaults have no asset dependencies. */
UCLASS(Blueprintable)
class HANSA_API AHansaWarehousePresentation : public AActor
{
	GENERATED_BODY()
public:
	AHansaWarehousePresentation();
	virtual void OnConstruction(const FTransform& Transform) override;
	void ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus Status);
	void ApplyInventory(const Hansa::Simulation::FHansaInventoryProjection* Inventory);
	[[nodiscard]] int32 GetCargoGroupCount() const { return CargoGroupCount; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation") TObjectPtr<UStaticMeshComponent> Storehouse;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation") TObjectPtr<UStaticMeshComponent> Hoist;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation") TObjectPtr<UInstancedStaticMeshComponent> Doors;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation") TObjectPtr<UInstancedStaticMeshComponent> Skids;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation") TObjectPtr<UInstancedStaticMeshComponent> Cargo;
private:
	static FVector StagingLocation(int32 Index);
	void RefreshCargo();
	int32 CargoGroupCount = 0;
	bool bComplete = true;
};
