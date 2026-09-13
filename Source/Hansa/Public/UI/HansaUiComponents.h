#pragma once

#include "CoreMinimal.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Animation/CurveSequence.h"

class SBorder;
class STextBlock;

namespace Hansa::UI
{
// Presentation-only state. Never enters the simulation or definition registry.
enum class EUiState : uint8 { Default, Selected, Disabled, Loading, Warning, Error, Empty, Success, Stale };
enum class EUiSurface : uint8 { TopBar, BottomTray, Panel, Card, Tooltip, Modal, Notification };
enum class EUiGlyph : uint8 { Information, Warning, Error, Check, Loading, Arrow, Cursor, Decoration, Bread, Fish, Planks, Building, Road, Production, Storage, Harbor, Civic, Farm, Mill, Bakery, Beer, Coin, Trend, People, Laborer, Wealthy, Pause, Play, Fast, Fastest, Grain, Flour, Timber, Salt, Iron, Tools, Close, Pin, Search, Star, Plus, Minus, Back, Up, Down, Lock, Settings, Research, Save, Map, Eye, Ship, Warehouse, Dock, Market, Hops, Malt, Barrels, LumberCamp, HopFarm, MaltHouse, Cooperage, Brewery, Count };
enum class EUiSeries : uint8 { Price, Stock, CitizenDemand, IndustrialDemand, Incoming, Reserve };

struct HANSA_API FUiPreferences
{
	bool bHighContrast = false;
	bool bReducedMotion = false;
	bool bLargeText = false;
	float UiScale = 1.f;
};

/** One shared policy for surface, contrast, state redundancy and geometry. */
struct HANSA_API FUiComponentStyle
{
	FSlateBrush Brush;
	FLinearColor Foreground;
	FLinearColor Accent;
	FMargin Padding;
	EUiGlyph Glyph = EUiGlyph::Information;
	bool bOnDark = false;
};

HANSA_API FUiComponentStyle GetComponentStyle(EUiSurface Surface, EUiState State, FUiPreferences Preferences = {});
HANSA_API FText GetStateLabel(EUiState State);
HANSA_API FSlateFontInfo GetComponentFont(EHansaUiTypographyToken Token, FUiPreferences Preferences = {});
HANSA_API FTableRowStyle GetLedgerRowStyle(bool bHighContrast = false);
HANSA_API bool CanActivate(EUiState State);

HANSA_API const FSlateBrush* GetGeneratedIconBrush(EUiGlyph Glyph, int32 PixelSize = 32);
HANSA_API EUiGlyph GlyphForGood(FName GoodId);

/** Individually generated raster artwork with aspect-preserving display-density selection. */
class HANSA_API SHansaGlyph final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHansaGlyph) : _Glyph(EUiGlyph::Information), _OnDark(false), _Size(24.f) {}
		SLATE_ARGUMENT(EUiGlyph, Glyph)
		SLATE_ARGUMENT(bool, OnDark)
		SLATE_ARGUMENT(float, Size)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void SetGlyph(EUiGlyph Value) { Glyph=Value; Invalidate(EInvalidateWidgetReason::Paint); }
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	EUiGlyph Glyph;
	bool bOnDark = false;
	float Size = 24.f;
};

/** Shared safe-area shell. Presenters supply content and own modal input/focus routing. */
class HANSA_API SHansaScreenShell final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHansaScreenShell) {}
		SLATE_NAMED_SLOT(FArguments, TopBar)
		SLATE_NAMED_SLOT(FArguments, BottomTray)
		SLATE_NAMED_SLOT(FArguments, Inspector)
		SLATE_NAMED_SLOT(FArguments, Notifications)
		SLATE_NAMED_SLOT(FArguments, Modal)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
};

/** Reusable top bar/tray/panel/card/tooltip/modal/notification surface. Content is always native. */
class HANSA_API SHansaSurface final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHansaSurface) : _Surface(EUiSurface::Panel), _State(EUiState::Default) {}
		SLATE_ARGUMENT(EUiSurface, Surface)
		SLATE_ARGUMENT(EUiState, State)
		SLATE_ARGUMENT(FUiPreferences, Preferences)
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(FText, Reason)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void SetState(EUiState State, const FText& Reason);
private:
	void Refresh();
	EActiveTimerReturnType AnimateEntrance(double, float);
	EUiSurface Surface;
	EUiState State;
	FUiPreferences Preferences;
	FUiComponentStyle Style;
	FText Reason;
	FCurveSequence Entrance;
	TSharedPtr<SBorder> Border;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<SHansaGlyph> StatusIcon;
	TSharedPtr<SWidget> StatusSlot;
};

/** Shared button, tab/category and building-card interaction; standard Slate pointer/key activation. */
class HANSA_API SHansaAction : public SButton
{
public:
	SLATE_BEGIN_ARGS(SHansaAction) : _Kind(EHansaUiButtonStyle::Primary), _State(EUiState::Default), _Compact(false) {}
		SLATE_ARGUMENT(EHansaUiButtonStyle, Kind)
		SLATE_ARGUMENT(bool, Compact)
		SLATE_ARGUMENT(EUiState, State)
		SLATE_ARGUMENT(FUiPreferences, Preferences)
		SLATE_ARGUMENT(FText, Label)
		SLATE_ARGUMENT(FText, Reason)
		SLATE_EVENT(FOnClicked, OnClicked)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void SetState(EUiState NewState, const FText& Reason = FText::GetEmpty());
	void SetLabel(const FText& Label);
	void SetFocusHandler(FSimpleDelegate Handler) { FocusHandler=MoveTemp(Handler); }
	FReply OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
	EUiState GetState() const { return State; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	FReply Activate();
	FButtonStyle Style;
	EUiState State = EUiState::Default;
	FUiPreferences Preferences;
	FOnClicked Action;
	bool bCompact = false;
	TSharedPtr<STextBlock> StateText;
	TSharedPtr<STextBlock> LabelText;
    TSharedPtr<SHansaGlyph> LabelIcon;
    FText IconLabel;
	FSimpleDelegate FocusHandler;
};

struct HANSA_API FUiChartSeries
{
	EUiSeries Role = EUiSeries::Price;
	TArray<FVector2D> Points; // normalized [0,1] presentation coordinates, not economic formulas
	FText Label;
	bool bEstimated = false;
};

/** Native chart, connector, progress and footprint rendering. No polling, texture stretching or gameplay rules. */
enum class EUiDiagram : uint8 { Chart, Connector, Progress, Footprint };
class HANSA_API SHansaDiagram final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SHansaDiagram) : _Kind(EUiDiagram::Chart), _State(EUiState::Default), _Fraction(0.f) {}
		SLATE_ARGUMENT(EUiDiagram, Kind)
		SLATE_ARGUMENT(EUiState, State)
		SLATE_ARGUMENT(FUiPreferences, Preferences)
		SLATE_ARGUMENT(float, Fraction)
		SLATE_ARGUMENT(FText, Summary)
		SLATE_ARGUMENT(TArray<FUiChartSeries>, Series)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void SetData(float Fraction, EUiState State, const FText& Summary, TArray<FUiChartSeries> Series = {});
	float GetFraction() const { return Fraction; }
	virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	EUiDiagram Kind;
	EUiState State;
	FUiPreferences Preferences;
	float Fraction = 0.f;
	TArray<FUiChartSeries> Series;
	TSharedPtr<STextBlock> SummaryText;
};
}
