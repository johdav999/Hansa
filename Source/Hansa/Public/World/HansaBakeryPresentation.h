#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaBakeryPresentation.generated.h"

class UStaticMeshComponent;

/** Read-only bakery roles. Visual stock cues are not inventory quantities. */
UCLASS(Blueprintable)
class HANSA_API AHansaBakeryPresentation : public AActor
{
	GENERATED_BODY()
public:
	AHansaBakeryPresentation();
	void ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus Status);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Bakery;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Construction;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> FlourSack;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> BreadCrate;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> Sign;
};
