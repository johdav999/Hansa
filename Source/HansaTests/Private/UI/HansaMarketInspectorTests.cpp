#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaMarketInspectorTestSupport.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/SHansaContextInspector.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UObject/StrongObjectPtr.h"
#include "Population/HansaPopulation.h"
#include "Definitions/HansaEconomicRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMarketInspectorTest,"Hansa.UI.MarketInspector.DemandAndSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaMarketInspectorTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>()); FString Error;
    if(!TestTrue(TEXT("Initialize"),Host->InitializeForLubeck(nullptr,Error)))return false;
    if(!TestTrue(TEXT("Construct market through gateway"),EnsureMarketInspectorTestBuilding(Host.Get())))return false;
    Host->AdvanceTicks(5);const auto P=Host->BuildProjection();
    if(!TestTrue(TEXT("Population projection"),P && !P.Value.GetPopulationCohorts().IsEmpty()))return false;
    auto A=P.Value.GetPopulationCohorts()[0];
    if(!TestTrue(TEXT("Evaluated needs"),!A.Needs.IsEmpty()))return false;
    const auto* Registry=Host->GetEconomicRegistry();
    const auto* First=A.Needs.FindByPredicate([](const auto& N){return N.GoodId.IsValid();});
    if(!TestNotNull(TEXT("Good need"),First))return false;
    auto Need=*First;
    A.Residents=10;
    for(auto& N:A.Needs){N.RequiredLastTick=FHansaQuantity::FromRaw(0);N.ConsumedLastTick=FHansaQuantity::FromRaw(0);}
    for(auto& N:A.Needs)if(N.NeedId==Need.NeedId){N.RequiredLastTick=FHansaQuantity::FromRaw(1000);N.ConsumedLastTick=FHansaQuantity::FromRaw(1000);}
    auto B=A;
    for(auto& N:B.Needs)if(N.NeedId==Need.NeedId){N.RequiredLastTick=FHansaQuantity::FromRaw(3000);N.ConsumedLastTick=FHansaQuantity::FromRaw(0);}
    auto Other=A;Other.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
    TArray<FHansaPopulationCohortProjection> Cohorts={A,B,Other};
    FHansaConsumptionProjection Consumption;
    Consumption.CoveredMinutes=6*1440;
    Consumption.Goods={{A.CityId,Need.GoodId,4000,1000},{Other.CityId,Need.GoodId,1000,1000}};
    auto Rows=BuildMarketDemandFlows(Cohorts,*Registry,A.CityId.ToString(),Consumption);
    auto Find=[&]()->const FHansaInspectorFlowPresentation*{return Rows.FindByPredicate([&](const auto& R){return R.DemandGoodId==FName(*Need.GoodId.ToString());});};
    if(!TestNotNull(TEXT("Good row"),Find()))return false;
    TestEqual(TEXT("Only selected city demand"),Find()->DemandRequired,int64(4000));
    TestEqual(TEXT("Recorded consumption replaces latest tick"),Find()->DemandSupplied,int64(1000));
    TestEqual(TEXT("Weighted fulfillment"),Find()->State.ToString(),FText::AsPercent(.25).ToString());
    TestTrue(TEXT("Partial supply warning"),Find()->bProblem);
    TestTrue(TEXT("Partial coverage labeled"),Find()->DemandPeriod.ToString().Contains(TEXT("6")));
    Consumption.Goods[0].Required=10000;Consumption.Goods[0].Consumed=7000;Consumption.bFullWindow=true;Consumption.CoveredMinutes=43200;
    Rows=BuildMarketDemandFlows({},*Registry,A.CityId.ToString(),Consumption);
    TestEqual(TEXT("Historical demand survives removal of residences"),Find()->State.ToString(),FText::AsPercent(.7).ToString());
    TestEqual(TEXT("Full window label"),Find()->DemandPeriod.ToString(),FString(TEXT("Last 30 days")));
    Consumption.Goods[0].Consumed=0;Rows=BuildMarketDemandFlows(Cohorts,*Registry,A.CityId.ToString(),Consumption);
    TestTrue(TEXT("Measured zero is known"),Find()->bDemandKnown && Find()->DemandSupplied==0);
    Consumption.Goods[0].Required=0;Rows=BuildMarketDemandFlows(Cohorts,*Registry,A.CityId.ToString(),Consumption);
    TestTrue(TEXT("No demand has no percentage"),Find()->bDemandKnown && Find()->DemandRequired==0 && Find()->State.ToString().Contains(TEXT("—")));
    Consumption={};Rows=BuildMarketDemandFlows(Cohorts,*Registry,A.CityId.ToString(),Consumption);
    TestFalse(TEXT("No history is pending"),Find()->bDemandKnown);

    const auto* Market=P.Value.GetBuildingWorldProjections().FindByPredicate([](const auto& V){return V.Placement.BuildingDefinitionId.ToString()==TEXT("Building.Market");});
    if(!TestNotNull(TEXT("World market"),Market))return false;
    TStrongObjectPtr<UHansaInspectorPresentationModel> M(NewObject<UHansaInspectorPresentationModel>());
    M->InitializeDefaults();M->BindRuntime(Host.Get());
    auto Show=[&]{M->ShowWorldBuilding(TEXT("Building.Market"),FText::FromString(TEXT("Market")),FText(),int64(Market->BuildingId.GetValue()),TEXT("Operating"),TEXT("None"),TEXT("Test"));};
    Show();TestTrue(TEXT("Market has distinct inspector routing"),M->GetSnapshot().Kind==EHansaInspectorObjectKind::Market);
    auto W=SNew(Hansa::UI::SHansaContextInspector).Model(M.Get());
    TestFalse(TEXT("Market begins compact"),M->GetSnapshot().bCauseExpanded);
    TestTrue(TEXT("Market lists demand"),!M->GetSnapshot().Flows.IsEmpty());
    if(!M->GetSnapshot().Flows.IsEmpty()){
        const auto Id=TEXT("Inspector.Flows.Item.")+M->GetSnapshot().Flows[0].StableId.ToString().Replace(TEXT("."),TEXT("_"));
        auto Before=W->ResolveSemanticWidget(Id);
        TestTrue(TEXT("Demand row exists and receives controller focus"),Before.IsValid()&&W->FocusSemanticId(Id));
        Show();TestTrue(TEXT("Refresh preserves row instance"),Before==W->ResolveSemanticWidget(Id));
    }
    TestTrue(TEXT("Details opens"),W->ActivateSemanticId(TEXT("Inspector.Action.OpenCause"))&&M->GetSnapshot().bCauseExpanded);
    TestTrue(TEXT("Details closes"),W->ActivateSemanticId(TEXT("Inspector.Action.OpenCause"))&&!M->GetSnapshot().bCauseExpanded);
    return !HasAnyErrors();
}
#endif
