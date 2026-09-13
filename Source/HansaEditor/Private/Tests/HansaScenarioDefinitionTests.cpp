#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaScenarioDefinitions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Editor::ScenarioTests
{
	TArray<const UHansaDefinitionBase*> ScenarioDefinitionTestsRaw(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		TArray<const UHansaDefinitionBase*> Result;
		Result.Reserve(Definitions.Num());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions) Result.Add(Definition.Get());
		return Result;
	}

	template <typename TDefinition>
	TDefinition* Find(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions, const TCHAR* StableId)
	{
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions)
		{
			if (Definition->StableDefinitionId == StableId) return CastChecked<TDefinition>(Definition.Get());
		}
		return nullptr;
	}

	bool HasCode(const FHansaEconomicRegistryCompileResult& Result, const TCHAR* Code)
	{
		return Result.Issues.ContainsByPredicate([Code](const FHansaDefinitionValidationIssue& Issue)
		{
			return Issue.Code == FName(Code);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaScenarioDefinitionValidationTest,
	"Hansa.Content.Definitions.ScenarioValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaScenarioDefinitionValidationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::ScenarioTests;
	{
		auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		const FHansaEconomicRegistryCompileResult Result = FHansaEconomicDefinitionCompiler::Compile(ScenarioDefinitionTestsRaw(Definitions));
		TestTrue(TEXT("The authored single scenario and its three endings compile"), Result.IsValid());
		TestEqual(TEXT("The scenario exposes exactly three bounded victory paths"), Result.Registry.GetVictories().Num(), 3);
	}
	{
		auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaScenarioObjectiveDefinition* Objective = Find<UHansaScenarioObjectiveDefinition>(Definitions, TEXT("ScenarioObjective.CivicSatisfaction"));
		TestNotNull(TEXT("Civic objective exists"), Objective);
		if (Objective != nullptr) Objective->TargetValue = 10'001;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(ScenarioDefinitionTestsRaw(Definitions));
		TestFalse(TEXT("An impossible civic threshold fails closed"), Result.IsValid());
		TestTrue(TEXT("Impossible-objective diagnostic is stable"), HasCode(Result, TEXT("HSA-OBJECTIVE-002")));
	}
	{
		auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaScenarioObjectiveDefinition* Objective = Find<UHansaScenarioObjectiveDefinition>(Definitions, TEXT("ScenarioObjective.SafeBreadReserve"));
		if (Objective != nullptr) Objective->GoodId = TEXT("Good.Missing");
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(ScenarioDefinitionTestsRaw(Definitions));
		TestFalse(TEXT("A missing objective good fails closed"), Result.IsValid());
		TestTrue(TEXT("Missing-objective-good diagnostic is stable"), HasCode(Result, TEXT("HSA-REGISTRY-031")));
	}
	{
		auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaVictoryDefinition* Victory = Find<UHansaVictoryDefinition>(Definitions, TEXT("Victory.TradeNetwork"));
		if (Victory != nullptr) Victory->ObjectiveIds[0] = TEXT("ScenarioObjective.Missing");
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(ScenarioDefinitionTestsRaw(Definitions));
		TestFalse(TEXT("A missing victory objective fails closed"), Result.IsValid());
		TestTrue(TEXT("Missing-victory-objective diagnostic is stable"), HasCode(Result, TEXT("HSA-REGISTRY-032")));
	}
	{
		auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaScenarioDefinition* Scenario = Find<UHansaScenarioDefinition>(Definitions, TEXT("Scenario.LubeckGrainShortageV1"));
		if (Scenario != nullptr) Scenario->VictoryIds[0] = TEXT("Victory.Missing");
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(ScenarioDefinitionTestsRaw(Definitions));
		TestFalse(TEXT("A missing scenario ending fails closed"), Result.IsValid());
		TestTrue(TEXT("Missing-ending diagnostic is stable"), HasCode(Result, TEXT("HSA-REGISTRY-034")));
	}
	{
		auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
		UHansaVictoryDefinition* Trade = Find<UHansaVictoryDefinition>(Definitions, TEXT("Victory.TradeNetwork"));
		UHansaVictoryDefinition* Prosperity = Find<UHansaVictoryDefinition>(Definitions, TEXT("Victory.ProsperityEconomic"));
		if (Trade != nullptr && Prosperity != nullptr) Trade->EndingPriority = Prosperity->EndingPriority;
		const auto Result = FHansaEconomicDefinitionCompiler::Compile(ScenarioDefinitionTestsRaw(Definitions));
		TestFalse(TEXT("Ambiguous ending priorities fail closed"), Result.IsValid());
		TestTrue(TEXT("Ambiguous-ending diagnostic is stable"), HasCode(Result, TEXT("HSA-REGISTRY-035")));
	}
	return !HasAnyErrors();
}

#endif
