#pragma once

#include "CoreMinimal.h"
#include "Components/SplineMeshComponent.h"
#include "HansaRoadSplineComponent.generated.h"

class UTexture2D;
class UMaterialInstanceDynamic;

/** A cosmetic road section. Spline grade plus a sampled residual handles cross slopes and junctions. */
UCLASS(ClassGroup=(Hansa), meta=(BlueprintSpawnableComponent))
class HANSA_API UHansaRoadSplineComponent : public USplineMeshComponent
{
    GENERATED_BODY()
public:
    UHansaRoadSplineComponent();
    /** World transform must contain yaw only. Preview clearance is explicit, never accumulated. */
    UFUNCTION(BlueprintCallable, Category="Hansa|Road|Terrain")
    bool FitTerrain(float Clearance = 3.f, bool bPreview = false, bool bForce = false);
    void InvalidateRoadRVT();
    /** Sampled ground, excluding the cosmetic crown; available for diagnostics and acceptance. */
    bool SampleGroundHeight(const FVector& WorldPosition, double& OutZ) const;
    int32 GetSectionCount() const { return ActiveSections; }
    virtual void OnUnregister() override;
    virtual void OnRegister() override;
    virtual void OnVisibilityChanged() override;
    UPROPERTY(EditAnywhere, Category="Hansa|Road|Terrain", meta=(ClampMin="0.5", ClampMax="10", Units="cm", ToolTip="Road surface clearance above sampled terrain; cosmetic only.", HansaAIAccess="Never"))
    float SurfaceClearance = 3.f;
    UPROPERTY(EditAnywhere, Category="Hansa|Road|Terrain", meta=(ClampMin="0.25", ClampMax="5", Units="cm", ToolTip="Maximum centreline error before splitting a straight section; the residual height field handles cross-slopes.", HansaAIAccess="Never"))
    float SplineErrorTolerance = 1.f;
    UPROPERTY(VisibleAnywhere, Transient, Category="Hansa|Road|Terrain") bool bTerrainFitted = false;
    UPROPERTY(VisibleAnywhere, Transient, Category="Hansa|Road|Terrain") int32 LastTraceCount = 0;
    UPROPERTY(VisibleAnywhere, Transient, Category="Hansa|Road|Terrain") double LastFitMilliseconds = 0;
    UPROPERTY(VisibleAnywhere, Transient, Category="Hansa|Road|Terrain") FString FitDiagnostic;
private:
    void LevelChanged(ULevel* Level, UWorld* World);
    UPROPERTY(Transient) TObjectPtr<UTexture2D> HeightTexture;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TerrainMaterial;
    UPROPERTY(Transient) TArray<TObjectPtr<USplineMeshComponent>> Sections;
    TArray<float> SampledHeights;
    int32 ActiveSections = 1;
    FTransform FittedTransform;
    TWeakObjectPtr<UStaticMesh> FittedMesh;
    float FittedClearance = -1;
    bool bFittedPreview = false;
    int32 FittedRVTMode = -1;
    float FittedErrorTolerance = -1;
    FDelegateHandle AddedHandle, RemovedHandle;
};
