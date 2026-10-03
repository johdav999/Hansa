#pragma once

#include "Components/StaticMeshComponent.h"
#include "World/HansaTerrainPlacement.h"

namespace Hansa::Game::BuildingSelection
{
// Shared approved footprint brackets and diamond, including streamed terrain clearance.
inline void ConfigureFootprint(UWorld* World, UStaticMeshComponent* SelectionOutline,
    const TArray<TObjectPtr<UStaticMeshComponent>>& SelectionCornerSegments,
    const double Width, const double Depth, const double GroundZ)
{
	const double HalfWidth = (Width + 40.0) * 0.5;
	const double HalfDepth = (Depth + 40.0) * 0.5;
	const double BracketLength = FMath::Clamp(FMath::Min(Width, Depth) * 0.18, 70.0, 140.0);
	const double BracketThickness = 12.0;
	const double MarkerHeight = 6.0;
	// The building is grounded at its centre, but its footprint may cross a slope.
	// Lift each complete L above both bars' terrain samples, including their edges.
	// Keep the authored datum as a floor for decks and worlds without terrain.
	const auto TerrainLift = [World](const UStaticMeshComponent* Marker)
	{
		double Lift = 0.0;
		for (const double X : { -50.0, 0.0, 50.0 })
		{
			for (const double Y : { -50.0, 0.0, 50.0 })
			{
				const FVector Bottom = Marker->GetComponentTransform().TransformPosition(FVector(X, Y, -50.0));
				FHitResult Hit;
				if (Hansa::Game::TerrainPlacement::Trace(World, Bottom + FVector(0, 0, 1000000),
					Bottom - FVector(0, 0, 1000000), Hit))
				{
					Lift = FMath::Max(Lift, Hit.ImpactPoint.Z + 2.0 - Bottom.Z);
				}
			}
		}
		return Lift;
	};

	int32 SegmentIndex = 0;
	for (const double XSign : { -1.0, 1.0 })
	{
		for (const double YSign : { -1.0, 1.0 })
		{
			UStaticMeshComponent* Horizontal = SelectionCornerSegments[SegmentIndex++];
			Horizontal->SetRelativeLocation(FVector(
				XSign * (HalfWidth - BracketLength * 0.5), YSign * HalfDepth, GroundZ));
			Horizontal->SetRelativeRotation(FRotator::ZeroRotator);
			Horizontal->SetRelativeScale3D(FVector(
				BracketLength / 100.0, BracketThickness / 100.0, MarkerHeight / 100.0));

			UStaticMeshComponent* Vertical = SelectionCornerSegments[SegmentIndex++];
			Vertical->SetRelativeLocation(FVector(
				XSign * HalfWidth, YSign * (HalfDepth - BracketLength * 0.5), GroundZ));
			Vertical->SetRelativeRotation(FRotator::ZeroRotator);
			Vertical->SetRelativeScale3D(FVector(
				BracketThickness / 100.0, BracketLength / 100.0, MarkerHeight / 100.0));
			const FVector Lift(0.0, 0.0, FMath::Max(TerrainLift(Horizontal), TerrainLift(Vertical)));
			Horizontal->AddWorldOffset(Lift);
			Vertical->AddWorldOffset(Lift);
		}
	}

	// SelectionOutline remains the stable public/test component, but is now the small
	// non-colour diamond cue from the approved reference rather than an opaque slab.
	SelectionOutline->SetRelativeLocation(FVector(0.0, -HalfDepth - 48.0, GroundZ));
	SelectionOutline->SetRelativeRotation(FRotator(0.0, 45.0, 0.0));
	SelectionOutline->SetRelativeScale3D(FVector(0.34, 0.34, MarkerHeight / 100.0));
	SelectionOutline->AddWorldOffset(FVector(0.0, 0.0, TerrainLift(SelectionOutline)));

}
}
