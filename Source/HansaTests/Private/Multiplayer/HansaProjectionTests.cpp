#include "Misc/AutomationTest.h"

#include "Network/HansaMultiplayerAuthority.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Multiplayer;
using namespace Hansa::Simulation;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCompleteAuthorizedProjectionTest,
	"Hansa.Multiplayer.Projections.CompletePrivateRelevantRevocation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCompleteAuthorizedProjectionTest::RunTest(const FString& Parameters)
{
	UHansaRuntimeSimulationHost* Host = NewObject<UHansaRuntimeSimulationHost>();
	FString Error;
	if (!TestTrue(TEXT("Projection fixture initializes"), Host->InitializeForLubeck(
		nullptr, Error, EHansaRuntimeScenario::LubeckGrainShortage, 0x4d503036ULL)))
	{
		AddError(Error);
		return false;
	}
	FHansaMultiplayerAuthority Authority;
	TestTrue(TEXT("Authority initializes"), Authority.Initialize(*Host));
	FHansaClientInterest Interest;
	Interest.CityIds = { TEXT("City.Lubeck"), TEXT("City.Rostock"), TEXT("City.Hamburg"), TEXT("City.Luneburg") };
	Interest.HistoryPageSize = 3;
	const FHansaParticipantId Participant = FHansaParticipantId::TryCreate(6001).Value;
	TestTrue(TEXT("Owner is admitted"), Authority.RegisterAdmittedClient(
		{5001, Participant, Host->GetHouseId(), EHansaAdmissionMode::LanOffline}, Interest, Error));

	FHansaClientProjectionSnapshot Initial;
	TestTrue(TEXT("Initial authorized view builds"), Authority.BuildProjection(5001, 0, true, Initial, Error));
	TestEqual(TEXT("Projection schema is current"), Initial.SchemaVersion,
		FHansaClientProjectionSnapshot::CurrentSchemaVersion);
	TestTrue(TEXT("Owner receives private inventories"), !Initial.Inventories.IsEmpty());
	TestTrue(TEXT("Owner receives production batches"), !Initial.Productions.IsEmpty());
	TestTrue(TEXT("Owner receives population needs"), !Initial.PopulationCohorts.IsEmpty());
	TestTrue(TEXT("Subscribed cities receive summaries"), !Initial.CitySummaries.IsEmpty());
	TestTrue(TEXT("Vehicles have cosmetic reconstruction state"), !Initial.Vehicles.IsEmpty());
	TestTrue(TEXT("Serialized byte count is measured"), Initial.SerializedBytes > 0);
	TestTrue(TEXT("Initial snapshot stays under the release cap"), Initial.SerializedBytes <= 8LL * 1024LL * 1024LL);
	TestTrue(TEXT("Refresh cost is measured and under the per-step p99 budget"),
		Initial.BuildMicroseconds >= 0 && Initial.BuildMicroseconds <= 40000);
	TestTrue(TEXT("Authorized digest is distinct diagnostic metadata"),
		!Initial.AuthorizedViewDigest.IsEmpty() && !Initial.AuthoritativeHash.IsEmpty());
	AddInfo(FString::Printf(TEXT("MP-06 initial authorized projection: bytes=%lld build_us=%lld placements=%d markets=%d routes=%d inventories=%d productions=%d cohorts=%d cities=%d vehicles=%d logistics=%d"),
		static_cast<long long>(Initial.SerializedBytes), static_cast<long long>(Initial.BuildMicroseconds),
		Initial.Placements.Num(), Initial.Markets.Num(), Initial.Routes.Num(), Initial.Inventories.Num(),
		Initial.Productions.Num(), Initial.PopulationCohorts.Num(), Initial.CitySummaries.Num(),
		Initial.Vehicles.Num(), Initial.LogisticsJobs.Num()));
	for (const FHansaReplicatedInventory& Inventory : Initial.Inventories)
	{
		TestEqual(TEXT("Private inventory belongs to the authorized owner"),
			Inventory.OwnerHouseId, static_cast<int64>(Host->GetHouseId().GetValue()));
	}
	for (const FHansaReplicatedMarket& Market : Initial.Markets)
	{
		TestTrue(TEXT("Market history page is bounded"),
			Market.HistoryPriceMilliMarks.Num() <= Interest.HistoryPageSize &&
			Market.HistoryTicks.Num() == Market.HistoryPriceMilliMarks.Num());
	}

	FHansaClientProjectionSnapshot Unchanged;
	TestTrue(TEXT("Matching revision builds a delta"),
		Authority.BuildProjection(5001, Initial.Revision, false, Unchanged, Error));
	TestFalse(TEXT("Matching revision is not a full refresh"), Unchanged.bFullRefresh);
	TestTrue(TEXT("Unchanged large collections are omitted"),
		Unchanged.Placements.IsEmpty() && Unchanged.Markets.IsEmpty() &&
		Unchanged.Inventories.IsEmpty() && Unchanged.Productions.IsEmpty() &&
		Unchanged.PopulationCohorts.IsEmpty() && Unchanged.Vehicles.IsEmpty());
	AddInfo(FString::Printf(TEXT("MP-06 unchanged delta: bytes=%lld build_us=%lld removals=%d events=%d"),
		static_cast<long long>(Unchanged.SerializedBytes), static_cast<long long>(Unchanged.BuildMicroseconds),
		Unchanged.Removed.Num(), Unchanged.Events.Num()));

	const FHansaHouseId Rival = Host->GetRivalHouseId();
	TestTrue(TEXT("Server grants one authorized house report"),
		Authority.SetAuthorizedReportHouses(5001, MakeArrayView(&Rival, 1), Error));
	FHansaClientProjectionSnapshot Granted;
	TestTrue(TEXT("Grant produces a delta"),
		Authority.BuildProjection(5001, Unchanged.Revision, false, Granted, Error));
	TestTrue(TEXT("Granted research is present"), Granted.AuthorizedResearchReports.ContainsByPredicate(
		[&Rival](const FHansaReplicatedResearch& Report)
		{ return Report.HouseId == static_cast<int64>(Rival.GetValue()); }));
	TestTrue(TEXT("Granted private rows never identify an unrelated house"),
		!Granted.Inventories.ContainsByPredicate([&](const FHansaReplicatedInventory& Inventory)
		{ return Inventory.OwnerHouseId != static_cast<int64>(Host->GetHouseId().GetValue()) &&
			Inventory.OwnerHouseId != static_cast<int64>(Rival.GetValue()); }));

	TestTrue(TEXT("Server revokes all authorized reports"),
		Authority.SetAuthorizedReportHouses(5001, TConstArrayView<FHansaHouseId>(), Error));
	FHansaClientProjectionSnapshot Revoked;
	TestTrue(TEXT("Revocation produces a delta"),
		Authority.BuildProjection(5001, Granted.Revision, false, Revoked, Error));
	TestTrue(TEXT("Revocation explicitly evicts the cached report"),
		Revoked.Removed.ContainsByPredicate([&Rival](const FHansaProjectionRemoval& Removal)
		{ return Removal.Collection == TEXT("reports") &&
			Removal.StableId == LexToString(static_cast<int64>(Rival.GetValue())); }));
	TestTrue(TEXT("Revoked delta contains no rival research"), Revoked.AuthorizedResearchReports.IsEmpty());

	FHansaClientInterest Invalid = Interest;
	Invalid.HistoryPageSize = FHansaClientInterest::MaximumHistoryPageSize + 1;
	TestFalse(TEXT("Oversized history subscriptions fail closed"),
		Authority.SetClientInterest(5001, Invalid, Error));
	const int64 BeforeTick = Host->GetSimulationTick();
	TestTrue(TEXT("Server simulation advances independently of client world streaming"), Host->AdvanceTicks(2));
	TestEqual(TEXT("Server advanced two ticks without a presentation world"),
		Host->GetSimulationTick(), BeforeTick + 2);
	return !HasAnyErrors();
}
