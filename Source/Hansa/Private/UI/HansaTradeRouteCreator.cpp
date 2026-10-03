#include "UI/HansaTradeMapPresentationModel.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Trade/HansaCargoPlan.h"

#define LOCTEXT_NAMESPACE "HansaTradeRouteCreator"
using namespace Hansa::Simulation;

namespace
{
    void AppendCreateIntentStops(const TConstArrayView<FHansaRouteStop> Stops, FHansaClientCommandIntent& Intent)
    {
        for (const FHansaRouteStop& Stop : Stops)
        {
            FHansaClientRouteStopIntent& ClientStop = Intent.RouteStops.AddDefaulted_GetRef();
            ClientStop.CityId = Stop.CityId.ToString();
            for (const FHansaRouteCargoAction& Action : Stop.Actions)
            {
                FHansaClientRouteActionIntent& ClientAction = ClientStop.Actions.AddDefaulted_GetRef();
                ClientAction.Kind = static_cast<uint8>(Action.Kind);
                ClientAction.GoodId = Action.GoodId.ToString();
                ClientAction.QuantityMilliUnits = Action.QuantityLimit.GetRawValue();
                ClientAction.MinimumSourceReserveMilliUnits = Action.MinimumSourceReserve.GetRawValue();
                ClientAction.CargoSlotIndex = Action.CargoSlotIndex;
            }
        }
    }
}

bool UHansaTradeMapPresentationModel::BeginCreateIntent(FName Good, FName SourceCity)
{
    if(bRemoteEstablishment){const auto Before=Snapshot;Snapshot.EditorStatus=RemoteActionReason(TEXT("TradeMap.New"));PublishIfChanged(Before);return false;}
    if (IsAnyTradeCommandPending() || Snapshot.bCreating || Snapshot.bDirty) return false;
      const auto Previous = Snapshot;
      int64 OwnerShip = bShipDetailOpen ? Snapshot.SelectedVehicleValue : 0;
      if (OwnerShip == 0 && (Runtime.IsValid() || LastProjection))
      {
          const auto Projection = Runtime.IsValid() ? Runtime->BuildProjection() : THansaValueResult<FHansaSimulationProjection>::Success(*LastProjection);
          if (Projection)
          {
              const auto Owner = Runtime.IsValid() ? Runtime->GetHouseId() : ViewerHouse;
              int64 FallbackShip = 0;
              for (const auto& Vehicle : Projection.Value.GetVehicles())
              {
                  if (Vehicle.OwnerId != Owner || Vehicle.DefinitionId.ToString() != TEXT("Vehicle.Cog") || Vehicle.Capacity.GetRawValue() <= 0) continue;
                  if (FallbackShip == 0) FallbackShip = Vehicle.Id.GetValue();
                  const bool bAssigned = Projection.Value.GetRoutes().ContainsByPredicate([&](const auto& Route){return Route.VehicleId == Vehicle.Id && Route.Lifecycle != EHansaRouteLifecycleState::Cancelled;});
                  if (!bAssigned && Vehicle.Cargo.GetRawValue() == 0) { OwnerShip = Vehicle.Id.GetValue(); break; }
              }
              if (OwnerShip == 0) OwnerShip = FallbackShip;
          }
      }
      bShipDetailOpen = true; ShipDetailTab = TEXT("Route");
    Snapshot.WorkspacePage = TEXT("Workspace"); Snapshot.ActiveSection = TEXT("Route"); Snapshot.bOpen = true; Snapshot.bCreating = true; Snapshot.bShipInTransit = false; Snapshot.bReview = false; Snapshot.bDirty = true;
      Snapshot.SelectedRouteValue = 0; Snapshot.SelectedStopIndex = 0; Snapshot.CogValue = OwnerShip;
    Snapshot.DraftName = TEXT("Baltic provisions");
    Snapshot.PreferredGoodStableId = Good.IsNone() ? FName(TEXT("Good.Grain")) : Good;
    DraftStops.Reset();
    for (int32 I = 0; I < 2; ++I)
    {
        FHansaRouteStop Stop;
        Stop.CityId = FHansaCityDefinitionId::TryParse(I == 0 ? TEXT("City.Lubeck") : (!SourceCity.IsNone()&&SourceCity!=TEXT("City.Lubeck")?*SourceCity.ToString():TEXT("City.Rostock"))).Value;
        FHansaRouteCargoAction Action;
        const bool bLoad = Stop.CityId.ToString() == SourceCity.ToString();
        Action.Kind = bLoad ? EHansaRouteCargoActionKind::Load : EHansaRouteCargoActionKind::Unload;
        Action.GoodId = FHansaGoodId::TryParse(Snapshot.PreferredGoodStableId.ToString()).Value;
        Action.QuantityLimit = FHansaQuantity::FromRaw(10000);
        Action.MinimumSourceReserve = FHansaQuantity::FromRaw(0);
        Action.CargoSlotIndex = 0;
        Stop.Actions.Add(Action); DraftStops.Add(Stop);
    }
    RebuildStops(true); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::DiscardCreateIntent()
{
    if (Snapshot.bCommandPending || !Snapshot.bCreating) return false;
    const auto Previous = Snapshot;
    if (!Snapshot.bDiscardConfirmation) {
        Snapshot.bDiscardConfirmation=true;
        Snapshot.Validation=LOCTEXT("ConfirmDiscard", "Discard this unsaved voyage? Choose Confirm discard or Keep draft. No route has changed.");
        PublishIfChanged(Previous);return true;
    }
    Snapshot.bDiscardConfirmation=false;
    Snapshot.bCreating = false; Snapshot.bReview = false; Snapshot.bDirty = false;
    Snapshot.bCanCreate = false; DraftStops.Reset(); Snapshot.Stops.Reset();
    if (Runtime.IsValid()) { const auto P = Runtime->BuildProjection(); if (P) ApplyProjection(P.Value, *Runtime->GetEconomicRegistry()); }
    Snapshot.EditorStatus = LOCTEXT("Discarded", "Draft discarded. No route was changed.");
    PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::CanEditStops() const
{
    const auto* Route=FindSelectedRoute();
    // Editing is local draft work, including while the Cog is sailing. The
    // authoritative EditRoute command still enforces its save-time restrictions.
    return !IsAnyTradeCommandPending() && !Snapshot.bDiscardConfirmation && (Snapshot.bCreating || (Route && Route->bOwnedByPlayer && !Route->bCancelled));
}
bool UHansaTradeMapPresentationModel::AddStopIntent()
{
    if (CargoEditor.bOpen || !CanEditStops() || DraftStops.Num() >= 8 || DraftStops.IsEmpty()) return false;
    const auto Previous = Snapshot;
    FHansaRouteStop Stop = DraftStops.Last();
    const auto* Registry=Runtime.IsValid()?Runtime->GetEconomicRegistry():LastRegistry;
    const auto* Selected=FindSelectedRoute();
    const auto* Definition=Registry?Registry->FindRoute(Snapshot.bCreating?TEXT("Route.BalticSea"):Selected?Selected->DefinitionStableId.ToString():FString()):nullptr;
    TArray<FString> Cities;
    if(Definition)for(const auto& Leg:Definition->Connections){Cities.AddUnique(Leg.SourceCityId);Cities.AddUnique(Leg.DestinationCityId);}
    if(Cities.IsEmpty()&&bRemoteEstablishment)for(const auto& C:AllCities)Cities.Add(C.StableId.ToString());
    if(Cities.IsEmpty())return false;
    const int32 CityIndex=Cities.IndexOfByKey(Stop.CityId.ToString());
    Stop.CityId=FHansaCityDefinitionId::TryParse(Cities[(CityIndex+1)%Cities.Num()]).Value;
    DraftStops.Add(Stop); Snapshot.SelectedStopIndex = DraftStops.Num() - 1;
    Snapshot.bDirty=true; Snapshot.bDiscardConfirmation=false; Snapshot.bReview = false; RebuildStops(true); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::RemoveStopIntent()
{
    if (CargoEditor.bOpen || !CanEditStops() || DraftStops.Num() <= 1 || !DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)) return false;
    const auto Previous = Snapshot; DraftStops.RemoveAt(Snapshot.SelectedStopIndex);
    Snapshot.SelectedStopIndex = FMath::Min(Snapshot.SelectedStopIndex, DraftStops.Num()-1);
    Snapshot.bDirty=true; Snapshot.bDiscardConfirmation=false; Snapshot.bReview = false; RebuildStops(true); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::CycleStopCityIntent()
{
    if (CargoEditor.bOpen || !CanEditStops() || !DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)) return false;
    const auto Previous = Snapshot; auto& Stop = DraftStops[Snapshot.SelectedStopIndex];
    const auto* Registry=Runtime.IsValid()?Runtime->GetEconomicRegistry():LastRegistry;
    const auto* Selected=FindSelectedRoute();
    const auto* Definition=Registry?Registry->FindRoute(Snapshot.bCreating?TEXT("Route.BalticSea"):Selected?Selected->DefinitionStableId.ToString():FString()):nullptr;
    TArray<FString> Cities;
    if(Definition)for(const auto& Leg:Definition->Connections){Cities.AddUnique(Leg.SourceCityId);Cities.AddUnique(Leg.DestinationCityId);}
    if(Cities.IsEmpty()&&bRemoteEstablishment)for(const auto& C:AllCities)Cities.Add(C.StableId.ToString());
    if(Cities.IsEmpty())return false;
    const int32 CityIndex=Cities.IndexOfByKey(Stop.CityId.ToString());
    Stop.CityId=FHansaCityDefinitionId::TryParse(Cities[(CityIndex+1)%Cities.Num()]).Value;
    Snapshot.bDirty=true; Snapshot.bDiscardConfirmation=false; Snapshot.bReview = false; RebuildStops(true); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::CycleStopGoodIntent()
{
    if(bRemoteEstablishment&&CanEditStops()&&DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)&&!DraftStops[Snapshot.SelectedStopIndex].Actions.IsEmpty()){
     TArray<FString> Goods;for(const auto& L:RemoteLedgers)for(const auto& R:L.Rows)Goods.AddUnique(R.Good.ToString());Goods.Sort();if(Goods.IsEmpty())return false;const auto Before=Snapshot;auto& A=DraftStops[Snapshot.SelectedStopIndex].Actions[0];const int32 I=Goods.IndexOfByKey(A.GoodId.ToString());A.GoodId=FHansaGoodId::TryParse(Goods[(I+1)%Goods.Num()]).Value;Snapshot.bDirty=true;RebuildStops(true);PublishIfChanged(Before);return true;
    }
    if (!CanEditStops() || (!Runtime.IsValid()&&!LastRegistry) || !DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)) return false;
    const auto* Registry = Runtime.IsValid()?Runtime->GetEconomicRegistry():LastRegistry; if (!Registry || Registry->GetGoods().IsEmpty()) return false;
    const auto Previous = Snapshot; auto& Action = DraftStops[Snapshot.SelectedStopIndex].Actions[0];
    const auto& Goods = Registry->GetGoods();
    int32 Index = Goods.IndexOfByPredicate([&](const auto& G){ return G.StableId == Action.GoodId.ToString(); });
    Action.GoodId = FHansaGoodId::TryParse(Goods[(Index+1)%Goods.Num()].StableId).Value;
    Snapshot.bDirty=true; Snapshot.bDiscardConfirmation=false; Snapshot.bReview = false; RebuildStops(true); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::CycleCogIntent()
{
    if (Snapshot.bCommandPending || !Snapshot.bCreating || (!Runtime.IsValid() && !LastProjection)) return false;
    const auto P = Runtime.IsValid() ? Runtime->BuildProjection() : THansaValueResult<FHansaSimulationProjection>::Success(*LastProjection); if (!P) return false;
    TArray<int64> Cogs;
    for (const auto& V : P.Value.GetVehicles())
        if (V.OwnerId == (Runtime.IsValid() ? Runtime->GetHouseId() : ViewerHouse) && V.DefinitionId.ToString() == TEXT("Vehicle.Cog")) Cogs.Add(V.Id.GetValue());
    if (Cogs.IsEmpty()) return false;
    const auto Previous = Snapshot;
    Snapshot.CogValue = Cogs[(Cogs.IndexOfByKey(Snapshot.CogValue)+1)%Cogs.Num()];
    Snapshot.bReview = false; UpdateCreatorReview(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::SetRouteNameIntent(const FString& Name)
{
    if (Snapshot.bCommandPending || !Snapshot.bCreating || Snapshot.DraftName == Name) return false;
    const auto Previous = Snapshot; Snapshot.DraftName = Name.Left(49);
    Snapshot.bReview = false; UpdateCreatorReview(); PublishIfChanged(Previous); return true;
}

void UHansaTradeMapPresentationModel::UpdateCreatorReview()
{
    if (!Snapshot.bCreating || Snapshot.bCommandPending || Snapshot.bDiscardConfirmation) return;
    Snapshot.bCanCreate = false; Snapshot.bReassignCog = false;
    Snapshot.Validation = LOCTEXT("NoSession", "The game session is unavailable. Keep this draft and retry after reconnecting.");
    Snapshot.CogLabel = LOCTEXT("NoCog", "Choose an owned Cog · next ship");
    Snapshot.CreatorReview = FText();
    if ((!Runtime.IsValid() && !LastProjection) || (!Runtime.IsValid() && !LastRegistry)) return;
    const auto P = Runtime.IsValid() ? Runtime->BuildProjection() : THansaValueResult<FHansaSimulationProjection>::Success(*LastProjection); if (!P) return;
    const auto* Registry=Runtime.IsValid()?Runtime->GetEconomicRegistry():LastRegistry;
    Snapshot.ValidationTarget=TEXT("TradeMap.Creator.Cog");
    const auto* V = P.Value.GetVehicles().FindByPredicate([this](const auto& It){ return It.Id.GetValue() == static_cast<uint64>(Snapshot.CogValue); });
    if (!V || V->OwnerId != (Runtime.IsValid()?Runtime->GetHouseId():ViewerHouse)) { Snapshot.Validation = LOCTEXT("ChooseCog", "Choose an owned Cog before departure."); return; }
    Snapshot.CogLabel = FText::Format(LOCTEXT("CogLabel", "Selected Cog {0} · {1} units capacity · choose next"), FText::AsNumber(Snapshot.CogValue), FText::AsNumber(V->Capacity.GetRawValue()/1000));
    const auto* Assigned = P.Value.GetRoutes().FindByPredicate([&](const auto& R){ return R.VehicleId == V->Id && R.Lifecycle != EHansaRouteLifecycleState::Cancelled; });
    Snapshot.bReassignCog = Assigned && Assigned->Lifecycle==EHansaRouteLifecycleState::Inactive && Assigned->CurrentStopIndex==0 && V->Cargo.GetRawValue()==0;
    Snapshot.CogLabel=FText::Format(LOCTEXT("ShipContext","{0}\nOwned by you · cargo {1} units · {2}\n{3}"),Snapshot.CogLabel,FText::AsNumber(double(V->Cargo.GetRawValue())/1000.),FText::FromString(V->CurrentCityId.ToString().RightChop(5)),Assigned?FText::Format(LOCTEXT("AssignedId","Assigned route #{0}"),FText::AsNumber(Assigned->Id.GetValue())):LOCTEXT("UnassignedShip","Unassigned"));
    FString StationReview;
    int32 Ticks = 0; int64 Peak = 0, Cargo = 0;
    TMap<FString,int64> CargoByGood;
    bool bCapacityRisk = false, bStockRisk = false, bUnknown = false, bStale = false, bUnload = false;
    bool bLegs = DraftStops.Num() >= 2;
    bool bOversizedAction = false;
    const auto* Definition = Registry->FindRoute(TEXT("Route.BalticSea"));
    // Inspect the return to the first stop as well: import routes start empty at their unload port.
    for (int32 Step = 0; Step < DraftStops.Num() * 2; ++Step)
    {
        const int32 I = Step % DraftStops.Num();
        const auto& Stop = DraftStops[I];
        const auto& Next = DraftStops[(I+1)%DraftStops.Num()];
        const auto* Leg = Definition ? Definition->Connections.FindByPredicate([&](const auto& L){ return (L.SourceCityId == Stop.CityId.ToString() && L.DestinationCityId == Next.CityId.ToString()) || (L.DestinationCityId == Stop.CityId.ToString() && L.SourceCityId == Next.CityId.ToString()); }) : nullptr;
        bLegs &= Leg != nullptr; if (Leg && Step < DraftStops.Num()) Ticks += Leg->TravelTicks;
        for (const auto& Action : Stop.Actions)
        {
            bOversizedAction |= Action.QuantityLimit.GetRawValue() > V->Capacity.GetRawValue();
            int64& Held = CargoByGood.FindOrAdd(Action.GoodId.ToString());
            if (IsStationTransfer(Action.Kind))
            {
                const FHansaTradeStationProjection* Station = nullptr;
                for (const auto& Entry : P.Value.GetTradeStations()) if (Entry.Station.OwnerId == V->OwnerId && Entry.Station.CityId == Stop.CityId && Entry.Station.Status != EHansaTradeStationStatus::Closed) { Station = &Entry; break; }
                const FHansaInventoryProjection* Storage = nullptr;
                if (Station) for (const auto& Inv : P.Value.GetInventories()) if (Inv.Id == Station->Station.InventoryId) { Storage = &Inv; break; }
                if (Step < DraftStops.Num())
                {
                    if (!Storage) StationReview += LOCTEXT("StationUnavailable", "\nStation storage unavailable. Establish or restore your station before activating this route.").ToString();
                    else
                    {
                        const auto* Stock = Storage->Stocks.FindByPredicate([&](const auto& S){return S.GoodId == Action.GoodId;});
                        int64 Reserve = Action.MinimumSourceReserve.GetRawValue();
                        for (const auto& O : Station->Station.Orders) if (!O.bCancelled && O.Terms.Side == EHansaStationOrderSide::Release && O.Terms.GoodId == Action.GoodId) Reserve = FMath::Max(Reserve, O.Terms.TargetOrReserveMilliUnits);
                        const int64 Prepared = Stock ? FMath::Max<int64>(0, FMath::Min(Stock->Available.GetRawValue(), Stock->Stock.GetRawValue()-Reserve)) : 0;
                        StationReview += FText::Format(LOCTEXT("StationCargoReview", "\nStation #{0}: {1} units stock, {2} protected reserve, at most {3} prepared; storage {4}/{5}. City market ↔ factor orders ↔ station ↔ Cog ↔ destination. Transfers settle no money."),
                            FText::AsNumber(Station->Station.Id.GetValue()), FText::AsNumber(Stock ? double(Stock->Stock.GetRawValue())/1000. : 0.), FText::AsNumber(double(Reserve)/1000.), FText::AsNumber(double(Prepared)/1000.), FText::AsNumber(double(Storage->UsedCapacity.GetRawValue())/1000.), FText::AsNumber(double(Storage->Capacity.GetRawValue())/1000.)).ToString();
                    }
                }
                const int64 Qty = Action.QuantityLimit.GetRawValue();
                if (IsRouteLoad(Action.Kind))
                {
                    const auto* Stock = Storage ? Storage->Stocks.FindByPredicate([&](const auto& S){return S.GoodId == Action.GoodId;}) : nullptr;
                    bStockRisk |= !Stock || Stock->Available.GetRawValue() - Action.MinimumSourceReserve.GetRawValue() < Qty;
                    bCapacityRisk |= Qty > V->Capacity.GetRawValue() - Cargo;
                    const int64 Loaded = FMath::Min(Qty, FMath::Max<int64>(0,V->Capacity.GetRawValue()-Cargo)); Held += Loaded; Cargo += Loaded; Peak = FMath::Max(Peak,Cargo);
                }
                else { const int64 Unloaded=FMath::Min(Held,Qty);Held-=Unloaded;Cargo-=Unloaded;bUnload|=Unloaded>0; }
                continue;
            }
            const auto PortReport=Runtime.IsValid()?Runtime->QueryKnownMarketPrice(Stop.CityId,Action.GoodId):TOptional<FHansaKnownMarketPriceProjection>();
            bUnknown |= !PortReport.IsSet() || PortReport->InformationState==EHansaMarketInformationState::Unknown;
            bStale |= PortReport.IsSet() && PortReport->InformationState!=EHansaMarketInformationState::Current;
            if (IsRouteLoad(Action.Kind))
            {
                const int64 Qty = Action.QuantityLimit.GetRawValue();
                bCapacityRisk |= Qty > V->Capacity.GetRawValue() - Cargo;
                const int64 Loaded = FMath::Min(Qty, FMath::Max<int64>(0, V->Capacity.GetRawValue()-Cargo));
                Held += Loaded; Cargo += Loaded; Peak = FMath::Max(Peak, Cargo);
                const auto Supply = Runtime.IsValid()?Runtime->QueryKnownMarketSupply(Stop.CityId, Action.GoodId):TOptional<FHansaKnownMarketSupplyDemandProjection>();
                bUnknown |= !Supply.IsSet() || !Supply->Stock.IsSet();
                if (Supply.IsSet())
                {
                    bStale |= Supply->InformationState != EHansaMarketInformationState::Current;
                    if (Supply->Stock.IsSet()) bStockRisk |= Supply->Stock->GetRawValue() - Action.MinimumSourceReserve.GetRawValue() < Qty;
                }
            }
            else { const int64 Unloaded = FMath::Min(Held, Action.QuantityLimit.GetRawValue()); Held -= Unloaded; Cargo -= Unloaded; bUnload |= Unloaded > 0; }
        }
    }
    const int64 Cost = Ticks * V->UpkeepPfennigPerTravelTick;
    Snapshot.CreatorReview = FText::Format(LOCTEXT("Review", "Peak cargo (first two circuits)  {0} / {1} units\nRound trip base estimate  {2} travel ticks + {3} port ticks\nBase travel upkeep  {4} pfennig / circuit\nUpkeep-only cash effect  {5} pfennig; total cash result unknown. Winter and delays can increase travel time and upkeep.\nStation and home actions transfer goods without payment. Market actions buy or sell at execution prices. Factor orders settle separately; future proceeds are not guaranteed. Transfers yield no automatic sale revenue.\n{6}\n{7}\n{8}"),
        FText::AsNumber(double(Peak)/1000.0), FText::AsNumber(V->Capacity.GetRawValue()/1000), FText::AsNumber(Ticks), FText::AsNumber(DraftStops.Num()), FText::AsNumber(Cost), FText::AsNumber(-Cost),
        Snapshot.bReassignCog ? LOCTEXT("Reassign", "Reassigns this Cog and cancels its stopped route when you activate.") : Assigned ? LOCTEXT("AssignedBusy", "Cog is assigned and unavailable. No route will be changed.") : LOCTEXT("Available", "Cog available; no existing route will be replaced."),
        bUnknown ? LOCTEXT("UnknownReports", "? Stock report unavailable. Delivery quantity cannot be predicted.") : bStale ? LOCTEXT("StaleReports", "! Estimated or older stock reports. Verify supply before departure.") : LOCTEXT("FreshReports", "Current stock reports; supply may change before loading."),
        bCapacityRisk ? LOCTEXT("CapacityRisk", "! Loading will be limited by free capacity.") : bStockRisk ? LOCTEXT("StockRisk", "! Stock above reserve may be insufficient; partial loads are possible.") : LOCTEXT("Reserves", "Minimum reserves are always protected; destination capacity may limit unloading."));
    const bool HasHomeStop=DraftStops.ContainsByPredicate([](const auto& Stop){return Stop.CityId.ToString()==TEXT("City.Lubeck");});
    const bool HasHomeDock=P.Value.GetPlacements().ContainsByPredicate([](const auto& Placement){return Placement.Spec.CityId.ToString()==TEXT("City.Lubeck")&&Placement.Spec.BuildingDefinitionId.ToString()==TEXT("Building.Dock");});
    if(HasHomeStop&&!HasHomeDock)Snapshot.CreatorReview=FText::Format(LOCTEXT("MissingHarbor","{0}\n! No home dock is placed. Build a dock and connect its road access before expecting sea loads or deliveries. A valid route plan alone does not provide port access."),Snapshot.CreatorReview);
    if (!StationReview.IsEmpty()) Snapshot.CreatorReview = FText::Format(LOCTEXT("BufferedReview", "{0}{1}"), Snapshot.CreatorReview, FText::FromString(StationReview));
    Snapshot.ValidationTarget = Snapshot.DraftName.IsEmpty() || Snapshot.DraftName.Len()>48 || Snapshot.DraftName.TrimStartAndEnd()!=Snapshot.DraftName ? FName(TEXT("TradeMap.Creator.Name"))
        : bOversizedAction ? FName(TEXT("TradeMap.Editor.Quantity.Decrease")) : !bLegs ? FName(TEXT("TradeMap.Creator.City"))
        : V->Cargo.GetRawValue()!=0 || (Assigned && Assigned->Lifecycle!=EHansaRouteLifecycleState::Inactive) ? FName(TEXT("TradeMap.Creator.Cog"))
        : !DraftStops.IsEmpty() && DraftStops[0].CityId!=V->CurrentCityId ? FName(TEXT("TradeMap.Editor.Stop.Up"))
        : !bUnload ? FName(TEXT("TradeMap.Editor.Action.Cycle")) : FName(TEXT("TradeMap.Editor.Reserve.Decrease"));
    if (Snapshot.DraftName.IsEmpty() || Snapshot.DraftName.Len() > 48 || Snapshot.DraftName.TrimStartAndEnd() != Snapshot.DraftName)
        Snapshot.Validation = LOCTEXT("NameInvalid", "Enter a route name of 1–48 characters without leading or trailing spaces.");
    else if (bOversizedAction) Snapshot.Validation = LOCTEXT("OversizedAction", "Reduce each cargo quantity to the Cog capacity or below, then review again.");
    else if (!bLegs) Snapshot.Validation = LOCTEXT("LegInvalid", "Use at least two stops with supported connections between each pair, including the return leg. Change duplicate or unreachable cities, then review again.");
    else if (V->Cargo.GetRawValue() != 0 || (Assigned && (Assigned->Lifecycle != EHansaRouteLifecycleState::Inactive || Assigned->CurrentStopIndex != 0)))
        Snapshot.Validation = LOCTEXT("CogBusy", "Cog unavailable: wait for it to return, unload its cargo and pause the current route at its first stop.");
    else if (DraftStops[0].CityId != V->CurrentCityId) Snapshot.Validation = LOCTEXT("StartCity", "The first stop must be the Cog's current city. Move that stop to the top.");
    else if (!bUnload) Snapshot.Validation = LOCTEXT("NoUnload", "Add an unload action for a good loaded on this circuit so the route can deliver cargo.");
    else if (FHansaCargoPlan::HasConflictingCityActions(DraftStops))
        Snapshot.Validation = LOCTEXT("CityCargoConflict", "A product is set to both load and unload in the same city. Remove one of those instructions before creating the route.");
    else
    {
        if (!Runtime.IsValid()) {
            Snapshot.bCanCreate=!!NetworkCommandIntent;
            Snapshot.Validation=LOCTEXT("RemoteReview", "Preliminary review only. The server validates research, stock, access and assignment when submitted; rejection preserves this draft.");
            return;
        }
        uint64 Id = 0;
        const FHansaCommandGatewayResult Result = Runtime->CreateTradeRoute(V->Id, DraftStops, Snapshot.DraftName, Snapshot.bReassignCog, true, Id);
        Snapshot.bCanCreate = !!Result;
        Snapshot.Validation = Snapshot.bCanCreate ? LOCTEXT("Valid", "Plan valid. Review port access, stock and the voyage before activation.")
            : Result.GetError() == EHansaCommandGatewayError::ResearchEffectRequired
                ? LOCTEXT("ReserveResearchRequired", "Complete Reserve instructions research before using a minimum reserve. Open Research, finish the required technology, then review this preserved draft again.")
                : LOCTEXT("Rejected", "The route could not be validated. Check the name, Cog, stops and goods; your draft is preserved.");
    }
    if(Snapshot.bCanCreate&&HasHomeStop&&!HasHomeDock)
        Snapshot.Validation=LOCTEXT("PortPrerequisiteSummary", "No home dock: build a dock and connect its road access before expecting sea transfers. You may activate this plan, but cargo cannot load or unload at home yet.");
}

bool UHansaTradeMapPresentationModel::ReviewCreateIntent()
{
    if (Snapshot.bCommandPending || !Snapshot.bCreating) return false;
    const auto Previous = Snapshot; Snapshot.bDiscardConfirmation=false; UpdateCreatorReview(); Snapshot.bReview = true; ReviewedVoyage=CreatorReviewKey();
    Snapshot.EditorStatus = Snapshot.Validation; PublishIfChanged(Previous); return true;
}
bool UHansaTradeMapPresentationModel::EditCreateIntent()
{
    if (Snapshot.bCommandPending || !Snapshot.bCreating || !Snapshot.bReview) return false;
    const auto Previous = Snapshot; Snapshot.bReview = false; Snapshot.bDiscardConfirmation=false; PublishIfChanged(Previous); return true;
}
bool UHansaTradeMapPresentationModel::CreateAndActivateIntent()
{
    if (Snapshot.bCommandPending || !Snapshot.bCreating || !Snapshot.bReview || (!Runtime.IsValid() && !NetworkCommandIntent)) return false;
    const auto Previous = Snapshot; UpdateCreatorReview();
    if (ReviewedVoyage != CreatorReviewKey()) {
        Snapshot.bReview=false; Snapshot.bCanCreate=false;
        Snapshot.Validation=LOCTEXT("ReviewChanged", "Voyage conditions changed since review. Your draft is preserved. Select Review voyage to inspect the updated conditions before confirming.");
        Snapshot.EditorStatus=Snapshot.Validation;PublishIfChanged(Previous);return false;
    }
    if (!Snapshot.bCanCreate) { Snapshot.EditorStatus = Snapshot.Validation; PublishIfChanged(Previous); return false; }
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::CreateRoute;
		Intent.VehicleId = Snapshot.CogValue; Intent.RouteName = Snapshot.DraftName;
		Intent.bReassignStoppedVehicle = Snapshot.bReassignCog; AppendCreateIntentStops(DraftStops, Intent);
		Snapshot.bCommandPending=true;bPendingCreate=true;bPendingRouteEdit=false;PendingSequence=PendingNonce=0;
		const bool bSent = NetworkCommandIntent(Intent);
		if (bSent && Snapshot.bCommandPending)
		{
			Snapshot.bCanCreate = false;
			Snapshot.EditorStatus = LOCTEXT("CreatePending", "Waiting for the server. Your submitted draft is preserved until acknowledgement.");
            Snapshot.Validation=Snapshot.EditorStatus;
		}
		else if(!bSent) {Snapshot.bCommandPending=false; Snapshot.EditorStatus = LOCTEXT("CreateSendFailed", "Route creation could not be sent; your draft is preserved.");}
		PublishIfChanged(Previous); return bSent;
	}
    uint64 Id = 0;
    const auto Result = Runtime->CreateTradeRoute(FHansaVehicleId::TryCreate(Snapshot.CogValue).Value, DraftStops, Snapshot.DraftName, Snapshot.bReassignCog, false, Id);
    if (!Result) { Snapshot.Validation = Result.GetError() == EHansaCommandGatewayError::ResearchEffectRequired
        ? LOCTEXT("CreateResearchRequired", "Complete the required route research, then review this preserved draft again.")
        : LOCTEXT("CreateFailed", "Departure failed because the route or Cog changed. Your draft is preserved; review and retry."); PublishIfChanged(Previous); return false; }
    Snapshot.bCreating = false; Snapshot.bReview = false; Snapshot.bDirty = false; Snapshot.bCanCreate = false;
    Snapshot.SelectedRouteValue = Id; Snapshot.ModeFilter = EHansaTradeMapModeFilter::Sea;
    const auto P = Runtime->BuildProjection(); if (P) ApplyProjection(P.Value, *Runtime->GetEconomicRegistry());
    Snapshot.EditorStatus = LOCTEXT("Created", "Route created and activated. The first port action runs on the next simulation tick.");
    PublishIfChanged(Previous); return true;
}

FString UHansaTradeMapPresentationModel::CreatorReviewKey() const
{
    return (Runtime.IsValid()?FString::Printf(TEXT("%llu|"),Runtime->GetLastProcessedCommandSequence()):FString())+Snapshot.CogLabel.ToString()+TEXT("|")+Snapshot.CreatorReview.ToString()+TEXT("|")+Snapshot.Validation.ToString();
}
bool UHansaTradeMapPresentationModel::KeepDraftIntent()
{
    if(!Snapshot.bDiscardConfirmation)return false;
    const auto Previous=Snapshot;Snapshot.bDiscardConfirmation=false;UpdateCreatorReview();PublishIfChanged(Previous);return true;
}
void UHansaTradeMapPresentationModel::ReceiveCommandFeedback(const FHansaClientCommandFeedback& Feedback)
{
    if(ReceiveOrderFeedback(Feedback))return;
    if(ReceiveEstablishmentFeedback(Feedback))return;
    if(ReceiveRecoveryFeedback(Feedback))return;
    if(ReceiveDecisionFeedback(Feedback))return;
    if(ReceiveSpecializationFeedback(Feedback))return;
    if(ReceivePresenceFeedback(Feedback))return;
    if(!Snapshot.bCommandPending)return;
    if(Feedback.State==EHansaClientCommandState::Pending) {
        if(PendingSequence==0){PendingSequence=Feedback.ClientSequence;PendingNonce=Feedback.ClientNonce;}return;
    }
    if(PendingSequence==0 || Feedback.ClientSequence!=PendingSequence || Feedback.ClientNonce!=PendingNonce)return;
    const auto Previous=Snapshot;Snapshot.bCommandPending=false;Snapshot.bReview=false;
    Snapshot.EditorStatus=FText::FromString(Feedback.Message+TEXT(" ")+Feedback.Remedy);
    if(Feedback.bAccepted){Snapshot.bDirty=false;if(bPendingCreate){Snapshot.bCreating=false;Snapshot.bCanCreate=false;Snapshot.SelectedVehicleValue=Snapshot.CogValue;Snapshot.SelectedRouteValue=0;}}
    else if(bPendingCreate||bPendingRouteEdit) {Snapshot.bDirty=true;Snapshot.Validation=FText::Format(LOCTEXT("RemoteRejected","{0} Draft preserved. Correct the issue and review again."),Snapshot.EditorStatus);}
    PublishIfChanged(Previous);
}
#undef LOCTEXT_NAMESPACE
