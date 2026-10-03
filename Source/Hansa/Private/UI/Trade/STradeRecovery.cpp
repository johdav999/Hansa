#include "STradeRecovery.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#define LOCTEXT_NAMESPACE "TradeRecovery"
namespace Hansa::UI {
class SRecoveryDetail final : public SBorder {
public:
 virtual bool SupportsKeyboardFocus() const override{return true;}
 virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& R,FSlateWindowElementList& O,int32 L,const FWidgetStyle& S,bool E) const override{
  const int32 End=SBorder::OnPaint(A,G,R,O,L,S,E);if(HasKeyboardFocus()){const auto Z=G.GetLocalSize();TArray<FVector2D> P={{1,1},{Z.X-1,1},{Z.X-1,Z.Y-1},{1,Z.Y-1},{1,1}};FSlateDrawElement::MakeLines(O,End+1,G.ToPaintGeometry(),P,ESlateDrawEffect::None,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass),true,2);}return End+1;
 }
};
void STradeRecovery::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& C){
 Initialize(C);
 Inspect=MakeControl(TEXT("TradeMap.Recovery.Inspect"),LOCTEXT("Inspect","Open selected recovery item"),EHansaUiButtonStyle::Secondary);
 Review=MakeControl(TEXT("TradeMap.Recovery.Review"),LOCTEXT("Review","Review closure"),EHansaUiButtonStyle::Secondary);
 Confirm=MakeControl(TEXT("TradeMap.Recovery.Confirm"),LOCTEXT("Confirm","Confirm closure"),EHansaUiButtonStyle::Destructive);
 Cancel=MakeControl(TEXT("TradeMap.Recovery.Cancel"),LOCTEXT("Cancel","Keep current state"),EHansaUiButtonStyle::Secondary);
 Back=MakeControl(TEXT("TradeMap.Recovery.Back"),LOCTEXT("Back","Return to city / alert"),EHansaUiButtonStyle::Secondary);
 auto D=SNew(SRecoveryDetail).BorderImage(FCoreStyle::Get().GetBrush("NoBorder")).Padding(4)[SAssignNew(Detail,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)];MapWidget(TEXT("TradeMap.Recovery.Detail"),D);
 ChildSlot[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("Heading","Recovery")).TextStyle(&LightHeadingStyle).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible)]
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Scroll,SScrollBox)
  +SScrollBox::Slot().Padding(0,8)[SAssignNew(Summary,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SScrollBox::Slot()[SAssignNew(Items,SVerticalBox)]
  +SScrollBox::Slot().Padding(0,8)[D]
  +SScrollBox::Slot()[Inspect.ToSharedRef()]
  +SScrollBox::Slot()[SAssignNew(Feedback,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SScrollBox::Slot()[Back.ToSharedRef()]]
 +SVerticalBox::Slot().AutoHeight()[Review.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1)[Confirm.ToSharedRef()]+SHorizontalBox::Slot().FillWidth(1)[Cancel.ToSharedRef()]]
 ];
 MapWidget(TEXT("TradeMap.Recovery.Status"),Summary);MapWidget(TEXT("TradeMap.Recovery.Feedback"),Feedback);
}
void STradeRecovery::Refresh(const FHansaTradeMapSnapshot& S){
 if(View.RecoveryItem!=S.RecoveryItem||View.bRecoveryReview!=S.bRecoveryReview)Scroll->ScrollToStart();View=S;
 Summary->SetText(FText::FromString(S.Recovery.Station?FString::Printf(TEXT("Station #%lld · %s\n%s"),S.Recovery.Station,*S.Recovery.Status,*S.Recovery.Cause):TEXT("No owned station in this city. Establish one through Presence.")));
 if(S.bRecoveryReview)Summary->SetText(FText::FromString(FString::Printf(TEXT("Review %s · Station #%lld"),S.Recovery.bFinalizing?TEXT("finalization"):TEXT("closure"),S.Recovery.Station)));
 if(Rows.Num()!=S.Recovery.Items.Num()||S.Recovery.Items.ContainsByPredicate([&](const auto& I){return !Rows.Contains(I.Id);})){Rows.Reset();Items->ClearChildren();for(const auto& I:S.Recovery.Items){auto W=MakeControl(*(TEXT("TradeMap.Recovery.Item.")+I.Id),FText::FromString(I.Label),EHansaUiButtonStyle::Secondary);Rows.Add(I.Id,W);Items->AddSlot().AutoHeight().Padding(0,2)[W];}}
 for(const auto& I:S.Recovery.Items){Rows[I.Id]->SetLabel(FText::FromString(I.Label));Rows[I.Id]->SetState(S.RecoveryItem==I.Id?EUiState::Selected:EUiState::Default);Rows[I.Id]->SetEnabled(!S.bRecoveryPending);}
 const auto* I=S.Recovery.Items.FindByPredicate([&](const auto& X){return X.Id==S.RecoveryItem;});
 Detail->SetText(FText::FromString(S.bRecoveryReview?S.Recovery.Terms:I?I->Detail:TEXT("Select a dependency to inspect its cause and safe action.")));
 Items->SetVisibility(S.bRecoveryReview?EVisibility::Collapsed:EVisibility::Visible);Inspect->SetVisibility(S.bRecoveryReview?EVisibility::Collapsed:EVisibility::Visible);
 if(I&&I->Target==TEXT("Ship")&&Model.IsValid()&&!Model->CanRecoveryIntent(TEXT("Inspect")))Detail->SetText(FText::FromString(I->Detail+TEXT("\nThis remote view shows the complete owned hold above. Separate fleet navigation is unavailable in this projection.")));
 Feedback->SetText(FText::FromString(S.bRecoveryPending?TEXT("Awaiting authoritative closure result. Do not resubmit."):S.RecoveryFeedback));
 Inspect->SetLabel(FText::FromString(I?(I->Target==TEXT("Route")?TEXT("Edit recovery route"):I->Target==TEXT("Orders")?TEXT("Manage selected order"):I->Target==TEXT("Presence")?TEXT("Review funding / rights"):I->Target==TEXT("Ship")?TEXT("Inspect ship manifest"):I->Target==TEXT("Construction")?TEXT("Inspect lease / buildings"):TEXT("Inspect station ledger")):TEXT("Select a dependency")));
 Review->SetLabel(S.Recovery.bFinalizing?LOCTEXT("Finalize","Review finalization"):LOCTEXT("Review","Review closure"));
 Review->SetToolTipText(FText::FromString(S.Recovery.Cause));Review->SetVisibility(S.bRecoveryReview?EVisibility::Collapsed:EVisibility::Visible);
 Confirm->SetVisibility(S.bRecoveryReview?EVisibility::Visible:EVisibility::Collapsed);Cancel->SetVisibility(S.bRecoveryReview?EVisibility::Visible:EVisibility::Collapsed);
 if(auto* M=Model.Get()){Inspect->SetEnabled(M->CanRecoveryIntent(TEXT("Inspect")));Review->SetEnabled(M->CanRecoveryIntent(TEXT("Review")));Confirm->SetEnabled(M->CanRecoveryIntent(TEXT("Confirm")));Cancel->SetEnabled(M->CanRecoveryIntent(TEXT("Cancel")));Back->SetEnabled(!S.bRecoveryPending);}
}
TArray<FString> STradeRecovery::FocusOrder() const{TArray<FString> R;if(!View.bRecoveryReview)for(const auto& I:View.Recovery.Items)R.Add(TEXT("TradeMap.Recovery.Item.")+I.Id);R.Add(TEXT("TradeMap.Recovery.Detail"));R.Add(TEXT("TradeMap.Recovery.Inspect"));R.Add(TEXT("TradeMap.Recovery.Review"));R.Add(TEXT("TradeMap.Recovery.Confirm"));R.Add(TEXT("TradeMap.Recovery.Cancel"));R.Add(TEXT("TradeMap.Recovery.Back"));return R;}
}
#undef LOCTEXT_NAMESPACE
