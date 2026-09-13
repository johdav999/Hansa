#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HansaLubeckWorldArt.generated.h"

namespace Hansa::Game::LubeckWorldArt
{
    // Presentation grading derived from the immutable placement topology, not surveyed elevation.
    HANSA_API double GroundHeight(const FVector2D& Position);
    HANSA_API bool IsDressingLocation(const FVector2D& Position, double Radius);
    inline constexpr double WaterHeight = -125.0;
}

/** Read-only environmental presentation. Never owns buildings, cargo or placement state. */
UCLASS()
class HANSA_API AHansaLubeckWorldArt : public AActor
{
    GENERATED_BODY()
public:
    AHansaLubeckWorldArt();
    virtual void OnConstruction(const FTransform& Transform) override;
    UFUNCTION(BlueprintCallable, Category="Hansa|World|Presentation")
    void SetLightingPreset(bool bEvening);

    UPROPERTY(VisibleAnywhere, Category="Hansa|World")
    TObjectPtr<class UDirectionalLightComponent> Sun;
    UPROPERTY(VisibleAnywhere, Category="Hansa|World")
    TObjectPtr<class USkyLightComponent> Sky;
    UPROPERTY(VisibleAnywhere, Category="Hansa|World")
    TObjectPtr<class USkyAtmosphereComponent> Atmosphere;
    UPROPERTY(VisibleAnywhere, Category="Hansa|World")
    TObjectPtr<class UExponentialHeightFogComponent> Fog;
    UPROPERTY(VisibleAnywhere, Category="Hansa|World")
    TObjectPtr<class UPostProcessComponent> Exposure;
    UPROPERTY(VisibleAnywhere, Category="Hansa|World")
    TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Quay;
    UPROPERTY(VisibleAnywhere, Category="Hansa|World")
    TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Moorings;
};
