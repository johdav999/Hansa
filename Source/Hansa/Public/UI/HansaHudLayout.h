#pragma once

#include "CoreMinimal.h"

namespace Hansa::UI
{
	struct HANSA_API FHansaHudLayoutMetrics final
	{
		FIntPoint ViewportSize = FIntPoint(1280, 720);
		float SafeArea = 16.0f;
		float TopBarHeight = 64.0f;
		float AlertWidth = 288.0f;
		float AlertExpandedHeight = 184.0f;
		float BottomWidth = 720.0f;
		float BottomHeight = 96.0f;
		float BuildMenuHeight = 440.0f;
		float InspectorWidth = 360.0f;
		float NotificationWidth = 320.0f;

		[[nodiscard]] float GetDefaultOcclusionRatio() const;
		[[nodiscard]] bool FitsSafeArea() const;
	};

	HANSA_API FHansaHudLayoutMetrics MakeHudLayoutMetrics(FIntPoint ViewportSize);
}
