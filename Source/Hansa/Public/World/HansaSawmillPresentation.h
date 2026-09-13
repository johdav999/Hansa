#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaSawmillPresentation.generated.h"

class UStaticMeshComponent;

/** Read-only, manual-power saw yard. Assets are assigned by a reviewed Blueprint.
 * No provider/staging paths, recipe ownership, animation or inventory counters. */
UCLASS(Blueprintable)
class HANSA_API AHansaSawmillPresentation : public AActor
{
	GENERATED_BODY()
public:
	AHansaSawmillPresentation();
	void ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus Status);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> WorkBuilding;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> LogInput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> PlankOutput;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Rack;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> SawWork;
};
