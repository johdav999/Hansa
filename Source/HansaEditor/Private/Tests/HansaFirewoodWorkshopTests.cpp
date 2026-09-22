#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaDefinitionBase.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"
#include "Model/HansaSimulationState.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodWorkshopTest,"Hansa.Integration.Firewood.WorkshopFuelAccounting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodWorkshopTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    auto& Assets=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Core")},true);
    TArray<FAssetData> Rows;Assets.GetAssetsByPath(TEXT("/Game/Hansa/Core"),Rows,true,false);
    TArray<const UHansaDefinitionBase*> Definitions;
    for(const auto& Row:Rows)if(const auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset()))Definitions.Add(D);
    const auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Definitions);
    if(!TestTrue(TEXT("Reloaded accepted catalog compiles"),Compiled.IsValid()))return false;
    const auto& R=Compiled.Registry;
    const auto Context=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.FirewoodAccounting")).Value,R.GetRegistryHash(),R);
    if(!TestTrue(TEXT("Accounting context valid"),Context.IsSuccess()))return false;
    const auto Owner=FHansaHouseId::TryCreate(1).Value;
    const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    const auto Fuel=FHansaGoodId::TryParse(TEXT("Good.Firewood")).Value;
    for(const TCHAR* Type:{TEXT("Building.Bakery"),TEXT("Building.MaltHouse"),TEXT("Building.Brewery")})
    for(const int FuelBatches:{0,2})
    {
        const auto* Building=R.FindBuilding(Type);check(Building && Building->RecipeIds.Num()==1);
        const auto* Recipe=R.FindRecipe(Building->RecipeIds[0]);check(Recipe);
        const auto* FuelInput=Recipe->Inputs.FindByPredicate([](const auto& A){return A.GoodId==TEXT("Good.Firewood");});
        if(!TestNotNull(TEXT("Workshop authors fuel input"),FuelInput))return false;
        // Isolated accounting fixture: explicit full staff, no households,
        // logistics, background supply or markets can alter its finite inputs.
        FHansaSimulationInitialization Init;
        Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value,60).Value;
        Init.CampaignSeed=991;Init.Houses.Add({Owner,FHansaMoney::FromRaw(100000)});Init.Cities.Add({City,{}});
        FHansaBuildingState B;B.Id=FHansaBuildingId::TryCreate(1).Value;B.DefinitionId=FHansaBuildingTypeId::TryParse(Type).Value;B.OwnerId=Owner;
        B.ConstructionProgress=FHansaRate::FromPartsPerMillion(FHansaRate::Scale);B.ConstructionState=EHansaConstructionState::Completed;Init.Buildings.Add(B);
        FHansaInventoryInitialization I;I.Id=FHansaInventoryId::TryCreate(1).Value;I.OwnerKind=EHansaInventoryOwnerKind::City;I.CityId=City;I.Capacity=FHansaQuantity::FromRaw(1000000);
        for(const auto& G:R.GetGoods())I.AcceptedGoods.Add(FHansaGoodId::TryParse(G.StableId).Value);
        for(const auto& A:Recipe->Inputs)
        {
            const int64 Amount=A.QuantityMilliUnits*(A.GoodId==TEXT("Good.Firewood")?FuelBatches:5);
            if(Amount>0)I.InitialStock.Add({FHansaGoodId::TryParse(A.GoodId).Value,FHansaQuantity::FromRaw(Amount)});
        }
        Init.Inventories.Add(I);
        FHansaProductionInitialization P;P.Id=FHansaProductionId::TryCreate(1).Value;P.BuildingId=B.Id;
        P.RecipeId=FHansaRecipeId::TryParse(Recipe->StableId).Value;P.InputInventoryId=P.OutputInventoryId=I.Id;
        P.AllocatedLaborerWorkforce=FMath::Max(Building->LaborerWorkforce,Recipe->LaborerWorkforce);
        P.AllocatedArtisanWorkforce=FMath::Max(Building->ArtisanWorkforce,Recipe->ArtisanWorkforce);Init.Productions.Add(P);
        const auto Created=FHansaSimulationState::TryCreate(Init);
        if(!TestTrue(TEXT("Accounting fixture valid"),Created.IsSuccess()))return false;
        auto State=Created.Value;FHansaSimulationTransientCache Cache;uint64 Sequence=0;
        int32 FrozenProgress=0;
        for(int T=1;T<=Recipe->CycleTicks*3+30;++T)
        {
            TArray<FHansaGameplayCommand> Commands;
            if(T==2 || T==27)
            {
                FHansaCommandHeader H;H.CommandId=FHansaCommandId::TryCreate(++Sequence).Value;H.GlobalSequence=Sequence;H.Authority.IssuingHouseId=Owner;H.Authority.PrincipalId=1;
                H.RequestedExecutionTick=State.CreateReadOnlyAccess(Context.Value).GetClock().GetTick();
                Commands.Add(FHansaGameplayCommand::Create(H,FHansaSetProductionActiveCommand{P.Id,T==27}));
            }
            if(!TestTrue(TEXT("Accounting tick succeeds"),FHansaGameplayCommandGateway::ExecuteTick(State,Context.Value,Commands,Cache).IsSuccess()))return false;
            const auto View=State.CreateReadOnlyAccess(Context.Value);const auto Projection=View.BuildProjection();
            if(!TestTrue(TEXT("Accounting projection valid"),Projection.IsSuccess()))return false;
            const auto& Production=Projection.Value.GetProductions()[0];
            if(T==1)FrozenProgress=Production.ProgressTicks;
            if(T>=2 && T<27)
            {
                TestEqual(TEXT("Paused batch progress remains fixed"),Production.ProgressTicks,FrozenProgress);
                TestEqual(TEXT("Paused workshop reports inactive"),Production.Blocker,EHansaProductionBlocker::Inactive);
            }
            for(const auto& A:Recipe->Inputs)
            {
                const auto Stock=View.GetInventories().QueryStock(I.Id,FHansaGoodId::TryParse(A.GoodId).Value);
                const int64 Initial=A.QuantityMilliUnits*(A.GoodId==TEXT("Good.Firewood")?FuelBatches:5);
                TestEqual(TEXT("Every input consumed exactly once per completed batch"),Stock.IsSet()?Stock->Stock.GetRawValue():0,Initial-static_cast<int64>(Production.CompletedCycles)*A.QuantityMilliUnits);
            }
            if(T==Recipe->CycleTicks*3+30)
            {
                TestEqual(TEXT("Finite fuel bounds completed batches"),Production.CompletedCycles,static_cast<uint64>(FuelBatches));
                TestEqual(TEXT("Fuel exhaustion blocks further production"),Production.Blocker,EHansaProductionBlocker::MissingInput);
                TestEqual(TEXT("Blocker identifies fuel"),Production.BlockingGoodId,Fuel);
                for(const auto& A:Recipe->Outputs)
                {
                    const auto Stock=View.GetInventories().QueryStock(I.Id,FHansaGoodId::TryParse(A.GoodId).Value);
                    TestEqual(TEXT("Outputs match completed batches only"),Stock.IsSet()?Stock->Stock.GetRawValue():0,static_cast<int64>(FuelBatches)*A.QuantityMilliUnits);
                }
                AddInfo(FString::Printf(TEXT("%s: %d fuel batches, %llu completions; pause/resume and exhausted-fuel conservation checked each tick."),Type,FuelBatches,Production.CompletedCycles));
            }
        }
    }
    return !HasAnyErrors();
}
#endif
