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
	/** Catalog 18 assigns visible approved presentation meshes to the malt house and cooperage. */
	static constexpr int32 MvpCatalogVersion = 18;
	/** P33 review candidate; cannot be selected in Shipping. */
	static constexpr uint64 P33CandidateRegistryHash = 0x92658D0E14439F91ULL;
	static constexpr uint64 MvpRegistryHash = 0x1C2B54191C78E4CAULL;
	static constexpr int32 ImmediatePreviousMvpCatalogVersion = 17;
	static constexpr uint64 ImmediatePreviousMvpRegistryHash = 0x968431FAD59A2C51ULL;
	static constexpr int32 PreviousStarterBalanceMvpCatalogVersion = 15;
	static constexpr uint64 PreviousStarterBalanceMvpRegistryHash = 0xF1A0A054CB2DFCD9ULL;
	static constexpr int32 PreviousFisheryPresentationMvpCatalogVersion = 14;
	static constexpr uint64 PreviousFisheryPresentationMvpRegistryHash = 0x73EC37D013D49BA0ULL;
	static constexpr int32 PreviousPresentationMvpCatalogVersion = 13;
	static constexpr uint64 PreviousPresentationMvpRegistryHash = 0x8586FD71211B707CULL;
	static constexpr int32 PreviousMvpCatalogVersion = 9;
	static constexpr uint64 PreviousMvpRegistryHash = 0xF1D0D88180A4C342ULL;
	static constexpr int32 LegacyMvpCatalogVersion = 8;
	static constexpr uint64 LegacyMvpRegistryHash = 0x6FAA28CD24E2C69EULL;
	static constexpr int32 OlderMvpCatalogVersion = 7;
	static constexpr uint64 OlderMvpRegistryHash = 0x22248A11101B32B0ULL;

	[[nodiscard]] static bool TryLoadMvpRegistry(
		Hansa::Simulation::FHansaEconomicRegistry& OutRegistry,
		FString& OutError);

	[[nodiscard]] static bool TryCreate(
		EHansaRuntimeScenario Scenario,
		Hansa::Simulation::FHansaEconomicRegistry Registry,
		Hansa::Simulation::FHansaPlacementInitialization Placement,
		FHansaLubeckScenarioState& OutState,
		FString& OutError,
		uint64 CampaignSeedOverride = 0,
        bool bEmptyPlayerCity = false);
};
