#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaProductionStockSummary.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Inventory/HansaInventory.h"
#include "Production/HansaProduction.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "World/HansaRuntimeSimulationHost.h"

Hansa::UI::FProductionStockSummary Hansa::UI::SummarizeProductionStock(
    const Hansa::Simulation::FHansaProductionProjection& Production, const Hansa::Simulation::FHansaGoodId& Good,
    TConstArrayView<Hansa::Simulation::FHansaInventoryProjection> Inventories,
    TConstArrayView<Hansa::Simulation::FHansaLogisticsJobProjection> Jobs,
    TConstArrayView<Hansa::Simulation::FHansaRouteProjection> Routes,
    TConstArrayView<Hansa::Simulation::FHansaVehicleProjection> Vehicles)
{
    using namespace Hansa::Simulation;
    FProductionStockSummary Result;
    TSet<FHansaInventoryId> MarketInventories;
    bool InputKnown = false, OutputKnown = false;
    auto Stock = [&](const FHansaInventoryProjection& Inventory)
    {
        const auto* Row = Inventory.Stocks.FindByPredicate([&](const auto& S) { return S.GoodId == Good; });
        return Row ? Row->Stock.GetRawValue() : int64(0);
    };
    for (const auto& Inventory : Inventories)
    {
        InputKnown |= Inventory.Id == Production.InputInventoryId;
        OutputKnown |= Inventory.Id == Production.OutputInventoryId;
        if (Inventory.Id == Production.InputInventoryId || Inventory.Id == Production.OutputInventoryId)
            Result.Building += Stock(Inventory); // Shared input/output inventory is visited only once.
        // City-owned inventories are the shared market pools; production/warehouse buffers are separate.
        if (Production.CityId.IsValid() && Inventory.CityId == Production.CityId &&
            Inventory.OwnerKind == EHansaInventoryOwnerKind::City)
        {
            MarketInventories.Add(Inventory.Id);
            Result.Markets += Stock(Inventory);
        }
    }
    Result.bBuildingKnown = InputKnown && OutputKnown;
    Result.bMarketsKnown = !MarketInventories.IsEmpty();
    for (const auto& Job : Jobs)
        if (Job.GoodId == Good && (MarketInventories.Contains(Job.SourceInventoryId) ||
            MarketInventories.Contains(Job.DestinationInventoryId)))
            Result.Markets += Job.CargoQuantity.GetRawValue(); // Zero before pickup and after delivery, also valid while blocked.
    TSet<FHansaInventoryId> CountedCargo;
    for (const auto& Route : Routes)
    {
        if (Route.Lifecycle != EHansaRouteLifecycleState::Traveling ||
            !Route.Stops.IsValidIndex(Route.CurrentStopIndex) || !Route.Stops.IsValidIndex(Route.NextStopIndex)) continue;
        if (Route.Stops[Route.CurrentStopIndex].CityId != Production.CityId &&
            Route.Stops[Route.NextStopIndex].CityId != Production.CityId) continue;
        const auto* Vehicle = Vehicles.FindByPredicate([&](const auto& V) { return V.Id == Route.VehicleId; });
        if (!Vehicle || CountedCargo.Contains(Vehicle->CargoInventoryId)) continue;
        CountedCargo.Add(Vehicle->CargoInventoryId);
        const auto* Cargo = Inventories.FindByPredicate([&](const auto& I) { return I.Id == Vehicle->CargoInventoryId; });
        if (Cargo) Result.Markets += Stock(*Cargo);
    }
    return Result;
}

void UHansaInspectorPresentationModel::BuildProductionDetail(
    const Hansa::Simulation::FHansaProductionProjection& P,
    const Hansa::Simulation::FHansaEconomicRegistry& Registry)
{
    using namespace Hansa::Simulation;
    auto& D = Snapshot.Production;
    D = {};
    const auto* Recipe = Registry.FindRecipe(P.RecipeId.ToString());
    if (!Recipe || P.CycleTicks <= 0) return;
    D.bValid = true;
    D.bActive = P.bActive;
    D.bCanProgress = P.bActive && P.Blocker == EHansaProductionBlocker::None;
    D.ProgressTicks = P.ProgressTicks;
    D.CycleTicks = P.CycleTicks;
    D.CompletedBatches = P.CompletedCycles;
    D.Laborers = P.AllocatedLaborerWorkforce; D.RequiredLaborers = P.RequiredLaborerWorkforce;
    D.Artisans = P.AllocatedArtisanWorkforce; D.RequiredArtisans = P.RequiredArtisanWorkforce;
    const auto* Host = RuntimeHost.Get();
    D.bSimulationPaused = Host && Host->GetSpeed() == EHansaRuntimeSimulationSpeed::Paused;
    const auto Projection = Host ? Host->BuildProjection()
        : THansaValueResult<FHansaSimulationProjection>::Failure(EHansaValueError::InvalidZero);
    const FHansaBuildingWorldProjection* World = Projection ? Projection.Value.GetBuildingWorldProjections().FindByPredicate(
        [&](const FHansaBuildingWorldProjection& Candidate) { return Candidate.BuildingId == P.BuildingId; }) : nullptr;
    if (World)
    {
        D.bRoadRequired = World->bRequiresRoad;
        D.bHasMarketAccess = World->bHasMarketAccess;
        D.bDeliveryBlocked = World->bDeliveryBlocked;
        D.MarketRoadDistanceCells = World->MarketRoadDistanceCells;
        D.BlockedDeliveryCount = World->BlockedDeliveryCount;
        D.SelectedMarketBuildingValue = static_cast<int64>(World->SelectedMarketBuildingId.GetValue());
        D.MarketAccessCode = FName(Hansa::Simulation::LexToString(
            World->bDeliveryBlocked ? World->DeliveryFailure : World->MarketAccessFailure));
    }
    auto AddPorts = [&](const TArray<FHansaCompiledGoodAmount>& Amounts, FHansaInventoryId Inventory,
        bool Input, TArray<FHansaInspectorProductionPort>& Ports)
    {
        const FHansaInventoryProjection* Stockpile = Projection ? Projection.Value.GetInventories().FindByPredicate(
            [&](const auto& I) { return I.Id == Inventory; }) : nullptr;
        for (const auto& Amount : Amounts)
        {
            FHansaInspectorProductionPort Port;
            Port.GoodId = FName(*Amount.GoodId);
            FString Label = Amount.GoodId; int32 Dot;
            if (Label.FindLastChar('.', Dot)) Label.RightChopInline(Dot + 1);
            Label.ReplaceInline(TEXT("_"), TEXT(" "));
            if(const auto* Good=Registry.FindGood(Amount.GoodId); Good && !Good->DisplayName.IsEmpty())Label=Good->DisplayName;
            Port.Label = FText::FromString(Label); Port.PerBatch = Amount.QuantityMilliUnits;
            if(const auto* Total=P.OutputTotals.FindByPredicate([&](const auto& T){return T.GoodId.ToString()==Amount.GoodId;})) Port.ProducedTotal=Total->QuantityMilliUnits;
            Port.bStockKnown = Stockpile != nullptr;
            if (Stockpile)
            {
                const auto* Stock = Stockpile->Stocks.FindByPredicate([&](const auto& S) { return S.GoodId.ToString() == Amount.GoodId; });
                if (Stock) { Port.Stock = Stock->Stock.GetRawValue(); Port.Available = Stock->Available.GetRawValue(); }
            }
            if (Projection)
            {
                const auto Totals = Hansa::UI::SummarizeProductionStock(P,
                    FHansaGoodId::TryParse(Amount.GoodId).Value, Projection.Value.GetInventories(),
                    Projection.Value.GetLogisticsJobs(), Projection.Value.GetRoutes(), Projection.Value.GetVehicles());
                Port.BuildingStock = Totals.Building; Port.MarketStock = Totals.Markets;
                Port.bBuildingStockKnown = Totals.bBuildingKnown; Port.bMarketStockKnown = Totals.bMarketsKnown;
            }
            // A cycle reserves all its authored inputs before its first progress tick,
            // retaining them until its atomic consume/produce transaction succeeds.
            // This is this production's reservation, never the shared inventory total.
            if (Input && P.ProgressTicks > 0) Port.Reserved = Amount.QuantityMilliUnits;
            Ports.Add(MoveTemp(Port));
        }
    };
    AddPorts(Recipe->Inputs, P.InputInventoryId, true, D.Inputs);
    AddPorts(Recipe->Outputs, P.OutputInventoryId, false, D.Outputs);
}

void UHansaInspectorPresentationModel::ApplyProductionConnectivity()
{
    auto& D = Snapshot.Production;
    if (!D.bValid || !D.bRoadRequired) return;

    FHansaInspectorFlowPresentation Row;
    Row.StableId = TEXT("Inspector.Logistics.MarketAccess");
    Row.Label = FText::FromString(TEXT("Market access"));
    Row.bProblem = !D.bHasMarketAccess || D.bDeliveryBlocked;
    if (D.bHasMarketAccess)
    {
        Row.Value = FText::FromString(FString::Printf(
            TEXT("Market #%lld · %d road cells"),
            static_cast<long long>(D.SelectedMarketBuildingValue), D.MarketRoadDistanceCells));
        Row.State = D.bDeliveryBlocked
            ? FText::FromString(FString::Printf(TEXT("%d delivery blocked"), D.BlockedDeliveryCount))
            : FText::FromString(TEXT("Connected"));
    }
    else
    {
        Row.Value = FText::FromName(D.MarketAccessCode);
        Row.State = NSLOCTEXT("HansaInspector", "MarketNotInRange", "Market not in range");
    }
    Snapshot.Flows.Add(MoveTemp(Row));

    if (D.bHasMarketAccess && !D.bDeliveryBlocked) return;
    Snapshot.Causal.RelatedSemanticId = TEXT("BuildMenu.Category.Roads");
    Snapshot.Causal.Severity = EHansaCausalSeverity::Warning;
    if (D.MarketAccessCode == TEXT("MarketNotInRange"))
    {
        Snapshot.Causal.StableCode = D.MarketAccessCode;
        Snapshot.Causal.Problem = NSLOCTEXT("HansaInspector", "MarketNotInRange", "Market not in range");
        Snapshot.Causal.Cause = NSLOCTEXT("HansaInspector", "MarketRangeCause", "The shortest completed road route exceeds the market's transport range.");
        Snapshot.Causal.Remedy = NSLOCTEXT("HansaInspector", "MarketRangeRemedy", "Build a closer market or shorten the road route. Goods transport resumes when a market is in range.");
        Snapshot.Causal.RelatedSemanticId = TEXT("BuildMenu.Category.Civic");
        return;
    }
    if (D.bDeliveryBlocked)
    {
        Snapshot.Causal.StableCode = TEXT("DeliveryBlocked");
        Snapshot.Causal.Problem = FText::FromString(TEXT("Delivery blocked"));
        Snapshot.Causal.Cause = FText::FromString(TEXT("An active local delivery lost its physical road connection."));
        Snapshot.Causal.Evidence = FText::FromString(FString::Printf(
            TEXT("%d paused delivery job(s); route state %s."),
            D.BlockedDeliveryCount, *D.MarketAccessCode.ToString()));
        Snapshot.Causal.Remedy = FText::FromString(TEXT("Reconnect the broken road segment. The paused cargo will resume on the authoritative route."));
        return;
    }

    Snapshot.Causal.StableCode = D.MarketAccessCode;
    Snapshot.Causal.Evidence = FText::FromString(FString::Printf(
        TEXT("Physical market-access query returned %s."), *D.MarketAccessCode.ToString()));
    if (D.MarketAccessCode == TEXT("NoCompletedRoad") ||
        D.MarketAccessCode == TEXT("SourceNotAdjacentToRoad"))
    {
        Snapshot.Causal.Problem = FText::FromString(TEXT("No adjacent road"));
        Snapshot.Causal.Cause = FText::FromString(TEXT("This building does not touch a completed road."));
        Snapshot.Causal.Remedy = FText::FromString(TEXT("Draw and complete a road next to this building."));
    }
    else if (D.MarketAccessCode == TEXT("NoOperationalMarket"))
    {
        Snapshot.Causal.Problem = FText::FromString(TEXT("No operational market"));
        Snapshot.Causal.Cause = FText::FromString(TEXT("The road network has no completed market hub."));
        Snapshot.Causal.Remedy = FText::FromString(TEXT("Build and complete a market next to the road network."));
    }
    else if (D.MarketAccessCode == TEXT("NoMarketRoadAccess") ||
        D.MarketAccessCode == TEXT("DestinationEndpointUnavailable"))
    {
        Snapshot.Causal.Problem = FText::FromString(TEXT("Market has no road access"));
        Snapshot.Causal.Cause = FText::FromString(TEXT("A market exists, but it is not an operational endpoint on this road network."));
        Snapshot.Causal.Remedy = FText::FromString(TEXT("Complete the market and connect a road beside it."));
    }
    else
    {
        Snapshot.Causal.Problem = FText::FromString(TEXT("Market connection lost"));
        Snapshot.Causal.Cause = FText::FromString(TEXT("This building's road component no longer reaches an operational market."));
        Snapshot.Causal.Remedy = FText::FromString(TEXT("Join this road component to the market road network."));
    }
}

float UHansaInspectorPresentationModel::GetBatchVisualFraction() const
{
    const auto& D = Snapshot.Production;
    if (!D.bValid || D.CycleTicks <= 0) return 0.f;
    const auto* Host = RuntimeHost.Get();
    // Interpolation is decoration only. It freezes with the authoritative clock,
    // never completes a batch early, and never changes quantities or counters.
    const double Fraction = Host && D.bCanProgress && D.ProgressTicks > 0 ? Host->GetPresentationTickFraction() : 0.;
    return FMath::Clamp(static_cast<float>((D.ProgressTicks + Fraction) / D.CycleTicks), 0.f, 1.f);
}

void UHansaInspectorPresentationModel::RefreshProductionClock()
{
    if (!Snapshot.Production.bValid || !RuntimeHost.IsValid()) return;
    const auto Previous = Snapshot;
    Snapshot.Production.bSimulationPaused = RuntimeHost->GetSpeed() == EHansaRuntimeSimulationSpeed::Paused;
    PublishIfChanged(Previous);
}
