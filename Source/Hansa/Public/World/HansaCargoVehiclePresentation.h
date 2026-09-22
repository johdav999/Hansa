#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Trade/HansaTrade.h"
#include "Logistics/HansaLocalLogistics.h"
#include "HansaCargoVehiclePresentation.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UBoxComponent;

/** Read-only vehicle skin. +X is forward; sea origin is waterline, land origin is ground.
 * Never owns cargo, advances a route, or submits a logistics command. */
UCLASS(Blueprintable)
class HANSA_API AHansaCargoVehiclePresentation : public AActor
{
    GENERATED_BODY()
public:
    AHansaCargoVehiclePresentation();
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hansa|Vehicle") bool bSeaVehicle = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TObjectPtr<UStaticMeshComponent> Rig;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TObjectPtr<UStaticMeshComponent> Sail;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TObjectPtr<UStaticMeshComponent> FurledSail;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TObjectPtr<UStaticMeshComponent> Cargo;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TArray<TObjectPtr<UStaticMeshComponent>> Wheels;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TObjectPtr<USceneComponent> LoadPoint;

    bool ApplyVehicle(const Hansa::Simulation::FHansaVehicleProjection& Vehicle,
        const Hansa::Simulation::FHansaRouteProjection* Route = nullptr);
    bool ApplyLocalDelivery(const Hansa::Simulation::FHansaLogisticsJobProjection& Job);
    /** Absolute distance supplied by a world projection; repeating a snapshot cannot accumulate rotation. */
    void SetWheelTravelDistance(double Centimetres);
    /** Eased shortest-arc yaw driven by simulation tick + fraction, including pause/speed. */
    void SampleHeading(TOptional<FRotator> Heading, double PresentationTime);
    /** Command ticks change state without advancing displayed motion. */
    void RebaseHeadingClock(double PresentationTime);
    void ClearProjection();
    void SetSelected(bool bSelected);
    /** Non-colliding waterline selection cue, editable in Blueprint Details. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle|Selection") TObjectPtr<UStaticMeshComponent> SelectionMarker;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") TObjectPtr<UBoxComponent> Selection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|Vehicle") FName SemanticId;
    Hansa::Simulation::FHansaVehicleId GetVehicleId() const { return VehicleId; }
    Hansa::Simulation::FHansaLogisticsJobId GetLogisticsJobId() const { return LogisticsJobId; }
    Hansa::Simulation::FHansaGoodId GetCargoGoodId() const { return CargoGoodId; }
    int64 GetCargoMilliUnits() const { return CargoMilliUnits; }

private:
    void SetCargoVisible(bool bVisible);
    Hansa::Simulation::FHansaVehicleId VehicleId;
    Hansa::Simulation::FHansaLogisticsJobId LogisticsJobId;
    Hansa::Simulation::FHansaGoodId CargoGoodId;
    int64 CargoMilliUnits = 0;
    bool bHeadingInitialized = false;
    double HeadingStartYaw = 0;
    double HeadingTargetYaw = 0;
    double HeadingStartTime = 0;
    double HeadingLastTime = 0;
    double HeadingDuration = 0;
};
