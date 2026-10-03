#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/HansaLandOverlayGeometry.h"
#include "HansaLandOverlayRenderer.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class AHansaLubeckWorldFoundation;

struct HANSA_API FHansaLandOverlayOptions
{
    Hansa::Game::LandOverlay::EMode Mode = Hansa::Game::LandOverlay::EMode::Buildable;
    /** Stable cell footprint; chunk boundaries never act as selection identities. */
    TOptional<FIntPoint> SelectedCell;
    TSharedPtr<const TArray<Hansa::Game::LandOverlay::FBoundary>> SelectionBoundaries;
    uint64 SelectionRevision=0;
    int32 SurveyStride=1;
    /** Caller derives this from camera scale; quantize updates to avoid zoom-frame rebuilding. */
    float LineWidthCm = 12.f;
    bool bHighContrast = false;
};

struct HANSA_API FHansaLandOverlayStats
{
    int32 ActiveChunks = 0;
    int32 PendingChunks = 0;
    int32 PooledComponents = 0;
    int32 Triangles = 0;
    int32 MissingTerrainSamples = 0;
    uint64 Rebuilds = 0;
};

/** Presentation-only pooled geometry. Consumes scoped query copies; never accesses mutable simulation state. */
UCLASS(NotBlueprintable, Transient)
class HANSA_API AHansaLandOverlayRenderer final : public AActor
{
    GENERATED_BODY()
public:
    AHansaLandOverlayRenderer();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;

    /** Core is at most 32x32 cells; Query includes a one-cell halo. Limit: 64 visible chunks. */
    bool ApplyChunk(FIntPoint ChunkId, const Hansa::Simulation::FHansaLandQueryResult& Query,
        FIntPoint CoreMin, FIntPoint CoreMax, const FTransform& GridToWorld,
        const FHansaLandOverlayOptions& Options);
    void RetainChunks(TConstArrayView<FIntPoint> VisibleChunkIds);
    void Clear();
    void InvalidateTerrain();
    FHansaLandOverlayStats GetStats() const;
    bool IsDisplayingCity(const FString& CityId) const;
    bool HasGroundMaterial() const { return FillMaterial.LoadSynchronous() != nullptr && RibbonMaterial.LoadSynchronous() != nullptr; }
    static bool MakeGridTransform(Hansa::Simulation::FHansaCityDefinitionId City,
        const AHansaLubeckWorldFoundation& Foundation, FTransform& OutTransform);

private:
    struct FChunk
    {
        Hansa::Simulation::FHansaLandQueryResult Query;
        FIntPoint Min, Max;
        FTransform GridToWorld;
        FHansaLandOverlayOptions Options;
        uint64 Digest = 0;
        int32 PoolIndex = INDEX_NONE;
        int32 Triangles = 0;
        int32 Missing = 0;
        bool bDirty = true;
    };
    void Rebuild(FChunk& Chunk);
    void OnLevelChanged(ULevel* Level, UWorld* World);
    TMap<FIntPoint, FChunk> Chunks;
    TArray<FIntPoint> BuildQueue;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Pool;
    // Soft defaults remain cook-discoverable without rooting material expressions
    // during class construction, which would prevent editor regeneration.
    UPROPERTY() TSoftObjectPtr<UMaterialInterface> FillMaterial;
    UPROPERTY() TSoftObjectPtr<UMaterialInterface> RibbonMaterial;
    uint64 RebuildCount = 0;
    uint64 LastRebuildFrame = MAX_uint64;
    FDelegateHandle LevelAddedHandle, LevelRemovedHandle;
};
