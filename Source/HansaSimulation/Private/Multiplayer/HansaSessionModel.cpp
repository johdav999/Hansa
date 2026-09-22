#include "Multiplayer/HansaSessionModel.h"

#include "Containers/Set.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr uint32 SessionMagic = 0x31534d48U; // HMS1
		constexpr uint32 LegacyVersion = 1;
		constexpr int32 MaximumParticipants = 8;
		constexpr int32 MaximumGrants = 128;
		constexpr int32 MaximumStringBytes = 128;

		class FWriter final
		{
		public:
			void U8(uint8 Value) { Bytes.Add(Value); }
			void U16(uint16 Value) { for (int32 Index = 0; Index < 2; ++Index) U8(static_cast<uint8>(Value >> (Index * 8))); }
			void U32(uint32 Value) { for (int32 Index = 0; Index < 4; ++Index) U8(static_cast<uint8>(Value >> (Index * 8))); }
			void U64(uint64 Value) { for (int32 Index = 0; Index < 8; ++Index) U8(static_cast<uint8>(Value >> (Index * 8))); }
			bool String(const FString& Value)
			{
				FTCHARToUTF8 Utf8(*Value);
				if (Utf8.Length() < 0 || Utf8.Length() > MaximumStringBytes) return false;
				U16(static_cast<uint16>(Utf8.Length()));
				for (int32 Index = 0; Index < Utf8.Length(); ++Index) U8(static_cast<uint8>(Utf8.Get()[Index]));
				return true;
			}
			template <typename TId> void Id(const TId& Value) { U64(Value.GetValue()); U32(Value.GetGeneration()); }
			TArray<uint8> Bytes;
		};

		class FReader final
		{
		public:
			explicit FReader(TConstArrayView<uint8> InBytes) : Bytes(InBytes) {}
			bool U8(uint8& Out) { if (Offset >= Bytes.Num()) return Fail(TEXT("Session data is truncated.")); Out = Bytes[Offset++]; return true; }
			bool U16(uint16& Out) { Out = 0; for (int32 Index = 0; Index < 2; ++Index) { uint8 Byte = 0; if (!U8(Byte)) return false; Out |= static_cast<uint16>(Byte) << (Index * 8); } return true; }
			bool U32(uint32& Out) { Out = 0; for (int32 Index = 0; Index < 4; ++Index) { uint8 Byte = 0; if (!U8(Byte)) return false; Out |= static_cast<uint32>(Byte) << (Index * 8); } return true; }
			bool U64(uint64& Out) { Out = 0; for (int32 Index = 0; Index < 8; ++Index) { uint8 Byte = 0; if (!U8(Byte)) return false; Out |= static_cast<uint64>(Byte) << (Index * 8); } return true; }
			bool String(FString& Out)
			{
				uint16 Length = 0; if (!U16(Length)) return false;
				if (Length > MaximumStringBytes || Bytes.Num() - Offset < Length) return Fail(TEXT("Session string is invalid or truncated."));
				TArray<ANSICHAR> Buffer; Buffer.SetNumUninitialized(Length + 1);
				for (uint16 Index = 0; Index < Length; ++Index) Buffer[Index] = static_cast<ANSICHAR>(Bytes[Offset++]);
				Buffer[Length] = '\0'; Out = UTF8_TO_TCHAR(Buffer.GetData()); return true;
			}
			template <typename TId> bool Id(TId& Out)
			{
				uint64 Value = 0; uint32 Generation = 0;
				if (!U64(Value) || !U32(Generation)) return false;
				if (Value == 0) { Out = TId(); return true; }
				const auto Created = TId::TryCreate(Value, Generation);
				if (!Created) return Fail(TEXT("Session contains an invalid stable identity."));
				Out = Created.Value; return true;
			}
			bool Count(uint16& Out, int32 Maximum) { return U16(Out) && (Out <= Maximum || Fail(TEXT("Session collection exceeds its bound."))); }
			bool Finish() { return Error.IsEmpty() && (Offset == Bytes.Num() || Fail(TEXT("Session data has trailing bytes."))); }
			bool Fail(const TCHAR* Message) { if (Error.IsEmpty()) Error = Message; return false; }
			FString Error;
		private:
			TConstArrayView<uint8> Bytes;
			int32 Offset = 0;
		};

		bool IsTerminal(EHansaParticipantLifecycle Lifecycle)
		{
			return Lifecycle == EHansaParticipantLifecycle::Left || Lifecycle == EHansaParticipantLifecycle::Kicked ||
				Lifecycle == EHansaParticipantLifecycle::Banned;
		}

		const FHansaParticipant* FindParticipant(const FHansaSessionState& State, FHansaParticipantId Id)
		{
			return State.Participants.FindByPredicate([Id](const FHansaParticipant& Participant) { return Participant.Id == Id; });
		}

		bool WriteState(const FHansaSessionState& State, uint32 Version, TArray<uint8>& OutBytes, FString& OutError)
		{
			FWriter Writer; Writer.U32(SessionMagic); Writer.U32(Version); Writer.Id(State.CampaignId);
			if (Version >= 2) Writer.Id(State.SessionId);
			Writer.U32(State.Compatibility.ProtocolVersion); Writer.U64(State.Compatibility.ContentManifestHash);
			if (!Writer.String(State.Compatibility.ScenarioId.ToString())) { OutError = TEXT("Scenario ID exceeds the session codec bound."); return false; }
			Writer.U16(static_cast<uint16>(State.Slots.Num()));
			for (const FHansaSessionSlot& Slot : State.Slots)
			{
				if (!Writer.String(Slot.SlotId)) { OutError = TEXT("Slot ID exceeds the session codec bound."); return false; }
				Writer.Id(Slot.HouseId); Writer.U8(static_cast<uint8>(Slot.State)); Writer.Id(Slot.ParticipantId);
				if (Version >= 2) Writer.Id(Slot.TeamId);
			}
			Writer.U16(static_cast<uint16>(State.Participants.Num()));
			for (const FHansaParticipant& Participant : State.Participants)
			{
				Writer.Id(Participant.Id); Writer.U8(static_cast<uint8>(Participant.Lifecycle)); Writer.U8(Participant.RoleMask);
				Writer.Id(Participant.HouseId); if (Version >= 2) Writer.Id(Participant.TeamId);
			}
			if (Version >= 2)
			{
				Writer.U16(static_cast<uint16>(State.AccessGrants.Num()));
				for (const FHansaHouseAccessGrant& Grant : State.AccessGrants)
				{
					Writer.Id(Grant.HouseId); Writer.Id(Grant.ParticipantId); Writer.Id(Grant.TeamId); Writer.U16(Grant.PermissionMask);
				}
			}
			OutBytes = MoveTemp(Writer.Bytes); return true;
		}
	}

	bool FHansaSessionModel::Validate(const FHansaSessionState& State, TConstArrayView<FHansaScenarioSlotRule> Rules,
		TArray<FHansaSessionValidationIssue>& OutIssues)
	{
		OutIssues.Reset();
		auto Issue = [&OutIssues](const TCHAR* Code, const FString& Path, const TCHAR* Cause, const TCHAR* Remedy)
		{
			OutIssues.Add({Code, Path, Cause, Remedy});
		};
		if (State.SchemaVersion != FHansaSessionState::CurrentSchemaVersion)
			Issue(TEXT("HSA-MP02-001"), TEXT("SchemaVersion"), TEXT("Session model schema is not current."), TEXT("Migrate the record before activation."));
		if (!State.CampaignId.IsValid() || !State.SessionId.IsValid() ||
			(State.CampaignId.GetValue() == State.SessionId.GetValue() && State.CampaignId.GetGeneration() == State.SessionId.GetGeneration()))
			Issue(TEXT("HSA-MP02-002"), TEXT("Identity"), TEXT("Campaign and session require distinct stable identities."), TEXT("Allocate separate nonzero campaign and session IDs."));
		if (State.Compatibility.ProtocolVersion == 0 || State.Compatibility.ContentManifestHash == 0 || !State.Compatibility.ScenarioId.IsValid())
			Issue(TEXT("HSA-MP02-003"), TEXT("Compatibility"), TEXT("Protocol, content manifest, or scenario identity is missing."), TEXT("Bind the server-approved compatibility tuple."));
		if (State.Slots.Num() < MinimumSlots || State.Slots.Num() > MaximumSlots || Rules.Num() != State.Slots.Num())
			Issue(TEXT("HSA-MP02-004"), TEXT("Slots"), TEXT("A session must have two through eight slots matching the authored rules."), TEXT("Use the scenario slot set without adding or omitting slots."));

		TSet<FString> SlotIds; TSet<FHansaHouseId> Houses; TSet<FHansaParticipantId> BoundParticipants;
		for (int32 Index = 0; Index < State.Slots.Num(); ++Index)
		{
			const FHansaSessionSlot& Slot = State.Slots[Index]; const FString Path = FString::Printf(TEXT("Slots[%d]"), Index);
			const FHansaScenarioSlotRule* Rule = Rules.FindByPredicate([&Slot](const FHansaScenarioSlotRule& Candidate) { return Candidate.SlotId == Slot.SlotId; });
			if (Slot.SlotId.IsEmpty() || SlotIds.Contains(Slot.SlotId) || !Slot.HouseId.IsValid() || Houses.Contains(Slot.HouseId))
				Issue(TEXT("HSA-MP02-005"), Path, TEXT("Slot and house bindings must be unique and valid."), TEXT("Assign one stable slot and house identity per authored entry."));
			SlotIds.Add(Slot.SlotId); Houses.Add(Slot.HouseId);
			if (Rule == nullptr || Rule->HouseId != Slot.HouseId || !Rule->Allows(Slot.State))
				Issue(TEXT("HSA-MP02-006"), Path, TEXT("Slot state or house violates the authored scenario rule."), TEXT("Restore an allowed state and the authored house binding."));
			const bool bNeedsParticipant = Slot.State == EHansaSessionSlotState::Human || Slot.State == EHansaSessionSlotState::Reserved;
			if (bNeedsParticipant != Slot.ParticipantId.IsValid() || (Slot.ParticipantId.IsValid() && BoundParticipants.Contains(Slot.ParticipantId)))
				Issue(TEXT("HSA-MP02-007"), Path, TEXT("Human/reserved slots need exactly one unique participant; other states need none."), TEXT("Correct the participant binding for the slot state."));
			if (Slot.ParticipantId.IsValid()) BoundParticipants.Add(Slot.ParticipantId);
			if (Rule != nullptr && ((Rule->bTeamRequired && !Slot.TeamId.IsValid()) ||
				(Rule->AuthoredTeamId.IsValid() && Rule->AuthoredTeamId != Slot.TeamId)))
				Issue(TEXT("HSA-MP02-008"), Path, TEXT("Slot team assignment violates the authored team constraint."), TEXT("Use the required authored team identity."));
		}

		TSet<FHansaParticipantId> ParticipantIds; int32 HostCount = 0;
		for (int32 Index = 0; Index < State.Participants.Num(); ++Index)
		{
			const FHansaParticipant& Participant = State.Participants[Index]; const FString Path = FString::Printf(TEXT("Participants[%d]"), Index);
			if (!Participant.Id.IsValid() || ParticipantIds.Contains(Participant.Id))
				Issue(TEXT("HSA-MP02-009"), Path, TEXT("Participant identity is invalid or duplicated."), TEXT("Allocate one stable participant ID per person or AI controller."));
			ParticipantIds.Add(Participant.Id);
			const FHansaSessionSlot* Slot = State.Slots.FindByPredicate([&Participant](const FHansaSessionSlot& Candidate) { return Candidate.ParticipantId == Participant.Id; });
			if (!IsTerminal(Participant.Lifecycle) && (Slot == nullptr || Participant.HouseId != Slot->HouseId || Participant.TeamId != Slot->TeamId))
				Issue(TEXT("HSA-MP02-010"), Path, TEXT("Non-terminal participant binding does not match its slot, house, and team."), TEXT("Bind the participant only through its assigned authored slot."));
			if (IsTerminal(Participant.Lifecycle) && Slot != nullptr)
				Issue(TEXT("HSA-MP02-011"), Path, TEXT("A terminal participant still occupies a slot."), TEXT("Release or reserve the seat through a non-terminal participant record."));
			if (Participant.HasRole(EHansaParticipantRole::Host))
			{
				++HostCount;
				if (!Participant.HasRole(EHansaParticipantRole::Admin) || IsTerminal(Participant.Lifecycle))
					Issue(TEXT("HSA-MP02-012"), Path, TEXT("The host must be a non-terminal administrator."), TEXT("Grant Admin to the active host or transfer host authority."));
			}
		}
		if (HostCount != 1) Issue(TEXT("HSA-MP02-013"), TEXT("Participants"), TEXT("Exactly one participant must hold the host role."), TEXT("Elect one active host; use Admin for additional moderators."));

		TSet<FString> GrantKeys;
		for (int32 Index = 0; Index < State.AccessGrants.Num(); ++Index)
		{
			const FHansaHouseAccessGrant& Grant = State.AccessGrants[Index];
			const bool bOneGrantee = Grant.ParticipantId.IsValid() != Grant.TeamId.IsValid();
			const FString Key = FString::Printf(TEXT("%llu:%llu:%llu"), Grant.HouseId.GetValue(), Grant.ParticipantId.GetValue(), Grant.TeamId.GetValue());
			if (!Houses.Contains(Grant.HouseId) || !bOneGrantee || Grant.PermissionMask == 0 || GrantKeys.Contains(Key) ||
				(Grant.ParticipantId.IsValid() && !ParticipantIds.Contains(Grant.ParticipantId)))
				Issue(TEXT("HSA-MP02-014"), FString::Printf(TEXT("AccessGrants[%d]"), Index), TEXT("Access grant has an invalid house, grantee, permission set, or duplicate."), TEXT("Grant bounded permissions once to an existing participant or team."));
			GrantKeys.Add(Key);
		}
		return OutIssues.IsEmpty();
	}

	bool FHansaSessionModel::CanRead(const FHansaSessionState& State, FHansaParticipantId Requester,
		FHansaHouseId SubjectHouse, EHansaInformationClass InformationClass)
	{
		if (InformationClass == EHansaInformationClass::Public) return SubjectHouse.IsValid();
		const FHansaParticipant* Participant = FindParticipant(State, Requester);
		if (Participant == nullptr || IsTerminal(Participant->Lifecycle)) return false;
		if (Participant->HouseId == SubjectHouse) return true;
		if (InformationClass == EHansaInformationClass::Team && Participant->TeamId.IsValid())
		{
			const FHansaSessionSlot* Subject = State.Slots.FindByPredicate([SubjectHouse](const FHansaSessionSlot& Slot) { return Slot.HouseId == SubjectHouse; });
			if (Subject != nullptr && Subject->TeamId == Participant->TeamId) return true;
		}
		return CanAct(State, Requester, SubjectHouse, EHansaHousePermission::ReadPrivate);
	}

	bool FHansaSessionModel::CanAct(const FHansaSessionState& State, FHansaParticipantId Requester,
		FHansaHouseId SubjectHouse, EHansaHousePermission Permission)
	{
		const FHansaParticipant* Participant = FindParticipant(State, Requester);
		if (Participant == nullptr || IsTerminal(Participant->Lifecycle) || !SubjectHouse.IsValid()) return false;
		if (Participant->HouseId == SubjectHouse) return true;
		return State.AccessGrants.ContainsByPredicate([&](const FHansaHouseAccessGrant& Grant)
		{
			return Grant.HouseId == SubjectHouse && Grant.Grants(Permission) &&
				((Grant.ParticipantId.IsValid() && Grant.ParticipantId == Requester) ||
				 (Grant.TeamId.IsValid() && Grant.TeamId == Participant->TeamId));
		});
	}

	bool FHansaSessionModel::IsCompatible(const FHansaSessionState& State, uint32 ProtocolVersion,
		uint64 ContentManifestHash, const FHansaScenarioId& ScenarioId)
	{
		return ProtocolVersion != 0 && State.Compatibility.ProtocolVersion == ProtocolVersion &&
			State.Compatibility.ContentManifestHash == ContentManifestHash && State.Compatibility.ScenarioId == ScenarioId;
	}

	bool FHansaSessionModel::Serialize(const FHansaSessionState& State, TArray<uint8>& OutBytes, FString& OutError)
	{
		TArray<FHansaSessionValidationIssue> Issues;
		if (State.SchemaVersion != FHansaSessionState::CurrentSchemaVersion || State.Slots.Num() > MaximumSlots ||
			State.Participants.Num() > MaximumParticipants || State.AccessGrants.Num() > MaximumGrants)
		{
			OutError = TEXT("Session record is not current or exceeds codec bounds."); return false;
		}
		return WriteState(State, FHansaSessionState::CurrentSchemaVersion, OutBytes, OutError);
	}

	FHansaSessionDeserializeResult FHansaSessionModel::Deserialize(TConstArrayView<uint8> Bytes, FHansaSessionState& OutState)
	{
		FHansaSessionDeserializeResult Result; FReader Reader(Bytes); uint32 Magic = 0, Version = 0;
		if (!Reader.U32(Magic) || !Reader.U32(Version) || Magic != SessionMagic || (Version != LegacyVersion && Version != FHansaSessionState::CurrentSchemaVersion))
		{
			Result.Error = Reader.Error.IsEmpty() ? TEXT("Session format is unsupported.") : Reader.Error; return Result;
		}
		Result.SourceVersion = Version; FHansaSessionState State; State.SchemaVersion = FHansaSessionState::CurrentSchemaVersion;
		if (!Reader.Id(State.CampaignId)) { Result.Error = Reader.Error; return Result; }
		if (Version >= 2) { if (!Reader.Id(State.SessionId)) { Result.Error = Reader.Error; return Result; } }
		else
		{
			const uint64 Derived = State.CampaignId.GetValue() ^ 0x9e3779b97f4a7c15ULL;
			const auto Session = FHansaSessionId::TryCreate(Derived == 0 ? 1 : Derived, State.CampaignId.GetGeneration());
			if (!Session) { Result.Error = TEXT("Legacy campaign identity cannot produce a session identity."); return Result; }
			State.SessionId = Session.Value; Result.AppliedMigrations.Add(TEXT("Hansa.Multiplayer.Session.1To2.SeparateSessionAndTeamPolicy"));
		}
		FString ScenarioText;
		if (!Reader.U32(State.Compatibility.ProtocolVersion) || !Reader.U64(State.Compatibility.ContentManifestHash) || !Reader.String(ScenarioText)) { Result.Error = Reader.Error; return Result; }
		const auto Scenario = FHansaScenarioId::TryParse(ScenarioText); if (!Scenario) { Result.Error = TEXT("Session scenario ID is invalid."); return Result; } State.Compatibility.ScenarioId = Scenario.Value;
		uint16 Count = 0; if (!Reader.Count(Count, MaximumSlots)) { Result.Error = Reader.Error; return Result; }
		for (uint16 Index = 0; Index < Count; ++Index)
		{
			FHansaSessionSlot Slot; uint8 SlotState = 0;
			if (!Reader.String(Slot.SlotId) || !Reader.Id(Slot.HouseId) || !Reader.U8(SlotState) || SlotState > static_cast<uint8>(EHansaSessionSlotState::Reserved) || !Reader.Id(Slot.ParticipantId) || (Version >= 2 && !Reader.Id(Slot.TeamId))) { Result.Error = Reader.Error.IsEmpty() ? TEXT("Session slot is invalid.") : Reader.Error; return Result; }
			Slot.State = static_cast<EHansaSessionSlotState>(SlotState); State.Slots.Add(MoveTemp(Slot));
		}
		if (!Reader.Count(Count, MaximumParticipants)) { Result.Error = Reader.Error; return Result; }
		for (uint16 Index = 0; Index < Count; ++Index)
		{
			FHansaParticipant Participant; uint8 Lifecycle = 0;
			if (!Reader.Id(Participant.Id) || !Reader.U8(Lifecycle) || Lifecycle > static_cast<uint8>(EHansaParticipantLifecycle::Banned) || !Reader.U8(Participant.RoleMask) || !Reader.Id(Participant.HouseId) || (Version >= 2 && !Reader.Id(Participant.TeamId))) { Result.Error = Reader.Error.IsEmpty() ? TEXT("Session participant is invalid.") : Reader.Error; return Result; }
			Participant.Lifecycle = static_cast<EHansaParticipantLifecycle>(Lifecycle); State.Participants.Add(MoveTemp(Participant));
		}
		if (Version >= 2)
		{
			if (!Reader.Count(Count, MaximumGrants)) { Result.Error = Reader.Error; return Result; }
			for (uint16 Index = 0; Index < Count; ++Index) { FHansaHouseAccessGrant Grant; if (!Reader.Id(Grant.HouseId) || !Reader.Id(Grant.ParticipantId) || !Reader.Id(Grant.TeamId) || !Reader.U16(Grant.PermissionMask)) { Result.Error = Reader.Error; return Result; } State.AccessGrants.Add(MoveTemp(Grant)); }
		}
		if (!Reader.Finish()) { Result.Error = Reader.Error; return Result; }
		OutState = MoveTemp(State); Result.bSuccess = true; return Result;
	}

#if WITH_HANSA_AUTOMATION
	bool FHansaSessionModel::SerializeLegacyV1ForMigrationTest(const FHansaSessionState& State,
		TArray<uint8>& OutBytes, FString& OutError)
	{
		return WriteState(State, LegacyVersion, OutBytes, OutError);
	}
#endif
}
