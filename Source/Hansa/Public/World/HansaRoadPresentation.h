#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/HansaRoadTopology.h"
#include "HansaRoadPresentation.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AHansaLubeckWorldFoundation;

/** Cosmetic 4m road tile. Neighbor ports are derived, never persisted simulation state. */
UCLASS(Blueprintable)
class HANSA_API AHansaRoadPresentation : public AActor
{
    GENERATED_BODY()
public:
    AHansaRoadPresentation();
    virtual void OnConstruction(const FTransform& Transform) override;
    UStaticMesh* MeshForMask(uint8 Mask) const;
    void ApplyNeighbors(uint8 Mask);
    UFUNCTION(BlueprintCallable, Category="Hansa|Road") void SetWetness(float Value);
    void ApplyGround(const AHansaLubeckWorldFoundation& Foundation);
    static void ConfigureGroundMaterial(UMaterialInstanceDynamic& Material, const AHansaLubeckWorldFoundation& Foundation, const FVector* SamplePosition = nullptr);
    static double GroundBaseHeight();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hansa|Road", meta=(ClampMin="0", ClampMax="1", ToolTip="Cosmetic weather response only; never changes road simulation state.")) float Wetness = 0;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Road") TObjectPtr<UStaticMeshComponent> Surface;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Road", meta=(ToolTip="Native 4m cell, no ports; approved production mesh only.")) TObjectPtr<UStaticMesh> Isolated;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Road", meta=(ToolTip="Native 4m cell, canonical +X port.")) TObjectPtr<UStaticMesh> End;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Road", meta=(ToolTip="Native 4m cell, canonical +X/-X ports.")) TObjectPtr<UStaticMesh> Straight;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Road", meta=(ToolTip="Native 4m cell, canonical +X/+Y ports.")) TObjectPtr<UStaticMesh> Corner;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Road", meta=(ToolTip="Native 4m cell, canonical +X/+Y/-X ports.")) TObjectPtr<UStaticMesh> TJunction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Road", meta=(ToolTip="Native 4m cell, all four ports.")) TObjectPtr<UStaticMesh> Crossroads;
private:
    UPROPERTY() TObjectPtr<AHansaLubeckWorldFoundation> GroundFoundation;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WetMaterial;
    // Serialized only for standalone authoring/review actors; live projections always rederive it.
    UPROPERTY() uint8 NeighborMask = 0;
};
