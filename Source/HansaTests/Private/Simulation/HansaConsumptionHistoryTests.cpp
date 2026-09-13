#include "Misc/AutomationTest.h"
#include "Population/HansaConsumptionHistory.h"
#include "Population/HansaPopulation.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaConsumptionWindowTest, "Hansa.Simulation.Population.RollingConsumption",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaConsumptionWindowTest::RunTest(const FString&)
{
    using namespace Hansa::Simulation;
    const auto City = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    const auto OtherCity = FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
    const auto Bread = FHansaGoodId::TryParse(TEXT("Good.Bread")).Value;
    for (const uint16 MinutesPerTick : {uint16(60), uint16(90), uint16(1440)})
    {
        auto Clock = FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,
            FHansaSimulationTick(), MinutesPerTick).Value;
        FHansaConsumptionHistory History, Reversed;
        TestEqual(TEXT("No fabricated history at start"), History.Project(Clock).CoveredMinutes, int64(0));
        const int32 Window = FHansaConsumptionHistory::WindowTicks(MinutesPerTick);
        for (int32 Tick = 1; Tick <= Window + 1; ++Tick)
        {
            Clock = Clock.TryAdvance(FHansaSimulationDuration::TryCreate(1).Value).Value;
            FHansaPopulationCohortState A; A.CityId = City;
            FHansaPopulationNeedState N; N.GoodId = Bread;
            // The first tick is a large fulfilled demand; subsequent ticks are only 70% supplied.
            N.RequiredLastTick = FHansaQuantity::FromRaw(Tick == 1 ? 100000 : 1000);
            N.ConsumedLastTick = FHansaQuantity::FromRaw(Tick == 1 ? 100000 : 700);
            A.Needs.Add(N);
            FHansaPopulationCohortState B = A; B.CityId = OtherCity;
            B.Needs[0].ConsumedLastTick = FHansaQuantity::FromRaw(0);
            // Services must never be counted as a product.
            FHansaPopulationNeedState Service; Service.RequiredLastTick = FHansaQuantity::FromRaw(9999);
            A.Needs.Add(Service);
            TestTrue(TEXT("Record consumption"), History.Record(TArray<FHansaPopulationCohortState>{A, B}, Clock));
            TestTrue(TEXT("Reverse discovery order"), Reversed.Record(TArray<FHansaPopulationCohortState>{B, A}, Clock));
            if (Tick == 2)
            {
                const auto Partial = History.Project(Clock);
                TestFalse(TEXT("Partial window is explicit"), Partial.bFullWindow);
                TestEqual(TEXT("Weighted numerator"), Partial.Goods[0].Consumed, int64(100700));
                TestEqual(TEXT("Weighted denominator"), Partial.Goods[0].Required, int64(101000));
            }
        }
        const auto P = History.Project(Clock), R = Reversed.Project(Clock);
        TestTrue(TEXT("Complete window"), P.bFullWindow);
        TestEqual(TEXT("Clock cadence defines thirty days"), P.CoveredMinutes, int64(43200));
        TestEqual(TEXT("Cities remain separate; services excluded"), P.Goods.Num(), 2);
        TestEqual(TEXT("Old high-demand tick expired"), P.Goods[0].Required, int64(Window) * 1000);
        TestEqual(TEXT("70 percent of retained demand fulfilled"), P.Goods[0].Consumed, int64(Window) * 700);
        TestEqual(TEXT("Canonical order"), P.Goods[0].Consumed, R.Goods[0].Consumed);
        TestEqual(TEXT("Other city shortage stays separate"), P.Goods[1].Consumed, int64(0));
        TestTrue(TEXT("Save invariants hold"), History.Validate(Clock));
        TestFalse(TEXT("Repeated tick rejected"), History.Record({}, Clock));
        TestEqual(TEXT("Rejected update is atomic"), History.Project(Clock).Goods[0].Consumed, P.Goods[0].Consumed);
        // Removing every residence cannot erase earlier demand. It expires naturally.
        Clock = Clock.TryAdvance(FHansaSimulationDuration::TryCreate(1).Value).Value;
        TestTrue(TEXT("Empty population still records elapsed time"), History.Record({}, Clock));
        TestEqual(TEXT("Historical demand survives removal"), History.Project(Clock).Goods[0].Required, int64(Window - 1) * 1000);
    }
    auto Clock = FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,
        FHansaSimulationTick::TryCreate(501).Value).Value;
    FHansaConsumptionHistory Migrated;
    TestTrue(TEXT("Old save begins at its next tick"), Migrated.Record({}, Clock));
    TestEqual(TEXT("Migration does not invent prior days"), Migrated.Project(Clock).CoveredMinutes, int64(60));
    FHansaPopulationCohortState Huge; Huge.CityId = City;
    FHansaPopulationNeedState Need; Need.GoodId = Bread; Need.RequiredLastTick = FHansaQuantity::FromRaw(MAX_int64);
    Huge.Needs.Add(Need);
    FHansaConsumptionHistory Overflow;
    TestFalse(TEXT("Overflow cannot wrap into a plausible percentage"), Overflow.Record(TArray<FHansaPopulationCohortState>{Huge, Huge}, Clock));
    TestEqual(TEXT("Overflow leaves history untouched"), Overflow.Project(Clock).CoveredMinutes, int64(0));
    return !HasAnyErrors();
}
#endif
