#include "UI/SHansaShipInspector.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HansaShipInspector"
namespace Hansa::UI
{
namespace
{
FLinearColor Color(EHansaUiColorToken T) { return UHansaUiStyleLibrary::GetColor(T); }
FText Quantity(int64 Raw) { FNumberFormattingOptions F; F.SetMaximumFractionalDigits(1); return FText::AsNumber(Raw / 1000., &F); }
}
TSharedRef<STextBlock> SHansaShipInspector::Text(FText Value, EHansaUiTypographyToken Token, bool Dark)
{
    return SNew(STextBlock).Text(Value).Font(GetComponentFont(Token, Preferences)).AutoWrapText(true)
        .ColorAndOpacity(Color(Dark ? EHansaUiColorToken::Chalk : EHansaUiColorToken::Ink));
}
TSharedRef<SHansaAction> SHansaShipInspector::Action(FName Id, FText Label, EHansaUiButtonStyle Kind)
{
    auto W = SNew(SHansaAction).Preferences(Preferences).Compact(true).Kind(Kind).Typography(EHansaUiTypographyToken::SerifBody).Label(Label)
        .OnClicked_Lambda([this,Id] { return Model.IsValid() && Model->ActivateAction(Id) ? FReply::Handled() : FReply::Unhandled(); });
    W->SetFocusHandler(FSimpleDelegate::CreateLambda([this,Id] { if (Model.IsValid()) Model->SetFocusedSemanticId(Id); }));
    Targets.Add(Id.ToString(), W); Buttons.Add(Id.ToString(), W); return W;
}
TSharedRef<SWidget> SHansaShipInspector::Section(FText Label, const TSharedRef<SWidget>& Content)
{
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,8)[SNew(SBox).HeightOverride(1)[SNew(SBorder).BorderImage(&Rule).Padding(0)]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Text(Label,EHansaUiTypographyToken::Heading2)]
        +SVerticalBox::Slot().AutoHeight()[Content];
}
void SHansaShipInspector::Construct(const FArguments& Args)
{
    // The bounded inspector mixes a fixed header/footer and a clipped live body.
    // Repaint this small subtree so cached layer IDs cannot obscure fixed content
    // when expanding the body or changing its scroll offset.
    ForceVolatile(true);
    Model=Args._Model; Preferences=Args._Preferences;
    Paper=GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
    Navy=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
    Rule=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Brass).CopyWithNewOpacity(.55f),0.f);
    Card=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Linen),2.f,Color(EHansaUiColorToken::Brass).CopyWithNewOpacity(.65f),1.f);
    BarStyle.SetBackgroundImage(FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Parchment),2.f));
    BarStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White,2.f)); BarStyle.EnableFillAnimation=false;
    Portrait=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/ShipDetails/cog--384x256.png"))),FVector2D(384,256));
    EmptyHold=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/ShipDetails/hold--64.png"))),FVector2D(64));
    Linen=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeWorkspace/Linen.png"))),FVector2D(1254));
    Linen->Tiling=ESlateBrushTileType::Both;
    Identity=Text(FText(),EHansaUiTypographyToken::Heading1,true);
    State=Text(FText(),EHansaUiTypographyToken::SerifBody,true);
    Voyage=Text(FText(),EHansaUiTypographyToken::Data); Location=Text(FText(),EHansaUiTypographyToken::Caption);
    Cargo=Text(FText(),EHansaUiTypographyToken::Data); Upkeep=Text(FText()); History=Text(FText()); Cause=Text(FText());
    Result=Text(FText(),EHansaUiTypographyToken::Caption,true); Overflow=Text(FText(),EHansaUiTypographyToken::Caption);
    auto Slots=SNew(SHorizontalBox);
    for(int32 I=0;I<3;++I)
    {
        auto Label=Text(FText(),EHansaUiTypographyToken::SerifBody); Label->SetJustification(ETextJustify::Center);
        auto Amount=Text(FText(),EHansaUiTypographyToken::Data); Amount->SetJustification(ETextJustify::Center);
        auto Glyph=SNew(SHansaGlyph).Size(48);
        auto Empty=SNew(SImage).Image(EmptyHold.Get());
        auto Slot=SNew(SBorder).BorderImage(&Card).Padding(4,8)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(FText::Format(LOCTEXT("Slot","Slot {0}"),FText::AsNumber(I+1)),EHansaUiTypographyToken::Caption)]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,4)[SNew(SBox).WidthOverride(64).HeightOverride(64)[SNew(SOverlay)+SOverlay::Slot()[Empty]+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Glyph]]]
            +SVerticalBox::Slot().AutoHeight()[Label]
            +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[Amount]];
        Slots->AddSlot().FillWidth(1).Padding(I==0?0:4,0,0,0)[Slot];
        SlotLabels.Add(Label); SlotAmounts.Add(Amount); SlotGlyphs.Add(Glyph); EmptyImages.Add(Empty);
        Targets.Add(FString::Printf(TEXT("Inspector.Ship.Slot.%d"),I),Slot);
    }
    auto Footer=SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,2,0)[Action(TEXT("Inspector.Ship.Route"),LOCTEXT("Route","Route"),EHansaUiButtonStyle::Primary)]
            +SHorizontalBox::Slot().FillWidth(1).Padding(2,0,0,0)[Action(TEXT("Inspector.Action.OpenRelated"),LOCTEXT("Map","Trade map [C]"))]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,2,0)[Action(TEXT("Inspector.Action.Frame"),LOCTEXT("Frame","Frame ship [F]"))]
            +SHorizontalBox::Slot().FillWidth(1).Padding(2,0,0,0)[Action(TEXT("Inspector.Action.Pin"),LOCTEXT("Pin","Pin tracker [P]"))]]
        +SVerticalBox::Slot().AutoHeight()[Result.ToSharedRef()];
    auto Header=SNew(SBorder).BorderImage(&Navy).Padding(0)[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(12,8)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Identity.ToSharedRef()]
            +SHorizontalBox::Slot().AutoWidth().Padding(8,0,0,0)[Action(TEXT("Inspector.Close"),LOCTEXT("Close","Close"),EHansaUiButtonStyle::Icon)]]
        +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(384).HeightOverride(104).Clipping(EWidgetClipping::ClipToBounds)
            .Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::HitTestInvisible)
            [SNew(SOverlay)+SOverlay::Slot().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(384).HeightOverride(256)[SNew(SImage).Image(Portrait.Get())]]]]
        +SVerticalBox::Slot().AutoHeight().Padding(12,6,12,8)[State.ToSharedRef()]];
    ChildSlot[SNew(SBorder).BorderImage(Preferences.bHighContrast?&Paper:Linen.Get()).Padding(0)[SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[Header]
        +SVerticalBox::Slot().FillHeight(1).Padding(16,0)[SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("Voyage","Voyage"),SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Voyage.ToSharedRef()]
                +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).HeightOverride(7)[SAssignNew(Progress,SProgressBar).Style(&BarStyle).FillColorAndOpacity(Color(EHansaUiColorToken::ProsperityTeal))]]
                +SVerticalBox::Slot().AutoHeight()[Location.ToSharedRef()])]
            +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("Cargo","Cargo hold"),SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Cargo.ToSharedRef()]
                +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,8)[SNew(SBox).HeightOverride(4)[SAssignNew(Capacity,SProgressBar).Style(&BarStyle).FillColorAndOpacity(Color(EHansaUiColorToken::Brass))]]
                +SVerticalBox::Slot().AutoHeight()[Slots]
                +SVerticalBox::Slot().AutoHeight()[Overflow.ToSharedRef()])]
            +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("Operating","Operating details"),Upkeep.ToSharedRef())]
            +SVerticalBox::Slot().AutoHeight()[Section(LOCTEXT("History","Recent activity"),History.ToSharedRef())]
            +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,4)[Action(TEXT("Inspector.Action.OpenCause"),LOCTEXT("Details","Details"))]
            +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,12)[SAssignNew(Details,SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Cause.ToSharedRef()]
                +SVerticalBox::Slot().AutoHeight()[SAssignNew(ExtraActions,SVerticalBox)]]]]
        +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&Navy).Padding(12,8)[Footer]]]];
    Targets.Add(TEXT("Inspector.Identity"),Identity); Targets.Add(TEXT("Inspector.Result"),Cargo);
    Targets.Add(TEXT("Inspector.Flows"),Voyage); Targets.Add(TEXT("Inspector.Ship.Progress"),Progress);
    Targets.Add(TEXT("Inspector.Ship.Capacity"),Capacity); Targets.Add(TEXT("Inspector.Ship.Scroll"),Scroll);
    Targets.Add(TEXT("Inspector.History"),History); Targets.Add(TEXT("Inspector.Problem.Cause"),Cause);
}
void SHansaShipInspector::Refresh(const FHansaInspectorSnapshot& S)
{
    if(Presented.ObjectStableId!=S.ObjectStableId)Scroll->ScrollToStart();
    const auto& D=S.Ship;
    Identity->SetText(S.Identity); State->SetText(S.State);
    Voyage->SetText(D.bVoyage?FText::Format(LOCTEXT("Progress","{0} complete"),FText::AsPercent(D.ProgressBasisPoints/10000.)):S.State);
    Progress->SetPercent(D.ProgressBasisPoints/10000.f); Progress->SetVisibility(D.bVoyage?EVisibility::Visible:EVisibility::Collapsed);
    const auto* Flow=S.Flows.FindByPredicate([](const auto& F){return F.StableId==TEXT("Inspector.Cargo.Progress");});
    Location->SetText(Flow?Flow->State:FText());
    Cargo->SetText(FText::Format(LOCTEXT("Capacity","{0} / {1} units · shared capacity"),Quantity(D.Cargo),Quantity(D.Capacity)));
    Capacity->SetPercent(D.Capacity>0?FMath::Clamp(float(double(D.Cargo)/D.Capacity),0.f,1.f):0.f);
    for(int32 I=0;I<3;++I)
    {
        const bool Known=D.bSlotsKnown && D.Slots.IsValidIndex(I);
        const bool Occupied=Known && D.Slots[I].Quantity>0;
        FText Name=LOCTEXT("Empty","Empty");
        if(Occupied){FString Label=D.Slots[I].GoodId.ToString();Label.RemoveFromStart(TEXT("Good."));Name=FText::FromString(FName::NameToDisplayString(Label,false));}
        SlotLabels[I]->SetText(Known?Name:LOCTEXT("Unknown","Unavailable"));
        SlotAmounts[I]->SetText(Known?FText::Format(LOCTEXT("Units","{0} units"),Quantity(D.Slots[I].Quantity)):FText());
        SlotGlyphs[I]->SetVisibility(Occupied?EVisibility::HitTestInvisible:EVisibility::Collapsed);
        EmptyImages[I]->SetVisibility(Known&&!Occupied?EVisibility::HitTestInvisible:EVisibility::Collapsed);
        if(Occupied)SlotGlyphs[I]->SetGlyph(GlyphForGood(D.Slots[I].GoodId));
    }
    Overflow->SetText(D.Slots.Num()>3?LOCTEXT("Overflow","Recovery cargo is also aboard. Open the trade map to review and unload it."):FText());
    Overflow->SetVisibility(D.Slots.Num()>3?EVisibility::Visible:EVisibility::Collapsed);
    Upkeep->SetText(FText::Format(LOCTEXT("Ledger","Owner · Your merchant house\nUpkeep · {0} pfennig per travel tick"),FText::AsNumber(D.Upkeep)));
    FText HistoryText=LOCTEXT("NoHistory","No recent cargo transfer recorded.");
    if(!S.History.IsEmpty())HistoryText=FText::Format(LOCTEXT("Receipt","{0}\n{1}"),S.History.Last().Label,S.History.Last().Age);
    History->SetText(HistoryText);
    Cause->SetText(FText::Format(LOCTEXT("Cause","{0}\n{1}\n{2}"),S.Causal.Cause,S.Causal.Evidence,S.Causal.Remedy));
    Details->SetVisibility(S.bCauseExpanded?EVisibility::Visible:EVisibility::Collapsed);
    Buttons[TEXT("Inspector.Action.OpenCause")]->SetLabel(S.bCauseExpanded?LOCTEXT("Less","Hide details"):LOCTEXT("Details","Details"));
    // Update existing controls without replacing focused buttons on every simulation tick.
    for(const auto& A:S.Actions)
    {
        if(!Buttons.Contains(A.StableId.ToString()))ExtraActions->AddSlot().AutoHeight().Padding(0,4)[Action(A.StableId,A.Label)];
        auto B=Buttons[A.StableId.ToString()];
        if(A.StableId!=TEXT("Inspector.Action.Frame") && A.StableId!=TEXT("Inspector.Action.OpenRelated"))B->SetLabel(A.Label);
        B->SetVisibility(EVisibility::Visible);
        B->SetState(!A.bEnabled?EUiState::Disabled:A.bSelected?EUiState::Selected:EUiState::Default,A.bEnabled?A.ToolTip:A.DisabledReason);
    }
    FocusOrder={TEXT("Inspector.Close"),TEXT("Inspector.Action.OpenCause")};
    for(const auto& Pair:Buttons)
    {
        if(Pair.Key==TEXT("Inspector.Close")||Pair.Key==TEXT("Inspector.Action.OpenCause"))continue;
        const auto* A=S.Actions.FindByPredicate([&](const auto& V){return V.StableId==FName(*Pair.Key);});
        if(!A)Pair.Value->SetVisibility(EVisibility::Collapsed);
    }
    for(const auto& A:S.Actions)
    {
        const bool Primary=A.StableId==TEXT("Inspector.Ship.Route")||A.StableId==TEXT("Inspector.Action.Frame")||A.StableId==TEXT("Inspector.Action.Pin")||A.StableId==TEXT("Inspector.Action.OpenRelated");
        if(A.bEnabled&&(Primary||S.bCauseExpanded))FocusOrder.Add(A.StableId.ToString());
    }
    Result->SetText(S.LastActionResult); Result->SetVisibility(S.LastActionResult.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
    Presented=S;
}
TSharedPtr<SWidget> SHansaShipInspector::Resolve(const FString& Id) const
{
    if(Id==TEXT("Inspector.Root"))return ConstCastSharedRef<SHansaShipInspector>(SharedThis(this));
    const auto* W=Targets.Find(Id);return W?*W:nullptr;
}
void SHansaShipInspector::Reveal(const FString& Id)
{
    if(Id==TEXT("Inspector.Action.OpenCause")||Id.StartsWith(TEXT("Inspector.Ship.Home"))||Id.StartsWith(TEXT("Inspector.Ship.Stop")))
        if(auto W=Resolve(Id))Scroll->ScrollDescendantIntoView(W,false,EDescendantScrollDestination::IntoView);
}
bool SHansaShipInspector::Focus(const FString& Id)
{
    auto W=Resolve(Id);if(!Presented.bOpen||!FocusOrder.Contains(Id)||!W||!W->IsEnabled())return false;
    Reveal(Id);if(Model.IsValid())Model->SetFocusedSemanticId(FName(*Id));
    FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);return true;
}
TArray<FHansaHudSemanticNode> SHansaShipInspector::GetSemanticSnapshot() const
{
    TArray<FHansaHudSemanticNode> Out;
    auto Add=[&](FString Id,FString Label,FString Value,EHansaHudSemanticRole Role=EHansaHudSemanticRole::Status)
    {
        FHansaHudSemanticNode N;N.Id=Id;N.ParentId=Id==TEXT("Inspector.Root")?TEXT("HUD.InspectorHost"):TEXT("Inspector.Root");N.Label=Label;N.Role=Role;N.State.Value=Value;N.State.ValueType=TEXT("ship");N.State.bVisible=Presented.bOpen;N.State.bEnabled=true;N.bCanFocus=FocusOrder.Contains(Id);N.bCanActivate=Buttons.Contains(Id);N.State.bFocused=Presented.FocusedSemanticId==FName(*Id);
        if(auto W=Resolve(Id)){const auto P=W->GetCachedGeometry().GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition();const auto Z=W->GetCachedGeometry().GetAbsoluteSize();N.Bounds=FIntRect(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.X+Z.X),FMath::RoundToInt(P.Y+Z.Y));N.State.bEnabled=W->IsEnabled();N.State.bVisible&=W->GetVisibility().IsVisible();}
        if(Id==TEXT("Inspector.Problem.Cause")||Id==TEXT("Inspector.Ship.Home")||Id==TEXT("Inspector.Ship.Stop"))N.State.bVisible&=Presented.bCauseExpanded;
        N.bCanFocus&=N.State.bVisible&&N.State.bEnabled;N.bCanActivate&=N.State.bVisible&&N.State.bEnabled;Out.Add(MoveTemp(N));
    };
    Add(TEXT("Inspector.Root"),TEXT("Ship details"),Presented.ObjectStableId.ToString(),EHansaHudSemanticRole::Panel);
    Add(TEXT("Inspector.Identity"),Presented.Identity.ToString(),Presented.State.ToString());
    Add(TEXT("Inspector.Result"),TEXT("Cargo and shared capacity"),Cargo->GetText().ToString());
    Add(TEXT("Inspector.Ship.Progress"),TEXT("Voyage progress"),FString::FromInt(Presented.Ship.ProgressBasisPoints));
    Add(TEXT("Inspector.Ship.Scroll"),TEXT("Ship detail body"),TEXT("scroll"),EHansaHudSemanticRole::Panel);
    for(int32 I=0;I<3;++I)Add(FString::Printf(TEXT("Inspector.Ship.Slot.%d"),I),SlotLabels[I]->GetText().ToString(),SlotAmounts[I]->GetText().ToString());
    Add(TEXT("Inspector.History"),TEXT("Recent activity"),History->GetText().ToString());
    Add(TEXT("Inspector.Problem.Cause"),TEXT("Details"),Cause->GetText().ToString());
    for(const auto& B:Buttons){const auto* A=Presented.Actions.FindByPredicate([&](const auto& V){return V.StableId==FName(*B.Key);});Add(B.Key,A?A->Label.ToString():B.Key,A?A->DisabledReason.ToString():FString(),EHansaHudSemanticRole::Button);}
    return Out;
}
}
#undef LOCTEXT_NAMESPACE
