#pragma once

#include "Dom/JsonObject.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Misc/Optional.h"

namespace Hansa::Automation
{
	/** Allowlisted JSON adapter over the actor-free production fixture. */
	class FHansaProductionFixtureService final
	{
	public:
		[[nodiscard]] TSharedRef<FJsonObject> ListFixtures() const;
		[[nodiscard]] bool Load(const FString& FixtureId, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool Query(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError) const;
		[[nodiscard]] bool Command(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool AssertPredicate(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError) const;
		[[nodiscard]] bool Step(int32 TickCount, TSharedRef<FJsonObject>& OutPayload, FString& OutError);
		[[nodiscard]] bool RunUntil(const TSharedRef<FJsonObject>& Request, TSharedRef<FJsonObject>& OutPayload, FString& OutError);

	private:
		[[nodiscard]] bool MatchesPredicate(const TSharedRef<FJsonObject>& Predicate, FString& OutError) const;
		[[nodiscard]] TSharedRef<FJsonObject> MakeSummary(int32 TicksAdvanced = 0) const;
		[[nodiscard]] static TSharedRef<FJsonObject> MakeProduction(const Hansa::Simulation::FHansaProductionProjection& Production);
		[[nodiscard]] static TSharedRef<FJsonObject> MakeMarket(const Hansa::Simulation::FHansaCityMarketProjection& Market);

		TOptional<Hansa::Simulation::FHansaProductionFixture> Fixture;
	};
}
