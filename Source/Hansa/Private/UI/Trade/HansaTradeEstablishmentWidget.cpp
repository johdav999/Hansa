#include "HansaTradeWorkspaceComponents.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "HansaTradeEstablishmentWidget"
using namespace Hansa::UI;
namespace {
FLinearColor Color(EHansaUiColorToken Token){return UHansaUiStyleLibrary::GetColor(Token);}
const FSlateBrush* PresenceArt(const TCHAR* Name,float Size,float Scale) {
 static TMap<FString,TSharedPtr<FSlateDynamicImageBrush>> Brushes;
 if(FString(Name)==TEXT("MerchantOffice")){
  auto& Office=Brushes.FindOrAdd(TEXT("MerchantOffice--112"));
  if(!Office)Office=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/TradeWorkspace/Icons/MerchantOffice--112.png"))),FVector2D(112));
  return Office.Get();
 }
 int32 Density=384;for(int32 Candidate:{64,96,128,192,256,384})if(Candidate>=Size*Scale){Density=Candidate;break;}
 const FString Key=FString::Printf(TEXT("%s--%d"),Name,Density);auto& Brush=Brushes.FindOrAdd(Key);
 if(!Brush)Brush=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/Presence")/(Key+TEXT(".png")))),FVector2D(Density));
 return Brush.Get();
}
}
void STradeEstablishment::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
 Initialize(In);
 const auto Brass=Color(EHansaUiColorToken::Brass),Linen=Color(EHansaUiColorToken::Linen),Parchment=Color(EHansaUiColorToken::Parchment),Muted=Color(EHansaUiColorToken::MutedInk);
 CardBrush=FSlateRoundedBoxBrush(FLinearColor(Linen.R,Linen.G,Linen.B,Preferences.bHighContrast?1.f:.65f),3.f,FLinearColor(Brass.R,Brass.G,Brass.B,.5f),1.f);
 NoticeBrush=FSlateRoundedBoxBrush(Parchment,3.f,Brass,1.f);RuleBrush=FSlateColorBrush(FLinearColor(Brass.R,Brass.G,Brass.B,.4f));
 CurrentStepBrush=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Oak),16.f,Brass,1.f);FutureStepBrush=FSlateRoundedBoxBrush(Muted,16.f,Linen,1.f);
 CaptionStyle=LightBodyStyle;auto Font=GetComponentFont(EHansaUiTypographyToken::SerifBody,Preferences);Font.Size=GetComponentFont(EHansaUiTypographyToken::Caption,Preferences).Size;CaptionStyle.SetFont(Font).SetColorAndOpacity(Muted);
 DataStyle=LightBodyStyle;DataStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
 RequirementProgressStyle.SetBackgroundImage(FSlateRoundedBoxBrush(Parchment,3.f,Muted,.5f)).SetFillImage(FSlateRoundedBoxBrush(Brass,3.f));
 auto PrimaryNavy=Color(EHansaUiColorToken::BalticNavy)*.72f;PrimaryNavy.A=1.f;
 PrimaryButtonStyle.SetNormal(FSlateRoundedBoxBrush(PrimaryNavy,2.f,Brass,1.f));
 auto Label=[&](const FText& Text)->TSharedRef<SWidget>{return SNew(STextBlock).Text(Text).TextStyle(&CaptionStyle).AutoWrapText(true);};
 auto Art=[&](const TCHAR* Name,float Size)->TSharedRef<SWidget>{return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(PresenceArt(Name,Size,Preferences.UiScale))]];};
 auto Metric=[&](EUiGlyph Glyph,TSharedPtr<STextBlock>& Value)->TSharedRef<SWidget>{return SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)[SNew(SHansaGlyph).Glyph(Glyph).Size(16)]
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(Value,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)];};
 auto Steps=SNew(SHorizontalBox);const FText StepNames[]={LOCTEXT("SiteStep","Site"),LOCTEXT("FundStep","Fund"),LOCTEXT("BuildStep","Build"),LOCTEXT("ReadyStep","Ready")};
 for(int32 I=0;I<4;++I){TSharedPtr<SBorder> Badge;TSharedPtr<STextBlock> StepLabel;Steps->AddSlot().FillWidth(1).VAlign(VAlign_Center)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(24).HeightOverride(24)[SAssignNew(Badge,SBorder).BorderImage(&FutureStepBrush).Padding(0).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::AsNumber(I+1)).TextStyle(&DarkBodyStyle)]]]
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(4,0)[SAssignNew(StepLabel,STextBlock).Text(StepNames[I]).TextStyle(&CaptionStyle).AutoWrapText(true)]];StepLabels.Add(StepLabel);StepBadges.Add(Badge);}

 auto SiteInfo=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SAssignNew(SiteLabel,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SAssignNew(SiteStatus,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[SAssignNew(SitePicker,SComboButton).ButtonStyle(&SecondaryButtonStyle).HasDownArrow(false).OnGetMenuContent(this,&STradeEstablishment::ChoiceMenu,true)
   .ButtonContent()[SNew(SBox).MinDesiredHeight(28).VAlign(VAlign_Center)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(LOCTEXT("ChangeSite","Change site")).TextStyle(&LightBodyStyle)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,0,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Down).Size(12)]]]];
 SiteCard=SNew(SBorder).BorderImage(&CardBrush).Padding(8)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[SAssignNew(SiteArt,SBox).WidthOverride(112).HeightOverride(112)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SAssignNew(SiteImage,SImage).Image(PresenceArt(TEXT("harbor-site"),112,Preferences.UiScale))]]]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SiteInfo]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SBox).HeightOverride(1)[SNew(SImage).Image(&RuleBrush)]]
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Metric(EUiGlyph::Storage,StorageValue)]
   +SHorizontalBox::Slot().FillWidth(1).Padding(4,0)[Metric(EUiGlyph::Loading,BuildValue)]
   +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Metric(EUiGlyph::Coin,UpkeepValue)]]];
 RequirementSection=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SAssignNew(RequirementsTitle,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,2,0,5)[SAssignNew(RequirementsStatus,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&CardBrush).Padding(6)[SAssignNew(RequirementRows,SVerticalBox)]];
 auto Money=[&](const FText& Title,TSharedPtr<STextBlock>& Value,TSharedPtr<STextBlock>* TitleWidget=nullptr)->TSharedRef<SWidget>{
  auto Heading=SNew(STextBlock).Text(Title).TextStyle(&CaptionStyle).AutoWrapText(true);if(TitleWidget)*TitleWidget=Heading;
  return SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Heading]+SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(Value,STextBlock).TextStyle(&DataStyle).AutoWrapText(true)];};
 FundingCard=SNew(SBorder).BorderImage(&CardBrush).Padding(8)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,5)[SAssignNew(CostTitle,STextBlock).Text(LOCTEXT("CostHeading","Trade station construction cost")).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[SNew(SBox).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible)[Art(TEXT("coin-purse"),52)]]
   +SHorizontalBox::Slot().FillWidth(1)[Money(LOCTEXT("Treasury","Treasury"),TreasuryValue)]
   +SHorizontalBox::Slot().FillWidth(1).Padding(6,0)[Money(LOCTEXT("Cost","Cost"),CostValue)]
   +SHorizontalBox::Slot().FillWidth(1)[Money(LOCTEXT("AfterFunding","After funding"),RemainderValue,&RemainderTitle)]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,5)[SAssignNew(InvestmentRow,SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[SAssignNew(InvestmentIcon,SHansaGlyph).Glyph(EUiGlyph::Check).Size(16)]
   +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(InvestmentText,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]]
   +SVerticalBox::Slot().AutoHeight()[SAssignNew(FundingHint,SBox)[Label(LOCTEXT("ChooseLater","This builds your trade station on a leased commercial site, with the storage shown above. Site reservation is free; choose an inventory and pay only after the reservation."))]]];
 Terms=MakeControl(TEXT("TradeMap.Station.Terms"),LOCTEXT("LeaseTerms","Lease terms & trading rights"),EHansaUiButtonStyle::Secondary);
 SourcePanel=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SAssignNew(SourcePicker,SComboButton).ButtonStyle(&SecondaryButtonStyle).HasDownArrow(false).OnGetMenuContent(this,&STradeEstablishment::ChoiceMenu,false)
   .ButtonContent()[SNew(SBox).MinDesiredHeight(32).VAlign(VAlign_Center)[SAssignNew(SourceLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(SourceDetail,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)];
 ConstructionCard=SNew(SBorder).BorderImage(&CardBrush).Padding(10)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SAssignNew(ConstructionText,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SBox).HeightOverride(8)[SAssignNew(ConstructionBar,SProgressBar).Style(&RequirementProgressStyle)]]];
 ShowOnMap=MakeControl(TEXT("TradeMap.Station.ShowOnMap"),LOCTEXT("ShowOnMap","Show on map"),EHansaUiButtonStyle::Secondary);
 Priority=MakeControl(TEXT("TradeMap.Station.Priority"),LOCTEXT("PriorityDelivery","Review construction priority"),EHansaUiButtonStyle::Secondary);
 Close=MakeControl(TEXT("TradeMap.Station.Close"),LOCTEXT("Close","Close station safely / recover lease"),EHansaUiButtonStyle::Secondary);
 ChildSlot[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[SAssignNew(Step,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[Steps]
  +SVerticalBox::Slot().AutoHeight()[SiteCard.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)[RequirementSection.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[FundingCard.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[SourcePanel.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[ConstructionCard.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)[Terms.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SAssignNew(Summary,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SAssignNew(Review,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Close.ToSharedRef()]];

 Market=MakeControl(TEXT("TradeMap.Station.Market"),LOCTEXT("Market","Open market / quay trade"),EHansaUiButtonStyle::Primary);Market->SetButtonStyle(&PrimaryButtonStyle);
 Market->SetContent(SNew(SBox).MinDesiredHeight(28)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Market).Size(24)]
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("Market","Open market / quay trade")).TextStyle(&DarkBodyStyle).AutoWrapText(true)]
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,0,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Arrow).Size(20)]]);
 Action=MakeControl(TEXT("TradeMap.Station.Action"),FText(),EHansaUiButtonStyle::Secondary);
 Action->SetContent(SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SAssignNew(ActionIcon,SHansaGlyph).Glyph(EUiGlyph::Lock).Size(20)]
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(ActionText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]);
 Confirm=MakeControl(TEXT("TradeMap.Station.Confirm"),FText(),EHansaUiButtonStyle::Primary);Cancel=MakeControl(TEXT("TradeMap.Station.Cancel"),LOCTEXT("Cancel","Back to choices"),EHansaUiButtonStyle::Secondary);
 Notice=SNew(SBorder).BorderImage(&NoticeBrush).Padding(6)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Warning).Size(18)]
  +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(NoticeText,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]];
 Footer=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)[SNew(SBox).HeightOverride(1)[SNew(SImage).Image(&RuleBrush)]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,4)[Notice.ToSharedRef()]
  // Wide compact inspectors share action rows so large text leaves room for the scrollable terms.
  +SVerticalBox::Slot().AutoHeight()[SNew(SUniformWrapPanel).HAlign(HAlign_Fill).SlotPadding(FMargin(0,2)).NumColumnsOverride_Lambda([this]{return GetCachedGeometry().GetLocalSize().X>=560.f?2:1;})
   +SUniformWrapPanel::Slot()[SAssignNew(MarketSlot,SBox)[Market.ToSharedRef()]]
   +SUniformWrapPanel::Slot()[Action.ToSharedRef()]
   +SUniformWrapPanel::Slot()[Priority.ToSharedRef()]
   +SUniformWrapPanel::Slot()[ShowOnMap.ToSharedRef()]
   +SUniformWrapPanel::Slot()[Confirm.ToSharedRef()]
   +SUniformWrapPanel::Slot()[Cancel.ToSharedRef()]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,3)[SAssignNew(BlockerText,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight()[SAssignNew(Feedback,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[SAssignNew(Autonomy,SBox)[Label(LOCTEXT("Autonomy","The city remains autonomous."))]];
 MapWidget(TEXT("TradeMap.Station.Footer"),Footer);MapWidget(TEXT("TradeMap.Station.Site"),SitePicker);MapWidget(TEXT("TradeMap.Station.Source"),SourcePicker);
 MapWidget(TEXT("TradeMap.Station.SiteCard"),SiteCard);MapWidget(TEXT("TradeMap.Station.Requirements"),RequirementSection);MapWidget(TEXT("TradeMap.Station.Funding"),FundingCard);
 MapWidget(TEXT("TradeMap.Station.Identity"),Step);MapWidget(TEXT("TradeMap.Station.BuildingName"),SiteLabel);MapWidget(TEXT("TradeMap.Station.Storage"),StorageValue);
 MapWidget(TEXT("TradeMap.Station.Review"),Review);MapWidget(TEXT("TradeMap.Station.Summary"),Summary);MapWidget(TEXT("TradeMap.Station.SourceDetail"),SourceDetail);MapWidget(TEXT("TradeMap.Station.Feedback"),Feedback);MapWidget(TEXT("TradeMap.Station.Blocker"),BlockerText);
}

TSharedRef<SWidget> STradeEstablishment::ChoiceMenu(bool Site) {
 const auto& Rows=Site?Sites:Sources;
 if(Rows.IsEmpty())return SNew(SBox).WidthOverride(300).Padding(12)[SNew(STextBlock).Text(Site?LOCTEXT("NoSites","No commercial site is available."):LOCTEXT("NoInventory","No eligible owned inventory. Acquire a ship or building store first.")).TextStyle(&LightBodyStyle).AutoWrapText(true)];
 return SNew(SBox).WidthOverride(320).MaxDesiredHeight(280)[SNew(SListView<TSharedPtr<FHansaEstablishmentChoice>>).ListItemsSource(Site?&Sites:&Sources).OnGenerateRow(this,&STradeEstablishment::ChoiceRow,Site)];
}
TSharedRef<ITableRow> STradeEstablishment::ChoiceRow(TSharedPtr<FHansaEstablishmentChoice> C,const TSharedRef<STableViewBase>& Owner,bool Site) {
 return SNew(STableRow<TSharedPtr<FHansaEstablishmentChoice>>,Owner)[SNew(SBox).MinDesiredHeight(48)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).ToolTipText(C->Detail).OnClicked_Lambda([this,C,Site]{if(auto* P=Model.Get()){if(Site)P->SelectEstablishmentSite(C->Id);else P->SelectEstablishmentSource(C->Id);}(Site?SitePicker:SourcePicker)->SetIsOpen(false);return FReply::Handled();})
 [SNew(STextBlock).Text(FText::Format(LOCTEXT("Choice","{0}\n{1}"),C->Label,C->bEligible?LOCTEXT("Available","Available"):LOCTEXT("NotReady","Not ready — select to inspect the reason"))).TextStyle(&LightBodyStyle).AutoWrapText(true)]]];
}
bool STradeEstablishment::ToggleTerms(){bTermsExpanded=!bTermsExpanded;Summary->SetVisibility(bTermsExpanded?EVisibility::Visible:EVisibility::Collapsed);return true;}

void STradeEstablishment::Refresh(const FHansaTradeEstablishment& E) {
 if(!E.bComplete){
  MapWidget(TEXT("TradeMap.Station.Terms"),Terms);MapWidget(TEXT("TradeMap.Station.Close"),Close);MapWidget(TEXT("TradeMap.Station.Action"),Action);MapWidget(TEXT("TradeMap.Station.ShowOnMap"),ShowOnMap);
  MapWidget(TEXT("TradeMap.Station.Identity"),Step);MapWidget(TEXT("TradeMap.Station.BuildingName"),SiteLabel);MapWidget(TEXT("TradeMap.Station.Storage"),StorageValue);MapWidget(TEXT("TradeMap.Station.Footer"),Footer);
 }
 auto Show=[](bool B){return B?EVisibility::Visible:EVisibility::Collapsed;};
 SetVisibility(Show(E.bVisible));Footer->SetVisibility(Show(E.bVisible));
 const bool Funding=E.bProposed||E.bArrears,Initial=!E.StationId;
 Step->SetText(E.bReview?LOCTEXT("ReviewStep","Review exact terms"):E.bOfficeBuilt?LOCTEXT("OfficeCompleteStep","Your Merchant Office"):E.bConstructing?LOCTEXT("ConstructStep","Your station is being built"):E.bComplete?LOCTEXT("CompleteStep","Your trade station"):E.bProposed&&E.bLocalDelivery?LOCTEXT("SupplyTitle","Deliver construction materials"):E.bProposed?LOCTEXT("FundTitle","Fund your trade station"):LOCTEXT("EstablishTitle","Establish a trade station"));
 StepLabels[1]->SetText(E.bLocalDelivery?LOCTEXT("DeliverStep","Deliver"):LOCTEXT("FundStep","Fund"));
 const int32 Current=E.bComplete?3:E.bConstructing?2:E.bProposed?1:0;for(int32 I=0;I<StepBadges.Num();++I)StepBadges[I]->SetBorderImage(I<=Current?&CurrentStepBrush:&FutureStepBrush);
 auto Update=[](auto& Rows,const auto& Choices){bool Same=Rows.Num()==Choices.Num();if(Same)for(int32 I=0;I<Rows.Num();++I)Same&=Rows[I]->Id==Choices[I].Id;if(!Same){Rows.Reset();for(const auto& C:Choices)Rows.Add(MakeShared<FHansaEstablishmentChoice>(C));}else for(int32 I=0;I<Rows.Num();++I)*Rows[I]=Choices[I];};Update(Sites,E.Sites);Update(Sources,E.Sources);
 const auto* Source=E.Sources.FindByPredicate([&](const auto& C){return C.Id==E.SourceId;});
 SiteLabel->SetText(E.SiteName);SiteStatus->SetText(E.SiteStatus);StorageValue->SetText(E.Storage);BuildValue->SetText(E.BuildDuration);UpkeepValue->SetText(E.DailyUpkeep);
 SiteImage->SetImage(PresenceArt(E.bOfficeBuilt?TEXT("MerchantOffice"):TEXT("harbor-site"),112,Preferences.UiScale));
 SiteArt->SetVisibility(Show(!Preferences.bLargeText&&E.CityId==TEXT("City.Rostock")));SiteCard->SetVisibility(Show(!E.bReview));SitePicker->SetVisibility(Show(Initial));SitePicker->SetEnabled(!E.bPending);
 RequirementsTitle->SetText(FText::Format(LOCTEXT("EarnPlace","Qualify for a trade station in {0}"),E.CityLabel));
 RequirementsStatus->SetText(E.UnmetRequirements?FText::Format(LOCTEXT("UnmetCount","{0} requirements remaining. Settled route sales into this city count toward trade and delivery progress."),FText::AsNumber(E.UnmetRequirements)):LOCTEXT("ProgressMet","Trading requirements met. Review the site reservation below."));RequirementSection->SetVisibility(Show(Initial&&!E.bReview&&!E.Requirements.IsEmpty()));
 FString Key;for(const auto& R:E.Requirements)Key+=R.Id+R.Value.ToString()+LexToString(R.bMet);
 if(Key!=RequirementKey){RequirementKey=Key;RequirementRows->ClearChildren();for(const auto& R:E.Requirements){
  if(RequirementRows->NumSlots())RequirementRows->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(1)[SNew(SImage).Image(&RuleBrush)]];
  auto Row=SNew(SHorizontalBox).ToolTipText(FText::Format(LOCTEXT("RequirementHint","{0}\n{1}"),R.bMet?LOCTEXT("Met","Met"):LOCTEXT("Unmet","Unmet"),R.Hint))
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[SNew(SHansaGlyph).Glyph(R.bMet?EUiGlyph::Check:EUiGlyph::Warning).Size(16)]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
     +SHorizontalBox::Slot().FillWidth(1.1f).VAlign(VAlign_Center)[SNew(STextBlock).Text(R.Label).TextStyle(&LightBodyStyle).AutoWrapText(true)]
     +SHorizontalBox::Slot().FillWidth(1).Padding(5,0,0,0).VAlign(VAlign_Center)[SNew(STextBlock).Text(R.Value).TextStyle(&CaptionStyle).Justification(ETextJustify::Right).AutoWrapText(true)]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,3,0,0)[SNew(SBox).HeightOverride(3)[SNew(SProgressBar).Style(&RequirementProgressStyle).Percent(R.Progress)]]];
  RequirementRows->AddSlot().AutoHeight().Padding(2,5)[Row];MapWidget(TEXT("TradeMap.Station.Requirement.")+R.Id,Row);
 }}
 TreasuryValue->SetText(E.Treasury);CostValue->SetText(E.Cost);RemainderValue->SetText(E.Remainder);
 CostTitle->SetText(E.bArrears?LOCTEXT("ArrearsHeading","Outstanding upkeep"):E.bAwaitingPickup||E.bConstructing||E.bComplete?LOCTEXT("PaidHeading","Trade station construction paid"):LOCTEXT("CostHeading","Trade station construction cost"));RemainderTitle->SetText(E.bAwaitingPickup||E.bConstructing||E.bComplete?LOCTEXT("Now","Current balance"):LOCTEXT("AfterFunding","After funding"));
 FundingCard->SetVisibility(Show(!E.bReview&&!E.bComplete));
 Priority->SetVisibility(Show(E.bCanPrioritizeDelivery&&!E.bReview));Priority->SetEnabled(Model.IsValid()&&Model->CanEstablishmentIntent(TEXT("Priority")));InvestmentText->SetText(E.Investment);InvestmentText->SetColorAndOpacity(Color(EHansaUiColorToken::Ink));InvestmentIcon->SetGlyph(E.bInvestmentMet?EUiGlyph::Check:EUiGlyph::Warning);InvestmentRow->SetVisibility(Show(E.bHasInvestment&&Initial));FundingHint->SetVisibility(Show(Initial));
 SourceLabel->SetText(Source?FText::Format(E.bLocalDelivery?LOCTEXT("DeliverySourceLabel","Selected supply source: {0}"):LOCTEXT("SourceLabel","Fund from: {0}"),Source->Label):LOCTEXT("ChooseSource","Choose funding inventory"));SourcePanel->SetVisibility(Show(Funding&&!E.bReview));SourcePicker->SetEnabled(!E.bPending&&(!E.bAwaitingPickup||E.bLocalDelivery));SourceDetail->SetText(E.SourceDetail);
 ConstructionCard->SetVisibility(Show(E.bConstructing));ConstructionText->SetText(E.ConstructionRemaining);ConstructionBar->SetPercent(E.ConstructionProgress);
 Terms->SetEnabled(!E.bPending);Terms->SetVisibility(Show(!E.bReview));Summary->SetText(E.Summary);Summary->SetVisibility(Show(bTermsExpanded&&!E.bReview));Review->SetText(E.Confirmation);Review->SetVisibility(Show(E.bReview));
 const bool CanAct=Model.IsValid()&&Model->CanEstablishmentIntent(TEXT("Review"));ActionText->SetText(E.Action);ActionIcon->SetGlyph(CanAct?EUiGlyph::Arrow:EUiGlyph::Lock);Action->SetEnabled(CanAct);Action->SetVisibility(Show(!E.bReview));Action->SetToolTipText(E.Blocker);
 Market->SetEnabled(E.bCanOpenMarket&&!E.bPending);MarketSlot->SetVisibility(Show(Initial&&!E.bReview));Market->SetToolTipText(E.bCanOpenMarket?LOCTEXT("MarketHint","Inspect the city market and trade from an owned ship at berth."):LOCTEXT("MarketLocked","Market access is unavailable. Review this city's trading rights."));
 NoticeText->SetText(FText::Format(LOCTEXT("TradeNotice","Quay trade in {0} requires an owned Cog berthed there. To qualify for a station, sell goods there through an operating route."),E.CityLabel));Notice->SetVisibility(Show(Initial&&E.UnmetRequirements>0&&!E.bReview&&!Preferences.bLargeText));
 BlockerText->SetText(E.bPending?LOCTEXT("Waiting","Waiting for confirmation…"):E.Blocker);BlockerText->SetVisibility(Show(!E.bReview||E.bPending));
 Confirm->SetLabel(E.bLocalDelivery&&E.bProposed?E.bPriorityReview?LOCTEXT("ConfirmPriority","Prioritize construction · no payment"):LOCTEXT("ConfirmSupply","Arrange delivery · no payment"):Funding?FText::Format(LOCTEXT("SpendSource","Pay {0} and start construction order"),E.Cost):LOCTEXT("Propose","Reserve trade station site — no payment"));Confirm->SetToolTipText(E.Confirmation);Confirm->SetVisibility(Show(E.bReview));Cancel->SetVisibility(Show(E.bReview));Confirm->SetEnabled(!E.bPending&&(Funding?E.bCanFund:E.bCanPropose));Cancel->SetEnabled(!E.bPending);
 ShowOnMap->SetVisibility(Show((E.bComplete||E.bLocalDelivery)&&!E.bReview));ShowOnMap->SetEnabled(Model.IsValid()&&Model->CanEstablishmentIntent(TEXT("ShowOnMap")));
 Feedback->SetText(E.Feedback);Feedback->SetVisibility(Show(!E.Feedback.IsEmpty()));Close->SetVisibility(Show(E.StationId&&!E.bReview));Close->SetEnabled(!E.bPending);Autonomy->SetVisibility(Show(!Preferences.bLargeText&&!E.bReview));
 Close->SetLabel(E.bOfficeBuilt?LOCTEXT("CloseOffice","Close Merchant Office safely / recover lease"):LOCTEXT("Close","Close station safely / recover lease"));
}
#undef LOCTEXT_NAMESPACE
