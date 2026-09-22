#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"
#include "Model/HansaSimulationState.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodBalanceTest,"Hansa.Integration.Firewood.SeasonalBalanceMeasurement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodBalanceTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    auto& Assets=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")},true);
    TArray<FAssetData> Rows;Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"),Rows,true,false);
    TArray<const UHansaDefinitionBase*> Definitions;
    for(const auto& Row:Rows)if(const auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset()))Definitions.Add(D);
    const auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Definitions);
    if(!TestTrue(TEXT("Accepted catalog compiles"),Compiled.IsValid()))return false;
    const auto& R=Compiled.Registry;
    const auto Owner=FHansaHouseId::TryCreate(1).Value;
    const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    FHansaSimulationInitialization Init;
    Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value,60).Value;
    Init.CampaignSeed=0xF1AE20260916ULL;
    Init.Houses.Add({Owner,FHansaMoney::FromRaw(50000)});Init.Cities.Add({City,{}});
    // Dedicated operating-capacity fixture, not a construction-grant opening:
    // completed buildings, 50 initial laborers, explicit finite stocks, no trade,
    // background markets, AI, recurring grants, or preallocated worker bypass.
    FHansaPlacementMapInitialization Map;Map.CityId=City;Map.BoundsMin={0,0};Map.BoundsMax={49,24};
    Map.RoadBuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
    for(int X=0;X<50;++X)for(int Y=0;Y<25;++Y)Map.Cells.Add({{X,Y},EHansaPlacementTerrain::Land,Owner,false});
    for(int X=14;X<22;++X)Map.TreeCells.Add({X,14});
    Init.Placement.Maps.Add(Map);
    auto AddBuilding=[&](uint64 Id,const TCHAR* Type,int X,int Y)
    {
        const auto* D=R.FindBuilding(Type);check(D);
        FHansaBuildingState B;B.Id=FHansaBuildingId::TryCreate(Id).Value;B.DefinitionId=FHansaBuildingTypeId::TryParse(Type).Value;B.OwnerId=Owner;
        B.ConstructionProgress=FHansaRate::FromPartsPerMillion(FHansaRate::Scale);B.ConstructionState=EHansaConstructionState::Completed;Init.Buildings.Add(B);
        FHansaPlacedBuildingRecord P;P.BuildingId=B.Id;P.OwnerId=Owner;P.Spec={City,B.DefinitionId,{X,Y},EHansaGridRotation::North};
        for(int DX=0;DX<D->FootprintWidthCells;++DX)for(int DY=0;DY<D->FootprintHeightCells;++DY)P.OccupiedCells.Add({X+DX,Y+DY});
        Init.Placement.Placements.Add(P);return B;
    };
    auto Inventory=[&](uint64 Id,EHansaInventoryOwnerKind Kind,FHansaBuildingId Building,int64 Capacity)
    {
        FHansaInventoryInitialization I;I.Id=FHansaInventoryId::TryCreate(Id).Value;if(Kind==EHansaInventoryOwnerKind::City)I.CityId=City;I.BuildingId=Building;I.OwnerKind=Kind;I.Capacity=FHansaQuantity::FromRaw(Capacity);
        for(const auto& G:R.GetGoods())I.AcceptedGoods.Add(FHansaGoodId::TryParse(G.StableId).Value);
        return I;
    };
    const auto Market=AddBuilding(1,TEXT("Building.Market"),14,5);
    auto Pool=Inventory(1,EHansaInventoryOwnerKind::City,Market.Id,2000000);
    for(const auto& G:TArray<FHansaCompiledGoodAmount>{{TEXT("Good.Bread"),60000},{TEXT("Good.Fish"),40000},{TEXT("Good.Grain"),30000},{TEXT("Good.Flour"),20000},{TEXT("Good.Timber"),12000},{TEXT("Good.Firewood"),20000},{TEXT("Good.Tools"),4000}})
        Pool.InitialStock.Add({FHansaGoodId::TryParse(G.GoodId).Value,FHansaQuantity::FromRaw(G.QuantityMilliUnits)});
    Init.Inventories.Add(Pool);
    for(const auto& G:R.GetGoods())
    {
        FHansaCityMarketInitialization M;M.CityId=City;M.GoodId=FHansaGoodId::TryParse(G.StableId).Value;M.InventoryIds={Pool.Id};
        M.InitialPriceMilliMarks=FMath::Max<int64>(1,G.BaseValueMilliMarks);M.DesiredReserve=FHansaQuantity::FromRaw(10000);
        // A functioning market record is required for household access. These
        // records have zero background production, demand and incoming supply.
        Init.Markets.Add(M);
    }
    // Every endpoint touches the same road; actual travel time and four-cart
    // concurrency are retained. Homes are immediately north of this road.
    for(int X=2;X<=40;++X)AddBuilding(100+X,TEXT("Building.Road"),X,8);
    for(int N=0;N<5;++N)
    {
        const auto* D=R.FindBuilding(TEXT("Building.Residence.Laborer"));
        const auto Home=AddBuilding(10+N,TEXT("Building.Residence.Laborer"),(N<3?2+N*4:26+(N-3)*4),8-D->FootprintHeightCells);
        FHansaPopulationCohortInitialization C;C.Id=FHansaPopulationCohortId::TryCreate(10+N).Value;C.ResidenceBuildingId=Home.Id;C.CityId=City;C.ConsumptionInventoryId=Pool.Id;
        C.TierId=FHansaPopulationTierId::TryParse(TEXT("PopulationTier.Laborer")).Value;C.Residents=10;C.ResidenceCapacity=D->ResidenceCapacity;Init.PopulationCohorts.Add(C);
    }
    struct Workshop{const TCHAR* Building;const TCHAR* Recipe;int X;};
    const Workshop Workshops[]={{TEXT("Building.LumberCamp"),TEXT("Recipe.FellTimber"),16},{TEXT("Building.GrainFarm"),TEXT("Recipe.GrowGrain"),6},{TEXT("Building.Mill"),TEXT("Recipe.MillFlour"),10},{TEXT("Building.Bakery"),TEXT("Recipe.BakeBread"),13},{TEXT("Building.Fishery"),TEXT("Recipe.CatchFish"),22},{TEXT("Building.WoodcutterYard"),TEXT("Recipe.SplitFirewood"),19}};
    uint64 Id=20;
    for(const auto& W:Workshops)
    {
        const auto* D=R.FindBuilding(W.Building);
        if(!TestNotNull(W.Building,D))return false;
        // Resolve the actual authored recipe from the building, avoiding aliases.
        if(!TestEqual(TEXT("One recipe per balance workshop"),D->RecipeIds.Num(),1))return false;
        const auto B=AddBuilding(Id,W.Building,W.X,9);
        const auto Buffer=Inventory(200+Id,EHansaInventoryOwnerKind::Building,B.Id,D->StorageCapacityMilliUnits);Init.Inventories.Add(Buffer);
        FHansaProductionInitialization P;P.Id=FHansaProductionId::TryCreate(Id).Value;P.BuildingId=B.Id;P.RecipeId=FHansaRecipeId::TryParse(D->RecipeIds[0]).Value;
        P.InputInventoryId=P.OutputInventoryId=Buffer.Id;P.bUsesCityWorkforce=true;Init.Productions.Add(P);++Id;
    }
    const auto Topology=FHansaPlacementTopology::TryCreate(Init.Placement.Maps);
    if(!TestTrue(TEXT("Balance map valid"),Topology.IsSuccess()))return false;
    const auto Context=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.FirewoodBalance")).Value,R.GetRegistryHash(),R,Topology.Value);
    const auto Created=FHansaSimulationState::TryCreate(Init);
    if(!TestTrue(TEXT("Balance state valid"),Created.IsSuccess())||!TestTrue(TEXT("Balance context valid"),Context.IsSuccess()))return false;
    auto State=Created.Value;FHansaSimulationTransientCache Cache;
    FString Csv=TEXT("tick,day,residents,satisfaction,firewoodRaw,householdDailyRaw,protectedRaw,surplusRaw,yardCycles,yardBlocker,breadRaw\n");
    int32 WinterMinimum=MAX_int32;int64 WinterOpening=0;uint64 Sequence=0;
    for(int T=1;T<=360*24;++T)
    {
        TArray<FHansaGameplayCommand> Commands;
        if(T==200*24 || T==207*24)
        {
            FHansaCommandHeader H;H.CommandId=FHansaCommandId::TryCreate(++Sequence).Value;H.GlobalSequence=Sequence;H.Authority.IssuingHouseId=Owner;H.Authority.PrincipalId=1;
            H.RequestedExecutionTick=State.CreateReadOnlyAccess(Context.Value).GetClock().GetTick();
            Commands.Add(FHansaGameplayCommand::Create(H,FHansaSetProductionActiveCommand{FHansaProductionId::TryCreate(25).Value,T==207*24}));
        }
        const auto Step=FHansaGameplayCommandGateway::ExecuteTick(State,Context.Value,Commands,Cache);
        if(!TestTrue(TEXT("Seasonal simulation tick succeeds"),Step.IsSuccess()))return false;
        const auto View=State.CreateReadOnlyAccess(Context.Value);const auto Population=View.QueryCityPopulation(City);const auto H=View.QueryHeating(City);
        if(T==180*24)WinterOpening=H.StockRaw;
        if(T>=180*24 && T<270*24 && Population.IsSet())WinterMinimum=FMath::Min(WinterMinimum,Population->TotalResidents);
        if(T%24==0)
        {
            const auto Projection=View.BuildProjection();const auto* Yard=Projection.Value.GetProductions().FindByPredicate([](const auto& P){return P.BuildingId.GetValue()==25;});
            const auto Bread=View.GetInventories().QueryStock(Pool.Id,FHansaGoodId::TryParse(TEXT("Good.Bread")).Value);
            Csv+=FString::Printf(TEXT("%d,%d,%d,%d,%lld,%lld,%lld,%lld,%llu,%s,%lld\n"),T,T/24,Population.IsSet()?Population->TotalResidents:0,Population.IsSet()?Population->SatisfactionBasisPoints:0,H.StockRaw,H.HouseholdDailyRaw,H.ProtectedRaw,H.SurplusRaw,Yard?Yard->CompletedCycles:0,Yard?LexToString(Yard->Blocker):TEXT("Absent"),Bread.IsSet()?Bread->Stock.GetRawValue():0);
        }
    }
    FFileHelper::SaveStringToFile(Csv,*(FPaths::ProjectSavedDir()/TEXT("GenerationJobs/Firewood_20260916/seasonal-balance.csv")));
    AddInfo(FString::Printf(TEXT("Measured winter minimum residents=%d, winter opening firewood=%lld milliunits; seven-day yard outage at day 200."),WinterMinimum,WinterOpening));
    TestTrue(TEXT("Target fifty residents survives winter with bounded interruption"),WinterMinimum>=50);
    const auto Final=State.CreateReadOnlyAccess(Context.Value).QueryCityPopulation(City);
    TestTrue(TEXT("Target fifty residents retained through spring"),Final.IsSet() && Final->TotalResidents>=50);
    return !HasAnyErrors();
}
#endif
