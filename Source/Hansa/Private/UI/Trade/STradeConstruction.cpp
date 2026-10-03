#include "STradeConstruction.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#define LOCTEXT_NAMESPACE "STradeConstruction"
namespace Hansa::UI
{
class STradeCostDossier final : public SBorder
{
public:
 virtual bool SupportsKeyboardFocus() const override { return true; }
 virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& O,int32 L,const FWidgetStyle& S,bool Enabled) const override
 {
  const int32 End=SBorder::OnPaint(A,G,R,O,L,S,Enabled);
  if(HasKeyboardFocus()){const auto Z=G.GetLocalSize();TArray<FVector2D> Points={{1,1},{Z.X-1,1},{Z.X-1,Z.Y-1},{1,Z.Y-1},{1,1}};FSlateDrawElement::MakeLines(O,End+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass),true,2);}
  return End+1;
 }
};
class STradeLeaseDiagram final : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(STradeLeaseDiagram){} SLATE_END_ARGS()
 void Construct(const FArguments&){}
 FHansaTradeConstructionPlot Plot;
 bool bHasPlot=false;
 virtual FVector2D ComputeDesiredSize(float) const override{return {200,168};}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
 {
  if(!bHasPlot)return Layer;
  const double W=double(Plot.BoundsMax.X)-Plot.BoundsMin.X+1,H=double(Plot.BoundsMax.Y)-Plot.BoundsMin.Y+1;
  if(W<=0||H<=0)return Layer;
  const auto Size=G.GetLocalSize();const float Scale=FMath::Min((Size.X-32)/W,(Size.Y-32)/H);
  const FVector2D Origin((Size.X-W*Scale)/2,(Size.Y-H*Scale)/2);
  auto Line=[&](FVector2D A,FVector2D B,const FLinearColor& C,float Thickness){TArray<FVector2D> P={A,B};FSlateDrawElement::MakeLines(Out,Layer+1,G.ToPaintGeometry(),P,ESlateDrawEffect::None,C,true,Thickness);};
  const FLinearColor Navy=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy),Brass=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
  FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,Navy);
  for(float Y=0;Y<H*Scale;Y+=12)Line(Origin+FVector2D(0,Y),Origin+FVector2D(FMath::Min(12.f,float(W*Scale)),FMath::Max(0.f,Y-12)),Brass.CopyWithNewOpacity(.5f),1);
  for(const auto C:Plot.OccupiedCells){FVector2D A=Origin+FVector2D(C.X-Plot.BoundsMin.X,C.Y-Plot.BoundsMin.Y)*Scale;Line(A,A+FVector2D(Scale,Scale),Brass,2);Line(A+FVector2D(Scale,0),A+FVector2D(0,Scale),Brass,2);}
  const FVector2D End=Origin+FVector2D(W*Scale,H*Scale);
  Line(Origin,{End.X,Origin.Y},Brass,2);Line({End.X,Origin.Y},End,Brass,2);Line(End,{Origin.X,End.Y},Brass,2);Line({Origin.X,End.Y},Origin,Brass,2);
  return Layer+1;
 }
};
void STradeConstruction::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& C)
{
 Initialize(C);
 Visit=MakeControl(TEXT("TradeMap.Construction.Visit"),LOCTEXT("Visit","Inspect city"),EHansaUiButtonStyle::Secondary);
 Place=MakeControl(TEXT("TradeMap.Construction.Place"),LOCTEXT("Place","Place selected building"),EHansaUiButtonStyle::Primary);
 ChildSlot[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Title","Leased construction")).TextStyle(&LightHeadingStyle)]
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Scroll,SScrollBox)
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(Status,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SScrollBox::Slot()[SAssignNew(Plots,SVerticalBox)]
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(Diagram,STradeLeaseDiagram).Visibility(EVisibility::Collapsed)]
  +SScrollBox::Slot()[SNew(STextBlock).Text(LOCTEXT("Legend","Brass boundaries on the map and in the city mark your leases. Crosses mark occupied cells; shaded sites and streets are protected. Outside is autonomous city.")).TextStyle(&LightCaptionStyle).AutoWrapText(true)]
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(Cards,SVerticalBox)]
  +SScrollBox::Slot()[SAssignNew(Locked,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(Dossier,STradeCostDossier).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Padding(4)[SAssignNew(Detail,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
  +SScrollBox::Slot()[Pair(TEXT("TradeMap.Construction.Presence"),LOCTEXT("Presence","Review rights"),TEXT("TradeMap.Construction.Ledger"),LOCTEXT("Ledger","Station materials"))]
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(WorldStatus,STextBlock).Text(LOCTEXT("WorldUnavailable","Choose an active lease and permitted building, then place it in Rostock. The authority checks rights, footprint and station materials again on confirmation.")).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight()[Place.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight()[Visit.ToSharedRef()]];
 MapWidget(TEXT("TradeMap.Construction.Status"),Status);MapWidget(TEXT("TradeMap.Construction.Detail"),Dossier);MapWidget(TEXT("TradeMap.Construction.Bounds"),Diagram);MapWidget(TEXT("TradeMap.Construction.WorldStatus"),WorldStatus);
}
void STradeConstruction::Refresh(const FHansaTradeConstruction& V)
{
 const bool ChoiceChanged=View.SelectedBuilding!=V.SelectedBuilding;View=V;Status->SetText(V.Status);Locked->SetText(V.LockedCategories);
 const bool RebuildPlots=PlotActions.Num()!=V.Plots.Num()||V.Plots.ContainsByPredicate([&](const auto& P){return !PlotActions.Contains(LexToString(P.Id));});
 if(RebuildPlots){Plots->ClearChildren();PlotActions.Reset();for(const auto& P:V.Plots){const FString Key=LexToString(P.Id);auto A=MakeControl(*(TEXT("TradeMap.Construction.Plot.")+Key),P.Summary,EHansaUiButtonStyle::Secondary);PlotActions.Add(Key,A);Plots->AddSlot().AutoHeight().Padding(0,4)[A];}}
 for(const auto& P:V.Plots){auto A=PlotActions[LexToString(P.Id)];A->SetLabel(P.Summary);A->SetState(P.Id==V.SelectedLease?EUiState::Selected:EUiState::Default);}
 const auto* P=V.Plots.FindByPredicate([&](const auto& X){return X.Id==V.SelectedLease;});Diagram->bHasPlot=P!=nullptr;if(P)Diagram->Plot=*P;Diagram->Invalidate(EInvalidateWidgetReason::Paint);Diagram->SetVisibility(EVisibility::Collapsed);
 const bool RebuildCards=CardActions.Num()!=V.Options.Num()||V.Options.ContainsByPredicate([&](const auto& O){return !CardActions.Contains(O.Id.ToString());});
 if(RebuildCards){Cards->ClearChildren();CardActions.Reset();for(const auto& O:V.Options){auto A=MakeControl(*(TEXT("TradeMap.Construction.")+O.Id.ToString()),O.Name,EHansaUiButtonStyle::Secondary);A->SetContent(SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(O.Id==TEXT("Building.Market")?EUiGlyph::Market:O.Id==TEXT("Building.Warehouse")?EUiGlyph::Warehouse:EUiGlyph::Dock).Size(48)]+SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(O.Name).TextStyle(&LightHeadingStyle).AutoWrapText(true)]);CardActions.Add(O.Id.ToString(),A);Cards->AddSlot().AutoHeight().Padding(0,4)[A];}}
 for(const auto& O:V.Options){auto A=CardActions[O.Id.ToString()];A->SetState(O.Id==V.SelectedBuilding?EUiState::Selected:EUiState::Default);A->SetToolTipText(FText::Format(LOCTEXT("Hint","{0}\n{1}"),O.Reason,O.Detail));}
 const auto* O=V.Options.FindByPredicate([&](const auto& X){return X.Id==V.SelectedBuilding;});Detail->SetText(O?FText::Format(LOCTEXT("Detail","{0}\n{1}\n{2}"),O->Name,O->Reason,O->Detail):LOCTEXT("Choose","Select a building to inspect its costs, workforce and prerequisites."));
 if(ChoiceChanged&&!V.SelectedBuilding.IsNone())Scroll->ScrollDescendantIntoView(Detail,false,EDescendantScrollDestination::TopOrLeft);
 Place->SetEnabled(V.City==TEXT("City.Rostock")&&O&&O->bPermitted&&O->bAffordable);
 if(Model.IsValid()&&Model->IsRemoteView()){Place->SetEnabled(false);Place->SetToolTipText(Model->RemoteActionReason(TEXT("TradeMap.Construction.Place")));WorldStatus->SetText(Model->RemoteActionReason(TEXT("TradeMap.Construction.Place")));}
 Visit->SetEnabled(V.City==TEXT("City.Rostock")&&(!Model.IsValid()||!Model->IsRemoteView()));Visit->SetToolTipText(Model.IsValid()&&Model->IsRemoteView()?Model->RemoteActionReason(TEXT("TradeMap.Construction.Visit")):LOCTEXT("VisitHint","Opens the rendered city for inspection. The selected lease and building remain in Expansion when you reopen Trade."));
}
TArray<FString> STradeConstruction::FocusOrder() const
{
 TArray<FString> R;for(const auto& P:View.Plots)R.Add(TEXT("TradeMap.Construction.Plot.")+LexToString(P.Id));for(const auto& O:View.Options)R.Add(TEXT("TradeMap.Construction.")+O.Id.ToString());R.Add(TEXT("TradeMap.Construction.Detail"));R.Append({TEXT("TradeMap.Construction.Place"),TEXT("TradeMap.Construction.Visit"),TEXT("TradeMap.Construction.Presence"),TEXT("TradeMap.Construction.Ledger")});return R;
}
}
#undef LOCTEXT_NAMESPACE
