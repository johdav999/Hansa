#pragma once
#include "CoreMinimal.h"
#include "Queries/HansaLandQuery.h"

class APlayerController;
namespace Hansa::Game::LandOverlay
{
/** Session-only, atomically assembled run survey. Never part of saved gameplay data. */
class HANSA_API FSurveyView
{
public:
    bool AcceptPage(int32 Page, const Hansa::Simulation::FHansaLandQueryResult& Reply);
    void Reset();
    int32 NextPage() const { return PendingPage; }
    int32 PageCount() const { return Pending.SurveyPages>0?Pending.SurveyPages:1; }
    const Hansa::Simulation::FHansaLandQueryResult& Get() const { return Published; }
    bool IsReady() const { return Published.SurveyPages>0; }
    Hansa::Simulation::FHansaLandQueryResult Extract(FIntPoint Min, FIntPoint Max, int32 Stride=1) const;
    int32 RegionAt(FIntPoint Cell) const;
    FBox2D RegionBounds(int32 Region) const;
private:
    Hansa::Simulation::FHansaLandQueryResult Published, Pending;
    int32 PendingPage=0;
};

/** Ground footprint shared by coverage and minimap. Rays only hit terrain. */
HANSA_API TArray<FVector> CameraGroundFootprint(APlayerController* Controller, double FallbackHeight);
/** Aligned, spatial chunk identities; conservative ground bounds plus one prefetch chunk. */
HANSA_API TArray<FIntPoint> VisibleSurveyChunks(TConstArrayView<FVector> Footprint,
    const FTransform& Grid, FIntPoint SurveyMin, FIntPoint SurveyMax, FIntPoint Focus, int32 ChunkSize=30);
HANSA_API FVector2D WorldToLandMap(FVector2D World, FVector2D Center, double Span);
HANSA_API FVector2D LandMapToWorld(FVector2D Map, FVector2D Center, double Span);
}
