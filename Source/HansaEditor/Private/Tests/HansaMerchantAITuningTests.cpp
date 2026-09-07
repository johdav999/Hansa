#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaMerchantAIDefinitions.h"
#include "Misc/AutomationTest.h"
#include "Schema/HansaEditorSchemaRegistry.h"

namespace
{
	TArray<const UHansaDefinitionBase*> Raw(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		TArray<const UHansaDefinitionBase*> Result;
		for (const auto& Definition : Definitions) Result.Add(Definition.Get());
		return Result;
	}

	UHansaMerchantAITuningDefinition* FindTuning(const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		for (const auto& Definition : Definitions)
		{
			if (auto* Tuning = Cast<UHansaMerchantAITuningDefinition>(Definition.Get())) return Tuning;
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMerchantAITuningAuthoringTest,
	"Hansa.Content.AI.TuningSchemaAndReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMerchantAITuningAuthoringTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FHansaEditorSchemaRegistry SchemaRegistry;
	SchemaRegistry.Refresh();
	const FHansaDefinitionClassSchema* Schema = SchemaRegistry.FindSchema(UHansaMerchantAITuningDefinition::StaticClass());
	TestTrue(TEXT("Merchant AI tuning receives generic reflected editor coverage"), Schema != nullptr && Schema->IsValid());

	TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	UHansaMerchantAITuningDefinition* Tuning = FindTuning(Definitions);
	if (!TestNotNull(TEXT("The MVP contains one merchant tuning definition"), Tuning)) return false;
	const FHansaEconomicRegistryCompileResult Valid = FHansaEconomicDefinitionCompiler::Compile(Raw(Definitions));
	TestTrue(TEXT("The authored tuning compiles into the runtime registry"), Valid.IsValid());
	TestEqual(TEXT("Exactly one merchant tuning is compiled"), Valid.Registry.GetMerchantAITunings().Num(), 1);

	Tuning->TradePlans[0].RouteDefinitionId = TEXT("Route.Missing");
	Tuning->RefreshContentHash();
	const FHansaEconomicRegistryCompileResult Invalid = FHansaEconomicDefinitionCompiler::Compile(Raw(Definitions));
	TestFalse(TEXT("A missing AI route reference rejects the registry"), Invalid.IsValid());
	TestTrue(TEXT("The invalid reference has a stable actionable diagnostic"), Invalid.Issues.ContainsByPredicate([](const auto& Issue)
	{
		return Issue.Code == TEXT("HSA-REGISTRY-028");
	}));
	return !HasAnyErrors();
}

#endif
