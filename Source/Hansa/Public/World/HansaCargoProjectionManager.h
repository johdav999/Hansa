#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "HansaCargoProjectionManager.generated.h"

class AHansaCargoVehiclePresentation;
class AHansaLubeckWorldFoundation;
class UHansaRuntimeSimulationHost;

UENUM(BlueprintType)
enum class EHansaCargoWorldPhase : uint8 { Berthed, Loading, Departing, Traveling, Arriving, Unloading, AwaitingPickup, PickupPaused, Delivered, Cancelled, DeliveryPaused };

/** An observation, never an inventory or command target. Quantities are milli-units. */
USTRUCT(BlueprintType)
struct HANSA_API FHansaCargoWorldObservation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName SemanticId;
    UPROPERTY(BlueprintReadOnly) FName CityId;
    UPROPERTY(BlueprintReadOnly) FString VehicleId;
    UPROPERTY(BlueprintReadOnly) FString RouteId;
    UPROPERTY(BlueprintReadOnly) FString JobId;
    UPROPERTY(BlueprintReadOnly) FString RequestId;
    UPROPERTY(BlueprintReadOnly) FString CargoInventoryId;
    UPROPERTY(BlueprintReadOnly) FString SourceInventoryId;
    UPROPERTY(BlueprintReadOnly) FString DestinationInventoryId;
    UPROPERTY(BlueprintReadOnly) FString SourceBuildingId;
    UPROPERTY(BlueprintReadOnly) FString DestinationBuildingId;
    UPROPERTY(BlueprintReadOnly) FName SourceBuildingDefinitionId;
    UPROPERTY(BlueprintReadOnly) FName DestinationBuildingDefinitionId;
    UPROPERTY(BlueprintReadOnly) FName GoodId;
    UPROPERTY(BlueprintReadOnly) int64 QuantityMilliUnits = 0;
    UPROPERTY(BlueprintReadOnly) int64 CargoMilliUnits = 0;
    UPROPERTY(BlueprintReadOnly) int64 TransferMilliUnits = 0;
    UPROPERTY(BlueprintReadOnly) int64 TransferTick = -1;
    UPROPERTY(BlueprintReadOnly) int64 SimulationTick = 0;
    UPROPERTY(BlueprintReadOnly) int32 RoadDistanceCells = 0;
    UPROPERTY(BlueprintReadOnly) int32 ElapsedTravelTicks = 0;
    UPROPERTY(BlueprintReadOnly) int32 RemainingTravelTicks = 0;
    UPROPERTY(BlueprintReadOnly) FName LogisticsStatus;
    UPROPERTY(BlueprintReadOnly) FName PauseReason;
    UPROPERTY(BlueprintReadOnly) EHansaCargoWorldPhase Phase = EHansaCargoWorldPhase::Berthed;
    UPROPERTY(BlueprintReadOnly) double Progress = 0;
    UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) bool bVisible = false;
    UPROPERTY(BlueprintReadOnly) FString PresentationFailure;
};

/** Non-replicated projection owner. Its expandable pool never feeds actor state back into simulation. */
UCLASS(NotBlueprintable)
class HANSA_API AHansaCargoProjectionManager final : public AActor
{
    GENERATED_BODY()
public:
    AHansaCargoProjectionManager();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Synchronize(const Hansa::Simulation::FHansaSimulationProjection& Projection,
        UHansaRuntimeSimulationHost& Host, AHansaLubeckWorldFoundation& Foundation);
    void Sample(double TickFraction);
    void SetRostockVisible(bool bVisible);
    UFUNCTION(BlueprintPure, Category="Hansa|World|Cargo") TArray<FHansaCargoWorldObservation> QueryCargo() const;
    UFUNCTION(BlueprintCallable, Category="Hansa|World|Cargo") bool SelectCargo(FName SemanticId);
    UFUNCTION(BlueprintPure, Category="Hansa|World|Cargo") FName GetSelectedCargo() const { return Selected; }
    UFUNCTION(BlueprintPure, Category="Hansa|World|Cargo") int32 GetActiveLocalWagonCount() const;
    UFUNCTION(BlueprintPure, Category="Hansa|World|Cargo") int32 GetPooledLocalWagonCount() const;
    UFUNCTION(BlueprintPure, Category="Hansa|World|Cargo") int32 GetPeakLocalWagonCount() const { return PeakLocalWagons; }
    void ClearSelection();
    const FHansaCargoWorldObservation* FindObservation(FName Id) const;
    AHansaCargoVehiclePresentation* FindActor(FName Id) const;
    static bool BuildRoadPath(const Hansa::Simulation::FHansaSimulationProjection& Projection,
        const Hansa::Simulation::FHansaLogisticsJobProjection& Job, TArray<FIntPoint>& OutPath);
    static FVector SamplePath(TConstArrayView<FVector> Path, double Progress, double& OutDistance, FRotator& OutHeading);
private:
    struct FEntry
    {
        FHansaCargoWorldObservation Observation;
        TArray<FVector> Path;
        double StartProgress = 0;
        double ProgressPerTick = 0;
        double LaneStart = 0;
        double LaneScale = 1;
        double LateralOffsetCentimetres = 0;
        double LongitudinalOffsetCentimetres = 0;
    };
    void ResetActors();
    void ReleaseActor(FName SemanticId);
    TArray<FEntry> Entries;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<AHansaCargoVehiclePresentation>> Actors;
    UPROPERTY(Transient) TArray<TObjectPtr<AHansaCargoVehiclePresentation>> SeaPool;
    UPROPERTY(Transient) TArray<TObjectPtr<AHansaCargoVehiclePresentation>> LocalWagonPool;
    UPROPERTY(Transient) TWeakObjectPtr<UHansaRuntimeSimulationHost> RuntimeHost;
    /** Guards corrupt projections while allowing every representative-city delivery to render. */
    UPROPERTY(EditDefaultsOnly, Category="Hansa|World|Cargo", meta=(ClampMin="1")) int32 MaximumSeaVehicleActors = 8;
    UPROPERTY(EditDefaultsOnly, Category="Hansa|World|Cargo", meta=(ClampMin="1")) int32 MaximumLocalWagonActors = 128;
    int32 PeakLocalWagons = 0;
    FName Selected;
    bool bRostockVisible = false;
};
