#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HansaResidencePresentation.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/** Bounded cosmetic residence variation. Tier identity remains with the building definition. */
UCLASS(Blueprintable)
class HANSA_API AHansaResidencePresentation : public AActor
{

	GENERATED_BODY()
public:
	AHansaResidencePresentation();
	virtual void OnConstruction(const FTransform& Transform) override;
	void ApplyParcel(int32 X, int32 Y);
	static int32 VariantForParcel(int32 X, int32 Y);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMeshComponent> ResidenceMesh;

	/** Reviewed meshes share footprint, ground pivot and +X entrance; no auto-fit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMesh> VariantA;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Presentation")
	TObjectPtr<UStaticMesh> VariantB;

private:
	int32 SelectedVariant = 0;
	void RefreshMesh();
};
