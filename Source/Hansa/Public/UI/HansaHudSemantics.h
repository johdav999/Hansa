#pragma once

#include "CoreMinimal.h"

namespace Hansa::UI
{
	enum class EHansaHudSemanticRole : uint8
	{
		Screen,
		Panel,
		Heading,
		Status,
		Text,
		Button,
		Alert,
		Tab,
		List,
		ListItem
	};

	struct HANSA_API FHansaHudSemanticState final
	{
		bool bVisible = true;
		bool bEnabled = true;
		bool bFocused = false;
		bool bSelected = false;
		bool bWarning = false;
		bool bError = false;
		FString ValueType;
		FString Value;
	};

	struct HANSA_API FHansaHudSemanticNode final
	{
		FString Id;
		FString ParentId;
		FString Label;
		EHansaHudSemanticRole Role = EHansaHudSemanticRole::Text;
		FHansaHudSemanticState State;
		FIntRect Bounds;
		bool bCanActivate = false;
		bool bCanFocus = false;
	};
}
