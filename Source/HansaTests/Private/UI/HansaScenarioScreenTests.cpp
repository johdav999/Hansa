#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Scenario/HansaScenario.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/SHansaScenarioScreen.h"
#include "UObject/StrongObjectPtr.h"

using namespace Hansa::Simulation;

namespace Hansa::Tests::ScenarioUI
{
	FHansaScenarioProgress Progress(const EHansaScenarioOutcome Outcome)
	{
		FHansaScenarioObjectiveProgress Objective;
		Objective.ObjectiveId = TEXT("ScenarioObjective.SafeGrainReserve");
		Objective.DisplayName = TEXT("Restore the grain reserve");
		Objective.Metric = EHansaScenarioObjectiveMetric::MarketStockAtLeast;
		Objective.CurrentValue = 16'000;
		Objective.TargetValue = 20'000;
		Objective.ProgressUnit = TEXT("milli-units");
		FHansaVictoryPathProgress Trade;
		Trade.VictoryId = TEXT("Victory.TradeNetwork");
		Trade.DisplayName = TEXT("Trade network");
		Trade.Summary = TEXT("Bind sea and land supply into a relief network.");
		Trade.RequiredSustainTicks = 5;
		Trade.ConsecutiveSatisfiedTicks = Outcome == EHansaScenarioOutcome::Victory ? 5 : 0;
		Trade.bAllObjectivesMet = Outcome == EHansaScenarioOutcome::Victory;
		Trade.bVictorious = Outcome == EHansaScenarioOutcome::Victory;
		Objective.bMet = Outcome == EHansaScenarioOutcome::Victory;
		if (Objective.bMet) Objective.CurrentValue = Objective.TargetValue;
		Trade.Objectives = {Objective};
		FHansaVictoryPathProgress Prosperity = Trade;
		Prosperity.VictoryId = TEXT("Victory.ProsperityEconomic");
		Prosperity.DisplayName = TEXT("City of prosperity");
		Prosperity.bVictorious = false;
		FHansaVictoryPathProgress Research = Trade;
		Research.VictoryId = TEXT("Victory.ResearchCivic");
		Research.DisplayName = TEXT("Research and civic resilience");
		Research.bVictorious = false;
		FHansaScenarioProgress Result;
		Result.ScenarioId = TEXT("Scenario.LubeckGrainShortageV1");
		Result.DisplayName = TEXT("Lübeck grain shortage");
		Result.Briefing = TEXT("Restore a durable grain supply.");
		Result.Outcome = Outcome;
		Result.WinningVictoryId = Outcome == EHansaScenarioOutcome::Victory ? TEXT("Victory.TradeNetwork") : TEXT("");
		Result.FailureReason = Outcome == EHansaScenarioOutcome::Failure ? TEXT("Sustained insolvency.") : TEXT("");
		Result.VictoryPaths = {Prosperity, Trade, Research};
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaScenarioScreenStateSemanticTest,
	"Hansa.UI.Scenario.StartProgressSuccessFailureSemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaScenarioScreenStateSemanticTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaScenarioPresentationModel> Model(NewObject<UHansaScenarioPresentationModel>());
	Model->InitializeDefaults();
	TestTrue(TEXT("Active progress applies"), Model->ApplyProgress(Hansa::Tests::ScenarioUI::Progress(EHansaScenarioOutcome::Active)));
	TestEqual(TEXT("The first application remains a start briefing"), Model->GetSnapshot().Phase, EHansaScenarioPresentationPhase::Briefing);
	TSharedRef<Hansa::UI::SHansaScenarioScreen> Screen = SNew(Hansa::UI::SHansaScenarioScreen).Model(Model.Get());
	auto Semantics = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Briefing exposes an explicit begin action"), Semantics.ContainsByPredicate([](const auto& Node)
	{
		return Node.Id == TEXT("Scenario.Begin") && Node.bCanActivate;
	}));
	TestEqual(TEXT("Close is the first controller focus stop"), Screen->GetControllerFocusOrder()[0], FString(TEXT("Scenario.Close")));
	TestTrue(TEXT("Begin action acknowledges the briefing"), Screen->ActivateSemanticId(TEXT("Scenario.Begin")));
	TestEqual(TEXT("Acknowledged scenario enters active progress"), Model->GetSnapshot().Phase, EHansaScenarioPresentationPhase::Active);
	TestTrue(TEXT("A victory path can be selected semantically"), Screen->ActivateSemanticId(TEXT("Scenario.Path.Victory_TradeNetwork")));
	Semantics = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Selected path exposes objective progress and warning redundancy"), Semantics.ContainsByPredicate([](const auto& Node)
	{
		return Node.Id == TEXT("Scenario.Objective.ScenarioObjective_SafeGrainReserve") && Node.State.bWarning &&
			Node.State.ValueType == TEXT("objective-progress");
	}));
	TestTrue(TEXT("Victory progress applies"), Model->ApplyProgress(Hansa::Tests::ScenarioUI::Progress(EHansaScenarioOutcome::Victory)));
	TestEqual(TEXT("Victory forces the outcome screen open"), Model->GetSnapshot().Phase, EHansaScenarioPresentationPhase::Victory);
	Semantics = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Victory outcome is selected and explained"), Semantics.ContainsByPredicate([](const auto& Node)
	{
		return Node.Id == TEXT("Scenario.Outcome") && Node.State.bSelected && !Node.State.Value.IsEmpty();
	}));
	TestTrue(TEXT("Failure progress applies"), Model->ApplyProgress(Hansa::Tests::ScenarioUI::Progress(EHansaScenarioOutcome::Failure)));
	TestEqual(TEXT("Failure switches to the failure phase"), Model->GetSnapshot().Phase, EHansaScenarioPresentationPhase::Failure);
	Semantics = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Failure outcome uses a warning state and visible reason"), Semantics.ContainsByPredicate([](const auto& Node)
	{
		return Node.Id == TEXT("Scenario.Outcome") && Node.State.bWarning && Node.State.Value.Contains(TEXT("insolvency"));
	}));
	return !HasAnyErrors();
}

#endif
