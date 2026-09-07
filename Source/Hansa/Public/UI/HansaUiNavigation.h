#pragma once

#include "CoreMinimal.h"

struct FKeyEvent;

namespace Hansa::UI
{
	enum class EHansaUiNavigationIntent : uint8
	{
		None,
		Next,
		Previous,
		Activate,
		Back
	};

	/** Device-neutral classification shared by every native MVP screen. */
	HANSA_API EHansaUiNavigationIntent ClassifyNavigationIntent(const FKeyEvent& KeyEvent);

	/** Returns a stable wrapped target. An unknown current focus starts at the first/last item. */
	HANSA_API FString FindWrappedFocusTarget(
		const TArray<FString>& FocusOrder,
		const FString& CurrentSemanticId,
		bool bForward);
}
