#include "UI/HansaHudLayout.h"

namespace Hansa::UI
{
	float FHansaHudLayoutMetrics::GetDefaultOcclusionRatio() const
	{
		const float ViewportArea = static_cast<float>(ViewportSize.X * ViewportSize.Y);
		if (ViewportArea <= 0.0f)
		{
			return 1.0f;
		}
		const float TopArea = FMath::Max(0.0f, ViewportSize.X - SafeArea * 2.0f) * TopBarHeight;
		const float AlertArea = AlertWidth * AlertExpandedHeight;
		const float BottomArea = BottomWidth * BottomHeight;
		return FMath::Clamp((TopArea + AlertArea + BottomArea) / ViewportArea, 0.0f, 1.0f);
	}

	bool FHansaHudLayoutMetrics::FitsSafeArea() const
	{
		return ViewportSize.X >= 1280 && ViewportSize.Y >= 720 && SafeArea >= 16.0f &&
		TopBarHeight + BottomHeight + SafeArea * 3.0f < ViewportSize.Y &&
		TopBarHeight + BuildMenuHeight + BottomHeight + SafeArea * 4.0f < ViewportSize.Y &&
		AlertWidth + InspectorWidth + SafeArea * 3.0f < ViewportSize.X;
	}

	FHansaHudLayoutMetrics MakeHudLayoutMetrics(const FIntPoint ViewportSize)
	{
		FHansaHudLayoutMetrics Result;
		Result.ViewportSize = ViewportSize;
		if (ViewportSize.X >= 1920 && ViewportSize.Y >= 1080)
		{
			Result.SafeArea = 24.0f;
			Result.TopBarHeight = 72.0f;
			Result.AlertWidth = 320.0f;
			Result.AlertExpandedHeight = 224.0f;
			Result.BottomWidth = 896.0f;
			Result.BottomHeight = 112.0f;
			Result.BuildMenuHeight = 440.0f;
			Result.InspectorWidth = 400.0f;
			Result.NotificationWidth = 360.0f;
		}
		return Result;
	}
}
