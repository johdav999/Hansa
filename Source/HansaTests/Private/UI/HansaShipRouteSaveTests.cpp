#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeEstablishmentTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteInheritedReserveSaveTest, "Hansa.TradeRoute.UI.InheritedReserveSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShipRouteInheritedReserveSaveTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;
    if (!TestTrue(TEXT("Initialize starter campaign"), Host->InitializeForLubeck(nullptr, Error))) return false;
    TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
    Model->InitializeDefaults(); Model->BindRuntime(Host.Get());
    Model->ApplyProjection(Host->BuildProjection().Value, *Host->GetEconomicRegistry());
    Model->Open(); Model->SelectRouteIntent(1);
    const auto Before = Host->BuildProjection().Value.GetRoutes()[0];
    TestEqual(TEXT("Starter Rostock grain reserve"), Before.Stops[1].Actions[0].MinimumSourceReserve.GetRawValue(), int64(30000));
    TestTrue(TEXT("Open Lubeck slot 2 load"), Model->OpenCargoCell(0, 1, true));
    TestTrue(TEXT("Choose planks"), Model->SetCargoProduct(TEXT("Good.Planks")));
    TestTrue(TEXT("Load ten planks"), Model->SetCargoQuantity(10000));
    TestTrue(TEXT("Confirm plank instruction"), Model->ConfirmCargoCell());
    TestTrue(TEXT("Save plank edit with inherited grain reserve"), Model->CommitIntent());
    TestFalse(TEXT("Save clears dirty state"), Model->GetSnapshot().bDirty);
    const auto Saved = Host->BuildProjection().Value.GetRoutes()[0];
    TestTrue(TEXT("Plank instruction persisted"), Saved.Stops[0].Actions.ContainsByPredicate([](const auto& A)
        { return A.CargoSlotIndex == 1 && A.GoodId.ToString() == TEXT("Good.Planks") && A.QuantityLimit.GetRawValue() == 10000; }));
    TestEqual(TEXT("Inherited reserve preserved"), Saved.Stops[1].Actions[0].MinimumSourceReserve.GetRawValue(), int64(30000));
    TestTrue(TEXT("Open existing reserved grain load"), Model->OpenCargoCell(1, 0, true));
    TestTrue(TEXT("Increase grain reserve"), Model->SetCargoQuantity(31000, true));
    TestTrue(TEXT("Confirm reserve increase in draft"), Model->ConfirmCargoCell());
    TestFalse(TEXT("New reserve remains research gated"), Model->CommitIntent());
    const FString Failure = Model->GetSnapshot().EditorStatus.ToString();
    TestTrue(TEXT("Failure explains research requirement"), Failure.Contains(TEXT("Reserve instructions research")));
    Model->ApplyProjection(Host->BuildProjection().Value, *Host->GetEconomicRegistry());
    TestEqual(TEXT("Projection refresh preserves save failure"), Model->GetSnapshot().EditorStatus.ToString(), Failure);
    TestTrue(TEXT("Rejected draft remains editable"), Model->GetSnapshot().bDirty);
    TestEqual(TEXT("Rejected reserve does not alter saved route"), Host->BuildProjection().Value.GetRoutes()[0].Stops[1].Actions[0].MinimumSourceReserve.GetRawValue(), int64(30000));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShipRouteMovingSaveTest, "Hansa.TradeRoute.UI.SaveWhileMoving",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShipRouteMovingSaveTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    using namespace Hansa::Multiplayer;
    for (bool Remote : {false, true})
    {
        TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
        FString Error;
        if (!TestTrue(TEXT("Initialize campaign"), Host->InitializeForLubeck(nullptr, Error))) return false;
        if (!TestTrue(TEXT("Restore loaded sailing Cog"), Hansa::Tests::PrepareEstablishment(*Host, Error, 500000,
            [&](FHansaSimulationInitialization& Init)
            {
                const auto Bread = FHansaGoodId::TryParse(TEXT("Good.Bread")).Value;
                const auto Planks = FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
                auto& Vehicle = Init.Vehicles[0];
                Vehicle.CurrentCityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
                Vehicle.Capacity = FHansaQuantity::FromRaw(60000);
                Init.Inventories[0].Capacity = Vehicle.Capacity;
                Init.Inventories[0].AcceptedGoods = {Bread, Planks};
                Init.Inventories[0].InitialStock = {{Bread, FHansaQuantity::FromRaw(30000)}};
                FHansaRouteCargoAction Load;
                Load.Kind = EHansaRouteCargoActionKind::Load;
                Load.GoodId = Bread; Load.CargoSlotIndex = 0;
                Load.QuantityLimit = FHansaQuantity::FromRaw(30000);
                auto Unload = Load; Unload.Kind = EHansaRouteCargoActionKind::Unload;
                FHansaRouteState Route;
                Route.Id = FHansaRouteId::TryCreate(1).Value;
                Route.OwnerId = Vehicle.OwnerId; Route.VehicleId = Vehicle.Id;
                Route.RouteDefinitionId = FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;
                Route.Mode = EHansaRouteMode::Sea;
                Route.Stops = {{Vehicle.CurrentCityId, {Load}},
                    {FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value, {Unload}}};
                Route.Lifecycle = EHansaRouteLifecycleState::Traveling;
                Route.TotalTravelTicks = 8; Route.RemainingTravelTicks = 5;
                Route.Progress = FHansaRate::TryRatio(3, 8).Value;
                Init.Routes.Add(Route);
                // Populate real city inventory and reported markets used by the selector.
                for (int32 Index = 0; Index < 2; ++Index)
                {
                    auto& Stock = *Init.Inventories.FindByPredicate([&](const auto& I){return I.OwnerKind == EHansaInventoryOwnerKind::City && I.CityId == Route.Stops[Index].CityId;});
                    Stock.Capacity = FHansaQuantity::FromRaw(500000);
                    Stock.AcceptedGoods.Add(Bread); Stock.AcceptedGoods.Add(Planks);
                    Stock.InitialStock = {{Bread, FHansaQuantity::FromRaw(100000)}, {Planks, FHansaQuantity::FromRaw(100000)}};
                    for (const auto Good : {Bread, Planks})
                    {
                        FHansaCityMarketInitialization Market;
                        Market.CityId = Stock.CityId; Market.GoodId = Good;
                        Market.InventoryIds = {Stock.Id}; Market.InitialPriceMilliMarks = 1000; Market.InitialReportTick = 0;
                        Init.Markets.Add(Market);
                    }
                }
            }))) { AddError(Error); return false; }
        TestTrue(TEXT("Evaluate fixture market stock before replication"), Host->AdvanceTicks(1));
        TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
        Model->InitializeDefaults(); Model->SetViewerHouse(Host->GetHouseId());
        FHansaMultiplayerAuthority Authority;
        if (Remote)
        {
            Authority.Initialize(*Host);
            FHansaClientInterest Interest; Interest.CityIds = {TEXT("City.Lubeck"), TEXT("City.Rostock")};
            if (!TestTrue(TEXT("Admit owner"), Authority.RegisterAdmittedClient(
                {928, FHansaParticipantId::TryCreate(928).Value, Host->GetHouseId(), EHansaAdmissionMode::LanOffline}, Interest, Error))) return false;
            Model->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Draft)
            {
                auto Intent = Draft; Intent.ClientSequence = Authority.GetExpectedClientSequence(928);
                Intent.ClientNonce = 92800 + Intent.ClientSequence;
                FHansaClientCommandFeedback Pending; Pending.State = EHansaClientCommandState::Pending;
                Pending.ClientSequence = Intent.ClientSequence; Pending.ClientNonce = Intent.ClientNonce;
                Model->ReceiveCommandFeedback(Pending);
                const auto Result = Authority.SubmitIntent(928, Intent);
                TestTrue(*(Result.Message + TEXT(" / ") + Result.Remedy), Result.bAccepted);
                Model->ReceiveCommandFeedback(Result);
                return true;
            });
        }
        else Model->BindRuntime(Host.Get());
        auto Refresh = [&]
        {
            if (Remote) { FHansaClientProjectionSnapshot Wire; Authority.BuildProjection(928, 0, true, Wire, Error); for(const auto& Market:Wire.Markets)if(Market.GoodId==TEXT("Good.Planks"))AddInfo(FString::Printf(TEXT("Remote plank report: %s stock=%lld"),*Market.CityId,Market.StockMilliUnits)); Model->ApplyRemoteEstablishment(Wire); }
            else Model->ApplyProjection(Host->BuildProjection().Value, *Host->GetEconomicRegistry());
        };
        Refresh(); Model->Open(); Model->SelectRouteIntent(1);
        auto Screen = SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get()).InitialViewportSize(FIntPoint(1536, 1024));
        const auto Before = Host->BuildProjection().Value;
        TestTrue(TEXT("Open moving Cog slot 2 load"), Screen->ActivateSemanticId(TEXT("TradeMap.Cargo.0.1.Load")));
        for(const auto& Choice:Model->CargoEditor.Products)if(Choice.GoodId.ToString()==TEXT("Good.Planks"))AddInfo(FString::Printf(TEXT("Remote=%d plank choice enabled=%d reported=%lld: %s"),Remote,Choice.bEnabled,Choice.Reported,*Choice.Detail.ToString()));
        TestTrue(TEXT("Choose planks"), Model->SetCargoProduct(TEXT("Good.Planks")));
        TestTrue(TEXT("Load twenty planks"), Model->SetCargoQuantity(20000));
        TestTrue(TEXT("Confirm load"), Model->ConfirmCargoCell());
        TestTrue(TEXT("Open destination slot 2 unload"), Screen->ActivateSemanticId(TEXT("TradeMap.Cargo.1.1.Unload")));
        TestTrue(TEXT("Choose planned planks"), Model->SetCargoProduct(TEXT("Good.Planks")));
        TestTrue(TEXT("Unload up to thirty planks"), Model->SetCargoQuantity(30000));
        TestTrue(TEXT("Confirm unload"), Model->ConfirmCargoCell());
        TestTrue(TEXT("Save through ship footer"), Screen->ActivateSemanticId(TEXT("TradeMap.Ship.Save")));
        TestFalse(TEXT("Acknowledged save clears dirty state"), Model->GetSnapshot().bDirty);
        const auto After = Host->BuildProjection().Value;
        const auto& OldRoute = Before.GetRoutes()[0]; const auto& SavedRoute = After.GetRoutes()[0];
        TestEqual(TEXT("Sailing lifecycle preserved"), SavedRoute.Lifecycle, OldRoute.Lifecycle);
        TestEqual(TEXT("Current stop preserved"), SavedRoute.CurrentStopIndex, OldRoute.CurrentStopIndex);
        TestEqual(TEXT("Destination preserved"), SavedRoute.NextStopIndex, OldRoute.NextStopIndex);
        TestEqual(TEXT("Remaining voyage preserved"), SavedRoute.RemainingTravelTicks, OldRoute.RemainingTravelTicks - 1);
        TestEqual(TEXT("Total voyage preserved"), SavedRoute.TotalTravelTicks, OldRoute.TotalTravelTicks);
        TestTrue(TEXT("Progress continues on same leg"), SavedRoute.Progress == FHansaRate::TryRatio(SavedRoute.TotalTravelTicks - SavedRoute.RemainingTravelTicks, SavedRoute.TotalTravelTicks).Value);
        TestEqual(TEXT("Physical bread cargo preserved"), After.GetVehicles()[0].Cargo.GetRawValue(), int64(30000));
        TestEqual(TEXT("Physical slot 2 stays empty until loading"), After.GetVehicles()[0].CargoSlots[1].Quantity.GetRawValue(), int64(0));
        TestTrue(TEXT("Plank load saved"), SavedRoute.Stops[0].Actions.ContainsByPredicate([](const auto& A){ return A.CargoSlotIndex == 1 && A.GoodId.ToString() == TEXT("Good.Planks") && A.QuantityLimit.GetRawValue() == 20000; }));
        TestTrue(TEXT("Plank unload saved"), SavedRoute.Stops[1].Actions.ContainsByPredicate([](const auto& A){ return A.CargoSlotIndex == 1 && A.GoodId.ToString() == TEXT("Good.Planks") && A.QuantityLimit.GetRawValue() == 30000; }));
        auto Reordered = SavedRoute.Stops; Reordered.Swap(0, 1);
        const auto Hash = After.GetFingerprint().Value;
        TestFalse(TEXT("Reordering moving route remains rejected"), Host->EditRoute(SavedRoute.Id, Reordered).IsSuccess());
        TestEqual(TEXT("Rejected edit rolls back completely"), Host->BuildProjection().Value.GetFingerprint().Value, Hash);
        TArray<uint8> Bytes;
        TestTrue(TEXT("Save mid-voyage"), Host->CaptureSaveBytes(Bytes, TEXT("Route edit"), TEXT("2026-09-28T00:00:00Z")).IsSuccess());
        TestTrue(TEXT("Reload mid-voyage"), Host->RestoreSaveBytes(Bytes).IsSuccess());
        TestEqual(TEXT("Reload preserves route edit and voyage"), Host->BuildProjection().Value.GetFingerprint().Value, Hash);
        TestTrue(TEXT("Continue to next stop"), Host->AdvanceTicks(SavedRoute.RemainingTravelTicks));
        const auto Arrival = Host->BuildProjection().Value;
        TestEqual(TEXT("Cog reaches original destination"), Arrival.GetRoutes()[0].CurrentStopIndex, 1);
        TestTrue(TEXT("Execute edited instructions at next stop"), Host->AdvanceTicks(1));
        const auto Applied = Host->BuildProjection().Value;
        TestEqual(TEXT("Next stop uses saved plank instruction"), Applied.GetRoutes()[0].LastTransfer.GoodId.ToString(), FString(TEXT("Good.Planks")));
        TestEqual(TEXT("Next stop requests saved unload quantity"), Applied.GetRoutes()[0].LastTransfer.RequestedQuantity.GetRawValue(), int64(30000));
        TestEqual(TEXT("Bread is unloaded at next stop"), Applied.GetVehicles()[0].CargoSlots[0].Quantity.GetRawValue(), int64(0));
    }
    return !HasAnyErrors();
}
#endif
