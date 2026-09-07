#pragma once

#include "CoreMinimal.h"
#include "Placement/HansaPlacement.h"

namespace Hansa::Game::LubeckPlacementGrid
{
	inline constexpr int32 WidthCells = 60;
	inline constexpr int32 HeightCells = 40;
	inline constexpr double CellSize = 400.0;

	/** Shared source geometry for the placeholder world and its authoritative terrain sampler. */
	struct FHansaSurfaceBox final
	{
		FName Name;
		FVector Location;
		FVector Scale;
		FRotator Rotation;
	};

	HANSA_API Hansa::Simulation::FHansaGridCoordinate WorldToGrid(const FVector& LocalWorldLocation);
	HANSA_API FVector GridToWorld(Hansa::Simulation::FHansaGridCoordinate Coordinate, double Height = 100.0);
	HANSA_API TConstArrayView<FHansaSurfaceBox> GetLandSurfaces();
	HANSA_API TConstArrayView<FHansaSurfaceBox> GetShoreSurfaces();
	HANSA_API Hansa::Simulation::EHansaPlacementTerrain TerrainAt(const FVector2D& LocalWorldLocation);
	HANSA_API Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaPlacementInitialization>
		TryBuildInitialization(
			Hansa::Simulation::FHansaHouseId OwnerId,
			const Hansa::Simulation::FHansaEconomicRegistry& Definitions);
}
