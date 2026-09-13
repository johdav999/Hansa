#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION
#include "Misc/AutomationTest.h"
#include "Fixtures/HansaProductionFixture.h"
#include "UI/HansaHudPresentationModel.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTopMenuProjectionTest,"Hansa.UI.HUD.TopMenu.AuthoritativeCityAndMonth",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaTopMenuProjectionTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    auto Created=FHansaProductionFixture::TryCreateGrainShortage();
    if(!TestTrue(TEXT("Fixture created"),bool(Created)))return false;
    auto Fixture=Created.Value;
    auto Initial=Fixture.BuildProjection();if(!Initial)return false;
    const auto Home=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    const auto Remote=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
    const auto House=FHansaHouseId::TryCreate(1).Value;
    TStrongObjectPtr<UHansaHudPresentationModel> Model(NewObject<UHansaHudPresentationModel>());
    Model->InitializeDefaults();Model->ApplyRuntimeStatus(Initial.Value,Home,House);
    TestEqual(TEXT("Incomplete monthly history is unknown"),Model->GetSnapshot().MoneyTrend.ToString(),FString(TEXT("—")));
    for(int Day=0;Day<30;++Day){
        if(!Fixture.Step(24))return false;
        auto P=Fixture.BuildProjection();if(!P)return false;
        Model->ApplyRuntimeStatus(P.Value,Home,House);
    }
    const auto Current=Fixture.BuildProjection();if(!Current)return false;
    const auto* Before=Initial.Value.GetHouses().FindByPredicate([&](const auto& H){return H.Id==House;});
    const auto* After=Current.Value.GetHouses().FindByPredicate([&](const auto& H){return H.Id==House;});
    if(!Before||!After)return false;
    double Delta=(double(After->Money.GetRawValue())-double(Before->Money.GetRawValue()))/1000.;
    FNumberFormattingOptions Format;Format.SetMaximumFractionalDigits(2);
    FString Expected=(Delta>0?TEXT("+"):TEXT(""))+FText::AsNumber(Delta,&Format).ToString();
    TestEqual(TEXT("Month is actual treasury difference"),Model->GetSnapshot().MoneyTrend.ToString(),Expected);
    const auto HomeCash=Model->GetSnapshot().Money.ToString();
    for(auto CityId:{Home,Remote}){
        Model->ApplyRuntimeStatus(Current.Value,CityId,House);
        const auto& S=Model->GetSnapshot();
        TestEqual(TEXT("Treasury remains player wide across cities"),S.Money.ToString(),HomeCash);
        const auto* City=Current.Value.GetCityPopulations().FindByPredicate([&](const auto& C){return C.CityId==CityId;});
        if(City){
            TestEqual(TEXT("Population is current city's residents"),S.Population.ToString(),FText::AsNumber(City->TotalResidents).ToString());
            TestEqual(TEXT("Laborers are residents, not workforce"),S.Workforce.ToString(),FText::AsNumber(City->LaborerResidents).ToString());
            TestEqual(TEXT("Wealthy maps to artisan residents"),S.WealthyCitizens.ToString(),FText::AsNumber(City->ArtisanResidents).ToString());
        }
        TestEqual(TEXT("Only Bread is tracked in every city"),S.TopProducts.Num(),1);
        TestEqual(TEXT("Bread remains selected regardless of output rank"),S.TopProducts[0].GoodId,FName(TEXT("Good.Bread")));
        TestTrue(TEXT("Product includes name and balance"),S.TopProducts[0].Value.ToString().StartsWith(TEXT("Bread ")));
        TestTrue(TEXT("Rate unit is explicit"),S.TopProducts[0].Tooltip.ToString().Contains(TEXT("per game day")));

    }
    Model->ApplyRuntimeStatus(Initial.Value,Home,House);
    TestEqual(TEXT("Time rewind discards incompatible history"),Model->GetSnapshot().MoneyTrend.ToString(),FString(TEXT("—")));
    Model->ResetCashHistory();
    TestEqual(TEXT("Load reset immediately clears the old delta"),Model->GetSnapshot().MoneyTrend.ToString(),FString(TEXT("—")));
    return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaBreadBalanceTest,"Hansa.UI.HUD.TopMenu.BreadBalance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaBreadBalanceTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    FHansaCityMarketProjection M; M.UpdateCadenceTicks=4;M.LastUpdateTick=8;M.bIsStale=false;
    FHansaMarketPriceHistoryEntry E;E.LocalProduction=FHansaQuantity::FromRaw(2000);E.CitizenDemand=FHansaQuantity::FromRaw(250);
    M.PriceHistory={E,E};
    auto Value=[&]{return UHansaHudPresentationModel::BuildBreadBalance(&M,60).Value.ToString();};
    TestEqual(TEXT("2 units over four hours minus .25/hour is +6/day"),Value(),FString(TEXT("Bread +6")));
    M.PriceHistory[0].LocalProduction=FHansaQuantity();M.PriceHistory[1].LocalProduction=FHansaQuantity();
    TestEqual(TEXT("Demand without production remains a deficit"),Value(),FString(TEXT("Bread -6")));
    for(auto& Entry:M.PriceHistory)Entry.LocalProduction=FHansaQuantity::FromRaw(1000);
    TestEqual(TEXT("Balanced flows display zero"),Value(),FString(TEXT("Bread 0")));
    M.bDemandPerReport=true;
    TestEqual(TEXT("Background demand uses report units"),Value(),FString(TEXT("Bread +"))+FText::AsNumber(4.5).ToString());
    TestEqual(TEXT("Faster authored calendar normalizes the same flow"),UHansaHudPresentationModel::BuildBreadBalance(&M,30).Value.ToString(),FString(TEXT("Bread +9")));
    M.CurrentStock=FHansaQuantity::FromRaw(900000);M.ExpectedIncomingSupply=FHansaQuantity::FromRaw(900000);
    TestEqual(TEXT("Stock and incoming shipments cannot inflate the rate"),Value(),FString(TEXT("Bread +"))+FText::AsNumber(4.5).ToString());
    M.bDemandPerReport=false;
    for(auto& Entry:M.PriceHistory){Entry.LocalProduction=FHansaQuantity::FromRaw(2000);Entry.IndustrialDemand=FHansaQuantity::FromRaw(250);}
    TestEqual(TEXT("Industrial demand is included"),Value(),FString(TEXT("Bread 0")));
    M.PriceHistory[0].LocalProduction=FHansaQuantity::FromRaw(4000);M.PriceHistory[1].LocalProduction=FHansaQuantity();
    TestEqual(TEXT("Batch spikes are averaged across reports"),Value(),FString(TEXT("Bread 0")));
    M.PriceHistory.Init(E,7);M.PriceHistory[0].LocalProduction=FHansaQuantity::FromRaw(900000);
    TestEqual(TEXT("Reports older than one day leave the window"),Value(),FString(TEXT("Bread +6")));
    M.PriceHistory.Reset();TestEqual(TEXT("No report keeps Bread pending"),Value(),FString(TEXT("Bread —")));
    TestEqual(TEXT("Missing market keeps Bread pending"),UHansaHudPresentationModel::BuildBreadBalance(nullptr,60).Value.ToString(),FString(TEXT("Bread —")));
    return !HasAnyErrors();
}
#endif
