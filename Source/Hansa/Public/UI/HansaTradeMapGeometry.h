#pragma once
#include "CoreMinimal.h"

namespace Hansa::UI::TradeGeometry
{
// WGS84 plate carree coordinates, displayed with longitude scaled at 55.5 N.
// Keep this extent identical to Scripts/BuildTradeChart.py. Never clamp a city.
inline FVector2D Project(double Longitude, double Latitude) { return {(Longitude + 1.) / 33., (61. - Latitude) / 11.}; }
inline bool IsLocated(FVector2D P) { return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && P.X >= 0 && P.X <= 1 && P.Y >= 0 && P.Y <= 1; }
struct HANSA_API FCamera
{
 double Zoom = 1.;
 FVector2D Pan = FVector2D::ZeroVector;
 FVector2D ChartSize(FVector2D Size) const;
 FVector2D Point(FVector2D P, FVector2D Size) const;
 void ZoomAt(double Delta, FVector2D Anchor, FVector2D Size);
 void Move(FVector2D Delta, FVector2D Size);
 void Reveal(FVector2D P, FVector2D Size);
 void Reset() { Zoom = 1.; Pan = FVector2D::ZeroVector; }
};
struct FLabelInput { FName Id; FVector2D Point; FVector2D Size; int32 Priority = 0; };
struct FLabel { FName Id; FSlateRect Bounds; };
HANSA_API TArray<FLabel> PlaceLabels(TArray<FLabelInput> Inputs, FVector2D ViewSize);
HANSA_API FName SpatialNeighbor(const TArray<TPair<FName,FVector2D>>& Points, FName Origin, FVector2D Direction);
}
