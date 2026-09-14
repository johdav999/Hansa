#include "Save/HansaSaveEnvelope.h"

#include "Diagnostics/HansaStateHash.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Misc/Compression.h"
#include "Misc/DateTime.h"
#include "Misc/SecureHash.h"
#include "Save/HansaPrimitiveSerialization.h"
#include <type_traits>

namespace Hansa::Simulation
{
	/** Private field codec: explicit scalar order, bounded allocation, no native archive object loading. */
	class FHansaSaveCodec final
	{
	public:
		TArray<uint8> Bytes;
		uint32 FormatVersion = FHansaSaveEnvelope::CurrentFormatVersion;
		bool bReading = false;
		bool bValid = true;
		int32 Offset = 0;
		int64 AllocationBudget = FHansaSaveEnvelope::MaximumBytes * 4LL;
		TArray<FHansaPlacementMapInitialization> LegacyPlacementMaps;

		FHansaSaveCodec() = default;
		explicit FHansaSaveCodec(TConstArrayView<uint8> Input) : bReading(true) { Bytes.Append(Input); }
		bool Finished() const { return bValid && (!bReading || Offset == Bytes.Num()); }

		template<typename T> requires std::is_integral_v<T>
		void Value(T& V)
		{
			if (!bValid) return;
			if constexpr (std::is_same_v<T, bool>)
			{
				uint8 Raw = V ? 1 : 0; Value(Raw);
				if (Raw > 1) bValid = false;
				if (bReading) V = Raw != 0;
			}
			else
			{
				using U = std::make_unsigned_t<T>;
				U Raw = static_cast<U>(V);
				if (bReading)
				{
					if (Bytes.Num() - Offset < static_cast<int32>(sizeof(T))) { bValid = false; return; }
					Raw = 0;
					for (uint32 I = 0; I < sizeof(T); ++I) Raw |= static_cast<U>(Bytes[Offset++]) << (I * 8);
					FMemory::Memcpy(&V, &Raw, sizeof(T));
				}
				else
				{
					if (Bytes.Num() > FHansaSaveEnvelope::MaximumBytes - static_cast<int32>(sizeof(T))) { bValid = false; return; }
					for (uint32 I = 0; I < sizeof(T); ++I) Bytes.Add(static_cast<uint8>(Raw >> (I * 8)));
				}
			}
		}

		template<typename T> requires std::is_enum_v<T>
		void Value(T& V)
		{
			uint8 Raw = static_cast<uint8>(V); Value(Raw);
			if (bReading) V = static_cast<T>(Raw);
			// Every serialized enum has an explicit closed range in this schema.
			uint8 Max = 0;
			if constexpr (std::is_same_v<T, EHansaConstructionState>) Max = 1;
			else if constexpr (std::is_same_v<T, EHansaRouteMode>) Max = 1;
			else if constexpr (std::is_same_v<T, EHansaRouteCargoActionKind>) Max = 1;
			else if constexpr (std::is_same_v<T, EHansaRouteLifecycleState>) Max = 3;
			else if constexpr (std::is_same_v<T, EHansaRouteTransferOutcome>) Max = 3;
			else if constexpr (std::is_same_v<T, EHansaResearchEffectKind>) Max = 7;
			else if constexpr (std::is_same_v<T, EHansaPlacementTerrain>) Max = 2;
			else if constexpr (std::is_same_v<T, EHansaGridRotation>) Max = 3;
			else if constexpr (std::is_same_v<T, EHansaInventoryOwnerKind>) Max = 3;
			else if constexpr (std::is_same_v<T, EHansaInventoryMovementKind>) Max = 5;
			else if constexpr (std::is_same_v<T, EHansaProductionKind>) Max = 1;
			else if constexpr (std::is_same_v<T, EHansaProductionBlocker>) Max = 8;
			else if constexpr (std::is_same_v<T, EHansaLogisticsPriority>) Max = 3;
			else if constexpr (std::is_same_v<T, EHansaLogisticsRequestStatus>) Max = 2;
			else if constexpr (std::is_same_v<T, EHansaLogisticsBottleneck>) Max = 6;
			else if constexpr (std::is_same_v<T, EHansaLogisticsJobStatus>) Max = 4;
			else if constexpr (std::is_same_v<T, EHansaLogisticsRoadPathFailure>) Max = 13;
			else if constexpr (std::is_same_v<T, EHansaCommandOrigin>) Max = 3;
			else if constexpr (std::is_same_v<T, EHansaScenarioOutcome>) Max = 2;
			else if constexpr (std::is_same_v<T, EHansaGameplayCommandType>) Max = 12;
			else static_assert(std::is_same_v<T, EHansaRouteCargoCondition>, "Add save enum range");
			if (Raw > Max) bValid = false;
		}

		template<typename T> void Value(TArray<T>& V)
		{
			int32 Count = V.Num(); Value(Count);
			if (!bValid || Count < 0 || Count > FHansaSaveEnvelope::MaximumBytes ||
				(bReading && (Count > Bytes.Num() - Offset || static_cast<int64>(Count) * static_cast<int64>(sizeof(T)) > AllocationBudget)))
			{ bValid = false; return; }
			if (bReading) { AllocationBudget -= static_cast<int64>(Count) * static_cast<int64>(sizeof(T)); V.SetNum(Count); }
			for (T& Item : V) { if (!bValid) break; Value(Item); }
		}

		void Value(FString& V)
		{
			// UTF-16 code units, independent of native FString serialization and locale.
			int32 Count = V.Len(); Value(Count);
			if (!bValid || Count < 0 || Count > 65536 || (bReading && Count > (Bytes.Num() - Offset) / 2))
			{ bValid = false; return; }
			if (bReading) V.Empty(Count);
			for (int32 I = 0; I < Count && bValid; ++I)
			{
				uint16 C = bReading ? 0 : static_cast<uint16>(V[I]); Value(C);
				if (C == 0) bValid = false;
				if (bReading) V.AppendChar(static_cast<TCHAR>(C));
			}
		}
		void Value(FName& V) { FString S = V.IsNone() ? FString() : V.ToString(); Value(S); if (bReading && bValid) V = FName(*S); }
		template<typename T> void Value(THansaDefinitionId<T>& V)
		{
			FString S = V.ToString(); Value(S);
			if (bReading && bValid)
			{
				if (S.IsEmpty()) V = {};
				else { const auto Parsed = THansaDefinitionId<T>::TryParse(S); bValid = Parsed.IsSuccess(); if (bValid) V = Parsed.Value; }
			}
		}
		template<typename T> void Value(THansaEntityId<T>& V)
		{
			uint64 Id = V.GetValue(); uint32 Generation = V.GetGeneration(); Value(Id); Value(Generation);
			if (bReading && bValid)
			{
				if (Id == 0 && Generation == 0) V = {};
				else { const auto Parsed = THansaEntityId<T>::TryCreate(Id, Generation); bValid = Parsed.IsSuccess(); if (bValid) V = Parsed.Value; }
			}
		}
		template<typename T> void Value(THansaSignedUnit<T>& V) { int64 Raw = V.GetRawValue(); Value(Raw); if (bReading) V = THansaSignedUnit<T>::FromRaw(Raw); }
		void Value(FHansaRate& V) { int64 Raw = V.GetPartsPerMillion(); Value(Raw); if (bReading) V = FHansaRate::FromPartsPerMillion(Raw); }
		void Value(FHansaSimulationTick& V)
		{
			int64 Raw = V.GetValue(); Value(Raw); const auto Parsed = FHansaSimulationTick::TryCreate(Raw);
			bValid &= Parsed.IsSuccess(); if (bReading && bValid) V = Parsed.Value;
		}
		template<typename T, typename W, typename R> void Primitive(T& V, W Write, R Read)
		{
			TArray<uint8> Data;
			if (!bReading) { FHansaPrimitiveWriter Writer; bValid &= (Writer.*Write)(V); Data = Writer.GetBytes(); }
			Value(Data);
			if (bReading && bValid) { FHansaPrimitiveReader Reader(Data); bValid = (Reader.*Read)(V) && Reader.Finish(); }
		}
		void Value(FHansaSimulationClock& V) { Primitive(V, &FHansaPrimitiveWriter::WriteClock, &FHansaPrimitiveReader::ReadClock); }
		void Value(FHansaRandomStream& V) { Primitive(V, &FHansaPrimitiveWriter::WriteRandomStream, &FHansaPrimitiveReader::ReadRandomStream); }

#include "HansaSaveFields.inl"
#include "HansaSaveValidation.inl"

		static bool MigrateV5LocalLogistics(
			FHansaSimulationState& State,
			const FHansaEconomicRegistry* Registry)
		{
			const int64 CurrentTick = State.Clock.GetTick().GetValue();
			for (FHansaLogisticsJobState& Job : State.LocalLogisticsJobs)
			{
				const int32 LegacyTravelTicks = static_cast<int32>(FMath::Clamp<int64>(
					Job.DeliveryTick.GetValue() - Job.PickupTick.GetValue(), 1, MAX_int32));
				Job.ElapsedTravelTicks = Job.Status == EHansaLogisticsJobStatus::AwaitingPickup
					? 0
					: static_cast<int32>(FMath::Clamp<int64>(
						CurrentTick - Job.PickupTick.GetValue(), 0, LegacyTravelTicks));
				Job.RemainingTravelTicks = Job.Status == EHansaLogisticsJobStatus::Completed
					? 0 : FMath::Max(1, LegacyTravelTicks - Job.ElapsedTravelTicks);
				const FHansaLogisticsRoadPathProjection Path = FHansaLocalLogisticsQueries::QueryRoadPath(
					Job.SourceInventoryId, Job.DestinationInventoryId,
					State.InventoryLedger.CreateReadOnlyAccess(), State.Placement, State.Buildings, Registry);
				if (Path.bConnected)
				{
					Job.SelectedMarketBuildingId = Path.SelectedMarketBuildingId;
					Job.RouteCells = Path.RouteCells;
					Job.RoadDistanceCells = Path.RoadDistanceCells;
					const int32 NewTravelTicks = static_cast<int32>(FMath::Min<int64>(MAX_int32,
						FMath::Max<int64>(1, static_cast<int64>(Path.RoadDistanceCells) *
							State.LocalLogisticsSettings.TicksPerRoadCell)));
					if (Job.Status != EHansaLogisticsJobStatus::Completed)
					{
						Job.RemainingTravelTicks = FMath::Max(1, NewTravelTicks - Job.ElapsedTravelTicks);
					}
				}
				else if (Job.Status == EHansaLogisticsJobStatus::AwaitingPickup)
				{
					if (Job.SourceReservationId.IsValid())
					{
						const FHansaInventoryTransactionResult Release = State.InventoryLedger.TryReleaseReservation(
							Job.SourceReservationId, State.Clock.GetTick(),
							State.InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
						if (!Release.IsSuccess()) return false;
						Job.SourceReservationId = FHansaReservationId();
					}
					Job.Status = EHansaLogisticsJobStatus::PausedAwaitingPickup;
					Job.PauseReason = Path.Failure;
				}
				else if (Job.Status == EHansaLogisticsJobStatus::InTransit)
				{
					Job.Status = EHansaLogisticsJobStatus::PausedInTransit;
					Job.PauseReason = Path.Failure;
				}
				if (Job.Status == EHansaLogisticsJobStatus::InTransit ||
					Job.Status == EHansaLogisticsJobStatus::PausedInTransit ||
					Job.Status == EHansaLogisticsJobStatus::Completed)
				{
					Job.SourceReservationId = FHansaReservationId();
				}
			}
			return true;
		}

		void Value(FHansaSaveRouteLabel& V) { Value(V.RouteValue); Value(V.Label); }
		void Value(FHansaGameplayCommand& V)
		{
			Value(V.Header); Value(V.Type);
			switch (V.Type)
			{
#define HANSA_SAVE_COMMAND(Name) case EHansaGameplayCommandType::Name: Value(V.Name); break
			HANSA_SAVE_COMMAND(CreateTestEntity); HANSA_SAVE_COMMAND(CancelTestEntity); HANSA_SAVE_COMMAND(NoOpTest);
			HANSA_SAVE_COMMAND(SetProductionActive); HANSA_SAVE_COMMAND(PlaceBuilding); HANSA_SAVE_COMMAND(CancelConstruction);
			HANSA_SAVE_COMMAND(RemoveBuilding); HANSA_SAVE_COMMAND(UpgradeResidence); HANSA_SAVE_COMMAND(CreateRoute);
			HANSA_SAVE_COMMAND(EditRoute); HANSA_SAVE_COMMAND(SetRouteActive); HANSA_SAVE_COMMAND(CancelRoute); HANSA_SAVE_COMMAND(QueueResearch);
#undef HANSA_SAVE_COMMAND
			default: bValid = false;
			}
		}
	};

	namespace
	{
		constexpr uint32 Magic = 0x56534E48; // HNSV
		constexpr int32 DigestBytes = 20;
		FHansaSaveResult Failure(EHansaSaveError Error, const TCHAR* Message)
		{
			FHansaSaveResult R; R.Error = Error; R.Message = Message; return R;
		}
		uint64 HashBytes(TConstArrayView<uint8> Bytes)
		{
			uint64 Hash = 14695981039346656037ULL;
			for (uint8 B : Bytes) { Hash ^= B; Hash *= 1099511628211ULL; }
			return Hash;
		}
		void Payload(FHansaSaveCodec& Codec, FHansaSaveSnapshot& Snapshot)
		{
			Codec.Value(Snapshot.State); Codec.Value(Snapshot.Players);
			Codec.Value(Snapshot.NextCommandId); Codec.Value(Snapshot.NextBuildingId);
			Codec.Value(Snapshot.PendingCommands); Codec.Value(Snapshot.Scenario);
			if (Codec.FormatVersion >= 3) Codec.Value(Snapshot.RouteLabels);
		}
		bool Validate(const FHansaSaveSnapshot& S, const FHansaSimulationDefinitionContext& D)
		{
			if (!S.State.IsInitialized() || !D.IsValid() || !FHansaSaveCodec::ValidateState(S.State, D)) return false;
			FDateTime Timestamp;
			if (S.BuildVersion.IsEmpty() || !FDateTime::ParseIso8601(*S.SavedUtc, Timestamp) || !S.SavedUtc.EndsWith(TEXT("Z"))) return false;
			const auto View = S.State.CreateReadOnlyAccess(D);
			if (S.NextCommandId != 0 && S.NextCommandId <= View.GetLastProcessedCommandId().GetValue()) return false;
			for (const auto& B : View.GetBuildings()) if (S.NextBuildingId != 0 && S.NextBuildingId <= B.Id.GetValue()) return false;
			TSet<uint64> LabelIds;
            for (const auto& L : S.RouteLabels)
            {
                if (!L.RouteValue || LabelIds.Contains(L.RouteValue) || L.Label.IsEmpty() || L.Label.Len() > 48 || L.Label.TrimStartAndEnd() != L.Label) return false;
                for (TCHAR C : L.Label) if (C < 32 || C == 127) return false;
                if (!View.QueryRoute(FHansaRouteId::TryCreate(L.RouteValue).Value).IsSet()) return false;
                LabelIds.Add(L.RouteValue);
            }
			TSet<uint64> Principals;
			for (const auto& Player : S.Players)
			{
				bool bHouseExists = false;
				for (const auto& House : View.GetHouses()) bHouseExists |= House.Id == Player.HouseId;
				if (!Player.PrincipalId || !Player.HouseId.IsValid() || !bHouseExists || Principals.Contains(Player.PrincipalId)) return false;
				Principals.Add(Player.PrincipalId);
			}
			uint64 Sequence = View.GetLastProcessedCommandSequence();
			FHansaCommandId PreviousId = View.GetLastProcessedCommandId();
			int64 Tick = View.GetClock().GetTick().GetValue();
			for (const auto& Command : S.PendingCommands)
			{
				const auto& H = Command.GetHeader();
				if (H.SchemaVersion != FHansaCommandHeader::CurrentSchemaVersion || !H.CommandId.IsValid() ||
					H.GlobalSequence <= Sequence || (PreviousId.IsValid() && !(PreviousId < H.CommandId)) ||
					H.RequestedExecutionTick.GetValue() < Tick || !S.Players.ContainsByPredicate([&](const auto& P)
					{ return P.PrincipalId == H.Authority.PrincipalId && P.HouseId == H.Authority.IssuingHouseId; })) return false;
				Sequence = H.GlobalSequence; PreviousId = H.CommandId; Tick = H.RequestedExecutionTick.GetValue();
			}
			if (!S.Scenario.ScenarioId.IsEmpty())
			{
				FHansaScenarioEvaluator Evaluator;
				if (!FHansaSaveEnvelope::RestoreScenario(S.Scenario, S.State, D, Evaluator)) return false;
			}
			return true;
		}
	}

	FHansaSaveResult FHansaSaveEnvelope::Encode(const FHansaSaveSnapshot& Snapshot,
		const FHansaSimulationDefinitionContext& Definitions, TArray<uint8>& OutBytes)
	{
		if (!Validate(Snapshot, Definitions)) return Failure(EHansaSaveError::InvalidSnapshot, TEXT("Save snapshot or ownership is invalid; capture an initialized authoritative session at a tick boundary."));
		if (Snapshot.State.CreateReadOnlyAccess(Definitions).GetClock().GetVersion().GetValue() != FHansaSimulationClock::CurrentSimulationVersion)
			return Failure(EHansaSaveError::IncompatibleSimulation, TEXT("Simulation version is not supported by this build."));
		FHansaSaveSnapshot Copy = Snapshot;
		Copy.Players.Sort([](const auto& A, const auto& B) { return A.PrincipalId < B.PrincipalId; });
		FHansaSaveCodec Body; Payload(Body, Copy);
		if (!Body.Finished()) return Failure(EHansaSaveError::InvalidSnapshot, TEXT("Save contains invalid values or exceeds archive limits."));
		int32 CompressedSize = FCompression::CompressMemoryBound(NAME_Zlib, Body.Bytes.Num());
		TArray<uint8> Compressed; Compressed.SetNumUninitialized(CompressedSize);
		if (!FCompression::CompressMemory(NAME_Zlib, Compressed.GetData(), CompressedSize, Body.Bytes.GetData(), Body.Bytes.Num()))
			return Failure(EHansaSaveError::InvalidSnapshot, TEXT("Unable to compress the authoritative snapshot."));
		Compressed.SetNum(CompressedSize);
		FHansaSaveResult Result;
		Result.SourceFormatVersion = CurrentFormatVersion;
		Result.AuthoritativeHash = FHansaStateHasher::Compute(Copy.State, Definitions).GetOverallHash();
		Result.CampaignHash = HashBytes(Body.Bytes);
		FHansaSaveCodec Header;
		uint32 M = Magic, Format = CurrentFormatVersion, Simulation = FHansaSimulationClock::CurrentSimulationVersion;
		uint32 Pipeline = FHansaSimulationState::CurrentSystemPipelineVersion, Fingerprint = FHansaSimulationState::DeterminismFingerprintVersion;
		uint64 Content = Definitions.GetDefinitionHash();
		uint64 Registry = Definitions.GetEconomicRegistry() ? Definitions.GetEconomicRegistry()->GetRegistryHash() : 0;
		uint64 PlacementTopology = Copy.State.CreateReadOnlyAccess(Definitions).GetPlacement().GetTopologyHash();
		FString Scenario = Definitions.GetScenarioId().ToString();
		Header.Value(M); Header.Value(Format); Header.Value(Simulation); Header.Value(Pipeline); Header.Value(Fingerprint);
		Header.Value(Content); Header.Value(Registry); Header.Value(PlacementTopology); Header.Value(Scenario);
		Header.Value(Copy.BuildVersion); Header.Value(Copy.SavedUtc); Header.Value(Copy.DisplayName);
		Header.Value(Copy.MigrationHistory);
		Header.Value(Result.AuthoritativeHash); Header.Value(Result.CampaignHash);
		int32 Size = Body.Bytes.Num(); Header.Value(Size); Header.Value(Compressed);
		if (!Header.Finished() || Header.Bytes.Num() > MaximumBytes - DigestBytes)
			return Failure(EHansaSaveError::SizeLimitExceeded, TEXT("Save exceeds the 64 MiB archive limit."));
		uint8 Digest[DigestBytes]; FSHA1::HashBuffer(Header.Bytes.GetData(), Header.Bytes.Num(), Digest);
		Header.Bytes.Append(Digest, DigestBytes); OutBytes = MoveTemp(Header.Bytes);
		return Result;
	}

	FHansaSaveResult FHansaSaveEnvelope::InspectMetadata(TConstArrayView<uint8> Bytes, FHansaSaveMetadata& OutMetadata)
	{
		if (Bytes.Num() > MaximumBytes) return Failure(EHansaSaveError::SizeLimitExceeded, TEXT("Save exceeds the 64 MiB archive limit."));
		if (Bytes.Num() < DigestBytes + 8) return Failure(EHansaSaveError::CorruptData, TEXT("Save is truncated; try another save slot."));
		uint8 Digest[DigestBytes]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num() - DigestBytes, Digest);
		if (FMemory::Memcmp(Digest, Bytes.GetData() + Bytes.Num() - DigestBytes, DigestBytes) != 0)
			return Failure(EHansaSaveError::CorruptData, TEXT("Save integrity check failed; restore a backup."));
		FHansaSaveCodec Header(Bytes.Left(Bytes.Num() - DigestBytes));
		uint32 MagicValue = 0; FHansaSaveMetadata Candidate;
		Header.Value(MagicValue); Header.Value(Candidate.FormatVersion);
		if (MagicValue != Magic) return Failure(EHansaSaveError::CorruptData, TEXT("File is not a Hansa save."));
		if (Candidate.FormatVersion < 1 || Candidate.FormatVersion > CurrentFormatVersion)
		{
			OutMetadata.FormatVersion = Candidate.FormatVersion;
			FHansaSaveResult Result = Failure(EHansaSaveError::UnsupportedFormat, TEXT("Save format is unsupported; use a compatible game build."));
			Result.SourceFormatVersion = Candidate.FormatVersion; return Result;
		}
		Header.Value(Candidate.SimulationVersion); Header.Value(Candidate.PipelineVersion); Header.Value(Candidate.FingerprintVersion);
		Header.Value(Candidate.ContentHash); Header.Value(Candidate.RegistryHash);
		if (Candidate.FormatVersion >= 7) Header.Value(Candidate.PlacementTopologyHash);
		Header.Value(Candidate.ScenarioId);
		Header.Value(Candidate.BuildVersion); Header.Value(Candidate.SavedUtc);
		if (Candidate.FormatVersion == 1) Candidate.DisplayName = Candidate.ScenarioId;
		else { Header.Value(Candidate.DisplayName); TArray<FString> MigrationHistory; Header.Value(MigrationHistory); }
		Header.Value(Candidate.AuthoritativeHash); Header.Value(Candidate.CampaignHash);
		int32 Size = 0; TArray<uint8> Compressed; Header.Value(Size); Header.Value(Compressed);
		if (!Header.Finished() || Size <= 0 || Size > MaximumBytes || Compressed.IsEmpty())
			return Failure(EHansaSaveError::CorruptData, TEXT("Save header or payload size is invalid."));
		OutMetadata = MoveTemp(Candidate);
		FHansaSaveResult Result; Result.SourceFormatVersion = OutMetadata.FormatVersion;
		Result.AuthoritativeHash = OutMetadata.AuthoritativeHash; Result.CampaignHash = OutMetadata.CampaignHash;
		return Result;
	}
	FHansaSaveResult FHansaSaveEnvelope::Decode(TConstArrayView<uint8> Bytes,
		const FHansaSimulationDefinitionContext& Definitions, FHansaSaveSnapshot& OutSnapshot)
	{
		if (Bytes.Num() > MaximumBytes) return Failure(EHansaSaveError::SizeLimitExceeded, TEXT("Save exceeds the 64 MiB archive limit."));
		if (Bytes.Num() < DigestBytes + 8) return Failure(EHansaSaveError::CorruptData, TEXT("Save is truncated; try another save slot."));
		uint8 Digest[DigestBytes]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num() - DigestBytes, Digest);
		if (FMemory::Memcmp(Digest, Bytes.GetData() + Bytes.Num() - DigestBytes, DigestBytes) != 0)
			return Failure(EHansaSaveError::CorruptData, TEXT("Save integrity check failed; restore a backup."));
		FHansaSaveCodec Header(Bytes.Left(Bytes.Num() - DigestBytes));
		uint32 M = 0, Format = 0, Simulation = 0, Pipeline = 0, Fingerprint = 0;
		Header.Value(M); Header.Value(Format);
		if (M != Magic) return Failure(EHansaSaveError::CorruptData, TEXT("File is not a Hansa save."));
		if (Format < 1 || Format > CurrentFormatVersion) return Failure(EHansaSaveError::UnsupportedFormat, TEXT("Save format is unsupported; use a compatible game build."));
		Header.Value(Simulation); Header.Value(Pipeline); Header.Value(Fingerprint);
		const bool bLegacyConsumption = Format < 4 && Fingerprint == 16;
        const bool bLegacyResidenceConsumption = Format == 4 && Fingerprint == 17;
		const bool bLegacyLocalLogistics = Format == 5 && Fingerprint == 18;
		const bool bLegacyTopology = Format == 6 && Fingerprint == 19;
        if (Simulation != FHansaSimulationClock::CurrentSimulationVersion || Pipeline != FHansaSimulationState::CurrentSystemPipelineVersion ||
			(!bLegacyConsumption && !bLegacyResidenceConsumption && !bLegacyLocalLogistics && !bLegacyTopology &&
                (Format != CurrentFormatVersion || Fingerprint != FHansaSimulationState::DeterminismFingerprintVersion)))
			return Failure(EHansaSaveError::IncompatibleSimulation, TEXT("Save simulation rules differ from this build; an explicit migration is required."));
		uint64 Content = 0, Registry = 0, PlacementTopologyHash = 0; FString Scenario;
		Header.Value(Content); Header.Value(Registry);
		if (Format >= 7) Header.Value(PlacementTopologyHash);
		Header.Value(Scenario);
		if (!Definitions.IsValid())
			return Failure(EHansaSaveError::IncompatibleContent, TEXT("Save content/registry hash differs; install the original content or an explicit definition migration."));
		const uint64 CurrentRegistryHash = Definitions.GetEconomicRegistry()
			? Definitions.GetEconomicRegistry()->GetRegistryHash()
			: 0;
		TOptional<FString> DefinitionMigration;
		if (Content != Definitions.GetDefinitionHash() || Registry != CurrentRegistryHash)
		{
			DefinitionMigration = Definitions.FindCompatibleDefinitionMigration(Content, Registry);
			if (!DefinitionMigration.IsSet())
				return Failure(EHansaSaveError::IncompatibleContent, TEXT("Save content/registry hash differs; install the original content or an explicit definition migration."));
		}
		if (Scenario != Definitions.GetScenarioId().ToString()) return Failure(EHansaSaveError::IncompatibleScenario, TEXT("Save belongs to a different scenario."));
		if (Format >= 7 && PlacementTopologyHash != Definitions.GetPlacementTopologyHash())
			return Failure(EHansaSaveError::IncompatibleContent, TEXT("Save placement topology differs; install the original map definition or an explicit topology migration."));
		FHansaSaveSnapshot Candidate; FHansaSaveResult Result; Result.SourceFormatVersion = Format;
		if (DefinitionMigration.IsSet()) Result.AppliedMigrations.Add(DefinitionMigration.GetValue());
		Header.Value(Candidate.BuildVersion); Header.Value(Candidate.SavedUtc);
		if (Format == 1)
		{
			// Synthetic v1 had no user display name. Preserve stable scenario identity as its deterministic default.
			Candidate.DisplayName = Scenario;
			Result.AppliedMigrations.Add(TEXT("Hansa.Save.1To2.AddDisplayNameFromScenario"));
		}
		else { Header.Value(Candidate.DisplayName); Header.Value(Candidate.MigrationHistory); }
		Candidate.MigrationHistory.Append(Result.AppliedMigrations);
		Header.Value(Result.AuthoritativeHash); Header.Value(Result.CampaignHash);
		int32 Size = 0; TArray<uint8> Compressed; Header.Value(Size); Header.Value(Compressed);
		if (!Header.Finished() || Size <= 0 || Size > MaximumBytes || Compressed.IsEmpty())
			return Failure(EHansaSaveError::CorruptData, TEXT("Save header or payload size is invalid."));
		TArray<uint8> Uncompressed; Uncompressed.SetNumUninitialized(Size);
		if (!FCompression::UncompressMemory(NAME_Zlib, Uncompressed.GetData(), Size, Compressed.GetData(), Compressed.Num()) || HashBytes(Uncompressed) != Result.CampaignHash)
			return Failure(EHansaSaveError::CorruptData, TEXT("Compressed save payload failed validation."));
		FHansaSaveCodec Body(Uncompressed); Body.FormatVersion = Format; Payload(Body, Candidate);
		if (Format < 7)
		{
			TSharedPtr<const FHansaPlacementTopology> LegacyTopology;
			if (!Body.LegacyPlacementMaps.IsEmpty())
			{
				auto CreatedTopology = FHansaPlacementTopology::TryCreate(MoveTemp(Body.LegacyPlacementMaps));
				if (!CreatedTopology)
					return Failure(EHansaSaveError::CorruptData, TEXT("Legacy save placement topology is invalid."));
				LegacyTopology = MakeShared<FHansaPlacementTopology>(MoveTemp(CreatedTopology.Value));
			}
			Candidate.State.Placement.Topology = LegacyTopology;
			if (Candidate.State.Placement.GetTopologyHash() != Definitions.GetPlacementTopologyHash())
				return Failure(EHansaSaveError::IncompatibleContent, TEXT("Legacy save placement topology differs; install the original map definition or an explicit topology migration."));
		}
		else
		{
			Candidate.State.Placement.Topology = Definitions.GetPlacementTopologyShared();
		}
		if (Format < 3) { Result.AppliedMigrations.Add(TEXT("Hansa.Save.2To3.EmptyCosmeticRouteLabels")); Candidate.MigrationHistory.Add(TEXT("Hansa.Save.2To3.EmptyCosmeticRouteLabels")); }
		TOptional<FHansaSimulationDefinitionContext> SavedDefinitions;
		const FHansaSimulationDefinitionContext* HashDefinitions = &Definitions;
		if (DefinitionMigration.IsSet())
		{
			const auto CreatedSavedDefinitions = FHansaSimulationDefinitionContext::TryCreate(
				Definitions.GetScenarioId(), Content);
			if (!CreatedSavedDefinitions.IsSuccess())
				return Failure(EHansaSaveError::IncompatibleContent, TEXT("The registered definition migration has an invalid source catalog hash."));
			SavedDefinitions = CreatedSavedDefinitions.Value;
			HashDefinitions = &SavedDefinitions.GetValue();
		}
		if (!Body.Finished())
			return Failure(EHansaSaveError::CorruptData, TEXT("Authoritative save payload contains invalid or incomplete records."));
		if (Candidate.State.CreateReadOnlyAccess(Definitions).GetClock().GetVersion().GetValue() != Simulation)
			return Failure(EHansaSaveError::CorruptData, TEXT("Authoritative save clock does not match its header."));
		if (
			(bLegacyConsumption ? FHansaStateHasher::ComputeLegacyV16(Candidate.State, *HashDefinitions) :
				bLegacyResidenceConsumption ? FHansaStateHasher::ComputeLegacyV17(Candidate.State, *HashDefinitions) :
				bLegacyLocalLogistics ? FHansaStateHasher::ComputeLegacyV18(Candidate.State, *HashDefinitions) :
				bLegacyTopology ? FHansaStateHasher::ComputeLegacyV19(Candidate.State, *HashDefinitions) :
				FHansaStateHasher::Compute(Candidate.State, *HashDefinitions)).GetOverallHash() != Result.AuthoritativeHash)
			return Failure(EHansaSaveError::CorruptData, TEXT("Authoritative save records or round-trip hash are invalid."));
		if (Format < 6)
		{
			if (!FHansaSaveCodec::MigrateV5LocalLogistics(Candidate.State, Definitions.GetEconomicRegistry()))
				return Failure(EHansaSaveError::CorruptData, TEXT("Legacy logistics reservations could not be migrated safely."));
			const FString Migration = TEXT("Hansa.Save.5To6.PersistLocalDeliveryRoutesAndPauses");
			Result.AppliedMigrations.Add(Migration);
			Candidate.MigrationHistory.Add(Migration);
			Result.AuthoritativeHash = FHansaStateHasher::Compute(Candidate.State, Definitions).GetOverallHash();
		}
		if (!Validate(Candidate, Definitions))
			return Failure(EHansaSaveError::CorruptData, TEXT("Migrated authoritative save records are invalid."));
        if (bLegacyConsumption)
        {
            const FString Migration = TEXT("Hansa.Save.3To4.StartConsumptionHistory");
            Result.AppliedMigrations.Add(Migration);
            Candidate.MigrationHistory.Add(Migration);
            Result.AuthoritativeHash = FHansaStateHasher::Compute(Candidate.State, Definitions).GetOverallHash();
        }
		if (Format < 5)
        {
            const FString Migration = TEXT("Hansa.Save.4To5.StartResidenceConsumptionHistory");
            Result.AppliedMigrations.Add(Migration);
            Candidate.MigrationHistory.Add(Migration);
            Result.AuthoritativeHash = FHansaStateHasher::Compute(Candidate.State, Definitions).GetOverallHash();
		}
		if (Format < 7)
		{
			const FString Migration = TEXT("Hansa.Save.6To7.MovePlacementTopologyToDefinitions");
			Result.AppliedMigrations.Add(Migration);
			Candidate.MigrationHistory.Add(Migration);
			Candidate.State.Placement.Topology = Definitions.GetPlacementTopologyShared();
			Candidate.State.InvalidateAllStateHashCaches();
			Result.AuthoritativeHash = FHansaStateHasher::Compute(Candidate.State, Definitions).GetOverallHash();
		}
		if (DefinitionMigration.IsSet())
		{
			Result.AuthoritativeHash = FHansaStateHasher::Compute(Candidate.State, Definitions).GetOverallHash();
		}
		OutSnapshot = MoveTemp(Candidate);
		return Result;
	}

	FHansaSaveScenarioState FHansaSaveEnvelope::CaptureScenario(const FHansaScenarioEvaluator& E)
	{
		FHansaSaveScenarioState S;
		if (!E.IsInitialized()) return S;
		S.HouseId = E.HouseId; S.ScenarioId = E.Progress.ScenarioId; S.Outcome = E.Progress.Outcome;
		S.WinningVictoryId = E.Progress.WinningVictoryId; S.ConsecutiveFailureTicks = E.Progress.ConsecutiveFailureTicks;
		S.LastEvaluatedTick = E.Progress.LastEvaluatedTick;
		for (const auto& P : E.Progress.VictoryPaths) S.VictoryStreaks.Add({P.VictoryId, P.ConsecutiveSatisfiedTicks});
		return S;
	}

	bool FHansaSaveEnvelope::RestoreScenario(const FHansaSaveScenarioState& S, const FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& D, FHansaScenarioEvaluator& OutEvaluator)
	{
		if (!State.IsInitialized() || !D.IsValid() || !D.GetEconomicRegistry() || S.ConsecutiveFailureTicks < 0) return false;
		const auto View = State.CreateReadOnlyAccess(D);
		bool bHouseExists = false;
		for (const auto& H : View.GetHouses()) bHouseExists |= H.Id == S.HouseId;
		if (!bHouseExists || S.LastEvaluatedTick.GetValue() > View.GetClock().GetTick().GetValue()) return false;
		FHansaScenarioEvaluator E;
		if (!E.Initialize(*D.GetEconomicRegistry(), S.ScenarioId, S.HouseId) || !E.Evaluate(View, *D.GetEconomicRegistry()) ||
			E.Progress.VictoryPaths.Num() != S.VictoryStreaks.Num()) return false;
		TSet<FString> Seen; bool bWinnerFound = false;
		for (auto& P : E.Progress.VictoryPaths)
		{
			const auto* Streak = S.VictoryStreaks.FindByPredicate([&](const auto& V) { return V.VictoryId == P.VictoryId; });
			if (!Streak || Seen.Contains(Streak->VictoryId) || Streak->ConsecutiveSatisfiedTicks < 0) return false;
			Seen.Add(Streak->VictoryId); P.ConsecutiveSatisfiedTicks = Streak->ConsecutiveSatisfiedTicks;
			P.bVictorious = S.Outcome == EHansaScenarioOutcome::Victory && P.VictoryId == S.WinningVictoryId;
			if (P.bVictorious && P.ConsecutiveSatisfiedTicks < P.RequiredSustainTicks) return false;
			bWinnerFound |= P.bVictorious;
		}
		if ((S.Outcome == EHansaScenarioOutcome::Victory) != bWinnerFound ||
			(S.Outcome != EHansaScenarioOutcome::Victory && !S.WinningVictoryId.IsEmpty()) ||
			(S.Outcome == EHansaScenarioOutcome::Failure && S.ConsecutiveFailureTicks < E.Progress.RequiredFailureTicks)) return false;
		E.Progress.Outcome = S.Outcome; E.Progress.WinningVictoryId = S.WinningVictoryId;
		E.Progress.ConsecutiveFailureTicks = S.ConsecutiveFailureTicks; E.Progress.LastEvaluatedTick = S.LastEvaluatedTick;
		E.Progress.FailureReason = S.Outcome == EHansaScenarioOutcome::Failure ? TEXT("Sustained insolvency with no active route or production.") : FString();
		OutEvaluator = MoveTemp(E); return true;
	}
}
