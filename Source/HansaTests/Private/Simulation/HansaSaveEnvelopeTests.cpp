#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Save/HansaSaveEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION
namespace Hansa::Tests::Save
{
	using namespace Hansa::Simulation;
	FHansaSaveSnapshot Capture(const FHansaProductionFixture& Fixture)
	{
		FHansaSaveSnapshot S;
		S.State = Fixture.GetState(); S.BuildVersion = TEXT("S11-P01-test");
		S.SavedUtc = TEXT("2026-09-06T00:00:00Z"); S.DisplayName = TEXT("Lübeck – round trip");
		for (const auto& H : S.State.CreateReadOnlyAccess(Fixture.GetDefinitions()).GetHouses()) S.Players.Add({H.Id.GetValue(), H.Id});
		return S;
	}
	void Sign(TArray<uint8>& Bytes)
	{
		uint8 Digest[20]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num() - 20, Digest);
		FMemory::Memcpy(Bytes.GetData() + Bytes.Num() - 20, Digest, 20);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveContinuationTest, "Hansa.Integration.Save.RoundTripContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveContinuationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::Save;
	for (int32 Kind = 0; Kind < 3; ++Kind)
	{
		auto Created = Kind == 0 ? FHansaProductionFixture::TryCreate() : Kind == 1 ?
			FHansaProductionFixture::TryCreateGrainShortage() : FHansaProductionFixture::TryCreateRouteDelivery();
		if (!TestTrue(TEXT("Fixture created"), Created.IsSuccess())) return false;
		auto Fixture = MoveTemp(Created.Value);
		for (int32 Checkpoint = 0; Checkpoint < 12; ++Checkpoint)
		{
			if (!TestTrue(TEXT("Reach nontrivial checkpoint"), Fixture.Step(3).IsSuccess())) return false;
			auto Saved = Capture(Fixture);
			const auto& D = Fixture.GetDefinitions();
			const auto Before = Saved.State.CreateReadOnlyAccess(D);
			FHansaCommandHeader H;
			H.CommandId = FHansaCommandId::TryCreate(Before.GetLastProcessedCommandId().GetValue() + 1).Value;
			H.GlobalSequence = Before.GetLastProcessedCommandSequence() + 1;
			H.RequestedExecutionTick = Before.GetClock().GetTick();
			H.Authority = {Saved.Players[0].HouseId, Saved.Players[0].PrincipalId, EHansaCommandOrigin::PlayerInput};
			Saved.PendingCommands.Add(FHansaGameplayCommand::Create(H, FHansaNoOpTestCommand{MAX_int64}));
			TArray<uint8> Bytes;
			const auto Encoded = FHansaSaveEnvelope::Encode(Saved, D, Bytes);
			if (!TestTrue(*Encoded.Message, Encoded.IsSuccess())) return false;
			FHansaSaveSnapshot Loaded;
			const auto Decoded = FHansaSaveEnvelope::Decode(Bytes, D, Loaded);
			if (!TestTrue(*Decoded.Message, Decoded.IsSuccess())) return false;
			TestEqual(TEXT("Authoritative checksum"), Decoded.AuthoritativeHash, Fixture.BuildStateHashes().GetOverallHash());
			TestEqual(TEXT("Campaign checksum includes pending commands"), Decoded.CampaignHash, Encoded.CampaignHash);
			TestEqual(TEXT("Unicode display metadata"), Loaded.DisplayName, Saved.DisplayName);
			TArray<uint8> Reencoded;
			TestTrue(TEXT("Encode restored copy"), FHansaSaveEnvelope::Encode(Loaded, D, Reencoded).IsSuccess());
			TestTrue(TEXT("Canonical byte round trip"), Bytes == Reencoded);
			FHansaSimulationTransientCache OriginalCache, RestoredCache;
			for (int32 Tick = 0; Tick < 15; ++Tick)
			{
				const TConstArrayView<FHansaGameplayCommand> Commands = Tick == 0 ? MakeArrayView(Saved.PendingCommands) : TConstArrayView<FHansaGameplayCommand>();
				const auto A = FHansaGameplayCommandGateway::ExecuteTick(Saved.State, D, Commands, OriginalCache);
				const auto B = FHansaGameplayCommandGateway::ExecuteTick(Loaded.State, D, Commands, RestoredCache);
				TestTrue(TEXT("Both timelines advance"), A.IsSuccess() && B.IsSuccess());
				TestTrue(TEXT("Deterministic continuation with rebuilt cache"), A.GetFingerprintAfter() == B.GetFingerprintAfter());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveRejectionTest, "Hansa.Integration.Save.CorruptionCompatibilityAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveRejectionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::Save;
	auto F = FHansaProductionFixture::TryCreate(); if (!F) return false;
	auto S = Capture(F.Value); const auto& D = F.Value.GetDefinitions();
	TArray<uint8> Bytes; if (!FHansaSaveEnvelope::Encode(S, D, Bytes)) return false;
	FHansaSaveSnapshot Target = S; Target.DisplayName = TEXT("Do not replace on failure");
	for (int32 Index : {0, 4, Bytes.Num() / 2, Bytes.Num() - 1})
	{
		auto Bad = Bytes; Bad[Index] ^= 0x80;
		TestTrue(TEXT("Bit corruption rejected"), FHansaSaveEnvelope::Decode(Bad, D, Target).Error == EHansaSaveError::CorruptData);
	}
	for (int32 Size : {0, 1, 27, Bytes.Num() - 1})
		TestFalse(TEXT("Truncation rejected"), FHansaSaveEnvelope::Decode(MakeArrayView(Bytes.GetData(), Size), D, Target).IsSuccess());
	auto Bad = Bytes; Bad.Add(0);
	TestFalse(TEXT("Trailing byte rejected"), FHansaSaveEnvelope::Decode(Bad, D, Target).IsSuccess());
	Bad = Bytes; Bad[4] = 99; Sign(Bad);
	TestTrue(TEXT("Future format has explicit error"), FHansaSaveEnvelope::Decode(Bad, D, Target).Error == EHansaSaveError::UnsupportedFormat);
	Bad = Bytes; Bad[8] = 99; Sign(Bad);
	TestTrue(TEXT("Simulation mismatch explicit"), FHansaSaveEnvelope::Decode(Bad, D, Target).Error == EHansaSaveError::IncompatibleSimulation);
	Bad = Bytes; Bad[20] ^= 1; Sign(Bad);
	TestTrue(TEXT("Content mismatch explicit"), FHansaSaveEnvelope::Decode(Bad, D, Target).Error == EHansaSaveError::IncompatibleContent);
	TestEqual(TEXT("Failures leave caller state unchanged"), Target.DisplayName, FString(TEXT("Do not replace on failure")));
	const auto DuplicatePlayer = S.Players[0]; S.Players.Add(DuplicatePlayer); TArray<uint8> Untouched = {1,2,3};
	TestTrue(TEXT("Duplicate principal rejected"), FHansaSaveEnvelope::Encode(S, D, Untouched).Error == EHansaSaveError::InvalidSnapshot);
	TestTrue(TEXT("Failed encoding atomic"), Untouched == TArray<uint8>({1,2,3}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveMigrationTest, "Hansa.Integration.Save.SyntheticPriorSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveMigrationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::Save;
	auto F = FHansaProductionFixture::TryCreate(); if (!F) return false;
	const auto& D = F.Value.GetDefinitions();
	TArray<uint8> Bytes;
	const FString GoldenPath = FPaths::ProjectDir() / TEXT("Tests/Fixtures/save_envelope_v1.hansa");
	if (!TestTrue(TEXT("Load checked-in synthetic v1 fixture"), FFileHelper::LoadFileToArray(Bytes, *GoldenPath))) return false;
	FHansaSaveSnapshot Loaded;
	const auto R = FHansaSaveEnvelope::Decode(Bytes, D, Loaded);
	if (!TestTrue(*R.Message, R.IsSuccess())) return false;
	TestEqual(TEXT("Prior format reported"), R.SourceFormatVersion, 1U);
	TestEqual(TEXT("One explicit migration"), R.AppliedMigrations.Num(), 1);
	TestEqual(TEXT("Migration lineage captured"), Loaded.MigrationHistory.Num(), 1);
	TestEqual(TEXT("Deterministic display default"), Loaded.DisplayName, D.GetScenarioId().ToString());
	TestEqual(TEXT("Golden state unchanged"), R.AuthoritativeHash, F.Value.BuildStateHashes().GetOverallHash());
	TArray<uint8> Current;
	TestTrue(TEXT("Migrated save writes current version"), FHansaSaveEnvelope::Encode(Loaded, D, Current).IsSuccess());
	FHansaSaveSnapshot Again;
	const auto R2 = FHansaSaveEnvelope::Decode(Current, D, Again);
	TestTrue(TEXT("Migration is not reapplied"), R2.IsSuccess() && R2.AppliedMigrations.IsEmpty());
	TestEqual(TEXT("Migration preserves campaign state"), R2.CampaignHash, R.CampaignHash);
	TestTrue(TEXT("Migration lineage persists in current save"), Again.MigrationHistory == Loaded.MigrationHistory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveMvpBudgetTest, "Hansa.Integration.Performance.MvpEnvelopeSizeAndTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveMvpBudgetTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::Save;
	(void)Parameters;
	auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!TestTrue(TEXT("MVP save benchmark fixture initializes"), Created.IsSuccess())) return false;
	FHansaProductionFixture Fixture = MoveTemp(Created.Value);
	if (!TestTrue(TEXT("MVP save benchmark reaches a nontrivial 256-tick state"), Fixture.Step(256).IsSuccess())) return false;
	const FHansaSaveSnapshot Snapshot = Capture(Fixture);
	const auto& Definitions = Fixture.GetDefinitions();
	constexpr int32 Iterations = 100;
	TArray<uint8> CanonicalBytes;
	bool bAllRoundTripsSucceeded = true;
	bool bAllBytesCanonical = true;
	const double StartedAt = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < Iterations; ++Index)
	{
		TArray<uint8> Bytes;
		const FHansaSaveResult Encoded = FHansaSaveEnvelope::Encode(Snapshot, Definitions, Bytes);
		FHansaSaveSnapshot Loaded;
		const FHansaSaveResult Decoded = Encoded.IsSuccess()
			? FHansaSaveEnvelope::Decode(Bytes, Definitions, Loaded)
			: FHansaSaveResult{Encoded.Error, Encoded.Message};
		bAllRoundTripsSucceeded &= Encoded.IsSuccess() && Decoded.IsSuccess();
		if (Index == 0)
		{
			CanonicalBytes = Bytes;
		}
		else
		{
			bAllBytesCanonical &= Bytes == CanonicalBytes;
		}
	}
	const double ElapsedMilliseconds = (FPlatformTime::Seconds() - StartedAt) * 1000.0;
	AddInfo(FString::Printf(
		TEXT("S14-P03 MVP save: %d bytes, %d encode/decode iterations, %.3f ms total, %.3f ms/round-trip"),
		CanonicalBytes.Num(), Iterations, ElapsedMilliseconds, ElapsedMilliseconds / Iterations));
	TestTrue(TEXT("One hundred MVP save encode/decode round trips succeed"), bAllRoundTripsSucceeded);
	TestTrue(TEXT("Repeated MVP save encoding remains byte-canonical"), bAllBytesCanonical);
	TestTrue(TEXT("MVP save remains inside the explicit 64 MiB envelope limit"),
		CanonicalBytes.Num() > 0 && CanonicalBytes.Num() <= FHansaSaveEnvelope::MaximumBytes);
	return !HasAnyErrors();
}
#endif



