#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Terrain/HansaCityTerrainToolset.h"
#include "Editor.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCityTerrainContextTest,
	"Hansa.Editor.Terrain.ContextIsReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCityTerrainContextTest::RunTest(const FString&)
{
	TestTrue(TEXT("Terrain tool is registered after engine initialization"), UToolsetRegistry::IsToolsetClassRegistered(UHansaCityTerrainToolset::StaticClass()));
	UWorld* Before = GEditor->GetEditorWorldContext().World();
	TSharedPtr<FJsonObject> Result;
	const FString Text = UHansaCityTerrainToolset::InspectAuthoringContext();
	TestTrue(TEXT("Context is structured JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result));
	if (Result)
	{
		TestTrue(TEXT("Project identity is exposed"), Result->HasField(TEXT("projectFile")));
		TestTrue(TEXT("Dirty packages are exposed"), Result->HasField(TEXT("dirtyPackages")));
		TestEqual(TEXT("Current map is reported"), Result->GetStringField(TEXT("map")), Before->GetOutermost()->GetName());
	}
	TestTrue(TEXT("Context inspection never replaces the map"), Before == GEditor->GetEditorWorldContext().World());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCityTerrainDirtyGuardTest,
	"Hansa.Editor.Terrain.DirtyWorkBlocksImport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCityTerrainDirtyGuardTest::RunTest(const FString&)
{
	// Small in-memory fixture; does not download survey data, import terrain, or save a package.
	UPackage* Fixture = CreatePackage(*FString::Printf(TEXT("/Game/Hansa/Generated/Staging/AutomationGuard_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	UCurveFloat* Asset = NewObject<UCurveFloat>(Fixture, TEXT("UnsavedFixture"), RF_Public | RF_Standalone);
	Fixture->SetDirtyFlag(true);
	// A fixture setup failure must never fall through into a real import.
	const FString Context = UHansaCityTerrainToolset::InspectAuthoringContext();
	if (!Context.Contains(Fixture->GetName()))
	{
		Fixture->SetDirtyFlag(false);
		Asset->ClearFlags(RF_Public | RF_Standalone);
		AddError(TEXT("Dirty fixture was not discovered; importer was NOT called"));
		return false;
	}
	UWorld* Before = GEditor->GetEditorWorldContext().World();
	const FString Text = UHansaCityTerrainToolset::ImportRostockSurveyDraft();
	Fixture->SetDirtyFlag(false);
	Asset->ClearFlags(RF_Public | RF_Standalone);
	TSharedPtr<FJsonObject> Result;
	TestTrue(TEXT("Guard result is structured JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result));
	if (Result)
	{
		TestFalse(TEXT("Dirty work rejects import"), Result->GetBoolField(TEXT("ok")));
		TestTrue(TEXT("Failure identifies unsaved packages"), Result->GetStringField(TEXT("error")).Contains(TEXT("dirty packages")));
	}
	TestTrue(TEXT("Rejected import preserves current map"), Before == GEditor->GetEditorWorldContext().World());
	return true;
}
#endif
