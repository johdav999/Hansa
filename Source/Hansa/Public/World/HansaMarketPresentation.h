#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaMarketPresentation.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;

/** Cosmetic civic market. Reviewed assets are assigned by a Blueprint, never here.
 * Equipment and sample cargo are not inventory, prices, or service-coverage state. */
UCLASS(Blueprintable)
class HANSA_API AHansaMarketPresentation : public AActor
{
	GENERATED_BODY()
public:
	AHansaMarketPresentation();
	virtual void OnConstruction(const FTransform& Transform) override;
	void ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus Status);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> CourtHall;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UInstancedStaticMeshComponent> Stalls;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UInstancedStaticMeshComponent> Awnings;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Scales;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UInstancedStaticMeshComponent> Baskets;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Cargo;
private:
	void RebuildInstances();
};
