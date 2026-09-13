#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRuntimeSaveTest, "Hansa.Integration.Save.RuntimeAuthorityContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRuntimeSaveTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Original(NewObject<UHansaRuntimeSimulationHost>());
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Loaded(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("Original runtime initialized"), Original->InitializeForLubeck(nullptr, Error)) ||
		!TestTrue(TEXT("Restore runtime initialized"), Loaded->InitializeForLubeck(nullptr, Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Reach active simulation"), Original->AdvanceTicks(7));
	const auto* Registry = Original->GetEconomicRegistry();
	bool bResearchQueued = false;
	for (const auto& Technology : Registry->GetTechnologies())
	{
		if (Technology.PrerequisiteTechnologyIds.IsEmpty() && Original->QueueResearch(Technology.StableId)) { bResearchQueued = true; break; }
	}
	TestTrue(TEXT("Research enters normal command path"), bResearchQueued);
	TArray<uint8> Bytes;
	const auto Saved = Original->CaptureSaveBytes(Bytes, TEXT("Runtime round trip"), TEXT("2026-09-06T00:00:00Z"));
	if (!TestTrue(*Saved.Message, Saved.IsSuccess())) return false;
	const auto Restored = Loaded->RestoreSaveBytes(Bytes);
	if (!TestTrue(*Restored.Message, Restored.IsSuccess())) return false;
	TestEqual(TEXT("Authoritative hash restored"), Restored.AuthoritativeHash, Saved.AuthoritativeHash);
	TestEqual(TEXT("Runtime campaign hash restored"), Restored.CampaignHash, Saved.CampaignHash);
	TestEqual(TEXT("Restore pauses frame adapter"), Loaded->GetSpeed(), EHansaRuntimeSimulationSpeed::Paused);
	TestEqual(TEXT("Saved scenario observer tick"), Loaded->GetScenarioProgress()->LastEvaluatedTick.GetValue(), Original->GetScenarioProgress()->LastEvaluatedTick.GetValue());
	for (int32 Tick = 0; Tick < 80; ++Tick)
	{
		if (!TestTrue(TEXT("Both runtime hosts continue"), Original->AdvanceTicks(1) && Loaded->AdvanceTicks(1))) return false;
		const auto A = Original->BuildProjection(), B = Loaded->BuildProjection();
		TestTrue(TEXT("Runtime continuation fingerprint"), A.IsSuccess() && B.IsSuccess() && A.Value.GetFingerprint() == B.Value.GetFingerprint());
		const auto* PA = Original->GetScenarioProgress(); const auto* PB = Loaded->GetScenarioProgress();
		TestEqual(TEXT("Scenario outcome"), PA->Outcome, PB->Outcome);
		TestEqual(TEXT("Failure streak"), PA->ConsecutiveFailureTicks, PB->ConsecutiveFailureTicks);
		for (int32 I = 0; I < PA->VictoryPaths.Num(); ++I)
			TestEqual(TEXT("Victory streak survives restore"), PA->VictoryPaths[I].ConsecutiveSatisfiedTicks, PB->VictoryPaths[I].ConsecutiveSatisfiedTicks);
	}
	const int64 BeforeFailure = Loaded->GetSimulationTick(); Bytes[Bytes.Num()/2] ^= 1;
	TestFalse(TEXT("Corrupt runtime save rejected"), Loaded->RestoreSaveBytes(Bytes).IsSuccess());
	TestEqual(TEXT("Failed runtime restore leaves tick unchanged"), Loaded->GetSimulationTick(), BeforeFailure);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRoadNetworkSaveTest,
	"Hansa.Integration.Save.RoadDrawingRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRoadNetworkSaveTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Original(NewObject<UHansaRuntimeSimulationHost>());
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Loaded(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("Original empty Lübeck runtime initializes"),
		Original->InitializeForLubeck(nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild)) ||
		!TestTrue(TEXT("Restore empty Lübeck runtime initializes"),
			Loaded->InitializeForLubeck(nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild)))
	{
		AddError(Error);
		return false;
	}
	const FHansaCityDefinitionId City = Original->GetCityId();
	const FHansaBuildingTypeId Road = FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
	FHansaPlacementSession Session;
	Session.SelectBuilding(City, Road, true, false);
	Session.BeginRoadDrag({ 18, 16 });
	Session.UpdateRoadDrag({ 20, 16 });
	const TArray<FHansaPlacementSpec> RoadSpecs = Session.BuildConfirmationSpecs();
	TestEqual(TEXT("Save fixture uses a three-cell player-drawn road"), RoadSpecs.Num(), 3);
	TestTrue(TEXT("Player-drawn road reaches the ordinary command gateway"), Original->PlaceBuildings(RoadSpecs).IsSuccess());

	TArray<uint8> Bytes;
	const FHansaSaveResult Saved = Original->CaptureSaveBytes(
		Bytes, TEXT("Road drawing round trip"), TEXT("2026-09-07T14:00:00Z"));
	if (!TestTrue(*Saved.Message, Saved.IsSuccess())) return false;
	const FHansaSaveResult Restored = Loaded->RestoreSaveBytes(Bytes);
	if (!TestTrue(*Restored.Message, Restored.IsSuccess())) return false;
	TestEqual(TEXT("All road cells survive save/load"), Loaded->GetPlacedBuildingCount(), 3);
	TestEqual(TEXT("Road save restores the exact authoritative hash"), Restored.AuthoritativeHash, Saved.AuthoritativeHash);
	const auto Projection = Loaded->BuildProjection();
	int32 RestoredRoads = 0;
	if (Projection)
	{
		for (const FHansaBuildingWorldProjection& Building : Projection.Value.GetBuildingWorldProjections())
		{
			RestoredRoads += Building.Placement.BuildingDefinitionId == Road ? 1 : 0;
		}
	}
	TestEqual(TEXT("Restored world projection contains every road presentation"), RestoredRoads, 3);

	FHansaPlacementSpec Continuation;
	Continuation.CityId = City;
	Continuation.BuildingDefinitionId = Road;
	Continuation.Anchor = { 21, 16 };
	TestTrue(TEXT("Original road can continue after save"), Original->PlaceBuildings({ Continuation }).IsSuccess());
	TestTrue(TEXT("Restored road can continue after load"), Loaded->PlaceBuildings({ Continuation }).IsSuccess());
	const auto OriginalProjection = Original->BuildProjection();
	const auto LoadedProjection = Loaded->BuildProjection();
	TestTrue(TEXT("Post-load road continuation remains deterministic"),
		OriginalProjection && LoadedProjection &&
		OriginalProjection.Value.GetFingerprint() == LoadedProjection.Value.GetFingerprint());
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaConstructionRecoverySaveTest,
	"Hansa.Integration.Save.ConstructionCancellationRecoveryRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaConstructionRecoverySaveTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Original(NewObject<UHansaRuntimeSimulationHost>());
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Loaded(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("Original recovery runtime initializes"), Original->InitializeForLubeck(
		nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild)) ||
		!TestTrue(TEXT("Loaded recovery runtime initializes"), Loaded->InitializeForLubeck(
		nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild)))
	{
		AddError(Error); return false;
	}
	FHansaPlacementSpec Road;
	Road.CityId = Original->GetCityId();
	Road.BuildingDefinitionId = FHansaBuildingTypeId::TryParse(TEXT("Building.Road")).Value;
	Road.Anchor = { 18, 16 };
	if (!TestTrue(TEXT("Mistaken construction site enters the command gateway"), Original->PlaceBuildings({ Road }).IsSuccess())) return false;
	const FHansaBuildingId SiteId = FHansaBuildingId::TryCreate(1).Value;
	TestTrue(TEXT("Cancellation preflight accepts the unfinished site"), Original->PreviewCancelConstruction(SiteId).IsSuccess());
	TArray<uint8> Bytes;
	const FHansaSaveResult Saved = Original->CaptureSaveBytes(Bytes, TEXT("Construction recovery"), TEXT("2026-09-07T16:00:00Z"));
	if (!TestTrue(*Saved.Message, Saved.IsSuccess())) return false;
	TestTrue(TEXT("Original site can be cancelled after saving"), Original->CancelConstruction(SiteId).IsSuccess());
	TestEqual(TEXT("Cancellation releases the mistaken placement"), Original->GetPlacedBuildingCount(), 0);
	const FHansaSaveResult Restored = Loaded->RestoreSaveBytes(Bytes);
	if (!TestTrue(*Restored.Message, Restored.IsSuccess())) return false;
	TestEqual(TEXT("Save/load restores the unfinished recovery point"), Loaded->GetPlacedBuildingCount(), 1);
	TestEqual(TEXT("Restored site retains construction state"), Loaded->GetBuildingWorldStatus(1), FString(TEXT("UnderConstruction")));
	TestTrue(TEXT("Restored site can still use the same cancellation command"), Loaded->CancelConstruction(SiteId).IsSuccess());
	TestEqual(TEXT("Restored cancellation releases the placement"), Loaded->GetPlacedBuildingCount(), 0);
	return !HasAnyErrors();
}
#endif

