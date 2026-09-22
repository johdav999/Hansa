#include "Misc/AutomationTest.h"
#include "Multiplayer/HansaAdmission.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests::Admission
{
using namespace Hansa::Simulation;
template <typename T> T Id(uint64 Value) { return T::TryCreate(Value).Value; }

struct FMockAuthentication final : IHansaAuthenticationAdapter
{
	TMap<FString, FHansaAuthenticationResult> Proofs;
	virtual FHansaAuthenticationResult Authenticate(const FString& Proof, int64) override
	{
		if (const FHansaAuthenticationResult* Result = Proofs.Find(Proof)) return *Result;
		return {};
	}
};

FHansaSessionState Session()
{
	FHansaSessionState State; State.CampaignId = Id<FHansaCampaignId>(1); State.SessionId = Id<FHansaSessionId>(2);
	State.Compatibility.ProtocolVersion = 7; State.Compatibility.ContentManifestHash = 0x123456789abcdef0ULL;
	State.Compatibility.ScenarioId = FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FHansaParticipantId Participant = Id<FHansaParticipantId>(101 + Index);
		const FHansaHouseId House = Id<FHansaHouseId>(1 + Index);
		State.Slots.Add({FString::Printf(TEXT("HouseSlot.%d"), Index + 1), House, EHansaSessionSlotState::Human, Participant, FHansaTeamId()});
		State.Participants.Add({Participant, EHansaParticipantLifecycle::Active,
			static_cast<uint8>(Index == 0 ? static_cast<uint8>(EHansaParticipantRole::Host) | static_cast<uint8>(EHansaParticipantRole::Admin) : 0),
			House, FHansaTeamId()});
	}
	return State;
}

FHansaAdmissionRequest Online(uint64 Nonce, const FString& Proof, uint64 Participant, uint64 House, int64 Now = 100)
{
	FHansaAdmissionRequest Request; Request.AuthenticationProof = Proof; Request.RequestedParticipantId = Id<FHansaParticipantId>(Participant);
	Request.RequestedHouseId = Id<FHansaHouseId>(House); Request.ProtocolVersion = 7; Request.ContentManifestHash = 0x123456789abcdef0ULL;
	Request.ScenarioId = FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value; Request.RequestNonce = Nonce; Request.StartedAtSeconds = Now;
	return Request;
}

FHansaAdmissionRequest Lan(uint64 Nonce, const FString& Credential, uint64 Participant, uint64 House, int64 Now)
{
	FHansaAdmissionRequest Request = Online(Nonce, FString(), Participant, House, Now); Request.Mode = EHansaAdmissionMode::LanOffline;
	Request.ReconnectCredential = Credential; return Request;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAuthenticatedAdmissionPolicyTest, "Hansa.Multiplayer.Admission.AuthenticationAndPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaAuthenticatedAdmissionPolicyTest::RunTest(const FString&)
{
	using namespace Hansa::Tests::Admission; using namespace Hansa::Simulation;
	FMockAuthentication Auth; Auth.Proofs.Add(TEXT("proof-a"), {true, TEXT("account-a"), true});
	Auth.Proofs.Add(TEXT("proof-b"), {true, TEXT("account-b"), false});
	FHansaAdmissionService Service; FHansaAdmissionConfig Config; Config.CredentialLifetimeSeconds = 10; FString Error;
	TestTrue(TEXT("Admission service initializes"), Service.Initialize(Session(), Config, &Auth, Error));
	TestTrue(TEXT("Server binds first validated account"), Service.BindOnlineIdentity(TEXT("account-a"), Id<FHansaParticipantId>(101), Id<FHansaHouseId>(1), true, Error));
	TestTrue(TEXT("Server binds second validated account"), Service.BindOnlineIdentity(TEXT("account-b"), Id<FHansaParticipantId>(102), Id<FHansaHouseId>(2), false, Error));

	const FHansaAdmissionResult Spoofed = Service.Admit(Online(1, TEXT("proof-a"), 102, 2), 100);
	TestEqual(TEXT("Spoofed house is rejected"), Spoofed.Failure, EHansaAdmissionFailure::SlotUnavailable);
	TestTrue(TEXT("Rejected join receives no authority capability"), !Spoofed.bAdmitted && Spoofed.Grant.PrincipalId == 0 && !Spoofed.Grant.HouseId.IsValid());
	TestEqual(TEXT("Spoofed proof is rejected"), Service.Admit(Online(2, TEXT("not-a-proof"), 101, 1), 100).Failure, EHansaAdmissionFailure::AuthenticationFailed);
	FHansaAdmissionRequest Protocol = Online(3, TEXT("proof-a"), 101, 1); Protocol.ProtocolVersion = 8;
	TestEqual(TEXT("Protocol mismatch is rejected"), Service.Admit(Protocol, 100).Failure, EHansaAdmissionFailure::IncompatibleProtocol);
	FHansaAdmissionRequest Content = Online(4, TEXT("proof-a"), 101, 1); Content.ContentManifestHash++;
	TestEqual(TEXT("Content mismatch is rejected"), Service.Admit(Content, 100).Failure, EHansaAdmissionFailure::IncompatibleContent);
	TestEqual(TEXT("Timed-out join is rejected"), Service.Admit(Online(5, TEXT("proof-a"), 101, 1, 50), 100).Failure, EHansaAdmissionFailure::TimedOut);
	TestTrue(TEXT("Ban is accepted"), Service.SetBanned(TEXT("account-b"), true));
	TestEqual(TEXT("Banned account is rejected"), Service.Admit(Online(6, TEXT("proof-b"), 102, 2), 100).Failure, EHansaAdmissionFailure::Banned);
	TestTrue(TEXT("Ban can be removed"), Service.SetBanned(TEXT("account-b"), false));

	const FHansaAdmissionResult First = Service.Admit(Online(7, TEXT("proof-a"), 101, 1), 100);
	TestTrue(TEXT("Validated account receives its bound seat"), First.bAdmitted && First.Grant.HouseId == Id<FHansaHouseId>(1));
	TestEqual(TEXT("Duplicate account cannot connect twice"), Service.Admit(Online(8, TEXT("proof-a"), 101, 1), 100).Failure, EHansaAdmissionFailure::DuplicateLogin);
	TestTrue(TEXT("Second account fills the session"), Service.Admit(Online(9, TEXT("proof-b"), 102, 2), 100).bAdmitted);
	TestEqual(TEXT("Full session rejects another join before releasing data"), Service.Admit(Online(10, TEXT("proof-a"), 101, 1), 100).Failure, EHansaAdmissionFailure::SessionFull);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCredentialLifecycleTest, "Hansa.Multiplayer.Admission.CredentialLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaCredentialLifecycleTest::RunTest(const FString&)
{
	using namespace Hansa::Tests::Admission; using namespace Hansa::Simulation;
	FHansaAdmissionService Service; FHansaAdmissionConfig Config; Config.JoinPolicy = EHansaJoinPolicy::DirectLan; Config.CredentialLifetimeSeconds = 10; FString Error, Credential;
	TestTrue(TEXT("LAN service initializes without pretending names authenticate"), Service.Initialize(Session(), Config, nullptr, Error));
	TestTrue(TEXT("Server issues an opaque LAN credential"), Service.IssueLanAdmissionCredential(Id<FHansaParticipantId>(101), Id<FHansaHouseId>(1), 100, Credential, Error));
	TestTrue(TEXT("Credential has bounded opaque material"), Credential.Len() == 64);
	TestEqual(TEXT("Display-name-only LAN join is rejected"), Service.Admit(Lan(1, FString(), 101, 1, 100), 100).Failure, EHansaAdmissionFailure::CredentialInvalid);
	TestEqual(TEXT("Expired credential is rejected"), Service.Admit(Lan(2, Credential, 101, 1, 100), 111).Failure, EHansaAdmissionFailure::CredentialExpired);

	FString Current; TestTrue(TEXT("Replacement credential is issued"), Service.IssueLanAdmissionCredential(Id<FHansaParticipantId>(101), Id<FHansaHouseId>(1), 120, Current, Error));
	const FHansaAdmissionResult Joined = Service.Admit(Lan(3, Current, 101, 1, 120), 121);
	TestTrue(TEXT("Current credential admits the reserved LAN seat"), Joined.bAdmitted);
	Service.Release(Joined.Grant.PrincipalId);
	const FHansaAdmissionResult Replay = Service.Admit(Lan(4, Current, 101, 1, 122), 122);
	TestEqual(TEXT("Consumed credential cannot be replayed"), Replay.Failure, EHansaAdmissionFailure::CredentialReplayed);
	TestTrue(TEXT("Rotated credential reconnects after release"), Service.Admit(Lan(5, Joined.RotatedReconnectCredential, 101, 1, 123), 123).bAdmitted);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaAdmissionRedactionTest, "Hansa.Multiplayer.Admission.RedactionAndReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaAdmissionRedactionTest::RunTest(const FString&)
{
	using namespace Hansa::Tests::Admission; using namespace Hansa::Simulation;
	FMockAuthentication Auth; const FString SecretProof = TEXT("private-proof-value"); const FString PrivateAccount = TEXT("private-account-value");
	Auth.Proofs.Add(SecretProof, {true, PrivateAccount, false}); FHansaAdmissionService Service; FHansaAdmissionConfig Config; FString Error;
	Service.Initialize(Session(), Config, &Auth, Error); Service.BindOnlineIdentity(PrivateAccount, Id<FHansaParticipantId>(101), Id<FHansaHouseId>(1), true, Error);
	const FHansaAdmissionResult Joined = Service.Admit(Online(77, SecretProof, 101, 1), 100);
	TestTrue(TEXT("Initial authenticated join succeeds"), Joined.bAdmitted);
	const FHansaAdmissionResult Replay = Service.Admit(Online(77, SecretProof, 101, 1), 100);
	TestEqual(TEXT("Admission request nonce is replay resistant"), Replay.Failure, EHansaAdmissionFailure::CredentialReplayed);
	const FString PublicDiagnostic = Replay.Code + Replay.Cause + Replay.Remedy;
	TestFalse(TEXT("Structured diagnostic redacts authentication proof"), PublicDiagnostic.Contains(SecretProof));
	TestFalse(TEXT("Structured diagnostic redacts provider account"), PublicDiagnostic.Contains(PrivateAccount));
	TestFalse(TEXT("Structured diagnostic redacts reconnect credential"), PublicDiagnostic.Contains(Joined.RotatedReconnectCredential));
	TestFalse(TEXT("Secret-free authority grant has no credential text"), Joined.Grant.PrincipalId == 0 || !Joined.Grant.ParticipantId.IsValid());
	return !HasAnyErrors();
}
#endif
