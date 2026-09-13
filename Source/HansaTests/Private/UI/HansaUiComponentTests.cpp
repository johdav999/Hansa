#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaUiComponents.h"
#include "InputCoreTypes.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Widgets/Text/STextBlock.h"

using namespace Hansa::UI;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaComponentAccessibilityTest,"Hansa.UI.Style.ComponentAccessibility",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaComponentAccessibilityTest::RunTest(const FString&)
{
	for(int32 Surface=0;Surface<=int32(EUiSurface::Notification);++Surface)
	for(int32 State=0;State<=int32(EUiState::Stale);++State)
	for(bool HighContrast:{false,true})
	{
		const auto Style=GetComponentStyle(EUiSurface(Surface),EUiState(State),{HighContrast,false,false});
		TestTrue(TEXT("Every state retains body contrast"),UHansaUiStyleLibrary::MeetsContrastTarget(
			Style.Foreground,Style.Brush.TintColor.GetSpecifiedColor(),EHansaUiContrastTarget::BodyText));
		TestTrue(TEXT("Surfaces have no raster resource"),Style.Brush.GetResourceObject()==nullptr && Style.Brush.DrawAs==ESlateBrushDrawType::RoundedBox);
		if(State!=0) TestFalse(TEXT("State has a localized non-color label"),GetStateLabel(EUiState(State)).IsEmpty());
	}
	const auto Body=GetComponentFont(EHansaUiTypographyToken::Body);
	const auto Large=GetComponentFont(EHansaUiTypographyToken::Body,{false,true,true});
	TestTrue(TEXT("Large text changes font metrics, not a raster transform"),Large.Size>Body.Size);
	for(auto Role:{EHansaUiTypographyToken::Body,EHansaUiTypographyToken::Display,EHansaUiTypographyToken::Data})
	{
		const auto Font=GetComponentFont(Role);
		const auto* Composite=Font.GetCompositeFont();
		if(TestNotNull(TEXT("Project composite font"),Composite))
		{
			const FString Path=Composite->DefaultTypeface.Fonts[0].Font.GetFontFilename();
			TestTrue(TEXT("Font belongs to the project"),Path.Contains(TEXT("Hansa/UI/Fonts")));
			TestTrue(TEXT("Font exists"),IFileManager::Get().FileExists(*Path));
		}
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaComponentActionTest,"Hansa.UI.Style.ComponentActivation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaComponentActionTest::RunTest(const FString&)
{
	int32 Count=0;
	auto Button=SNew(SHansaAction).Label(FText::FromString(TEXT("Produktionskettenübersicht öffnen")))
		.OnClicked_Lambda([&Count]{++Count;return FReply::Handled();});
	const FGeometry Geometry=FGeometry::MakeRoot(FVector2D(320,100),FSlateLayoutTransform());
	for(EUiState State:{EUiState::Default,EUiState::Selected,EUiState::Disabled,EUiState::Loading,EUiState::Warning,EUiState::Error})
	{
		Button->SetState(State,FText::FromString(TEXT("Missing prerequisite: complete research first.")));
		TestEqual(TEXT("Disabled and loading actions reject input"),Button->IsEnabled(),CanActivate(State));
		const int32 Before=Count;
		for(const FKey Key:{EKeys::Enter,EKeys::SpaceBar,EKeys::Gamepad_FaceButton_Bottom})
		{
			const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
			Button->OnKeyDown(Geometry,Event); Button->OnKeyUp(Geometry,Event);
		}
		TestEqual(TEXT("Keyboard and controller use the same guarded action"),Count-Before,CanActivate(State)?3:0);
	}
	auto Progress=SNew(SHansaDiagram).Kind(EUiDiagram::Progress).Fraction(2.f);
	TestEqual(TEXT("Overfull progress clamps"),Progress->GetFraction(),1.f);
	Progress->SetData(-.5f,EUiState::Warning,FText::FromString(TEXT("Insufficient materials")));
	TestEqual(TEXT("Negative progress clamps"),Progress->GetFraction(),0.f);
	return !HasAnyErrors();
}
#endif
