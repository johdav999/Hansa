#include "UI/HansaTradeMapPresentationModel.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaRuntimeSimulationHost.h"

#define LOCTEXT_NAMESPACE "HansaStationOverview"
using namespace Hansa::Simulation;
namespace {
FText Remedy(EHansaStationOrderBlocker B) {
 switch(B) {
 case EHansaStationOrderBlocker::Funds:return LOCTEXT("Funds","Insufficient treasury funds. Add funds or pause the order.");
 case EHansaStationOrderBlocker::Budget:return LOCTEXT("Budget","Purchase budget exhausted. Review the order budget.");
 case EHansaStationOrderBlocker::MarketStock:return LOCTEXT("MarketStock","Market stock is insufficient. Wait for supply or change the order.");
 case EHansaStationOrderBlocker::StationCapacity:return LOCTEXT("StationCapacity","Station storage is full. Ship goods out or reduce the target.");
 case EHansaStationOrderBlocker::MarketCapacity:return LOCTEXT("MarketCapacity","The market cannot receive more goods. Wait or pause the sale.");
 case EHansaStationOrderBlocker::ReservedStock:return LOCTEXT("ReservedStock","Available stock is protected or reserved. Review reservations and the sale reserve.");
 case EHansaStationOrderBlocker::Access:return LOCTEXT("Access","Trading access is unavailable. Review lease rights and station status.");
 case EHansaStationOrderBlocker::MarketUnavailable:return LOCTEXT("MarketUnavailable","Market information is unavailable. Open the market to review access.");
 case EHansaStationOrderBlocker::PriceLimit:return LOCTEXT("PriceLimit","The price limit is not met. Wait for a suitable price or review the order.");
 case EHansaStationOrderBlocker::Arithmetic:return LOCTEXT("Arithmetic","The order could not settle safely. Review its quantities and budget.");
 default:return LOCTEXT("Review","Review the order and station status before retrying.");
 }
}
}

FHansaStationOverviewPresentation UHansaTradeMapPresentationModel::GetStationOverviewPresentation() const {
 FHansaStationOverviewPresentation V;V.Ledger=GetLedgerPresentation();const auto& E=Snapshot.Establishment;
 const auto& L=V.Ledger;
 V.Storage=L.bAvailable?FText::Format(LOCTEXT("Storage","{0} / {1} units"),FText::AsNumber(double(L.Used)/1000.),FText::AsNumber(double(L.Capacity)/1000.)):LOCTEXT("Unavailable","Unavailable");
 V.StorageFraction=L.bAvailable&&L.Capacity>0?FMath::Clamp(float(L.Used)/float(L.Capacity),0.f,1.f):0.f;
 V.UpkeepState=E.bArrears?E.Blocker:LOCTEXT("NoArrears","Commercial lease · No unpaid upkeep");
 if(E.bPendingUpgrade)V.UpgradeStatus=FText::Format(LOCTEXT("UpgradeStatus","Upgrade in progress · {0}"),Snapshot.PresenceStatus.IsEmpty()?Snapshot.PresenceFundingDetail:Snapshot.PresenceStatus);
 const bool OrdersKnown=!bRemoteEstablishment||RemoteStationOrders.ContainsByPredicate([&](const auto& X){return X.City==Snapshot.SelectedCityStableId&&X.StationId==E.StationId;});
 for(const auto& O:StationOrders) {
  const auto* Last=O.History.IsEmpty()?nullptr:&O.History.Last();
  if(!O.bCancelled) {
   if(O.bPaused)++V.Paused;
   else if(!bStationOrdersWritable||(Last&&(Last->Outcome==EHansaStationOrderOutcome::Blocked||Last->Outcome==EHansaStationOrderOutcome::Suspended||Last->Blocker!=EHansaStationOrderBlocker::None))) {
    ++V.Blocked;
    if(V.Blocker.IsEmpty())V.Blocker=FText::Format(LOCTEXT("OrderWaiting","{0} {1} is waiting\n{2}"),O.Terms.Side==EHansaStationOrderSide::Acquire?LOCTEXT("Buy","Buy"):LOCTEXT("Sell","Sell"),GetOrderGoodLabel(O.Terms.GoodId),!bStationOrdersWritable?LOCTEXT("Rights","An active station and trading rights are required. Review the lease and operating status."):Remedy(Last->Blocker));
   }else if(Last&&Last->Outcome==EHansaStationOrderOutcome::Completed)++V.Completed;
   else ++V.Running;
  }
  // Retained executed receipts only; pending plans and failed attempts are not sales/deliveries.
  for(int32 I=0;I<O.History.Num();++I) {
   const auto& H=O.History[I];if(H.Tick<0||H.AppliedMilliUnits<=0)continue;
   FHansaStationOverviewRow R;R.Id=FString::Printf(TEXT("Order.%llu.%d"),O.Id,I);R.Tick=H.Tick;
   R.Label=FText::Format(LOCTEXT("Ago","{0} ago"),Hansa::UI::PresenceDuration(FMath::Max<int64>(0,E.Tick-H.Tick),E.MinutesPerTick));
   R.Detail=FText::Format(LOCTEXT("Receipt","{0} {1} {2}"),O.Terms.Side==EHansaStationOrderSide::Acquire?LOCTEXT("Bought","Bought"):LOCTEXT("Sold","Sold"),FText::AsNumber(double(H.AppliedMilliUnits)/1000.),GetOrderGoodLabel(O.Terms.GoodId));
   V.Activity.Add(MoveTemp(R));
  }
 }
 V.Trading=OrdersKnown?FText::Format(LOCTEXT("OrderCounts","{0} running · {1} paused · {2} blocked{3}"),FText::AsNumber(V.Running),FText::AsNumber(V.Paused),FText::AsNumber(V.Blocked),V.Completed?FText::Format(LOCTEXT("Completed"," · {0} completed"),FText::AsNumber(V.Completed)):FText()):LOCTEXT("OrdersUnknown","Order status unavailable");
 if(V.Blocker.IsEmpty()&&E.bArrears)V.Blocker=E.Blocker;
 const uint64 Owner=Runtime.IsValid()?Runtime->GetHouseId().GetValue():ViewerHouse.GetValue();
 for(const auto& Route:AllRoutes) {
  if(!Route.bOwnedByPlayer||Route.bCancelled||!Route.CityIds.Contains(Snapshot.SelectedCityStableId))continue;
  FHansaStationOverviewRow R;R.Id=LexToString(Route.RouteValue);R.Label=Route.StopSummary;
  FHansaTradeSchedulePresentation S;
  if(bRemoteEstablishment){if(const auto* X=RemoteWorkspace.Schedules.FindByPredicate([&](const auto& X){return X.RouteId==Route.RouteValue;})){S=*X;S.RestoreReplicatedOptionals();}}
  else if(LastProjection&&LastRegistry)S=Hansa::UI::BuildTradeSchedule(*LastProjection,*LastRegistry,Route.RouteValue,Route.VehicleValue,Owner);
  const bool NextIsStation=S.Rows.ContainsByPredicate([&](const auto& X){return X.Group==TEXT("Stops")&&X.StopIndex==S.NextStop&&X.CityId==Snapshot.SelectedCityStableId;});
  const FText Arrival=NextIsStation&&S.ArrivalTick.IsSet()?FText::Format(LOCTEXT("Arrival","Arrival in {0}"),Hansa::UI::PresenceDuration(FMath::Max<int64>(0,S.ArrivalTick.GetValue()-S.Tick),E.MinutesPerTick)):LOCTEXT("NoArrival","Arrival time unavailable");
  R.Detail=FText::Format(LOCTEXT("Route","{0} #{1} · {2}\n{3}"),Route.bSea?LOCTEXT("Ship","Ship"):LOCTEXT("Vehicle","Vehicle"),FText::AsNumber(Route.VehicleValue),S.TravelStatus.IsEmpty()?Route.State:S.TravelStatus,Arrival);
  V.Transport.Add(MoveTemp(R));
 }
 V.Activity.Sort([](const auto& A,const auto& B){return A.Tick==B.Tick?A.Id<B.Id:A.Tick>B.Tick;});
 if(V.Activity.Num()>3)V.Activity.SetNum(3);
 return V;
}
#undef LOCTEXT_NAMESPACE
