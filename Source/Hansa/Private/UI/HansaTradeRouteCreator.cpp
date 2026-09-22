#include "UI/HansaTradeMapPresentationModel.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "World/HansaRuntimeSimulationHost.h"

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
            }
        }
    }
}

bool UHansaTradeMapPresentationModel::BeginCreateIntent(FName Good, FName SourceCity)
{
    if (Snapshot.bCreating) return false;
    const auto Previous = Snapshot;
    Snapshot.bOpen = true; Snapshot.bCreating = true; Snapshot.bShipInTransit = false; Snapshot.bReview = false; Snapshot.bDirty = true;
    Snapshot.SelectedRouteValue = 0; Snapshot.SelectedStopIndex = 0; Snapshot.CogValue = 0;
    Snapshot.DraftName = TEXT("Baltic provisions");
    Snapshot.PreferredGoodStableId = Good.IsNone() ? FName(TEXT("Good.Grain")) : Good;
    DraftStops.Reset();
    for (int32 I = 0; I < 2; ++I)
    {
        FHansaRouteStop Stop;
        Stop.CityId = FHansaCityDefinitionId::TryParse(I == 0 ? TEXT("City.Lubeck") : TEXT("City.Rostock")).Value;
        FHansaRouteCargoAction Action;
        const bool bLoad = Stop.CityId.ToString() == SourceCity.ToString();
        Action.Kind = bLoad ? EHansaRouteCargoActionKind::Load : EHansaRouteCargoActionKind::Unload;
        Action.GoodId = FHansaGoodId::TryParse(Snapshot.PreferredGoodStableId.ToString()).Value;
        Action.QuantityLimit = FHansaQuantity::FromRaw(10000);
        Action.MinimumSourceReserve = FHansaQuantity::FromRaw(bLoad ? 5000 : 0);
        Stop.Actions.Add(Action); DraftStops.Add(Stop);
    }
    CycleCogIntent(); RebuildStops(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::DiscardCreateIntent()
{
    if (!Snapshot.bCreating) return false;
    const auto Previous = Snapshot;
    Snapshot.bCreating = false; Snapshot.bReview = false; Snapshot.bDirty = false;
    Snapshot.bCanCreate = false; DraftStops.Reset(); Snapshot.Stops.Reset();
    if (Runtime.IsValid()) { const auto P = Runtime->BuildProjection(); if (P) ApplyProjection(P.Value, *Runtime->GetEconomicRegistry()); }
    Snapshot.EditorStatus = LOCTEXT("Discarded", "Draft discarded. No route was changed.");
    PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::AddStopIntent()
{
    if (!Snapshot.bCreating || DraftStops.Num() >= 8 || DraftStops.IsEmpty()) return false;
    const auto Previous = Snapshot;
    FHansaRouteStop Stop = DraftStops.Last();
    Stop.CityId = FHansaCityDefinitionId::TryParse(Stop.CityId.ToString() == TEXT("City.Lubeck") ? TEXT("City.Rostock") : TEXT("City.Lubeck")).Value;
    DraftStops.Add(Stop); Snapshot.SelectedStopIndex = DraftStops.Num() - 1;
    Snapshot.bReview = false; RebuildStops(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::RemoveStopIntent()
{
    if (!Snapshot.bCreating || DraftStops.Num() <= 1 || !DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)) return false;
    const auto Previous = Snapshot; DraftStops.RemoveAt(Snapshot.SelectedStopIndex);
    Snapshot.SelectedStopIndex = FMath::Min(Snapshot.SelectedStopIndex, DraftStops.Num()-1);
    Snapshot.bReview = false; RebuildStops(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::CycleStopCityIntent()
{
    if (!Snapshot.bCreating || !DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)) return false;
    const auto Previous = Snapshot; auto& Stop = DraftStops[Snapshot.SelectedStopIndex];
    Stop.CityId = FHansaCityDefinitionId::TryParse(Stop.CityId.ToString() == TEXT("City.Lubeck") ? TEXT("City.Rostock") : TEXT("City.Lubeck")).Value;
    Snapshot.bReview = false; RebuildStops(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::CycleStopGoodIntent()
{
    if (!Snapshot.bCreating || !Runtime.IsValid() || !DraftStops.IsValidIndex(Snapshot.SelectedStopIndex)) return false;
    const auto* Registry = Runtime->GetEconomicRegistry(); if (!Registry || Registry->GetGoods().IsEmpty()) return false;
    const auto Previous = Snapshot; auto& Action = DraftStops[Snapshot.SelectedStopIndex].Actions[0];
    const auto& Goods = Registry->GetGoods();
    int32 Index = Goods.IndexOfByPredicate([&](const auto& G){ return G.StableId == Action.GoodId.ToString(); });
    Action.GoodId = FHansaGoodId::TryParse(Goods[(Index+1)%Goods.Num()].StableId).Value;
    Snapshot.bReview = false; RebuildStops(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::CycleCogIntent()
{
    if (!Snapshot.bCreating || !Runtime.IsValid()) return false;
    const auto P = Runtime->BuildProjection(); if (!P) return false;
    TArray<int64> Cogs;
    for (const auto& V : P.Value.GetVehicles())
        if (V.OwnerId == Runtime->GetHouseId() && V.DefinitionId.ToString() == TEXT("Vehicle.Cog")) Cogs.Add(V.Id.GetValue());
    if (Cogs.IsEmpty()) return false;
    const auto Previous = Snapshot;
    Snapshot.CogValue = Cogs[(Cogs.IndexOfByKey(Snapshot.CogValue)+1)%Cogs.Num()];
    Snapshot.bReview = false; UpdateCreatorReview(); PublishIfChanged(Previous); return true;
}

bool UHansaTradeMapPresentationModel::SetRouteNameIntent(const FString& Name)
{
    if (!Snapshot.bCreating || Snapshot.DraftName == Name) return false;
    const auto Previous = Snapshot; Snapshot.DraftName = Name.Left(49);
    Snapshot.bReview = false; UpdateCreatorReview(); PublishIfChanged(Previous); return true;
}

void UHansaTradeMapPresentationModel::UpdateCreatorReview()
{
    if (!Snapshot.bCreating) return;
    Snapshot.bCanCreate = false; Snapshot.bReassignCog = false;
    Snapshot.Validation = LOCTEXT("NoSession", "The game session is unavailable. Keep this draft and retry after reconnecting.");
    Snapshot.CogLabel = LOCTEXT("NoCog", "No owned Cog available");
    Snapshot.CreatorReview = FText();
    if (!Runtime.IsValid()) return;
    const auto P = Runtime->BuildProjection(); if (!P) return;
    const auto* V = P.Value.GetVehicles().FindByPredicate([this](const auto& It){ return It.Id.GetValue() == static_cast<uint64>(Snapshot.CogValue); });
    if (!V) { Snapshot.Validation = LOCTEXT("ChooseCog", "Choose an owned Cog before departure."); return; }
    Snapshot.CogLabel = FText::Format(LOCTEXT("CogLabel", "Cog {0} · {1} units capacity"), FText::AsNumber(Snapshot.CogValue), FText::AsNumber(V->Capacity.GetRawValue()/1000));
    const auto* Assigned = P.Value.GetRoutes().FindByPredicate([&](const auto& R){ return R.VehicleId == V->Id && R.Lifecycle != EHansaRouteLifecycleState::Cancelled; });
    Snapshot.bReassignCog = Assigned && Assigned->Lifecycle==EHansaRouteLifecycleState::Inactive && Assigned->CurrentStopIndex==0 && V->Cargo.GetRawValue()==0;
    FString StationReview;
    int32 Ticks = 0; int64 Peak = 0, Cargo = 0;
    TMap<FString,int64> CargoByGood;
    bool bCapacityRisk = false, bStockRisk = false, bUnknown = false, bStale = false, bUnload = false;
    bool bLegs = DraftStops.Num() >= 2;
    bool bOversizedAction = false;
    const auto* Definition = Runtime->GetEconomicRegistry()->FindRoute(TEXT("Route.BalticSea"));
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
            const auto PortReport=Runtime->QueryKnownMarketPrice(Stop.CityId,Action.GoodId);
            bUnknown |= !PortReport.IsSet() || PortReport->InformationState==EHansaMarketInformationState::Unknown;
            bStale |= PortReport.IsSet() && PortReport->InformationState!=EHansaMarketInformationState::Current;
            if (IsRouteLoad(Action.Kind))
            {
                const int64 Qty = Action.QuantityLimit.GetRawValue();
                bCapacityRisk |= Qty > V->Capacity.GetRawValue() - Cargo;
                const int64 Loaded = FMath::Min(Qty, FMath::Max<int64>(0, V->Capacity.GetRawValue()-Cargo));
                Held += Loaded; Cargo += Loaded; Peak = FMath::Max(Peak, Cargo);
                const auto Supply = Runtime->QueryKnownMarketSupply(Stop.CityId, Action.GoodId);
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
    Snapshot.CreatorReview = FText::Format(LOCTEXT("Review", "Peak cargo (first two circuits)  {0} / {1} units\nRound trip  {2} travel ticks + {3} port ticks\nUpkeep  {4} pfennig / circuit\nKnown minimum cash result  {5} pfennig before trade settlement\nStation and home actions transfer goods without payment. Market actions buy or sell at execution prices. Factor orders settle separately; future proceeds are not guaranteed.\n{6}\n{7}\n{8}"),
        FText::AsNumber(double(Peak)/1000.0), FText::AsNumber(V->Capacity.GetRawValue()/1000), FText::AsNumber(Ticks), FText::AsNumber(DraftStops.Num()), FText::AsNumber(Cost), FText::AsNumber(-Cost),
        Snapshot.bReassignCog ? LOCTEXT("Reassign", "Reassigns this Cog and cancels its stopped route when you activate.") : Assigned ? LOCTEXT("AssignedBusy", "Cog is assigned and unavailable. No route will be changed.") : LOCTEXT("Available", "Cog available; no existing route will be replaced."),
        bUnknown ? LOCTEXT("UnknownReports", "? Stock report unavailable. Delivery quantity cannot be predicted.") : bStale ? LOCTEXT("StaleReports", "! Estimated or older stock reports. Verify supply before departure.") : LOCTEXT("FreshReports", "Current stock reports; supply may change before loading."),
        bCapacityRisk ? LOCTEXT("CapacityRisk", "! Loading will be limited by free capacity.") : bStockRisk ? LOCTEXT("StockRisk", "! Stock above reserve may be insufficient; partial loads are possible.") : LOCTEXT("Reserves", "Minimum reserves are always protected; destination capacity may limit unloading."));
    if (!StationReview.IsEmpty()) Snapshot.CreatorReview = FText::Format(LOCTEXT("BufferedReview", "{0}{1}"), Snapshot.CreatorReview, FText::FromString(StationReview));
    if (Snapshot.DraftName.IsEmpty() || Snapshot.DraftName.Len() > 48 || Snapshot.DraftName.TrimStartAndEnd() != Snapshot.DraftName)
        Snapshot.Validation = LOCTEXT("NameInvalid", "Enter a route name of 1–48 characters without leading or trailing spaces.");
    else if (bOversizedAction) Snapshot.Validation = LOCTEXT("OversizedAction", "Reduce each cargo quantity to the Cog capacity or below, then review again.");
    else if (!bLegs) Snapshot.Validation = LOCTEXT("LegInvalid", "Use at least two alternating Lübeck / Rostock stops. Adjacent duplicate cities cannot be sailed.");
    else if (V->Cargo.GetRawValue() != 0 || (Assigned && (Assigned->Lifecycle != EHansaRouteLifecycleState::Inactive || Assigned->CurrentStopIndex != 0)))
        Snapshot.Validation = LOCTEXT("CogBusy", "Cog unavailable: wait for it to return, unload its cargo and pause the current route at its first stop.");
    else if (DraftStops[0].CityId != V->CurrentCityId) Snapshot.Validation = LOCTEXT("StartCity", "The first stop must be the Cog's current city. Move that stop to the top.");
    else if (!bUnload) Snapshot.Validation = LOCTEXT("NoUnload", "Add an unload action for a good loaded on this circuit so the route can deliver cargo.");
    else
    {
        uint64 Id = 0;
        const FHansaCommandGatewayResult Result = Runtime->CreateTradeRoute(V->Id, DraftStops, Snapshot.DraftName, Snapshot.bReassignCog, true, Id);
        Snapshot.bCanCreate = !!Result;
        Snapshot.Validation = Snapshot.bCanCreate ? LOCTEXT("Valid", "Ready. Review the voyage, then create and activate.")
            : Result.GetError() == EHansaCommandGatewayError::ResearchEffectRequired
                ? LOCTEXT("ReserveResearchRequired", "Complete Reserve instructions research before using a minimum reserve. Open Research, finish the required technology, then review this preserved draft again.")
                : LOCTEXT("Rejected", "The route could not be validated. Check the name, Cog, stops and goods; your draft is preserved.");
    }
}

bool UHansaTradeMapPresentationModel::ReviewCreateIntent()
{
    if (!Snapshot.bCreating) return false;
    const auto Previous = Snapshot; UpdateCreatorReview(); Snapshot.bReview = true;
    Snapshot.EditorStatus = Snapshot.Validation; PublishIfChanged(Previous); return true;
}
bool UHansaTradeMapPresentationModel::EditCreateIntent()
{
    if (!Snapshot.bCreating || !Snapshot.bReview) return false;
    const auto Previous = Snapshot; Snapshot.bReview = false; PublishIfChanged(Previous); return true;
}
bool UHansaTradeMapPresentationModel::CreateAndActivateIntent()
{
    if (!Snapshot.bCreating || !Snapshot.bReview || (!Runtime.IsValid() && !NetworkCommandIntent)) return false;
    const auto Previous = Snapshot; UpdateCreatorReview();
    if (!Snapshot.bCanCreate) { Snapshot.EditorStatus = Snapshot.Validation; PublishIfChanged(Previous); return false; }
	if (NetworkCommandIntent)
	{
		FHansaClientCommandIntent Intent; Intent.Type = EHansaClientIntentType::CreateRoute;
		Intent.VehicleId = Snapshot.CogValue; Intent.RouteName = Snapshot.DraftName;
		Intent.bReassignStoppedVehicle = Snapshot.bReassignCog; AppendCreateIntentStops(DraftStops, Intent);
		const bool bSent = NetworkCommandIntent(Intent);
		if (bSent)
		{
			Snapshot.bCreating = false; Snapshot.bReview = false; Snapshot.bDirty = false; Snapshot.bCanCreate = false;
			Snapshot.EditorStatus = LOCTEXT("CreatePending", "Route creation sent to the authoritative server.");
		}
		else Snapshot.EditorStatus = LOCTEXT("CreateSendFailed", "Route creation could not be sent; your draft is preserved.");
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
#undef LOCTEXT_NAMESPACE
