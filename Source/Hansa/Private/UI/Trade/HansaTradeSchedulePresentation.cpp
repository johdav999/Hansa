#include "UI/HansaTradeSchedulePresentation.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaRuntimeSimulationHost.h"
#define LOCTEXT_NAMESPACE "HansaTradeSchedule"
using namespace Hansa::Simulation;
namespace {
FText City(FHansaCityDefinitionId Id) { FString N=Id.ToString();N.RemoveFromStart(TEXT("City."));if(N==TEXT("Lubeck"))return LOCTEXT("Lubeck","Lübeck");return FText::FromString(N); }
FText Qty(TOptional<int64> V) {return V.IsSet()?FText::Format(LOCTEXT("Units","{0} units"),FText::AsNumber(double(V.GetValue())/1000.)):LOCTEXT("Unknown","Unavailable");}
TOptional<int64> Stock(const FHansaInventoryProjection* I,FHansaGoodId G,bool Available) {
 if(!I)return {};const auto* S=I->Stocks.FindByPredicate([&](const auto& X){return X.GoodId==G;});return S?(Available?S->Available.GetRawValue():S->Stock.GetRawValue()):0;
}
FText Action(EHansaRouteCargoActionKind K,bool Priced) {
 switch(K){case EHansaRouteCargoActionKind::Load:return Priced?LOCTEXT("Buy","Buy / load"):LOCTEXT("LocalLoad","Load city stock");case EHansaRouteCargoActionKind::Unload:return Priced?LOCTEXT("Sell","Sell / unload"):LOCTEXT("LocalUnload","Unload to city");case EHansaRouteCargoActionKind::StationLoad:return LOCTEXT("StationLoad","Load from station");case EHansaRouteCargoActionKind::StationUnload:return LOCTEXT("StationUnload","Unload to station");case EHansaRouteCargoActionKind::OwnedCityLoad:return LOCTEXT("HomeLoad","Load home stock");default:return LOCTEXT("HomeUnload","Unload to home");}
}
}
FHansaTradeSchedulePresentation Hansa::UI::BuildTradeSchedule(const FHansaSimulationProjection& P,const FHansaEconomicRegistry& Registry,int64 RouteId,int64 VehicleId,uint64 Viewer) {
 FHansaTradeSchedulePresentation S;S.Tick=P.GetClock().GetTick().GetValue();S.Heading=LOCTEXT("Schedule","Journey and cargo");
 const auto* R=P.GetRoutes().FindByPredicate([&](const auto& X){return int64(X.Id.GetValue())==RouteId;});
 const auto* V=P.GetVehicles().FindByPredicate([&](const auto& X){return int64(X.Id.GetValue())==(R?int64(R->VehicleId.GetValue()):VehicleId);});
 if(!Viewer||(R&&R->OwnerId.GetValue()!=Viewer)||(V&&V->OwnerId.GetValue()!=Viewer)){S.Heading=LOCTEXT("PrivateHeading","Private manifest");S.Context=LOCTEXT("Private","Cargo and schedule are private. Select a vehicle owned by your house.");return S;}
 S.RouteId=R?R->Id.GetValue():0;S.VehicleId=V?V->Id.GetValue():0;S.InventoryId=V?V->CargoInventoryId.GetValue():0;
 const auto* Hold=V?P.GetInventories().FindByPredicate([&](const auto& I){return I.Id==V->CargoInventoryId;}):nullptr;
 S.Evidence=FText::Format(LOCTEXT("Evidence","Snapshot tick {0}; route {1}; vehicle {2}; cargo inventory {3}"),FText::AsNumber(S.Tick),FText::AsNumber(S.RouteId),FText::AsNumber(S.VehicleId),FText::AsNumber(S.InventoryId));
 S.Context=LOCTEXT("Unassigned","No route assigned. Create a route to schedule this vehicle.");TSet<FName> Planned;
 if(R){
 S.CurrentStop=R->CurrentStopIndex;S.NextStop=R->NextStopIndex;
 const FText Current=R->Stops.IsValidIndex(S.CurrentStop)?City(R->Stops[S.CurrentStop].CityId):LOCTEXT("UnknownStop","Unknown stop");
 const FText Next=R->Stops.IsValidIndex(S.NextStop)?City(R->Stops[S.NextStop].CityId):LOCTEXT("UnknownStop","Unknown stop");
 S.Heading=FText::Format(LOCTEXT("LegHeading","{0} to {1}"),Current,Next);
 S.DepartureCity=Current;S.ArrivalCity=Next;
 S.Identity=FText::Format(LOCTEXT("VoyageIdentity","Route {0} · {1} #{2}"),FText::AsNumber(S.RouteId),R->Mode==EHansaRouteMode::Sea?LOCTEXT("Ship","Ship"):LOCTEXT("Vehicle","Vehicle"),FText::AsNumber(S.VehicleId));
 S.TravelStatus=R->Lifecycle==EHansaRouteLifecycleState::Traveling?(R->Mode==EHansaRouteMode::Sea?LOCTEXT("AtSea","At sea"):LOCTEXT("EnRoute","En route")):R->Lifecycle==EHansaRouteLifecycleState::Inactive?LOCTEXT("PausedTimeline","Paused"):R->Lifecycle==EHansaRouteLifecycleState::Cancelled?LOCTEXT("CancelledTimeline","Cancelled"):LOCTEXT("AtPort","At port");
 S.DepartureDetail=R->Lifecycle==EHansaRouteLifecycleState::Traveling?FText::Format(LOCTEXT("DepartedTick","Departed\nTick {0}"),FText::AsNumber(S.Tick-FMath::Max(0,R->TotalTravelTicks-R->RemainingTravelTicks))):LOCTEXT("DeparturePending","Departure pending");
 S.ArrivalDetail=R->Lifecycle==EHansaRouteLifecycleState::Traveling?FText::Format(LOCTEXT("ArrivalTick","ETA\nTick {0}"),FText::AsNumber(S.Tick+FMath::Max(0,R->RemainingTravelTicks))):LOCTEXT("ArrivalUnknown","ETA unavailable");
 if(R->Lifecycle==EHansaRouteLifecycleState::Traveling){
  S.ArrivalTick=S.Tick+FMath::Max(0,R->RemainingTravelTicks);if(R->TotalTravelTicks>0)S.Progress=FMath::Clamp(1.f-float(R->RemainingTravelTicks)/R->TotalTravelTicks,0.f,1.f);
  S.Context=FText::Format(LOCTEXT("Travel","{0} to {1} · traveling · ETA {2} ticks (tick {3})"),Current,Next,FText::AsNumber(R->RemainingTravelTicks),FText::AsNumber(S.ArrivalTick.GetValue()));
 }else S.Context=FText::Format(LOCTEXT("Berth","{0} · {1} · next: {2} · ETA unavailable"),Current,R->Lifecycle==EHansaRouteLifecycleState::Cancelled?LOCTEXT("Cancelled","Cancelled"):R->Lifecycle==EHansaRouteLifecycleState::Inactive?LOCTEXT("Paused","Stopped / paused"):LOCTEXT("AtBerth","At berth; cargo actions run on the next simulation step"),Next);
 const auto& Latest=R->LastTransfer;
 FText Phase;
 if(R->Lifecycle!=EHansaRouteLifecycleState::Cancelled&&Latest.Outcome!=EHansaRouteTransferOutcome::None&&Latest.Tick.GetValue()==S.Tick&&Latest.AppliedQuantity.GetRawValue()>0)
  Phase=IsRouteLoad(Latest.Kind)?LOCTEXT("LoadingNow","Loading recorded this tick"):LOCTEXT("UnloadingNow","Unloading recorded this tick");
 else if(R->Lifecycle==EHansaRouteLifecycleState::Traveling&&R->TotalTravelTicks>0)
  Phase=R->RemainingTravelTicks<=FMath::Max(1,R->TotalTravelTicks/4)?LOCTEXT("Arriving","Arriving"):R->Progress.GetPartsPerMillion()<FHansaRate::Scale/4?LOCTEXT("Departing","Departing"):LOCTEXT("Traveling","Traveling");
 if(!Phase.IsEmpty())S.Context=FText::Format(LOCTEXT("Phase","{0} · {1}"),Phase,S.Context);
 for(int32 I=0;I<R->Stops.Num();++I){
  const auto& Stop=R->Stops[I];auto& Row=S.Rows.AddDefaulted_GetRef();Row.Id=FString::Printf(TEXT("TradeMap.Schedule.Stop.%lld.%d"),S.RouteId,I);Row.Group=TEXT("Stops");Row.RouteId=S.RouteId;Row.StopIndex=I;Row.CityId=FName(*Stop.CityId.ToString());
  Row.Label=FText::Format(LOCTEXT("Stop","{0}. {1}"),FText::AsNumber(I+1),City(Stop.CityId));Row.Detail=FText::Format(LOCTEXT("StopDetail","{0} · {1} cargo instructions"),I==S.CurrentStop?LOCTEXT("Current","Current stop / departure"):I==S.NextStop?LOCTEXT("Next","Next stop"):LOCTEXT("Planned","Scheduled stop"),FText::AsNumber(Stop.Actions.Num()));Row.Evidence=S.Evidence;
  for(int32 A=0;A<Stop.Actions.Num();++A){
   const auto& Instr=Stop.Actions[A];auto& C=S.Rows.AddDefaulted_GetRef();C.Id=FString::Printf(TEXT("TradeMap.Schedule.Cargo.%lld.%d.%d"),S.RouteId,I,A);C.Group=TEXT("Cargo");C.RouteId=S.RouteId;C.StopIndex=I;C.ActionIndex=A;C.CityId=FName(*Stop.CityId.ToString());C.GoodId=FName(*Instr.GoodId.ToString());Planned.Add(C.GoodId);
   const auto* Good=Registry.FindGood(Instr.GoodId.ToString());C.Label=FText::Format(LOCTEXT("CargoTitle","{0} · {1} · {2}"),Good?FText::FromString(Good->DisplayName):FText::FromName(C.GoodId),Action(Instr.Kind,Registry.FindCityMarket(Stop.CityId.ToString())&&Registry.FindCityMarket(Stop.CityId.ToString())->bMarketOnly),City(Stop.CityId));
   C.GoodLabel=Good?FText::FromString(Good->DisplayName):FText::FromName(C.GoodId);C.ActionLabel=Action(Instr.Kind,Registry.FindCityMarket(Stop.CityId.ToString())&&Registry.FindCityMarket(Stop.CityId.ToString())->bMarketOnly);
   C.ActionCity=City(Stop.CityId);C.Requested=Instr.QuantityLimit.GetRawValue();C.Reserve=Instr.MinimumSourceReserve.GetRawValue();C.Carried=Stock(Hold,Instr.GoodId,false);
   if(Instr.Kind==EHansaRouteCargoActionKind::StationLoad){const auto* Station=P.GetTradeStations().FindByPredicate([&](const auto& X){return X.Station.OwnerId==R->OwnerId&&X.Station.CityId==Stop.CityId;});if(Station){const auto* Inventory=P.GetInventories().FindByPredicate([&](const auto& X){return X.Id==Station->Station.InventoryId;});C.Prepared=Stock(Inventory,Instr.GoodId,true);}}
   FText Result=LOCTEXT("NoReceipt","No retained transfer for this instruction.");const auto& T=R->LastTransfer;
   if(T.Outcome!=EHansaRouteTransferOutcome::None&&T.StopIndex==I&&T.ActionIndex==A&&T.CityId==Stop.CityId&&T.GoodId==Instr.GoodId&&T.Kind==Instr.Kind){C.Applied=T.AppliedQuantity.GetRawValue();C.TransferRequested=T.RequestedQuantity.GetRawValue();C.TransferTick=T.Tick.GetValue();C.bWarning=T.Outcome!=EHansaRouteTransferOutcome::Completed;
    Result=FText::Format(LOCTEXT("Result","{0}: {1} / {2} at tick {3}. {4}"),T.Outcome==EHansaRouteTransferOutcome::Completed?LOCTEXT("Complete","Completed"):T.Outcome==EHansaRouteTransferOutcome::Partial?LOCTEXT("Partial","Partial"):LOCTEXT("Missed","Missed"),Qty(C.Applied),Qty(T.RequestedQuantity.GetRawValue()),FText::AsNumber(C.TransferTick),C.bWarning?LOCTEXT("UnknownCause","Exact cause was not recorded. Inspect source stock/reserve, capacity, funds and station access before retrying."):LOCTEXT("Settled","Transfer settled."));}
   C.Detail=FText::Format(LOCTEXT("CargoDetail","Requested {0} · carried {1} · prepared at station {2} · protected reserve {3}\nAction port: {4}. {5}"),Qty(C.Requested),Qty(C.Carried),Qty(C.Prepared),Qty(C.Reserve),City(Stop.CityId),Result);
   FText Destination=IsRouteLoad(Instr.Kind)?LOCTEXT("NoDestination","No unload destination planned"):City(Stop.CityId);
   if(IsRouteLoad(Instr.Kind))for(int32 Offset=1;Offset<=R->Stops.Num();++Offset){const auto& Target=R->Stops[(I+Offset)%R->Stops.Num()];if(Target.Actions.ContainsByPredicate([&](const auto& X){return X.GoodId==Instr.GoodId&&!IsRouteLoad(X.Kind);})){Destination=City(Target.CityId);break;}}
   C.Destination=Destination;C.TransferSummary=C.TransferTick<0?LOCTEXT("NoTransfer","No retained transfer"):FText::Format(LOCTEXT("ShortTransfer","{0} · {1} units at tick {2}{3}"),C.bWarning?(C.Applied>0?LOCTEXT("Partial","Partial"):LOCTEXT("Missed","Missed")):LOCTEXT("Complete","Completed"),FText::AsNumber(double(C.Applied)/1000.),FText::AsNumber(C.TransferTick),C.bWarning?LOCTEXT("ShortCause","; cause unrecorded — inspect stock/capacity"):FText());
   C.Detail=FText::Format(LOCTEXT("Destination","Destination: {0}\n{1}"),Destination,C.Detail);
   C.Evidence=FText::Format(LOCTEXT("CargoEvidence","{0}; stop {1}; action {2}; transfer tick {3}. Only the latest transfer is retained; no separate receipt ID is exposed."),S.Evidence,FText::AsNumber(I),FText::AsNumber(A),FText::AsNumber(C.TransferTick));
  }
 }
 if(S.ArrivalTick.IsSet()&&R->Stops.IsValidIndex(S.NextStop)){auto& Row=S.Rows.AddDefaulted_GetRef();Row.Group=TEXT("Arrivals");Row.RouteId=S.RouteId;Row.StopIndex=S.NextStop;Row.CityId=FName(*R->Stops[S.NextStop].CityId.ToString());Row.Id=FString::Printf(TEXT("TradeMap.Schedule.Arrival.%lld.%d"),S.RouteId,S.NextStop);Row.Label=Next;Row.Detail=S.Context;Row.Evidence=S.Evidence;}
 }
 if(Hold)for(const auto& StockRow:Hold->Stocks)if(StockRow.Stock.GetRawValue()>0&&!Planned.Contains(FName(*StockRow.GoodId.ToString()))){auto& Row=S.Rows.AddDefaulted_GetRef();Row.Group=TEXT("Cargo");Row.GoodId=FName(*StockRow.GoodId.ToString());Row.CityId=V?FName(*V->CurrentCityId.ToString()):NAME_None;Row.Id=FString::Printf(TEXT("TradeMap.Schedule.Hold.%lld.%s"),S.VehicleId,*StockRow.GoodId.ToString());const auto* Good=Registry.FindGood(StockRow.GoodId.ToString());Row.Label=Good?FText::FromString(Good->DisplayName):FText::FromName(Row.GoodId);Row.Carried=StockRow.Stock.GetRawValue();Row.Detail=FText::Format(LOCTEXT("Unplanned","Carried {0} · no planned instruction or destination. Add an unload stop to recover this cargo."),Qty(Row.Carried));Row.Evidence=S.Evidence;Row.bWarning=true;}
 return S;
}
FHansaTradeSchedulePresentation UHansaTradeMapPresentationModel::GetSchedulePresentation() const {
 if(Snapshot.bCreating){FHansaTradeSchedulePresentation S;S.Heading=LOCTEXT("Draft","Draft voyage");S.Context=LOCTEXT("DraftInfo","Not executing. Review the route editor, then activate to begin the journey.");return S;}
 if(bRemoteEstablishment){
  if(const auto* V=RemoteWorkspace.Schedules.FindByPredicate([&](const auto& X){return Snapshot.SelectedRouteValue?X.RouteId==Snapshot.SelectedRouteValue:X.VehicleId==Snapshot.SelectedVehicleValue;})){auto S=*V;S.RestoreReplicatedOptionals();return S;}
  FHansaTradeSchedulePresentation S;S.Heading=LOCTEXT("RemotePrivate","Manifest unavailable");S.Context=LOCTEXT("RemotePrivateReason","Select an owned vessel. Rival cargo and exact voyage progress are private.");return S;
 }
 if(!LastProjection||!LastRegistry){FHansaTradeSchedulePresentation S;S.Heading=LOCTEXT("Loading","Loading journey");S.Context=LOCTEXT("Wait","Waiting for authoritative trade data.");return S;}
 const auto House=Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse;return Hansa::UI::BuildTradeSchedule(*LastProjection,*LastRegistry,Snapshot.SelectedRouteValue,Snapshot.SelectedVehicleValue,House.GetValue());
}
bool UHansaTradeMapPresentationModel::SelectScheduleRowIntent(const FString& Id){
 if(IsAnyTradeCommandPending()||!Snapshot.bOpen||Snapshot.bCreating)return false;const auto Schedule=GetSchedulePresentation();const auto* Row=Schedule.Rows.FindByPredicate([&](const auto& X){return X.Id==Id;});if(!Row)return false;
 const auto Previous=Snapshot;if(Row->StopIndex!=INDEX_NONE)Snapshot.SelectedStopIndex=Row->StopIndex;if(!Row->GoodId.IsNone())Snapshot.PreferredGoodStableId=Row->GoodId;if(!Row->CityId.IsNone())Snapshot.SelectedCityStableId=Row->CityId;Snapshot.FocusedSemanticId=FName(*Id);
 if(bRemoteEstablishment){if(RemoteInterestRequested)RemoteInterestRequested(Snapshot.SelectedCityStableId);RefreshRemoteCityReports();RefreshEstablishment();RefreshRemotePresence();}
 if(LastProjection&&LastRegistry){const auto P=LastProjection;ApplyProjection(*P,*LastRegistry);}PublishIfChanged(Previous);return true;
}
#undef LOCTEXT_NAMESPACE

void FHansaTradeSchedulePresentation::PrepareForReplication() {
 bArrivalKnown=ArrivalTick.IsSet();ArrivalValue=ArrivalTick.Get(0);bProgressKnown=Progress.IsSet();ProgressValue=Progress.Get(0);
 for(auto& R:Rows){R.bCarriedKnown=R.Carried.IsSet();R.CarriedValue=R.Carried.Get(0);R.bPreparedKnown=R.Prepared.IsSet();R.PreparedValue=R.Prepared.Get(0);R.bTransferRequestedKnown=R.TransferRequested.IsSet();R.TransferRequestedValue=R.TransferRequested.Get(0);}
}
void FHansaTradeSchedulePresentation::RestoreReplicatedOptionals() {
 ArrivalTick=bArrivalKnown?TOptional<int64>(ArrivalValue):TOptional<int64>();Progress=bProgressKnown?TOptional<float>(ProgressValue):TOptional<float>();
 for(auto& R:Rows){R.Carried=R.bCarriedKnown?TOptional<int64>(R.CarriedValue):TOptional<int64>();R.Prepared=R.bPreparedKnown?TOptional<int64>(R.PreparedValue):TOptional<int64>();R.TransferRequested=R.bTransferRequestedKnown?TOptional<int64>(R.TransferRequestedValue):TOptional<int64>();}
}
