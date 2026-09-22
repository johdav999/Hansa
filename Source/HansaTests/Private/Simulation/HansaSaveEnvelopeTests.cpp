#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Save/HansaSaveEnvelope.h"
#include "Diagnostics/HansaStateHash.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveStationOrderMigrationTest, "Hansa.Integration.Save.StationOrdersPriorFormat14",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveStationOrderMigrationTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Save;
 auto F=FHansaProductionFixture::TryCreate();if(!F)return false;const auto& D=F.Value.GetDefinitions();
 auto S=Capture(F.Value);TArray<uint8> Bytes;auto Encoded=FHansaSaveEnvelope::Encode(S,D,Bytes);if(!TestTrue(TEXT("Current empty-order archive encodes"),Encoded.IsSuccess()))return false;
 TestTrue(TEXT("No station records in synthetic migration source"),S.State.CreateReadOnlyAccess(D).GetTradeStations().IsEmpty());
 // With no station records or pending commands, v14 and v15 payload layouts are identical.
 // Reconstruct the genuine previous header and checksum, not a bypass of decode validation.
 const auto Put=[&](int32 Offset,uint64 Value,int32 Count){for(int32 I=0;I<Count;++I)Bytes[Offset+I]=static_cast<uint8>(Value>>(I*8));};
 Put(4,14,4);Put(16,27,4);
 int32 Offset=44;
 const auto Read32=[&](int32 At){uint32 V=0;for(int32 I=0;I<4;++I)V|=uint32(Bytes[At+I])<<(I*8);return V;};
 for(int32 I=0;I<4;++I){const int32 Length=Read32(Offset);Offset+=4+Length*2;}
 const int32 Migrations=Read32(Offset);Offset+=4;
 for(int32 I=0;I<Migrations;++I){const int32 Length=Read32(Offset);Offset+=4+Length*2;}
 Put(Offset,FHansaStateHasher::ComputeSavedVersion(S.State,D,27).GetOverallHash(),8);Sign(Bytes);
 FHansaSaveSnapshot Loaded;auto Result=FHansaSaveEnvelope::Decode(Bytes,D,Loaded);if(!TestTrue(*Result.Message,Result.IsSuccess()))return false;
 TestTrue(TEXT("v14 gets explicit order migration"),Result.AppliedMigrations.Contains(TEXT("Hansa.Save.14To15.AddStationOrders")));
 TestEqual(TEXT("Migration preserves existing gameplay"),Result.AuthoritativeHash,Encoded.AuthoritativeHash);
 TArray<uint8> Again;TestTrue(TEXT("Migrated archive resaves"),FHansaSaveEnvelope::Encode(Loaded,D,Again).IsSuccess());
 FHansaSaveSnapshot Twice;TestTrue(TEXT("Migration is idempotent"),FHansaSaveEnvelope::Decode(Again,D,Twice).AppliedMigrations.IsEmpty());return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveEveryIntermediateMigrationTest,"Hansa.Integration.Save.EveryIntermediateVersionRecovery",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaSaveEveryIntermediateMigrationTest::RunTest(const FString&)
{
 using namespace Hansa::Tests::Save;
 auto F=FHansaProductionFixture::TryCreate();if(!TestTrue(TEXT("Historical fixture source creates"),F.IsSuccess()))return false;const auto& D=F.Value.GetDefinitions();const auto S=Capture(F.Value);
 struct FPair{uint32 Format;uint32 Fingerprint;};const FPair Pairs[]={{1,16},{2,16},{3,16},{4,17},{5,18},{6,19},{7,20},{8,21},{9,22},{10,23},{11,24},{12,25},{13,26},{14,27},{15,28},{16,28},{17,29},{18,30},{19,31},{20,32}};
 for(const FPair Pair:Pairs)
 {
  TArray<uint8> Bytes;const auto Written=FHansaSaveEnvelope::EncodeHistoricalFixtureForTests(S,D,Pair.Format,Pair.Fingerprint,Bytes);if(!TestTrue(FString::Printf(TEXT("Format %u fixture writes genuine body"),Pair.Format),Written.IsSuccess()))return false;
  FHansaSaveMetadata Metadata;const auto DryRun=FHansaSaveEnvelope::InspectMetadata(Bytes,Metadata);TestTrue(FString::Printf(TEXT("Format %u dry-run metadata succeeds"),Pair.Format),DryRun.IsSuccess());TestEqual(FString::Printf(TEXT("Format %u dry-run reports source"),Pair.Format),Metadata.FormatVersion,Pair.Format);
  FHansaSaveSnapshot Loaded;Loaded.DisplayName=TEXT("unchanged-until-commit");const auto Migrated=FHansaSaveEnvelope::Decode(Bytes,D,Loaded);if(!TestTrue(FString::Printf(TEXT("Format %u applies safely: %s"),Pair.Format,*Migrated.Message),Migrated.IsSuccess()))return false;
  TestEqual(FString::Printf(TEXT("Format %u source is retained"),Pair.Format),Migrated.SourceFormatVersion,Pair.Format);TestTrue(FString::Printf(TEXT("Format %u records migration lineage"),Pair.Format),!Migrated.AppliedMigrations.IsEmpty());
  TArray<uint8> Current;if(!TestTrue(TEXT("Migrated state re-encodes current"),FHansaSaveEnvelope::Encode(Loaded,D,Current).IsSuccess()))return false;FHansaSaveSnapshot Again;const auto Repeated=FHansaSaveEnvelope::Decode(Current,D,Again);
  TestTrue(FString::Printf(TEXT("Format %u repeated load is idempotent"),Pair.Format),Repeated.IsSuccess()&&Repeated.AppliedMigrations.IsEmpty());TestEqual(FString::Printf(TEXT("Format %u repeated hash is stable"),Pair.Format),Repeated.AuthoritativeHash,Migrated.AuthoritativeHash);
 }
 return !HasAnyErrors();
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
            for (const auto& Cohort : Before.BuildPopulationProjection())
            {
                const auto Restored = Loaded.State.CreateReadOnlyAccess(D).QueryPopulationCohort(Cohort.Id);
                if (!TestTrue(TEXT("Residence restored"), Restored.IsSet())) return false;
                TestEqual(TEXT("Residence coverage restored"), Restored->Consumption.CoveredMinutes, Cohort.Consumption.CoveredMinutes);
                TestEqual(TEXT("Residence goods restored"), Restored->Consumption.Goods.Num(), Cohort.Consumption.Goods.Num());
                for (int32 I = 0; I < Cohort.Consumption.Goods.Num(); ++I)
                {
                    TestEqual(TEXT("Residence required sum restored"), Restored->Consumption.Goods[I].Required, Cohort.Consumption.Goods[I].Required);
                    TestEqual(TEXT("Residence consumed sum restored"), Restored->Consumption.Goods[I].Consumed, Cohort.Consumption.Goods[I].Consumed);
                }
            }
            const auto ConsumptionBefore = Before.BuildProjection().Value.GetCitizenConsumption();
            const auto ConsumptionAfter = Loaded.State.CreateReadOnlyAccess(D).BuildProjection().Value.GetCitizenConsumption();
            TestEqual(TEXT("Recorded window survives save"), ConsumptionAfter.CoveredMinutes, ConsumptionBefore.CoveredMinutes);
            TestEqual(TEXT("Goods history survives save"), ConsumptionAfter.Goods.Num(), ConsumptionBefore.Goods.Num());
            for (int32 I = 0; I < ConsumptionBefore.Goods.Num(); ++I)
            {
                TestEqual(TEXT("Required sum restored"), ConsumptionAfter.Goods[I].Required, ConsumptionBefore.Goods[I].Required);
                TestEqual(TEXT("Consumed sum restored"), ConsumptionAfter.Goods[I].Consumed, ConsumptionBefore.Goods[I].Consumed);
            }
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
    TestTrue(TEXT("Station order migration is explicit"),R.AppliedMigrations.Contains(TEXT("Hansa.Save.14To15.AddStationOrders")));
	TestEqual(TEXT("Eighteen explicit migrations through recoverable station interruptions"), R.AppliedMigrations.Num(), 18);
    TestTrue(TEXT("Merchant-office specialization migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.17To18.AddMerchantOfficeSpecializationsAndPriceLimits")));
    TestTrue(TEXT("Bounded foreign-construction migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.18To19.AddBoundedForeignConstructionRights")));
    TestTrue(TEXT("City privilege/project/charter migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.19To20.AddCityPrivilegesProjectsAndCharters")));
	TestTrue(TEXT("Recoverable station interruption migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.20To21.AddRecoverableTradeStationInterruptions")));
	TestTrue(TEXT("Presence progression migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.16To17.AddPresenceProgression")));
    TestTrue(TEXT("Heating migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.7To8.DefaultHouseholdHeatingPolicy")));
	TestTrue(TEXT("Foreign presence migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.11To12.SeedAuthoredForeignPresence")));
	TestTrue(TEXT("Trade-station migration named explicitly"), R.AppliedMigrations.Contains(TEXT("Hansa.Save.13To14.AddTradeStationLifecycle")));
    TestEqual(TEXT("Legacy campaign without Rostock receives no orphaned presence"), Loaded.State.CreateReadOnlyAccess(D).GetForeignPresences().Num(), 0);
	TestEqual(TEXT("Migration lineage captured"), Loaded.MigrationHistory.Num(), 18);
	TestEqual(TEXT("Deterministic display default"), Loaded.DisplayName, D.GetScenarioId().ToString());
	TestEqual(TEXT("Navigation migration preserves the reviewed legacy gameplay checksum"),
        FHansaStateHasher::ComputeSavedVersion(Loaded.State,D,22).GetOverallHash(),448186139431435968ULL);
	TestEqual(TEXT("Migrated v1 state preserves the reviewed fingerprint-v23 checksum"),
		FHansaStateHasher::ComputeSavedVersion(Loaded.State, D, 23).GetOverallHash(), 17984526814460210456ULL);
	TestEqual(TEXT("Migration retains the reviewed fingerprint-v27 checksum"),
		FHansaStateHasher::ComputeSavedVersion(Loaded.State, D, 27).GetOverallHash(), 17181715998215868145ULL);
	TArray<uint8> Current;
	TestTrue(TEXT("Migrated save writes current version"), FHansaSaveEnvelope::Encode(Loaded, D, Current).IsSuccess());
	FHansaSaveSnapshot Again;
	const auto R2 = FHansaSaveEnvelope::Decode(Current, D, Again);
	TestTrue(TEXT("Migration is not reapplied"), R2.IsSuccess() && R2.AppliedMigrations.IsEmpty());
	TestEqual(TEXT("Migration preserves authoritative gameplay state"), R2.AuthoritativeHash, R.AuthoritativeHash);
	TestTrue(TEXT("Migration lineage persists in current save"), Again.MigrationHistory == Loaded.MigrationHistory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSaveV2RouteLabels, "Hansa.Integration.Save.Format2RouteLabelMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSaveV2RouteLabels::RunTest(const FString&)
{
    using namespace Hansa::Tests::Save;
    auto F=FHansaProductionFixture::TryCreate();if(!F)return false;
    TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/TEXT("Tests/Fixtures/save_envelope_v2.hansa"))))return false;
    FHansaSaveSnapshot Loaded;
    const auto Result=FHansaSaveEnvelope::Decode(Bytes,F.Value.GetDefinitions(),Loaded);
    TestTrue(*Result.Message,!!Result);if(!Result)return false;
    TestEqual(TEXT("Prior format 2 recognized"),Result.SourceFormatVersion,2U);
    TestTrue(TEXT("Order migration present"),Result.AppliedMigrations.Contains(TEXT("Hansa.Save.14To15.AddStationOrders")));
    TestTrue(TEXT("Route-target migration present"),Result.AppliedMigrations.Contains(TEXT("Hansa.Save.15To16.ExplicitRouteTargets")));
	TestEqual(TEXT("Catalog through recoverable station interruptions"),Result.AppliedMigrations.Num(),17);
	TestTrue(TEXT("Recovery migration present"),Result.AppliedMigrations.Contains(TEXT("Hansa.Save.20To21.AddRecoverableTradeStationInterruptions")));
    TestTrue(TEXT("Prior campaigns get no invented labels"),Loaded.RouteLabels.IsEmpty());
    TArray<uint8> Current;TestTrue(TEXT("Migrated save encodes"),!!FHansaSaveEnvelope::Encode(Loaded,F.Value.GetDefinitions(),Current));
    FHansaSaveSnapshot Again;const auto R=FHansaSaveEnvelope::Decode(Current,F.Value.GetDefinitions(),Again);
    TestTrue(TEXT("Migration is idempotent"),R&&R.AppliedMigrations.IsEmpty());
    TestEqual(TEXT("Authoritative state preserved"),R.AuthoritativeHash,Result.AuthoritativeHash);
    return !HasAnyErrors();
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

