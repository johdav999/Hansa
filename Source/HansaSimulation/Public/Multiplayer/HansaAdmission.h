#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Model/HansaIds.h"
#include "Multiplayer/HansaSessionModel.h"

namespace Hansa::Simulation
{
	enum class EHansaAdmissionMode : uint8 { OnlineAuthenticated = 0, LanOffline };
	enum class EHansaJoinPolicy : uint8 { Public = 0, Friends, InviteOnly, DirectLan, Closed, ReservedOnly };
	enum class EHansaAdmissionFailure : uint8 { None = 0, InvalidRequest, TimedOut, AuthenticationFailed, AdapterUnavailable, IncompatibleProtocol, IncompatibleContent, WrongScenario, SessionFull, ReservationRequired, Banned, JoinClosed, DuplicateLogin, SlotUnavailable, CredentialInvalid, CredentialExpired, CredentialReplayed };
	struct HANSASIMULATION_API FHansaAuthenticationResult final { bool bAuthenticated = false; FString CanonicalAccountId; bool bFriendOfHost = false; };
	class HANSASIMULATION_API IHansaAuthenticationAdapter { public: virtual ~IHansaAuthenticationAdapter() = default; virtual FHansaAuthenticationResult Authenticate(const FString& OpaqueProof, int64 ServerNowSeconds) = 0; };
	struct HANSASIMULATION_API FHansaAdmissionConfig final { EHansaJoinPolicy JoinPolicy = EHansaJoinPolicy::Public; int64 AdmissionTimeoutSeconds = 15; int64 CredentialLifetimeSeconds = 300; int32 MaximumTrackedNonces = 1024; int32 MaximumCredentialRecords = 1024; };
	struct HANSASIMULATION_API FHansaAdmissionRequest final
	{
		EHansaAdmissionMode Mode = EHansaAdmissionMode::OnlineAuthenticated; FString AuthenticationProof; FString ReconnectCredential;
		FHansaParticipantId RequestedParticipantId; FHansaHouseId RequestedHouseId; uint32 ProtocolVersion = 0; uint64 ContentManifestHash = 0;
		FHansaScenarioId ScenarioId; uint64 RequestNonce = 0; int64 StartedAtSeconds = 0;
	};
	struct HANSASIMULATION_API FHansaAdmissionGrant final { uint64 PrincipalId = 0; FHansaParticipantId ParticipantId; FHansaHouseId HouseId; EHansaAdmissionMode Mode = EHansaAdmissionMode::OnlineAuthenticated; };
	struct HANSASIMULATION_API FHansaAdmissionResult final
	{
		bool bAdmitted = false; EHansaAdmissionFailure Failure = EHansaAdmissionFailure::None; FString Code; FString Cause; FString Remedy;
		FHansaAdmissionGrant Grant; FString RotatedReconnectCredential;
	};
	/** Runtime-only admission directory; secrets/provider IDs never enter gameplay state or projections. */
	class HANSASIMULATION_API FHansaAdmissionService final
	{
	public:
		bool Initialize(const FHansaSessionState& InSession, const FHansaAdmissionConfig& InConfig, IHansaAuthenticationAdapter* InAuthenticationAdapter, FString& OutError);
		bool BindOnlineIdentity(const FString& CanonicalAccountId, FHansaParticipantId ParticipantId, FHansaHouseId HouseId, bool bReserved, FString& OutError);
		bool SetBanned(const FString& CanonicalAccountId, bool bBanned);
		bool IssueLanAdmissionCredential(FHansaParticipantId ParticipantId, FHansaHouseId HouseId, int64 ServerNowSeconds, FString& OutCredential, FString& OutError);
		FHansaAdmissionResult Admit(const FHansaAdmissionRequest& Request, int64 ServerNowSeconds);
		void Release(uint64 PrincipalId); [[nodiscard]] bool IsActive(uint64 PrincipalId) const;
	private:
		struct FBinding { FHansaParticipantId ParticipantId; FHansaHouseId HouseId; bool bReserved = false; };
		struct FCredential { FString Digest; EHansaAdmissionMode Mode = EHansaAdmissionMode::OnlineAuthenticated; FString CanonicalAccountId; FHansaParticipantId ParticipantId; FHansaHouseId HouseId; int64 ExpiresAtSeconds = 0; bool bUsed = false; };
		struct FActive { FString CanonicalAccountId; FHansaAdmissionGrant Grant; };
		FHansaAdmissionResult Reject(EHansaAdmissionFailure Failure, const TCHAR* Code, const TCHAR* Cause, const TCHAR* Remedy) const;
		bool ValidateBinding(const FBinding& Binding) const;
		FString IssueCredential(EHansaAdmissionMode Mode, const FString& CanonicalAccountId, FHansaParticipantId ParticipantId, FHansaHouseId HouseId, int64 ServerNowSeconds);
		FCredential* FindCredential(const FString& Plaintext); void RememberNonce(uint64 Nonce);
		FHansaSessionState Session; FHansaAdmissionConfig Config; IHansaAuthenticationAdapter* AuthenticationAdapter = nullptr;
		TMap<FString, FBinding> OnlineBindings; TSet<FString> BannedAccounts; TArray<FCredential> Credentials; TMap<uint64, FActive> Active;
		TSet<uint64> SeenNonces; TArray<uint64> NonceOrder; uint64 NextPrincipalId = 1;
	};
}
