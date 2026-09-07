#pragma once

#include "CoreMinimal.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Model/HansaSimulationState.h"
#include "Placement/HansaPlacement.h"

enum class EHansaRuntimeScenario : uint8
{
	LubeckGrainShortage,
	EmptyLubeckBuild
};

/** Runtime-safe output shared by playable Development and Shipping initialization. */
struct HANSA_API FHansaLubeckScenarioState final
{
	Hansa::Simulation::FHansaSimulationDefinitionContext Definitions;
	Hansa::Simulation::FHansaSimulationState State;
	Hansa::Simulation::FHansaHouseId HouseId;
	Hansa::Simulation::FHansaHouseId RivalHouseId;
	Hansa::Simulation::FHansaCityDefinitionId CityId;
	uint64 NextBuildingId = 1;
};

/**
 * Loads the cooked authored MVP definitions and creates a deterministic playable Lübeck state.
 * This code has no dependency on HansaAutomation or its fixture-only types.
 */
class HANSA_API FHansaLubeckScenarioInitializer final
{
public:
	static constexpr const TCHAR* GrainShortageId = TEXT("lubeck_grain_shortage_v1");
	static constexpr const TCHAR* EmptyBuildId = TEXT("empty_lubeck_build_v1");
	/** Reviewed hash of the cooked runtime catalog; deliberately independent of automation fixture hashes. */
	static constexpr uint64 MvpRegistryHash = 0x724BD5DE8DB9C292ULL;

	[[nodiscard]] static bool TryLoadMvpRegistry(
		Hansa::Simulation::FHansaEconomicRegistry& OutRegistry,
		FString& OutError);

	[[nodiscard]] static bool TryCreate(
		EHansaRuntimeScenario Scenario,
		Hansa::Simulation::FHansaEconomicRegistry Registry,
		Hansa::Simulation::FHansaPlacementInitialization Placement,
		FHansaLubeckScenarioState& OutState,
		FString& OutError,
		uint64 CampaignSeedOverride = 0);
};
