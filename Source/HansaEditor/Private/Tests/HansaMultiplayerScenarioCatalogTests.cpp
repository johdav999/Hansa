#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaScenarioDefinitions.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMultiplayerScenarioCatalogTest,
	"Hansa.Multiplayer.SessionModel.AuthoredCatalogHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMultiplayerScenarioCatalogTest::RunTest(const FString& Parameters)
{
	auto Definitions = Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	TArray<const UHansaDefinitionBase*> Raw;
	for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions) Raw.Add(Definition.Get());
	const FHansaEconomicRegistryCompileResult Compiled = FHansaEconomicDefinitionCompiler::Compile(Raw);
	TestTrue(TEXT("MP-02 authored catalog compiles"), Compiled.IsValid());
	const Hansa::Simulation::FHansaCompiledScenarioDefinition* Scenario =
		Compiled.Registry.FindScenario(TEXT("Scenario.LubeckGrainShortageV1"));
	TestNotNull(TEXT("MP-02 scenario is compiled"), Scenario);
	if (Scenario != nullptr)
	{
		AddInfo(FString::Printf(TEXT("MP-02 compiled registry=%016llX scenario=%016llX"),
			static_cast<unsigned long long>(Compiled.Registry.GetRegistryHash()),
			static_cast<unsigned long long>(Scenario->ContentHash)));
		TestEqual(TEXT("Eight authored house slots are compiled"), Scenario->MultiplayerSlots.Num(), 8);
	}
	UHansaScenarioDefinition* LegacyScenario = NewObject<UHansaScenarioDefinition>(GetTransientPackage());
	LegacyScenario->SchemaVersion = 1;
	LegacyScenario->MultiplayerSlots.Reset();
	LegacyScenario->PostLoad();
	TestEqual(TEXT("Schema-1 scenario migrates to schema 3"), LegacyScenario->SchemaVersion, 3);
	TestEqual(TEXT("Schema-1 scenario receives the complete eight-slot roster"), LegacyScenario->MultiplayerSlots.Num(), 8);
	return !HasAnyErrors();
}

#endif
