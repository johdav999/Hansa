#include "Misc/AutomationTest.h"
#include "Gameplay/HansaStrategicAutomationFixture.h"
#include "World/HansaRuntimeSimulationHost.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveRoundTripFixtureTest,
	"Hansa.Architecture.Automation.Save.RoundTripV1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSaveRoundTripFixtureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace Hansa::Automation;
	FHansaStrategicAutomationFixture Fixture;
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	FString Error;
	if (!TestTrue(TEXT("save_roundtrip_v1 reaches its nontrivial authoritative checkpoint"),
		Fixture.Load(FHansaStrategicAutomationFixture::SaveRoundTripFixtureId, Payload, Error)))
	{
		AddError(Error); return false;
	}
	TestTrue(TEXT("Fixture advances beyond initialization"), Fixture.GetHost()->GetSimulationTick() > 0);
	TArray<uint8> HeaderBytes;
	const auto HeaderSave = Fixture.GetHost()->CaptureSaveBytes(HeaderBytes, TEXT("Metadata checkpoint"), TEXT("2026-09-06T12:34:56Z"));
	Hansa::Simulation::FHansaSaveMetadata Metadata;
	const auto HeaderRead = Hansa::Simulation::FHansaSaveEnvelope::InspectMetadata(HeaderBytes, Metadata);
	TestTrue(TEXT("Header metadata is integrity checked without a definition context"), HeaderSave && HeaderRead);
	TestEqual(TEXT("Header preserves timestamp"), Metadata.SavedUtc, FString(TEXT("2026-09-06T12:34:56Z")));
	TestEqual(TEXT("Header preserves scenario"), Metadata.ScenarioId, FString(TEXT("Scenario.LubeckGrainShortageV1")));
	TestEqual(TEXT("Header preserves display name"), Metadata.DisplayName, FString(TEXT("Metadata checkpoint")));
	Payload = MakeShared<FJsonObject>();
	if (!TestTrue(TEXT("Manual slot captures without a path argument"), Fixture.SaveSlot(TEXT("manual"), Payload, Error)))
	{
		AddError(Error); return false;
	}
	const FString SavedHash = Payload->GetStringField(TEXT("authoritativeHash"));
	const FString SavedProjection = Payload->GetStringField(TEXT("projectionDigest"));
	Payload = MakeShared<FJsonObject>();
	if (!TestTrue(TEXT("Manual slot restores and continues deterministically"), Fixture.LoadSlot(TEXT("manual"), Payload, Error)))
	{
		AddError(Error); return false;
	}
	TestEqual(TEXT("Authoritative hash survives the round trip"), Payload->GetStringField(TEXT("restoredAuthoritativeHash")), SavedHash);
	TestEqual(TEXT("Projection digest survives the round trip"), Payload->GetStringField(TEXT("restoredProjectionDigest")), SavedProjection);
	TestTrue(TEXT("Continuation matches an independently restored reference"), Payload->GetBoolField(TEXT("deterministicContinuation")));
	Payload = MakeShared<FJsonObject>();
	if (!TestTrue(TEXT("All S11-P02 coverage assertions pass"), Fixture.AssertRoundTrip(Payload, Error)))
	{
		AddError(Error); return false;
	}
	const TSharedPtr<FJsonObject> Coverage = Payload->GetObjectField(TEXT("coverage"));
	for (const FString Field : {TEXT("construction"), TEXT("inventories"), TEXT("prices"), TEXT("routeCargo"), TEXT("research"), TEXT("ai"), TEXT("objectives")})
		TestTrue(*FString::Printf(TEXT("%s state is covered"), *Field), Coverage->GetBoolField(Field));
	return true;
}

#endif