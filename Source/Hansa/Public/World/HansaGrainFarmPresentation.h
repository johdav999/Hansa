#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaGrainFarmPresentation.generated.h"

class UStaticMeshComponent;

/** Presentation only: all roles reconstruct from the existing simulation status. */
UCLASS(Blueprintable)
class HANSA_API AHansaGrainFarmPresentation : public AActor
{
	GENERATED_BODY()
public:
	AHansaGrainFarmPresentation();
	void ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus Status);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Farm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Construction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> WorkProps;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TArray<TObjectPtr<UStaticMeshComponent>> Fields;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TArray<TObjectPtr<UStaticMeshComponent>> Furrows;
};
