#include "Misc/AutomationTest.h"
#include "Multiplayer/HansaSessionModel.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::SessionModel
{
	using namespace Hansa::Simulation;

	template <typename TId> TId Id(uint64 Value) { return TId::TryCreate(Value).Value; }

	struct FFixture final
	{
		FHansaSessionState State;
		TArray<FHansaScenarioSlotRule> Rules;
	};

	FFixture MakeFixture(int32 SlotCount)
	{
		FFixture Fixture;
		Fixture.State.CampaignId = Id<FHansaCampaignId>(1001);
		Fixture.State.SessionId = Id<FHansaSessionId>(2001);
		Fixture.State.Compatibility.ProtocolVersion = 7;
		Fixture.State.Compatibility.ContentManifestHash = 0x123456789abcdef0ULL;
		Fixture.State.Compatibility.ScenarioId = FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value;
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			const FString SlotId = FString::Printf(TEXT("HouseSlot.%02d"), Index + 1);
			const FHansaHouseId HouseId = Id<FHansaHouseId>(Index + 1);
			const bool bHuman = Index == 0;
			Fixture.Rules.Add({SlotId, HouseId, bHuman ? EHansaSessionSlotState::Human : EHansaSessionSlotState::AI,
				0x1f, FHansaTeamId(), false, true});
			Fixture.State.Slots.Add({SlotId, HouseId, bHuman ? EHansaSessionSlotState::Human : EHansaSessionSlotState::AI,
				bHuman ? Id<FHansaParticipantId>(101) : FHansaParticipantId(), FHansaTeamId()});
		}
		Fixture.State.Participants.Add({Id<FHansaParticipantId>(101), EHansaParticipantLifecycle::Active,
			static_cast<uint8>(EHansaParticipantRole::Host) | static_cast<uint8>(EHansaParticipantRole::Admin),
			Id<FHansaHouseId>(1), FHansaTeamId()});
		return Fixture;
	}

	bool HasCode(const TArray<FHansaSessionValidationIssue>& Issues, const TCHAR* Code)
	{
		return Issues.ContainsByPredicate([Code](const FHansaSessionValidationIssue& Issue) { return Issue.Code == Code; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSessionCapacityAndBindingTest,
	"Hansa.Multiplayer.SessionModel.CapacityAndBindings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSessionCapacityAndBindingTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::SessionModel; using namespace Hansa::Simulation;
	for (int32 Count = 2; Count <= 8; ++Count)
	{
		FFixture Fixture = MakeFixture(Count); TArray<FHansaSessionValidationIssue> Issues;
		TestTrue(*FString::Printf(TEXT("%d authored slots validate"), Count), FHansaSessionModel::Validate(Fixture.State, Fixture.Rules, Issues));
	}
	{
		FFixture Fixture = MakeFixture(2); Fixture.State.Slots[1].HouseId = Fixture.State.Slots[0].HouseId;
		TArray<FHansaSessionValidationIssue> Issues;
		TestFalse(TEXT("Duplicate house binding fails closed"), FHansaSessionModel::Validate(Fixture.State, Fixture.Rules, Issues));
		TestTrue(TEXT("Duplicate binding has stable diagnostic"), HasCode(Issues, TEXT("HSA-MP02-005")));
	}
	{
		FFixture Fixture = MakeFixture(2); Fixture.State.Slots[1].State = EHansaSessionSlotState::Human;
		Fixture.State.Slots[1].ParticipantId = Fixture.State.Slots[0].ParticipantId;
		TArray<FHansaSessionValidationIssue> Issues;
		TestFalse(TEXT("Duplicate participant binding fails closed"), FHansaSessionModel::Validate(Fixture.State, Fixture.Rules, Issues));
		TestTrue(TEXT("Duplicate participant has stable diagnostic"), HasCode(Issues, TEXT("HSA-MP02-007")));
	}
	{
		FFixture Fixture = MakeFixture(8);
		const FHansaSessionSlot ExtraSlot = Fixture.State.Slots.Last();
		const FHansaScenarioSlotRule ExtraRule = Fixture.Rules.Last();
		Fixture.State.Slots.Add(ExtraSlot); Fixture.Rules.Add(ExtraRule);
		TArray<FHansaSessionValidationIssue> Issues;
		TestFalse(TEXT("A ninth slot is rejected"), FHansaSessionModel::Validate(Fixture.State, Fixture.Rules, Issues));
		TestTrue(TEXT("Capacity has stable diagnostic"), HasCode(Issues, TEXT("HSA-MP02-004")));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSessionTeamAndPermissionPolicyTest,
	"Hansa.Multiplayer.SessionModel.TeamAndPermissionPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSessionTeamAndPermissionPolicyTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::SessionModel; using namespace Hansa::Simulation;
	FFixture Fixture = MakeFixture(3); const FHansaTeamId Team = Id<FHansaTeamId>(71); const FHansaParticipantId Ally = Id<FHansaParticipantId>(102);
	Fixture.Rules[0].bTeamRequired = true; Fixture.Rules[0].AuthoredTeamId = Team;
	Fixture.Rules[1].bTeamRequired = true; Fixture.Rules[1].AuthoredTeamId = Team;
	Fixture.State.Slots[0].TeamId = Team; Fixture.State.Participants[0].TeamId = Team;
	Fixture.State.Slots[1].State = EHansaSessionSlotState::Human; Fixture.State.Slots[1].ParticipantId = Ally; Fixture.State.Slots[1].TeamId = Team;
	Fixture.State.Participants.Add({Ally, EHansaParticipantLifecycle::Active, 0, Id<FHansaHouseId>(2), Team});
	TArray<FHansaSessionValidationIssue> Issues;
	TestTrue(TEXT("Authored team constraints validate"), FHansaSessionModel::Validate(Fixture.State, Fixture.Rules, Issues));
	TestTrue(TEXT("Public information is visible"), FHansaSessionModel::CanRead(Fixture.State, Ally, Id<FHansaHouseId>(3), EHansaInformationClass::Public));
	TestTrue(TEXT("Team information is shared within the authored team"), FHansaSessionModel::CanRead(Fixture.State, Ally, Id<FHansaHouseId>(1), EHansaInformationClass::Team));
	TestFalse(TEXT("Team membership alone does not grant destructive commands"), FHansaSessionModel::CanAct(Fixture.State, Ally, Id<FHansaHouseId>(1), EHansaHousePermission::ManageAssets));
	Fixture.State.AccessGrants.Add({Id<FHansaHouseId>(1), Ally, FHansaTeamId(), static_cast<uint16>(EHansaHousePermission::ManageAssets)});
	TestTrue(TEXT("Explicit server-side grant authorizes its bounded action"), FHansaSessionModel::CanAct(Fixture.State, Ally, Id<FHansaHouseId>(1), EHansaHousePermission::ManageAssets));
	TestFalse(TEXT("A bounded grant does not imply private reports"), FHansaSessionModel::CanRead(Fixture.State, Ally, Id<FHansaHouseId>(1), EHansaInformationClass::HousePrivate));
	Fixture.State.Slots[1].TeamId = FHansaTeamId();
	Issues.Reset(); TestFalse(TEXT("Missing required team fails closed"), FHansaSessionModel::Validate(Fixture.State, Fixture.Rules, Issues));
	TestTrue(TEXT("Team constraint has stable diagnostic"), HasCode(Issues, TEXT("HSA-MP02-008")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaSessionSaveAndMigrationTest,
	"Hansa.Multiplayer.SessionModel.SaveRoundTripAndMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaSessionSaveAndMigrationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Tests::SessionModel; using namespace Hansa::Simulation;
	FFixture Fixture = MakeFixture(8); TArray<uint8> Bytes; FString Error;
	TestTrue(TEXT("Current session serializes"), FHansaSessionModel::Serialize(Fixture.State, Bytes, Error));
	FHansaSessionState Restored; const FHansaSessionDeserializeResult Read = FHansaSessionModel::Deserialize(Bytes, Restored);
	TestTrue(TEXT("Current session deserializes"), Read.bSuccess); TestEqual(TEXT("Current source version"), Read.SourceVersion, 2u);
	TArray<uint8> Reencoded; TestTrue(TEXT("Restored session reserializes"), FHansaSessionModel::Serialize(Restored, Reencoded, Error));
	TestEqual(TEXT("Session codec is byte-deterministic"), Bytes, Reencoded);
	TArray<FHansaSessionValidationIssue> Issues;
	TestTrue(TEXT("Restored eight-slot state validates"), FHansaSessionModel::Validate(Restored, Fixture.Rules, Issues));
	TestTrue(TEXT("Exact compatibility tuple is accepted"), FHansaSessionModel::IsCompatible(Restored, 7, 0x123456789abcdef0ULL, Restored.Compatibility.ScenarioId));
	TestFalse(TEXT("Protocol mismatch is rejected"), FHansaSessionModel::IsCompatible(Restored, 8, 0x123456789abcdef0ULL, Restored.Compatibility.ScenarioId));

	TArray<uint8> LegacyBytes;
	TestTrue(TEXT("Legacy fixture serializes"), FHansaSessionModel::SerializeLegacyV1ForMigrationTest(Fixture.State, LegacyBytes, Error));
	FHansaSessionState Migrated; const FHansaSessionDeserializeResult LegacyRead = FHansaSessionModel::Deserialize(LegacyBytes, Migrated);
	TestTrue(TEXT("Supported old session content migrates"), LegacyRead.bSuccess); TestEqual(TEXT("Legacy source version"), LegacyRead.SourceVersion, 1u);
	TestTrue(TEXT("Migration is recorded"), LegacyRead.AppliedMigrations.Contains(TEXT("Hansa.Multiplayer.Session.1To2.SeparateSessionAndTeamPolicy")));
	TestTrue(TEXT("Campaign and migrated session IDs remain separate"), Migrated.CampaignId.GetValue() != Migrated.SessionId.GetValue());
	Issues.Reset(); TestTrue(TEXT("Migrated legacy state validates"), FHansaSessionModel::Validate(Migrated, Fixture.Rules, Issues));
	return !HasAnyErrors();
}

#endif
