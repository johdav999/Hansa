#pragma once

#include "CoreMinimal.h"
#include "Queries/HansaLandQuery.h"

namespace Hansa::Game::LandOverlay
{
enum class EMode : uint8 { Off, Buildable, Ownership };
enum class ESurface : uint8 { Permitted, Conditional, Restricted, Unknown, Ownership, Hidden };

struct HANSA_API FStyleKey
{
    ESurface Surface = ESurface::Unknown;
    uint64 Owner = 0;
    bool operator==(const FStyleKey& Other) const { return Surface == Other.Surface && Owner == Other.Owner; }
};

/** Integer grid corners, not cell centres. Closed paths include their first point again. */
struct HANSA_API FBoundary
{
    TArray<FIntPoint> Points;
    bool bClosed = false;
};

struct HANSA_API FRegion
{
    FStyleKey Style;
    TArray<FIntPoint> Cells;
    TArray<FBoundary> Boundaries;
    bool bSelected = false;
};

struct HANSA_API FGeometry
{
    TArray<FRegion> Regions;
    int32 UnitEdgeCount = 0;
    bool bValid = false;
};

/** Shared cross-sections for a continuous terrain ribbon. UV phase is anchored to
 * grid position, so independently built chunks cannot restart a dash pattern. */
struct HANSA_API FRibbonSection
{
    FVector2D Left, Right;
    double Phase = 0;
};

/** Exact orthogonal cell contours: bounded miter joins, no curve fitting or change
 * to the legal centreline. Closed paths may repeat their first point. */
HANSA_API TArray<FRibbonSection> BuildRibbon(TConstArrayView<FVector2D> Points,
    bool bClosed, double Width, double MaximumStep = .25);

/** Quantized ground width targeting a readable core at the camera focus. */
HANSA_API float RibbonWidthForView(float Distance, float HorizontalFovDegrees,
    int32 ViewportWidth, float PitchDegrees, float CorePixels = 2.5f);

/** A one-cell halo around the core suppresses artificial boundaries at rendering chunk seams. */
HANSA_API FGeometry Build(const Hansa::Simulation::FHansaLandQueryResult& Query,
    FIntPoint CoreMin, FIntPoint CoreMax, EMode Mode, TOptional<FIntPoint> SelectedCell = {});
HANSA_API FStyleKey Classify(const Hansa::Simulation::FHansaLandCellView& Cell, EMode Mode);
/** Exact selected component contours from compact runs, including holes. */
HANSA_API TArray<FBoundary> SurveySelectionBoundaries(
    TConstArrayView<Hansa::Simulation::FHansaLandCellView> Runs, int32 RegionId);
}
