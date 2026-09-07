#include "Misc/AutomationTest.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveScenarioStreakTest, "Hansa.Integration.Save.ScenarioSustainAndTerminalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveScenarioStreakTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	for (bool bFailure : {false, true})
	{
		FHansaCompiledScenarioObjective Objective;
		Objective.StableId = TEXT("ScenarioObjective.Money"); Objective.TargetValue = 1;
		FHansaCompiledVictoryDefinition Victory;
		Victory.StableId = TEXT("Victory.Sustained"); Victory.ObjectiveIds = {Objective.StableId}; Victory.SustainTicks = 4;
		FHansaCompiledScenarioDefinition Scenario;
		Scenario.StableId = TEXT("Scenario.SaveStreak"); Scenario.VictoryIds = {Victory.StableId}; Scenario.FailureSustainTicks = 4;
		FHansaEconomicRegistry Registry({}, {}, {}, 123, {}, {}, {}, {}, {}, {}, {}, {Objective}, {Victory}, {Scenario});
		const auto D = FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(Scenario.StableId).Value, 123, Registry).Value;
		const auto House = FHansaHouseId::TryCreate(1).Value;
		FHansaSimulationInitialization Initial;
		Initial.Clock = FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value, FHansaSimulationTick()).Value;
		Initial.Houses.Add({House, FHansaMoney::FromRaw(bFailure ? 0 : 100)});
		FHansaSaveSnapshot S; S.State = FHansaSimulationState::TryCreate(Initial).Value;
		S.BuildVersion = TEXT("ScenarioStreakTest"); S.SavedUtc = TEXT("2026-09-06T00:00:00Z"); S.Players.Add({1, House});
		FHansaScenarioEvaluator Original; if (!Original.Initialize(Registry, Scenario.StableId, House)) return false;
		FHansaSimulationTransientCache Cache;
		for (int32 I = 0; I < 2; ++I)
		{
			if (!FHansaGameplayCommandGateway::ExecuteTick(S.State, D, {}, Cache) || !Original.Evaluate(S.State.CreateReadOnlyAccess(D), Registry)) return false;
		}
		S.Scenario = FHansaSaveEnvelope::CaptureScenario(Original);
		TestEqual(TEXT("Nonzero sustained progress"), bFailure ? S.Scenario.ConsecutiveFailureTicks : S.Scenario.VictoryStreaks[0].ConsecutiveSatisfiedTicks, 2);
		for (int32 Phase = 0; Phase < 2; ++Phase)
		{
			TArray<uint8> Bytes; const auto Encoded = FHansaSaveEnvelope::Encode(S, D, Bytes);
			if (!TestTrue(*Encoded.Message, Encoded.IsSuccess())) return false;
			FHansaSaveSnapshot Loaded; const auto Decoded = FHansaSaveEnvelope::Decode(Bytes, D, Loaded);
			if (!TestTrue(*Decoded.Message, Decoded.IsSuccess())) return false;
			FHansaScenarioEvaluator Restored;
			if (!TestTrue(TEXT("Restore scenario observer"), FHansaSaveEnvelope::RestoreScenario(Loaded.Scenario, Loaded.State, D, Restored))) return false;
			TestEqual(TEXT("Restoration adds no sustain tick"), Restored.GetProgress().ConsecutiveFailureTicks, Original.GetProgress().ConsecutiveFailureTicks);
			TestEqual(TEXT("Restoration adds no victory tick"), Restored.GetProgress().VictoryPaths[0].ConsecutiveSatisfiedTicks, Original.GetProgress().VictoryPaths[0].ConsecutiveSatisfiedTicks);
			FHansaSimulationTransientCache RestoredCache;
			for (int32 I = 0; I < 2; ++I)
			{
				if (!FHansaGameplayCommandGateway::ExecuteTick(S.State, D, {}, Cache) || !FHansaGameplayCommandGateway::ExecuteTick(Loaded.State, D, {}, RestoredCache)) return false;
				if (!Original.Evaluate(S.State.CreateReadOnlyAccess(D), Registry) || !Restored.Evaluate(Loaded.State.CreateReadOnlyAccess(D), Registry)) return false;
				TestEqual(TEXT("Outcome resolves on identical tick"), Original.GetProgress().Outcome, Restored.GetProgress().Outcome);
			}
			TestEqual(TEXT("Expected terminal outcome"), Restored.GetProgress().Outcome, bFailure ? EHansaScenarioOutcome::Failure : EHansaScenarioOutcome::Victory);
			S.Scenario = FHansaSaveEnvelope::CaptureScenario(Original);
		}
	}
	return !HasAnyErrors();
}
#endif

