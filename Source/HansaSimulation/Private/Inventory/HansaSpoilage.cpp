#include "Inventory/HansaSpoilageInternal.h"
namespace Hansa::Simulation
{
void FHansaSpoilageExecutor::Advance(FHansaInventoryLedger& Ledger, TArray<FHansaLogisticsJobState>& Jobs,
 TArray<FHansaLogisticsRequestState>& Requests, TArray<FHansaProductionState>& Productions,
 const FHansaEconomicRegistry& Registry, FHansaSimulationTick Tick, uint32 MinutesPerTick)
{
 constexpr int64 Denominator = 10000LL * 1440;
 for (const auto& Good : Registry.GetGoods())
 {
  if (!Good.bSpoilageEnabled || Good.SpoilageBasisPointsPerDay <= 0) continue;
  const auto Id = FHansaGoodId::TryParse(Good.StableId).Value;
  auto* Carry = Ledger.Spoilage.FindByPredicate([&](const auto& V) { return V.GoodId == Id; });
  if (!Carry) { Ledger.Spoilage.Add({Id,0,0}); Ledger.Spoilage.Sort([](const auto& A,const auto& B){return A.GoodId<B.GoodId;}); Carry=Ledger.Spoilage.FindByPredicate([&](const auto& V){return V.GoodId==Id;}); }
  const int64 Numerator = FMath::Min<int64>(Denominator, static_cast<int64>(Good.SpoilageBasisPointsPerDay)*MinutesPerTick);
  auto Loss = [&](int64 Raw)
  {
   // Decompose first: products stay bounded even for very large legal inventories.
   const int64 Whole = (Raw / Denominator) * Numerator;
   const int64 Fraction = (Raw % Denominator) * Numerator + Carry->RemainderNumerator;
   const int64 Result = FMath::Min(Raw, Whole + Fraction / Denominator);
   Carry->RemainderNumerator = Fraction % Denominator;
   Carry->DestroyedMilliUnits = FMath::Min(MAX_int64 - Result, Carry->DestroyedMilliUnits) + Result;
   return Result;
  };
  for (auto& Inventory : Ledger.Inventories)
  {
   auto* Stock=Inventory.Stocks.FindByPredicate([&](const auto& V){return V.GoodId==Id;});
   if (!Stock || Stock->Quantity.GetRawValue()<=0) continue;
   const int64 Raw=Stock->Quantity.GetRawValue(), Lost=Loss(Raw);
   if (!Lost) continue;
   // Split a stock's loss proportionately across its physical reservations. Remainders are charged to free stock.
   int64 ReservedLost=0;
   for (auto& Reservation : Ledger.Reservations)
   {
    if (Reservation.InventoryId!=Inventory.Id || Reservation.GoodId!=Id) continue;
    const auto Share=FHansaCheckedIntegerMath::TryMultiplyDivide(Lost,Reservation.Quantity.GetRawValue(),Raw,EHansaRoundingMode::TowardZero);
    int64 RLost=Share ? Share.Value : 0;
    // If all stock is reserved, charge the last fractional unit to a reservation rather than creating negative free stock.
    if (ReservedLost+RLost < Lost-(Raw-Stock->Reserved.GetRawValue()))
     RLost=FMath::Min(Reservation.Quantity.GetRawValue(),Lost-(Raw-Stock->Reserved.GetRawValue())-ReservedLost);
    RLost=FMath::Min(RLost,Lost-ReservedLost);
    Reservation.Quantity=FHansaQuantity::FromRaw(Reservation.Quantity.GetRawValue()-RLost); ReservedLost+=RLost;
   }
   Stock->Quantity=FHansaQuantity::FromRaw(Raw-Lost);
   Stock->Reserved=FHansaQuantity::FromRaw(Stock->Reserved.GetRawValue()-ReservedLost);
   FHansaInventoryMovement Movement;
   Movement.Sequence=++Ledger.LastMovementSequence; Movement.Tick=Tick; Movement.Kind=EHansaInventoryMovementKind::SinkWithdrawal;
   Movement.InventoryId=Inventory.Id; Movement.ExternalEndpointId=TEXT("Spoilage"); Movement.GoodId=Id; Movement.Quantity=FHansaQuantity::FromRaw(Lost);
   Ledger.AddRecentMovement(MoveTemp(Movement));
  }
  // Local haulers hold authoritative cargo outside an inventory. Include it exactly once in the same carry.
  for (auto& Job : Jobs)
  {
   if (Job.GoodId!=Id || Job.Status==EHansaLogisticsJobStatus::Completed) continue;
   int64 Lost=0;
   if (Job.CargoQuantity.GetRawValue()>0)
   {
    Lost=Loss(Job.CargoQuantity.GetRawValue());
    Job.CargoQuantity=FHansaQuantity::FromRaw(Job.CargoQuantity.GetRawValue()-Lost);
   }
   else if (Job.SourceReservationId.IsValid())
   {
    const auto* R=Ledger.Reservations.FindByPredicate([&](const auto& V){return V.Id==Job.SourceReservationId;});
    Lost=Job.Quantity.GetRawValue()-(R ? R->Quantity.GetRawValue() : 0);
   }
   if (Lost<=0) continue;
   Job.Quantity=FHansaQuantity::FromRaw(Job.Quantity.GetRawValue()-Lost);
   if (auto* Request=Requests.FindByPredicate([&](const auto& V){return V.Id==Job.RequestId;}))
    Request->InFlightQuantity=FHansaQuantity::FromRaw(Request->InFlightQuantity.GetRawValue()-Lost);
   if (Job.Quantity.GetRawValue()==0)
   {
    Job.Status=EHansaLogisticsJobStatus::Completed; Job.RemainingTravelTicks=0;
    Job.SourceReservationId=FHansaReservationId(); Job.PauseReason=EHansaLogisticsRoadPathFailure::None;
   }
  }
 }
 // A batch whose perishable input reservation was damaged must restart; never grant full output for partial inputs.
 for (auto& Production : Productions)
 {
  bool bDamaged=false;
  for (const auto& Input : Production.InputReservations)
  {
   const auto* R=Ledger.Reservations.FindByPredicate([&](const auto& V){return V.Id==Input.ReservationId;});
   if (!R || R->Quantity!=Input.Quantity) bDamaged=true;
  }
  if (!bDamaged) continue;
  for (const auto& Input : Production.InputReservations)
   if (const auto* R=Ledger.Reservations.FindByPredicate([&](const auto& V){return V.Id==Input.ReservationId;}))
    if (R->Quantity.GetRawValue()>0) (void)Ledger.TryReleaseReservation(R->Id,Tick,Ledger.LastMovementSequence+1);
  Production.InputReservations.Reset(); Production.ProgressTicks=0;
 }
 Ledger.Reservations.RemoveAll([](const auto& R){return R.Quantity.GetRawValue()==0;});
}
}
