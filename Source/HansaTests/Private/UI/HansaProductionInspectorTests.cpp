#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaProductionStockSummary.h"
#include "Widgets/SToolTip.h"
#include "UI/SHansaContextInspector.h"
#include "UI/SHansaProductionInspector.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Production/HansaProduction.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionConstructedBakery,"Hansa.UI.ProductionInspector.ConstructedBakeryLifecycle",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionConstructedBakery::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;
    if (!TestTrue(TEXT("Empty city initializes"), Host->InitializeForLubeck(nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild))) return false;
    FHansaPlacementSpec Spec;
    Spec.CityId = Host->GetCityId();
    Spec.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Bakery")).Value;
    bool Placed = false;
    for (int32 Y=0; Y<60 && !Placed; ++Y) for (int32 X=0; X<60 && !Placed; ++X)
    {
        Spec.Anchor = {X,Y};
        auto Validation = Host->ValidatePlacement(Spec);
        if (!Validation && Validation.GetReasons().Num() == 1 && Validation.GetPrimaryFailure() == EHansaPlacementFailure::RoadRequired)
        {
            auto Road = Spec;
            Road.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
            Road.Anchor.X -= 1;
            if (Host->ValidatePlacement(Road)) Host->PlaceBuildings({Road});
        }
        if (Host->ValidatePlacement(Spec)) Placed = Host->PlaceBuildings({Spec}).IsSuccess();
    }
    if (!TestTrue(TEXT("Bakery placed through player command gateway"), Placed)) return false;
    auto Before = Host->BuildProjection();
    const auto* Bakery = Before.Value.GetBuildingWorldProjections().FindByPredicate([](const auto& B) {
        return B.Placement.BuildingDefinitionId.ToString() == TEXT("Building.Bakery"); });
    if (!TestNotNull(TEXT("Placed bakery is projected"), Bakery)) return false;
    const auto Building = Bakery->BuildingId;
    TStrongObjectPtr<UHansaInspectorPresentationModel> Model(NewObject<UHansaInspectorPresentationModel>());
    Model->InitializeDefaults(); Model->BindRuntime(Host.Get());
    auto Refresh = [&]() { Model->ShowWorldBuilding(TEXT("Building.Bakery"), FText::FromString(TEXT("Bakery")), FText(),
        int64(Building.GetValue()), Host->GetBuildingWorldStatus(Building.GetValue()), TEXT("None"), TEXT("World.Selection")); };
    Refresh();
    auto Widget = SNew(Hansa::UI::SHansaContextInspector).Model(Model.Get());
    TestTrue(TEXT("Construction uses compact construction panel"), Model->GetSnapshot().Production.bConstruction);
    TestTrue(TEXT("Completion tick succeeds"), Host->AdvanceTicks(Host->FindBuildingDefinition(TEXT("Building.Bakery"))->BuildTicks));
    Refresh();
    const auto Completed = Host->BuildProjection();
    const auto* P = Completed.Value.GetProductions().FindByPredicate([&](const auto& V) { return V.BuildingId == Building; });
    if (!TestNotNull(TEXT("Player-built bakery owns a production unit after completion"), P)) return false;
    TestEqual(TEXT("Uses authored bread recipe"), P->RecipeId.ToString(), FString(TEXT("Recipe.BakeBread")));
    TestTrue(TEXT("Workforce comes from city allocation"), P->bUsesCityWorkforce);
    TestTrue(TEXT("Completed bakery uses compact operating panel"), Model->GetSnapshot().Production.bValid && !Model->GetSnapshot().Production.bConstruction);
    TestTrue(TEXT("Native batch ring survives completion"), Widget->ResolveSemanticWidget(TEXT("Inspector.Production.Batch")).IsValid());
    TestTrue(TEXT("Input storage is real"), Model->GetSnapshot().Production.Inputs[0].bStockKnown);
    TestEqual(TEXT("No free input goods are invented"), Model->GetSnapshot().Production.Inputs[0].Stock, int64(0));
    TestTrue(TEXT("Missing workforce or inputs is an actual blocker"), P->Blocker != EHansaProductionBlocker::None);
    const int32 ProductionCount = Completed.Value.GetProductions().Num();
    const int32 InventoryCount = Completed.Value.GetInventories().Num();
    TestTrue(TEXT("Pause action reaches new production"), Host->SetProductionActive(P->Id, false).IsSuccess());
    TestTrue(TEXT("Repeated synchronization advances"), Host->AdvanceTicks(3));
    auto After = Host->BuildProjection();
    TestEqual(TEXT("No duplicate production units"), After.Value.GetProductions().Num(), ProductionCount);
    TestEqual(TEXT("No duplicate buffers"), After.Value.GetInventories().Num(), InventoryCount);
    const auto* Paused = After.Value.GetProductions().FindByPredicate([&](const auto& V) { return V.Id == P->Id; });
    TestTrue(TEXT("Synchronization preserves player pause"), Paused && !Paused->bActive);
    TArray<uint8> Save;
    TestTrue(TEXT("Activated building saves"), Host->CaptureSaveBytes(Save, TEXT("Constructed bakery"), TEXT("2026-09-11T12:00:00Z")).IsSuccess());
    TestTrue(TEXT("Activated building restores"), Host->RestoreSaveBytes(Save).IsSuccess());
    Refresh();
    TestTrue(TEXT("Restored bakery retains compact panel"), Model->GetSnapshot().Production.bValid && !Model->GetSnapshot().Production.bConstruction);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionInspectorRecipes,"Hansa.UI.ProductionInspector.RecipeAndInventoryTruth",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionInspectorRecipes::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error; if(!TestTrue(TEXT("Scenario initializes"),Host->InitializeForLubeck(nullptr,Error))){AddError(Error);return false;}
    Host->AdvanceTicks(3);
    TStrongObjectPtr<UHansaInspectorPresentationModel> Model(NewObject<UHansaInspectorPresentationModel>());
    Model->InitializeDefaults(); Model->BindRuntime(Host.Get());
    auto Projection=Host->BuildProjection();if(!Projection)return false;
    int32 Buildings=0;
    for(const auto& P:Projection.Value.GetProductions())
    {
        if(!P.BuildingId.IsValid())continue;
        const auto* W=Projection.Value.GetBuildingWorldProjections().FindByPredicate([&](const auto& B){return B.BuildingId==P.BuildingId;});
        if(!W)continue;
        Model->ShowWorldBuilding(W->Placement.BuildingDefinitionId.ToString(),FText(),FText(),int64(P.BuildingId.GetValue()),TEXT("Ready"),LexToString(P.Blocker),TEXT("World.Selection"));
        const auto& D=Model->GetSnapshot().Production;
        TestTrue(TEXT("Live building uses rich production data"),D.bValid);
        TestEqual(TEXT("Batch progress is authoritative"),D.ProgressTicks,P.ProgressTicks);
        TestEqual(TEXT("Batch completion count is authoritative"),D.CompletedBatches,P.CompletedCycles);
        const auto* R=Host->GetEconomicRegistry()->FindRecipe(P.RecipeId.ToString());
        TestEqual(TEXT("Every recipe input is present"),D.Inputs.Num(),R->Inputs.Num());
        TestEqual(TEXT("Every recipe output is present"),D.Outputs.Num(),R->Outputs.Num());
        for(const auto& Port:D.Inputs)
        {
            TestTrue(TEXT("Input inventory was joined"),Port.bStockKnown);
            const auto* I=Projection.Value.GetInventories().FindByPredicate([&](const auto& V){return V.Id==P.InputInventoryId;});
            const auto* Stock=I?I->Stocks.FindByPredicate([&](const auto& V){return V.GoodId.ToString()==Port.GoodId.ToString();}):nullptr;
            TestEqual(TEXT("Available excludes every inventory reservation"),Port.Available,Stock?Stock->Available.GetRawValue():int64(0));
        }
        auto Widget=SNew(Hansa::UI::SHansaContextInspector).Model(Model.Get());
        TestTrue(TEXT("Batch has a real native semantic target"),Widget->ResolveSemanticWidget(TEXT("Inspector.Production.Batch")).IsValid());
        TestFalse(TEXT("New production selections start compact, including paused units"),Model->GetSnapshot().bCauseExpanded);
        const auto Nodes=Widget->GetSemanticSnapshot();
        for (bool Input : {true, false}) for (const auto& Port : Input ? D.Inputs : D.Outputs)
        {
            const FString Id = FString(Input ? TEXT("Inspector.Production.Input.") : TEXT("Inspector.Production.Output.")) + Port.GoodId.ToString();
            auto Product = Widget->ResolveSemanticWidget(Id);
            TestTrue(TEXT("Each product has a native hover popup"), Product.IsValid() && Product->GetToolTip().IsValid());
            const auto* Tip = Nodes.FindByPredicate([&](const auto& N) { return N.Id == Id + TEXT(".Tooltip"); });
            TestTrue(TEXT("Popup exposes both counts and starts hidden"), Tip && !Tip->State.bVisible &&
                Tip->State.Value.Contains(TEXT("in storage\n")) && Tip->State.Value.Contains(TEXT("in markets")));
        }

        const auto* Cost=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Production.Cost");});
        const auto* Time=Nodes.FindByPredicate([](const auto& N){return N.Id==TEXT("Inspector.Production.Batch.Time");});
        TestTrue(TEXT("Unknown operating cost has no invented amount"),Cost&&Cost->State.Value.IsEmpty());
        TestTrue(TEXT("Batch time is formatted from real normal-speed duration"),Time&&Time->State.Value==FString::Printf(TEXT("%02d:%02d"),P.CycleTicks/60,P.CycleTicks%60));
        const auto* StockNode=Nodes.FindByPredicate([](const auto& N){return N.Id.EndsWith(TEXT(".Stock"));});
        TestTrue(TEXT("Stock ledger starts collapsed"),StockNode&&!StockNode->State.bVisible);
        Model->OpenCauseIntent();
        const auto Expanded=Widget->GetSemanticSnapshot();
        TestTrue(TEXT("Details reveals stock ledger"),Expanded.ContainsByPredicate([](const auto& N){return N.Id.EndsWith(TEXT(".Stock"))&&N.State.bVisible;}));
        Model->OpenCauseIntent();
        auto Pin=Widget->ResolveSemanticWidget(TEXT("Inspector.Action.Pin"));Model->TogglePinIntent();
        TestTrue(TEXT("Pin updates retain widget identity"),Pin==Widget->ResolveSemanticWidget(TEXT("Inspector.Action.Pin")));
        FName Destination;Model->OnRelatedTargetRequested().AddLambda([&](FName Id){Destination=Id;});
        Model->ActivateAction(TEXT("Inspector.Action.ViewStorage"));TestEqual(TEXT("Storage opens stock overview"),Destination,FName(TEXT("CityOverview.Market")));
        Model->ActivateAction(TEXT("Inspector.Action.OpenRelated"));TestEqual(TEXT("Chain opens production overview"),Destination,FName(TEXT("CityOverview.Production")));
        Model->OnRelatedTargetRequested().Clear();
        ++Buildings;
    }
    TestTrue(TEXT("At least one actual production building was inspected"),Buildings>0);
    // All authored recipe shapes, including sources and multi-input recipes, use the same presenter.
    Model->BindRuntime(nullptr);
    bool Source=false;
    for(const auto& R:Host->GetEconomicRegistry()->GetRecipes())
    {
        FHansaProductionProjection P;P.Id=FHansaProductionId::TryCreate(1).Value;P.BuildingId=FHansaBuildingId::TryCreate(1).Value;
        P.RecipeId=FHansaRecipeId::TryParse(R.StableId).Value;P.CycleTicks=R.CycleTicks;
        if(!Model->ShowProduction(P,*Host->GetEconomicRegistry(),{},TEXT("Test")))continue;
        TestEqual(TEXT("Arbitrary recipe preserves every input"),Model->GetSnapshot().Production.Inputs.Num(),R.Inputs.Num());
        TestEqual(TEXT("Arbitrary recipe preserves every output"),Model->GetSnapshot().Production.Outputs.Num(),R.Outputs.Num());
        for(const auto& Port:Model->GetSnapshot().Production.Outputs)TestFalse(TEXT("Missing inventory is unavailable, not invented zero"),Port.bStockKnown);
        Source|=R.Inputs.IsEmpty();
    }
    TestTrue(TEXT("Source recipes covered"),Source);
    FHansaCompiledRecipeDefinition Multi;
    Multi.StableId=TEXT("Recipe.TestMulti");Multi.CycleTicks=10;
    Multi.Inputs={{TEXT("Good.Grain"),2000},{TEXT("Good.Flour"),1000}};
    Multi.Outputs={{TEXT("Good.Bread"),3000},{TEXT("Good.Flour"),500}};
    FHansaEconomicRegistry Fixture(Host->GetEconomicRegistry()->GetGoods(),{Multi},{},1);
    FHansaProductionProjection P;P.Id=FHansaProductionId::TryCreate(1).Value;
    P.BuildingId=FHansaBuildingId::TryCreate(1).Value;
    P.RecipeId=FHansaRecipeId::TryParse(Multi.StableId).Value;P.CycleTicks=10;
    TestTrue(TEXT("Multi-port recipe can be presented"),Model->ShowProduction(P,Fixture,{},TEXT("Test")));
    TestEqual(TEXT("Both independent input ports retained"),Model->GetSnapshot().Production.Inputs.Num(),2);
    TestEqual(TEXT("Both independent output ports retained"),Model->GetSnapshot().Production.Outputs.Num(),2);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionInspectorClock,"Hansa.UI.ProductionInspector.ClockPauseAndCompletion",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionInspectorClock::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
    if(!Host->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
    TStrongObjectPtr<UHansaInspectorPresentationModel> Model(NewObject<UHansaInspectorPresentationModel>());Model->BindRuntime(Host.Get());
    Host->AdvanceTicks(1);auto Projection=Host->BuildProjection();if(!Projection)return false;
    const auto* P=Projection.Value.GetProductions().FindByPredicate([](const auto& V){return V.BuildingId.IsValid()&&V.ProgressTicks>0&&V.Blocker==EHansaProductionBlocker::None;});
    if(!TestNotNull(TEXT("Scenario has a working batch"),P))return false;
    Model->ShowProduction(*P,*Host->GetEconomicRegistry(),{},TEXT("Test"));
    Host->AdvanceRealTime(.25);const float Running=Model->GetBatchVisualFraction();
    TestTrue(TEXT("Ring interpolates within the authoritative tick"),Running>float(P->ProgressTicks)/P->CycleTicks);
    Host->SetSpeed(EHansaRuntimeSimulationSpeed::Paused);Model->RefreshProductionClock();Host->AdvanceRealTime(.5);
    TestEqual(TEXT("Pause retains exact fractional progress"),Model->GetBatchVisualFraction(),Running);
    TestTrue(TEXT("Paused state is exposed"),Model->GetSnapshot().Production.bSimulationPaused);
    TestEqual(TEXT("Interpolation never invents completed batches"),Model->GetSnapshot().Production.CompletedBatches,P->CompletedCycles);
    Host->AdvanceTicks(P->CycleTicks+1);
    const auto Completed=Host->BuildProjection();
    const auto* Finished=Completed?Completed.Value.GetProductions().FindByPredicate([&](const auto& V){return V.Id==P->Id;}):nullptr;
    TestTrue(TEXT("Real simulation completes and records a batch"),Finished&&Finished->CompletedCycles>P->CompletedCycles);
    if(Finished){Model->ShowProduction(*Finished,*Host->GetEconomicRegistry(),{},TEXT("Test"));TestEqual(TEXT("Panel receives actual completed batch count"),Model->GetSnapshot().Production.CompletedBatches,Finished->CompletedCycles);}
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProductionProductStock,"Hansa.UI.ProductionInspector.ProductStockAndTransit",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProductionProductStock::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    const auto Other=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
    const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
    FHansaProductionProjection P; P.CityId=City;
    P.InputInventoryId=FHansaInventoryId::TryCreate(1).Value;
    P.OutputInventoryId=FHansaInventoryId::TryCreate(2).Value;
    TArray<FHansaInventoryProjection> Inventories;
    auto Inventory=[&](uint64 Id, EHansaInventoryOwnerKind Kind, FHansaCityDefinitionId OwnerCity, int64 Amount)
    {
        FHansaInventoryProjection I;I.Id=FHansaInventoryId::TryCreate(Id).Value;I.OwnerKind=Kind;I.CityId=OwnerCity;
        FHansaInventoryStockProjection S;S.GoodId=Grain;S.Stock=FHansaQuantity::FromRaw(Amount);
        S.Available=FHansaQuantity::FromRaw(0); // Everything reserved: physical stock still counts.
        I.Stocks.Add(S);Inventories.Add(I);
    };
    Inventory(1,EHansaInventoryOwnerKind::Building,City,2000);
    Inventory(2,EHansaInventoryOwnerKind::Building,City,1000);
    Inventory(3,EHansaInventoryOwnerKind::City,City,5000);
    Inventory(4,EHansaInventoryOwnerKind::City,Other,99000);
    Inventory(5,EHansaInventoryOwnerKind::City,City,1000);
    Inventory(6,EHansaInventoryOwnerKind::Warehouse,City,77000);
    TArray<FHansaLogisticsJobProjection> Jobs;
    auto Job=[&](uint64 Source,uint64 Destination,int64 Cargo)
    {
        FHansaLogisticsJobProjection J;J.GoodId=Grain;J.SourceInventoryId=FHansaInventoryId::TryCreate(Source).Value;
        J.DestinationInventoryId=FHansaInventoryId::TryCreate(Destination).Value;
        J.Quantity=FHansaQuantity::FromRaw(10000);J.CargoQuantity=FHansaQuantity::FromRaw(Cargo);Jobs.Add(J);
    };
    Job(3,1,0); // Awaiting pickup: source stock already includes the reserved load.
    Job(3,1,2000);Job(2,5,3000);Job(3,5,1000); // Both directions and market-to-market, counted once.
    Job(4,6,9000); // Unrelated transport.
    TArray<FHansaRouteProjection> Routes;TArray<FHansaVehicleProjection> Vehicles;
    auto Read=[&](){return Hansa::UI::SummarizeProductionStock(P,Grain,Inventories,Jobs,Routes,Vehicles);};
    auto Totals=Read();
    TestTrue(TEXT("Inventories are known"),Totals.bBuildingKnown&&Totals.bMarketsKnown);
    TestEqual(TEXT("Building storage includes both buffers and reservations"),Totals.Building,int64(3000));
    TestEqual(TEXT("All city markets plus loaded inbound and outbound cargo"),Totals.Markets,int64(12000));
    // Delivery moves cargo into the market without changing its combined quantity.
    Jobs[2].CargoQuantity=FHansaQuantity::FromRaw(0);
    Inventories[4].Stocks[0].Stock=FHansaQuantity::FromRaw(4000);
    TestEqual(TEXT("Delivery does not double-count cargo"),Read().Markets,int64(12000));
    Inventory(7,EHansaInventoryOwnerKind::Vehicle,Other,4000);
    FHansaVehicleProjection V;V.Id=FHansaVehicleId::TryCreate(1).Value;V.CargoInventoryId=Inventories.Last().Id;Vehicles.Add(V);
    FHansaRouteProjection R;R.VehicleId=V.Id;R.Lifecycle=EHansaRouteLifecycleState::Traveling;
    FHansaRouteStop A;A.CityId=Other;FHansaRouteStop B;B.CityId=City;R.Stops={A,B};Routes.Add(R);
    TestEqual(TEXT("Actual inbound ship cargo is included"),Read().Markets,int64(16000));
    Routes[0].CurrentStopIndex=1;Routes[0].NextStopIndex=0;
    TestEqual(TEXT("Actual outbound ship cargo is included"),Read().Markets,int64(16000));
    Routes[0].Lifecycle=EHansaRouteLifecycleState::Cancelled;
    TestEqual(TEXT("Cancelled routes are not in transit"),Read().Markets,int64(12000));
    P.OutputInventoryId=P.InputInventoryId;
    TestEqual(TEXT("Shared production buffer is counted once"),Read().Building,int64(2000));
    Inventories.Reset();Jobs.Reset();Routes.Reset();Vehicles.Reset();Totals=Read();
    TestFalse(TEXT("Missing building inventory is unknown"),Totals.bBuildingKnown);
    TestFalse(TEXT("Missing city inventory is unknown"),Totals.bMarketsKnown);
    return !HasAnyErrors();
}
#endif
