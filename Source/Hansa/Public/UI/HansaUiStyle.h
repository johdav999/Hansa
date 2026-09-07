#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateTypes.h"

#include "HansaUiStyle.generated.h"

class ISlateStyle;

UENUM(BlueprintType)
enum class EHansaUiColorToken : uint8
{
	BalticNavy,
	HarborSlate,
	Ink,
	MutedInk,
	Linen,
	Parchment,
	Oak,
	Brass,
	HanseaticBrick,
	Oxblood,
	ProsperityTeal,
	BalticBlue,
	WarningAmber,
	FrostBlue,
	Chalk
};

UENUM(BlueprintType)
enum class EHansaUiTypographyToken : uint8
{
	Display,
	Heading1,
	Heading2,
	Body,
	Data,
	Caption
};

UENUM(BlueprintType)
enum class EHansaUiSpacingToken : uint8
{
	Micro,
	Unit,
	Compact,
	Panel,
	Spacious,
	Gutter,
	MinimumPointerTarget,
	PreferredPointerTarget,
	ControllerFocusTarget
};

UENUM(BlueprintType)
enum class EHansaUiSeverity : uint8
{
	Ambient,
	Notice,
	Warning,
	Critical,
	Decision
};

UENUM(BlueprintType)
enum class EHansaUiStatusShape : uint8
{
	Dot,
	InformationCircle,
	WarningTriangle,
	CriticalOctagon,
	DecisionSeal
};

UENUM(BlueprintType)
enum class EHansaUiMotionToken : uint8
{
	HoverFeedback,
	PanelTransition,
	NumericPulse,
	MajorScaleTransition
};

UENUM(BlueprintType)
enum class EHansaUiMotionCurve : uint8
{
	Linear,
	EaseOut,
	EaseInOut
};

UENUM(BlueprintType)
enum class EHansaUiPanelStyle : uint8
{
	WorldOverlay,
	Floating,
	Working,
	Decision,
	Critical,
	Tooltip
};

UENUM(BlueprintType)
enum class EHansaUiButtonStyle : uint8
{
	Primary,
	Secondary,
	Destructive,
	Icon
};

UENUM(BlueprintType)
enum class EHansaUiContrastTarget : uint8
{
	BodyText,
	LargeTextOrEssentialIcon
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaUiFocusStyle final
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	float RingWidth = 3.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	float OuterPadding = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	float MinimumTargetSize = 48.0f;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaUiSeverityStyle final
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	FLinearColor AccentColor = FLinearColor::White;

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	EHansaUiStatusShape Shape = EHansaUiStatusShape::Dot;

	/** Stable localization key for the native text label paired with the color and shape. */
	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	FName LabelKey;

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	float EdgeWidth = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	bool bPersistent = false;
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaUiMotionSpec final
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	float DurationSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Hansa|UI")
	EHansaUiMotionCurve Curve = EHansaUiMotionCurve::Linear;
};

/**
 * Registered code-native Slate style set shared by runtime UMG/Slate and development UI.
 * No raster resources are owned by this style system.
 */
class HANSA_API FHansaUiStyle final
{
public:
	static void Initialize();
	static void Shutdown();
	static FName GetStyleSetName();
	static const ISlateStyle& Get();
};

/** Blueprint-accessible token and native-widget style facade. */
UCLASS()
class HANSA_API UHansaUiStyleLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Tokens")
	static FLinearColor GetColor(EHansaUiColorToken Token);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Tokens")
	static FSlateFontInfo GetTypography(EHansaUiTypographyToken Token);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Tokens")
	static float GetSpacing(EHansaUiSpacingToken Token);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Tokens")
	static FHansaUiFocusStyle GetFocusStyle(bool bHighContrast = false);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Tokens")
	static FHansaUiSeverityStyle GetSeverityStyle(EHansaUiSeverity Severity);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Tokens")
	static FHansaUiMotionSpec GetMotion(EHansaUiMotionToken Token, bool bReducedMotion = false);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Styles")
	static FSlateBrush GetPanelBrush(EHansaUiPanelStyle Style);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Styles")
	static FButtonStyle GetButtonStyle(EHansaUiButtonStyle Style);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Styles")
	static FCheckBoxStyle GetTabStyle();

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Styles")
	static FTextBlockStyle GetTextStyle(EHansaUiTypographyToken Token, bool bOnDarkSurface = false);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Styles")
	static FTextBlockStyle GetTooltipTextStyle();

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Accessibility")
	static float GetContrastRatio(FLinearColor Foreground, FLinearColor Background);

	UFUNCTION(BlueprintPure, Category = "Hansa|UI|Accessibility")
	static bool MeetsContrastTarget(
		FLinearColor Foreground,
		FLinearColor Background,
		EHansaUiContrastTarget Target);
};
