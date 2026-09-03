#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerStart.h"

#include "HansaLubeckWorldFoundation.generated.h"

class UArrowComponent;
class UBoxComponent;
class UDirectionalLightComponent;
class USceneComponent;
class USkyLightComponent;
class UStaticMeshComponent;

namespace Hansa::Game::LubeckMap
{
	HANSA_API const FString& StableMapId();
	HANSA_API const FName& AutomationStartId();
	HANSA_API FTransform AutomationStartTransform();
	HANSA_API FVector2D CameraBoundsMin();
	HANSA_API FVector2D CameraBoundsMax();
}

/**
 * Deterministic, placeholder-only Lübeck terrain composition for the MVP vertical slice.
 * The fixed boxes deliberately expose land, shore, water, quay, pier and road topology through tags.
 */
UCLASS(Blueprintable)
class HANSA_API AHansaLubeckWorldFoundation : public AActor
{
	GENERATED_BODY()

public:
	AHansaLubeckWorldFoundation();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintPure, Category = "Hansa|World")
	FString GetStableMapId() const { return StableMapId; }

	UFUNCTION(BlueprintPure, Category = "Hansa|World")
	FTransform GetAutomationStartTransform() const;

	UFUNCTION(BlueprintPure, Category = "Hansa|World")
	FVector2D GetCameraBoundsMin() const;

	UFUNCTION(BlueprintPure, Category = "Hansa|World")
	FVector2D GetCameraBoundsMax() const;

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Placement")
	bool WorldToPlacementCell(FVector WorldLocation, int32& OutX, int32& OutY) const;

	UFUNCTION(BlueprintPure, Category = "Hansa|World|Placement")
	FVector PlacementCellToWorld(int32 X, int32 Y, float Height = 100.0f) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World")
	FString StableMapId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World")
	FName StableAutomationStartId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World")
	TObjectPtr<UBoxComponent> CameraBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World")
	TObjectPtr<UArrowComponent> AutomationStart;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Placeholder Lighting")
	TObjectPtr<UDirectionalLightComponent> SunLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World|Placeholder Lighting")
	TObjectPtr<USkyLightComponent> SkyLight;

private:
	UPROPERTY(VisibleAnywhere, Category = "Hansa|World")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Hansa|World")
	TArray<TObjectPtr<UStaticMeshComponent>> TopologyComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> PlaceholderMaterials;

	UPROPERTY()
	TObjectPtr<class UMaterialInterface> PlaceholderBaseMaterial;
};

/** Stable PlayerStart used by manual play and explicitly by automation fixture launch. */
UCLASS()
class HANSA_API AHansaLubeckAutomationStart : public APlayerStart
{
	GENERATED_BODY()

public:
	AHansaLubeckAutomationStart(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World")
	FString StableMapId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hansa|World")
	FName StableStartId;
};
