#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "World/HansaRuntimeSimulationHost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaResidenceConsumptionMigrationTest,
    "Hansa.Integration.Save.StarterBalanceCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaResidenceConsumptionMigrationTest::RunTest(const FString&)
{
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;
    if (!TestTrue(TEXT("Initialize current runtime"), Host->InitializeForLubeck(nullptr, Error))) return false;
    TArray<uint8> Before;
    const auto Saved = Host->CaptureSaveBytes(Before, TEXT("Before incompatible load"), TEXT("2026-09-12T00:00:00Z"));
    if (!TestTrue(*Saved.Message, Saved.IsSuccess())) return false;
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Read preserved prior economy save"), FFileHelper::LoadFileToArray(Bytes,
        *(FPaths::ProjectDir()/TEXT("Tests/Fixtures/residence_consumption_prior_v4.hansa"))))) return false;
    const auto Result = Host->RestoreSaveBytes(Bytes);
    TestFalse(TEXT("Old balance is not silently reinterpreted"), Result.IsSuccess());
    TArray<uint8> After;
    const auto Unchanged = Host->CaptureSaveBytes(After, TEXT("After incompatible load"), TEXT("2026-09-12T00:00:00Z"));
    TestTrue(TEXT("Current city still saves"), Unchanged.IsSuccess());
    TestEqual(TEXT("Rejected old balance leaves current state unchanged"), Unchanged.AuthoritativeHash, Saved.AuthoritativeHash);
    const auto Again = Host->RestoreSaveBytes(Before);
    TestTrue(TEXT("Current catalog reloads without migration"), Again.IsSuccess() && Again.AppliedMigrations.IsEmpty());
    TestEqual(TEXT("Current history hash survives round trip"), Again.AuthoritativeHash, Saved.AuthoritativeHash);
    return !HasAnyErrors();
}
#endif
