#include "UI/SHansaResidenceInspector.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#define LOCTEXT_NAMESPACE "HansaResidenceInspector"
namespace Hansa::UI {
namespace {
class SResidenceNeedRow final:public SBorder {
public:
 SLATE_BEGIN_ARGS(SResidenceNeedRow){} SLATE_EVENT(FSimpleDelegate,OnFocused) SLATE_DEFAULT_SLOT(FArguments,Content) SLATE_END_ARGS()
 void Construct(const FArguments& Args){OnFocused=Args._OnFocused;FocusBrush=FSlateRoundedBoxBrush(FLinearColor::Transparent,2.f,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink),2.f);SBorder::Construct(SBorder::FArguments().BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBorder"))).Padding(2)[Args._Content.Widget]);}
 bool SupportsKeyboardFocus()const override{return true;}
 FReply OnFocusReceived(const FGeometry&,const FFocusEvent&)override{OnFocused.ExecuteIfBound();return FReply::Handled();}
 int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& O,int32 L,const FWidgetStyle& S,bool E)const override{int32 Top=SBorder::OnPaint(A,G,R,O,L,S,E);if(HasKeyboardFocus()||HasUserFocus(0))FSlateDrawElement::MakeBox(O,++Top,G.ToPaintGeometry(),&FocusBrush);return Top;}
private: FSimpleDelegate OnFocused;FSlateBrush FocusBrush;
};
FLinearColor ResidenceColor(EHansaUiColorToken T){return UHansaUiStyleLibrary::GetColor(T);}
FText NeedPercent(int32 Value){FNumberFormattingOptions Format;Format.SetMaximumFractionalDigits(1);return FText::AsPercent(FMath::Clamp(Value,0,10000)/10000.,&Format);}
FString NeedTarget(FName Id){FString S=Id.ToString();S.RemoveFromStart(TEXT("Need."));return TEXT("Inspector.Residence.Need.")+S;}
EUiGlyph NeedGlyph(const FHansaInspectorNeedData& N){
 if(N.bService)return EUiGlyph::Civic;
 if(N.GoodId==TEXT("Good.Bread"))return EUiGlyph::Bread;
 if(N.GoodId==TEXT("Good.Fish"))return EUiGlyph::Fish;
 if(N.GoodId==TEXT("Good.Beer"))return EUiGlyph::Beer;
 if(N.GoodId==TEXT("Good.Tools"))return EUiGlyph::Production;
 return EUiGlyph::Information;
}
}
TSharedRef<STextBlock> SHansaResidenceInspector::Text(FText Value,EHansaUiTypographyToken Token){return SNew(STextBlock).Text(Value).Font(GetComponentFont(Token,Preferences)).ColorAndOpacity(ResidenceColor(EHansaUiColorToken::Ink)).AutoWrapText(true);}
TSharedRef<SHansaAction> SHansaResidenceInspector::Action(FName Id,FText Label){
 auto B=SNew(SHansaAction).Compact(true).Preferences(Preferences).Label(Label).OnClicked_Lambda([this,Id]{return Model.IsValid()&&Model->ActivateAction(Id)?FReply::Handled():FReply::Unhandled();});
 B->SetFocusHandler(FSimpleDelegate::CreateLambda([this,Id]{if(Model.IsValid())Model->SetFocusedSemanticId(Id);}));
 Buttons.Add(Id.ToString(),B);Targets.Add(Id.ToString(),B);return B;
}
void SHansaResidenceInspector::Construct(const FArguments& Args){
 Model=Args._Model;Preferences=Args._Preferences;
 Paper=GetComponentStyle(EUiSurface::Panel,EUiState::Default,Preferences).Brush;
 Navy=GetComponentStyle(EUiSurface::TopBar,EUiState::Default,Preferences).Brush;
 Arch=FSlateRoundedBoxBrush(ResidenceColor(EHansaUiColorToken::Linen),FVector4(48,48,3,3),ResidenceColor(EHansaUiColorToken::Brass),1.f);
 Tip=FSlateRoundedBoxBrush(ResidenceColor(EHansaUiColorToken::BalticNavy),3.f,ResidenceColor(EHansaUiColorToken::Brass),1.f);
 BarStyle.SetBackgroundImage(FSlateRoundedBoxBrush(ResidenceColor(EHansaUiColorToken::Parchment),2.f));
 BarStyle.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White,2.f));BarStyle.EnableFillAnimation=false;
 PortraitBrush=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/Residence/citizen--160.png"))),FVector2D(80,80));
 auto Portrait=SNew(SBox).WidthOverride(96).HeightOverride(88)[SNew(SBorder).BorderImage(&Arch).Padding(8,4)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(PortraitBrush.Get())]]];
 Targets.Add(TEXT("Inspector.Residence.Portrait"),Portrait);
 Identity=Text(FText(),EHansaUiTypographyToken::Heading2);Identity->SetColorAndOpacity(ResidenceColor(EHansaUiColorToken::Chalk));
 Occupancy=Text(FText(),EHansaUiTypographyToken::Heading2);State=Text(FText(),EHansaUiTypographyToken::Caption);Cause=Text(FText());Result=Text(FText(),EHansaUiTypographyToken::Caption);
 NeedsHeading=Text(LOCTEXT("Needs","Citizen needs"),EHansaUiTypographyToken::Heading2);
 DetailsButton=Action(TEXT("Inspector.Action.OpenCause"),LOCTEXT("Details","Details"));
 ChildSlot[SNew(SBorder).BorderImage(&Paper).Padding(0)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,4)[Portrait]
  +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&Navy).Padding(8,4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(4,0,8,0)[Identity.ToSharedRef()]
   +SHorizontalBox::Slot().AutoWidth()[Action(TEXT("Inspector.Close"),LOCTEXT("Close","Close"))]]]
  +SVerticalBox::Slot().FillHeight(1).Padding(16,8)[SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[Occupancy.ToSharedRef()]
   +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SBox).HeightOverride(6)[SAssignNew(OccupancyBar,SProgressBar).Style(&BarStyle).Percent(0.f).FillColorAndOpacity(ResidenceColor(EHansaUiColorToken::ProsperityTeal))]]
   +SVerticalBox::Slot().AutoHeight()[State.ToSharedRef()]
   +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,4)[NeedsHeading.ToSharedRef()]
   +SVerticalBox::Slot().AutoHeight()[SAssignNew(Needs,SVerticalBox)]
   +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Details,SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Cause.ToSharedRef()]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Actions,SVerticalBox)]]]]
  +SVerticalBox::Slot().AutoHeight().Padding(16,0,16,8)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[DetailsButton.ToSharedRef()]
   +SVerticalBox::Slot().AutoHeight()[Result.ToSharedRef()]]]];
 Targets.Add(TEXT("Inspector.Identity"),Identity);Targets.Add(TEXT("Inspector.Result"),Occupancy);
 Targets.Add(TEXT("Inspector.Residence.Occupancy"),Occupancy);Targets.Add(TEXT("Inspector.Residence.OccupancyBar"),OccupancyBar);
 Targets.Add(TEXT("Inspector.Residence.ConsumptionPeriod"),State);Targets.Add(TEXT("Inspector.Flows"),Needs);Targets.Add(TEXT("Inspector.Problem.Cause"),Cause);
}
void SHansaResidenceInspector::Refresh(const FHansaInspectorSnapshot& S){
 if(Presented.BuildingValue!=S.BuildingValue)Scroll->ScrollToStart();
 const auto& D=S.Residence;
 NeedsHeading->SetText(LOCTEXT("Needs","Citizen needs"));
 Identity->SetText(S.Identity);Occupancy->SetText(FText::Format(LOCTEXT("Residents","{0} / {1} residents"),FText::AsNumber(D.Residents),FText::AsNumber(D.Capacity)));
 Occupancy->SetToolTipText(LOCTEXT("CapacityTip","People currently living in this house / maximum resident capacity."));
 OccupancyBar->SetPercent(D.Capacity>0?FMath::Clamp(float(D.Residents)/D.Capacity,0.f,1.f):0.f);
 const FText ResidenceState=D.Residents>0?S.State:S.Causal.Severity==EHansaCausalSeverity::None
  ?LOCTEXT("EmptyHouseReady","Empty house · accepting residents")
  :FText::Format(LOCTEXT("EmptyHouseBlocked","Empty house · {0}"),S.Causal.Problem);
 State->SetText(FText::Format(LOCTEXT("ResidencePeriod","{0}\n{1}"),ResidenceState,D.ConsumptionPeriod));
 TArray<FName> Ids;for(const auto& N:D.Needs)Ids.Add(N.NeedId);
 if(Ids!=NeedIds){
  for(const auto& Id:NeedIds){Targets.Remove(NeedTarget(Id));Targets.Remove(TEXT("Inspector.Flows.Item.")+Id.ToString().Replace(TEXT("."),TEXT("_")));}
  NeedIds=Ids;Needs->ClearChildren();NeedBars.Reset();NeedPercents.Reset();NeedHints.Reset();NeedAmounts.Reset();
  for(const auto& N:D.Needs){
   auto Percent=Text(FText(),EHansaUiTypographyToken::Data);Percent->SetAutoWrapText(false);
   auto Amount=Text(FText(),EHansaUiTypographyToken::Caption);
   TSharedPtr<SProgressBar> Bar;
   auto Row=SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(NeedGlyph(N)).Size(32)]
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
      +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Text(N.Label)]
      +SHorizontalBox::Slot().AutoWidth()[Percent]]
     +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SBox).HeightOverride(6)[SAssignNew(Bar,SProgressBar).Style(&BarStyle).Percent(0.f).FillColorAndOpacity(ResidenceColor(EHansaUiColorToken::ProsperityTeal))]]
     +SVerticalBox::Slot().AutoHeight()[Amount]];
   auto Hint=Text(FText());Hint->SetColorAndOpacity(ResidenceColor(EHansaUiColorToken::Chalk));NeedHints.Add(N.NeedId,Hint);
   auto FocusRow=SNew(SResidenceNeedRow).OnFocused_Lambda([this,Id=NeedTarget(N.NeedId)]{if(Model.IsValid())Model->SetFocusedSemanticId(FName(*Id));})[Row];
   FocusRow->SetToolTip(SNew(SToolTip).BorderImage(&Tip).TextMargin(12)[SNew(SBox).MaxDesiredWidth(280)[Hint]]);
   Needs->AddSlot().AutoHeight().Padding(0,2)[FocusRow];Targets.Add(NeedTarget(N.NeedId),FocusRow);
   Targets.Add(TEXT("Inspector.Flows.Item.")+N.NeedId.ToString().Replace(TEXT("."),TEXT("_")),Row);
   NeedPercents.Add(N.NeedId,Percent);NeedBars.Add(N.NeedId,Bar);NeedAmounts.Add(N.NeedId,Amount);
  }
 }
 for(const auto& N:D.Needs){
  NeedPercents[N.NeedId]->SetText(N.Percent);
  NeedAmounts[N.NeedId]->SetText(N.Amount);
  NeedBars[N.NeedId]->SetPercent(N.bKnown?FMath::Clamp(N.Fulfillment/10000.f,0.f,1.f):0.f);
  const FText Explanation=N.bService
   ?FText::Format(LOCTEXT("CurrentServiceTip","{0}\nCurrent service level: {1}\nAccess: {2}\nAffordability: {3}\nReliability: {4}\nServices have no consumed/required product quantities."),N.Label,N.Percent,NeedPercent(N.Access),NeedPercent(N.Affordability),NeedPercent(N.Reliability))
   :FText::Format(LOCTEXT("ResidenceConsumptionTip","{0}\n{1}\nDemand fulfilled: {2}\nThis residence · {3}\nGoods count only when consumed. Access and affordability can limit consumption.\nCurrent access: {4} · affordability: {5}"),N.Label,N.Amount,N.Percent,D.ConsumptionPeriod,NeedPercent(N.Access),NeedPercent(N.Affordability));
  NeedHints[N.NeedId]->SetText(Explanation);
 }
 if(S.Actions!=Presented.Actions){
  Actions->ClearChildren();for(auto It=Buttons.CreateIterator();It;++It)if(It.Key()!=TEXT("Inspector.Close")&&It.Key()!=TEXT("Inspector.Action.OpenCause")){Targets.Remove(It.Key());It.RemoveCurrent();}
  for(const auto& A:S.Actions)if(A.StableId!=TEXT("Inspector.Action.OpenCause")&&A.StableId!=TEXT("Inspector.Close"))Actions->AddSlot().AutoHeight().Padding(0,4)[Action(A.StableId,A.Label)];
 }
 for(const auto& A:S.Actions)if(auto* B=Buttons.Find(A.StableId.ToString()))(*B)->SetState(A.bEnabled?EUiState::Default:EUiState::Disabled,A.bEnabled?A.ToolTip:A.DisabledReason);
 DetailsButton->SetLabel(S.bCauseExpanded?LOCTEXT("Less","Less"):LOCTEXT("Details","Details"));
 Details->SetVisibility(S.bCauseExpanded?EVisibility::Visible:EVisibility::Collapsed);
 Cause->SetText(FText::Format(LOCTEXT("Explanation","{0}\n{1}\n{2}\nWorkforce supplied: {3}"),S.Causal.Problem,S.Causal.Cause,S.Causal.Remedy,FText::AsNumber(D.Workforce)));
 Result->SetText(S.LastActionResult);Result->SetVisibility(S.LastActionResult.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
 FocusOrder={TEXT("Inspector.Close")};for(const auto& N:D.Needs)FocusOrder.Add(NeedTarget(N.NeedId));FocusOrder.Add(TEXT("Inspector.Action.OpenCause"));
 if(S.bCauseExpanded)for(const auto& A:S.Actions)if(A.bEnabled&&A.StableId!=TEXT("Inspector.Action.OpenCause"))FocusOrder.AddUnique(A.StableId.ToString());
 Presented=S;
}
TSharedPtr<SWidget> SHansaResidenceInspector::Resolve(const FString& Id)const{
 if(Id==TEXT("Inspector.Root"))return ConstCastSharedRef<SHansaResidenceInspector>(SharedThis(this));
 const auto* W=Targets.Find(Id);return W?*W:nullptr;
}
void SHansaResidenceInspector::Reveal(const FString& Id){if(auto W=Resolve(Id))Scroll->ScrollDescendantIntoView(W,false,EDescendantScrollDestination::IntoView);}
bool SHansaResidenceInspector::Focus(const FString& Id){auto W=Resolve(Id);if(!Presented.bOpen||!FocusOrder.Contains(Id)||!W.IsValid()||!W->IsEnabled())return false;if(Id!=TEXT("Inspector.Close")&&Id!=TEXT("Inspector.Action.OpenCause"))Reveal(Id);if(Model.IsValid())Model->SetFocusedSemanticId(FName(*Id));FSlateApplication::Get().SetKeyboardFocus(W,EFocusCause::Navigation);return true;}
TArray<FHansaHudSemanticNode> SHansaResidenceInspector::GetSemanticSnapshot()const{
 TArray<FHansaHudSemanticNode> Out;
 auto Add=[&](FString Id,FString Label,FString Value,EHansaHudSemanticRole Role=EHansaHudSemanticRole::Status){
  FHansaHudSemanticNode N;N.Id=Id;N.ParentId=Id==TEXT("Inspector.Root")?TEXT("HUD.InspectorHost"):TEXT("Inspector.Root");N.Label=Label;N.Role=Role;N.State.Value=Value;N.State.ValueType=TEXT("residence");N.State.bVisible=Presented.bOpen;N.State.bEnabled=true;N.bCanFocus=FocusOrder.Contains(Id);N.bCanActivate=Buttons.Contains(Id);N.State.bFocused=Presented.FocusedSemanticId==FName(*Id);
  if(auto W=Resolve(Id)){auto P=W->GetCachedGeometry().GetAbsolutePosition()-GetCachedGeometry().GetAbsolutePosition();auto Z=W->GetCachedGeometry().GetAbsoluteSize();N.Bounds=FIntRect(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.X+Z.X),FMath::RoundToInt(P.Y+Z.Y));N.State.bEnabled=W->IsEnabled();}
  if((Buttons.Contains(Id)&&Id!=TEXT("Inspector.Close")&&Id!=TEXT("Inspector.Action.OpenCause"))||Id==TEXT("Inspector.Problem.Cause"))N.State.bVisible&=Presented.bCauseExpanded;
  N.bCanActivate&=N.State.bVisible&&N.State.bEnabled;Out.Add(MoveTemp(N));
 };
 Add(TEXT("Inspector.Root"),TEXT("Residence inspector"),Presented.ObjectStableId.ToString(),EHansaHudSemanticRole::Panel);
 Add(TEXT("Inspector.Identity"),Presented.Identity.ToString(),Presented.State.ToString());
 Add(TEXT("Inspector.Result"),TEXT("Residents and capacity"),Occupancy->GetText().ToString());
 Add(TEXT("Inspector.Residence.Occupancy"),TEXT("Residents / maximum residents"),FString::Printf(TEXT("residents=%d;capacity=%d"),Presented.Residence.Residents,Presented.Residence.Capacity));
 Add(TEXT("Inspector.Residence.Portrait"),TEXT("Citizen portrait"),TEXT("Hansa citizen engraving"));
 Add(TEXT("Inspector.Flows"),TEXT("Citizen needs"),FString::FromInt(Presented.Residence.Needs.Num()),EHansaHudSemanticRole::Panel);
 Add(TEXT("Inspector.Residence.ConsumptionPeriod"),TEXT("Residence status and recorded consumption period"),FString::Printf(TEXT("status=%s;cause=%s;coveredMinutes=%lld;fullWindow=%s;period=%s"),*State->GetText().ToString().Replace(TEXT("\n"),TEXT(" | ")),*Presented.Causal.StableCode.ToString(),Presented.Residence.CoveredMinutes,Presented.Residence.bFullWindow?TEXT("true"):TEXT("false"),*Presented.Residence.ConsumptionPeriod.ToString()));
 for(const auto& N:Presented.Residence.Needs){const FString V=FString::Printf(TEXT("known=%s;fulfilledBasisPoints=%d;access=%d;affordability=%d;reliability=%d;good=%s;percent=%s;requiredMilliUnits=%lld;consumedMilliUnits=%lld;scope=%s"),N.bKnown?TEXT("true"):TEXT("false"),N.Fulfillment,N.Access,N.Affordability,N.Reliability,*N.GoodId.ToString(),*NeedPercents[N.NeedId]->GetText().ToString(),N.Required,N.Consumed,N.bService?TEXT("current-service"):TEXT("residence-30-days"));Add(NeedTarget(N.NeedId),N.Label.ToString(),V);Add(TEXT("Inspector.Flows.Item.")+N.NeedId.ToString().Replace(TEXT("."),TEXT("_")),N.Label.ToString(),V);}
 Add(TEXT("Inspector.Problem.Cause"),TEXT("Cause and remedy"),Cause->GetText().ToString());
 for(const auto& B:Buttons){const auto* A=Presented.Actions.FindByPredicate([&](const auto& V){return V.StableId==FName(*B.Key);});Add(B.Key,A?A->Label.ToString():B.Key,A?A->DisabledReason.ToString():FString(),EHansaHudSemanticRole::Button);}
 return Out;
}
}
#undef LOCTEXT_NAMESPACE
