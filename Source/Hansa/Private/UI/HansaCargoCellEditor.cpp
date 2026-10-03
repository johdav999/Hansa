#include "UI/HansaCargoCellEditor.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "Trade/HansaCargoPlan.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Queries/HansaSimulationReadOnly.h"

using namespace Hansa::Simulation;

bool UHansaTradeMapPresentationModel::SetStopTown(int32 Stop, FName Town)
{
 if (!CanEditStops() || CargoEditor.bOpen || !DraftStops.IsValidIndex(Stop) || !AllCities.ContainsByPredicate([&](const auto& C){return C.StableId==Town;})) return false;
 const auto City=FHansaCityDefinitionId::TryParse(Town.ToString());if(!City)return false;
 const auto Previous=Snapshot;DraftStops[Stop].CityId=City.Value;Snapshot.bDirty=true;Snapshot.bReview=false;RebuildStops(true);PublishIfChanged(Previous);return true;
}

TArray<FHansaRouteStop> UHansaTradeMapPresentationModel::GetSlotDraft() const
{
 auto Stops = DraftStops;
 TArray<FHansaGoodId> Goods; Goods.SetNum(3);
 const auto Cargo=GetShipCargo();for(int32 I=0;I<FMath::Min(3,Cargo.Num());++I)Goods[I]=Cargo[I].GoodId;
 for (const auto& Stop : Stops) for (const auto& A : Stop.Actions) if (A.CargoSlotIndex == INDEX_NONE && !Goods.Contains(A.GoodId)) { const int32 Empty=Goods.IndexOfByPredicate([](const auto& G){return !G.IsValid();}); if(Empty!=INDEX_NONE)Goods[Empty]=A.GoodId;else Goods.Add(A.GoodId); }
 for (auto& Stop : Stops) for (auto& A : Stop.Actions) if (A.CargoSlotIndex == INDEX_NONE) A.CargoSlotIndex = Goods.IndexOfByKey(A.GoodId);
 return Stops;
}

TArray<FHansaCargoSlot> UHansaTradeMapPresentationModel::GetShipCargo() const
{
 const int64 Ship = Snapshot.bCreating ? Snapshot.CogValue : Snapshot.SelectedVehicleValue;
 if (LastProjection && !LastProjection->GetVehicles().ContainsByPredicate([&](const auto& V){return int64(V.Id.GetValue())==Ship && V.OwnerId==(Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse);})) return {};
 if (LastProjection) for (const auto& I : LastProjection->GetInventories()) if (I.OwnerKind == EHansaInventoryOwnerKind::Vehicle && int64(I.VehicleId.GetValue()) == Ship) return I.CargoSlots;
 TArray<FHansaCargoSlot> Result; for (const auto& V : RemoteVehicles) if (V.VehicleId == Ship && V.bPrivateDetailsVisible) for(const auto& S:V.CargoSlots)Result.Add({FHansaGoodId::TryParse(S.GoodId).Value,FHansaQuantity::FromRaw(S.QuantityMilliUnits)});
 return Result;
}

int64 UHansaTradeMapPresentationModel::GetShipCapacity() const
{
 const int64 Ship = Snapshot.bCreating ? Snapshot.CogValue : Snapshot.SelectedVehicleValue;
 if (LastProjection) for (const auto& V : LastProjection->GetVehicles()) if (int64(V.Id.GetValue()) == Ship) return V.Capacity.GetRawValue();
 for (const auto& V : RemoteVehicles) if (V.VehicleId == Ship) return V.CapacityMilliUnits;
 return 0;
}

FText UHansaTradeMapPresentationModel::GetShipIdentity() const
{
 const int64 Ship = Snapshot.bCreating ? Snapshot.CogValue : Snapshot.SelectedVehicleValue;
 // The simulation currently has stable vessel IDs rather than player-assigned names.
 return FText::FromString(Ship ? FString::Printf(TEXT("Cog #%lld"), Ship) : TEXT("Select a Cog"));
}
FName UHansaTradeMapPresentationModel::GetShipTown() const
{
 const int64 Ship=Snapshot.bCreating?Snapshot.CogValue:Snapshot.SelectedVehicleValue;
 if(LastProjection)for(const auto& V:LastProjection->GetVehicles())if(int64(V.Id.GetValue())==Ship)return FName(*V.CurrentCityId.ToString());
 for(const auto& V:RemoteVehicles)if(V.VehicleId==Ship)return FName(*V.CurrentCityId);
 return NAME_None;
}

bool UHansaTradeMapPresentationModel::CanOpenCargoCell(int32 Stop, int32 Slot, bool Load) const
{
 if (!CanEditStops() || !DraftStops.IsValidIndex(Stop) || Slot < 0 || Slot >= 3) return false;
 if (Load) return true;
 const auto Stops = GetSlotDraft();
 // Existing instructions remain editable/clearable even if their cargo has gone.
 if (Stops[Stop].Actions.ContainsByPredicate([&](const auto& A){return A.CargoSlotIndex == Slot && !IsRouteLoad(A.Kind);})) return true;
 const auto Planned = FHansaCargoPlan::Before(Stops, Stop, false, GetShipCapacity());
 const auto Current = GetShipCargo();
 return (Planned.IsValidIndex(Slot) && Planned[Slot].GoodId.IsValid() && Planned[Slot].Quantity.GetRawValue() > 0) ||
  (Current.IsValidIndex(Slot) && Current[Slot].GoodId.IsValid() && Current[Slot].Quantity.GetRawValue() > 0);
}

bool UHansaTradeMapPresentationModel::OpenCargoCell(int32 Stop, int32 Slot, bool Load)
{
 if (!CanOpenCargoCell(Stop, Slot, Load)) return false;
 if (GetShipCapacity() <= 0)
 {
  const auto Previous = Snapshot;
  Snapshot.EditorStatus = FText::FromString(TEXT("Choose an owned Cog with cargo capacity before setting load or unload instructions."));
  PublishIfChanged(Previous);
  return false;
 }
 CargoEditor = {};
 CargoEditor.bOpen = true; CargoEditor.bLoad = Load; CargoEditor.Stop = Stop; CargoEditor.Slot = Slot;
 CargoEditor.Ship = Snapshot.bCreating ? Snapshot.CogValue : Snapshot.SelectedVehicleValue;
 CargoEditor.Route = Snapshot.SelectedRouteValue; CargoEditor.City = DraftStops[Stop].CityId;
 const bool bOwnedTown = AllCities.ContainsByPredicate([&](const auto& City){return City.StableId.ToString()==CargoEditor.City.ToString() && City.bOwned;});
 CargoEditor.Draft.Kind = bOwnedTown
  ? (Load ? EHansaRouteCargoActionKind::OwnedCityLoad : EHansaRouteCargoActionKind::OwnedCityUnload)
  : (Load ? EHansaRouteCargoActionKind::Load : EHansaRouteCargoActionKind::Unload);
 CargoEditor.Draft.CargoSlotIndex = Slot; CargoEditor.Draft.QuantityLimit = FHansaQuantity::FromRaw(1000);
 const auto Stops = GetSlotDraft();
 for (const auto& A : Stops[Stop].Actions) if (A.CargoSlotIndex == Slot && IsRouteLoad(A.Kind) == Load) CargoEditor.Draft = A;
 RefreshCargoChoices();
 // New unloads start with the cargo expected in this slot at this stop.
 // Existing instructions retain the player's chosen quantity.
 if (!Load && !CargoEditor.Draft.GoodId.IsValid())
 {
  const auto* Choice = CargoEditor.Products.FindByPredicate([](const auto& P){return P.bEnabled && P.Planned > 0;});
  if (!Choice) Choice = CargoEditor.Products.FindByPredicate([](const auto& P){return P.bEnabled && P.Current > 0;});
  if (Choice)
  {
   CargoEditor.Draft.GoodId = Choice->GoodId;
   CargoEditor.Draft.QuantityLimit = FHansaQuantity::FromRaw(Choice->Planned > 0 ? Choice->Planned : Choice->Current);
  }
 }
 Changed.Broadcast(Snapshot, ++Revision); return true;
}

void UHansaTradeMapPresentationModel::RefreshCargoChoices()
{
 const auto Stops = GetSlotDraft(); const int32 Stop=CargoEditor.Stop, Slot=CargoEditor.Slot; const bool Load=CargoEditor.bLoad;
 CargoEditor.Products.Reset();
 const auto* Town=AllCities.FindByPredicate([&](const auto& C){return C.StableId.ToString()==CargoEditor.City.ToString();});
 CargoEditor.bTownMarketOnly=Town?Town->bMarketOnly:true;
 CargoEditor.SourceLabel=FText::FromString(IsStationTransfer(CargoEditor.Draft.Kind)?TEXT("Station transfer · no payment"):(CargoEditor.Draft.Kind==EHansaRouteCargoActionKind::OwnedCityLoad||CargoEditor.Draft.Kind==EHansaRouteCargoActionKind::OwnedCityUnload)?TEXT("Home inventory transfer · no payment"):!CargoEditor.bTownMarketOnly?TEXT("City stock transfer · no payment"):Load?TEXT("Market purchase · price at loading"):TEXT("Market sale · price at unloading"));
 const auto Planned = FHansaCargoPlan::Before(Stops, Stop, Load, GetShipCapacity());
 TArray<FHansaCargoSlot> Current;
 if (LastProjection) for (const auto& I : LastProjection->GetInventories()) if (int64(I.VehicleId.GetValue()) == CargoEditor.Ship && I.OwnerKind == EHansaInventoryOwnerKind::Vehicle) Current = I.CargoSlots;
 for (const auto& V : RemoteVehicles) if (V.VehicleId == CargoEditor.Ship && V.bPrivateDetailsVisible) for (const auto& CargoSlot : V.CargoSlots) Current.Add({FHansaGoodId::TryParse(CargoSlot.GoodId).Value, FHansaQuantity::FromRaw(CargoSlot.QuantityMilliUnits)});
 for (const auto& Id : AvailableGoods)
 {
  const auto Parsed = FHansaGoodId::TryParse(Id.ToString()); if (!Parsed) continue;
  FHansaCargoProductChoice Choice; Choice.GoodId = Parsed.Value; Choice.Name = GetOrderGoodLabel(Parsed.Value);
  Choice.Planned = Planned.IsValidIndex(Slot) && Planned[Slot].GoodId == Choice.GoodId ? Planned[Slot].Quantity.GetRawValue() : 0;
  Choice.Current = Current.IsValidIndex(Slot) && Current[Slot].GoodId == Choice.GoodId ? Current[Slot].Quantity.GetRawValue() : 0;
  if (Load)
  {
   const auto Supply = Runtime.IsValid() ? Runtime->QueryKnownMarketSupply(CargoEditor.City, Choice.GoodId) : TOptional<FHansaKnownMarketSupplyDemandProjection>();
   Choice.bEnabled = Supply && Supply->Stock.IsSet() && Supply->Stock->GetRawValue() > 0;
   Choice.Reported = Supply && Supply->Stock.IsSet() ? Supply->Stock->GetRawValue() : -1;
   Choice.Detail = FText::FromString(Supply && Supply->Stock.IsSet() ? FString::Printf(TEXT("Reported stock: %g · availability may change"), Supply->Stock->GetRawValue()/1000.) : TEXT("Stock report unavailable"));
   if (bRemoteEstablishment) for (const auto& Report : RemoteMarkets) if (Report.CityId == CargoEditor.City.ToString() && Report.GoodId == Choice.GoodId.ToString()) { Choice.bEnabled = Report.StockMilliUnits > 0; Choice.Reported=Report.StockMilliUnits;Choice.PricePf=Report.CurrentPriceMilliMarks;Choice.ReportAge=Report.ReportAgeTicks; Choice.Detail = FText::FromString(FString::Printf(TEXT("Reported: %g · price %g pf · %lld ticks old"), Report.StockMilliUnits/1000., Report.CurrentPriceMilliMarks/1000., Report.ReportAgeTicks)); }
   if (Runtime.IsValid()) { const auto Price = Runtime->QueryKnownMarketPrice(CargoEditor.City, Choice.GoodId); if (Price && Price->PriceMilliMarks.IsSet()) { Choice.PricePf=Price->PriceMilliMarks.GetValue();Choice.ReportAge=Price->ReportAgeTicks.Get(0); } if (Price && Price->PriceMilliMarks.IsSet()) Choice.Detail = FText::FromString(Choice.Detail.ToString() + FString::Printf(TEXT(" · price %g pf · %lld ticks old"), Price->PriceMilliMarks.GetValue()/1000., Price->ReportAgeTicks.Get(0))); }
   if(IsStationTransfer(CargoEditor.Draft.Kind)&&bRemoteEstablishment){Choice.bEnabled=false;Choice.Reported=-1;Choice.PricePf=-1;for(const auto& Ledger:RemoteLedgers)if(Ledger.City.ToString()==CargoEditor.City.ToString()&&Ledger.bAvailable)for(const auto& Row:Ledger.Rows)if(Row.Good.ToString()==Choice.GoodId.ToString()){Choice.Reported=Row.Available;Choice.bEnabled=Row.Available>0;Choice.Detail=FText::FromString(TEXT("Current owned station inventory · transfer"));}}
   if (IsStationTransfer(CargoEditor.Draft.Kind) && LastProjection)
   {
    Choice.bEnabled = false;Choice.Reported=-1;Choice.PricePf=-1;Choice.Detail=FText::FromString(TEXT("No available owned station stock at this stop"));
    for (const auto& S : LastProjection->GetTradeStations()) if (S.Station.OwnerId == (Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse) && S.Station.CityId == CargoEditor.City)
     for (const auto& I : LastProjection->GetInventories()) if (I.Id == S.Station.InventoryId)
      for (const auto& Stock : I.Stocks) if (Stock.GoodId == Choice.GoodId) { Choice.bEnabled = Stock.Available.GetRawValue() > 0; Choice.Reported=Stock.Available.GetRawValue();Choice.PricePf=-1;Choice.Detail = FText::FromString(FString::Printf(TEXT("Current station stock: %g · transfer"), Stock.Available.GetRawValue()/1000.)); }
   }
  }
  else
  {
   // Unload choices belong to this physical slot, never to the whole hold.
   // Keep this slot's route product visible for direction/conflict explanations
   // and retain existing instructions so an invalid draft can still be repaired.
   const bool bAssignedToSlot = Choice.Planned > 0 || Choice.Current > 0 ||
    Stops.ContainsByPredicate([&](const auto& RouteStop){return RouteStop.Actions.ContainsByPredicate(
     [&](const auto& A){return A.CargoSlotIndex == Slot && A.GoodId == Choice.GoodId;});});
   if(!bAssignedToSlot && Choice.GoodId!=CargoEditor.Draft.GoodId)continue;
   Choice.AssignedSlot = bAssignedToSlot ? Slot : INDEX_NONE;
   Choice.bEnabled = Choice.Planned > 0 || Choice.Current > 0;
   Choice.Detail = FText::FromString(FString::Printf(TEXT("%g planned on arrival · %g currently in this slot"), Choice.Planned/1000., Choice.Current/1000.));
  }
  if(!CargoEditor.bTownMarketOnly||IsStationTransfer(CargoEditor.Draft.Kind))Choice.PricePf=-1;
  if (FHansaCargoPlan::HasOppositeAction(Stops, CargoEditor.City, Choice.GoodId, Load))
  {
   Choice.bEnabled = false;
   Choice.Detail = FText::FromString(Load
    ? TEXT("Already set to unload in this city. Remove that unload instruction first.")
    : TEXT("Already set to load in this city. Remove that load instruction first."));
  }
  CargoEditor.Products.Add(Choice);
 }

}

bool UHansaTradeMapPresentationModel::SetCargoProduct(const FString& Good)
{
 if (!CargoEditor.bOpen) return false;
 const auto* Choice = CargoEditor.Products.FindByPredicate([&](const auto& P){return P.GoodId.ToString() == Good && P.bEnabled;});
 if (!Choice) return false;
 if (!CargoEditor.bLoad && CargoEditor.Draft.GoodId != Choice->GoodId)
  CargoEditor.Draft.QuantityLimit = FHansaQuantity::FromRaw(Choice->Planned > 0 ? Choice->Planned : Choice->Current);
 CargoEditor.Draft.GoodId = Choice->GoodId; CargoEditor.Error = FText();
 Changed.Broadcast(Snapshot, ++Revision); return true;
}

bool UHansaTradeMapPresentationModel::SetCargoQuantity(int64 Quantity, bool Reserve)
{
 if (!CargoEditor.bOpen) return false;
 if (Quantity < (Reserve ? 0 : 1) || Quantity > (Reserve ? 1000000000000LL : GetShipCapacity())) { CargoEditor.Error=FText::FromString(TEXT("Enter a positive cargo quantity within total ship capacity, or a nonnegative reserve."));Changed.Broadcast(Snapshot,++Revision);return false; }
 if (Reserve) CargoEditor.Draft.MinimumSourceReserve = FHansaQuantity::FromRaw(Quantity);
 else CargoEditor.Draft.QuantityLimit = FHansaQuantity::FromRaw(Quantity);
 CargoEditor.Error = FText(); Changed.Broadcast(Snapshot, ++Revision); return true;
}

bool UHansaTradeMapPresentationModel::SetCargoSource(uint8 Kind)
{
 if (!CargoEditor.bOpen || Kind > 5 || IsRouteLoad(static_cast<EHansaRouteCargoActionKind>(Kind)) != CargoEditor.bLoad) return false;
 if(Kind>=4&&CargoEditor.bTownMarketOnly){CargoEditor.Error=FText::FromString(TEXT("Home inventory transfers are unavailable at this foreign market."));Changed.Broadcast(Snapshot,++Revision);return false;}
 CargoEditor.Error=FText();
 CargoEditor.Draft.Kind = static_cast<EHansaRouteCargoActionKind>(Kind);
 RefreshCargoChoices(); Changed.Broadcast(Snapshot, ++Revision); return true;
}

bool UHansaTradeMapPresentationModel::ConfirmCargoCell(bool Remove)
{
 if (!CargoEditor.bOpen || !CanEditStops() || !DraftStops.IsValidIndex(CargoEditor.Stop) || DraftStops[CargoEditor.Stop].CityId != CargoEditor.City ||
  CargoEditor.Ship != (Snapshot.bCreating ? Snapshot.CogValue : Snapshot.SelectedVehicleValue) || CargoEditor.Route != Snapshot.SelectedRouteValue) return false;
 if(!Remove&&!CargoEditor.Error.IsEmpty())return false;
 auto Candidate = GetSlotDraft();
 for(const auto& Stop:Candidate)for(int32 I=0;I<Stop.Actions.Num();++I){const auto& A=Stop.Actions[I];for(int32 J=I+1;J<Stop.Actions.Num();++J)if(A.CargoSlotIndex==Stop.Actions[J].CargoSlotIndex&&IsRouteLoad(A.Kind)==IsRouteLoad(Stop.Actions[J].Kind)){CargoEditor.Error=FText::FromString(TEXT("This legacy route has multiple instructions for one cell. Its original plan is preserved. Create a replacement slot route before cancelling the old route."));Changed.Broadcast(Snapshot,++Revision);return false;}}
 auto& Actions = Candidate[CargoEditor.Stop].Actions;
 Actions.RemoveAll([&](const auto& A){return A.CargoSlotIndex == CargoEditor.Slot && IsRouteLoad(A.Kind) == CargoEditor.bLoad;});
 if (!Remove)
 {
  if (FHansaCargoPlan::HasOppositeAction(Candidate, CargoEditor.City, CargoEditor.Draft.GoodId, CargoEditor.bLoad))
  { CargoEditor.Error = FText::FromString(TEXT("A product cannot be loaded and unloaded in the same city. Remove the opposite instruction first.")); Changed.Broadcast(Snapshot, ++Revision); return false; }
  if (!CargoEditor.Draft.GoodId.IsValid() || !CargoEditor.Products.ContainsByPredicate([&](const auto& P){return P.GoodId == CargoEditor.Draft.GoodId && P.bEnabled;}))
  { CargoEditor.Error = FText::FromString(TEXT("Choose a product available for this slot and source.")); Changed.Broadcast(Snapshot, ++Revision); return false; }
  Actions.Add(CargoEditor.Draft);
 }
 const bool bRouteCargoValid = FHansaCargoPlan::Validate(Candidate, GetShipCapacity());
 DraftStops = MoveTemp(Candidate); Snapshot.bDirty = true; Snapshot.bReview = false; RebuildStops(true);
 if (!bRouteCargoValid)
  Snapshot.EditorStatus = FText::FromString(TEXT("Route draft updated. This cargo slot still has a conflicting product at another stop; unload it or use a different slot before saving the route."));
 CancelCargoCell(); return true;
}

void UHansaTradeMapPresentationModel::CancelCargoCell()
{
 const FName Origin(*FString::Printf(TEXT("TradeMap.Cargo.%d.%d.%s"), CargoEditor.Stop, CargoEditor.Slot, CargoEditor.bLoad ? TEXT("Load") : TEXT("Unload")));
 CargoEditor = {}; Changed.Broadcast(Snapshot, ++Revision); FocusRestoreRequested.Broadcast(Origin);
}
