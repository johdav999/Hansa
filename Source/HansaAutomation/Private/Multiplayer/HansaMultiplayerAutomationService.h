#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;

namespace Hansa::Automation
{
	/**
	 * Process-local adapter for the S11-P04 launched multiplayer proof.
	 * It owns no gameplay state and only observes or submits through replicated gameplay classes.
	 */
	class FHansaMultiplayerAutomationService final
	{
	public:
		static constexpr const TCHAR* FixtureId = TEXT("two_player_authority_v1");
		static constexpr int32 FixtureVersion = 1;
		static constexpr uint64 FixtureSeed = 0x533131503034ULL;

		void AppendFixtureDescriptor(TArray<TSharedPtr<FJsonValue>>& Fixtures) const;
		bool ActivateFixture(TSharedRef<FJsonObject> OutResult, FString& OutError);
		void DeactivateFixture() { bFixtureActive = false; }
		[[nodiscard]] bool IsFixtureActive() const { return bFixtureActive; }

		bool Query(const FString& CorrelationId, TSharedRef<FJsonObject> Payload,
			TSharedRef<FJsonObject> OutResult, FString& OutError) const;
		bool Command(const FString& CorrelationId, TSharedRef<FJsonObject> Payload,
			TSharedRef<FJsonObject> OutResult, FString& OutError) const;
		bool Step(const FString& CorrelationId, int32 TickCount,
			TSharedRef<FJsonObject> OutResult, FString& OutError) const;
		TSharedRef<FJsonObject> MakeEvidenceSnapshot(const FString& CorrelationId,
			FString& OutError) const;

	private:
		bool BuildStatus(const FString& CorrelationId, TSharedRef<FJsonObject> OutResult,
			FString& OutError) const;

		bool bFixtureActive = false;
	};
}