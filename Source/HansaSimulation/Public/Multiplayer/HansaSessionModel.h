#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/UnrealString.h"
#include "Model/HansaIds.h"

namespace Hansa::Simulation
{
	enum class EHansaSessionSlotState : uint8 { Human = 0, AI, Open, Closed, Reserved };
	enum class EHansaParticipantLifecycle : uint8 { Invited = 0, Joining, Active, Disconnected, Left, Kicked, Banned };
	enum class EHansaParticipantRole : uint8 { None = 0, Host = 1 << 0, Admin = 1 << 1 };
	enum class EHansaHousePermission : uint16
	{
		None = 0,
		ReadPrivate = 1 << 0,
		IssueEconomicCommands = 1 << 1,
		ManageAssets = 1 << 2,
		ManageAgreements = 1 << 3
	};
	enum class EHansaInformationClass : uint8 { Public = 0, Team, HousePrivate };

	struct HANSASIMULATION_API FHansaSessionCompatibility final
	{
		uint32 ProtocolVersion = 0;
		uint64 ContentManifestHash = 0;
		FHansaScenarioId ScenarioId;
	};

	struct HANSASIMULATION_API FHansaScenarioSlotRule final
	{
		FString SlotId;
		FHansaHouseId HouseId;
		EHansaSessionSlotState DefaultState = EHansaSessionSlotState::Closed;
		uint8 AllowedStateMask = 0;
		FHansaTeamId AuthoredTeamId;
		bool bTeamRequired = false;
		bool bAllowHumanTakeover = true;

		[[nodiscard]] bool Allows(EHansaSessionSlotState State) const
		{
			return (AllowedStateMask & (1u << static_cast<uint8>(State))) != 0;
		}
	};

	struct HANSASIMULATION_API FHansaSessionSlot final
	{
		FString SlotId;
		FHansaHouseId HouseId;
		EHansaSessionSlotState State = EHansaSessionSlotState::Closed;
		FHansaParticipantId ParticipantId;
		FHansaTeamId TeamId;
	};

	struct HANSASIMULATION_API FHansaParticipant final
	{
		FHansaParticipantId Id;
		EHansaParticipantLifecycle Lifecycle = EHansaParticipantLifecycle::Invited;
		uint8 RoleMask = static_cast<uint8>(EHansaParticipantRole::None);
		FHansaHouseId HouseId;
		FHansaTeamId TeamId;

		[[nodiscard]] bool HasRole(EHansaParticipantRole Role) const
		{
			return (RoleMask & static_cast<uint8>(Role)) != 0;
		}
	};

	struct HANSASIMULATION_API FHansaHouseAccessGrant final
	{
		FHansaHouseId HouseId;
		FHansaParticipantId ParticipantId;
		FHansaTeamId TeamId;
		uint16 PermissionMask = static_cast<uint16>(EHansaHousePermission::None);

		[[nodiscard]] bool Grants(EHansaHousePermission Permission) const
		{
			return (PermissionMask & static_cast<uint16>(Permission)) != 0;
		}
	};

	struct HANSASIMULATION_API FHansaSessionState final
	{
		static constexpr uint32 CurrentSchemaVersion = 2;

		uint32 SchemaVersion = CurrentSchemaVersion;
		FHansaCampaignId CampaignId;
		FHansaSessionId SessionId;
		FHansaSessionCompatibility Compatibility;
		TArray<FHansaSessionSlot> Slots;
		TArray<FHansaParticipant> Participants;
		TArray<FHansaHouseAccessGrant> AccessGrants;
	};

	struct HANSASIMULATION_API FHansaSessionValidationIssue final
	{
		FString Code;
		FString Path;
		FString Cause;
		FString Remedy;
	};

	struct HANSASIMULATION_API FHansaSessionDeserializeResult final
	{
		bool bSuccess = false;
		uint32 SourceVersion = 0;
		TArray<FString> AppliedMigrations;
		FString Error;
	};

	/** Runtime-only, provider-neutral validation, policy, and save codec for multiplayer membership. */
	class HANSASIMULATION_API FHansaSessionModel final
	{
	public:
		static constexpr uint32 MinimumSlots = 2;
		static constexpr uint32 MaximumSlots = 8;

		static bool Validate(const FHansaSessionState& State, TConstArrayView<FHansaScenarioSlotRule> Rules,
			TArray<FHansaSessionValidationIssue>& OutIssues);
		static bool CanRead(const FHansaSessionState& State, FHansaParticipantId Requester,
			FHansaHouseId SubjectHouse, EHansaInformationClass InformationClass);
		static bool CanAct(const FHansaSessionState& State, FHansaParticipantId Requester,
			FHansaHouseId SubjectHouse, EHansaHousePermission Permission);
		static bool IsCompatible(const FHansaSessionState& State, uint32 ProtocolVersion,
			uint64 ContentManifestHash, const FHansaScenarioId& ScenarioId);
		static bool Serialize(const FHansaSessionState& State, TArray<uint8>& OutBytes, FString& OutError);
		static FHansaSessionDeserializeResult Deserialize(TConstArrayView<uint8> Bytes,
			FHansaSessionState& OutState);
#if WITH_HANSA_AUTOMATION
		static bool SerializeLegacyV1ForMigrationTest(const FHansaSessionState& State,
			TArray<uint8>& OutBytes, FString& OutError);
#endif
	};
}
