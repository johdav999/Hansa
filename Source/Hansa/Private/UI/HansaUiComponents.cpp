#include "UI/HansaUiComponents.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Misc/Paths.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElementTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSafeZone.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HansaUiComponents"
namespace Hansa::UI
{
namespace
{
// Paint inherited disabled content at normal contrast; input routing remains disabled at SButton.
class SReadableContent final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SReadableContent) {} SLATE_DEFAULT_SLOT(FArguments,Content) SLATE_END_ARGS()
	void Construct(const FArguments& Args) { ChildSlot[Args._Content.Widget]; }
	int32 OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,
		int32 Layer,const FWidgetStyle& Style,bool) const override
	{ return SCompoundWidget::OnPaint(Args,G,Clip,Out,Layer,Style,true); }
};
FLinearColor Color(EHansaUiColorToken Token) { return UHansaUiStyleLibrary::GetColor(Token); }
float Space(EHansaUiSpacingToken Token) { return UHansaUiStyleLibrary::GetSpacing(Token); }
void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry,
	const TArray<FVector2D>& Points, FLinearColor Tint, float Width = 1.f)
{
	FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Width);
}
void Frame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FLinearColor Tint, float Width, float Inset)
{
	const FVector2D Size = G.GetLocalSize();
	Line(Out, Layer, G, {{Inset,Inset},{Size.X-Inset,Inset},{Size.X-Inset,Size.Y-Inset},
		{Inset,Size.Y-Inset},{Inset,Inset}}, Tint, Width);
}
EHansaUiColorToken SeriesColor(EUiSeries Role)
{
	switch(Role)
	{
	case EUiSeries::Price: return EHansaUiColorToken::Brass;
	case EUiSeries::Stock: return EHansaUiColorToken::BalticBlue;
	case EUiSeries::CitizenDemand: return EHansaUiColorToken::HanseaticBrick;
	case EUiSeries::IndustrialDemand: return EHansaUiColorToken::Oak;
	case EUiSeries::Incoming: return EHansaUiColorToken::ProsperityTeal;
	case EUiSeries::Reserve: return EHansaUiColorToken::MutedInk;
	default: return EHansaUiColorToken::Ink;
	}
}
}

bool CanActivate(EUiState State) { return State != EUiState::Disabled && State != EUiState::Loading; }
FText GetStateLabel(EUiState State)
{
	switch(State)
	{
	case EUiState::Selected: return LOCTEXT("Selected", "Selected");
	case EUiState::Disabled: return LOCTEXT("Unavailable", "Unavailable");
	case EUiState::Loading: return LOCTEXT("Loading", "Loading");
	case EUiState::Warning: return LOCTEXT("Warning", "Warning");
	case EUiState::Error: return LOCTEXT("Error", "Error");
	case EUiState::Empty: return LOCTEXT("Empty", "No information available");
	case EUiState::Success: return LOCTEXT("Success", "Complete");
	case EUiState::Stale: return LOCTEXT("Stale", "Outdated report");
	default: return FText::GetEmpty();
	}
}
FSlateFontInfo GetComponentFont(EHansaUiTypographyToken Token, FUiPreferences Preferences)
{
	FSlateFontInfo Font = UHansaUiStyleLibrary::GetTypography(Token);
	if(Preferences.bLargeText) Font.Size = FMath::CeilToFloat(Font.Size * 1.25f);
	return Font;
}
FUiComponentStyle GetComponentStyle(EUiSurface Surface, EUiState State, FUiPreferences Preferences)
{
	FUiComponentStyle Result;
	Result.bOnDark = Surface == EUiSurface::TopBar || Surface == EUiSurface::BottomTray ||
		Surface == EUiSurface::Tooltip || Surface == EUiSurface::Notification;
	const auto Fill = Color(Result.bOnDark ? EHansaUiColorToken::BalticNavy :
		(Surface == EUiSurface::Card ? EHansaUiColorToken::Parchment : EHansaUiColorToken::Linen));
	Result.Foreground = Color(Result.bOnDark ? EHansaUiColorToken::Chalk : EHansaUiColorToken::Ink);
	Result.Accent = Color(Surface == EUiSurface::Modal || Result.bOnDark ? EHansaUiColorToken::Brass : EHansaUiColorToken::Oak);
	Result.Padding = FMargin(Space(EHansaUiSpacingToken::Panel));
	if(Surface == EUiSurface::TopBar) Result.Padding = FMargin(Space(EHansaUiSpacingToken::Panel),Space(EHansaUiSpacingToken::Unit));
	switch(State)
	{
	case EUiState::Selected: Result.Accent=Color(EHansaUiColorToken::Brass); Result.Glyph=EUiGlyph::Check; break;
	case EUiState::Warning: Result.Accent=Color(EHansaUiColorToken::WarningAmber); Result.Glyph=EUiGlyph::Warning; break;
	case EUiState::Error: Result.Accent=Color(EHansaUiColorToken::Oxblood); Result.Glyph=EUiGlyph::Error; break;
	case EUiState::Success: Result.Accent=Color(EHansaUiColorToken::ProsperityTeal); Result.Glyph=EUiGlyph::Check; break;
	case EUiState::Loading: Result.Glyph=EUiGlyph::Loading; break;
	case EUiState::Disabled: Result.Accent=Color(EHansaUiColorToken::MutedInk); break;
	default: break;
	}
	Result.Brush = FSlateRoundedBoxBrush(Fill, 4.f,
		Preferences.bHighContrast ? Result.Foreground : Result.Accent,
		Preferences.bHighContrast ? 3.f : (State == EUiState::Default ? 1.f : 2.f));
	return Result;
}
FTableRowStyle GetLedgerRowStyle(bool bHighContrast)
{
	const auto Linen=Color(EHansaUiColorToken::Linen), Paper=Color(EHansaUiColorToken::Parchment), Ink=Color(EHansaUiColorToken::Ink);
	const FSlateRoundedBoxBrush Normal(Linen, 2.f), Alternate(Paper, 2.f);
	const FSlateRoundedBoxBrush Selected(Paper, 2.f, bHighContrast ? Ink : Color(EHansaUiColorToken::Oak), 2.f);
	return FTableRowStyle().SetEvenRowBackgroundBrush(Normal).SetOddRowBackgroundBrush(Alternate)
		.SetEvenRowBackgroundHoveredBrush(Selected).SetOddRowBackgroundHoveredBrush(Selected)
		.SetActiveBrush(Selected).SetActiveHoveredBrush(Selected).SetInactiveBrush(Selected).SetInactiveHoveredBrush(Selected)
		.SetSelectorFocusedBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, 2.f, Ink, bHighContrast ? 4.f : 3.f))
		.SetTextColor(Ink).SetSelectedTextColor(Ink);
}

void SHansaGlyph::Construct(const FArguments& Args)
{
	Glyph=Args._Glyph; bOnDark=Args._OnDark; Size=FMath::Max(20.f,Args._Size);
	SetVisibility(EVisibility::HitTestInvisible);
}
FVector2D SHansaGlyph::ComputeDesiredSize(float) const { return FVector2D(Size,Size); }
const FSlateBrush* GetGeneratedIconBrush(EUiGlyph Glyph, int32 PixelSize)
{
    static const TCHAR* Names[] = {TEXT("Information"),TEXT("Warning"),TEXT("Error"),TEXT("Check"),TEXT("Loading"),TEXT("Arrow"),TEXT("Cursor"),TEXT("Decoration"),TEXT("Bread"),TEXT("Fish"),TEXT("Planks"),TEXT("Building"),TEXT("Road"),TEXT("Production"),TEXT("Storage"),TEXT("Harbor"),TEXT("Civic"),TEXT("Farm"),TEXT("Mill"),TEXT("Bakery"),TEXT("Beer"),TEXT("Coin"),TEXT("Trend"),TEXT("People"),TEXT("Laborer"),TEXT("Wealthy"),TEXT("Pause"),TEXT("Play"),TEXT("Fast"),TEXT("Fastest"),TEXT("Grain"),TEXT("Flour"),TEXT("Timber"),TEXT("Salt"),TEXT("Iron"),TEXT("Tools"),TEXT("Close"),TEXT("Pin"),TEXT("Search"),TEXT("Star"),TEXT("Plus"),TEXT("Minus"),TEXT("Back"),TEXT("Up"),TEXT("Down"),TEXT("Lock"),TEXT("Settings"),TEXT("Research"),TEXT("Save"),TEXT("Map"),TEXT("Eye"),TEXT("Ship"),TEXT("Warehouse"),TEXT("Dock"),TEXT("Market"),TEXT("Hops"),TEXT("Malt"),TEXT("Barrels"),TEXT("LumberCamp"),TEXT("HopFarm"),TEXT("MaltHouse"),TEXT("Cooperage"),TEXT("Brewery")};
    static_assert(UE_ARRAY_COUNT(Names) == int32(EUiGlyph::Count));
    static TMap<FString,TSharedPtr<FSlateDynamicImageBrush>> Brushes;
    int32 Density = 160;
    for (int32 Candidate : {16,20,24,28,32,40,48,56,64,80,96,112,160})
        if (Candidate >= PixelSize) { Density=Candidate; break; }
    const FString Key = FString::Printf(TEXT("%s--%d"),Names[FMath::Clamp(int32(Glyph),0,UE_ARRAY_COUNT(Names)-1)],Density);
    auto& Brush = Brushes.FindOrAdd(Key);
    if (!Brush) Brush = MakeShared<FSlateDynamicImageBrush>(
        FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/Icons/")+Key+TEXT(".png"))),FVector2D(Density,Density));
    return Brush.Get();
}

EUiGlyph GlyphForGood(FName GoodId)
{
    static const TMap<FName,EUiGlyph> Goods={
        {TEXT("Good.Grain"),EUiGlyph::Grain},{TEXT("Good.Flour"),EUiGlyph::Flour},{TEXT("Good.Bread"),EUiGlyph::Bread},
        {TEXT("Good.Fish"),EUiGlyph::Fish},{TEXT("Good.Planks"),EUiGlyph::Planks},{TEXT("Good.Timber"),EUiGlyph::Timber},
        {TEXT("Good.Salt"),EUiGlyph::Salt},{TEXT("Good.Iron"),EUiGlyph::Iron},{TEXT("Good.Tools"),EUiGlyph::Tools},
        {TEXT("Good.Hops"),EUiGlyph::Hops},{TEXT("Good.Malt"),EUiGlyph::Malt},{TEXT("Good.Barrels"),EUiGlyph::Barrels},
        {TEXT("Good.Beer"),EUiGlyph::Beer}};
    const auto* Found=Goods.Find(GoodId);return Found?*Found:EUiGlyph::Information;
}

int32 SHansaGlyph::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out,
    int32 Layer, const FWidgetStyle& WidgetStyle, bool) const
{
    const FVector2D S=G.GetLocalSize();
    const float Side=FMath::Min(S.X,S.Y);
    const FVector2D Offset=(S-FVector2D(Side,Side))*.5;
    const int32 Pixels=FMath::CeilToInt(Side*G.GetAccumulatedLayoutTransform().GetScale());
    // Full-color art retains its generated palette on both navy and linen surfaces.
    FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(FVector2D(Side,Side),FSlateLayoutTransform(Offset)),
        GetGeneratedIconBrush(Glyph,Pixels),ESlateDrawEffect::None,WidgetStyle.GetColorAndOpacityTint());
    return Layer;
}

void SHansaScreenShell::Construct(const FArguments& Args)
{
	const float Gutter=Space(EHansaUiSpacingToken::Spacious);
	ChildSlot[SNew(SSafeZone)[SNew(SOverlay)
		+SOverlay::Slot()[Args._Content.Widget]
		+SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top).Padding(Gutter)[Args._TopBar.Widget]
		+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(Gutter)[Args._BottomTray.Widget]
		+SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(Gutter)[Args._Inspector.Widget]
		+SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(Gutter)[Args._Notifications.Widget]
		+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(Gutter)[Args._Modal.Widget]]];
}

void SHansaSurface::Construct(const FArguments& Args)
{
	Surface=Args._Surface;State=Args._State;Preferences=Args._Preferences;Reason=Args._Reason;
	Style=GetComponentStyle(Surface,State,Preferences);
	ChildSlot[SAssignNew(Border,SBorder).BorderImage(&Style.Brush).Padding(Style.Padding)
		[SNew(SVerticalBox)
		+SVerticalBox::Slot().AutoHeight()
		[SNew(STextBlock).Text(Args._Title).Font(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences))
		.ColorAndOpacity(Style.Foreground).AutoWrapText(true).Visibility(Args._Title.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible)]
		+SVerticalBox::Slot().AutoHeight().Padding(0,Space(EHansaUiSpacingToken::Unit))
		[SAssignNew(StatusSlot,SHorizontalBox)
		+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SAssignNew(StatusIcon,SHansaGlyph).OnDark(Style.bOnDark).Glyph(Style.Glyph)]
		+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(StatusText,STextBlock).AutoWrapText(true)
		.Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)).ColorAndOpacity(Style.Foreground)]]
		+SVerticalBox::Slot().FillHeight(1.f)[Args._Content.Widget]]];
	Refresh();
	const auto Motion=UHansaUiStyleLibrary::GetMotion(EHansaUiMotionToken::PanelTransition,Preferences.bReducedMotion);
	if(Motion.DurationSeconds>0.f)
	{
		Entrance.AddCurve(0.f,Motion.DurationSeconds,ECurveEaseFunction::CubicOut);
		Entrance.Play(AsShared());
		SetRenderOpacity(0.f);
		RegisterActiveTimer(0.f,FWidgetActiveTimerDelegate::CreateSP(this,&SHansaSurface::AnimateEntrance));
	}
}
EActiveTimerReturnType SHansaSurface::AnimateEntrance(double, float)
{
	SetRenderOpacity(Entrance.GetLerp());
	return Entrance.IsPlaying()?EActiveTimerReturnType::Continue:EActiveTimerReturnType::Stop;
}
void SHansaSurface::SetState(EUiState NewState,const FText& InReason){State=NewState;Reason=InReason;Refresh();}
void SHansaSurface::Refresh()
{
	Style=GetComponentStyle(Surface,State,Preferences);
	StatusText->SetText(State==EUiState::Default?Reason:Reason.IsEmpty()?GetStateLabel(State):FText::Format(LOCTEXT("StateReason","{0}: {1}"),GetStateLabel(State),Reason));
	StatusSlot->SetVisibility(State==EUiState::Default&&Reason.IsEmpty()?EVisibility::Hidden:EVisibility::Visible);
	StatusIcon->SetGlyph(Style.Glyph);
	Border->Invalidate(EInvalidateWidgetReason::Paint);
}


namespace {
bool SymbolIcon(const FText& Label, EUiGlyph& Glyph, FText& Accessible)
{
    const FString S=Label.ToString();
    if(S==TEXT("×")||S==TEXT("✕")){Glyph=EUiGlyph::Close;Accessible=LOCTEXT("IconClose","Close");}
    else if(S==TEXT("+")){Glyph=EUiGlyph::Plus;Accessible=LOCTEXT("IconIncrease","Increase");}
    else if(S==TEXT("−")||S==TEXT("-")){Glyph=EUiGlyph::Minus;Accessible=LOCTEXT("IconDecrease","Decrease");}
    else if(S==TEXT("◀")){Glyph=EUiGlyph::Back;Accessible=LOCTEXT("IconBack","Back");}
    else if(S==TEXT("▶")){Glyph=EUiGlyph::Play;Accessible=LOCTEXT("IconPlay","Play");}
    else if(S==TEXT("⏸")){Glyph=EUiGlyph::Pause;Accessible=LOCTEXT("IconPause","Pause");}
    else return false;
    return true;
}
}

void SHansaAction::Construct(const FArguments& Args)
{
	State=Args._State;Preferences=Args._Preferences;Action=Args._OnClicked;bCompact=Args._Compact;
	Style=UHansaUiStyleLibrary::GetButtonStyle(Args._Kind);
	// A press never moves or rescales content; state is expressed by fill and outline.
	if(bCompact) Style.SetNormalPadding(FMargin(12.f,4.f));
	Style.SetPressedPadding(Style.NormalPadding);
	// Target size includes button padding; it is not an additional content height.
	const float Target = Space(EHansaUiSpacingToken::ControllerFocusTarget) / FMath::Min(1.f,FMath::Clamp(Preferences.UiScale,.8f,1.4f));
	const float ContentHeight = FMath::Max(0.f, Target-Style.NormalPadding.Top-Style.NormalPadding.Bottom);
	TSharedRef<SWidget> Content=Args._Content.Widget;
    if(Content==SNullWidget::NullWidget)
    {
        EUiGlyph Icon;
        if(SymbolIcon(Args._Label,Icon,IconLabel)) Content=SAssignNew(LabelIcon,SHansaGlyph).Glyph(Icon).Size(24);
        else Content=SAssignNew(LabelText,STextBlock).Text(Args._Label).WrappingPolicy(ETextWrappingPolicy::DefaultWrapping).Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences))
            .ColorAndOpacity(FSlateColor::UseForeground()).AutoWrapText(!bCompact);
    }

	SButton::Construct(SButton::FArguments().ButtonStyle(&Style).ContentPadding(0).IsFocusable(true)
		.OnClicked(this,&SHansaAction::Activate)
		[SNew(SReadableContent)[SNew(SBox)
		.MinDesiredWidth(FMath::Max(0.f,Target-Style.NormalPadding.Left-Style.NormalPadding.Right))
		.MinDesiredHeight(ContentHeight).VAlign(VAlign_Center)[Content]]]);
	SetState(State,Args._Reason);
}
FReply SHansaAction::OnFocusReceived(const FGeometry& Geometry,const FFocusEvent& Event){auto Reply=SButton::OnFocusReceived(Geometry,Event);FocusHandler.ExecuteIfBound();return Reply;}
void SHansaAction::SetLabel(const FText& Label)
{
    if(LabelText.IsValid())LabelText->SetText(Label);
    if(LabelIcon.IsValid()){EUiGlyph Icon;if(SymbolIcon(Label,Icon,IconLabel)){LabelIcon->SetGlyph(Icon);SetToolTipText(IconLabel);}}
}
void SHansaAction::SetState(EUiState NewState,const FText& Reason)
{
	State=NewState; SetEnabled(CanActivate(State));
	SetToolTipText(Reason.IsEmpty() ? (!IconLabel.IsEmpty()?IconLabel:GetStateLabel(State)) : Reason);
	// State belongs to the action semantics/tooltip and outline, not a second
	// caption row that inflates every control. Contextual labels describe toggles.
	Invalidate(EInvalidateWidgetReason::Paint);
}
FReply SHansaAction::Activate(){return CanActivate(State)&&Action.IsBound()?Action.Execute():FReply::Unhandled();}
int32 SHansaAction::OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,
	int32 Layer,const FWidgetStyle& WidgetStyle,bool ParentEnabled) const
{
	int32 Top=SButton::OnPaint(Args,G,Clip,Out,Layer,WidgetStyle,ParentEnabled);
	if(State==EUiState::Selected) Frame(Out,++Top,G,Color(EHansaUiColorToken::Brass),2.f,2.f);
	if(HasKeyboardFocus() || HasUserFocus(0))
	{
		const auto Focus=UHansaUiStyleLibrary::GetFocusStyle(Preferences.bHighContrast);
		Frame(Out,++Top,G,Color(EHansaUiColorToken::Ink),Focus.RingWidth+4.f,5.f);
		Frame(Out,++Top,G,Focus.Color,Focus.RingWidth,5.f);
	}
	return Top;
}

void SHansaDiagram::Construct(const FArguments& Args)
{
	Kind=Args._Kind; Preferences=Args._Preferences;
	ChildSlot.VAlign(VAlign_Bottom)[SAssignNew(SummaryText,STextBlock).AutoWrapText(true)
		.Font(GetComponentFont(EHansaUiTypographyToken::Body,Preferences)).ColorAndOpacity(Color(EHansaUiColorToken::Ink))];
	SetData(Args._Fraction,Args._State,Args._Summary,Args._Series);
}
void SHansaDiagram::SetData(float InFraction,EUiState InState,const FText& Summary,TArray<FUiChartSeries> InSeries)
{
	Fraction=FMath::IsFinite(InFraction)?FMath::Clamp(InFraction,0.f,1.f):0.f;State=InState;Series=MoveTemp(InSeries);
	FText Text=Summary;
	if(State!=EUiState::Default) Text=FText::Format(LOCTEXT("DiagramStatus","{0} — {1}"),GetStateLabel(State),Summary);
	for(const auto& Item:Series) Text=FText::Format(LOCTEXT("Legend","{0} | {1}"),Text,Item.Label);
	SummaryText->SetText(Text);Invalidate(EInvalidateWidgetReason::Paint);
}
int32 SHansaDiagram::OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,
	int32 Layer,const FWidgetStyle& WidgetStyle,bool Enabled) const
{
	const auto S=G.GetLocalSize();
	const double Bottom=FMath::Max(8.,S.Y-SummaryText->GetDesiredSize().Y-16.);
	const double Right=FMath::Max(8.,S.X-8.);
	const auto Ink=Color(EHansaUiColorToken::Ink);
	const auto Accent=GetComponentStyle(EUiSurface::Panel,State,Preferences).Accent;
	if(Kind==EUiDiagram::Chart)
	{
		Line(Out,Layer,G,{{8,8},{8,Bottom},{Right,Bottom}},Ink);
		if(State!=EUiState::Empty && State!=EUiState::Loading && State!=EUiState::Error)
		for(const auto& Item:Series)
		{
			TArray<FVector2D> Points;
			for(const auto& P:Item.Points) if(FMath::IsFinite(P.X)&&FMath::IsFinite(P.Y))
				Points.Add({8+FMath::Clamp(P.X,0.,1.)*(Right-8),Bottom-FMath::Clamp(P.Y,0.,1.)*(Bottom-8)});
			const bool Dashed=Item.bEstimated||State==EUiState::Stale||Item.Role==EUiSeries::Incoming||Item.Role==EUiSeries::Reserve;
			// Dark under-stroke guarantees contrast for Brass/Blue on paper.
			for(int32 I=1;I<Points.Num();++I)
			{
				const FVector2D A=Points[I-1],B=Points[I]; const double Length=(B-A).Size();
				const double Dash=Item.Role==EUiSeries::Reserve?2.:8.;
				for(double Start=0;Start<Length;Start+=Dashed?Dash*2:Length)
				{
					const auto From=FMath::Lerp(A,B,Start/Length),To=FMath::Lerp(A,B,FMath::Min(1.,(Start+(Dashed?Dash:Length))/Length));
					Line(Out,Layer+1,G,{From,To},Ink,4.f);Line(Out,Layer+2,G,{From,To},Color(SeriesColor(Item.Role)),2.f);
				}
			}
		}
	}
	else if(Kind==EUiDiagram::Connector)
		Line(Out,Layer,G,{{8,Bottom/2},{Right,Bottom/2},{Right-8,Bottom/2-8},{Right,Bottom/2},{Right-8,Bottom/2+8}},Ink,2.f);
	else if(Kind==EUiDiagram::Progress)
	{
		Line(Out,Layer,G,{{8,Bottom/2},{Right,Bottom/2}},Ink,12.f);
		if(State!=EUiState::Loading&&State!=EUiState::Empty)
			Line(Out,Layer+1,G,{{8,Bottom/2},{8+(Right-8)*Fraction,Bottom/2}},Color(EHansaUiColorToken::ProsperityTeal),8.f);
	}
	else
	{
		const double Cell=Space(EHansaUiSpacingToken::Spacious);
		for(double X=8;X<Right;X+=Cell) for(double Y=8;Y<Bottom-Cell;Y+=Cell)
		{
			const double R=FMath::Min(Right,X+Cell);
			Line(Out,Layer,G,{{X,Y},{R,Y},{R,Y+Cell},{X,Y+Cell},{X,Y}},Ink,1.f);
			if(State==EUiState::Error || State==EUiState::Warning)
				Line(Out,Layer+1,G,{{X,Y+Cell},{R,Y}},Accent,2.f);
		}
	}
	return SCompoundWidget::OnPaint(Args,G,Clip,Out,Layer+3,WidgetStyle,Enabled);
}
}
#undef LOCTEXT_NAMESPACE
