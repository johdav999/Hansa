#include "HansaTradeWorkspaceComponents.h"
#include "TradeArtwork.h"
#include "UI/HansaUiStyle.h"
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
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HansaCityOverviewReference"
namespace Hansa::UI {
namespace {
const FSlateBrush* CityIllustration(const TCHAR* Name, FVector2D Size) {
 static TMap<FString,TSharedPtr<FSlateDynamicImageBrush>> Images;
 auto& Image=Images.FindOrAdd(Name);
 if(!Image)Image=MakeShared<FSlateDynamicImageBrush>(FName(*(FPaths::ProjectContentDir()/TEXT("Hansa/UI/CityOverview")/(FString(Name)+TEXT(".png")))),Size);
 return Image.Get();
}
TSharedRef<SWidget> Art(const FSlateBrush* Brush,float Width,float Height) {
 return SNew(SBox).WidthOverride(Width).HeightOverride(Height)
 [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(Brush)]];
}
FText Count(int32 Value) { return Value<0?LOCTEXT("Unavailable","Unavailable"):FText::AsNumber(Value); }
}

void STradeContext::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In,const TArray<TSharedRef<SWidget>>& Views) {
 Initialize(In);OrdersView=Views[3];ConstructionView=Views[5];DecisionsView=Views[6];RecoveryView=Views[7];
 const auto Brass=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Brass);
 const auto Linen=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen);
 const auto Parchment=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment);
 QuoteBrush=FSlateRoundedBoxBrush(FLinearColor(Linen.R,Linen.G,Linen.B,.28f),3.f,FLinearColor(Brass.R,Brass.G,Brass.B,.45f),1.f);
 NoticeBrush=FSlateRoundedBoxBrush(FLinearColor(Parchment.R,Parchment.G,Parchment.B,.70f),3.f,Brass,1.f);
 RuleBrush=FSlateColorBrush(FLinearColor(Brass.R,Brass.G,Brass.B,.70f));
 OverviewPrimaryStyle=PrimaryButtonStyle;
 auto Navy=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy)*.65f;Navy.A=1;
 OverviewPrimaryStyle.SetNormal(FSlateRoundedBoxBrush(Navy,2.f,Brass,1.f));
 OverviewCaptionStyle=LightBodyStyle;
 auto CaptionFont=GetComponentFont(EHansaUiTypographyToken::SerifBody,Preferences);
 CaptionFont.Size=GetComponentFont(EHansaUiTypographyToken::Caption,Preferences).Size;
 OverviewCaptionStyle.SetFont(CaptionFont).SetColorAndOpacity(UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::MutedInk));
 OverviewValueStyle=LightHeadingStyle;
 if(Preferences.bLargeText)HeadingStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Heading2,Preferences));
 auto Tabs=SNew(SHorizontalBox);
 for(const auto& Tab:TArray<TPair<FString,FText>>{{TEXT("Overview"),LOCTEXT("Overview","Overview")},{TEXT("Presence"),LOCTEXT("Presence","Presence")},{TEXT("Specialization"),LOCTEXT("Office","Office")}}) {
  auto Button=MakeControl(*(TEXT("TradeMap.Navigate.")+Tab.Key),Tab.Value,EHansaUiButtonStyle::Icon);
  Button->SetButtonStyle(&DarkRowStyle);
  Button->SetUnderlinedSelection(true);
  Tabs->AddSlot().FillWidth(1)[Button];
 }
 SAssignNew(SectionMenuScroll,SScrollBox);
 for(const auto& Tab:TArray<TPair<FString,FText>>{{TEXT("Orders"),LOCTEXT("Orders","Orders")},{TEXT("Ledger"),LOCTEXT("Ledger","Ledger")},{TEXT("Construction"),LOCTEXT("Expansion","Expansion")},{TEXT("Decisions"),LOCTEXT("Decisions","Decisions")},{TEXT("Recovery"),LOCTEXT("Recovery","Recovery")}})
  SectionMenuScroll->AddSlot().Padding(0,2)[MakeControl(*(TEXT("TradeMap.Navigate.")+Tab.Key),Tab.Value,EHansaUiButtonStyle::Secondary)];
 CityNavigation=Pair(TEXT("TradeMap.Selection.PreviousCity"),LOCTEXT("Previous","Previous city"),TEXT("TradeMap.Selection.NextCity"),LOCTEXT("Next","Next city"));
 SectionMenuScroll->AddSlot().Padding(0,8)[CityNavigation.ToSharedRef()];
 Tabs->AddSlot().FillWidth(1)[SAssignNew(SectionMenu,SComboButton).ButtonStyle(&DarkRowStyle).HasDownArrow(false)
  .ButtonContent()[SNew(SBox).MinDesiredHeight(40).VAlign(VAlign_Center)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center)[SAssignNew(MoreLabel,STextBlock).Text(LOCTEXT("More","More")).TextStyle(&DarkBodyStyle)]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SHansaGlyph).Glyph(EUiGlyph::Down).Size(14)]]]
  .MenuContent()[SNew(SBox).WidthOverride(280).MaxDesiredHeight(420)[SectionMenuScroll.ToSharedRef()]]];
 MapWidget(TEXT("TradeMap.City.More"),SectionMenu);
 // World-selected establishments host the same details and orders views, with
 // direct tabs rather than the trade workspace's city navigation menu.
 WorldDetailsTab=MakeControl(TEXT("TradeMap.WorldStation.Tab.Details"),LOCTEXT("WorldDetails","Overview"),EHansaUiButtonStyle::Icon);
 WorldOrdersTab=MakeControl(TEXT("TradeMap.WorldStation.Tab.Orders"),LOCTEXT("Orders","Orders"),EHansaUiButtonStyle::Icon);
 WorldUpgradeTab=MakeControl(TEXT("TradeMap.WorldStation.Tab.Upgrade"),LOCTEXT("Upgrade","Upgrade"),EHansaUiButtonStyle::Icon);
 for(auto Button:{WorldDetailsTab,WorldOrdersTab,WorldUpgradeTab}){
  Button->SetButtonStyle(&DarkRowStyle);Button->SetUnderlinedSelection(true);
  Button->SetContent(SNew(SBox).MinDesiredHeight(40).HAlign(HAlign_Center).VAlign(VAlign_Center)
   [SNew(STextBlock).Text(Button==WorldDetailsTab?LOCTEXT("WorldDetails","Overview"):Button==WorldOrdersTab?LOCTEXT("Orders","Orders"):LOCTEXT("Upgrade","Upgrade")).TextStyle(&DarkBodyStyle)]);
 }
 WorldStationTabs=SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1)[WorldDetailsTab.ToSharedRef()]
  +SHorizontalBox::Slot().FillWidth(1)[WorldOrdersTab.ToSharedRef()]
  +SHorizontalBox::Slot().FillWidth(1)[WorldUpgradeTab.ToSharedRef()];
 CitySectionTabs=Tabs;
 auto HeaderAction=[&](const TCHAR* Id,EUiGlyph Icon,const FText& Hint) {
  auto Button=MakeControl(Id,FText(),EHansaUiButtonStyle::Icon);
  Button->SetButtonStyle(&DarkRowStyle);Button->SetContent(SNew(SBox).WidthOverride(40).HeightOverride(40).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SHansaGlyph).Glyph(Icon).Size(20)]);
  Button->SetToolTipText(Hint);return Button;
 };
 auto MarketAction=MakeControl(TEXT("TradeMap.Overview.Market"),LOCTEXT("MarketAction","Open market / quay trade"),EHansaUiButtonStyle::Primary);
 auto PresenceAction=MakeControl(TEXT("TradeMap.Overview.Presence"),LOCTEXT("PresenceAction","Review presence requirements"),EHansaUiButtonStyle::Secondary);
 if(Preferences.bLargeText)OverviewFooter=SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[MarketAction]
  +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[PresenceAction];
 else OverviewFooter=SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[MarketAction]
  +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,0)[PresenceAction];
 auto CityHeader=SNew(SBorder).BorderImage(&OverlayBrush).Padding(FMargin(10,10,6,4))
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Harbor).Size(36)]
    +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[SAssignNew(SelectedCityText,STextBlock).TextStyle(&HeadingStyle).AutoWrapText(true)]
     +SVerticalBox::Slot().AutoHeight()[SAssignNew(SelectedRouteText,STextBlock).TextStyle(&DarkBodyStyle).AutoWrapText(true).Visibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible)]]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[HeaderAction(TEXT("TradeMap.City.Locate"),EUiGlyph::Pin,LOCTEXT("LocateCity","Locate this city on the trade map"))]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)[HeaderAction(TEXT("TradeMap.City.Close"),EUiGlyph::Close,LOCTEXT("CloseCity","Close trade map"))]];
 auto CityTabs=SNew(SBorder).BorderImage(&OverlayBrush).Padding(0)[SAssignNew(Navigation,SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[CitySectionTabs.ToSharedRef()]
  +SVerticalBox::Slot().AutoHeight()[WorldStationTabs.ToSharedRef()]];
 TSharedRef<SWidget> Identity=Preferences.bLargeText?StaticCastSharedRef<SWidget>(SNew(SUniformWrapPanel).HAlign(HAlign_Fill).NumColumnsOverride_Lambda([this]{return Model.IsValid()&&(Model->GetSnapshot().ActiveSection==TEXT("Presence")||Model->GetSnapshot().ActiveSection==TEXT("Orders"))&&GetCachedGeometry().GetLocalSize().X>=700.f?2:1;})+SUniformWrapPanel::Slot()[CityHeader]+SUniformWrapPanel::Slot()[CityTabs]):StaticCastSharedRef<SWidget>(SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[CityHeader]+SVerticalBox::Slot().AutoHeight()[CityTabs]);
 ChildSlot[SNew(SHansaReferenceFrame).Dark(false).Surface(Preferences.bHighContrast?nullptr:TradeArtwork(TEXT("Linen"),true)).Padding(6)
 [SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[Identity]
  +SVerticalBox::Slot().FillHeight(1)[SNew(SOverlay)
   +SOverlay::Slot().Padding(10)[SAssignNew(RouteEditorScroll,SScrollBox)]
   +SOverlay::Slot().Padding(10)[SAssignNew(PresenceScroll,SScrollBox)+SScrollBox::Slot()[Views[1]]]
   +SOverlay::Slot().Padding(10)[SAssignNew(SpecializationScroll,SScrollBox)+SScrollBox::Slot()[Views[2]]]
   +SOverlay::Slot().Padding(10)[Views[3]]
   +SOverlay::Slot().Padding(10)[Views[5]]
   +SOverlay::Slot().Padding(10)[Views[6]]
   +SOverlay::Slot().Padding(10)[Views[7]]
   +SOverlay::Slot()[SAssignNew(OverviewScroll,SScrollBox).ScrollBarThickness(FVector2D(4,4))+SScrollBox::Slot()[BuildOverview()]]]
  +SVerticalBox::Slot().AutoHeight().Padding(10,4,10,10)[SAssignNew(OverviewPrimary,SBox)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,6)[SNew(SBox).HeightOverride(1)[SNew(SImage).Image(&RuleBrush)]]
   +SVerticalBox::Slot().AutoHeight()[OverviewFooter.ToSharedRef()]]]
  +SVerticalBox::Slot().AutoHeight().Padding(10,4,10,8)[SAssignNew(PresenceFooter,SBox)[Views[8]]]
  +SVerticalBox::Slot().AutoHeight()[Views[4]]]];
 // Separate artwork and native labels inside the real controls; the reference bitmap is never loaded.
 for(const auto& Entry:TArray<TPair<FString,EUiGlyph>>{{TEXT("TradeMap.Overview.Market"),EUiGlyph::Market},{TEXT("TradeMap.Overview.Presence"),EUiGlyph::Save}}) {
  auto Label=SNew(STextBlock).Text(Entry.Key.EndsWith(TEXT("Market"))?LOCTEXT("MarketAction","Open market / quay trade"):LOCTEXT("PresenceAction","Review presence requirements")).TextStyle(Entry.Key.EndsWith(TEXT("Market"))?&DarkBodyStyle:&LightBodyStyle).AutoWrapText(true);
  if(Entry.Key.EndsWith(TEXT("Presence")))PresenceActionLabel=Label;
  // Controls were already registered by MakeControl. Retain their event/focus behavior.
  auto Button=Entry.Key.EndsWith(TEXT("Market"))?MarketAction:PresenceAction;
  if(Entry.Key.EndsWith(TEXT("Market")))Button->SetButtonStyle(&OverviewPrimaryStyle);
  const TSharedRef<SWidget> Icon=Entry.Key.EndsWith(TEXT("Market"))?Art(CityIllustration(TEXT("scales--48"),FVector2D(48)),24,24):StaticCastSharedRef<SWidget>(SNew(SHansaGlyph).Glyph(Entry.Value).Size(24));
  Button->SetContent(SNew(SBox).MinDesiredHeight(28)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[Icon]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Label]]);
 }
 MapWidget(TEXT("TradeMap.City.Panel"),SharedThis(this));
 MapWidget(TEXT("TradeMap.Station.Scroll"),PresenceScroll);
 MapWidget(TEXT("TradeMap.Overview.Scroll"),OverviewScroll);
 MapWidget(TEXT("TradeMap.Overview.ConstructionStatus"),ConstructionStatus);
 MapWidget(TEXT("TradeMap.Overview.Report"),OverviewReport);MapWidget(TEXT("TradeMap.Overview.Access"),OverviewAccess);
}

TSharedRef<SWidget> STradeContext::BuildOverview() {
 auto RefreshAction=MakeControl(TEXT("TradeMap.Overview.Refresh"),LOCTEXT("RefreshReport","Refresh report"),EHansaUiButtonStyle::Secondary);
 RefreshAction->SetToolTipText(LOCTEXT("RefreshReportHelp","Open the authorized market workflow to obtain current information."));
 auto Rule=[this]()->TSharedRef<SWidget>{return SNew(SBox).HeightOverride(1)[SNew(SImage).Image(&RuleBrush)];};
 auto Text=[this](const FText& Value)->TSharedRef<SWidget>{return SNew(STextBlock).Text(Value).TextStyle(&OverviewCaptionStyle).AutoWrapText(true);};
 auto Access=SNew(SVerticalBox);
 const FText Names[]={LOCTEXT("Reports","Market reports"),LOCTEXT("PublicTrade","Public market trade"),LOCTEXT("RoutesAccess","Route access")};
 for(int32 I=0;I<3;++I) {
  if(I)Access->AddSlot().AutoHeight()[Rule()];
  Access->AddSlot().AutoHeight().Padding(4,3)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text(Names[I]).TextStyle(&LightBodyStyle).AutoWrapText(true)]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0)[SAssignNew(AccessIcons[I],SHansaGlyph).Glyph(EUiGlyph::Check).Size(14)]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SAssignNew(AccessValues[I],STextBlock).TextStyle(&LightBodyStyle)]];
 }
 auto Metric=[&](const FText& Label,const FSlateBrush* Icon,TSharedPtr<STextBlock>& Value)->TSharedRef<SWidget>{
  return SNew(SBorder).BorderImage(&QuoteBrush).Padding(6)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[Art(Icon,48,42)]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Text(Label)]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(Value,STextBlock).TextStyle(&OverviewValueStyle).AutoWrapText(true)]]];
 };
 auto Body=SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(CityArt,SBox).HeightOverride_Lambda([this] {return FMath::Max(0.f,GetCachedGeometry().GetLocalSize().X-12.f)*552.f/2031.f;})
  [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SImage).Image(CityIllustration(TEXT("panorama--840"),FVector2D(840,228)))]]]
 +SVerticalBox::Slot().AutoHeight().Padding(10,8,10,0)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight()[SAssignNew(NoticeCard,SBorder).BorderImage(&NoticeBrush).Padding(7)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,6,0)[SNew(SBox).WidthOverride(26).HeightOverride(32)[SAssignNew(ReportIcon,SImage).Image(GetGeneratedIconBrush(EUiGlyph::Loading,40))]]
   +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(WarningTitle,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(WarningDetail,STextBlock).TextStyle(&OverviewCaptionStyle).AutoWrapText(true)]]
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,0,0)[SNew(SBox).WidthOverride(128)[RefreshAction]]]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,2)[SNew(STextBlock).Text(LOCTEXT("MarketReport","Market report")).TextStyle(&LightHeadingStyle)]
  +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(&QuoteBrush).Padding(6)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(.85f).VAlign(VAlign_Center)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,4,0)[SAssignNew(GoodIcon,SHansaGlyph).Glyph(EUiGlyph::Grain).Size(34)]
    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(GoodLabel,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
   +SHorizontalBox::Slot().FillWidth(1).Padding(7,0)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Price","Reported price"))]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(PriceValue,STextBlock).TextStyle(&OverviewValueStyle).AutoWrapText(true)]]
   +SHorizontalBox::Slot().FillWidth(.9f)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Text(LOCTEXT("Stock","Reported stock"))]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(StockValue,STextBlock).TextStyle(&OverviewValueStyle).AutoWrapText(true)]]]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,3).HAlign(HAlign_Center)[SAssignNew(OverviewReport,STextBlock).TextStyle(&OverviewCaptionStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,2)[Rule()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,2,0,4)[SNew(STextBlock).Text(LOCTEXT("Harbour","Harbour & access")).TextStyle(&LightHeadingStyle)]
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Metric(LOCTEXT("YourRoutes","Your routes"),CityIllustration(TEXT("routes--96"),FVector2D(96)),RoutesValue)]
   +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Metric(LOCTEXT("ApproachingShips","Approaching ships"),GetGeneratedIconBrush(EUiGlyph::Ship,80),ShipsValue)]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[SNew(SBorder).BorderImage(&QuoteBrush).Padding(4)[Access]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,3).HAlign(HAlign_Center)[SAssignNew(OverviewAccess,STextBlock).TextStyle(&OverviewCaptionStyle).AutoWrapText(true)]
  +SVerticalBox::Slot().AutoHeight().Padding(0,2)[Rule()]
  +SVerticalBox::Slot().AutoHeight().Padding(0,2,0,4)[SNew(STextBlock).Text(LOCTEXT("YourPresence","Your presence")).TextStyle(&LightHeadingStyle)]
  +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,8,0)[SAssignNew(PresenceArt,SBox)[Art(CityIllustration(TEXT("office--240"),FVector2D(240,160)),96,64)]]
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(LeaseTitle,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,2)[SAssignNew(SiteSummary,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(PresenceHint,STextBlock).TextStyle(&OverviewCaptionStyle).AutoWrapText(true)]]]
  +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(OverviewInfo,STextBlock).TextStyle(&OverviewCaptionStyle).AutoWrapText(true)]
   +SHorizontalBox::Slot().AutoWidth().Padding(6,0)[SNew(SHansaGlyph).Glyph(EUiGlyph::Lock).Size(14)]
   +SHorizontalBox::Slot().FillWidth(.8f)[SAssignNew(ConstructionStatus,STextBlock).TextStyle(&OverviewCaptionStyle).AutoWrapText(true)]]];
 MapWidget(TEXT("TradeMap.Overview.Price"),PriceValue);MapWidget(TEXT("TradeMap.Overview.Stock"),StockValue);
 MapWidget(TEXT("TradeMap.Overview.Routes"),RoutesValue);MapWidget(TEXT("TradeMap.Overview.Ships"),ShipsValue);
 return Body;
}

void STradeContext::RefreshOverview(const FHansaTradeMapCityPresentation* City) {
 const FName CityId=City?City->StableId:NAME_None;
 if(CityId!=DisplayedCity){if(!DisplayedCity.IsNone())CityOverviewOffsets.Add(DisplayedCity,OverviewScroll->GetScrollOffset());DisplayedCity=CityId;OverviewScroll->SetScrollOffset(CityOverviewOffsets.FindRef(CityId));}
 const auto& S=Model->GetSnapshot();const auto& Summary=S.CityInspector;
 // Compact workspaces span the viewport. Omit decoration rather than allowing
 // a width-derived panorama height to displace the actual city information.
 CityArt->SetVisibility(!Preferences.bLargeText&&!S.bCompact&&CityId==TEXT("City.Rostock")?EVisibility::Visible:EVisibility::Collapsed);
 PresenceArt->SetVisibility(Preferences.bLargeText?EVisibility::Collapsed:EVisibility::Visible);
 const bool Unknown=!City||City->bUnknown;
 const bool Stale=City&&City->bStale;
 WarningTitle->SetText(Unknown?LOCTEXT("MissingReport","Market report unavailable"):Stale?LOCTEXT("OldReport","Market report needs attention"):LOCTEXT("CurrentReport","Current market report"));
 WarningDetail->SetText(Unknown?LOCTEXT("ReportRemedy","Visit an accessible market to obtain a report."):FText::Format(LOCTEXT("ReportAge","{0} {0}|plural(one=tick,other=ticks) old · {1}"),FText::AsNumber(City->ReportAgeTicks),Stale?LOCTEXT("RefreshHint","Refresh before committing cargo or money."):LOCTEXT("CurrentHint","Authorized market information.")));
 ReportIcon->SetImage(GetGeneratedIconBrush(Unknown?EUiGlyph::Information:Stale?EUiGlyph::Loading:EUiGlyph::Check,40));
 FString Good=S.PreferredGoodStableId.ToString();Good.RemoveFromStart(TEXT("Good."));
 GoodLabel->SetText(FText::FromString(Good));GoodIcon->SetGlyph(GlyphForGood(S.PreferredGoodStableId));
 PriceValue->SetText(!City||City->ReportedPriceMilliMarks<0?LOCTEXT("Unknown","Unknown"):FText::Format(LOCTEXT("PriceValue","{0} Mark"),FText::AsNumber(double(City->ReportedPriceMilliMarks)/1000.)));
 StockValue->SetText(!City||City->ReportedStockMilliUnits<0?LOCTEXT("Unknown","Unknown"):FText::Format(LOCTEXT("StockValue","{0} units"),FText::AsNumber(double(City->ReportedStockMilliUnits)/1000.)));
 OverviewReport->SetText(Stale?LOCTEXT("Changed","Reported values may have changed."):Unknown?LOCTEXT("UnknownStock","Unknown stock is not zero."):LOCTEXT("AuthorizedQuote","Values from your authorized report."));
 OverviewReport->SetToolTipText(City?City->GoodReport:FText());
 RoutesValue->SetText(Count(Summary.OwnedRouteCount));ShipsValue->SetText(Count(Summary.ApproachingShipCount));
 const int32 Access[]={Summary.ReportsAccess,Summary.PublicTradeAccess,Summary.RoutesAccess};
 for(int32 I=0;I<3;++I){AccessValues[I]->SetText(Access[I]<0?LOCTEXT("Unknown","Unknown"):Access[I]?LOCTEXT("Granted","Granted"):LOCTEXT("Locked","Locked"));AccessValues[I]->SetColorAndOpacity(UHansaUiStyleLibrary::GetColor(Access[I]==1?EHansaUiColorToken::ProsperityTeal:EHansaUiColorToken::MutedInk));AccessIcons[I]->SetGlyph(Access[I]<0?EUiGlyph::Information:Access[I]?EUiGlyph::Check:EUiGlyph::Lock);}
 OverviewAccess->SetText(Summary.State==EHansaTradeCityState::Home?LOCTEXT("HomeTransfers","Local transfers follow ownership rules."):Summary.PublicTradeAccess==1?LOCTEXT("Berth","Bring an owned ship to berth to trade."):LOCTEXT("AccessReview","Review market access before trading."));
 OverviewAccess->SetToolTipText(FText::Format(LOCTEXT("AccessDetails","{0}\n{1}"),Summary.MarketAccess,Summary.Routes));
 LeaseTitle->SetText(Summary.OwnedRouteCount<0?LOCTEXT("PresenceUnknown","Presence unavailable"):Summary.State==EHansaTradeCityState::Home?LOCTEXT("Home","Home city"):Summary.bActiveLease?LOCTEXT("Lease","Active station lease"):LOCTEXT("NoLease","No active station lease"));
 SiteSummary->SetText(Summary.StationSiteCount<0?LOCTEXT("NoSites","No supported station site"):FText::Format(LOCTEXT("Sites","{0} station {0}|plural(one=site,other=sites)"),FText::AsNumber(Summary.StationSiteCount)));
 PresenceHint->SetText(Summary.bActiveLease?S.TradeStationState:Summary.bStationSupported?LOCTEXT("Requirements","Review requirements before funding."):Summary.Overview);
 PresenceHint->SetToolTipText(Summary.Expansion);
 OverviewInfo->SetText(Summary.State==EHansaTradeCityState::Home?Summary.Presence:LOCTEXT("Autonomous","City buildings remain autonomous."));
 OverviewInfo->SetToolTipText(Summary.Issue);
 ConstructionStatus->SetText(City&&City->bBuildable?LOCTEXT("LocalConstruction","Local construction"):Summary.bActiveLease?LOCTEXT("LeaseConstruction","Leased placement only"):LOCTEXT("NoConstruction","Construction unavailable"));
 if(PresenceActionLabel)PresenceActionLabel->SetText(Summary.PrimaryAction);
}
}
#undef LOCTEXT_NAMESPACE
