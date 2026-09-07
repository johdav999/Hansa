#include "UI/HansaUiStyle.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace Hansa::UI::Private
{
	TSharedPtr<FSlateStyleSet> StyleSet;

	const FName StyleSetName(TEXT("HansaUi"));

	const FName PanelWorldOverlay(TEXT("Hansa.Panel.WorldOverlay"));
	const FName PanelFloating(TEXT("Hansa.Panel.Floating"));
	const FName PanelWorking(TEXT("Hansa.Panel.Working"));
	const FName PanelDecision(TEXT("Hansa.Panel.Decision"));
	const FName PanelCritical(TEXT("Hansa.Panel.Critical"));
	const FName PanelTooltip(TEXT("Hansa.Panel.Tooltip"));

	const FName ButtonPrimary(TEXT("Hansa.Button.Primary"));
	const FName ButtonSecondary(TEXT("Hansa.Button.Secondary"));
	const FName ButtonDestructive(TEXT("Hansa.Button.Destructive"));
	const FName ButtonIcon(TEXT("Hansa.Button.Icon"));
	const FName Tab(TEXT("Hansa.Tab"));

	const FName TextDisplayLight(TEXT("Hansa.Text.Display.Light"));
	const FName TextHeading1Light(TEXT("Hansa.Text.Heading1.Light"));
	const FName TextHeading2Light(TEXT("Hansa.Text.Heading2.Light"));
	const FName TextBodyLight(TEXT("Hansa.Text.Body.Light"));
	const FName TextDataLight(TEXT("Hansa.Text.Data.Light"));
	const FName TextCaptionLight(TEXT("Hansa.Text.Caption.Light"));
	const FName TextDisplayDark(TEXT("Hansa.Text.Display.Dark"));
	const FName TextHeading1Dark(TEXT("Hansa.Text.Heading1.Dark"));
	const FName TextHeading2Dark(TEXT("Hansa.Text.Heading2.Dark"));
	const FName TextBodyDark(TEXT("Hansa.Text.Body.Dark"));
	const FName TextDataDark(TEXT("Hansa.Text.Data.Dark"));
	const FName TextCaptionDark(TEXT("Hansa.Text.Caption.Dark"));
	const FName TextTooltip(TEXT("Hansa.Text.Tooltip"));

	FLinearColor ColorFromHex(const TCHAR* Hex)
	{
		return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
	}

	const FName& PanelName(const EHansaUiPanelStyle Style)
	{
		switch (Style)
		{
		case EHansaUiPanelStyle::WorldOverlay: return PanelWorldOverlay;
		case EHansaUiPanelStyle::Floating: return PanelFloating;
		case EHansaUiPanelStyle::Working: return PanelWorking;
		case EHansaUiPanelStyle::Decision: return PanelDecision;
		case EHansaUiPanelStyle::Critical: return PanelCritical;
		case EHansaUiPanelStyle::Tooltip: return PanelTooltip;
		default: return PanelWorking;
		}
	}

	const FName& ButtonName(const EHansaUiButtonStyle Style)
	{
		switch (Style)
		{
		case EHansaUiButtonStyle::Primary: return ButtonPrimary;
		case EHansaUiButtonStyle::Secondary: return ButtonSecondary;
		case EHansaUiButtonStyle::Destructive: return ButtonDestructive;
		case EHansaUiButtonStyle::Icon: return ButtonIcon;
		default: return ButtonSecondary;
		}
	}

	const FName& TextName(const EHansaUiTypographyToken Token, const bool bOnDarkSurface)
	{
		if (bOnDarkSurface)
		{
			switch (Token)
			{
			case EHansaUiTypographyToken::Display: return TextDisplayDark;
			case EHansaUiTypographyToken::Heading1: return TextHeading1Dark;
			case EHansaUiTypographyToken::Heading2: return TextHeading2Dark;
			case EHansaUiTypographyToken::Body: return TextBodyDark;
			case EHansaUiTypographyToken::Data: return TextDataDark;
			case EHansaUiTypographyToken::Caption: return TextCaptionDark;
			default: return TextBodyDark;
			}
		}

		switch (Token)
		{
		case EHansaUiTypographyToken::Display: return TextDisplayLight;
		case EHansaUiTypographyToken::Heading1: return TextHeading1Light;
		case EHansaUiTypographyToken::Heading2: return TextHeading2Light;
		case EHansaUiTypographyToken::Body: return TextBodyLight;
		case EHansaUiTypographyToken::Data: return TextDataLight;
		case EHansaUiTypographyToken::Caption: return TextCaptionLight;
		default: return TextBodyLight;
		}
	}

	FSlateRoundedBoxBrush Rounded(
		const FLinearColor Fill,
		const float Radius,
		const FLinearColor Outline,
		const float OutlineWidth)
	{
		return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
	}

	FButtonStyle MakeButtonStyle(
		const FLinearColor Normal,
		const FLinearColor Hovered,
		const FLinearColor Pressed,
		const FLinearColor Foreground,
		const FLinearColor DisabledForeground)
	{
		const FLinearColor DisabledFill = Normal.CopyWithNewOpacity(0.38f);
		return FButtonStyle()
			.SetNormal(Rounded(Normal, 4.0f, UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oak), 1.0f))
			.SetHovered(Rounded(Hovered, 4.0f, UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass), 2.0f))
			.SetPressed(Rounded(Pressed, 4.0f, UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass), 1.0f))
			.SetDisabled(Rounded(DisabledFill, 4.0f, UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk), 1.0f))
			.SetNormalForeground(Foreground)
			.SetHoveredForeground(Foreground)
			.SetPressedForeground(Foreground)
			.SetDisabledForeground(DisabledForeground)
			.SetNormalPadding(FMargin(16.0f, 10.0f))
			.SetPressedPadding(FMargin(16.0f, 11.0f, 16.0f, 9.0f));
	}

	void SetTextStyles(FSlateStyleSet& InStyleSet)
	{
		const FLinearColor Ink = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink);
		const FLinearColor Chalk = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk);
		const FLinearColor MutedInk = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk);
		const FLinearColor Parchment = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment);

		auto MakeText = [](const FSlateFontInfo& Font, const FLinearColor Color)
		{
			return FTextBlockStyle()
				.SetFont(Font)
				.SetColorAndOpacity(Color)
				.SetShadowOffset(FVector2f::ZeroVector)
				.SetShadowColorAndOpacity(FLinearColor::Transparent);
		};

		InStyleSet.Set(TextDisplayLight, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Display), Ink));
		InStyleSet.Set(TextHeading1Light, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Heading1), Ink));
		InStyleSet.Set(TextHeading2Light, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Heading2), Ink));
		InStyleSet.Set(TextBodyLight, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Body), Ink));
		InStyleSet.Set(TextDataLight, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Data), Ink));
		InStyleSet.Set(TextCaptionLight, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Caption), MutedInk));
		InStyleSet.Set(TextDisplayDark, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Display), Chalk));
		InStyleSet.Set(TextHeading1Dark, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Heading1), Chalk));
		InStyleSet.Set(TextHeading2Dark, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Heading2), Chalk));
		InStyleSet.Set(TextBodyDark, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Body), Chalk));
		InStyleSet.Set(TextDataDark, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Data), Chalk));
		InStyleSet.Set(TextCaptionDark, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Caption), Parchment));
		InStyleSet.Set(TextTooltip, MakeText(UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Body), Chalk));
	}
}

void FHansaUiStyle::Initialize()
{
	using namespace Hansa::UI::Private;
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(StyleSetName);
	const FLinearColor Navy = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy);
	const FLinearColor Slate = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::HarborSlate);
	const FLinearColor Ink = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink);
	const FLinearColor MutedInk = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk);
	const FLinearColor Linen = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen);
	const FLinearColor Parchment = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment);
	const FLinearColor Oak = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oak);
	const FLinearColor Brass = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
	const FLinearColor Brick = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::HanseaticBrick);
	const FLinearColor Oxblood = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Oxblood);
	const FLinearColor Chalk = UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk);

	StyleSet->Set(PanelWorldOverlay, new FSlateRoundedBoxBrush(Navy.CopyWithNewOpacity(0.88f), 2.0f));
	StyleSet->Set(PanelFloating, new FSlateRoundedBoxBrush(Navy, 4.0f, Brass, 1.0f));
	StyleSet->Set(PanelWorking, new FSlateRoundedBoxBrush(Linen, 4.0f, Oak, 1.0f));
	StyleSet->Set(PanelDecision, new FSlateRoundedBoxBrush(Linen, 4.0f, Brass, 2.0f));
	StyleSet->Set(PanelCritical, new FSlateRoundedBoxBrush(Navy, 4.0f, Oxblood, 3.0f));
	StyleSet->Set(PanelTooltip, new FSlateRoundedBoxBrush(Navy, 4.0f, Brass, 1.0f));

	StyleSet->Set(ButtonPrimary, MakeButtonStyle(Slate, Brick, Navy, Chalk, MutedInk));
	StyleSet->Set(ButtonSecondary, MakeButtonStyle(Parchment, Linen, Brass, Ink, MutedInk));
	StyleSet->Set(ButtonDestructive, MakeButtonStyle(Oxblood, Brick, Oxblood, Chalk, MutedInk));
	StyleSet->Set(ButtonIcon, MakeButtonStyle(Navy, Slate, Navy, Chalk, MutedInk));

	const FCheckBoxStyle TabStyle = FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
		.SetUncheckedImage(Rounded(Slate, 2.0f, Oak, 1.0f))
		.SetUncheckedHoveredImage(Rounded(Slate, 2.0f, Brass, 2.0f))
		.SetUncheckedPressedImage(Rounded(Navy, 2.0f, Brass, 1.0f))
		.SetCheckedImage(Rounded(Navy, 2.0f, Brass, 2.0f))
		.SetCheckedHoveredImage(Rounded(Slate, 2.0f, Brass, 3.0f))
		.SetCheckedPressedImage(Rounded(Navy, 2.0f, Brass, 2.0f))
		.SetUndeterminedImage(Rounded(Slate, 2.0f, Brass, 1.0f))
		.SetUndeterminedHoveredImage(Rounded(Slate, 2.0f, Brass, 2.0f))
		.SetUndeterminedPressedImage(Rounded(Navy, 2.0f, Brass, 2.0f))
		.SetForegroundColor(Chalk)
		.SetHoveredForegroundColor(Chalk)
		.SetPressedForegroundColor(Chalk)
		.SetCheckedForegroundColor(Chalk)
		.SetCheckedHoveredForegroundColor(Chalk)
		.SetCheckedPressedForegroundColor(Chalk)
		.SetUndeterminedForegroundColor(Chalk)
		.SetPadding(FMargin(16.0f, 10.0f));
	StyleSet->Set(Tab, TabStyle);

	SetTextStyles(*StyleSet);
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FHansaUiStyle::Shutdown()
{
	using namespace Hansa::UI::Private;
	if (!StyleSet.IsValid())
	{
		return;
	}
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
	ensure(StyleSet.IsUnique());
	StyleSet.Reset();
}

FName FHansaUiStyle::GetStyleSetName()
{
	return Hansa::UI::Private::StyleSetName;
}

const ISlateStyle& FHansaUiStyle::Get()
{
	Initialize();
	return *Hansa::UI::Private::StyleSet;
}

FLinearColor UHansaUiStyleLibrary::GetColor(const EHansaUiColorToken Token)
{
	using Hansa::UI::Private::ColorFromHex;
	switch (Token)
	{
	case EHansaUiColorToken::BalticNavy: return ColorFromHex(TEXT("152A35"));
	case EHansaUiColorToken::HarborSlate: return ColorFromHex(TEXT("29424D"));
	case EHansaUiColorToken::Ink: return ColorFromHex(TEXT("202628"));
	case EHansaUiColorToken::MutedInk: return ColorFromHex(TEXT("596160"));
	case EHansaUiColorToken::Linen: return ColorFromHex(TEXT("F2E9D8"));
	case EHansaUiColorToken::Parchment: return ColorFromHex(TEXT("DFCFAF"));
	case EHansaUiColorToken::Oak: return ColorFromHex(TEXT("795137"));
	case EHansaUiColorToken::Brass: return ColorFromHex(TEXT("C19A52"));
	case EHansaUiColorToken::HanseaticBrick: return ColorFromHex(TEXT("A44C3F"));
	case EHansaUiColorToken::Oxblood: return ColorFromHex(TEXT("762F32"));
	case EHansaUiColorToken::ProsperityTeal: return ColorFromHex(TEXT("35766F"));
	case EHansaUiColorToken::BalticBlue: return ColorFromHex(TEXT("397FA3"));
	case EHansaUiColorToken::WarningAmber: return ColorFromHex(TEXT("D09132"));
	case EHansaUiColorToken::FrostBlue: return ColorFromHex(TEXT("9CC3CF"));
	case EHansaUiColorToken::Chalk: return ColorFromHex(TEXT("FAF7EF"));
	default: return ColorFromHex(TEXT("FF00FF"));
	}
}

FSlateFontInfo UHansaUiStyleLibrary::GetTypography(const EHansaUiTypographyToken Token)
{
	const bool bSerifRole = Token == EHansaUiTypographyToken::Display ||
		Token == EHansaUiTypographyToken::Heading1 || Token == EHansaUiTypographyToken::Heading2;
	const TCHAR* Typeface = bSerifRole ? TEXT("Bold") : TEXT("Regular");
	int32 Size = 16;
	switch (Token)
	{
	case EHansaUiTypographyToken::Display: Size = 36; break;
	case EHansaUiTypographyToken::Heading1: Size = 26; break;
	case EHansaUiTypographyToken::Heading2: Size = 20; break;
	case EHansaUiTypographyToken::Body: Size = 16; break;
	case EHansaUiTypographyToken::Data: Size = 15; break;
	case EHansaUiTypographyToken::Caption: Size = 13; break;
	default: break;
	}
	return FCoreStyle::GetDefaultFontStyle(Typeface, Size);
}

float UHansaUiStyleLibrary::GetSpacing(const EHansaUiSpacingToken Token)
{
	switch (Token)
	{
	case EHansaUiSpacingToken::Micro: return 4.0f;
	case EHansaUiSpacingToken::Unit: return 8.0f;
	case EHansaUiSpacingToken::Compact: return 12.0f;
	case EHansaUiSpacingToken::Panel: return 16.0f;
	case EHansaUiSpacingToken::Spacious: return 24.0f;
	case EHansaUiSpacingToken::Gutter: return 32.0f;
	case EHansaUiSpacingToken::MinimumPointerTarget: return 40.0f;
	case EHansaUiSpacingToken::PreferredPointerTarget: return 44.0f;
	case EHansaUiSpacingToken::ControllerFocusTarget: return 48.0f;
	default: return 0.0f;
	}
}

FHansaUiFocusStyle UHansaUiStyleLibrary::GetFocusStyle(const bool bHighContrast)
{
	FHansaUiFocusStyle Result;
	Result.Color = GetColor(bHighContrast ? EHansaUiColorToken::Chalk : EHansaUiColorToken::Brass);
	Result.RingWidth = bHighContrast ? 4.0f : 3.0f;
	Result.OuterPadding = 2.0f;
	Result.MinimumTargetSize = GetSpacing(EHansaUiSpacingToken::ControllerFocusTarget);
	return Result;
}

FHansaUiSeverityStyle UHansaUiStyleLibrary::GetSeverityStyle(const EHansaUiSeverity Severity)
{
	FHansaUiSeverityStyle Result;
	switch (Severity)
	{
	case EHansaUiSeverity::Ambient:
		Result.AccentColor = GetColor(EHansaUiColorToken::MutedInk);
		Result.Shape = EHansaUiStatusShape::Dot;
		Result.LabelKey = TEXT("UI.Severity.Ambient");
		Result.EdgeWidth = 1.0f;
		Result.bPersistent = false;
		break;
	case EHansaUiSeverity::Notice:
		Result.AccentColor = GetColor(EHansaUiColorToken::BalticBlue);
		Result.Shape = EHansaUiStatusShape::InformationCircle;
		Result.LabelKey = TEXT("UI.Severity.Notice");
		Result.EdgeWidth = 1.0f;
		Result.bPersistent = false;
		break;
	case EHansaUiSeverity::Warning:
		Result.AccentColor = GetColor(EHansaUiColorToken::WarningAmber);
		Result.Shape = EHansaUiStatusShape::WarningTriangle;
		Result.LabelKey = TEXT("UI.Severity.Warning");
		Result.EdgeWidth = 2.0f;
		Result.bPersistent = true;
		break;
	case EHansaUiSeverity::Critical:
		Result.AccentColor = GetColor(EHansaUiColorToken::Oxblood);
		Result.Shape = EHansaUiStatusShape::CriticalOctagon;
		Result.LabelKey = TEXT("UI.Severity.Critical");
		Result.EdgeWidth = 3.0f;
		Result.bPersistent = true;
		break;
	case EHansaUiSeverity::Decision:
		Result.AccentColor = GetColor(EHansaUiColorToken::Brass);
		Result.Shape = EHansaUiStatusShape::DecisionSeal;
		Result.LabelKey = TEXT("UI.Severity.Decision");
		Result.EdgeWidth = 2.0f;
		Result.bPersistent = true;
		break;
	default:
		break;
	}
	return Result;
}

FHansaUiMotionSpec UHansaUiStyleLibrary::GetMotion(const EHansaUiMotionToken Token, const bool bReducedMotion)
{
	FHansaUiMotionSpec Result;
	if (bReducedMotion)
	{
		return Result;
	}

	switch (Token)
	{
	case EHansaUiMotionToken::HoverFeedback:
		Result.DurationSeconds = 0.08f;
		Result.Curve = EHansaUiMotionCurve::EaseOut;
		break;
	case EHansaUiMotionToken::PanelTransition:
		Result.DurationSeconds = 0.15f;
		Result.Curve = EHansaUiMotionCurve::EaseOut;
		break;
	case EHansaUiMotionToken::NumericPulse:
		Result.DurationSeconds = 0.18f;
		Result.Curve = EHansaUiMotionCurve::EaseInOut;
		break;
	case EHansaUiMotionToken::MajorScaleTransition:
		Result.DurationSeconds = 0.30f;
		Result.Curve = EHansaUiMotionCurve::EaseInOut;
		break;
	default:
		break;
	}
	return Result;
}

FSlateBrush UHansaUiStyleLibrary::GetPanelBrush(const EHansaUiPanelStyle Style)
{
	return *FHansaUiStyle::Get().GetBrush(Hansa::UI::Private::PanelName(Style));
}

FButtonStyle UHansaUiStyleLibrary::GetButtonStyle(const EHansaUiButtonStyle Style)
{
	return FHansaUiStyle::Get().GetWidgetStyle<FButtonStyle>(Hansa::UI::Private::ButtonName(Style));
}

FCheckBoxStyle UHansaUiStyleLibrary::GetTabStyle()
{
	return FHansaUiStyle::Get().GetWidgetStyle<FCheckBoxStyle>(Hansa::UI::Private::Tab);
}

FTextBlockStyle UHansaUiStyleLibrary::GetTextStyle(
	const EHansaUiTypographyToken Token,
	const bool bOnDarkSurface)
{
	return FHansaUiStyle::Get().GetWidgetStyle<FTextBlockStyle>(Hansa::UI::Private::TextName(Token, bOnDarkSurface));
}

FTextBlockStyle UHansaUiStyleLibrary::GetTooltipTextStyle()
{
	return FHansaUiStyle::Get().GetWidgetStyle<FTextBlockStyle>(Hansa::UI::Private::TextTooltip);
}

float UHansaUiStyleLibrary::GetContrastRatio(const FLinearColor Foreground, const FLinearColor Background)
{
	auto Luminance = [](const FLinearColor Color)
	{
		return 0.2126f * FMath::Clamp(Color.R, 0.0f, 1.0f) +
			0.7152f * FMath::Clamp(Color.G, 0.0f, 1.0f) +
			0.0722f * FMath::Clamp(Color.B, 0.0f, 1.0f);
	};
	const float ForegroundLuminance = Luminance(Foreground);
	const float BackgroundLuminance = Luminance(Background);
	return (FMath::Max(ForegroundLuminance, BackgroundLuminance) + 0.05f) /
		(FMath::Min(ForegroundLuminance, BackgroundLuminance) + 0.05f);
}

bool UHansaUiStyleLibrary::MeetsContrastTarget(
	const FLinearColor Foreground,
	const FLinearColor Background,
	const EHansaUiContrastTarget Target)
{
	const float MinimumRatio = Target == EHansaUiContrastTarget::BodyText ? 4.5f : 3.0f;
	return GetContrastRatio(Foreground, Background) >= MinimumRatio;
}
