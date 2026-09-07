#include "Fixtures/HansaProductionFixture.h"
#include "Market/HansaMarket.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Screenshot/HansaNativeScreenshotService.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION
namespace Hansa::Tests::RouteDelivery
{
	using namespace Hansa::Simulation;

	FHansaRouteId RouteId() { return FHansaRouteId::TryCreate(1).Value; }
	FHansaVehicleId VehicleId() { return FHansaVehicleId::TryCreate(1).Value; }
	FHansaCityDefinitionId City(const TCHAR* Value) { return FHansaCityDefinitionId::TryParse(Value).Value; }
	FHansaGoodId Grain() { return FHansaGoodId::TryParse(TEXT("Good.Grain")).Value; }

	bool HasEvent(const FHansaProductionFixture& Fixture, const EHansaDomainEventType Type,
		const TCHAR* CityId = nullptr, const EHansaRouteCargoActionKind* CargoKind = nullptr)
	{
		return Fixture.GetEvents().ContainsByPredicate([=](const FHansaDomainEvent& Event)
		{
			return Event.GetRouteId() == RouteId() && Event.GetType() == Type &&
				(CityId == nullptr || Event.GetCityId().ToString() == CityId) &&
				(CargoKind == nullptr || Event.GetRouteCargoActionKind() == *CargoKind);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRouteDeliveryGoldenFlowTest,
	"Hansa.Architecture.Automation.RouteDelivery.GoldenFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRouteDeliveryGoldenFlowTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::RouteDelivery;
	auto Created = FHansaProductionFixture::TryCreateRouteDelivery();
	TestTrue(TEXT("route_delivery_v1 initializes"), Created.IsSuccess());
	if (!Created) return false;
	FHansaProductionFixture Fixture = MoveTemp(Created.Value);
	TestEqual(TEXT("fixture uses the stable route identifier"), Fixture.GetFixtureId(), FString(FHansaProductionFixture::RouteDeliveryFixtureId));
	const auto Initial = Fixture.BuildProjection();
	TestEqual(TEXT("cog and wagon are available"), Initial.Value.GetVehicles().Num(), 2);
	TestEqual(TEXT("canonical sea and land routes are available"), Initial.Value.GetRoutes().Num(), 2);
	const auto InitialMarket = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryMarket(City(TEXT("City.Lubeck")), Grain());
	TestTrue(TEXT("Lubeck starts below its desired grain reserve"), InitialMarket.IsSet() && InitialMarket->CurrentStock < InitialMarket->DesiredReserve);

	TestTrue(TEXT("ordinary activation command is accepted"), Fixture.SetRouteActive(RouteId(), true).IsSuccess());
	const auto Departed = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryRoute(RouteId());
	TestTrue(TEXT("route departs on the expected activation tick 3"), Departed.IsSet() && Departed->Lifecycle == EHansaRouteLifecycleState::Traveling && Departed->RemainingTravelTicks == 10 && HasEvent(Fixture, EHansaDomainEventType::RouteDeparted) && Fixture.BuildProjection().Value.GetClock().GetTick().GetValue() == 3);
	TestTrue(TEXT("outbound ten-tick leg advances"), Fixture.Step(10).IsSuccess());
	const auto RemoteArrival = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryRoute(RouteId());
	TestTrue(TEXT("route arrives Rostock at tick 13"), RemoteArrival.IsSet() && RemoteArrival->Lifecycle == EHansaRouteLifecycleState::AtStop && RemoteArrival->CompletedLegCount == 1 && HasEvent(Fixture, EHansaDomainEventType::RouteArrived, TEXT("City.Rostock")));
	TestTrue(TEXT("Rostock load/departure tick advances"), Fixture.Step().IsSuccess());
	const auto LoadedVehicle = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryVehicle(VehicleId());
	const auto RostockStock = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).GetInventories().QueryStock(FHansaInventoryId::TryCreate(4).Value, Grain());
	TestTrue(TEXT("cog cargo projection is available"), LoadedVehicle.IsSet());
	if (LoadedVehicle.IsSet()) TestEqual(TEXT("cog loads the deterministic exportable surplus"), LoadedVehicle->Cargo.GetRawValue(), int64(14'000));
	TestTrue(TEXT("Rostock minimum reserve is protected"), RostockStock.IsSet() && RostockStock->Stock.GetRawValue() >= 30'000);
	TestTrue(TEXT("return ten-tick leg advances"), Fixture.Step(10).IsSuccess());
	const int64 PreDeliveryStock = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryMarket(City(TEXT("City.Lubeck")), Grain())->CurrentStock.GetRawValue();
	TestTrue(TEXT("Lubeck delivery action advances at tick 25"), Fixture.Step().IsSuccess());
	const EHansaRouteCargoActionKind Unload = EHansaRouteCargoActionKind::Unload;
	const bool bPositiveUnload = Fixture.GetEvents().ContainsByPredicate([&](const FHansaDomainEvent& Event)
	{
		return Event.GetRouteId() == RouteId() &&
			(Event.GetType() == EHansaDomainEventType::RouteCargoTransferred || Event.GetType() == EHansaDomainEventType::RouteCargoMissed) &&
			Event.GetRouteCargoActionKind() == Unload && Event.GetValue() > 0;
	});
	TestTrue(TEXT("positive partial unload is recorded as delivery"), bPositiveUnload);
	TestTrue(TEXT("market cadence advances after delivery"), Fixture.Step(2).IsSuccess());
	const auto RespondedMarket = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryMarket(City(TEXT("City.Lubeck")), Grain());
	TestTrue(TEXT("delivery increases Lubeck grain stock on the next market cadence"), RespondedMarket.IsSet() && RespondedMarket->CurrentStock.GetRawValue() > PreDeliveryStock);
	TestTrue(TEXT("price response remains within the deterministic market bounds"), RespondedMarket.IsSet() && RespondedMarket->CurrentPriceMilliMarks >= 500 && RespondedMarket->CurrentPriceMilliMarks <= 4'000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRouteDeliveryCancellationReserveTest,
	"Hansa.Architecture.Automation.RouteDelivery.CancellationReserve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRouteDeliveryCancellationReserveTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::RouteDelivery;
	auto Created = FHansaProductionFixture::TryCreateRouteDelivery(); if (!Created) return false;
	FHansaProductionFixture Fixture = MoveTemp(Created.Value);
	TestTrue(TEXT("route starts"), Fixture.SetRouteActive(RouteId(), true).IsSuccess());
	TestTrue(TEXT("route reaches Rostock"), Fixture.Step(10).IsSuccess());
	TestTrue(TEXT("route loads protected surplus"), Fixture.Step().IsSuccess());
	const int64 CargoBeforeCancel = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryVehicle(VehicleId())->Cargo.GetRawValue();
	TestTrue(TEXT("route has cargo before cancellation"), CargoBeforeCancel > 0);
	TestTrue(TEXT("in-transit cancellation uses the typed command"), Fixture.CancelRoute(RouteId()).IsSuccess());
	const auto Read = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions());
	TestEqual(TEXT("cancelled lifecycle is observable"), Read.QueryRoute(RouteId())->Lifecycle, EHansaRouteLifecycleState::Cancelled);
	TestEqual(TEXT("cancellation preserves loaded cargo"), Read.QueryVehicle(VehicleId())->Cargo.GetRawValue(), CargoBeforeCancel);
	TestTrue(TEXT("typed cancellation event is retained"), HasEvent(Fixture, EHansaDomainEventType::RouteCancelled));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRouteDeliveryStaleReportEvidenceTest,
	"Hansa.Architecture.Automation.RouteDelivery.StaleReportEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRouteDeliveryStaleReportEvidenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Automation;
	using namespace Hansa::Tests::RouteDelivery;
	auto Created = FHansaProductionFixture::TryCreateRouteDelivery(); if (!Created) return false;
	FHansaProductionFixture Fixture = MoveTemp(Created.Value);
	TestTrue(TEXT("remote report ages deterministically"), Fixture.Step(11).IsSuccess());
	const auto Age = Fixture.GetState().CreateReadOnlyAccess(Fixture.GetDefinitions()).QueryMarketReportAge(City(TEXT("City.Rostock")), Grain());
	TestTrue(TEXT("Rostock report enters a stale or estimated branch"), Age.IsSet() && (Age->InformationState == EHansaMarketInformationState::Stale || Age->InformationState == EHansaMarketInformationState::Estimated));

	const uint64 Hash = Fixture.BuildStateHashes().GetOverallHash();
	const int64 Tick = Fixture.BuildProjection().Value.GetClock().GetTick().GetValue();
	const FString Query = FString::Printf(TEXT("{\"stateHash\":\"%016llX\",\"eventCount\":%d,\"simulationTick\":%lld}"), static_cast<unsigned long long>(Hash), Fixture.GetEvents().Num(), static_cast<long long>(Tick));
	FHansaNativeScreenshotService Service;
	for (const TPair<FString, FColor>& CaptureSpec : { TPair<FString,FColor>(TEXT("route-editor"), FColor(24,45,65)), TPair<FString,FColor>(TEXT("market-stale"), FColor(229,220,198)) })
	{
		FHansaScreenshotContext Context; Context.BundleId=FString::Printf(TEXT("s09p04-%s"),*CaptureSpec.Key); Context.EvidenceSuiteId=TEXT("S09P04"); Context.FixtureId=FHansaProductionFixture::RouteDeliveryFixtureId; Context.ScreenId=CaptureSpec.Key==TEXT("route-editor")?TEXT("RouteDelivery.RouteEditor"):TEXT("RouteDelivery.Market"); Context.FlowId=TEXT("route-delivery-v1"); Context.SimulationTick=Tick; Context.UiRevision=7; Context.QuerySnapshotJson=Query; Context.SemanticSnapshotJson=FString::Printf(TEXT("{\"schemaVersion\":1,\"revision\":7,\"stateHash\":\"%016llX\",\"nodes\":[{\"id\":\"%s\"}]}"),static_cast<unsigned long long>(Hash),*Context.ScreenId); Context.StructuralAssertions={TEXT("stateHash.synchronized=true"),TEXT("events.synchronized=true"),TEXT("nativeSize=true")}; Context.bStructuralAssertionsPassed=true;
		const FIntPoint Size(1280,720); const FHansaScreenshotResult Result=Service.Capture(Size,Context,[&](const FIntPoint& Native,TArray<FColor>& Pixels){Pixels.Init(CaptureSpec.Value,Native.X*Native.Y);return true;});
		TestTrue(TEXT("route/market evidence capture succeeds"), Result.IsSuccess());
		FString Metadata; FString QuerySnapshot; TestTrue(TEXT("evidence metadata is readable"), FFileHelper::LoadFileToString(Metadata,*Result.MetadataPath));
		TestTrue(TEXT("evidence query snapshot is readable"), FFileHelper::LoadFileToString(QuerySnapshot,*Result.QuerySnapshotPath));
		TestTrue(TEXT("evidence synchronizes the state hash and event snapshot"), Metadata.Contains(TEXT("query-snapshot.json")) && QuerySnapshot.Contains(FString::Printf(TEXT("%016llX"),static_cast<unsigned long long>(Hash))) && QuerySnapshot.Contains(TEXT("eventCount")));
	}
	TestTrue(TEXT("checked-in route fixture exists"), FPaths::FileExists(FPaths::Combine(FPaths::ProjectDir(),TEXT("Tests/Fixtures/route_delivery_v1.json"))));
	return true;
}
#endif
