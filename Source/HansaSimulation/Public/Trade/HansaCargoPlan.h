#pragma once
#include "Trade/HansaTrade.h"
#include "Inventory/HansaInventory.h"

namespace Hansa::Simulation
{
/** Shared deterministic planner. Quantities are optimistic plans, never stock reports. */
struct HANSASIMULATION_API FHansaCargoPlan
{
 static constexpr int32 SlotCount = 3;
 static bool HasOppositeAction(TConstArrayView<FHansaRouteStop> Stops, FHansaCityDefinitionId City,
  FHansaGoodId Good, bool Load)
 {
  for (const auto& Stop : Stops) if (Stop.CityId == City)
   for (const auto& Action : Stop.Actions)
    if (Action.GoodId == Good && IsRouteLoad(Action.Kind) != Load) return true;
  return false;
 }
 static bool HasConflictingCityActions(TConstArrayView<FHansaRouteStop> Stops)
 {
  for (const auto& Stop : Stops) for (const auto& Action : Stop.Actions)
   if (HasOppositeAction(Stops, Stop.CityId, Action.GoodId, IsRouteLoad(Action.Kind))) return true;
  return false;
 }
 static bool Apply(TArray<FHansaCargoSlot>& Slots, const FHansaRouteCargoAction& Action, int64 Capacity)
 {
  if (Action.CargoSlotIndex == INDEX_NONE) return true; // Readable legacy instruction, execution remains pooled.
  if (!Slots.IsValidIndex(Action.CargoSlotIndex) || Action.QuantityLimit.GetRawValue() <= 0) return false;
  auto& Slot = Slots[Action.CargoSlotIndex];
  if (!IsRouteLoad(Action.Kind))
  {
   // An empty first departure is valid; the same instruction serves subsequent loops.
   if (!Slot.GoodId.IsValid()) return true;
   if (Slot.GoodId != Action.GoodId) return false;
   const bool Protected = Action.Kind == EHansaRouteCargoActionKind::StationUnload || Action.Kind == EHansaRouteCargoActionKind::OwnedCityUnload;
   int64 TotalGood=0;for(const auto& S:Slots)if(S.GoodId==Action.GoodId)TotalGood+=S.Quantity.GetRawValue();
   const int64 Available=Protected?FMath::Max<int64>(0,TotalGood-Action.MinimumSourceReserve.GetRawValue()):TotalGood;
   const int64 Removed=FMath::Min(Slot.Quantity.GetRawValue(),FMath::Min(Action.QuantityLimit.GetRawValue(),Available));
   Slot.Quantity = FHansaQuantity::FromRaw(Slot.Quantity.GetRawValue()-Removed);
   if (!Slot.Quantity.GetRawValue()) Slot.GoodId = FHansaGoodId();
   return true;
  }
  if (Slot.GoodId.IsValid() && Slot.GoodId != Action.GoodId) return false;
  int64 Used = 0; for (const auto& S : Slots) Used += S.Quantity.GetRawValue();
  const int64 Added = FMath::Min(Action.QuantityLimit.GetRawValue(), FMath::Max<int64>(0, Capacity - Used));
  if (Added) { Slot.GoodId = Action.GoodId; Slot.Quantity = FHansaQuantity::FromRaw(Slot.Quantity.GetRawValue() + Added); }
  return true;
 }
 static bool Validate(TConstArrayView<FHansaRouteStop> Stops, int64 Capacity)
 {
  if (HasConflictingCityActions(Stops)) return false;
  TArray<FHansaCargoSlot> Slots; Slots.SetNum(SlotCount);
  for (int32 Loop = 0; Loop < 2; ++Loop) for (const auto& Stop : Stops)
   for (bool Load : {false, true}) for (const auto& Action : Stop.Actions)
    if (IsRouteLoad(Action.Kind) == Load && !Apply(Slots, Action, Capacity)) return false;
  return true;
 }
 static TArray<FHansaCargoSlot> Before(TConstArrayView<FHansaRouteStop> Stops, int32 StopIndex, bool Load, int64 Capacity)
 {
  TArray<FHansaCargoSlot> Slots; Slots.SetNum(SlotCount);
  for (int32 Loop = 0; Loop < 2; ++Loop) for (int32 I = 0; I < Stops.Num(); ++I)
   for (bool Row : {false, true})
   {
    if (Loop == 1 && I == StopIndex && Row == Load) return Slots;
    for (const auto& Action : Stops[I].Actions) if (IsRouteLoad(Action.Kind) == Row) Apply(Slots, Action, Capacity);
   }
  return Slots;
 }
};
}
