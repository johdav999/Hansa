#include "UI/HansaTradeMapPresentationModel.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Market/HansaMarket.h"
#include "Queries/HansaSimulationReadOnly.h"

#define LOCTEXT_NAMESPACE "HansaOrderReference"
using namespace Hansa::Simulation;

FString FHansaStationOrderEditorPresentation::Key() const {
 FString K=Good.ToString()+FString::Printf(TEXT("|%d|%d|%d|%d|%d|%.6f"),bBuy,bStale,bReportAvailable,bPaused,bCancelled,StockFraction);
 for(const FText* T:{&GoodLabel,&Heading,&State,&Stock,&ModeHint,&TargetLabel,&TargetHint,&RateLabel,&RateHint,&RateUnit,&ReportHeading,&Price,&ReportAge,&Limit,&Summary,&Activity,&FooterHint,&Rights})K+=TEXT("|")+T->ToString();
 return K;
}
void UHansaTradeMapPresentationModel::RefreshStationOrderEditor() {
 auto& V=StationOrderEditor;V={};
 V.Good=FName(*OrderDraft.GoodId.ToString());V.GoodLabel=GetOrderGoodLabel(OrderDraft.GoodId);
 V.bBuy=OrderDraft.Side==EHansaStationOrderSide::Acquire;
 const auto* Selected=StationOrders.FindByPredicate([&](const auto& O){return O.Id==SelectedStationOrder;});
 V.bPaused=Selected&&Selected->bPaused;V.bCancelled=Selected&&Selected->bCancelled;
 V.Heading=Selected?LOCTEXT("Edit","Trading order"):LOCTEXT("New","New trading order");
 V.State=!Selected?LOCTEXT("Draft","Draft"):V.bCancelled?LOCTEXT("Cancelled","Cancelled"):V.bPaused?LOCTEXT("Paused","Paused"):LOCTEXT("Active","Active");
 const FText Target=FText::AsNumber(double(OrderDraft.TargetOrReserveMilliUnits)/1000.);
 V.ModeHint=V.bBuy?LOCTEXT("BuyHint","Buy into your office storage."):LOCTEXT("SellHint","Sell from your office while keeping a reserve.");
 V.TargetLabel=V.bBuy?LOCTEXT("Target","Target stock"):LOCTEXT("Reserve","Keep in reserve");
 V.TargetHint=V.bBuy?LOCTEXT("TargetHint","Buy while stock is below this amount."):LOCTEXT("ReserveHint","Sell only stock above this amount.");
 V.RateLabel=V.bBuy?LOCTEXT("BuyRate","Maximum purchase rate"):LOCTEXT("SellRate","Maximum sale rate");
 const int32 Minutes=LastProjection?LastProjection->GetClock().GetMinutesPerTick():Snapshot.Establishment.MinutesPerTick;
 const FText Interval=Minutes>0?Hansa::UI::PresenceDuration(1,Minutes):LOCTEXT("UnknownInterval","an unknown interval");
 V.RateHint=FText::Format(LOCTEXT("RateHint","Limit each purchase or sale, every {0}."),Interval);
 V.RateUnit=Minutes==60?LOCTEXT("HourUnits","units / hour"):FText::Format(LOCTEXT("IntervalUnits","units / {0}"),Interval);
 const auto L=GetLedgerPresentation();const auto* Stock=L.Rows.FindByPredicate([&](const auto& R){return R.Good==V.Good;});
 V.Stock=L.bAvailable?FText::Format(LOCTEXT("Stock","{0} / {1} units"),FText::AsNumber(double(Stock?Stock->Physical:0)/1000.),Target):LOCTEXT("StockUnknown","Unavailable");
 V.StockFraction=L.bAvailable&&OrderDraft.TargetOrReserveMilliUnits>0?FMath::Clamp(float(Stock?Stock->Physical:0)/float(OrderDraft.TargetOrReserveMilliUnits),0.f,1.f):0.f;
 int64 Price=0,Age=0;
 if(Runtime.IsValid()&&OrderDraft.GoodId.IsValid()){
  const auto City=FHansaCityDefinitionId::TryParse(Snapshot.SelectedCityStableId.ToString());
  if(City){const auto P=Runtime->QueryKnownMarketPrice(City.Value,OrderDraft.GoodId);if(P&&P->PriceMilliMarks.IsSet()&&P->ReportAgeTicks.IsSet()){
   V.bReportAvailable=true;Price=P->PriceMilliMarks.GetValue();Age=P->ReportAgeTicks.GetValue();V.bStale=P->InformationState==EHansaMarketInformationState::Stale;
  }}
 }else if(const auto* M=RemoteMarkets.FindByPredicate([&](const auto& X){return X.CityId==Snapshot.SelectedCityStableId.ToString()&&X.GoodId==OrderDraft.GoodId.ToString()&&X.CurrentPriceMilliMarks>0;})){
  V.bReportAvailable=true;Price=M->CurrentPriceMilliMarks;Age=M->ReportAgeTicks;V.bStale=M->bStale;
 }
 V.ReportHeading=!V.bReportAvailable?LOCTEXT("NoReport","Market report unavailable"):V.bStale?LOCTEXT("OldReport","Older market report"):Age>0?LOCTEXT("RecentReport","Recent market report"):LOCTEXT("CurrentReport","Current market report");
 V.Price=V.bReportAvailable?FText::Format(LOCTEXT("Price","{0} marks / unit"),FText::AsNumber(double(Price)/1000.)):LOCTEXT("Unavailable","Unavailable");
 V.ReportAge=V.bReportAvailable?Hansa::UI::PresenceDuration(Age,Minutes):LOCTEXT("Unavailable","Unavailable");
 // Price-limit authoring is not exposed by the current command UI. Do not imply a working control.
 V.Limit=OrderDraft.LimitUnitPriceMilliMarks>0?FText::Format(LOCTEXT("Limit","Current limit: {0} marks / unit"),FText::AsNumber(double(OrderDraft.LimitUnitPriceMilliMarks)/1000.)):LOCTEXT("LimitHelp","Requires Market specialization; editing unavailable here.");
 V.Summary=V.bBuy?FText::Format(LOCTEXT("BuySummary","Buy up to {0} units of {1}.\nSpend no more than {2} pfennig in total."),Target,V.GoodLabel,FText::AsNumber(OrderDraft.TotalBudgetPfennig)):FText::Format(LOCTEXT("SellSummary","Sell {0} while keeping {1} units in reserve.\nNo purchase budget is required."),V.GoodLabel,Target);
 V.Activity=V.bBuy?LOCTEXT("NoPurchases","No purchases yet\nActivity appears after the order starts."):LOCTEXT("NoSales","No sales yet\nActivity appears after the order starts.");
 if(Selected&&!Selected->History.IsEmpty()){
  const auto& E=Selected->History.Last();const int64 Now=LastProjection?LastProjection->GetClock().GetTick().GetValue():Snapshot.Establishment.Tick;
  V.Activity=FText::Format(LOCTEXT("LastActivity","{0} ago · {1}\n{2} units · {3} pfennig{4}"),Hansa::UI::PresenceDuration(FMath::Max<int64>(0,Now-E.Tick),Minutes),FText::FromString(LexToString(E.Outcome)),FText::AsNumber(double(E.AppliedMilliUnits)/1000.),FText::AsNumber(E.MoneyDelta),E.Blocker==EHansaStationOrderBlocker::None?FText():FText::FromString(FString(TEXT(" · "))+LexToString(E.Blocker)));
 }
 V.FooterHint=Selected?LOCTEXT("SaveHint","Changes apply after you save the order."):LOCTEXT("CreateHint","The factor begins trading after you create the order.");
 V.Rights=bStationOrdersWritable?FText():LOCTEXT("Rights","Saving or resuming needs an active office and trading rights. Existing orders can still be paused or cancelled.");
 Snapshot.StationOrderEditorKey=V.Key();
}
#undef LOCTEXT_NAMESPACE
