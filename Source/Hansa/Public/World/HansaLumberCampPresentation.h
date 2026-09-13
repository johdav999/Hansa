#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaLumberCampPresentation.generated.h"

class UStaticMeshComponent;

/** Bounded art roles, configured by the reviewed presentation Blueprint.
 * No asset defaults point at drafts; no component owns simulation state. */
UCLASS(Blueprintable)
class HANSA_API AHansaLumberCampPresentation : public AActor
{
	GENERATED_BODY()
public:
	AHansaLumberCampPresentation();
	void ApplyStatus(Hansa::Simulation::EHansaBuildingWorldStatus Status);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> WorkBuilding;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> LogPile;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> CutTimber;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> ToolShelter;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> StumpSlash;
};
