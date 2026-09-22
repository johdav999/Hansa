#include "Presence/HansaStationOrders.h"

namespace Hansa::Simulation
{
 const TCHAR* LexToString(EHansaStationOrderOutcome V)
 {
  switch(V) {
  case EHansaStationOrderOutcome::Pending: return TEXT("Pending market update");
  case EHansaStationOrderOutcome::Filled: return TEXT("Update cap filled");
  case EHansaStationOrderOutcome::Partial: return TEXT("Partial fill");
  case EHansaStationOrderOutcome::Blocked: return TEXT("Blocked");
  case EHansaStationOrderOutcome::Completed: return TEXT("Target reached; waiting for stock to change");
  case EHansaStationOrderOutcome::Paused: return TEXT("Paused; resume to trade");
  case EHansaStationOrderOutcome::Suspended: return TEXT("Suspended");
  case EHansaStationOrderOutcome::Cancelled: return TEXT("Cancelled");
  default: return TEXT("Unavailable"); }
 }
 const TCHAR* LexToString(EHansaStationOrderBlocker V)
 {
  switch(V) {
  case EHansaStationOrderBlocker::None: return TEXT("No blocker");
  case EHansaStationOrderBlocker::Funds: return TEXT("Insufficient house funds; earn money or pause purchases");
  case EHansaStationOrderBlocker::Budget: return TEXT("Purchase budget exhausted; increase the total budget");
  case EHansaStationOrderBlocker::MarketStock: return TEXT("Market stock unavailable; wait for supply");
  case EHansaStationOrderBlocker::StationCapacity: return TEXT("Station full; release stock or lower the target");
  case EHansaStationOrderBlocker::MarketCapacity: return TEXT("Market capacity unavailable; wait for local demand");
  case EHansaStationOrderBlocker::ReservedStock: return TEXT("Stock reserved or protected; wait for reservations to clear");
  case EHansaStationOrderBlocker::Access: return TEXT("Station rights suspended; restore commercial access");
  case EHansaStationOrderBlocker::MarketUnavailable: return TEXT("Market unavailable; choose an authorized good with a report");
  case EHansaStationOrderBlocker::PriceLimit: return TEXT("Price limit not met; order remains open for a later market update");
  case EHansaStationOrderBlocker::Arithmetic: return TEXT("Settlement limit reached; reduce the cap or budget");
  default: return TEXT("Unavailable"); }
 }
 bool ValidateStationOrder(const FHansaStationOrderState& O, int64 Tick)
 {
  if (!O.Id || !O.LastCommandId.IsValid() || !O.Terms.GoodId.IsValid() || O.Terms.Side > EHansaStationOrderSide::Release ||
   O.Terms.TargetOrReserveMilliUnits < 0 || O.Terms.CapMilliUnits <= 0 || O.Terms.TotalBudgetPfennig < 0 || O.Terms.LimitUnitPriceMilliMarks < 0 ||
   (O.Terms.LimitUnitPriceMilliMarks > 0 && (O.Terms.ReviewedMarketUpdateTick < 0 || O.Terms.ReviewedUnitPriceMilliMarks <= 0)) || O.Terms.LimitUnitPriceMilliMarks < 0 ||
   (O.Terms.LimitUnitPriceMilliMarks > 0 && (O.Terms.ReviewedMarketUpdateTick < 0 || O.Terms.ReviewedUnitPriceMilliMarks <= 0)) ||
   O.SpentPfennig < 0 || O.SpentPfennig > O.Terms.TotalBudgetPfennig || O.NextUpdateTick < 0 || O.History.Num() > 16) return false;
  int64 Previous = -1;
  for (const auto& E : O.History) {
   if(E.Tick < Previous || E.Tick > Tick || E.Tick < 0 || E.MarketUpdateTick < -1 || E.MarketUpdateTick > E.Tick ||
    E.RequestedMilliUnits < 0 || E.AppliedMilliUnits < 0 || E.AppliedMilliUnits > E.RequestedMilliUnits ||
    E.UnitPriceMilliMarks < 0 || E.Outcome > EHansaStationOrderOutcome::Cancelled || E.Blocker > EHansaStationOrderBlocker::Arithmetic ||
    E.LastMovementSequence < E.FirstMovementSequence) return false;
   Previous = E.Tick;
  }
  return true;
 }
}
