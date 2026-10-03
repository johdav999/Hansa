#include "Presence/HansaConstructionDelivery.h"

namespace Hansa::Simulation
{
 bool FHansaConstructionDelivery::Release(FHansaTradeStationState& S,FHansaInventoryLedger& L,FHansaSimulationTick Tick)
 {
  const auto Reservations=L.CreateReadOnlyAccess().CaptureSnapshot();
  for(const auto Id:S.DeliveryReservations)
   if(Reservations.GetReservations().ContainsByPredicate([&](const auto& R){return R.Id==Id;})&&!L.TryReleaseReservation(Id,Tick,L.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess())return false;
  S.DeliveryReservations.Reset();S.DeliveryMode=0;return true;
 }

 void FHansaConstructionDelivery::Collect(FHansaTradeStationState& S,FHansaInventoryLedger& L,
  TConstArrayView<FHansaCompiledPresenceUpgradeGoodCost> Costs,FHansaSimulationTick Tick,
  bool AtDestination,TConstArrayView<FHansaInventoryId> Sources)
 {
  if(!S.DeliveryMode||S.Status!=EHansaTradeStationStatus::Proposed)return;
  Collect(S.FundingInventoryId,S.SpentGoods,S.DeliveryReservations,L,Costs,Tick,AtDestination,Sources);
 }

 void FHansaConstructionDelivery::Collect(FHansaInventoryId Cargo,TArray<FHansaTradeStationSpentGood>& DeliveredGoods,
  TArray<FHansaReservationId>& Reservations,FHansaInventoryLedger& L,
  TConstArrayView<FHansaCompiledPresenceUpgradeGoodCost> Costs,FHansaSimulationTick Tick,
  bool AtDestination,TConstArrayView<FHansaInventoryId> Sources)
 {
  for(const auto& C:Costs){
   const auto Good=FHansaGoodId::TryParse(C.GoodId);if(!Good)continue;
   auto Delivered=[&](){const auto* D=DeliveredGoods.FindByPredicate([&](const auto& G){return G.GoodId==Good.Value;});return D?D->Quantity.GetRawValue():int64(0);};
   // Finish reservations first. SourceReservationId consumes physical cargo and its reservation atomically.
   if(AtDestination){
    const auto Snapshot=L.CreateReadOnlyAccess().CaptureSnapshot();
    for(const auto Id:Reservations){const auto* R=Snapshot.GetReservations().FindByPredicate([&](const auto& R){return R.Id==Id&&R.GoodId==Good.Value;});if(!R)continue;
     if(L.TryTransfer(FHansaInventoryEndpoint::Inventory(Cargo),FHansaInventoryEndpoint::Sink(TEXT("PresenceConstructionEscrow")),Good.Value,R->Quantity,Tick,L.CreateReadOnlyAccess().GetLastMovementSequence()+1,Id).IsSuccess()){
      auto* D=DeliveredGoods.FindByPredicate([&](const auto& G){return G.GoodId==Good.Value;});if(D)D->Quantity=FHansaQuantity::FromRaw(D->Quantity.GetRawValue()+R->Quantity.GetRawValue());else DeliveredGoods.Add({Good.Value,R->Quantity});
     }
    }
    const auto Remaining=L.CreateReadOnlyAccess().CaptureSnapshot();Reservations.RemoveAll([&](const auto Id){return !Remaining.GetReservations().ContainsByPredicate([&](const auto& R){return R.Id==Id;});});
   }
   int64 Carried=0;const auto Reserved=L.CreateReadOnlyAccess().CaptureSnapshot();
   for(const auto& R:Reserved.GetReservations())if(Reservations.Contains(R.Id)&&R.GoodId==Good.Value)Carried+=R.Quantity.GetRawValue();
   int64 Missing=FMath::Max<int64>(0,C.QuantityMilliUnits-Delivered()-Carried);if(!Missing)continue;
   // Use cargo already aboard, then exact missing quantities from connected home stock.
   TOptional<FHansaInventoryLedger> BeforePickup;if(!AtDestination&&!Sources.IsEmpty())BeforePickup=L;
   auto Stock=L.CreateReadOnlyAccess().QueryStock(Cargo,Good.Value);
   int64 Available=Stock?FMath::Min(Missing,Stock->Available.GetRawValue()):0;
   if(!AtDestination&&Available<Missing)for(const auto Source:Sources){
    const auto Local=L.CreateReadOnlyAccess().QueryStock(Source,Good.Value);const auto Hold=L.CreateReadOnlyAccess().QueryInventory(Cargo);
    const int64 Raw=Local&&Hold?FMath::Min3(Missing-Available,FMath::Max<int64>(0,Local->Available.GetRawValue()-L.CreateReadOnlyAccess().QueryProtectedRaw(Source,Good.Value)),Hold->FreeCapacity.GetRawValue()):0;
    if(Raw>0&&L.TryTransfer(FHansaInventoryEndpoint::Inventory(Source),FHansaInventoryEndpoint::CargoSlot(Cargo,INDEX_NONE),Good.Value,FHansaQuantity::FromRaw(Raw),Tick,L.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess())Available+=Raw;
    if(Available==Missing)break;
   }
   if(Available<=0)continue;
   if(AtDestination){
    if(L.TryTransfer(FHansaInventoryEndpoint::Inventory(Cargo),FHansaInventoryEndpoint::Sink(TEXT("PresenceConstructionEscrow")),Good.Value,FHansaQuantity::FromRaw(Available),Tick,L.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess()){
     auto* D=DeliveredGoods.FindByPredicate([&](const auto& G){return G.GoodId==Good.Value;});if(D)D->Quantity=FHansaQuantity::FromRaw(D->Quantity.GetRawValue()+Available);else DeliveredGoods.Add({Good.Value,FHansaQuantity::FromRaw(Available)});
    }
   }else{
    // Dedicated reservation namespace, collision checked against every live ledger reservation.
    const auto AllReservations=L.CreateReadOnlyAccess().CaptureSnapshot();
    uint64 RawId=uint64(1)<<63;for(const auto& R:AllReservations.GetReservations())if(R.Id.GetValue()>=RawId){if(R.Id.GetValue()==MAX_uint64){RawId=0;break;}RawId=R.Id.GetValue()+1;}
    const auto Id=FHansaReservationId::TryCreate(RawId);
    if(Id&&L.TryReserve(Cargo,Id.Value,Good.Value,FHansaQuantity::FromRaw(Available),Tick,L.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess())Reservations.Add(Id.Value);else if(BeforePickup)L=MoveTemp(BeforePickup.GetValue());
   }
  }
  DeliveredGoods.Sort([](const auto& A,const auto& B){return A.GoodId<B.GoodId;});
 }
}
