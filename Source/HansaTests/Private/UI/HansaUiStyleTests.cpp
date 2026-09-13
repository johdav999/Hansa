#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "UI/HansaHudSemanticIds.h"
#include "UI/HansaUiStyle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaUiTokenContractTest,
	"Hansa.UI.Style.TokenContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaUiTokenContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	auto TestColor = [this](const TCHAR* Label, const EHansaUiColorToken Token, const TCHAR* Hex)
	{
		TestTrue(Label, UHansaUiStyleLibrary::GetColor(Token).ToFColorSRGB() == FColor::FromHex(Hex));
	};
	TestColor(TEXT("Baltic Navy matches the approved palette"), EHansaUiColorToken::BalticNavy, TEXT("152A35"));
	TestColor(TEXT("Harbor Slate matches the approved palette"), EHansaUiColorToken::HarborSlate, TEXT("29424D"));
	TestColor(TEXT("Ink matches the approved palette"), EHansaUiColorToken::Ink, TEXT("202628"));
	TestColor(TEXT("Muted Ink matches the approved palette"), EHansaUiColorToken::MutedInk, TEXT("596160"));
	TestColor(TEXT("Linen matches the approved palette"), EHansaUiColorToken::Linen, TEXT("F2E9D8"));
	TestColor(TEXT("Parchment matches the approved palette"), EHansaUiColorToken::Parchment, TEXT("DFCFAF"));
	TestColor(TEXT("Oak matches the approved palette"), EHansaUiColorToken::Oak, TEXT("795137"));
	TestColor(TEXT("Brass matches the approved palette"), EHansaUiColorToken::Brass, TEXT("C19A52"));
	TestColor(TEXT("Hanseatic Brick matches the approved palette"), EHansaUiColorToken::HanseaticBrick, TEXT("A44C3F"));
	TestColor(TEXT("Oxblood matches the approved palette"), EHansaUiColorToken::Oxblood, TEXT("762F32"));
	TestColor(TEXT("Prosperity Teal matches the approved palette"), EHansaUiColorToken::ProsperityTeal, TEXT("35766F"));
	TestColor(TEXT("Baltic Blue matches the approved palette"), EHansaUiColorToken::BalticBlue, TEXT("397FA3"));
	TestColor(TEXT("Warning Amber matches the approved palette"), EHansaUiColorToken::WarningAmber, TEXT("D09132"));
	TestColor(TEXT("Frost Blue matches the approved palette"), EHansaUiColorToken::FrostBlue, TEXT("9CC3CF"));
	TestColor(TEXT("Chalk matches the approved palette"), EHansaUiColorToken::Chalk, TEXT("FAF7EF"));

	TestEqual(TEXT("Micro spacing is half the eight-pixel base unit"),
		UHansaUiStyleLibrary::GetSpacing(EHansaUiSpacingToken::Micro), 4.0f);
	TestEqual(TEXT("Base spacing unit is eight pixels"),
		UHansaUiStyleLibrary::GetSpacing(EHansaUiSpacingToken::Unit), 8.0f);
	TestEqual(TEXT("Panel padding uses the compact approved value"),
		UHansaUiStyleLibrary::GetSpacing(EHansaUiSpacingToken::Panel), 16.0f);
	TestEqual(TEXT("Controller targets are at least 48 pixels"),
		UHansaUiStyleLibrary::GetSpacing(EHansaUiSpacingToken::ControllerFocusTarget), 48.0f);

	TestEqual(TEXT("Display typography is inside the 32-40 pixel range"),
		UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Display).Size, 27.0f);
	TestEqual(TEXT("Body typography uses the approved 15-17 pixel range"),
		UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Body).Size, 12.0f);
	TestEqual(TEXT("Caption typography never drops below 12 pixels"),
		UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Caption).Size, 9.75f);

	const FHansaUiFocusStyle Focus = UHansaUiStyleLibrary::GetFocusStyle();
	const FHansaUiFocusStyle HighContrastFocus = UHansaUiStyleLibrary::GetFocusStyle(true);
	TestTrue(TEXT("Focus uses Brass independently of hover"),
		Focus.Color.ToFColorSRGB() == FColor::FromHex(TEXT("C19A52")));
	TestTrue(TEXT("High contrast strengthens the focus ring"), HighContrastFocus.RingWidth > Focus.RingWidth);
	TestTrue(TEXT("Focus target satisfies the controller minimum"), Focus.MinimumTargetSize >= 48.0f);

	const FHansaUiMotionSpec Hover = UHansaUiStyleLibrary::GetMotion(EHansaUiMotionToken::HoverFeedback);
	const FHansaUiMotionSpec Panel = UHansaUiStyleLibrary::GetMotion(EHansaUiMotionToken::PanelTransition);
	const FHansaUiMotionSpec Major = UHansaUiStyleLibrary::GetMotion(EHansaUiMotionToken::MajorScaleTransition);
	TestTrue(TEXT("Hover feedback remains under 100 milliseconds"), Hover.DurationSeconds > 0.0f && Hover.DurationSeconds < 0.1f);
	TestTrue(TEXT("Panel transition remains inside 120-180 milliseconds"), Panel.DurationSeconds >= 0.12f && Panel.DurationSeconds <= 0.18f);
	TestTrue(TEXT("Major transition remains inside 250-400 milliseconds"), Major.DurationSeconds >= 0.25f && Major.DurationSeconds <= 0.4f);
	TestEqual(TEXT("Reduced motion makes spatial transition immediate"),
		UHansaUiStyleLibrary::GetMotion(EHansaUiMotionToken::MajorScaleTransition, true).DurationSeconds, 0.0f);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaUiContrastAndSeverityTest,
	"Hansa.UI.Style.ContrastAndSeverity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaUiContrastAndSeverityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	auto Color = [](const EHansaUiColorToken Token) { return UHansaUiStyleLibrary::GetColor(Token); };
	TestTrue(TEXT("Ink on Linen meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::Ink), Color(EHansaUiColorToken::Linen), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("Ink on Parchment meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::Ink), Color(EHansaUiColorToken::Parchment), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("Muted Ink on Linen meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::MutedInk), Color(EHansaUiColorToken::Linen), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("Chalk on Baltic Navy meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::Chalk), Color(EHansaUiColorToken::BalticNavy), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("Chalk on Harbor Slate meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::Chalk), Color(EHansaUiColorToken::HarborSlate), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("Chalk on Hanseatic Brick meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::Chalk), Color(EHansaUiColorToken::HanseaticBrick), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("Chalk on Oxblood meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::Chalk), Color(EHansaUiColorToken::Oxblood), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("Ink on Warning Amber meets body-text AA"), UHansaUiStyleLibrary::MeetsContrastTarget(
		Color(EHansaUiColorToken::Ink), Color(EHansaUiColorToken::WarningAmber), EHansaUiContrastTarget::BodyText));

	const FHansaUiSeverityStyle Notice = UHansaUiStyleLibrary::GetSeverityStyle(EHansaUiSeverity::Notice);
	const FHansaUiSeverityStyle Warning = UHansaUiStyleLibrary::GetSeverityStyle(EHansaUiSeverity::Warning);
	const FHansaUiSeverityStyle Critical = UHansaUiStyleLibrary::GetSeverityStyle(EHansaUiSeverity::Critical);
	TestTrue(TEXT("Notice, warning and critical use distinct non-color shapes"),
		Notice.Shape != Warning.Shape && Warning.Shape != Critical.Shape && Notice.Shape != Critical.Shape);
	TestFalse(TEXT("Notice is a timed non-persistent state"), Notice.bPersistent);
	TestTrue(TEXT("Warning and critical remain persistent"), Warning.bPersistent && Critical.bPersistent);
	TestTrue(TEXT("Every tested severity has a stable localization key"),
		!Notice.LabelKey.IsNone() && !Warning.LabelKey.IsNone() && !Critical.LabelKey.IsNone());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaUiNativeStyleTest,
	"Hansa.UI.Style.NativeWidgetStyles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaUiNativeStyleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FHansaUiStyle::Initialize();
	TestEqual(TEXT("Registered style has the stable name"), FHansaUiStyle::GetStyleSetName(), FName(TEXT("HansaUi")));
	TestTrue(TEXT("Working panel is code-native rounded geometry"),
		UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Working).DrawAs == ESlateBrushDrawType::RoundedBox);
	TestTrue(TEXT("Tooltip is code-native rounded geometry"),
		UHansaUiStyleLibrary::GetPanelBrush(EHansaUiPanelStyle::Tooltip).DrawAs == ESlateBrushDrawType::RoundedBox);
	TestTrue(TEXT("Primary button has explicit native state brushes"),
		UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary).Normal.DrawAs == ESlateBrushDrawType::RoundedBox &&
		UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary).Hovered.DrawAs == ESlateBrushDrawType::RoundedBox &&
		UHansaUiStyleLibrary::GetButtonStyle(EHansaUiButtonStyle::Primary).Pressed.DrawAs == ESlateBrushDrawType::RoundedBox);
	TestTrue(TEXT("Tab style uses native toggle-button behavior"),
		UHansaUiStyleLibrary::GetTabStyle().CheckBoxType == ESlateCheckBoxType::ToggleButton);
	TestEqual(TEXT("Tooltip text uses body-size native text"),
		UHansaUiStyleLibrary::GetTooltipTextStyle().Font.Size, 12.0f);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaHudSemanticContractTest,
	"Hansa.UI.HUD.SemanticContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaHudSemanticContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TSet<FString> SeenIds;
	int32 InstancePrefixCount = 0;
	for (const Hansa::UI::FHansaHudSemanticId& Entry : Hansa::UI::GetMvpHudSemanticIds())
	{
		const FString Id(Entry.Id);
		const FString ParentId(Entry.ParentId);
		TestFalse(FString::Printf(TEXT("%s has a component name"), Entry.Id), FString(Entry.Component).IsEmpty());
		TestTrue(FString::Printf(TEXT("%s is a valid stable semantic ID"), Entry.Id), Hansa::Automation::IsValidSemanticId(Id));
		TestFalse(FString::Printf(TEXT("%s is unique"), Entry.Id), SeenIds.Contains(Id));
		if (!ParentId.IsEmpty())
		{
			TestTrue(FString::Printf(TEXT("%s names an earlier stable parent"), Entry.Id), SeenIds.Contains(ParentId));
		}
		SeenIds.Add(Id);
		InstancePrefixCount += Entry.bInstancePrefix ? 1 : 0;
	}
	TestTrue(TEXT("Contract inventories the complete HUD component family"), SeenIds.Num() >= 50);
	TestTrue(TEXT("HUD, build menu, placement and inspector roots are present"),
		SeenIds.Contains(TEXT("HUD.Root")) && SeenIds.Contains(TEXT("BuildMenu.Root")) &&
		SeenIds.Contains(TEXT("Placement.Root")) && SeenIds.Contains(TEXT("Inspector.Root")));
	TestTrue(TEXT("Repeated rows and controls declare stable instance prefixes"), InstancePrefixCount >= 5);
	return !HasAnyErrors();
}

#endif
