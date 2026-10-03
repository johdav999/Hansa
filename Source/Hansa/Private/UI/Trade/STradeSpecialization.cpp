#include "HansaTradeWorkspaceComponents.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaUiStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#define LOCTEXT_NAMESPACE "STradeSpecialization"
namespace Hansa::UI {
namespace { FString Id(const FString& S){return TEXT("TradeMap.Presence.Specialization.")+S;} }
void STradeSpecialization::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& C){
 Initialize(C);
 const auto Labels=SpecializationDimensionLabels();
 for(const FString Branch:{TEXT("Warehouse"),TEXT("Market"),TEXT("Harbor")}){
  auto Choice=MakeControl(*Id(Branch),FText::FromString(Branch),EHansaUiButtonStyle::Secondary);Choices.Add(Branch,Choice);
  auto Card=SNew(SVerticalBox);
  Card->AddSlot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(Branch==TEXT("Warehouse")?EUiGlyph::Warehouse:Branch==TEXT("Market")?EUiGlyph::Market:EUiGlyph::Harbor).Size(48)]
   +SHorizontalBox::Slot().FillWidth(1)[Choice]];
  TSharedPtr<STextBlock> State,Recommendation;
  Card->AddSlot().AutoHeight().Padding(4)[SAssignNew(State,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)];
  Card->AddSlot().AutoHeight().Padding(4)[SAssignNew(Recommendation,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)];
  States.Add(Branch,State);Recommendations.Add(Branch,Recommendation);
  for(int32 D=0;D<Labels.Num();++D){
   const FString Key=FString::Printf(TEXT("Dimension.%s.%d"),*Branch,D);
   auto Row=MakeControl(*Id(Key),FText(),EHansaUiButtonStyle::Secondary);
   TSharedPtr<STextBlock> Value;
   Row->SetContent(SNew(SBox).MinDesiredHeight(48)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Labels[D]).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(Value,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]]);
   Row->SetButtonStyle(&LightRowStyle);Values.Add(Key,Value);
   Card->AddSlot().AutoHeight().Padding(0,2)[Row];
  }
  Cards.Add(Branch,SNew(SHansaReferenceFrame).Dark(false).Padding(16)[Card]);
 }
 Back=MakeControl(*Id(TEXT("Back")),LOCTEXT("Back","Back to Presence"),EHansaUiButtonStyle::Secondary);
 Apply=MakeControl(*Id(TEXT("Apply")),LOCTEXT("Review","Review selection"),EHansaUiButtonStyle::Primary);
 Confirm=MakeControl(*Id(TEXT("Confirm")),LOCTEXT("Confirm","Confirm investment"),EHansaUiButtonStyle::Primary);
 Cancel=MakeControl(*Id(TEXT("Cancel")),LOCTEXT("Edit","Edit review"),EHansaUiButtonStyle::Secondary);
 SAssignNew(SourcePicker,SComboButton).ButtonStyle(&SecondaryButtonStyle).ContentPadding(FMargin(12,8))
 .OnGetMenuContent_Lambda([this]{
  auto Menu=SNew(SVerticalBox);const auto* M=Model.Get();if(!M)return Menu;
  const auto& S=M->GetSnapshot();const auto* O=S.Specialization.Options.FindByPredicate([&](const auto& X){return X.Id==S.SelectedPresenceSpecializationId;});
  if(O)for(const auto& Source:O->Sources)Menu->AddSlot().AutoHeight()[SNew(SBox).MinDesiredHeight(48)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).OnClicked_Lambda([this,Key=Source.Id]{SourcePicker->SetIsOpen(false);return Invoke(Id(TEXT("Source.")+Key));})[SNew(STextBlock).Text(Source.Label).ToolTipText(Source.Detail).TextStyle(&LightBodyStyle)]]];
  return Menu;
 }).ButtonContent()[SNew(SBox).MinDesiredHeight(32)[SAssignNew(SourceLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]];
 MapWidget(Id(TEXT("Source")),SourcePicker);
 SAssignNew(ReviewPanel,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("ReviewHeading","Review investment")).TextStyle(&LightHeadingStyle)]
 +SVerticalBox::Slot().AutoHeight().Padding(8)[SAssignNew(Review,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)];
 auto Actions=SNew(SWrapBox).UseAllottedSize(true);
 Actions->AddSlot().Padding(0,0,8,0)[SourcePicker.ToSharedRef()];Actions->AddSlot().Padding(0,0,8,0)[SNew(SBox).MinDesiredWidth(120)[Apply.ToSharedRef()]];Actions->AddSlot().Padding(0,0,8,0)[Confirm.ToSharedRef()];Actions->AddSlot()[Cancel.ToSharedRef()];
 ChildSlot[SNew(SHansaReferenceFrame).Dark(false).Padding(16)[SAssignNew(SpecializationPanel,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[Back.ToSharedRef()]+SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Heading,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(Summary,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[SAssignNew(CardsHost,SBox)]+SScrollBox::Slot()[ReviewPanel.ToSharedRef()]+SScrollBox::Slot().Padding(0,4)[SAssignNew(Feedback,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight()[Actions]]];
}
void STradeSpecialization::Refresh(const FHansaTradeMapSnapshot& S){
 if(!bLayoutReady||bCompactCards!=S.bCompact){
  CardsHost->SetContent(SNullWidget::NullWidget);bCompactCards=S.bCompact;bLayoutReady=true;
  if(bCompactCards){auto Stack=SNew(SVerticalBox);for(const FString B:{TEXT("Warehouse"),TEXT("Market"),TEXT("Harbor")})Stack->AddSlot().AutoHeight().Padding(0,0,0,8)[Cards[B].ToSharedRef()];CardsHost->SetContent(Stack);}
  else {auto Row=SNew(SHorizontalBox);for(const FString B:{TEXT("Warehouse"),TEXT("Market"),TEXT("Harbor")})Row->AddSlot().FillWidth(1).Padding(4,0)[Cards[B].ToSharedRef()];CardsHost->SetContent(Row);}
 }
 Back->SetLabel(S.bCompact?LOCTEXT("CompactBack","Back"):LOCTEXT("Back","Back to Presence"));
 Heading->SetText(FText::Format(S.bCompact?LOCTEXT("CompactHeading","{0} · Office"):LOCTEXT("Heading","{0} · Merchant Office specialization"),FText::FromString(S.SelectedCityStableId.ToString().Replace(TEXT("City."),TEXT("")))));
 Summary->SetVisibility(S.bCompact?EVisibility::Collapsed:EVisibility::Visible);Summary->SetText(S.PresenceSpecializationComparison);Feedback->SetText(S.PresenceSpecializationFeedback);Feedback->SetVisibility(S.PresenceSpecializationFeedback.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
 for(const auto& Entry:Choices){
  const auto* O=S.Specialization.Options.FindByPredicate([&](const auto& X){return X.Id==Entry.Key;});
  Cards[Entry.Key]->SetVisibility(O?EVisibility::Visible:EVisibility::Collapsed);if(!O)continue;
  Entry.Value->SetLabel(O->Name);Entry.Value->SetEnabled(!S.bSpecializationPending&&!S.bSpecializationReview);
  Entry.Value->SetState(S.SelectedPresenceSpecializationId==O->Id?EUiState::Selected:EUiState::Default);
  States[Entry.Key]->SetText(O->bCurrent?LOCTEXT("Current","Current branch"):!O->bAvailable?O->Blocker:S.SelectedPresenceSpecializationId==O->Id?LOCTEXT("Selected","Selected for review"):LOCTEXT("Option","Available option"));
  Recommendations[Entry.Key]->SetText(O->Recommendation);Recommendations[Entry.Key]->SetVisibility(O->Recommendation.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
  for(int32 D=0;D<O->Dimensions.Num();++D)if(auto* Text=Values.Find(FString::Printf(TEXT("Dimension.%s.%d"),*O->Id,D)))(*Text)->SetText(O->Dimensions[D]);
 }
 const auto* O=S.Specialization.Options.FindByPredicate([&](const auto& X){return X.Id==S.SelectedPresenceSpecializationId;});
 const auto* Source=O?O->Sources.FindByPredicate([&](const auto& X){return X.Id==S.SpecializationSourceId;}):nullptr;
 SourceLabel->SetText(S.bCompact?(Source?FText::Format(LOCTEXT("CompactSourceSelected","Source: {0}"),FText::FromString(Source->Id)):LOCTEXT("CompactSource","Choose source")):(Source?Source->Label:LOCTEXT("Source","Choose station funding source")));SourcePicker->SetToolTipText(Source?Source->Detail:S.SpecializationReview);
 SourcePicker->SetEnabled(O&&!O->Sources.IsEmpty()&&!S.bSpecializationPending);
 SourcePicker->SetVisibility(S.bSpecializationReview?EVisibility::Collapsed:EVisibility::Visible);
 Apply->SetLabel(S.bCompact?LOCTEXT("CompactReview","Review"):S.PresenceSpecializationAction);Apply->SetEnabled(S.bCanPresenceSpecializationAction);Apply->SetVisibility(S.bSpecializationReview?EVisibility::Collapsed:EVisibility::Visible);
 Confirm->SetLabel(O&&O->bRespec?LOCTEXT("RespecConfirm","Confirm respec"):LOCTEXT("ApplyConfirm","Confirm investment"));Confirm->SetEnabled(S.bCanPresenceSpecializationAction&&!S.bSpecializationPending);
 Confirm->SetVisibility(S.bSpecializationReview?EVisibility::Visible:EVisibility::Collapsed);Cancel->SetVisibility(S.bSpecializationReview?EVisibility::Visible:EVisibility::Collapsed);
 CardsHost->SetVisibility(S.bSpecializationReview?EVisibility::Collapsed:EVisibility::Visible);ReviewPanel->SetVisibility(S.bSpecializationReview?EVisibility::Visible:EVisibility::Collapsed);Review->SetText(S.SpecializationReview);
}
TArray<FString> STradeSpecialization::FocusOrder() const{
 TArray<FString> R={Id(TEXT("Back"))};const auto* M=Model.Get();if(!M)return R;const auto& S=M->GetSnapshot();
 if(S.bSpecializationReview){R.Add(Id(TEXT("Confirm")));R.Add(Id(TEXT("Cancel")));return R;}
 for(const auto& B:{TEXT("Warehouse"),TEXT("Market"),TEXT("Harbor")}){R.Add(Id(B));for(int32 D=0;D<8;++D)R.Add(Id(FString::Printf(TEXT("Dimension.%s.%d"),B,D)));}
 R.Add(Id(TEXT("Source")));R.Add(Id(TEXT("Apply")));return R;
}
}
#undef LOCTEXT_NAMESPACE
