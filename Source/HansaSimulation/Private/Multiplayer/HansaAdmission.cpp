#include "Multiplayer/HansaAdmission.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"

namespace Hansa::Simulation
{
namespace
{
	FString Digest(const FString& Value)
	{
		FTCHARToUTF8 Utf8(*Value); uint8 Bytes[20]; FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Bytes);
		return BytesToHex(Bytes, UE_ARRAY_COUNT(Bytes));
	}
	bool Occupied(EHansaSessionSlotState State) { return State == EHansaSessionSlotState::Human || State == EHansaSessionSlotState::Reserved; }
}

bool FHansaAdmissionService::Initialize(const FHansaSessionState& InSession, const FHansaAdmissionConfig& InConfig,
	IHansaAuthenticationAdapter* InAdapter, FString& OutError)
{
	if (InConfig.AdmissionTimeoutSeconds < 1 || InConfig.AdmissionTimeoutSeconds > 120 ||
		InConfig.CredentialLifetimeSeconds < 1 || InConfig.CredentialLifetimeSeconds > 86400 ||
		InConfig.MaximumTrackedNonces < 16 || InConfig.MaximumTrackedNonces > 65536 ||
		InConfig.MaximumCredentialRecords < 16 || InConfig.MaximumCredentialRecords > 65536 ||
		InSession.Slots.Num() < 2 || InSession.Slots.Num() > 8)
	{
		OutError = TEXT("Admission configuration or session capacity is invalid."); return false;
	}
	Session = InSession; Config = InConfig; AuthenticationAdapter = InAdapter; OnlineBindings.Reset();
	BannedAccounts.Reset(); Credentials.Reset(); Active.Reset(); SeenNonces.Reset(); NonceOrder.Reset(); NextPrincipalId = 1;
	OutError.Reset(); return true;
}

bool FHansaAdmissionService::ValidateBinding(const FBinding& Binding) const
{
	const FHansaSessionSlot* Slot = Session.Slots.FindByPredicate([&](const FHansaSessionSlot& Item)
		{ return Item.HouseId == Binding.HouseId && Item.ParticipantId == Binding.ParticipantId && Occupied(Item.State); });
	return Slot && Session.Participants.ContainsByPredicate([&](const FHansaParticipant& Item)
	{
		return Item.Id == Binding.ParticipantId && Item.HouseId == Binding.HouseId &&
			Item.Lifecycle != EHansaParticipantLifecycle::Left && Item.Lifecycle != EHansaParticipantLifecycle::Kicked &&
			Item.Lifecycle != EHansaParticipantLifecycle::Banned;
	});
}

bool FHansaAdmissionService::BindOnlineIdentity(const FString& Account, FHansaParticipantId Participant,
	FHansaHouseId House, bool bReserved, FString& OutError)
{
	const FBinding Binding{Participant, House, bReserved};
	bool bParticipantAlreadyBound = false;
	for (const TPair<FString, FBinding>& Item : OnlineBindings)
		if (Item.Value.ParticipantId == Participant) { bParticipantAlreadyBound = true; break; }
	if (Account.IsEmpty() || Account.Len() > 256 || !ValidateBinding(Binding) || OnlineBindings.Contains(Account) || bParticipantAlreadyBound)
	{
		OutError = TEXT("Identity binding is missing, duplicated, or does not match an occupied session slot."); return false;
	}
	OnlineBindings.Add(Account, Binding); OutError.Reset(); return true;
}

bool FHansaAdmissionService::SetBanned(const FString& Account, bool bBanned)
{
	if (!OnlineBindings.Contains(Account)) return false;
	if (bBanned) BannedAccounts.Add(Account); else BannedAccounts.Remove(Account); return true;
}

FString FHansaAdmissionService::IssueCredential(EHansaAdmissionMode Mode, const FString& Account,
	FHansaParticipantId Participant, FHansaHouseId House, int64 Now)
{
	const FString Plain = FGuid::NewGuid().ToString(EGuidFormats::Digits) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	// Retain consumed digests until expiry so a replay has a distinct, actionable failure.
	Credentials.RemoveAll([Now](const FCredential& Item) { return Now > Item.ExpiresAtSeconds; });
	while (Credentials.Num() >= Config.MaximumCredentialRecords) Credentials.RemoveAt(0, 1, EAllowShrinking::No);
	Credentials.Add({Digest(Plain), Mode, Account, Participant, House, Now + Config.CredentialLifetimeSeconds, false});
	return Plain;
}

bool FHansaAdmissionService::IssueLanAdmissionCredential(FHansaParticipantId Participant, FHansaHouseId House,
	int64 Now, FString& OutCredential, FString& OutError)
{
	if (!ValidateBinding({Participant, House, true}))
	{
		OutError = TEXT("LAN credential target does not match a reserved session participant and house."); return false;
	}
	OutCredential = IssueCredential(EHansaAdmissionMode::LanOffline, FString(), Participant, House, Now);
	OutError.Reset(); return true;
}

FHansaAdmissionService::FCredential* FHansaAdmissionService::FindCredential(const FString& Plain)
{
	if (Plain.IsEmpty() || Plain.Len() > 256) return nullptr; const FString Hash = Digest(Plain);
	return Credentials.FindByPredicate([&](const FCredential& Item) { return Item.Digest == Hash; });
}

FHansaAdmissionResult FHansaAdmissionService::Reject(EHansaAdmissionFailure Failure, const TCHAR* Code,
	const TCHAR* Cause, const TCHAR* Remedy) const
{
	FHansaAdmissionResult Result; Result.Failure = Failure; Result.Code = Code; Result.Cause = Cause; Result.Remedy = Remedy; return Result;
}

void FHansaAdmissionService::RememberNonce(uint64 Nonce)
{
	SeenNonces.Add(Nonce); NonceOrder.Add(Nonce);
	while (NonceOrder.Num() > Config.MaximumTrackedNonces)
	{
		SeenNonces.Remove(NonceOrder[0]); NonceOrder.RemoveAt(0, 1, EAllowShrinking::No);
	}
}

FHansaAdmissionResult FHansaAdmissionService::Admit(const FHansaAdmissionRequest& Request, int64 Now)
{
	if (!Request.RequestNonce || Request.StartedAtSeconds <= 0 || Now < Request.StartedAtSeconds ||
		Request.AuthenticationProof.Len() > 4096 || Request.ReconnectCredential.Len() > 256)
		return Reject(EHansaAdmissionFailure::InvalidRequest, TEXT("HSA-MP03-001"), TEXT("The admission request is malformed."), TEXT("Create a new bounded request."));
	if (Now - Request.StartedAtSeconds > Config.AdmissionTimeoutSeconds)
		return Reject(EHansaAdmissionFailure::TimedOut, TEXT("HSA-MP03-002"), TEXT("Admission timed out."), TEXT("Start a new join attempt."));
	if (SeenNonces.Contains(Request.RequestNonce))
		return Reject(EHansaAdmissionFailure::CredentialReplayed, TEXT("HSA-MP03-003"), TEXT("The admission request was already used."), TEXT("Start a new join attempt."));
	RememberNonce(Request.RequestNonce);
	if (Request.ProtocolVersion != Session.Compatibility.ProtocolVersion)
		return Reject(EHansaAdmissionFailure::IncompatibleProtocol, TEXT("HSA-MP03-004"), TEXT("The network protocol is incompatible."), TEXT("Use the same game build as the server."));
	if (Request.ContentManifestHash != Session.Compatibility.ContentManifestHash)
		return Reject(EHansaAdmissionFailure::IncompatibleContent, TEXT("HSA-MP03-005"), TEXT("The content manifest is incompatible."), TEXT("Install the server's content build."));
	if (Request.ScenarioId != Session.Compatibility.ScenarioId)
		return Reject(EHansaAdmissionFailure::WrongScenario, TEXT("HSA-MP03-006"), TEXT("The scenario is incompatible."), TEXT("Join with the advertised scenario."));

	FString Account; FBinding Binding; FCredential* Credential = nullptr;
	if (!Request.ReconnectCredential.IsEmpty())
	{
		Credential = FindCredential(Request.ReconnectCredential);
		if (!Credential || Credential->Mode != Request.Mode)
			return Reject(EHansaAdmissionFailure::CredentialInvalid, TEXT("HSA-MP03-007"), TEXT("The reconnect credential is invalid."), TEXT("Request a new invitation or authenticate again."));
		if (Credential->bUsed)
			return Reject(EHansaAdmissionFailure::CredentialReplayed, TEXT("HSA-MP03-008"), TEXT("The reconnect credential was already used."), TEXT("Use the most recently rotated credential."));
		if (Now > Credential->ExpiresAtSeconds)
			return Reject(EHansaAdmissionFailure::CredentialExpired, TEXT("HSA-MP03-009"), TEXT("The reconnect credential expired."), TEXT("Authenticate or request a new LAN credential."));
		Binding = {Credential->ParticipantId, Credential->HouseId, true}; Account = Credential->CanonicalAccountId;
	}

	FHansaAuthenticationResult Authentication;
	if (Request.Mode == EHansaAdmissionMode::OnlineAuthenticated)
	{
		if (!AuthenticationAdapter)
			return Reject(EHansaAdmissionFailure::AdapterUnavailable, TEXT("HSA-MP03-010"), TEXT("Online authentication is unavailable."), TEXT("Retry when the identity service is available."));
		Authentication = AuthenticationAdapter->Authenticate(Request.AuthenticationProof, Now);
		if (!Authentication.bAuthenticated || Authentication.CanonicalAccountId.IsEmpty() || Authentication.CanonicalAccountId.Len() > 256)
			return Reject(EHansaAdmissionFailure::AuthenticationFailed, TEXT("HSA-MP03-011"), TEXT("The online identity proof was rejected."), TEXT("Sign in again and retry."));
		Account = Authentication.CanonicalAccountId; const FBinding* Bound = OnlineBindings.Find(Account);
		if (!Bound) return Reject(EHansaAdmissionFailure::SlotUnavailable, TEXT("HSA-MP03-012"), TEXT("This identity has no authorized participant seat."), TEXT("Ask the host for an invitation or reservation."));
		if (Credential && Credential->CanonicalAccountId != Account)
			return Reject(EHansaAdmissionFailure::CredentialInvalid, TEXT("HSA-MP03-013"), TEXT("The credential is not bound to this identity."), TEXT("Use the credential issued to the signed-in account."));
		Binding = *Bound;
		if (BannedAccounts.Contains(Account))
			return Reject(EHansaAdmissionFailure::Banned, TEXT("HSA-MP03-014"), TEXT("Admission is denied by server policy."), TEXT("Contact the server administrator."));
		if (Config.JoinPolicy == EHansaJoinPolicy::Friends && !Authentication.bFriendOfHost)
			return Reject(EHansaAdmissionFailure::ReservationRequired, TEXT("HSA-MP03-015"), TEXT("This session is limited to the host's friends."), TEXT("Ask the host to reserve a seat."));
	}
	else if (!Credential)
		return Reject(EHansaAdmissionFailure::CredentialInvalid, TEXT("HSA-MP03-016"), TEXT("LAN admission requires a server-issued credential."), TEXT("Enter the credential supplied by the host."));

	if (Request.RequestedParticipantId != Binding.ParticipantId || Request.RequestedHouseId != Binding.HouseId)
		return Reject(EHansaAdmissionFailure::SlotUnavailable, TEXT("HSA-MP03-017"), TEXT("The requested participant or house is not authorized."), TEXT("Join the seat assigned by the server."));
	if (!ValidateBinding(Binding))
		return Reject(EHansaAdmissionFailure::SlotUnavailable, TEXT("HSA-MP03-018"), TEXT("The authorized seat is no longer available."), TEXT("Refresh the lobby membership."));
	if (Config.JoinPolicy == EHansaJoinPolicy::Closed && !Credential)
		return Reject(EHansaAdmissionFailure::JoinClosed, TEXT("HSA-MP03-019"), TEXT("The session is closed to new joins."), TEXT("Reconnect with a current credential or wait for the host."));
	if ((Config.JoinPolicy == EHansaJoinPolicy::InviteOnly || Config.JoinPolicy == EHansaJoinPolicy::ReservedOnly) && !Binding.bReserved)
		return Reject(EHansaAdmissionFailure::ReservationRequired, TEXT("HSA-MP03-020"), TEXT("This session requires a reserved seat."), TEXT("Ask the host to reserve your participant seat."));
	if (Active.Num() >= Session.Slots.Num())
		return Reject(EHansaAdmissionFailure::SessionFull, TEXT("HSA-MP03-021"), TEXT("The session is full."), TEXT("Wait for a seat to become available."));
	bool bDuplicate = false;
	for (const TPair<uint64, FActive>& Item : Active)
		if (Item.Value.Grant.ParticipantId == Binding.ParticipantId || (!Account.IsEmpty() && Item.Value.CanonicalAccountId == Account)) { bDuplicate = true; break; }
	if (bDuplicate)
		return Reject(EHansaAdmissionFailure::DuplicateLogin, TEXT("HSA-MP03-022"), TEXT("This participant is already connected."), TEXT("Close the other connection before retrying."));

	if (Credential) Credential->bUsed = true;
	FHansaAdmissionResult Result; Result.bAdmitted = true; Result.Grant = {NextPrincipalId++, Binding.ParticipantId, Binding.HouseId, Request.Mode};
	Result.RotatedReconnectCredential = IssueCredential(Request.Mode, Account, Binding.ParticipantId, Binding.HouseId, Now);
	Active.Add(Result.Grant.PrincipalId, {Account, Result.Grant}); return Result;
}

void FHansaAdmissionService::Release(uint64 PrincipalId) { Active.Remove(PrincipalId); }
bool FHansaAdmissionService::IsActive(uint64 PrincipalId) const { return Active.Contains(PrincipalId); }
}
