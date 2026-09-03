#pragma once

#include "CoreMinimal.h"

namespace Hansa::Game
{
	/** Presentation-only camera state. It never participates in authoritative simulation state or checksums. */
	struct HANSA_API FHansaStrategyCameraState
	{
		FVector2D Focus = FVector2D::ZeroVector;
		float YawDegrees = 45.0f;
		float ZoomDistance = 6500.0f;

		bool IsFinite() const;
	};

	/** Device-neutral camera intent consumed identically by keyboard, mouse, controller and automation. */
	struct HANSA_API FHansaStrategyCameraIntent
	{
		FVector2D Pan = FVector2D::ZeroVector;
		float Rotate = 0.0f;
		float ZoomSteps = 0.0f;
		bool bFastPan = false;

		bool IsFinite() const;
	};

	struct HANSA_API FHansaStrategyCameraSettings
	{
		FVector2D BoundsMin = FVector2D(-12000.0f, -8000.0f);
		FVector2D BoundsMax = FVector2D(12000.0f, 8000.0f);
		float PanUnitsPerSecond = 2400.0f;
		float FastPanMultiplier = 2.5f;
		float RotationDegreesPerSecond = 75.0f;
		float ZoomUnitsPerStep = 900.0f;
		float MinimumZoomDistance = 1800.0f;
		float MaximumZoomDistance = 9500.0f;

		bool IsValid() const;
	};

	/** Stateless intent reducer used by the pawn and fast automation tests. */
	class HANSA_API FHansaStrategyCameraModel
	{
	public:
		static FHansaStrategyCameraState Advance(
			const FHansaStrategyCameraState& Current,
			const FHansaStrategyCameraIntent& Intent,
			const FHansaStrategyCameraSettings& Settings,
			float DeltaSeconds);
	};
}
