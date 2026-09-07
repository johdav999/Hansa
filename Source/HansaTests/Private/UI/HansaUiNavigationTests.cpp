#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "UI/HansaUiNavigation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FKeyEvent KeyEvent(const FKey& Key, const bool bShift = false)
	{
		return FKeyEvent(Key, FModifierKeysState(bShift, false, false, false, false, false, false, false, false), 0, false, 0, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaUiNavigationIntentTest,
	"Hansa.UI.Navigation.KeyboardControllerParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaUiNavigationIntentTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::UI;
	(void)Parameters;
	TestEqual(TEXT("Tab moves forward"), ClassifyNavigationIntent(KeyEvent(EKeys::Tab)), EHansaUiNavigationIntent::Next);
	TestEqual(TEXT("Shift+Tab moves backward"), ClassifyNavigationIntent(KeyEvent(EKeys::Tab, true)), EHansaUiNavigationIntent::Previous);
	TestEqual(TEXT("D-pad right moves forward"), ClassifyNavigationIntent(KeyEvent(EKeys::Gamepad_DPad_Right)), EHansaUiNavigationIntent::Next);
	TestEqual(TEXT("D-pad up moves backward"), ClassifyNavigationIntent(KeyEvent(EKeys::Gamepad_DPad_Up)), EHansaUiNavigationIntent::Previous);
	TestEqual(TEXT("Enter activates"), ClassifyNavigationIntent(KeyEvent(EKeys::Enter)), EHansaUiNavigationIntent::Activate);
	TestEqual(TEXT("Controller A activates"), ClassifyNavigationIntent(KeyEvent(EKeys::Gamepad_FaceButton_Bottom)), EHansaUiNavigationIntent::Activate);
	TestEqual(TEXT("Escape backs out"), ClassifyNavigationIntent(KeyEvent(EKeys::Escape)), EHansaUiNavigationIntent::Back);
	TestEqual(TEXT("Controller B backs out"), ClassifyNavigationIntent(KeyEvent(EKeys::Gamepad_FaceButton_Right)), EHansaUiNavigationIntent::Back);
	const TArray<FString> Order = { TEXT("First"), TEXT("Second"), TEXT("Third") };
	TestEqual(TEXT("Forward navigation wraps"), FindWrappedFocusTarget(Order, TEXT("Third"), true), FString(TEXT("First")));
	TestEqual(TEXT("Backward navigation wraps"), FindWrappedFocusTarget(Order, TEXT("First"), false), FString(TEXT("Third")));
	return !HasAnyErrors();
}

#endif
