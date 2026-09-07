#include "UI/HansaUiNavigation.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"

namespace Hansa::UI
{
	EHansaUiNavigationIntent ClassifyNavigationIntent(const FKeyEvent& KeyEvent)
	{
		const FKey Key = KeyEvent.GetKey();
		if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
		{
			return EHansaUiNavigationIntent::Back;
		}
		if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
		{
			return EHansaUiNavigationIntent::Activate;
		}
		if ((Key == EKeys::Tab && KeyEvent.IsShiftDown()) || Key == EKeys::Up || Key == EKeys::Left ||
			Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Left)
		{
			return EHansaUiNavigationIntent::Previous;
		}
		if (Key == EKeys::Tab || Key == EKeys::Down || Key == EKeys::Right ||
			Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_DPad_Right)
		{
			return EHansaUiNavigationIntent::Next;
		}
		return EHansaUiNavigationIntent::None;
	}

	FString FindWrappedFocusTarget(
		const TArray<FString>& FocusOrder,
		const FString& CurrentSemanticId,
		const bool bForward)
	{
		if (FocusOrder.IsEmpty())
		{
			return FString();
		}
		const int32 CurrentIndex = FocusOrder.IndexOfByKey(CurrentSemanticId);
		if (CurrentIndex == INDEX_NONE)
		{
			return bForward ? FocusOrder[0] : FocusOrder.Last();
		}
		const int32 Offset = bForward ? 1 : FocusOrder.Num() - 1;
		return FocusOrder[(CurrentIndex + Offset) % FocusOrder.Num()];
	}
}
