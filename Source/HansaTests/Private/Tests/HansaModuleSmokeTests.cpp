#include "HansaAutomationModule.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Modules
{
	bool LoadTextFile(FAutomationTestBase& Test, const FString& Path, FString& OutText)
	{
		if (!Test.TestTrue(
			FString::Printf(TEXT("Expected source file exists: %s"), *Path),
			IFileManager::Get().FileExists(*Path)))
		{
			return false;
		}

		return Test.TestTrue(
			FString::Printf(TEXT("Source file is readable: %s"), *Path),
			FFileHelper::LoadFileToString(OutText, *Path));
	}

	bool VerifyModuleType(
		FAutomationTestBase& Test,
		const TArray<TSharedPtr<FJsonValue>>& Modules,
		const FString& ModuleName,
		const FString& ExpectedType)
	{
		for (const TSharedPtr<FJsonValue>& ModuleValue : Modules)
		{
			const TSharedPtr<FJsonObject> ModuleObject = ModuleValue->AsObject();
			if (!ModuleObject.IsValid())
			{
				continue;
			}

			FString Name;
			if (ModuleObject->TryGetStringField(TEXT("Name"), Name) && Name == ModuleName)
			{
				FString Type;
				if (!Test.TestTrue(
					FString::Printf(TEXT("%s has a module type"), *ModuleName),
					ModuleObject->TryGetStringField(TEXT("Type"), Type)))
				{
					return false;
				}

				return Test.TestEqual(
					FString::Printf(TEXT("%s host type"), *ModuleName),
					Type,
					ExpectedType);
			}
		}

		Test.AddError(FString::Printf(TEXT("Module descriptor not found: %s"), *ModuleName));
		return false;
	}

	void VerifyForbiddenDependency(
		FAutomationTestBase& Test,
		const FString& RulesText,
		const FString& OwnerModule,
		const FString& ForbiddenModule)
	{
		Test.TestFalse(
			FString::Printf(TEXT("%s must not depend on %s"), *OwnerModule, *ForbiddenModule),
			RulesText.Contains(FString::Printf(TEXT("\"%s\""), *ForbiddenModule)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaModuleLoadabilityTest,
	"Hansa.Architecture.Modules.Loadability",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::ClientContext |
		EAutomationTestFlags::EngineFilter)

bool FHansaModuleLoadabilityTest::RunTest(const FString& Parameters)
{
	FModuleManager& ModuleManager = FModuleManager::Get();

	TestNotNull(
		TEXT("HansaSimulation loads"),
		ModuleManager.LoadModulePtr<IModuleInterface>(TEXT("HansaSimulation")));
	TestNotNull(
		TEXT("Hansa loads"),
		ModuleManager.LoadModulePtr<IModuleInterface>(TEXT("Hansa")));

#if WITH_HANSA_AUTOMATION
	TestNotNull(
		TEXT("HansaAutomation loads in an approved development target"),
		ModuleManager.LoadModulePtr<IModuleInterface>(TEXT("HansaAutomation")));
	TestTrue(TEXT("HansaAutomation reports available"), FHansaAutomationModule::IsAvailable());

	if (!FParse::Param(FCommandLine::Get(), TEXT("HansaAutomation")))
	{
		TestFalse(
			TEXT("Automation transport is not requested by the default command line"),
			FHansaAutomationModule::Get().IsTransportRequested());
	}
#endif

#if WITH_EDITOR
	TestNotNull(
		TEXT("HansaEditor loads in an Editor target"),
		ModuleManager.LoadModulePtr<IModuleInterface>(TEXT("HansaEditor")));
#endif

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaModuleBoundaryTest,
	"Hansa.Architecture.Modules.Boundaries",
	EAutomationTestFlags::EditorContext |
		EAutomationTestFlags::EngineFilter)

bool FHansaModuleBoundaryTest::RunTest(const FString& Parameters)
{
	const FString ProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectJson;
	if (!Hansa::Tests::Modules::LoadTextFile(*this, ProjectFilePath, ProjectJson))
	{
		return false;
	}

	TSharedPtr<FJsonObject> ProjectObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ProjectJson);
	if (!TestTrue(TEXT("Hansa.uproject contains valid JSON"), FJsonSerializer::Deserialize(Reader, ProjectObject)) ||
		!ProjectObject.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
	if (!TestTrue(TEXT("Hansa.uproject declares modules"), ProjectObject->TryGetArrayField(TEXT("Modules"), Modules)) ||
		Modules == nullptr)
	{
		return false;
	}

	Hansa::Tests::Modules::VerifyModuleType(*this, *Modules, TEXT("HansaSimulation"), TEXT("Runtime"));
	Hansa::Tests::Modules::VerifyModuleType(*this, *Modules, TEXT("Hansa"), TEXT("Runtime"));
	Hansa::Tests::Modules::VerifyModuleType(*this, *Modules, TEXT("HansaEditor"), TEXT("Editor"));
	Hansa::Tests::Modules::VerifyModuleType(*this, *Modules, TEXT("HansaAutomation"), TEXT("DeveloperTool"));
	Hansa::Tests::Modules::VerifyModuleType(*this, *Modules, TEXT("HansaTests"), TEXT("DeveloperTool"));

	const FString SourceRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));
	const TMap<FString, TArray<FString>> ForbiddenDependencies = {
		{ TEXT("HansaSimulation"), { TEXT("CoreUObject"), TEXT("Engine"), TEXT("Hansa"), TEXT("HansaEditor"), TEXT("HansaAutomation"), TEXT("HansaTests"), TEXT("Slate"), TEXT("SlateCore"), TEXT("UMG"), TEXT("UnrealEd") } },
		{ TEXT("Hansa"), { TEXT("HansaEditor"), TEXT("HansaAutomation"), TEXT("HansaTests"), TEXT("UnrealEd") } },
		{ TEXT("HansaEditor"), { TEXT("HansaAutomation"), TEXT("HansaTests") } },
		{ TEXT("HansaAutomation"), { TEXT("HansaEditor"), TEXT("HansaTests"), TEXT("UnrealEd") } },
		{ TEXT("HansaTests"), { TEXT("HansaEditor"), TEXT("UnrealEd") } }
	};

	for (const TPair<FString, TArray<FString>>& Entry : ForbiddenDependencies)
	{
		const FString RulesPath = FPaths::Combine(SourceRoot, Entry.Key, Entry.Key + TEXT(".Build.cs"));
		FString RulesText;
		if (!Hansa::Tests::Modules::LoadTextFile(*this, RulesPath, RulesText))
		{
			continue;
		}

		for (const FString& ForbiddenModule : Entry.Value)
		{
			Hansa::Tests::Modules::VerifyForbiddenDependency(
				*this,
				RulesText,
				Entry.Key,
				ForbiddenModule);
		}

		for (const FString& ProviderToken : { TEXT("OpenAI"), TEXT("Tripo"), TEXT("ElevenLabs"), TEXT("TRELLIS") })
		{
			TestFalse(
				FString::Printf(TEXT("%s build rules contain no provider SDK dependency: %s"), *Entry.Key, *ProviderToken),
				RulesText.Contains(ProviderToken));
		}
	}

	const FString DefaultGamePath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini"));
	FString DefaultGameText;
	if (Hansa::Tests::Modules::LoadTextFile(*this, DefaultGamePath, DefaultGameText))
	{
		TestTrue(
			TEXT("Hansa automation uses the checked-in namespaced config section"),
			DefaultGameText.Contains(TEXT("[Hansa.Automation]")));
		TestTrue(
			TEXT("Hansa automation transport is disabled by default"),
			DefaultGameText.Contains(TEXT("bEnableTransport=False")));
		TestTrue(
			TEXT("Hansa automation permission ceiling defaults to read-only"),
			DefaultGameText.Contains(TEXT("MaximumPermission=ReadOnly")));
		TestTrue(
			TEXT("Developer content is excluded from cook"),
			DefaultGameText.Contains(TEXT("/Game/Hansa/Developer")));
		TestTrue(
			TEXT("Generated staging content is excluded from cook"),
			DefaultGameText.Contains(TEXT("/Game/Hansa/Generated/Staging")));
	}

	return !HasAnyErrors();
}

#endif
