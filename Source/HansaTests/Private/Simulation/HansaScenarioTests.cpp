#if WITH_DEV_AUTOMATION_TESTS

#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Scenario/HansaScenario.h"
#include "UObject/StrongObjectPtr.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Simulation;

namespace Hansa::Tests::Scenario
{
	FHansaEconomicRegistry MakeRuleRegistry(
		const int64 ObjectiveTarget,
		const int32 FailureTicks,
		TArray<FHansaCompiledVictoryDefinition> Victories,
		TArray<FString> VictoryIds)
	{
		FHansaCompiledScenarioObjective Objective;
		Objective.StableId = TEXT("ScenarioObjective.Ready");
		Objective.DisplayName = TEXT("Ready");
		Objective.Metric = EHansaScenarioObjectiveMetric::HouseMoneyAtLeast;
		Objective.TargetValue = ObjectiveTarget;
		Objective.ProgressUnit = TEXT("pfennig");
		FHansaCompiledScenarioDefinition Scenario;
		Scenario.StableId = TEXT("Scenario.ResolutionRules");
		Scenario.DisplayName = TEXT("Resolution rules");
		Scenario.VictoryIds = MoveTemp(VictoryIds);
		Scenario.InsolvencyThresholdPfennig = 0;
		Scenario.FailureSustainTicks = FailureTicks;
		return FHansaEconomicRegistry({}, {}, {}, 0x510003ULL, {}, {}, {}, {}, {}, {}, {},
			{MoveTemp(Objective)}, MoveTemp(Victories), {MoveTemp(Scenario)});
	}

	FHansaSimulationState MakeRuleState(const FHansaHouseId House, const int64 Money)
	{
		FHansaSimulationInitialization Initial;
		Initial.Clock = FHansaSimulationClock::TryCreate(
			FHansaSimulationVersion::TryCreate(1).Value,
			FHansaSimulationTick::TryCreate(0).Value).Value;
		Initial.Houses = {{House, FHansaMoney::FromRaw(Money)}};
		return FHansaSimulationState::TryCreate(MoveTemp(Initial)).Value;
	}

	FHansaSimulationDefinitionContext MakeEmptyDefinitions()
	{
		FHansaEconomicRegistry Registry({}, {}, {}, 0x510003ULL);
		return FHansaSimulationDefinitionContext::TryCreate(
			FHansaScenarioId::TryParse(TEXT("Scenario.Rules")).Value,
			0x510003ULL, MoveTemp(Registry)).Value;
	}

	FString Describe(const FHansaScenarioProgress* Progress)
	{
		if (Progress == nullptr) return TEXT("<null progress>");
		TArray<FString> Paths;
		for (const FHansaVictoryPathProgress& Path : Progress->VictoryPaths)
		{
			TArray<FString> Objectives;
			for (const FHansaScenarioObjectiveProgress& Objective : Path.Objectives)
			{
				Objectives.Add(FString::Printf(TEXT("%s=%lld/%lld:%s"), *Objective.ObjectiveId,
					static_cast<long long>(Objective.CurrentValue), static_cast<long long>(Objective.TargetValue), Objective.bMet ? TEXT("met") : TEXT("open")));
			}
			Paths.Add(FString::Printf(TEXT("%s sustain=%d/%d [%s]"), *Path.VictoryId,
				Path.ConsecutiveSatisfiedTicks, Path.RequiredSustainTicks, *FString::Join(Objectives, TEXT(", "))));
		}
		return FString::Printf(TEXT("outcome=%s winner=%s paths=%s"), LexToString(Progress->Outcome),
			*Progress->WinningVictoryId, *FString::Join(Paths, TEXT(" | ")));
	}

	FHansaCompiledVictoryDefinition Victory(const TCHAR* Id, const int32 Priority, const int32 SustainTicks = 1)
	{
		FHansaCompiledVictoryDefinition Result;
		Result.StableId = Id;
		Result.DisplayName = Id;
		Result.ObjectiveIds = {TEXT("ScenarioObjective.Ready")};
		Result.EndingPriority = Priority;
		Result.SustainTicks = SustainTicks;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaScenarioAuthoredVictoryPathsTest,
	"Hansa.Simulation.Scenario.LubeckGrainShortage.AuthoredVictoryPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaScenarioAuthoredVictoryPathsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	{
		TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
		FString Error;
		if (!TestTrue(TEXT("Prosperity fixture initializes"), Host->InitializeForLubeck(nullptr, Error))) { AddError(Error); return false; }
		const auto ProductionId = FHansaProductionId::TryCreate(1);
		if (!TestTrue(TEXT("The grain-farm production ID is valid"), ProductionId.IsSuccess())) return false;
		TestTrue(TEXT("The player can activate the fourth production through the authoritative gateway"),
			Host->SetProductionActive(ProductionId.Value, true).IsSuccess());
		TestTrue(TEXT("The first market update plus five satisfied ticks resolve prosperity"), Host->AdvanceTicks(8));
		const FHansaScenarioProgress* Progress = Host->GetScenarioProgress();
		AddInfo(Hansa::Tests::Scenario::Describe(Progress));
		TestTrue(TEXT("Prosperity ending wins"), Progress != nullptr && Progress->Outcome == EHansaScenarioOutcome::Victory &&
			Progress->WinningVictoryId == TEXT("Victory.ProsperityEconomic"));
	}
	{
		TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
		FString Error;
		if (!TestTrue(TEXT("Trade fixture initializes"), Host->InitializeForLubeck(nullptr, Error))) { AddError(Error); return false; }
		const auto SeaRoute = FHansaRouteId::TryCreate(1);
		const auto LandRoute = FHansaRouteId::TryCreate(2);
		if (!SeaRoute || !LandRoute) return false;
		TestTrue(TEXT("The player activates the canonical sea route"), Host->SetRouteActive(SeaRoute.Value, true).IsSuccess());
		TestTrue(TEXT("The player activates the canonical land route"), Host->SetRouteActive(LandRoute.Value, true).IsSuccess());
		TestTrue(TEXT("The bounded route window completes supply legs and sustain time"), Host->AdvanceTicks(35));
		const FHansaScenarioProgress* Progress = Host->GetScenarioProgress();
		AddInfo(Hansa::Tests::Scenario::Describe(Progress));
		TestTrue(TEXT("Trade-network ending wins"), Progress != nullptr && Progress->Outcome == EHansaScenarioOutcome::Victory &&
			Progress->WinningVictoryId == TEXT("Victory.TradeNetwork"));
	}
	{
		TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
		FString Error;
		if (!TestTrue(TEXT("Research fixture initializes"), Host->InitializeForLubeck(nullptr, Error))) { AddError(Error); return false; }
		TestTrue(TEXT("Commerce research queues through the authoritative gateway"),
			Host->QueueResearch(TEXT("Technology.Commerce.MarketReports")).IsSuccess());
		TestTrue(TEXT("The first root technology completes deterministically"), Host->AdvanceTicks(5));
		TestTrue(TEXT("Logistics research queues through the same gateway"),
			Host->QueueResearch(TEXT("Technology.Logistics.WarehouseHandling")).IsSuccess());
		TestTrue(TEXT("The second technology and victory sustain window complete"), Host->AdvanceTicks(10));
		const FHansaScenarioProgress* Progress = Host->GetScenarioProgress();
		TestTrue(TEXT("Research/civic ending wins"), Progress != nullptr && Progress->Outcome == EHansaScenarioOutcome::Victory &&
			Progress->WinningVictoryId == TEXT("Victory.ResearchCivic"));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaScenarioResolutionRulesTest,
	"Hansa.Simulation.Scenario.DeterministicResolutionRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaScenarioResolutionRulesTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::Scenario;
	(void)Parameters;
	const FHansaHouseId House = FHansaHouseId::TryCreate(1, 1).Value;
	{
		FHansaEconomicRegistry Registry = MakeRuleRegistry(1, 20,
			{Victory(TEXT("Victory.Later"), 20), Victory(TEXT("Victory.First"), 10)},
			{TEXT("Victory.Later"), TEXT("Victory.First")});
		FHansaSimulationState State = MakeRuleState(House, 100);
		FHansaScenarioEvaluator Evaluator;
		TestTrue(TEXT("Priority fixture initializes"), Evaluator.Initialize(Registry, TEXT("Scenario.ResolutionRules"), House));
		const FHansaSimulationDefinitionContext Definitions = MakeEmptyDefinitions();
		TestTrue(TEXT("Priority fixture evaluates"), Evaluator.Evaluate(State.CreateReadOnlyAccess(Definitions), Registry));
		TestEqual(TEXT("Lower authored priority wins simultaneous valid endings"), Evaluator.GetProgress().WinningVictoryId, FString(TEXT("Victory.First")));
	}
	{
		FHansaEconomicRegistry Registry = MakeRuleRegistry(1, 20,
			{Victory(TEXT("Victory.Beta"), 10), Victory(TEXT("Victory.Alpha"), 10)},
			{TEXT("Victory.Beta"), TEXT("Victory.Alpha")});
		FHansaSimulationState State = MakeRuleState(House, 100);
		FHansaScenarioEvaluator Evaluator;
		TestTrue(TEXT("Stable-ID fixture initializes"), Evaluator.Initialize(Registry, TEXT("Scenario.ResolutionRules"), House));
		const FHansaSimulationDefinitionContext EmptyDefinitions = MakeEmptyDefinitions();
		TestTrue(TEXT("Stable-ID fixture evaluates"), Evaluator.Evaluate(State.CreateReadOnlyAccess(EmptyDefinitions), Registry));
		TestEqual(TEXT("Stable ID is the deterministic defensive fallback for equal priorities"),
			Evaluator.GetProgress().WinningVictoryId, FString(TEXT("Victory.Alpha")));
	}
	{
		FHansaEconomicRegistry Registry = MakeRuleRegistry(-1, 1,
			{Victory(TEXT("Victory.FinalTick"), 10)}, {TEXT("Victory.FinalTick")});
		FHansaSimulationState State = MakeRuleState(House, -1);
		FHansaScenarioEvaluator Evaluator;
		TestTrue(TEXT("Failure-tie fixture initializes"), Evaluator.Initialize(Registry, TEXT("Scenario.ResolutionRules"), House));
		const FHansaSimulationDefinitionContext EmptyDefinitions = MakeEmptyDefinitions();
		TestTrue(TEXT("Failure-tie fixture evaluates"), Evaluator.Evaluate(State.CreateReadOnlyAccess(EmptyDefinitions), Registry));
		TestEqual(TEXT("Failure wins an exact-tick tie against a newly satisfied victory"),
			Evaluator.GetProgress().Outcome, EHansaScenarioOutcome::Failure);
		TestTrue(TEXT("Failure exposes a player-facing reason"), !Evaluator.GetProgress().FailureReason.IsEmpty());
	}
	return !HasAnyErrors();
}

#endif
