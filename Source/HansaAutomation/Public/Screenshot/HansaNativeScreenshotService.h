#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Templates/Function.h"

namespace Hansa::Automation
{
	enum class EHansaScreenshotError : uint8
	{
		None = 0,
		InvalidSize,
		InvalidBundleId,
		CaptureUnavailable,
		UnexpectedPixelCount,
		EvidenceWriteFailed
	};

	struct HANSAAUTOMATION_API FHansaScreenshotContext final
	{
		FString BundleId;
		FString EvidenceSuiteId = TEXT("S02P04");
		FString FixtureId = TEXT("automation-proof-v1");
		FString MapName;
		FString ScreenId = TEXT("AutomationProof.Screen");
		FString CaptureMethod = TEXT("Slate.TakeScreenshot.NativeSize");
		FString SemanticSnapshotJson;
		uint64 UiRevision = 0;
		int64 SimulationTick = 0;
		uint64 FrameNumber = 0;
		float UiScale = 1.0f;
		FString FlowId;
		TArray<FString> StructuralAssertions;
		bool bStructuralAssertionsPassed = false;
	};

	struct HANSAAUTOMATION_API FHansaScreenshotResult final
	{
		EHansaScreenshotError Error = EHansaScreenshotError::None;
		FIntPoint Size = FIntPoint::ZeroValue;
		FString ScreenshotPath;
		FString MetadataPath;
		FString SemanticSnapshotPath;
		FString ContentSha1;

		[[nodiscard]] bool IsSuccess() const { return Error == EHansaScreenshotError::None; }
	};

	/** Captures at a supported native target size and encodes those pixels directly. */
	class HANSAAUTOMATION_API FHansaNativeScreenshotService final
	{
	public:
		using FNativeCapture = TFunction<bool(const FIntPoint&, TArray<FColor>&)>;

		[[nodiscard]] static bool IsSupportedSize(const FIntPoint& Size);
		FHansaScreenshotResult Capture(
			const FIntPoint& RequestedSize,
			const FHansaScreenshotContext& Context,
			FNativeCapture NativeCapture) const;
	};
}
