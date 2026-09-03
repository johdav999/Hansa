#include "Tests/HansaEditorSchemaTestDefinitions.h"

#include "Definitions/HansaFoundationSampleDefinition.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Schema/HansaEditorSchemaRegistry.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Editor::Tests
{
	const FHansaDefinitionClassSchema* FindFoundationSampleSchema(
		FAutomationTestBase& Test,
		FHansaEditorSchemaRegistry& Registry)
	{
		Registry.Refresh();
		const FHansaDefinitionClassSchema* Schema = Registry.FindSchema(UHansaFoundationSampleDefinition::StaticClass());
		Test.TestNotNull(TEXT("Foundation sample definition is discovered"), Schema);
		return Schema;
	}

	FString NormalizeNewlines(FString Text)
	{
		Text.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaAutomaticSchemaDiscoveryTest,
	"Hansa.Architecture.Authoring.SchemaDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaAutomaticSchemaDiscoveryTest::RunTest(const FString& Parameters)
{
	FHansaEditorSchemaRegistry Registry;
	const FHansaDefinitionClassSchema* Schema = Hansa::Editor::Tests::FindFoundationSampleSchema(*this, Registry);
	if (Schema == nullptr)
	{
		return false;
	}

	TestTrue(TEXT("Foundation sample metadata is complete"), Schema->IsValid());
	const FHansaEditorSchemaProperty* SampleProperty = Schema->Properties.FindByPredicate(
		[](const FHansaEditorSchemaProperty& Property)
		{
			return Property.Name == GET_MEMBER_NAME_STRING_CHECKED(UHansaFoundationSampleDefinition, SampleValue);
		});
	TestNotNull(TEXT("Reflected SampleValue appears without a handwritten form"), SampleProperty);
	if (SampleProperty != nullptr)
	{
		TestEqual(TEXT("SampleValue JSON type"), SampleProperty->JsonType, FString(TEXT("integer")));
		TestEqual(TEXT("SampleValue unit"), SampleProperty->Unit, FString(TEXT("Percent")));
		TestEqual(TEXT("SampleValue AI policy"), SampleProperty->AIAccess, FString(TEXT("Suggest")));
		TestTrue(TEXT("SampleValue is available to the native Details framework"), SampleProperty->ReflectedProperty != nullptr);
	}

	const FString Json = Registry.ExportJsonSchema(*Schema);
	TestTrue(TEXT("Exported schema contains automatic SampleValue property"), Json.Contains(TEXT("\"SampleValue\"")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSchemaMetadataCoverageTest,
	"Hansa.Architecture.Authoring.MetadataCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSchemaMetadataCoverageTest::RunTest(const FString& Parameters)
{
	FHansaEditorSchemaRegistry Registry;
	const FHansaDefinitionClassSchema Schema =
		Registry.BuildSchemaForClass(UHansaIncompleteMetadataTestDefinition::StaticClass());
	TestFalse(TEXT("Incomplete metadata test class is rejected"), Schema.IsValid());
	TestTrue(
		TEXT("Missing authoring metadata names the property and remedy"),
		Schema.Diagnostics.ContainsByPredicate([](const FHansaSchemaDiagnostic& Diagnostic)
		{
			return Diagnostic.PropertyPath == TEXT("MissingMetadata") &&
				(Diagnostic.Code == TEXT("HSA-SCHEMA-003") || Diagnostic.Code == TEXT("HSA-SCHEMA-004")) &&
				!Diagnostic.Remedy.IsEmpty();
		}));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaGoldenSchemaTest,
	"Hansa.Architecture.Authoring.GoldenSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaGoldenSchemaTest::RunTest(const FString& Parameters)
{
	FHansaEditorSchemaRegistry Registry;
	const FHansaDefinitionClassSchema* Schema = Hansa::Editor::Tests::FindFoundationSampleSchema(*this, Registry);
	if (Schema == nullptr)
	{
		return false;
	}

	const FString First = Registry.ExportJsonSchema(*Schema);
	const FString Second = Registry.ExportJsonSchema(*Schema);
	TestEqual(TEXT("JSON Schema export is deterministic within a run"), First, Second);

	const FString EvidenceDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("authoring_schema_v1"));
	IFileManager::Get().MakeDirectory(*EvidenceDirectory, true);
	FFileHelper::SaveStringToFile(
		First,
		*FPaths::Combine(EvidenceDirectory, TEXT("foundation_sample.actual.schema.json")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const FString GoldenPath = FPaths::Combine(
		FPaths::ProjectDir(),
		TEXT("Tests"),
		TEXT("Golden"),
		TEXT("Editor"),
		TEXT("foundation_sample.schema.json"));
	FString Golden;
	if (!TestTrue(TEXT("Golden schema is readable"), FFileHelper::LoadFileToString(Golden, *GoldenPath)))
	{
		return false;
	}
	TestEqual(
		TEXT("Export matches reviewed golden JSON Schema"),
		Hansa::Editor::Tests::NormalizeNewlines(First),
		Hansa::Editor::Tests::NormalizeNewlines(Golden));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaDefinitionTransactionTest,
	"Hansa.Architecture.Authoring.Transactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaDefinitionTransactionTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Editor transaction system is available"), GEditor) || GEditor->Trans == nullptr)
	{
		return false;
	}

	TStrongObjectPtr<UHansaFoundationSampleDefinition> Sample(
		NewObject<UHansaFoundationSampleDefinition>(GetTransientPackage()));
	Sample->SetFlags(RF_Transactional);
	const int32 InitialValue = Sample->SampleValue;
	{
		const FScopedTransaction Transaction(
			NSLOCTEXT("HansaEditorTests", "ChangeSampleValue", "Change Hansa sample value"));
		Sample->Modify();
		Sample->SampleValue = 77;
		Sample->RefreshContentHash();
	}
	TestEqual(TEXT("Transaction applies the edit"), Sample->SampleValue, 77);

	TestTrue(TEXT("Transaction can be undone"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores the reflected value"), Sample->SampleValue, InitialValue);
	TestTrue(TEXT("Transaction can be redone"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo restores the edited reflected value"), Sample->SampleValue, 77);
	return !HasAnyErrors();
}

#endif
