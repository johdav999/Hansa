#include "TradeArtwork.h"
#include "HansaTradeWorkspaceComponents.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaUiStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#define LOCTEXT_NAMESPACE "HansaTradeScheduleWidget"
namespace Hansa::UI {
class STradeJourneyTimeline final : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(STradeJourneyTimeline){} SLATE_END_ARGS()
 void Construct(const FArguments&){}
 void SetJourney(const FHansaTradeSchedulePresentation& S,const FUiPreferences& P){View=S;Preferences=P;Invalidate(EInvalidateWidgetReason::Paint);}
 FVector2D ComputeDesiredSize(float) const override{return {440,96};}
 int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool) const override{
  const float Left=54,Right=FMath::Max(Left,float(G.GetLocalSize().X)-54),Y=30,Now=FMath::Lerp(Left,Right,View.Progress.Get(0.f));
  const auto Ink=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy);
  auto Line=[&](TArray<FVector2D> Points,float Width){FSlateDrawElement::MakeLines(Out,L,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Ink,true,Width);};
  if(View.Progress.IsSet())Line({{Left,Y},{Now,Y}},3);
  for(float X=Now;X<Right;X+=7)Line({{X,Y},{FMath::Min(Right,X+3),Y}},1.5f);
  for(float X:{Left,(Left+Right)*.5f,Right})FSlateDrawElement::MakeBox(Out,L+1,G.ToPaintGeometry(FVector2f(14,14),FSlateLayoutTransform(FVector2f(X-7,Y-7))),TradeIconArtwork(X==Right?TEXT("CityMarkerSelected"):TEXT("CityMarker"),FMath::CeilToInt(14*G.GetAccumulatedLayoutTransform().GetScale())));
  FSlateDrawElement::MakeBox(Out,L+2,G.ToPaintGeometry(FVector2f(26,26),FSlateLayoutTransform(FVector2f(Now-13,0))),GetGeneratedIconBrush(EUiGlyph::Ship,32));
  const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
  auto Text=[&](const FText& T,float X,float Top,EHansaUiTypographyToken Token){const auto Font=GetComponentFont(Token,Preferences);const auto S=Measure->Measure(T,Font);FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2f(S),FSlateLayoutTransform(FVector2f(X-S.X*.5,Top))),T,Font,ESlateDrawEffect::None,Ink);};
  Text(View.DepartureCity,Left,43,EHansaUiTypographyToken::Data);Text(View.TravelStatus,(Left+Right)*.5f,43,EHansaUiTypographyToken::Data);Text(View.ArrivalCity,Right,43,EHansaUiTypographyToken::Data);
  Text(View.DepartureDetail,Left,64,EHansaUiTypographyToken::SerifBody);Text(View.ArrivalDetail,Right,64,EHansaUiTypographyToken::SerifBody);
  if(View.ArrivalTick.IsSet())Text(FText::Format(LOCTEXT("TimeRemaining","{0} ticks remaining"),FText::AsNumber(FMath::Max(int64(0),View.ArrivalTick.GetValue()-View.Tick))),(Left+Right)*.5f,64,EHansaUiTypographyToken::SerifBody);
  return L+3;
 }
private:FHansaTradeSchedulePresentation View;FUiPreferences Preferences;
};
namespace {
const float ColumnWidths[]={.15f,.14f,.085f,.085f,.085f,.085f,.11f,.26f};
FText CompactScheduleRow(const FHansaTradeScheduleRow& D){
 if(D.Group!=TEXT("Cargo")||D.ActionIndex==INDEX_NONE)return FText::Format(LOCTEXT("Row","{0}\n{1}"),D.Label,D.Detail);
 auto Q=[](TOptional<int64> V){return V.IsSet()?FText::AsNumber(double(V.GetValue())/1000.):LOCTEXT("UnknownShort","unknown");};
 return FText::Format(LOCTEXT("CompactCargo","{0} · {1} at {2}\nRequested {3} · aboard {4} · prepared {5} · reserve {6}\nTo {7} · {8}"),D.GoodLabel,D.ActionLabel,D.ActionCity,Q(D.Requested),Q(D.Carried),Q(D.Prepared),Q(D.Reserve),D.Destination,D.TransferSummary);
}
TArray<FText> ScheduleColumns(const FHansaTradeScheduleRow& D){
 auto Q=[](TOptional<int64> V){return V.IsSet()?FText::AsNumber(double(V.GetValue())/1000.):LOCTEXT("Unknown","Unavailable");};
 if(D.ActionIndex==INDEX_NONE)return {D.GoodLabel.IsEmpty()?D.Label:D.GoodLabel,LOCTEXT("UnplannedCargo","No planned instruction"),LOCTEXT("NotPlanned","Not planned"),Q(D.Carried),Q(D.Prepared),LOCTEXT("NotPlanned","Not planned"),LOCTEXT("NoDestination","No destination"),LOCTEXT("PlanRecovery","Add an unload stop")};
 const FText Plan=D.ActionCity.IsEmpty()?D.ActionLabel:FText::Format(LOCTEXT("PlanAtPort","{0}\nAt {1}"),D.ActionLabel,D.ActionCity);
 const FText Result=D.TransferTick<0?LOCTEXT("NoTransferYet","No retained transfer"):FText::Format(LOCTEXT("ReceiptResult","{0} ({1}/{2})\nRecorded at tick {3}{4}"),D.bWarning?(D.Applied>0?LOCTEXT("Partial","Partial"):LOCTEXT("Missed","Missed")):LOCTEXT("Loaded","Completed"),Q(D.Applied),Q(D.TransferRequested),FText::AsNumber(D.TransferTick),D.bWarning?LOCTEXT("InspectCause"," · inspect source"):FText());
 return {D.GoodLabel.IsEmpty()?D.Label:D.GoodLabel,Plan,Q(D.Requested),Q(D.Carried),Q(D.Prepared),Q(D.Reserve),D.Destination,Result};
}
const FSlateBrush* CargoArt(FName Id){return Id==TEXT("Good.Grain")?TradeIconArtwork(TEXT("GrainSack"),80):GetGeneratedIconBrush(GlyphForGood(Id),56);}
}
void STradeSchedule::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& In) {
 Initialize(In);TradeArtwork(TEXT("GrainSack"));TradeArtwork(TEXT("CityMarker"));TradeArtwork(TEXT("CityMarkerSelected"));
 const auto Linen=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Linen),Parchment=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Parchment),Navy=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::BalticNavy),Ink=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Ink),Chalk=UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::Chalk);
 const auto Selection=FMath::Lerp(Linen,UHansaUiStyleLibrary::GetColor(EHansaUiColorToken::FrostBlue),.45f);
 ManifestRowStyle=LightRowStyle;ManifestRowStyle.SetNormal(FSlateRoundedBoxBrush(Linen,2.f,Parchment,1.f)).SetHovered(FSlateRoundedBoxBrush(Selection,2.f,Navy,1.f)).SetPressed(FSlateRoundedBoxBrush(Selection,2.f,Navy,1.f)).SetNormalPadding(FMargin(8,8)).SetPressedPadding(FMargin(8,8));
 ManifestSelectedStyle=ManifestRowStyle;ManifestSelectedStyle.SetNormal(FSlateRoundedBoxBrush(Selection,2.f,Navy,2.f));
 ManifestTabStyle=SecondaryButtonStyle;ManifestTabStyle.SetNormalPadding(FMargin(20,6)).SetPressedPadding(FMargin(20,6));
 ManifestSelectedTabStyle=ManifestTabStyle;ManifestSelectedTabStyle.SetNormal(FSlateRoundedBoxBrush(Navy,3.f,Parchment,1.f)).SetHovered(FSlateRoundedBoxBrush(Navy,3.f,Parchment,1.f)).SetPressed(FSlateRoundedBoxBrush(Navy,3.f,Parchment,1.f)).SetNormalForeground(Chalk).SetHoveredForeground(Chalk).SetPressedForeground(Chalk);
 ManifestGoodStyle=LightBodyStyle;ManifestGoodStyle.SetFont(GetComponentFont(EHansaUiTypographyToken::Data,Preferences));
 ScheduleListStyle.SetBackgroundBrush(FSlateColorBrush(FLinearColor::Transparent));
 auto Tabs=SNew(SHorizontalBox);
 for(const auto& Pair:TArray<TPair<FString,FText>>{{TEXT("Stops"),LOCTEXT("Stops","Stops")},{TEXT("Cargo"),LOCTEXT("Cargo","Cargo")},{TEXT("Arrivals"),LOCTEXT("Arrivals","Arrivals")}}){
  auto B=MakeControl(*(TEXT("TradeMap.Schedule.Tab.")+Pair.Key),Pair.Value,EHansaUiButtonStyle::Secondary);TabButtons.Add(Pair.Key,B);Tabs->AddSlot().AutoWidth().Padding(0,0,4,0)[SNew(SBox).MinDesiredWidth(120).MinDesiredHeight(48)[B]];
 }
 auto Header=SNew(SHorizontalBox);
 const TArray<FText> Labels={LOCTEXT("Good","Good"),LOCTEXT("Action","Planned load / unload"),LOCTEXT("Requested","Requested\n(units)"),LOCTEXT("Carried","Carried\n(units)"),LOCTEXT("Prepared","Prepared\n(units)"),LOCTEXT("Reserve","Reserve\n(units)"),LOCTEXT("Destination","Destination"),LOCTEXT("Result","Last result")};
 for(int32 I=0;I<Labels.Num();++I)Header->AddSlot().FillWidth(ColumnWidths[I]).Padding(4,6)[SNew(STextBlock).TextStyle(&LightBodyStyle).Text(Labels[I]).AutoWrapText(true)];
 TableHeader=SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(Parchment.CopyWithNewOpacity(.35f)).Padding(FMargin(8,0))[Header];
 OpenJourney=MakeControl(TEXT("TradeMap.Schedule.OpenJourney"),LOCTEXT("OpenJourney","Open journey"),EHansaUiButtonStyle::Secondary);
 OpenJourney->SetContent(SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(GetGeneratedIconBrush(EUiGlyph::Map,24))]]+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("OpenJourney","Open journey")).TextStyle(&LightBodyStyle)]);
 ChildSlot[SAssignNew(ScheduleBounds,SBox).HeightOverride(360)[SNew(SHansaReferenceFrame).Dark(false).Surface(Preferences.bHighContrast?nullptr:TradeArtwork(TEXT("Linen"),true)).Padding(FMargin(18,10)).HAlign(HAlign_Center)[SNew(SBox).WidthOverride(1600)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,14,0)[SNew(SBox).WidthOverride(56).HeightOverride(64)[SNew(SImage).Image(GetGeneratedIconBrush(EUiGlyph::Ship,64))]]
  +SHorizontalBox::Slot().FillWidth(.42f).VAlign(VAlign_Center)[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight()[SAssignNew(JourneyHeading,STextBlock).TextStyle(&LightHeadingStyle).Font(GetComponentFont(EHansaUiTypographyToken::Heading1,Preferences)).AutoWrapText(true)]
   +SVerticalBox::Slot().AutoHeight().Padding(0,4)[SAssignNew(JourneyIdentity,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]
  +SHorizontalBox::Slot().FillWidth(.58f).Padding(12,0)[SAssignNew(JourneyProgress,STradeJourneyTimeline)]
  +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12,0,0,0)[OpenJourney.ToSharedRef()]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SAssignNew(ScheduleText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight()[Tabs]
 +SVerticalBox::Slot().AutoHeight().Padding(0,8)[SAssignNew(EmptyText,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]
 +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[SAssignNew(ChooseOwned,SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).Kind(EHansaUiButtonStyle::Icon).Label(LOCTEXT("ChooseOwned","Select your vessel")).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Schedule.ChooseOwned")))]
 +SVerticalBox::Slot().AutoHeight()[TableHeader.ToSharedRef()]
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(List,SListView<TSharedPtr<FTradeScheduleListRow>>).ListViewStyle(&ScheduleListStyle).ListItemsSource(&Rows).SelectionMode(ESelectionMode::None)
  .OnGenerateRow_Lambda([this](TSharedPtr<FTradeScheduleListRow> Row,const TSharedRef<STableViewBase>& Owner){
   SAssignNew(Row->Text,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).Text(CompactScheduleRow(Row->Data));
   Row->Action=MakeControl(*Row->Data.Id,Row->Data.Label,EHansaUiButtonStyle::Secondary);Row->Action->SetButtonStyle(Row->Data.Id==SelectedRow?&ManifestSelectedStyle:&ManifestRowStyle);
   Row->Action->SetFocusHandler(FSimpleDelegate::CreateLambda([this,Id=Row->Data.Id]{SelectedRow=Id;if(auto* P=Model.Get())P->SelectScheduleRowIntent(Id);}));
   Row->Action->SetToolTipText(FText::Format(LOCTEXT("Tooltip","{0}\n{1}"),Row->Data.Detail,Row->Data.Evidence));
   auto Content=SNew(SHorizontalBox);
   if((Group!=TEXT("Cargo")||bCompactSchedule)&&!Row->Data.GoodId.IsNone())Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[SNew(SBox).WidthOverride(48).HeightOverride(48)[SNew(SImage).Image(CargoArt(Row->Data.GoodId))]];
   if(Group==TEXT("Cargo")&&!bCompactSchedule){
    const auto Values=ScheduleColumns(Row->Data);Row->Columns.Reset();auto Table=SNew(SHorizontalBox);
    for(int32 I=0;I<Values.Num();++I){TSharedPtr<STextBlock> Cell;auto Column=SNew(SHorizontalBox);
     if(I==0&&!Row->Data.GoodId.IsNone())Column->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[SNew(SBox).WidthOverride(48).HeightOverride(48)[SNew(SImage).Image(CargoArt(Row->Data.GoodId))]];
     if(I==7)Column->AddSlot().AutoWidth().VAlign(VAlign_Top).Padding(0,2,8,0)[SNew(SBox).WidthOverride(20).HeightOverride(20)[SAssignNew(Row->StatusIcon,SImage).Image(GetGeneratedIconBrush(Row->Data.bWarning?EUiGlyph::Warning:EUiGlyph::Check,24)).Visibility(Row->Data.TransferTick>=0?EVisibility::Visible:EVisibility::Hidden)]];
     Column->AddSlot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(Cell,STextBlock).TextStyle(I==0?&ManifestGoodStyle:&LightBodyStyle).Text(Values[I]).AutoWrapText(true)];
     Table->AddSlot().FillWidth(ColumnWidths[I]).Padding(4,2).VAlign(VAlign_Center)[Column];Row->Columns.Add(Cell);
    }
    Content->AddSlot().FillWidth(1)[Table];
   }else Content->AddSlot().FillWidth(1)[Row->Text.ToSharedRef()];
   Row->Action->SetContent(SNew(SBox).MinDesiredHeight(52)[Content]);
   return SNew(STableRow<TSharedPtr<FTradeScheduleListRow>>,Owner).Padding(FMargin(0,1))[Row->Action.ToSharedRef()];
  }).OnItemScrolledIntoView_Lambda([this](TSharedPtr<FTradeScheduleListRow> Row,const TSharedPtr<ITableRow>&){if(PendingFocus==Row->Data.Id&&Row->Action){PendingFocus.Reset();FSlateApplication::Get().SetKeyboardFocus(Row->Action,EFocusCause::Navigation);}})
 ]]]]];
 MapWidget(TEXT("TradeMap.Schedule.OpenJourney"),OpenJourney);MapWidget(TEXT("TradeMap.Schedule.ChooseOwned"),ChooseOwned);MapWidget(TEXT("TradeMap.Schedule.Context"),ScheduleText);MapWidget(TEXT("TradeMap.Schedule.Progress"),JourneyProgress);
}
void STradeSchedule::SetCompact(bool Compact){ScheduleBounds->SetHeightOverride(Compact?FOptionalSize():FOptionalSize(bOrdersPreview?285:360));if(bCompactSchedule!=Compact){bCompactSchedule=Compact;List->RebuildList();Refresh(View);}}
void STradeSchedule::SetOrdersPreview(bool bOrders){if(bOrdersPreview==bOrders)return;bOrdersPreview=bOrders;ScheduleBounds->SetHeightOverride(bCompactSchedule?FOptionalSize():FOptionalSize(bOrdersPreview?285:360));}
void STradeSchedule::Refresh(const FHansaTradeSchedulePresentation& S){
 View=S;JourneyHeading->SetText(S.Heading.IsEmpty()?LOCTEXT("Journey","Voyage and cargo"):S.Heading);JourneyIdentity->SetText(S.Identity);JourneyIdentity->SetVisibility(S.Identity.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
 ChooseOwned->SetVisibility(S.Rows.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed);OpenJourney->SetVisibility(S.RouteId?EVisibility::Visible:EVisibility::Collapsed);
 TableHeader->SetVisibility(Group==TEXT("Cargo")&&!bCompactSchedule&&!S.Rows.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed);
 ScheduleText->SetText(S.Context);ScheduleText->SetToolTipText(S.Evidence);ScheduleText->SetVisibility(bCompactSchedule||S.RouteId==0?EVisibility::Visible:EVisibility::Collapsed);
 JourneyProgress->SetJourney(S,Preferences);JourneyProgress->SetVisibility(S.RouteId&&!bCompactSchedule?EVisibility::Visible:EVisibility::Collapsed);
 TArray<TSharedPtr<FTradeScheduleListRow>> Next;
 if(Model.IsValid()&&S.Rows.ContainsByPredicate([&](const auto& R){return R.Id==Model->GetSnapshot().FocusedSemanticId.ToString();}))SelectedRow=Model->GetSnapshot().FocusedSemanticId.ToString();
 if(!S.Rows.ContainsByPredicate([&](const auto& R){return R.Group==Group&&R.Id==SelectedRow;})){const auto* First=S.Rows.FindByPredicate([&](const auto& R){return R.Group==Group;});SelectedRow=First?First->Id:FString();}
 for(const auto& D:S.Rows)if(D.Group==Group){
  auto* Existing=Rows.FindByPredicate([&](const auto& R){return R->Data.Id==D.Id;});auto Row=Existing?*Existing:MakeShared<FTradeScheduleListRow>();Row->Data=D;
  const auto Values=ScheduleColumns(D);for(int32 I=0;I<Row->Columns.Num()&&I<Values.Num();++I)Row->Columns[I]->SetText(Values[I]);
  if(Row->Text)Row->Text->SetText(CompactScheduleRow(D));
  if(Row->StatusIcon){Row->StatusIcon->SetImage(GetGeneratedIconBrush(D.bWarning?EUiGlyph::Warning:EUiGlyph::Check,24));Row->StatusIcon->SetVisibility(D.TransferTick>=0?EVisibility::Visible:EVisibility::Hidden);}
  if(Row->Action){Row->Action->SetButtonStyle(D.Id==SelectedRow?&ManifestSelectedStyle:&ManifestRowStyle);Row->Action->SetToolTipText(FText::Format(LOCTEXT("Tooltip","{0}\n{1}"),D.Detail,D.Evidence));}Next.Add(Row);
 }
 bool Changed=Rows.Num()!=Next.Num();if(!Changed)for(int32 I=0;I<Rows.Num();++I)Changed|=Rows[I]!=Next[I];Rows=MoveTemp(Next);if(Changed)List->RequestListRefresh();
 List->SetVisibility(Rows.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible);
 for(const auto& B:TabButtons){int32 Count=0;for(const auto& R:S.Rows)if(R.Group==B.Key)++Count;const FText Name=B.Key==TEXT("Stops")?LOCTEXT("Stops","Stops"):B.Key==TEXT("Cargo")?LOCTEXT("Cargo","Cargo"):LOCTEXT("Arrivals","Arrivals");B.Value->SetLabel(FText::Format(LOCTEXT("CountedTab","{0} ({1})"),Name,FText::AsNumber(Count)));B.Value->SetButtonStyle(B.Key==Group?&ManifestSelectedTabStyle:&ManifestTabStyle);B.Value->SetState(EUiState::Default);}
 EmptyText->SetText(Group==TEXT("Arrivals")?LOCTEXT("NoArrivals","No arrival is currently timed. Stopped routes have no guaranteed ETA."):LOCTEXT("NoRows","No authorized entries in this view."));EmptyText->SetVisibility(Rows.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed);
}
bool STradeSchedule::SelectTab(const FString& Tab){if(!TabButtons.Contains(Tab))return false;Offsets.Add(Group,List->GetScrollOffset());Group=Tab;Refresh(View);List->SetScrollOffset(Offsets.FindRef(Group));return true;}
bool STradeSchedule::Reveal(const FString& Id){const auto* Row=Rows.FindByPredicate([&](const auto& X){return X->Data.Id==Id;});if(!Row)return false;SelectedRow=Id;PendingFocus=Id;List->RequestScrollIntoView(*Row);if((*Row)->Action)FSlateApplication::Get().SetKeyboardFocus((*Row)->Action,EFocusCause::Navigation);return true;}
}
#undef LOCTEXT_NAMESPACE

