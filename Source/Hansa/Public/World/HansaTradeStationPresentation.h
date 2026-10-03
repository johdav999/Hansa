#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Presence/HansaForeignPresence.h"
#include "HansaTradeStationPresentation.generated.h"

/** Cosmetic view of an existing station. Never creates storage, rights or construction. */
UCLASS()
class HANSA_API AHansaTradeStationPresentation : public AActor
{
 GENERATED_BODY()
public:
 AHansaTradeStationPresentation();
 bool ApplyStation(const Hansa::Simulation::FHansaTradeStationProjection& Station, bool bMerchantOfficeBuilt=false);
 static FVector WarehouseLocation(const Hansa::Simulation::FHansaLeasedPlotState& Lease);
 static FTransform SiteTransform(const UWorld* World);
 virtual void Tick(float DeltaSeconds) override;
 int64 GetStationId() const { return StationId; }
 void SetSelected(bool Selected);
 bool IsSelected() const { return bSelected; }
 UPROPERTY(VisibleAnywhere, Category="Hansa|Station") TObjectPtr<class UStaticMeshComponent> SelectionOutline;
 UPROPERTY(VisibleAnywhere, Category="Hansa|Station") TArray<TObjectPtr<class UStaticMeshComponent>> SelectionCornerSegments;
 UPROPERTY(VisibleAnywhere, Category="Hansa|Station") TObjectPtr<class UStaticMeshComponent> Warehouse;
 UPROPERTY(VisibleAnywhere, Category="Hansa|Station") TObjectPtr<class UChildActorComponent> Dock;
 UPROPERTY(VisibleAnywhere, Category="Hansa|Station") TObjectPtr<class UInstancedStaticMeshComponent> AccessPath;
 UPROPERTY(VisibleAnywhere, Category="Hansa|Station") TObjectPtr<class UStaticMeshComponent> LoadingSkid;
private:
 void RefreshSelection();
 UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> SelectionMaterial;
 UPROPERTY() TObjectPtr<class UBoxComponent> Selection;
 int64 StationId=0;
 bool bSelected=false;
 bool bCurrentOfficeBuilt=false;
 UPROPERTY() TObjectPtr<class UStaticMesh> StationHouseMesh;
 UPROPERTY() TObjectPtr<class UStaticMesh> MerchantOfficeMesh;
 FString DockClassPath;
 double LastDeckHeight=-MAX_dbl;
 TOptional<Hansa::Simulation::FHansaTradeStationProjection> CurrentStation;
 UPROPERTY() TObjectPtr<class UWorldPartitionStreamingSourceComponent> StreamingSource;
};
