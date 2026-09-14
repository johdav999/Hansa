#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Model/HansaSimulationTime.h"
#include "HansaLubeckWorldArt.generated.h"

namespace Hansa::Game::LubeckWorldArt
{
    struct HANSA_API FHansaLightingState
    {
        double SolarHour = 13.0;
        float SolarElevationDegrees = 36.0f;
        float SunYawDegrees = -35.0f;
        float SunIntensityLux = 15000.0f;
        float SunTemperatureKelvin = 5700.0f;
        float SunSourceAngleDegrees = 4.0f;
        float SkyLightIntensity = 1.8f;
        float SkyTemperatureKelvin = 7500.0f;
        float ExposureEV100 = 14.0f;
    };

    inline constexpr float ExposureCompensationStops = 3.8f;
    inline constexpr float AmbientOcclusionIntensity = 0.35f;
    inline constexpr float LumenAmbientOcclusionIntensity = 0.70f;
    inline float ExposureShutterSpeedForEV100(const float EV100) { return FMath::Pow(2.0f, EV100) / 16.0f; }

    // Presentation grading derived from the immutable placement topology, not surveyed elevation.
    HANSA_API double GroundHeight(const FVector2D& Position);
    HANSA_API bool IsDressingLocation(const FVector2D& Position, double Radius);
    /** Continuous, presentation-only Baltic daylight derived from the authoritative simulation calendar. */
    HANSA_API FHansaLightingState EvaluateLighting(
        const Hansa::Simulation::FHansaCalendarProjection& Calendar,
        double PresentationTickFraction,
        uint16 MinutesPerTick);
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
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, Category="Hansa|World|Presentation")
    void SetLightingPreset(bool bEvening);
    UFUNCTION(BlueprintCallable, Category="Hansa|World|Presentation")
    void ClearLightingPresetOverride();

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

private:
    void ApplyLightingState(const Hansa::Game::LubeckWorldArt::FHansaLightingState& State);
    bool ApplyAuthoritativeSimulationLighting();
    bool bLightingPresetOverride = false;
};
