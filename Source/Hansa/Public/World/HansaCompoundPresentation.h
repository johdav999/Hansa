#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Definitions/HansaResidentialCompoundDefinition.h"
#include "HansaCompoundPresentation.generated.h"
class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
/** Cosmetic, no-tick projection and native Details-panel authoring preview. One Actor for the whole parcel. */
UCLASS(Blueprintable)
class HANSA_API AHansaCompoundPresentation : public AActor
{
 GENERATED_BODY()
public:
 AHansaCompoundPresentation();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
 virtual void Destroyed() override;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound preview") TObjectPtr<UHansaResidentialCompoundDefinition> Definition;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound preview", meta=(ClampMin="1",ClampMax="3")) int32 Stage=1;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound preview") FName RoadContext=TEXT("Straight");
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound preview") FString DistrictId;
 UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compound preview") int64 PreviewSeed=1;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compound preview") FString SelectedLayoutId;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compound preview") TArray<FString> Diagnostics;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Compound preview") TArray<FHansaCompoundNode> AccessNodes;
 UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category="Compound preview") TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;
 UFUNCTION(CallInEditor, Category="Compound preview") void RebuildPreview();
 bool ApplyCompound(UHansaResidentialCompoundDefinition* InDefinition,uint64 Seed,int32 InStage,FName Context,const FString& District);
 FBox GetParcelBounds() const;
 void FitGround(bool bSuppressGrass, bool bConnectRoad = true);
 UPROPERTY(Transient, VisibleAnywhere, Category="Ground") TObjectPtr<UProceduralMeshComponent> GroundCoverage;
 UPROPERTY(Transient, VisibleAnywhere, Category="Ground") TObjectPtr<UProceduralMeshComponent> Foundations;
 UPROPERTY(VisibleAnywhere, Category="Ground") int32 GroundSampleCount = 0;
 UPROPERTY(VisibleAnywhere, Category="Ground") double MaximumFoundationDepth = 0;
 UPROPERTY(VisibleAnywhere, Category="Ground") bool bTerrainComplete = false;
 UPROPERTY(Transient) TArray<TObjectPtr<UObject>> GrassExclusionOwners;
private:
 FString AppliedKey;
 FString FittedKey;
 FTransform FittedTransform;
 uint64 AppliedSeed = 0;
 FHansaCompoundComposition GroundComposition;
 struct FInstanceBinding { int32 Batch; int32 Instance; FTransform Authored; int32 CompositionIndex; };
 TArray<FInstanceBinding> InstanceBindings;
 FDelegateHandle LevelAddedHandle,LevelRemovedHandle;
 bool bLastSuppressGrass=false,bLastConnectRoad=true;
 void TerrainLevelChanged(ULevel* Level,UWorld* World);
 void ClearGrass();
 void ClearBatches();
};
