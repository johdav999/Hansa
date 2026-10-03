#include "STradeDecisions.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Text/STextBlock.h"
#define LOCTEXT_NAMESPACE "TradeDecisions"
namespace Hansa::UI {
class SDecisionTerms final : public SBorder {
public:
 virtual bool SupportsKeyboardFocus() const override {return true;}
 virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& O,int32 L,const FWidgetStyle& S,bool Enabled) const override {
  const int32 End=SBorder::OnPaint(A,G,R,O,L,S,Enabled);
  if(HasKeyboardFocus()){const auto Z=G.GetLocalSize();TArray<FVector2D> Points={{1,1},{Z.X-1,1},{Z.X-1,Z.Y-1},{1,Z.Y-1},{1,1}};FSlateDrawElement::MakeLines(O,End+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass),true,2);}return End+1;
 }
};
void STradeDecisions::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& C)
{
 Initialize(C);
 Source=MakeControl(TEXT("TradeMap.Decisions.Source"),LOCTEXT("Source","Choose funding source"),EHansaUiButtonStyle::Secondary);
 Review=MakeControl(TEXT("TradeMap.Decisions.Review"),LOCTEXT("Review","Review decision"),EHansaUiButtonStyle::Primary);
 Confirm=MakeControl(TEXT("TradeMap.Decisions.Confirm"),LOCTEXT("Confirm","Confirm decision"),EHansaUiButtonStyle::Primary);
 Cancel=MakeControl(TEXT("TradeMap.Decisions.Cancel"),LOCTEXT("Cancel","Edit review"),EHansaUiButtonStyle::Secondary);
 ChildSlot[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Heading","City decisions")).TextStyle(&LightHeadingStyle).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible)]
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Scroll,SScrollBox)
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(ContextText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SScrollBox::Slot()[SAssignNew(Choices,SVerticalBox)]
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(Status,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
  +SScrollBox::Slot().Padding(0,8)[Source.ToSharedRef()]
  +SScrollBox::Slot()[SAssignNew(SourceDetail,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SScrollBox::Slot()[SAssignNew(Dossier,SDecisionTerms).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Padding(4)[SAssignNew(Terms,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(Feedback,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight()[Review.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1)[Confirm.ToSharedRef()]
  +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Cancel.ToSharedRef()]]];
 MapWidget(TEXT("TradeMap.Decisions.Terms"),Dossier);MapWidget(TEXT("TradeMap.Decisions.Status"),Status);
}
void STradeDecisions::Refresh(const FHansaTradeMapSnapshot& S)
{
 const bool SelectionChanged=View.DecisionId!=S.DecisionId||View.bDecisionReview!=S.bDecisionReview;View=S;Choices->SetVisibility(S.bDecisionReview?EVisibility::Collapsed:EVisibility::Visible);ContextText->SetVisibility(S.bDecisionReview?EVisibility::Collapsed:EVisibility::Visible);if(SelectionChanged)Scroll->ScrollToStart();ContextText->SetText(S.Decisions.Context);
 if(Actions.Num()!=S.Decisions.Options.Num()||S.Decisions.Options.ContainsByPredicate([&](const auto& O){return !Actions.Contains(O.Id);})){Choices->ClearChildren();Actions.Reset();for(const auto& O:S.Decisions.Options){auto A=MakeControl(*(TEXT("TradeMap.Decisions.")+O.Id),O.Title,EHansaUiButtonStyle::Secondary);Actions.Add(O.Id,A);Choices->AddSlot().AutoHeight().Padding(0,4)[A];}}
 for(const auto& O:S.Decisions.Options){Actions[O.Id]->SetState(S.DecisionId==O.Id?EUiState::Selected:EUiState::Default);Actions[O.Id]->SetEnabled(!S.bDecisionReview&&!S.bDecisionPending);}
 const auto* O=S.Decisions.Options.FindByPredicate([&](const auto& X){return X.Id==S.DecisionId;});
 const auto* Funding=O?O->Sources.FindByPredicate([&](const auto& X){return X.Id==S.DecisionSource;}):nullptr;
 Status->SetText(O?FText::Format(LOCTEXT("IdentityStatus","{0}\n{1}"),O->Title,O->Status):LOCTEXT("Select","Choose a privilege, project or charter."));Terms->SetText(O?O->Terms:FText());
 Source->SetVisibility(!S.bDecisionReview&&O&&(O->Kind==0||O->Kind==2)&&!O->Sources.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed);
 Source->SetLabel(Funding?Funding->Label:LOCTEXT("Choose","Choose funding source"));
 SourceDetail->SetText(Funding?Funding->Detail:O&&O->bAvailable&&(O->Kind==0||O->Kind==2)?LOCTEXT("NoSource","Choose an owned station inventory. Ship and city stock are not funding sources for this decision."):FText());
 Feedback->SetText(FText::FromString(S.bDecisionReview&&O?O->Commitment.ToString()+(Funding?TEXT(" · ")+Funding->Label.ToString():FString()):S.bDecisionPending?TEXT("Awaiting authority; do not resubmit."):S.bDecisionReview?TEXT("Review exact terms above. Confirmation revalidates rights and costs."):S.DecisionFeedback));
 Review->SetVisibility(S.bDecisionReview?EVisibility::Collapsed:EVisibility::Visible);Confirm->SetVisibility(S.bDecisionReview?EVisibility::Visible:EVisibility::Collapsed);Cancel->SetVisibility(S.bDecisionReview?EVisibility::Visible:EVisibility::Collapsed);
 if(auto* M=Model.Get()){Source->SetEnabled(M->CanDecisionIntent(TEXT("Source")));Review->SetEnabled(M->CanDecisionIntent(TEXT("Review")));Confirm->SetEnabled(M->CanDecisionIntent(TEXT("Confirm")));Cancel->SetEnabled(M->CanDecisionIntent(TEXT("Cancel")));}
 Review->SetToolTipText(O?O->Status:LOCTEXT("Missing","Choose a decision first."));
 Confirm->SetLabel(O&&O->Kind==1?LOCTEXT("Revoke","Confirm revocation"):O&&O->Kind==2?LOCTEXT("Fund","Confirm full funding"):O&&O->Kind==3?LOCTEXT("Charter","Confirm charter"):LOCTEXT("Acquire","Confirm privilege"));
}
TArray<FString> STradeDecisions::FocusOrder() const
{
 TArray<FString> R;for(const auto& O:View.Decisions.Options)R.Add(TEXT("TradeMap.Decisions.")+O.Id);
 if(!View.DecisionId.IsEmpty())R.Add(TEXT("TradeMap.Decisions.Terms"));R.Add(TEXT("TradeMap.Decisions.Source"));R.Add(View.bDecisionReview?TEXT("TradeMap.Decisions.Confirm"):TEXT("TradeMap.Decisions.Review"));if(View.bDecisionReview)R.Add(TEXT("TradeMap.Decisions.Cancel"));return R;
}
}
#undef LOCTEXT_NAMESPACE
