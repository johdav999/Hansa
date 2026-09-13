#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaHarborPresentation.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class USceneComponent;

/** Modular cosmetic dock. Origin is the deck/placement datum, not the submerged pile bottom.
 * +X faces water. Berth markers are alignment targets only, never route or inventory authority. */
UCLASS(Blueprintable)
class HANSA_API AHansaHarborPresentation : public AActor
{
    GENERATED_BODY()
public:
    AHansaHarborPresentation();
    virtual void OnConstruction(const FTransform& Transform) override;
    void ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus Status);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<UInstancedStaticMeshComponent> PierDeck;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<UInstancedStaticMeshComponent> QuayEdge;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<UInstancedStaticMeshComponent> QuayCorner;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<UStaticMeshComponent> Steps;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<UInstancedStaticMeshComponent> Moorings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<UStaticMeshComponent> Hoist;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<UStaticMeshComponent> Cargo;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<USceneComponent> Berth;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Harbor") TObjectPtr<USceneComponent> LoadPoint;
};
