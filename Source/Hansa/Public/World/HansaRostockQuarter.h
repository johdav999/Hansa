#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HansaRostockQuarter.generated.h"

/** Bounded, prebuilt remote quarter. Roles are presentation identities, never building entities. */
UCLASS()
class HANSA_API AHansaRostockQuarter : public AActor
{
    GENERATED_BODY()
public:
    AHansaRostockQuarter();
    virtual void OnConstruction(const FTransform& Transform) override;
    void RefreshTerrainPlacement();
    static double GroundHeight(double Y);
    static FVector VisitOffset() { return FVector(60000,0,0); }
    static FName RoleFor(const UPrimitiveComponent* Component);
    static FText LabelFor(FName Role);
    UPROPERTY(VisibleAnywhere, Category="Hansa|Rostock") TArray<TObjectPtr<class UHierarchicalInstancedStaticMeshComponent>> Modules;
};
