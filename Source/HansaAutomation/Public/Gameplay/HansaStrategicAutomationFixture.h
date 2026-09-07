#pragma once

#include "Dom/JsonObject.h"
#include "UObject/StrongObjectPtr.h"

class UHansaRuntimeSimulationHost;

namespace Hansa::Automation
{
	/** Development-only adapter over the real playable runtime used by the S10-P04 strategic flow. */
	class HANSAAUTOMATION_API FHansaStrategicAutomationFixture final
	{
	public:
		static constexpr const TCHAR* SeedAlphaFixtureId = TEXT("strategic_vertical_slice_seed_alpha_v1");
		static constexpr const TCHAR* SeedBetaFixtureId = TEXT("strategic_vertical_slice_seed_beta_v1");
		static constexpr const TCHAR* SaveRoundTripFixtureId = TEXT("save_roundtrip_v1");
		static constexpr const TCHAR* GoldenFixtureId = TEXT("lubeck_grain_shortage_v1");
		static constexpr uint32 FixtureVersion = 1;
		static constexpr uint32 GoldenFixtureVersion = 4;
		static constexpr uint64 SeedAlpha = 0x4C554245434B4752ULL;
		static constexpr uint64 SeedBeta = 0x4C554245434B4753ULL;

		[[nodiscard]] static bool IsStrategicFixtureId(const FString& FixtureId);
		void AppendFixtureDescriptors(TArray<TSharedPtr<FJsonValue>>& Fixtures) const;
		[[nodiscard]] bool Load(const FString& FixtureId, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool Query(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError) const;
		[[nodiscard]] bool Command(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool AssertPredicate(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError) const;
		[[nodiscard]] bool Step(int32 TickCount, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool RunUntil(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool SaveSlot(const FString& SlotId, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool LoadSlot(const FString& SlotId, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool ListSaveSlots(TSharedRef<FJsonObject>& OutPayload, FString& OutError) const;
		[[nodiscard]] bool AssertRoundTrip(TSharedRef<FJsonObject>& OutPayload, FString& OutError) const;

		[[nodiscard]] bool IsLoaded() const { return Host.IsValid(); }
		[[nodiscard]] const FString& GetFixtureId() const { return LoadedFixtureId; }
		[[nodiscard]] UHansaRuntimeSimulationHost* GetHost() const { return Host.Get(); }
		[[nodiscard]] TSharedRef<FJsonObject> MakeSummary(int32 TicksAdvanced = 0) const;
		[[nodiscard]] TSharedRef<FJsonObject> MakeEvidenceSnapshot() const;
		[[nodiscard]] TSharedRef<FJsonObject> MakeLogSnapshot(int32 MaximumEntries = 256) const;
		[[nodiscard]] bool HasVerifiedRoundTrip() const { return bRoundTripEquivalent && bDeterministicContinuation; }
		[[nodiscard]] uint32 GetFixtureVersion() const { return LoadedFixtureId == GoldenFixtureId ? GoldenFixtureVersion : FixtureVersion; }

	private:
		[[nodiscard]] bool MatchesPredicate(const TSharedRef<FJsonObject>& Predicate, FString& OutError) const;
		[[nodiscard]] bool PrepareSaveRoundTripState(FString& OutError);
		[[nodiscard]] FString ProjectionDigest() const;

		TStrongObjectPtr<UHansaRuntimeSimulationHost> Host;
		FString LoadedFixtureId;
		FString InitialStateHash;
		int32 InitialBuildingCount = 0;
		TMap<FString, TArray<uint8>> SaveSlots;
		TMap<FString, FString> SaveSlotUtc;
		FString SavedAuthoritativeHash;
		FString SavedProjectionDigest;
		bool bRoundTripEquivalent = false;
		bool bDeterministicContinuation = false;
		bool bSavedConstruction = false, bSavedInventories = false, bSavedPrices = false, bSavedRouteCargo = false;
		bool bSavedResearch = false, bSavedAi = false, bSavedObjectives = false;
	};
}
