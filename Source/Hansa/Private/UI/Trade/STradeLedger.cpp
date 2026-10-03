#include "HansaTradeWorkspaceComponents.h"
#include "UI/SHansaReferenceFrame.h"
#include "UI/HansaUiStyle.h"
#include "TradeArtwork.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateColorBrush.h"
#define LOCTEXT_NAMESPACE "STradeLedger"
namespace Hansa::UI {
namespace { const float Widths[]={200,100,110,110,110,110,130,100,120,120,320};
FText Q(int64 V){return FText::AsNumber(double(V)/1000.);}
FString RowId(FName G){return TEXT("TradeMap.Ledger.Good.")+G.ToString();}
}
void STradeLedger::Construct(const FArguments&,const TSharedRef<FTradeComponentContext>& C){
 Initialize(C);LedgerListStyle.SetBackgroundBrush(FSlateColorBrush(FLinearColor::Transparent));
 auto Bar=SNew(SWrapBox).UseAllottedSize(true);
 const FText TabLabels[]={LOCTEXT("StockTab","Stock"),LOCTEXT("OperationsTab","Operations"),LOCTEXT("DetailTab","Detail")};int32 TabIndex=0;
 for(const TCHAR* T:{TEXT("Stock"),TEXT("Operations"),TEXT("Detail")}){auto B=MakeControl(*(FString(TEXT("TradeMap.Ledger."))+T),TabLabels[TabIndex++],EHansaUiButtonStyle::Secondary);Tabs.Add(T,B);Bar->AddSlot().Padding(0,0,4,0)[B];}
 Bar->AddSlot()[SAssignNew(FilterButton,SHansaAction).Preferences(Preferences).Typography(EHansaUiTypographyToken::SerifBody).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Ledger.Filter")))];MapWidget(TEXT("TradeMap.Ledger.Filter"),FilterButton);
 ColumnsButton=MakeControl(TEXT("TradeMap.Ledger.Columns"),LOCTEXT("Columns","Columns"),EHansaUiButtonStyle::Secondary);Bar->AddSlot()[ColumnsButton.ToSharedRef()];
 Bar->AddSlot()[MakeControl(TEXT("TradeMap.Ledger.Back"),LOCTEXT("Back","Back"),EHansaUiButtonStyle::Secondary)];
 auto Header=SNew(SHorizontalBox);const FText Names[]={LOCTEXT("GoodColumn","Good / units"),LOCTEXT("PhysicalColumn","Physical"),LOCTEXT("ReservedColumn","Hard reserved"),LOCTEXT("OrderColumn","For orders*"),LOCTEXT("RouteColumn","For routes*"),LOCTEXT("AvailableColumn","Available"),LOCTEXT("ReserveColumn","Desired reserve"),LOCTEXT("ShareColumn","Capacity share"),LOCTEXT("IncomingColumn","Incoming*"),LOCTEXT("OutgoingColumn","Outgoing*"),LOCTEXT("ResultColumn","Last retained result")};
 for(int32 I=0;I<UE_ARRAY_COUNT(Widths);++I)Header->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(Widths[I]).Padding(0,0,8,0)[SNew(STextBlock).Text(Names[I]).TextStyle(&LightBodyStyle).AutoWrapText(true)]];
 SAssignNew(StockPanel,SVerticalBox)
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Horizontal,SScrollBox).Orientation(Orient_Horizontal)+SScrollBox::Slot()[SNew(SBox).WidthOverride(1630)[SNew(SVerticalBox)
  +SVerticalBox::Slot().AutoHeight().Padding(8,4)[Header]
  +SVerticalBox::Slot().FillHeight(1)[SAssignNew(List,SListView<TSharedPtr<FTradeLedgerItem>>).ListViewStyle(&LedgerListStyle).ListItemsSource(&Rows).OnGenerateRow(this,&STradeLedger::MakeRow).SelectionMode(ESelectionMode::None).ItemHeight(56)]]]]
 +SVerticalBox::Slot().AutoHeight()[SAssignNew(Empty,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)];
 SAssignNew(DetailsPanel,SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(OrdersButton,SHansaAction).Preferences(Preferences).Label(LOCTEXT("Orders","Open orders")).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Ledger.Orders"))) ]
  +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(RouteButton,SHansaAction).Preferences(Preferences).Label(LOCTEXT("Route","Open route")).OnClicked(this,&STradeComponent::Invoke,FString(TEXT("TradeMap.Ledger.Route"))) ]]
 +SVerticalBox::Slot().FillHeight(1)[SAssignNew(DetailsScroll,SScrollBox)+SScrollBox::Slot()[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SAssignNew(OperationCards,SVerticalBox)]+SVerticalBox::Slot().AutoHeight()[SAssignNew(Detail,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true).WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)]]];
 const TCHAR* CardNames[]={TEXT("Storage / units"),TEXT("Upkeep / arrears"),TEXT("Factor / orders"),TEXT("Route handling"),TEXT("Highest blocker / next step"),TEXT("Preserved assets")};
 for(const TCHAR* Name:CardNames){TSharedPtr<STextBlock> Value;OperationCards->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBorder).BorderImage(&WorkingBrush).Padding(8)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(Name)).TextStyle(&LightHeadingStyle)]+SVerticalBox::Slot().AutoHeight()[SAssignNew(Value,STextBlock).TextStyle(&LightBodyStyle).AutoWrapText(true)]]];CardValues.Add(Value);}
 auto Ownership=SNew(SWrapBox).UseAllottedSize(true);const EUiGlyph Glyphs[]={EUiGlyph::Warehouse,EUiGlyph::Market,EUiGlyph::Ship,EUiGlyph::Civic};const TCHAR* Owners[]={TEXT("Your station"),TEXT("City market"),TEXT("Ship cargo"),TEXT("Home-city stores")};for(int32 I=0;I<4;++I)Ownership->AddSlot().Padding(4)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(GetGeneratedIconBrush(Glyphs[I],24))]]+SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(STextBlock).Text(FText::FromString(Owners[I])).TextStyle(&LightBodyStyle)]];OperationCards->AddSlot().AutoHeight()[Ownership];
 MapWidget(TEXT("TradeMap.Ledger.Orders"),OrdersButton);MapWidget(TEXT("TradeMap.Ledger.Route"),RouteButton);MapWidget(TEXT("TradeMap.Ledger.DetailText"),Detail);
 ChildSlot[SNew(SHansaReferenceFrame).Dark(false).Surface(Preferences.bHighContrast?nullptr:TradeArtwork(TEXT("Linen"),true)).Padding(8)[SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().AutoWidth().Padding(0,0,8,0)[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(GetGeneratedIconBrush(EUiGlyph::Warehouse,24))]]
  +SHorizontalBox::Slot().FillWidth(1)[SAssignNew(Title,STextBlock).TextStyle(&LightHeadingStyle).AutoWrapText(true)]]
 +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Bar]
 +SVerticalBox::Slot().FillHeight(1)[SNew(SOverlay)+SOverlay::Slot()[StockPanel.ToSharedRef()]+SOverlay::Slot()[DetailsPanel.ToSharedRef()]]]];
 MapWidget(TEXT("TradeMap.Ledger.Title"),Title);MapWidget(TEXT("TradeMap.Ledger.Table"),List);
}
TArray<FText> STradeLedger::Cells(const FHansaTradeLedgerRow& R)const {return {R.Label,Q(R.Physical),Q(R.Reserved),LOCTEXT("Unattributed","Unknown"),LOCTEXT("Unattributed","Unknown"),Q(R.Available),R.bHasReserve?Q(R.DesiredReserve):LOCTEXT("Unset","Not set"),R.CapacityShare,LOCTEXT("Unavailable","Unavailable"),LOCTEXT("Unavailable","Unavailable"),R.LastResult};}
TSharedRef<ITableRow> STradeLedger::MakeRow(TSharedPtr<FTradeLedgerItem> R,const TSharedRef<STableViewBase>& Owner){
 if(!R->Action){R->Action=MakeControl(*RowId(R->Data.Good),R->Data.Label,EHansaUiButtonStyle::Secondary);R->Action->SetButtonStyle(&LightRowStyle);auto Box=SNew(SHorizontalBox);const auto Values=Cells(R->Data);
 for(int32 I=0;I<Values.Num();++I){TSharedPtr<STextBlock> Text;auto Cell=SNew(SHorizontalBox);if(I==0)Cell->AddSlot().AutoWidth().Padding(0,0,8,0)[SNew(SBox).WidthOverride(24).HeightOverride(24)[SNew(SImage).Image(GetGeneratedIconBrush(GlyphForGood(R->Data.Good),24))]];
 Cell->AddSlot().FillWidth(1).VAlign(VAlign_Center)[SAssignNew(Text,STextBlock).Text(Values[I]).TextStyle(&LightBodyStyle).OverflowPolicy(ETextOverflowPolicy::Ellipsis)];R->Cells.Add(Text);Box->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(Widths[I])[Cell]];}
 R->Action->SetContent(SNew(SBox).MinDesiredHeight(48)[Box]);R->Action->SetToolTipText(R->Data.Detail);}
 if(PendingFocus==RowId(R->Data.Good)){PendingFocus.Reset();FSlateApplication::Get().SetKeyboardFocus(R->Action,EFocusCause::Navigation);}
 return SNew(STableRow<TSharedPtr<FTradeLedgerItem>>,Owner).Padding(0)[R->Action.ToSharedRef()];
}
void STradeLedger::Refresh(const FHansaTradeLedger& L){
 if(View.City!=L.City||View.StationId!=L.StationId){SelectedGood=NAME_None;Filter=0;Page=TEXT("Stock");if(List)List->ScrollToTop();}
 View=L;Title->SetText(FText::Format(LOCTEXT("Title","Your station ledger · {0}"),FText::FromString(L.City.ToString().Replace(TEXT("City."),TEXT("")))));Title->SetToolTipText(L.Identity);Rebuild();
}
void STradeLedger::Rebuild(){
 TArray<TSharedPtr<FTradeLedgerItem>> Next;for(const auto& D:View.Rows)if(D.Matches(Filter)){auto* Existing=Rows.FindByPredicate([&](const auto& X){return X->Data.Good==D.Good;});auto R=Existing?*Existing:MakeShared<FTradeLedgerItem>();R->Data=D;if(R->Action){const auto V=Cells(D);for(int32 I=0;I<V.Num();++I)R->Cells[I]->SetText(V[I]);R->Action->SetToolTipText(D.Detail);R->Action->SetState(SelectedGood==D.Good?EUiState::Selected:EUiState::Default);}Next.Add(R);}
 if(Next!=Rows){Rows=MoveTemp(Next);List->RequestListRefresh();}
 const FText Filters[]={LOCTEXT("AllFilter","All stock"),LOCTEXT("ReservedFilter","Reserved / protected"),LOCTEXT("ShortageFilter","Shortage"),LOCTEXT("OverstockFilter","Over acquire target"),LOCTEXT("RouteFilter","Route cargo"),LOCTEXT("OrderFilter","Order cargo")};FilterButton->SetLabel(Filters[Filter]);FilterButton->SetVisibility(Page==TEXT("Stock")?EVisibility::Visible:EVisibility::Collapsed);
 Empty->SetText(!View.bAvailable?View.Blocker:Rows.IsEmpty()?LOCTEXT("Empty","No goods match. Change the filter; overstock requires an explicit acquire target."):FText());Empty->SetVisibility(!View.bAvailable||Rows.IsEmpty()?EVisibility::Visible:EVisibility::Collapsed);
 for(const auto& T:Tabs)T.Value->SetState(T.Key==Page?EUiState::Selected:EUiState::Default);
 StockPanel->SetVisibility(Page==TEXT("Stock")?EVisibility::Visible:EVisibility::Collapsed);DetailsPanel->SetVisibility(Page!=TEXT("Stock")?EVisibility::Visible:EVisibility::Collapsed);
 const auto* R=View.Rows.FindByPredicate([&](const auto& X){return X.Good==SelectedGood;});
 const bool Remote=Model.IsValid()&&!Model->RemoteLedgers.IsEmpty();OrdersButton->SetEnabled(R&&!Remote);RouteButton->SetEnabled(R&&R->RouteId&&!Remote);
 OrdersButton->SetVisibility(Page==TEXT("Detail")?EVisibility::Visible:EVisibility::Collapsed);RouteButton->SetVisibility(Page==TEXT("Detail")?EVisibility::Visible:EVisibility::Collapsed);
 OrdersButton->SetToolTipText(Remote?LOCTEXT("RemoteAction","Remote order editing is unavailable in this scoped ledger. Use an authoritative host session."):LOCTEXT("OrdersHelp","Open the selected good's first live order or start a new order."));RouteButton->SetToolTipText(LOCTEXT("RouteHelp","Open the first related owned route; unavailable when none exists or remote route editing is unavailable."));
 OperationCards->SetVisibility(Page==TEXT("Operations")?EVisibility::Visible:EVisibility::Collapsed);
 ColumnsButton->SetVisibility(Page==TEXT("Stock")?EVisibility::Visible:EVisibility::Collapsed);
 CardValues[0]->SetText(View.bAvailable?FText::Format(LOCTEXT("Capacity","Capacity {0} · used {1} · free {2} · hard reserved {3}"),Q(View.Capacity),Q(View.Used),Q(View.Free),Q(View.Reserved)):LOCTEXT("CapacityUnknown","Unavailable; no zero stock assumed."));
 CardValues[1]->SetText(View.Upkeep);CardValues[2]->SetText(View.Factor);CardValues[3]->SetText(View.Handling);CardValues[4]->SetText(FText::Format(LOCTEXT("BlockerCard","{0} — {1}"),View.Status,View.Blocker));CardValues[5]->SetText(View.Preservation);
 if(Page==TEXT("Operations"))Detail->SetText(FText::Format(LOCTEXT("OwnershipScope","{0}\nThese inventories are separate. This ledger contains only your station stock.\n* Hard reservations have no order/route attribution. Incoming/outgoing commitments are unavailable. Select a good for reserve policy, planned instructions and retained results."),View.Identity));
 else Detail->SetText(R?R->Detail:LOCTEXT("SelectGood","Select a stock row to inspect its reservation causes and related actions."));
}
bool STradeLedger::Intent(const FString& A){
 if(A==TEXT("Back")){Model->SelectSectionIntent(TEXT("Presence"));return true;}
 if(A==TEXT("Columns")){Horizontal->SetScrollOffset(Horizontal->GetScrollOffset()<500?650:Horizontal->GetScrollOffset()<1000?1200:0);return true;}
 if(A==TEXT("Filter")){Filter=(Filter+1)%6;Rebuild();return true;}
 if(A==TEXT("Stock")||A==TEXT("Operations")||A==TEXT("Detail")){Page=A;Rebuild();return true;}
 if(A.StartsWith(TEXT("Good."))){const FName G(*A.RightChop(5));if(!View.Rows.ContainsByPredicate([&](const auto& R){return R.Good==G;}))return false;SelectedGood=G;Page=TEXT("Detail");Rebuild();return true;}
 if(A==TEXT("Orders")||A==TEXT("Route"))return Model->LedgerRelatedIntent(SelectedGood,A==TEXT("Orders"));return false;
}
bool STradeLedger::Reveal(const FString& Id){for(const auto& R:Rows)if(RowId(R->Data.Good)==Id){Page=TEXT("Stock");Rebuild();PendingFocus=Id;List->RequestScrollIntoView(R);if(R->Action){PendingFocus.Reset();FSlateApplication::Get().SetKeyboardFocus(R->Action,EFocusCause::Navigation);}return true;}return false;}
void STradeLedger::Scroll(float Direction){if(Page==TEXT("Stock"))List->AddScrollOffset(Direction*3.f);else DetailsScroll->SetScrollOffset(FMath::Max(0.f,DetailsScroll->GetScrollOffset()+Direction*FMath::Max(48.f,float(DetailsScroll->GetCachedGeometry().GetLocalSize().Y)*.8f)));}
FText STradeLedger::AccessibleDetail() const {FString S=Detail->GetText().ToString();if(Page==TEXT("Operations"))for(const auto& C:CardValues)S+=TEXT("\n")+C->GetText().ToString();return FText::FromString(S);}
TArray<FString> STradeLedger::FocusOrder()const {TArray<FString> R={TEXT("TradeMap.Ledger.Stock"),TEXT("TradeMap.Ledger.Operations"),TEXT("TradeMap.Ledger.Detail"),TEXT("TradeMap.Ledger.Back")};if(Page==TEXT("Stock")){R.Add(TEXT("TradeMap.Ledger.Filter"));R.Add(TEXT("TradeMap.Ledger.Columns"));for(const auto& X:Rows)R.Add(RowId(X->Data.Good));}else if(Page==TEXT("Detail")){if(OrdersButton->IsEnabled())R.Add(TEXT("TradeMap.Ledger.Orders"));if(RouteButton->IsEnabled())R.Add(TEXT("TradeMap.Ledger.Route"));}return R;}
}
#undef LOCTEXT_NAMESPACE
