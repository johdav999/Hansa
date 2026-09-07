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
#endif

