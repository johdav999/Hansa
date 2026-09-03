#pragma once

#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Events/HansaDomainEvent.h"
#include "Model/HansaSimulationState.h"
#include "Systems/HansaSimulationPipeline.h"

#if WITH_HANSA_AUTOMATION
namespace Hansa::Simulation
{
	/** Named, actor-free MVP production fixture used by automation, commandlets and golden tests. */
	class HANSASIMULATION_API FHansaProductionFixture final
	{
	public:
		static constexpr const TCHAR* StableFixtureId = TEXT("mvp_production_chains_v1");
		static constexpr const TCHAR* GrainShortageFixtureId = TEXT("lubeck_grain_shortage_v1");
		static constexpr uint32 FixtureVersion = 3;
		static constexpr uint64 RegistryHash = 0xB0481C9F740D6C18ULL;

		[[nodiscard]] static THansaValueResult<FHansaProductionFixture> TryCreate();
		[[nodiscard]] static THansaValueResult<FHansaProductionFixture> TryCreateGrainShortage();

		[[nodiscard]] const FString& GetFixtureId() const { return FixtureId; }
		[[nodiscard]] uint32 GetFixtureVersion() const { return FixtureVersion; }
		[[nodiscard]] uint64 GetRegistryHash() const { return Definitions.GetDefinitionHash(); }
		[[nodiscard]] const FHansaSimulationDefinitionContext& GetDefinitions() const { return Definitions; }
		[[nodiscard]] const FHansaSimulationState& GetState() const { return State; }
		[[nodiscard]] TConstArrayView<FHansaDomainEvent> GetEvents() const { return Events; }
		[[nodiscard]] FHansaStateHashReport BuildStateHashes() const;
		[[nodiscard]] THansaValueResult<FHansaSimulationProjection> BuildProjection() const;

		/** Advances through the sole gameplay command gateway. The bound prevents automation monopolizing a frame. */
		[[nodiscard]] FHansaCommandGatewayResult Step(int32 TickCount = 1);
		/** Issues a normal authoritative command; it never writes stock or price directly. */
		[[nodiscard]] FHansaCommandGatewayResult SetProductionActive(FHansaProductionId ProductionId, bool bActive);
		/** Exercises the same explicit residence-progression command available to every command origin. */
		[[nodiscard]] FHansaCommandGatewayResult UpgradeResidence(FHansaBuildingId BuildingId);

	private:
		FString FixtureId = StableFixtureId;
		FHansaSimulationDefinitionContext Definitions;
		FHansaSimulationState State;
		FHansaSimulationTransientCache Cache;
		TArray<FHansaDomainEvent> Events;
	};

	/** Stable machine-readable evidence for a named production-fixture execution. */
	class HANSASIMULATION_API FHansaProductionEvidenceWriter final
	{
	public:
		static constexpr uint32 EvidenceSchemaVersion = 1;

		[[nodiscard]] static FString WriteJson(
			const FHansaProductionFixture& Fixture,
			const FHansaStateHashReport& InitialState,
			int32 TicksRun);
	};
}
#endif
