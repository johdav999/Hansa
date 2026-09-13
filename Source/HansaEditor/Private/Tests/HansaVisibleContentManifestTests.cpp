#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Misc/AutomationTest.h"
#include "Validation/HansaVisibleContentManifest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using namespace Hansa::Editor::VisibleContent;

	TArray<const UHansaDefinitionBase*> RawDefinitions(
		const TArray<TStrongObjectPtr<UHansaDefinitionBase>>& Definitions)
	{
		TArray<const UHansaDefinitionBase*> Result;
		Result.Reserve(Definitions.Num());
		for (const TStrongObjectPtr<UHansaDefinitionBase>& Definition : Definitions)
		{
			Result.Add(Definition.Get());
		}
		return Result;
	}

	bool ContainsIssue(const TArray<FIssue>& Issues, const FString& Code)
	{
		return Issues.ContainsByPredicate([&Code](const FIssue& Issue) { return Issue.Code == Code; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaVisibleContentManifestCompletenessTest,
	"Hansa.Content.VisibleManifest.Completeness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaVisibleContentManifestCompletenessTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::VisibleContent;
	FManifest Manifest;
	FString Error;
	if (!TestTrue(TEXT("Checked-in visible-content manifest parses"), FManifestValidator::LoadProjectManifest(Manifest, Error)))
	{
		AddError(Error);
		return false;
	}
	const TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	const TArray<const UHansaDefinitionBase*> Raw = RawDefinitions(Definitions);
	const TArray<FIssue> CompletenessIssues = FManifestValidator::Validate(
		Manifest, EValidationMode::Completeness, Raw);
	for (const FIssue& Issue : CompletenessIssues)
	{
		AddError(FString::Printf(TEXT("[%s] %s: %s"), *Issue.Code, *Issue.StableId, *Issue.Message));
	}

	// The current audited manifest deliberately contains each release-blocking class. These assertions
	// prove the strict validator cannot silently accept missing, staging, Engine-fallback, or incomplete UI.
	const TArray<FIssue> ReleaseIssues = FManifestValidator::Validate(
		Manifest, EValidationMode::ReleaseReadiness, Raw);
	TestTrue(TEXT("Strict gate rejects a golden definition with no presentation"),
		ContainsIssue(ReleaseIssues, TEXT("HVCM-DEF-001")));
    FManifest StagingFixture = Manifest;
    FEntry ForbiddenEntry;
    ForbiddenEntry.StableId = TEXT("Presentation.Test.ForbiddenStaging");
    ForbiddenEntry.CurrentReference = TEXT("/Game/Hansa/Generated/Staging/Test.Asset");
    ForbiddenEntry.Status = TEXT("staging-only");
    ForbiddenEntry.bGoldenPath = true;
    StagingFixture.Entries.Add(ForbiddenEntry);
    TestTrue(TEXT("Strict gate rejects explicit staging fixture after production promotion"),
        ContainsIssue(FManifestValidator::Validate(StagingFixture, EValidationMode::ReleaseReadiness, Raw), TEXT("HVCM-PATH-001")));
	TestTrue(TEXT("Strict gate rejects Engine basic-shape fallback"),
		ContainsIssue(ReleaseIssues, TEXT("HVCM-PATH-002")));
	TestTrue(TEXT("Strict gate rejects a missing required UI state"),
		ContainsIssue(ReleaseIssues, TEXT("HVCM-UI-001")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaVisibleContentReleaseReadinessTest,
	"Hansa.Release.VisibleManifest.Readiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaVisibleContentReleaseReadinessTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Editor::VisibleContent;
	FManifest Manifest;
	FString Error;
	if (!FManifestValidator::LoadProjectManifest(Manifest, Error))
	{
		AddError(Error);
		return false;
	}
	const TArray<TStrongObjectPtr<UHansaDefinitionBase>> Definitions =
		Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
	const TArray<const UHansaDefinitionBase*> Raw = RawDefinitions(Definitions);
	const TArray<FIssue> Issues = FManifestValidator::Validate(
		Manifest, EValidationMode::ReleaseReadiness, Raw);
	TMap<FString, int32> CountsByCode;
	for (const FIssue& Issue : Issues)
	{
		++CountsByCode.FindOrAdd(Issue.Code);
		AddInfo(FString::Printf(TEXT("[%s] %s: %s"), *Issue.Code, *Issue.StableId, *Issue.Message));
	}
	if (!Issues.IsEmpty())
	{
		TArray<FString> Codes;
		CountsByCode.GetKeys(Codes);
		Codes.Sort();
		TArray<FString> Summary;
		for (const FString& Code : Codes)
		{
			Summary.Add(FString::Printf(TEXT("%s=%d"), *Code, CountsByCode[Code]));
		}
		AddError(FString::Printf(
			TEXT("Visible-content release readiness has %d blockers (%s). See Info diagnostics for exact stable IDs, owners, and canonical destinations."),
			Issues.Num(), *FString::Join(Summary, TEXT(", "))));
	}
	return !HasAnyErrors();
}

#endif
