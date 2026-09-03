#pragma once

#include "CoreMinimal.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Game::LubeckPlacementGrid
{
	inline constexpr int32 WidthCells = 60;
	inline constexpr int32 HeightCells = 40;
	inline constexpr double CellSize = 400.0;

	HANSA_API Hansa::Simulation::FHansaGridCoordinate WorldToGrid(const FVector& LocalWorldLocation);
	HANSA_API FVector GridToWorld(Hansa::Simulation::FHansaGridCoordinate Coordinate, double Height = 100.0);
	HANSA_API Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaPlacementInitialization>
		TryBuildInitialization(
			Hansa::Simulation::FHansaHouseId OwnerId,
			const Hansa::Simulation::FHansaEconomicRegistry& Definitions);
}
