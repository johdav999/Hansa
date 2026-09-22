#include "Misc/AutomationTest.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaDefinitionBase.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "World/HansaLubeckPlacementGrid.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "Definitions/HansaPopulationDefinitions.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "UObject/StrongObjectPtr.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UI/HansaInspectorPresentationModel.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodCandidateTest, "Hansa.Integration.Firewood.CatalogPolicyAndSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodCandidateTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    auto& Assets=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    Assets.ScanPathsSynchronous({TEXT("/Game/Hansa/Generated/Staging/Firewood")},true);
    TArray<FAssetData> Rows;Assets.GetAssetsByPath(TEXT("/Game/Hansa/Generated/Staging/Firewood"),Rows,true,false);
    TArray<const UHansaDefinitionBase*> Definitions;
    for(const auto& Row:Rows) if(const auto* D=Cast<UHansaDefinitionBase>(Row.GetAsset())) Definitions.Add(D);
    const auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Definitions);
    if(!TestTrue(TEXT("Staged catalog compiles"),Compiled.IsValid()))return false;
    TestEqual(TEXT("Fourteen goods"),Compiled.Registry.GetGoods().Num(),14);
    TestEqual(TEXT("Draft registry matches review manifest"),Compiled.Registry.GetRegistryHash(),FHansaLubeckScenarioInitializer::FirewoodCandidateRegistryHash);
    const auto* Heating=Compiled.Registry.FindNeed(TEXT("Need.Heating"));
    if(!TestNotNull(TEXT("Heating authored"),Heating))return false;
    TestTrue(TEXT("Heating cannot consume food substitutes"),Heating->Alternatives.IsEmpty());
    for(const TCHAR* Id:{TEXT("Recipe.BakeBread"),TEXT("Recipe.MaltGrain"),TEXT("Recipe.BrewBeer")})
    {
        const auto* Recipe=Compiled.Registry.FindRecipe(Id);
        if(!TestNotNull(Id,Recipe))return false;
        TestEqual(TEXT("Exactly one firewood input"),Recipe->Inputs.FilterByPredicate([](const auto& I){return I.GoodId==TEXT("Good.Firewood");}).Num(),1);
    }
    const auto* Yard=Compiled.Registry.FindBuilding(TEXT("Building.WoodcutterYard"));
    if(!TestNotNull(TEXT("Yard authored"),Yard))return false;
    TestEqual(TEXT("No artisan dependency"),Yard->ArtisanWorkforce,0);
    TestTrue(TEXT("Yard is constructible"),Yard->bShowInConstructionMenu);
    for(const auto& Building:Compiled.Registry.GetBuildings())if(Building.ResidenceCapacity>0)
    {
        const auto* Tier=Compiled.Registry.FindPopulationTier(Building.ResidentPopulationTierId);
        TestTrue(*FString::Printf(TEXT("%s has exactly one cohort-tier heating need"),*Building.StableId),Tier && Tier->Needs.FilterByPredicate([](const auto& N){return N.NeedId==TEXT("Need.Heating");}).Num()==1);
    }
    FHansaLubeckScenarioState Scenario;FString Error;
    const auto Placement=Hansa::Game::LubeckPlacementGrid::TryBuildInitialization(FHansaHouseId::TryCreate(1).Value,Compiled.Registry);
    if(!TestTrue(TEXT("Normal scenario initializes"),Placement.IsSuccess() && FHansaLubeckScenarioInitializer::TryCreate(EHansaRuntimeScenario::LubeckGrainShortage,Compiled.Registry,Placement.Value,Scenario,Error))) {AddError(Error);return false;}
    FHansaSimulationTransientCache Cache;
    auto Command=[&](uint64 Seq,FHansaHouseId Owner,int32 Days,bool Release)
    {
        FHansaCommandHeader H;H.CommandId=FHansaCommandId::TryCreate(Seq).Value;H.GlobalSequence=Seq;
        H.Authority.IssuingHouseId=Owner;H.Authority.PrincipalId=1;
        H.RequestedExecutionTick=Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).GetClock().GetTick();
        const TArray<FHansaGameplayCommand> Commands={FHansaGameplayCommand::Create(H,FHansaSetHeatingReserveCommand{FHansaBuildingId::TryCreate(14).Value,Days,Release})};
        return FHansaGameplayCommandGateway::ExecuteTick(Scenario.State,Scenario.Definitions,Commands,Cache);
    };
    const auto Before=Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).BuildStateHashReport().GetOverallHash();
    TestFalse(TEXT("Other owner cannot change policy"),Command(1,FHansaHouseId::TryCreate(2).Value,9,true).IsSuccess());
    TestEqual(TEXT("Rejected policy is atomic"),Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).BuildStateHashReport().GetOverallHash(),Before);
    TestFalse(TEXT("Invalid days rejected"),Command(1,Scenario.HouseId,91,false).IsSuccess());
    TestTrue(TEXT("Market owner can release reserve"),Command(1,Scenario.HouseId,7,true).IsSuccess());
    auto H=Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).QueryHeating(Scenario.CityId);
    TestTrue(TEXT("Building workshop demand derives city from placement"),H.WorkshopDailyRaw>0);
    TestEqual(TEXT("Authored days changed"),H.ReserveDays,7);TestTrue(TEXT("Override visible"),H.bOverride);
    FHansaSaveSnapshot Snapshot,Restored;Snapshot.State=Scenario.State;Snapshot.BuildVersion=TEXT("Firewood-review");
    Snapshot.SavedUtc=TEXT("2026-09-16T00:00:00Z");Snapshot.DisplayName=TEXT("Firewood policy");Snapshot.Players.Add({1,Scenario.HouseId});
    TArray<uint8> Bytes;
    TestTrue(TEXT("Policy saves"),FHansaSaveEnvelope::Encode(Snapshot,Scenario.Definitions,Bytes).IsSuccess());
    TestTrue(TEXT("Policy reloads"),FHansaSaveEnvelope::Decode(Bytes,Scenario.Definitions,Restored).IsSuccess());
    auto View=Restored.State.CreateReadOnlyAccess(Scenario.Definitions);
    TestEqual(TEXT("Roundtrip state hash"),View.BuildStateHashReport().GetOverallHash(),Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).BuildStateHashReport().GetOverallHash());
    TestEqual(TEXT("Reserve days persisted"),View.QueryHeating(Scenario.CityId).ReserveDays,7);
    TestTrue(TEXT("Override persisted"),View.QueryHeating(Scenario.CityId).bOverride);
    TestTrue(TEXT("Restore protection through command"),Command(2,Scenario.HouseId,3,false).IsSuccess());
    TestFalse(TEXT("Protection restored"),Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).QueryHeating(Scenario.CityId).bOverride);
    // Ordinary construction and delivery path; this is a bounded functional fixture,
    // not evidence of long-term balance (the baseline scenario includes development grants).
    FHansaCommandHeader BuildHeader;BuildHeader.CommandId=FHansaCommandId::TryCreate(3).Value;BuildHeader.GlobalSequence=3;
    BuildHeader.Authority.IssuingHouseId=Scenario.HouseId;BuildHeader.Authority.PrincipalId=1;
    BuildHeader.RequestedExecutionTick=Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).GetClock().GetTick();
    const auto YardId=FHansaBuildingId::TryCreate(1000).Value;
    bool Placed=false;
    for(int Y=15;Y<=16 && !Placed;++Y) for(int X=14;X<=23 && !Placed;++X)
    {
        FHansaPlacementSpec Spec;Spec.CityId=Scenario.CityId;Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.WoodcutterYard")).Value;Spec.Anchor={X,Y};
        const TArray<FHansaGameplayCommand> Commands={FHansaGameplayCommand::Create(BuildHeader,FHansaPlaceBuildingCommand{YardId,Spec})};
        const auto Result=FHansaGameplayCommandGateway::ExecuteTick(Scenario.State,Scenario.Definitions,Commands,Cache);
        Placed=Result.IsSuccess();
        if(Placed) AddInfo(FString::Printf(TEXT("Yard placed at %d,%d"),X,Y));
        else AddInfo(FString::Printf(TEXT("Placement %d,%d: %s / %s"),X,Y,LexToString(Result.GetError()),Result.GetPlacementValidation().IsSet()?LexToString(Result.GetPlacementValidation()->GetPrimaryFailure()):TEXT("no placement error")));
    }
    if(!TestTrue(TEXT("Yard placed through normal construction gateway"),Placed))return false;
    for(int Tick=0;Tick<800;++Tick)
        if(!FHansaGameplayCommandGateway::ExecuteTick(Scenario.State,Scenario.Definitions,{},Cache).IsSuccess()){AddError(TEXT("Construction playthrough tick failed"));return false;}
    const auto Built=Scenario.State.CreateReadOnlyAccess(Scenario.Definitions).BuildProjection();
    if(!TestTrue(TEXT("Constructed scenario projects"),Built.IsSuccess()))return false;
    const auto* Output=Built.Value.GetProductions().FindByPredicate([&](const auto& P){return P.BuildingId==YardId;});
    if(!TestNotNull(TEXT("Yard starts normal recipe production"),Output))return false;
    AddInfo(FString::Printf(TEXT("Yard cycles=%lld blocker=%s"),static_cast<long long>(Output->CompletedCycles),LexToString(Output->Blocker)));
    TestTrue(TEXT("Delivered timber produces firewood"),Output->CompletedCycles>0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodSchemaTest, "Hansa.Integration.Firewood.SeasonalAuthoringContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodSchemaTest::RunTest(const FString&)
{
    FHansaEditorSchemaRegistry Registry;
    const auto Schema=Registry.BuildSchemaForClass(UHansaNeedDefinition::StaticClass());
    TestTrue(TEXT("Seasonal metadata complete"),Schema.IsValid());
    TestEqual(TEXT("Need schema version"),Schema.SchemaVersion,2);
    for(const TCHAR* Name:{TEXT("bSeasonal"),TEXT("SeasonDays"),TEXT("FixedSeason"),TEXT("DefaultReserveDays"),TEXT("SeasonMultipliers")})
        TestTrue(Name,Schema.Properties.ContainsByPredicate([&](const auto& P){return P.Name==Name && P.ReflectedProperty && !P.Migration.IsEmpty();}));
    FFileHelper::SaveStringToFile(Registry.ExportJsonSchema(Schema),*(FPaths::ProjectSavedDir()/TEXT("GenerationJobs/Firewood_20260916/need-v2.schema.json")));
    auto* Need=NewObject<UHansaNeedDefinition>();Need->StableDefinitionId=TEXT("Need.Heating");Need->GoodId=TEXT("Good.Firewood");Need->bSeasonal=true;
    for(int Mode=0;Mode<5;++Mode)
    {
        Need->SeasonDays=90;Need->FixedSeason=-1;Need->DefaultReserveDays=3;Need->SeasonMultipliers={0,4000,10000,4000};
        if(Mode==0)Need->SeasonDays=0;
        if(Mode==1)Need->FixedSeason=4;
        if(Mode==2)Need->DefaultReserveDays=91;
        if(Mode==3)Need->SeasonMultipliers.Pop();
        if(Mode==4)Need->SeasonMultipliers[0]=10001;
        TArray<FHansaDefinitionValidationIssue> Issues;Need->ValidateDefinition(Issues);
        TestTrue(TEXT("Invalid seasonal authoring rejected"),Issues.ContainsByPredicate([](const auto& I){return I.Code==TEXT("HSA-NEED-SEASON");}));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodControlsTest,"Hansa.Integration.Firewood.SemanticReserveControls",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodControlsTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    const FString Original=FCommandLine::Get();
    FCommandLine::Set(*(Original+TEXT(" -FirewoodCandidate")));
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;const bool Ready=Host->InitializeForLubeck(nullptr,Error);
    FCommandLine::Set(*Original);
    if(!TestTrue(*Error,Ready))return false;
    TStrongObjectPtr<UHansaInspectorPresentationModel> Inspector(NewObject<UHansaInspectorPresentationModel>());
    Inspector->InitializeDefaults();Inspector->BindRuntime(Host.Get());
    Inspector->ShowWorldBuilding(TEXT("Building.Market"),FText::FromString(TEXT("Market")),FText(),14,TEXT("Ready"),TEXT(""),TEXT("Firewood.Test"));
    TestTrue(TEXT("Semantic increase routes through player action"),Inspector->ActivateAction(TEXT("Inspector.Heating.Increase")));
    TestEqual(TEXT("Policy changed authoritatively"),Host->QueryHeating().ReserveDays,4);
    TestTrue(TEXT("Semantic release"),Inspector->ActivateAction(TEXT("Inspector.Heating.Override")));
    TestTrue(TEXT("Release visible in projection"),Host->QueryHeating().bOverride);
    TestTrue(TEXT("Semantic restore"),Inspector->ActivateAction(TEXT("Inspector.Heating.Override")));
    TestFalse(TEXT("Protection restored"),Host->QueryHeating().bOverride);
    TestTrue(TEXT("Semantic decrease"),Inspector->ActivateAction(TEXT("Inspector.Heating.Decrease")));
    TestEqual(TEXT("Default days restored"),Host->QueryHeating().ReserveDays,3);
    return !HasAnyErrors();
}
#endif
