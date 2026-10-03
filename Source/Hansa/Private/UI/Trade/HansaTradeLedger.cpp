#include "UI/HansaTradeLedger.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Definitions/HansaEconomicRegistry.h"
#define LOCTEXT_NAMESPACE "HansaTradeLedger"
using namespace Hansa::Simulation;
bool FHansaTradeLedgerRow::Matches(int32 F) const {
 switch(F){case 1:return Reserved>0||DesiredReserve>0;case 2:return Available<DesiredReserve||(bHasTarget&&Physical<AcquireTarget);case 3:return bHasTarget&&Physical>AcquireTarget;case 4:return bRouteCargo;case 5:return bOrderCargo;default:return Physical>0||Reserved>0||bOrderCargo||bRouteCargo;}
}
FString FHansaTradeLedger::Key() const {
 FString S=FString::Printf(TEXT("%s|%lld|%lld|%lld|%lld|%lld|%lld|%d"),*City.ToString(),StationId,InventoryId,Tick,Capacity,Used,Reserved,bAvailable)+Identity.ToString()+Status.ToString()+Upkeep.ToString()+Factor.ToString()+Handling.ToString()+Blocker.ToString()+Preservation.ToString();
 for(const auto& R:Rows)S+=R.Good.ToString()+R.Detail.ToString()+R.LastResult.ToString()+R.CapacityShare.ToString();return S;
}
namespace Hansa::UI {
FHansaTradeLedger BuildTradeLedger(const FHansaSimulationProjection& P,const FHansaEconomicRegistry& Registry,FHansaHouseId Viewer,FName City) {
 FHansaTradeLedger L;L.City=City;L.Tick=P.GetClock().GetTick().GetValue();
 L.Identity=LOCTEXT("NoStation","Station inventory unavailable");L.Blocker=LOCTEXT("NoStationHelp","Select a city with your station, or establish one through Presence. City-market stock is not your station stock.");
 if(!Viewer.IsValid())return L;
 const FHansaTradeStationProjection* T=nullptr;
 for(const auto& X:P.GetTradeStations())if(X.Station.OwnerId==Viewer&&X.Station.CityId.ToString()==City.ToString()&&(!T||T->Station.Id<X.Station.Id))T=&X;
 if(!T)return L;const auto& S=T->Station;L.StationId=S.Id.GetValue();L.InventoryId=S.InventoryId.GetValue();
 L.Identity=FText::Format(LOCTEXT("Identity","Your station · {0} · inventory #{1}"),FText::FromName(City),FText::AsNumber(L.InventoryId));
 const auto* I=P.GetInventories().FindByPredicate([&](const auto& X){return X.Id==S.InventoryId&&X.TradeStationId==S.Id&&X.OwnerKind==EHansaInventoryOwnerKind::TradeStation;});
 L.Preservation=FText::FromString(T->PreservedAssets+TEXT("\n")+T->ContinuingCosts+TEXT("\n")+T->RecoveryDiagnostic);
 L.Blocker=FText::FromString(T->Blocker.IsEmpty()?TEXT("No operational blocker."):T->Blocker+TEXT(". ")+T->NextStep);
 if(S.Status==EHansaTradeStationStatus::Active&&S.OperationalState==EHansaTradeStationOperationalState::Active&&T->RecoveryDiagnostic.IsEmpty())L.Blocker=LOCTEXT("Operational","No operational blocker. Stock remains owned by your house; closing preserves cargo for outbound recovery.");
 const TCHAR* State=TEXT("Active");
 if(S.Status==EHansaTradeStationStatus::Proposed)State=S.FundingInventoryId.IsValid()?TEXT("Awaiting pickup"):TEXT("Awaiting funding");else if(S.Status==EHansaTradeStationStatus::UnderConstruction)State=TEXT("Under construction");else if(S.Status==EHansaTradeStationStatus::Closed)State=TEXT("Closed");
 else switch(S.OperationalState){case EHansaTradeStationOperationalState::Underfunded:State=TEXT("Underfunded");break;case EHansaTradeStationOperationalState::StorageBlocked:State=TEXT("Storage blocked");break;case EHansaTradeStationOperationalState::OrderSuspended:State=TEXT("Orders suspended");break;case EHansaTradeStationOperationalState::RightsSuspended:State=TEXT("Rights suspended / outbound recovery");break;case EHansaTradeStationOperationalState::VoluntarilyClosed:State=TEXT("Closing / outbound recovery");break;case EHansaTradeStationOperationalState::Revoked:State=TEXT("Revoked / outbound recovery");break;default:if(S.Status==EHansaTradeStationStatus::Suspended)State=TEXT("Suspended");break;}
 L.Status=FText::FromString(State);if(!T->RecoveryDiagnostic.IsEmpty())L.Status=LOCTEXT("MissingDefinition","Incompatible or missing definition");
 L.Upkeep=FText::Format(LOCTEXT("Upkeep","Upkeep: {0} pfennig/tick · arrears: {1} pfennig\n{2}"),FText::AsNumber(S.UpkeepPfennigPerTick),FText::AsNumber(S.OutstandingUpkeepPfennig),FText::FromString(T->ContinuingCosts));
 int32 Live=0,Paused=0;for(const auto& O:S.Orders)if(!O.bCancelled){++Live;if(O.bPaused)++Paused;}
 L.Factor=FText::Format(LOCTEXT("Factor","Factor #{0} · {1} live orders · {2} paused"),FText::AsNumber(S.FactorId.GetValue()),FText::AsNumber(Live),FText::AsNumber(Paused));
 const auto* Policy=Registry.FindCityTradePolicyForCity(City.ToString());
 if(!Policy||!Policy->TradeStationSites.ContainsByPredicate([&](const auto& Site){return Site.SiteId==S.SiteId;})){L.Status=LOCTEXT("MissingContent","Missing station definition");L.Blocker=LOCTEXT("RestoreContent","Restore the original city policy/site or apply an explicit content migration. Preserve inventory and saved identities; do not delete records.");}
 // Physical station transfer executor uses a 50-unit base cap (HansaTradeInternal).
 int64 Handling=50000;
 const auto* Presence=P.GetForeignPresences().FindByPredicate([&](const auto& X){return X.HouseId==Viewer&&X.CityId==S.CityId;});
 if(Presence)for(const auto& B:Presence->Specializations)if(B.bSelected)Handling+=B.StationTransferCapBonusMilliUnits;
 L.Handling=Policy?FText::Format(LOCTEXT("Handling","Route handling: {0} units per physical operation; stock, reserve, rights and ship space still apply."),FText::AsNumber(double(Handling)/1000.)):LOCTEXT("UnknownHandling","Route handling unavailable: restore compatible city policy.");
 if(!I){L.Blocker=LOCTEXT("MissingInventory","Station inventory unavailable. Preserve the save and restore compatible content; no stock is assumed to be zero.");return L;}
 L.bAvailable=true;L.Capacity=I->Capacity.GetRawValue();L.Used=I->UsedCapacity.GetRawValue();L.Free=I->FreeCapacity.GetRawValue();L.Reserved=I->Reserved.GetRawValue();
 if(S.Status==EHansaTradeStationStatus::Closed&&(L.Used>0||L.Reserved>0||!T->Lease.OccupyingBuildingIds.IsEmpty()))L.Status=LOCTEXT("RecoveringClosure","Closing / outbound recovery");
 TArray<FHansaGoodId> Goods=I->AcceptedGoods;for(const auto& X:I->Stocks)Goods.AddUnique(X.GoodId);for(const auto& O:S.Orders)Goods.AddUnique(O.Terms.GoodId);Goods.Sort();
 for(const auto G:Goods){FHansaTradeLedgerRow R;R.Good=FName(*G.ToString());const auto* Def=Registry.FindGood(G.ToString());R.Label=FText::FromString(Def?Def->DisplayName:G.ToString()+TEXT(" (definition unavailable)"));
  if(const auto* X=I->Stocks.FindByPredicate([&](const auto& X){return X.GoodId==G;})){R.Physical=X->Stock.GetRawValue();R.Reserved=X->Reserved.GetRawValue();R.Available=X->Available.GetRawValue();}
  int64 Latest=-1;FString Causes;
  for(const auto& O:S.Orders)if(O.Terms.GoodId==G){if(!O.bCancelled){R.bOrderCargo=true;if(!R.OrderId)R.OrderId=O.Id;if(O.Terms.Side==EHansaStationOrderSide::Release){R.bHasReserve=true;R.DesiredReserve=FMath::Max(R.DesiredReserve,O.Terms.TargetOrReserveMilliUnits);}else{R.bHasTarget=true;R.AcquireTarget=FMath::Max(R.AcquireTarget,O.Terms.TargetOrReserveMilliUnits);}
   Causes+=FString::Printf(TEXT("Order #%llu: %s %lld milli-units; %s.\n"),O.Id,O.Terms.Side==EHansaStationOrderSide::Acquire?TEXT("acquire target"):TEXT("protected reserve"),O.Terms.TargetOrReserveMilliUnits,O.bPaused?TEXT("paused; sale reserve remains protected"):TEXT("running"));}
   if(!O.History.IsEmpty()&&O.History.Last().Tick>=Latest){const auto& E=O.History.Last();Latest=E.Tick;R.LastResult=FText::FromString(FString::Printf(TEXT("Order #%llu · %s · %lld/%lld milli-units · tick %lld · %s"),O.Id,LexToString(E.Outcome),E.AppliedMilliUnits,E.RequestedMilliUnits,E.Tick,LexToString(E.Blocker)));}}
  for(const auto& Route:P.GetRoutes())if(Route.OwnerId==Viewer){for(const auto& Stop:Route.Stops)if(Stop.CityId==S.CityId)for(const auto& A:Stop.Actions)if(A.GoodId==G&&IsStationTransfer(A.Kind)&&Route.Lifecycle!=EHansaRouteLifecycleState::Cancelled){R.bRouteCargo=true;if(!R.RouteId)R.RouteId=Route.Id.GetValue();Causes+=FString::Printf(TEXT("Route #%llu: %s cap %lld; source floor %lld milli-units (%s). Plan only, not committed cargo.\n"),Route.Id.GetValue(),LexToString(A.Kind),A.QuantityLimit.GetRawValue(),A.MinimumSourceReserve.GetRawValue(),LexToString(Route.Lifecycle));}
   const auto& E=Route.LastTransfer;if(E.CityId==S.CityId&&E.GoodId==G&&IsStationTransfer(E.Kind)&&E.Outcome!=EHansaRouteTransferOutcome::None&&E.Tick.GetValue()>=Latest){Latest=E.Tick.GetValue();R.LastResult=FText::FromString(FString::Printf(TEXT("Route #%llu · %s · %s · %lld/%lld milli-units · tick %lld"),Route.Id.GetValue(),LexToString(E.Kind),LexToString(E.Outcome),E.AppliedQuantity.GetRawValue(),E.RequestedQuantity.GetRawValue(),Latest));}}
  if(Latest<0)R.LastResult=LOCTEXT("NoReceipt","No retained order/route result");
  R.CapacityShare=L.Capacity>0?FText::Format(LOCTEXT("Share","{0}%"),FText::AsNumber(100.*double(R.Physical)/double(L.Capacity))):LOCTEXT("NoCapacity","Unavailable");
  R.Detail=FText::FromString(FString::Printf(TEXT("%s — your station inventory #%lld\nPhysical %lld = hard reserved %lld + available %lld milli-units.\nProtected sale reserve: %lld; available above that floor: %lld milli-units. A reserve is a policy floor, not a second physical reservation. Paused sale orders retain their floor.\nHard reservations have no order/route attribution in this projection. Incoming/outgoing commitments are unavailable; route caps and acquisition targets are plans, not guaranteed transfers.\n%s\nLatest retained result (not full movement history): %s"),*R.Label.ToString(),L.InventoryId,R.Physical,R.Reserved,R.Available,R.DesiredReserve,FMath::Max<int64>(0,R.Available-R.DesiredReserve),*Causes,*R.LastResult.ToString()));
  L.Rows.Add(MoveTemp(R));
 }
 return L;
}
}
#undef LOCTEXT_NAMESPACE
