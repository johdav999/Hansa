#include "STradeStationDetails.h"
#include "TradeArtwork.h"
#include "UI/SHansaReferenceFrame.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Misc/Paths.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HansaStationDetails"
namespace Hansa::UI {
namespace {
FLinearColor Color(EHansaUiColorToken T){return UHansaUiStyleLibrary::GetColor(T);}
FText Quantity(int64 Raw){return FText::AsNumber(double(Raw)/1000.);}
float ControlContentHeight(const FUiPreferences& Preferences,EHansaUiButtonStyle Kind){
 const auto Style=UHansaUiStyleLibrary::GetButtonStyle(Kind);
 return FMath::Max(32.f,49.f/FMath::Min(1.f,FMath::Clamp(Preferences.UiScale,.8f,1.4f))-Style.NormalPadding.Top-Style.NormalPadding.Bottom);
}
const FSlateBrush* BannerBrush(float Scale){
 static TMap<int32,TSharedPtr<FSlateDynamicImageBrush>> Images;
 const int32 Width=Scale>1.1f?720:Scale>.8f?540:360;
 auto& Image=Images.FindOrAdd(Width);
 if(!Image)Image=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/FString::Printf(TEXT("Hansa/UI/TradeStationDetails/harbor--%d.png"),Width))),FVector2D(Width,Width/3));
 return Image.Get();
}
EUiGlyph RightGlyph(const FString& Id){
 if(Id.Contains(TEXT("Storage")))return EUiGlyph::Storage;
 if(Id.Contains(TEXT("Report")))return EUiGlyph::Save;
 if(Id.Contains(TEXT("PublicMarket")))return EUiGlyph::Market;
 if(Id.Contains(TEXT("Route")))return EUiGlyph::Ship;
 if(Id.Contains(TEXT("Order")))return EUiGlyph::Research;
 return EUiGlyph::Building;
}
}
TSharedRef<SWidget> STradeStationDetails::Text(const FText& Value,bool Heading){return SNew(STextBlock).Text(Value).TextStyle(Heading?&LightHeadingStyle:&LightBodyStyle).AutoWrapText(true);}
TSharedRef<SWidget> STradeStationDetails::Rule(){return SNew(SBox).HeightOverride(1)[SNew(SImage).Image(&RuleBrush)];}
TSharedRef<SHansaAction> STradeStationDetails::Control(const TCHAR* Id,const FText& Label,EUiGlyph Glyph,EHansaUiButtonStyle Kind){
 auto Button=MakeControl(Id,Label,Kind);
 Button->SetContent(SNew(SBox).MinDesiredHeight(ControlContentHeight(Preferences,Kind))[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(Glyph).Size(24)]
 +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this,Label,SemanticId=FString(Id)]{
  if(SemanticId==TEXT("TradeMap.Station.Action")&&Model.IsValid()&&Model->GetSnapshot().Establishment.bArrears)return LOCTEXT("StationRecovery","Review station recovery");
  if(SemanticId==TEXT("TradeMap.Presence.Upgrade")&&Model.IsValid())return Model->GetSnapshot().bPresenceReview?Model->GetSnapshot().PresenceUpgradeAction:Model->IsAutomaticMerchantOfficeUpgrade()?LOCTEXT("ReviewUpgrade","Review Merchant Office upgrade"):Model->GetSnapshot().PresenceUpgradeAction;
  return Label;
 }).TextStyle(Kind==EHansaUiButtonStyle::Primary?&DarkBodyStyle:&LightBodyStyle).AutoWrapText(true)]]);
 return Button;
}
TSharedRef<SWidget> STradeStationDetails::Metric(const FText& Label,EUiGlyph Glyph,TSharedPtr<STextBlock>& Value){return SNew(SHorizontalBox)
 +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(Glyph).Size(32)]
 +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(Label).TextStyle(&CaptionStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Value,STextBlock).TextStyle(&ValueStyle).AutoWrapText(true)]];}
TSharedRef<SWidget> STradeStationDetails::LedgerValue(const FText& Label,TSharedPtr<STextBlock>& Value,bool Strong){return SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(.9f).VAlign(VAlign_Center)[Text(Label,Strong)]
 +SHorizontalBox::Slot().FillWidth(1.1f).VAlign(VAlign_Center).Padding(8,0,0,0)[SAssignNew(Value,STextBlock).TextStyle(Strong?&ValueStyle:&LightBodyStyle).Justification(ETextJustify::Right).AutoWrapText(true)];}

void STradeStationDetails::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In){
 Initialize(In);
 CardBrush=FSlateRoundedBoxBrush(Color(EHansaUiColorToken::Parchment),2.f,Color(EHansaUiColorToken::Brass),1.f);
 RuleBrush=FSlateColorBrush(Color(EHansaUiColorToken::Brass)*.55f);
 DarkBrush=FSlateColorBrush(Color(EHansaUiColorToken::BalticNavy));
 CaptionStyle=LightBodyStyle;CaptionStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Caption,Preferences)).SetColorAndOpacity(Color(EHansaUiColorToken::MutedInk));
 ValueStyle=LightHeadingStyle;ValueStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
 OverviewTab=Control(TEXT("TradeMap.Station.Tab.Overview"),LOCTEXT("OverviewTab","Overview"),EUiGlyph::Storage);
 UpgradeTab=Control(TEXT("TradeMap.Station.Tab.Upgrade"),LOCTEXT("UpgradeTab","Upgrade"),EUiGlyph::Tools);
 LocalTabs=SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(1)[OverviewTab.ToSharedRef()]
 +SHorizontalBox::Slot().FillWidth(1)[UpgradeTab.ToSharedRef()];
 Banner=SNew(SBox).HeightOverride_Lambda([this]{return FMath::Max(90.f,GetCachedGeometry().GetLocalSize().X/3.f);})
 [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(BannerBrush(Preferences.UiScale))]];
 auto Metrics=SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Metric(LOCTEXT("Storage","Storage used"),EUiGlyph::Storage,Storage)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[SNew(SBox).HeightOverride(5)[SNew(SBorder).BorderImage(&DarkBrush).Padding(0)[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]{return FMath::Max(.0001f,StorageFraction);}))[SNew(SImage).Image(&RuleBrush).Visibility_Lambda([this]{return StorageFraction>0?EVisibility::Visible:EVisibility::Hidden;})]
 +SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]{return FMath::Max(.0001f,1.f-StorageFraction);}))[SNew(SBox)]]]]]
 +SHorizontalBox::Slot().FillWidth(1).Padding(8,0,0,0)[Metric(LOCTEXT("Upkeep","Daily upkeep"),EUiGlyph::Coin,Upkeep)];
 UpgradeAction=Control(TEXT("TradeMap.Presence.Upgrade"),LOCTEXT("ReviewUpgrade","Review Merchant Office upgrade"),EUiGlyph::Tools,EHansaUiButtonStyle::Primary);
 EditReview=Control(TEXT("TradeMap.Presence.Cancel"),LOCTEXT("EditReview","Edit review"),EUiGlyph::Back);
 auto Source=SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Source","Material source"))]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(SourcePicker,SComboButton).ButtonStyle(&SecondaryButtonStyle).OnGetMenuContent(this,&STradeStationDetails::SourceMenu)
 .ButtonContent()[SNew(SBox).MinDesiredHeight(ControlContentHeight(Preferences,EHansaUiButtonStyle::Secondary))[SAssignNew(SourceLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]];
 SourceHost=Source;
 SAssignNew(Upgrade,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(UpgradeHeading,STextBlock).TextStyle(&CaptionStyle)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[SNew(SBox).WidthOverride(88).HeightOverride(88).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible)
 [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(TradeIconArtwork(TEXT("MerchantOffice"),112))]]]
 +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Office","Merchant Office"),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(UpgradeBenefit,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]]
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Operational","Station stays operational."))]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Materials,SVerticalBox)]
 +SVerticalBox::Slot().AutoHeight()[Source]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(MoneyHost,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Rule()]
 +SVerticalBox::Slot().AutoHeight().Padding(0,2)[LedgerValue(LOCTEXT("Treasury","Treasury"),Treasury)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,2)[LedgerValue(LOCTEXT("Cost","Upgrade cost"),Cost)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Rule()]
 +SVerticalBox::Slot().AutoHeight()[LedgerValue(LOCTEXT("After","After upgrade"),Remainder,true)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Duration,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Delivery,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(ReviewTerms,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Conditions,SVerticalBox)]
 +SVerticalBox::Slot().AutoHeight()[UpgradeAction.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[EditReview.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(UpgradeReviewNotice,SBox)[Text(LOCTEXT("ReviewOnly","Payment is made on confirmation."))]];
 Terms=Control(TEXT("TradeMap.Station.Terms"),LOCTEXT("Terms","Lease terms & trading rights"),EUiGlyph::Save);
 Stock=Control(TEXT("TradeMap.Station.Overview.Stock"),LOCTEXT("Stock","View all stock & reservations"),EUiGlyph::Storage);
 OrdersLink=Control(TEXT("TradeMap.Station.Overview.Orders"),LOCTEXT("ReviewOrders","Review orders"),EUiGlyph::Research);
 Market=Control(TEXT("TradeMap.Station.Market"),LOCTEXT("Market","Open market"),EUiGlyph::Market,EHansaUiButtonStyle::Primary);
 Closure=Control(TEXT("TradeMap.Station.Close"),LOCTEXT("Closure","Review station closure"),EUiGlyph::Building);
 Operations=Control(TEXT("TradeMap.Station.Action"),LOCTEXT("Operations","Open station operations"),EUiGlyph::Settings,EHansaUiButtonStyle::Primary);
 ShowMap=Control(TEXT("TradeMap.Station.ShowOnMap"),LOCTEXT("Map","Show on map"),EUiGlyph::Map);
 Return=Control(TEXT("TradeMap.Station.Return"),LOCTEXT("Return","Return to station"),EUiGlyph::Back,EHansaUiButtonStyle::Primary);
 ChildSlot[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[LocalTabs.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight()[Banner.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight().Padding(0,12)[Metrics]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(SiteName,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SAssignNew(UpkeepSummary,STextBlock).TextStyle(&CaptionStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SAssignNew(Overview,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(UpgradeSummary,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight()[SNew(SHansaReferenceFrame).Dark(false).Padding(12)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("StoredGoods","Stored goods"),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SHorizontalBox)
 +SHorizontalBox::Slot().FillWidth(1.5f)[Text(LOCTEXT("Goods","Goods"))]
 +SHorizontalBox::Slot().FillWidth(1)[Text(LOCTEXT("Available","Available"))]
 +SHorizontalBox::Slot().FillWidth(1)[Text(LOCTEXT("Reserved","Reserved"))]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Goods,SVerticalBox)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,0)[Stock.ToSharedRef()]]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHansaReferenceFrame).Dark(false).Padding(12)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Trading","Trading"),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(TradingSummary,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SAssignNew(TradingWarning,SHorizontalBox)
 +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Warning).Size(24)]
 +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(TradingBlocker,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight()[OrdersLink.ToSharedRef()]]]
 +SVerticalBox::Slot().AutoHeight()[SNew(SHansaReferenceFrame).Dark(false).Padding(12)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Transport","Transport"),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(TransportRows,SVerticalBox)]]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SNew(SHansaReferenceFrame).Dark(false).Padding(12)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Activity","Recent activity"),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(ActivityRows,SVerticalBox)]]]
 +SVerticalBox::Slot().AutoHeight()[Terms.ToSharedRef()]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,12)[SAssignNew(Main,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(UpgradeHost,SHansaReferenceFrame).Dark(false).Padding(12)[Upgrade.ToSharedRef()]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(OfficeCard,SHansaReferenceFrame).Dark(false).Padding(12)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[SNew(SBox).WidthOverride(88).HeightOverride(88).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(TradeIconArtwork(TEXT("MerchantOffice"),112))]]]
  +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Office","Merchant Office"),true)]
   +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Text(LOCTEXT("OfficeFacilities","Merchant Office upgrade complete. Inspect stock and lease rights under Overview, and trading instructions under Orders."))]]]]
 ]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(Lease,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[LedgerValue(LOCTEXT("Paid","Construction paid"),Paid)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Text(LOCTEXT("Rights","Your trading rights"),true)]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Rights,SVerticalBox)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(SBorder).BorderImage(&CardBrush).Padding(12)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Separate","Separate rights required"),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(LeaseState,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Rule()]
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Autonomy","The city remains autonomous."),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Text(LOCTEXT("Scope","This lease grants commercial access, not authority over the city."))]]]
 +SVerticalBox::Slot().AutoHeight()[SNew(SHansaReferenceFrame).Dark(false).Padding(12)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("IfClose","If you close the station"),true)]
 +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Text(LOCTEXT("Preserved","Stock and ship cargo are preserved. Clear stock, reservations and buildings before releasing the lease."))]
 +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Recovery","Orders pause. Traveling ships retain their journey. Recover station stock through outbound routes."))]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(RefundNotice,SBox)[SAssignNew(Refund,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight()[Closure.ToSharedRef()]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Text(LOCTEXT("BeforeConfirm","Review consequences before confirming."))]]]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Feedback,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]];
 ReviewConfirm=Control(TEXT("TradeMap.Presence.Upgrade"),LOCTEXT("ConfirmUpgrade","Confirm upgrade"),EUiGlyph::Tools,EHansaUiButtonStyle::Primary);
 ReviewEdit=Control(TEXT("TradeMap.Presence.Cancel"),LOCTEXT("EditReview","Edit review"),EUiGlyph::Back);
 Footer=SNew(SBorder).BorderImage(&DarkBrush).Padding(8)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(ReviewFooter,SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(0,2)[ReviewConfirm.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,2)[ReviewEdit.ToSharedRef()]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(NormalFooter,SUniformWrapPanel).HAlign(HAlign_Fill).SlotPadding(FMargin(2)).NumColumnsOverride_Lambda([this]{return GetCachedGeometry().GetLocalSize().X>=420.f&&!Preferences.bLargeText?2:1;})
 +SUniformWrapPanel::Slot()[Market.ToSharedRef()]
 +SUniformWrapPanel::Slot()[Operations.ToSharedRef()]
 +SUniformWrapPanel::Slot()[Return.ToSharedRef()]
 +SUniformWrapPanel::Slot()[ShowMap.ToSharedRef()]]];
 MapWidget(TEXT("TradeMap.Station.Details"),SharedThis(this));
 MapWidget(TEXT("TradeMap.Station.BuildingName"),SiteName);MapWidget(TEXT("TradeMap.Station.Storage"),Storage);
 MapWidget(TEXT("TradeMap.Station.Rights"),Rights);MapWidget(TEXT("TradeMap.Station.ConstructionPaid"),Paid);
 MapWidget(TEXT("TradeMap.Station.UpgradeCard"),Upgrade);MapWidget(TEXT("TradeMap.Station.Lease"),Lease);
 MapWidget(TEXT("TradeMap.Station.Materials"),Materials);MapWidget(TEXT("TradeMap.Station.UpgradeCost"),Cost);
 MapWidget(TEXT("TradeMap.Presence.Source"),SourcePicker);
}
TSharedRef<SWidget> STradeStationDetails::SourceMenu(){
 auto Rows=SNew(SVerticalBox);
 if(Model.IsValid())for(const auto& C:Model->GetSnapshot().PresenceSources){Rows->AddSlot().AutoHeight().Padding(0,2)[SNew(SButton).ButtonStyle(&SecondaryButtonStyle).ToolTipText(C.Detail).OnClicked_Lambda([this,Id=C.Id]{if(Model.IsValid())Model->SelectPresenceSource(Id);SourcePicker->SetIsOpen(false);return FReply::Handled();})[SNew(SBox).MinDesiredHeight(48)[Text(C.Label)]]];}
 return SNew(SBox).WidthOverride(320).MaxDesiredHeight(320)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]];
}
void STradeStationDetails::RefreshMaterials(const FHansaEstablishmentChoice* Source){
 TArray<FString> Ids;if(Source)for(const auto& M:Source->Materials)Ids.Add(M.GoodId);
 if(Ids!=MaterialIds){MaterialIds=Ids;Materials->ClearChildren();MaterialRequired.Reset();MaterialAvailable.Reset();MaterialStatus.Reset();
  Materials->AddSlot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1.4f)[Text(LOCTEXT("Material","Material"))]
  +SHorizontalBox::Slot().FillWidth(.8f)[Text(LOCTEXT("Required","Required"))]
  +SHorizontalBox::Slot().FillWidth(1)[Text(LOCTEXT("Available","Available"))]];
  if(Source)for(const auto& M:Source->Materials){TSharedPtr<STextBlock> Required,Available;TSharedPtr<SHansaGlyph> Status;
   Materials->AddSlot().AutoHeight()[Rule()];
   auto Row=SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1.4f).VAlign(VAlign_Center)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,6,0)[SNew(SHansaGlyph).Glyph(GlyphForGood(FName(*M.GoodId))).Size(24)]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(M.Label)]]
   +SHorizontalBox::Slot().FillWidth(.8f).VAlign(VAlign_Center)[SAssignNew(Required,STextBlock).TextStyle(&LightBodyStyle)]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,4,0)[SAssignNew(Status,SHansaGlyph).Size(16)]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(Available,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]];
   Materials->AddSlot().AutoHeight().Padding(0,6)[Row];MaterialRequired.Add(Required);MaterialAvailable.Add(Available);MaterialStatus.Add(Status);
   MapWidget(TEXT("TradeMap.Station.Material.")+M.GoodId,Row);
  }
 }
 if(Source)for(int32 I=0;I<Source->Materials.Num();++I){const auto& M=Source->Materials[I];MaterialRequired[I]->SetText(Quantity(M.RequiredMilliUnits));MaterialAvailable[I]->SetText(Quantity(M.AvailableMilliUnits));MaterialStatus[I]->SetGlyph(M.AvailableMilliUnits>=M.RequiredMilliUnits?EUiGlyph::Check:EUiGlyph::Warning);}
}
void STradeStationDetails::RefreshRights(const FHansaTradeEstablishment& E){
 TArray<FString> Ids;for(const auto& R:E.Rights)Ids.Add(R.Id);
 if(Ids!=RightIds){RightIds=Ids;Rights->ClearChildren();RightStates.Reset();RightIcons.Reset();
  for(const auto& R:E.Rights){TSharedPtr<STextBlock> State;TSharedPtr<SHansaGlyph> Icon;
   Rights->AddSlot().AutoHeight()[Rule()];auto Row=SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(RightGlyph(R.Id)).Size(28)]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(R.Label)]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8,0,4,0)[SAssignNew(Icon,SHansaGlyph).Size(18)]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(State,STextBlock).TextStyle(&LightBodyStyle)];
   Rights->AddSlot().AutoHeight().Padding(0,8)[Row];RightStates.Add(State);RightIcons.Add(Icon);MapWidget(TEXT("TradeMap.Station.Right.")+R.Id,State);
  }
  if(E.Rights.IsEmpty())Rights->AddSlot().AutoHeight()[Text(LOCTEXT("UnknownRights","Trading rights unavailable. Inspect city policy."))];
 }
 for(int32 I=0;I<E.Rights.Num();++I){RightStates[I]->SetText(E.Rights[I].bGranted?LOCTEXT("Granted","Granted"):LOCTEXT("Unavailable","Unavailable"));RightIcons[I]->SetGlyph(E.Rights[I].bGranted?EUiGlyph::Check:EUiGlyph::Lock);}
}
void STradeStationDetails::RefreshOverview(){
 const auto V=Model->GetStationOverviewPresentation();Storage->SetText(V.Storage);StorageFraction=V.StorageFraction;
 UpkeepSummary->SetText(V.UpkeepState);TradingSummary->SetText(V.Trading);TradingBlocker->SetText(V.Blocker);
 TradingWarning->SetVisibility(V.Blocker.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
 UpgradeSummary->SetText(V.UpgradeStatus);UpgradeSummary->SetVisibility(V.UpgradeStatus.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
 TArray<const FHansaTradeLedgerRow*> Rows;TArray<FName> Ids;
 if(V.Ledger.bAvailable)for(const auto& R:V.Ledger.Rows)if(R.Matches(0)&&Rows.Num()<5){Rows.Add(&R);Ids.Add(R.Good);}
 if(Ids!=GoodsIds||Rows.IsEmpty()||Goods->NumSlots()==0){
  GoodsIds=Ids;Goods->ClearChildren();GoodsAvailable.Reset();GoodsReserved.Reset();
  for(const auto* R:Rows){TSharedPtr<STextBlock> Available,Reserved;
   Goods->AddSlot().AutoHeight()[Rule()];auto Row=SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1.5f).VAlign(VAlign_Center)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SHansaGlyph).Glyph(GlyphForGood(R->Good)).Size(24)]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(R->Label)]]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(Available,STextBlock).TextStyle(&ValueStyle)]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(Reserved,STextBlock).TextStyle(&LightBodyStyle)];
   Goods->AddSlot().AutoHeight().Padding(0,6)[Row];GoodsAvailable.Add(Available);GoodsReserved.Add(Reserved);
   MapWidget(TEXT("TradeMap.Station.Overview.Good.")+R->Good.ToString(),Row);
  }
  if(Rows.IsEmpty())Goods->AddSlot().AutoHeight().Padding(0,6)[Text(V.Ledger.bAvailable?LOCTEXT("EmptyStock","No stored goods or active stock commitments."):LOCTEXT("UnknownStock","Station inventory unavailable."))];
 }
 for(int32 I=0;I<Rows.Num();++I){GoodsAvailable[I]->SetText(Quantity(Rows[I]->Available));GoodsReserved[I]->SetText(Quantity(Rows[I]->Reserved));}
 Stock->SetEnabled(V.Ledger.bAvailable);Stock->SetToolTipText(LOCTEXT("StockScope","First five stocked or committed goods. Open the ledger for all goods and reservation details. Quantities are units."));
 TransportRows->ClearChildren();
 for(int32 I=0;I<FMath::Min(2,V.Transport.Num());++I){const auto& R=V.Transport[I];auto Row=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[Text(R.Label,true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,3)[Text(R.Detail)];TransportRows->AddSlot().AutoHeight().Padding(0,4)[Row];MapWidget(TEXT("TradeMap.Station.Overview.Route.")+R.Id,Row);}
 TransportRows->AddSlot().AutoHeight()[Text(V.Transport.IsEmpty()?LOCTEXT("NoTransport","No owned route serving this station."):LOCTEXT("TransportScope","Routes serving this station. Route plans are not committed deliveries."))];
 ActivityRows->ClearChildren();
 for(const auto& R:V.Activity){auto Row=SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1.5f)[Text(R.Detail)]
  +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(R.Label).TextStyle(&CaptionStyle).AutoWrapText(true).Justification(ETextJustify::Right)];
  ActivityRows->AddSlot().AutoHeight().Padding(0,4)[Row];MapWidget(TEXT("TradeMap.Station.Overview.Activity.")+R.Id,Row);}
 ActivityRows->AddSlot().AutoHeight()[Text(V.Activity.IsEmpty()?LOCTEXT("NoActivity","No completed trades in retained order history."):LOCTEXT("ActivityScope","Latest completed trades from retained order history."))];
 MapWidget(TEXT("TradeMap.Station.Overview.Trading"),TradingSummary);MapWidget(TEXT("TradeMap.Station.Overview.Blocker"),TradingBlocker);
 MapWidget(TEXT("TradeMap.Station.Overview.Upgrade"),UpgradeSummary);MapWidget(TEXT("TradeMap.Station.Overview.Upkeep"),UpkeepSummary);
}
void STradeStationDetails::Refresh(){
 if(!Model.IsValid())return;const auto& S=Model->GetSnapshot();const auto& E=S.Establishment;
 if(DisplayedStation!=E.StationId){DisplayedStation=E.StationId;bLeaseOpen=false;}
 auto Show=[](bool B){return B?EVisibility::Visible:EVisibility::Collapsed;};
 const bool IsUpgrade=S.ActiveSection==TEXT("StationUpgrade");
 Main->SetVisibility(Show(!bLeaseOpen&&IsUpgrade));Overview->SetVisibility(Show(!bLeaseOpen&&!IsUpgrade));Lease->SetVisibility(Show(bLeaseOpen));
 LocalTabs->SetVisibility(Show(!Model->bWorldStationDetail));OverviewTab->SetState(IsUpgrade?EUiState::Default:EUiState::Selected);UpgradeTab->SetState(IsUpgrade?EUiState::Selected:EUiState::Default);
 Banner->SetVisibility(Show(!Preferences.bLargeText&&!IsUpgrade&&!bLeaseOpen&&E.CityId==TEXT("City.Rostock")));
 SiteName->SetText(E.SiteName);Upkeep->SetText(E.DailyUpkeep);Paid->SetText(E.ConstructionPaid);RefreshOverview();
 LeaseState->SetText(E.bOfficeBuilt?LOCTEXT("OfficeScope","Specialization and broader construction require separate rights."):LOCTEXT("StationScope","Office upgrades, specialization and broader construction require separate rights."));
 Refund->SetText(FText::Format(LOCTEXT("RefundTerms","Pending upgrade refund: {0}% of paid upgrade money and delivered materials. Undelivered cargo stays aboard."),FText::AsNumber(double(E.CancellationRefundBasisPoints)/100.)));
 RefundNotice->SetVisibility(Show(E.bPendingUpgrade));RefreshRights(E);
 const auto* Source=S.PresenceSources.FindByPredicate([&](const auto& C){return C.Id==S.PresenceSourceId;});
 const bool CanUpgrade=Model->IsAutomaticMerchantOfficeUpgrade()&&!S.PresenceSources.IsEmpty()&&!E.bOfficeBuilt;
 const bool OfficeCandidate=!E.bOfficeBuilt&&S.bPresenceOfficeVisual;
 UpgradeHeading->SetText(E.bPendingUpgrade?LOCTEXT("UpgradeProgress","Upgrade progress"):LOCTEXT("Next","Next upgrade"));MoneyHost->SetVisibility(Show(CanUpgrade&&Source&&!Source->UpgradeCost.IsEmpty()));
 ReviewTerms->SetText(FText::Format(LOCTEXT("UpgradeCancellation","If you close the station during the upgrade, {0}% of paid upgrade money and delivered materials is refunded. Undelivered cargo stays aboard."),FText::AsNumber(double(E.CancellationRefundBasisPoints)/100.)));
 ReviewTerms->SetVisibility(Show(S.bPresenceReview));
 UpgradeHost->SetVisibility(Show(OfficeCandidate||(!E.bOfficeBuilt&&!S.PresenceFundingDetail.IsEmpty())));OfficeCard->SetVisibility(Show(E.bOfficeBuilt));
 UpgradeBenefit->SetText(Source?Source->UpgradeBenefit:FText());Treasury->SetText(Source?Source->UpgradeTreasury:E.Treasury);Cost->SetText(Source?Source->UpgradeCost:LOCTEXT("Unknown","Unavailable"));Remainder->SetText(Source?Source->UpgradeRemainder:LOCTEXT("Unknown","Unavailable"));
 Duration->SetText(Source?FText::Format(LOCTEXT("Duration","Construction: {0} after materials arrive."),Source->UpgradeDuration):FText());
 Duration->SetVisibility(Show(Source!=nullptr));
 SourceLabel->SetText(Source?Source->Label:LOCTEXT("ChooseSource","Choose a delivery source"));SourcePicker->SetEnabled(!S.bPresenceReview&&!S.PresenceSources.IsEmpty());
 SourceHost->SetVisibility(Show(OfficeCandidate&&!S.PresenceSources.IsEmpty()));Materials->SetVisibility(Show(Source&&!Source->Materials.IsEmpty()));RefreshMaterials(Source);
 Materials->SetToolTipText(LOCTEXT("AvailableStock","Available stock includes usable station storage and the selected delivery source. Reserved stock is excluded."));
 const FText DeliveryText=CanUpgrade?(S.bPresenceReview?(Source?(Source->bLocalStationSource?LOCTEXT("UseLocalMaterials","Use materials stored in this station. Pay once on confirmation. Construction starts automatically."):Source->Transfer):FText()):(Source&&!Source->bEligible?S.PresenceFundingDetail:FText())):S.PresenceFundingDetail;
 Delivery->SetText(DeliveryText);Delivery->SetVisibility(Show(!DeliveryText.IsEmpty()));
 UpgradeAction->SetEnabled(S.bCanPresenceUpgradeAction);UpgradeAction->SetVisibility(Show(OfficeCandidate&&!S.bPresenceReview&&(!E.bPendingUpgrade||!S.PresenceSources.IsEmpty())));
 UpgradeAction->SetToolTipText(S.PresenceReview);EditReview->SetVisibility(EVisibility::Collapsed);UpgradeReviewNotice->SetVisibility(Show(CanUpgrade&&!S.bPresenceReview));
 ReviewFooter->SetVisibility(Show(S.bPresenceReview&&IsUpgrade));NormalFooter->SetVisibility(Show(!S.bPresenceReview||!IsUpgrade));ReviewConfirm->SetEnabled(S.bCanPresenceUpgradeAction);ReviewConfirm->SetToolTipText(S.PresenceReview);
 Conditions->ClearChildren();for(const auto& R:S.PresenceRequirements)if(!R.bMet&&R.RequirementId!=TEXT("AvailableMoney")&&!R.RequirementId.StartsWith(TEXT("UpgradeGood.")))Conditions->AddSlot().AutoHeight().Padding(0,4)[Text(FText::FromString(R.Description))];
 Feedback->SetText(E.bArrears?E.Blocker:E.bOfficeBuilt||Model->GetPresenceActionFeedback().IsEmpty()?E.Feedback:Model->GetPresenceActionFeedback());Feedback->SetVisibility(Show(!Feedback->GetText().IsEmpty()));
 Operations->SetVisibility(Show(!bLeaseOpen&&E.bArrears));Market->SetVisibility(Show(!bLeaseOpen&&!E.bArrears));Market->SetEnabled(E.bCanOpenMarket&&!Model->IsAnyTradeCommandPending());Return->SetVisibility(Show(bLeaseOpen));Operations->SetEnabled(E.bArrears?!Model->IsAnyTradeCommandPending():Model->CanEstablishmentIntent(TEXT("Review")));ShowMap->SetEnabled(Model->CanEstablishmentIntent(TEXT("ShowOnMap")));
 Closure->SetEnabled(!E.bPending&&!S.bPresenceReview);Terms->SetEnabled(!S.bPresenceReview);Return->SetEnabled(!S.bPresenceReview);
 MapWidget(TEXT("TradeMap.Station.Action"),Operations);MapWidget(TEXT("TradeMap.Station.Terms"),Terms);MapWidget(TEXT("TradeMap.Station.Close"),Closure);MapWidget(TEXT("TradeMap.Station.ShowOnMap"),ShowMap);MapWidget(TEXT("TradeMap.Station.Return"),Return);
 MapWidget(TEXT("TradeMap.Presence.Upgrade"),S.bPresenceReview?ReviewConfirm:UpgradeAction);MapWidget(TEXT("TradeMap.Presence.Cancel"),ReviewEdit);MapWidget(TEXT("TradeMap.Presence.Source"),SourcePicker);
 MapWidget(TEXT("TradeMap.Station.Storage"),Storage);MapWidget(TEXT("TradeMap.Station.BuildingName"),SiteName);MapWidget(TEXT("TradeMap.Station.Footer"),Footer);
}
bool STradeStationDetails::ToggleTerms(){bLeaseOpen=!bLeaseOpen;Refresh();return true;}
}
#undef LOCTEXT_NAMESPACE
