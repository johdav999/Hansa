#include "Misc/AutomationTest.h"
#include "Population/HansaSeasonalNeeds.h"
#include "Inventory/HansaInventory.h"

#if WITH_DEV_AUTOMATION_TESTS
using namespace Hansa::Simulation;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodCalendarTest, "Hansa.Simulation.Firewood.CalendarAndFractionalDemand", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodCalendarTest::RunTest(const FString&)
{
    FHansaCompiledNeedDefinition Need; Need.bSeasonal = true;
    auto Tick = [](int64 N) { return FHansaSimulationTick::TryCreate(N).Value; };
    TestEqual(TEXT("Summer requires no household fuel"), FHansaSeasonalNeeds::Demand(Need, 1, 50, Tick(1), 60), 0LL);
    TestEqual(TEXT("Autumn boundary"), FHansaSeasonalNeeds::Multiplier(Need, Tick(90*24), 60), 4000);
    TestEqual(TEXT("Winter boundary"), FHansaSeasonalNeeds::Multiplier(Need, Tick(180*24), 60), 10000);
    TestEqual(TEXT("Next summer"), FHansaSeasonalNeeds::Multiplier(Need, Tick(360*24), 60), 0);
    Need.FixedSeason = 1;
    int64 Total = 0;
    for (int64 T=0; T<10000; ++T) Total += FHansaSeasonalNeeds::Demand(Need, 1, 1, Tick(T), 60);
    TestEqual(TEXT("Fractional demand is conserved"), Total, 4000LL);
    Need.FixedSeason = 2;
    TestEqual(TEXT("Fixed winter ignores calendar"), FHansaSeasonalNeeds::Demand(Need, 1, 50, Tick(0), 60), 50LL);
    TestEqual(TEXT("Empty homes consume nothing"), FHansaSeasonalNeeds::Demand(Need, 1, 0, Tick(0), 60), 0LL);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodProtectionTest, "Hansa.Simulation.Firewood.ReserveConservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodProtectionTest::RunTest(const FString&)
{
    const auto Good = FHansaGoodId::TryParse(TEXT("Good.Firewood")).Value;
    const auto Id = FHansaInventoryId::TryCreate(1).Value;
    const auto Reservation = FHansaReservationId::TryCreate(1).Value;
    const auto Tick = FHansaSimulationTick::TryCreate(1).Value;
    FHansaInventoryInitialization Pool;
    Pool.Id=Id; Pool.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
    Pool.Capacity=FHansaQuantity::FromRaw(20000); Pool.AcceptedGoods={Good}; Pool.InitialStock={{Good,FHansaQuantity::FromRaw(10000)}};
    auto Made=FHansaInventoryLedger::TryCreate({Pool},64);
    if (!TestTrue(TEXT("Valid inventory"), Made.IsSuccess())) return false;
    auto Ledger=MoveTemp(Made.Value);
    TestTrue(TEXT("Old industrial commitment"), Ledger.TryReserve(Id,Reservation,Good,FHansaQuantity::FromRaw(2000),Tick,1).IsSuccess());
    Ledger.SetHouseholdProtection({{Id,Good,6000}});
    const auto Source=FHansaInventoryEndpoint::Inventory(Id);
    const auto Industry=FHansaInventoryEndpoint::Sink(TEXT("Production"));
    TestEqual(TEXT("New industrial withdrawal blocked"), Ledger.TryTransfer(Source,Industry,Good,FHansaQuantity::FromRaw(3000),Tick,2).Error, EHansaInventoryTransactionError::HouseholdReserveProtected);
    TestTrue(TEXT("Exact surplus can leave"), Ledger.TryTransfer(Source,Industry,Good,FHansaQuantity::FromRaw(2000),Tick,2).IsSuccess());
    TestTrue(TEXT("Existing commitment remains valid"), Ledger.TryTransfer(Source,Industry,Good,FHansaQuantity::FromRaw(2000),Tick,3,Reservation).IsSuccess());
    TestTrue(TEXT("Household can use reserve"), Ledger.TryTransfer(Source,FHansaInventoryEndpoint::Sink(TEXT("PopulationConsumption")),Good,FHansaQuantity::FromRaw(1000),Tick,4).IsSuccess());
    TestEqual(TEXT("No duplication or loss"), Ledger.CreateReadOnlyAccess().QueryStock(Id,Good)->Stock.GetRawValue(), 5000LL);
    Ledger.SetHouseholdProtection({});
    TestTrue(TEXT("Explicit release restores industrial access"), Ledger.TryTransfer(Source,Industry,Good,FHansaQuantity::FromRaw(5000),Tick,5).IsSuccess());
    TestEqual(TEXT("Final stock accounted"), Ledger.CreateReadOnlyAccess().QueryStock(Id,Good)->Stock.GetRawValue(), 0LL);
    return true;
}
#endif
