#include "World/HansaStrategyCameraModel.h"

namespace Hansa::Game
{
	bool FHansaStrategyCameraState::IsFinite() const
	{
		return FMath::IsFinite(Focus.X) && FMath::IsFinite(Focus.Y) &&
			FMath::IsFinite(YawDegrees) && FMath::IsFinite(PitchDegrees) && FMath::IsFinite(ZoomDistance);
	}

	bool FHansaStrategyCameraIntent::IsFinite() const
	{
		return FMath::IsFinite(Pan.X) && FMath::IsFinite(Pan.Y) &&
            FMath::IsFinite(PanDisplacement.X) && FMath::IsFinite(PanDisplacement.Y) &&
			FMath::IsFinite(Rotate) && FMath::IsFinite(YawDisplacement) &&
			FMath::IsFinite(PitchDisplacement) && FMath::IsFinite(ZoomSteps);
	}

	bool FHansaStrategyCameraSettings::IsValid() const
	{
		return BoundsMin.X <= BoundsMax.X && BoundsMin.Y <= BoundsMax.Y &&
			PanUnitsPerSecond >= 0.0f && FastPanMultiplier >= 1.0f &&
			RotationDegreesPerSecond >= 0.0f && ZoomUnitsPerStep >= 0.0f &&
			MinimumZoomDistance > 0.0f && MinimumZoomDistance <= MaximumZoomDistance &&
			FMath::IsFinite(MinimumPitchDegrees) && FMath::IsFinite(MaximumPitchDegrees) &&
			MinimumPitchDegrees > -90.0f && MaximumPitchDegrees < 0.0f &&
			MinimumPitchDegrees <= MaximumPitchDegrees;
	}

	float FHansaStrategyCameraModel::YawPointerDisplacement(const FVector2D& PointerDelta)
	{
		return PointerDelta.ContainsNaN() ? 0.0f : PointerDelta.X;
	}

	bool FHansaStrategyCameraModel::ShouldContinuePointerDrag(
		const bool bButtonDown,
		const bool bApplicationReady,
		const bool bApplicationActive,
		const bool bPointerInsideViewport)
	{
		return bButtonDown && bApplicationReady && bApplicationActive && bPointerInsideViewport;
	}

	FHansaStrategyCameraState FHansaStrategyCameraModel::Advance(
		const FHansaStrategyCameraState& Current,
		const FHansaStrategyCameraIntent& Intent,
		const FHansaStrategyCameraSettings& Settings,
		const float DeltaSeconds)
	{
		if (!Current.IsFinite() || !Intent.IsFinite() || !Settings.IsValid() ||
			!FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
		{
			return Current;
		}

		FHansaStrategyCameraState Next = Current;
		FVector2D Pan = Intent.Pan.GetClampedToMaxSize(1.0f);
		const float YawRadians = FMath::DegreesToRadians(Current.YawDegrees);
		const FVector2D Forward(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
		const FVector2D Right(-Forward.Y, Forward.X);
		const float PanMultiplier = Intent.bFastPan ? Settings.FastPanMultiplier : 1.0f;
		Next.Focus += (Right * Pan.X + Forward * Pan.Y) *
			Settings.PanUnitsPerSecond * PanMultiplier * DeltaSeconds;
        // Pointer deltas are distances, independent of frame time and Shift.
        Next.Focus += Right * Intent.PanDisplacement.X + Forward * Intent.PanDisplacement.Y;
		Next.Focus.X = FMath::Clamp(Next.Focus.X, Settings.BoundsMin.X, Settings.BoundsMax.X);
		Next.Focus.Y = FMath::Clamp(Next.Focus.Y, Settings.BoundsMin.Y, Settings.BoundsMax.Y);

		const float RotationIntent = FMath::Clamp(Intent.Rotate, -1.0f, 1.0f);
		Next.YawDegrees = FMath::UnwindDegrees(
			Next.YawDegrees + Intent.YawDisplacement + RotationIntent * Settings.RotationDegreesPerSecond * DeltaSeconds);
		Next.ZoomDistance = FMath::Clamp(
			Next.ZoomDistance - Intent.ZoomSteps * Settings.ZoomUnitsPerStep,
			Settings.MinimumZoomDistance,
			Settings.MaximumZoomDistance);
		Next.PitchDegrees = FMath::Clamp(
			Next.PitchDegrees + Intent.PitchDisplacement,
			Settings.MinimumPitchDegrees, Settings.MaximumPitchDegrees);
		return Next;
	}
}
