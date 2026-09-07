#include "World/HansaLubeckPlacementGrid.h"

#include "Definitions/HansaEconomicRegistry.h"

namespace Hansa::Game::LubeckPlacementGrid
{
	namespace
	{
		const FVector2D WorldOrigin(-12000.0, -8000.0);

		bool InsideSurface(
			const FVector2D Point,
			const FHansaSurfaceBox& Surface)
		{
			const double Radians = FMath::DegreesToRadians(-Surface.Rotation.Yaw);
			const double Cosine = FMath::Cos(Radians);
			const double Sine = FMath::Sin(Radians);
			const double DeltaX = Point.X - Surface.Location.X;
			const double DeltaY = Point.Y - Surface.Location.Y;
			const double LocalX = DeltaX * Cosine - DeltaY * Sine;
			const double LocalY = DeltaX * Sine + DeltaY * Cosine;
			// /Engine/BasicShapes/Cube is 100 cm wide, so its half extent is Scale * 50.
			return FMath::Abs(LocalX) <= Surface.Scale.X * 50.0 &&
				FMath::Abs(LocalY) <= Surface.Scale.Y * 50.0;
		}
	}

	TConstArrayView<FHansaSurfaceBox> GetLandSurfaces()
	{
		static const TArray<FHansaSurfaceBox> Surfaces {
			{ TEXT("LandCore"), FVector(-4200.0, 500.0, -25.0), FVector(78.0, 105.0, 2.0), FRotator::ZeroRotator },
			{ TEXT("LandNorth"), FVector(-900.0, 4300.0, -25.0), FVector(62.0, 35.0, 2.0), FRotator(0.0, -8.0, 0.0) },
			{ TEXT("LandSouth"), FVector(-1700.0, -4300.0, -25.0), FVector(55.0, 34.0, 2.0), FRotator(0.0, 12.0, 0.0) }
		};
		return Surfaces;
	}

	TConstArrayView<FHansaSurfaceBox> GetShoreSurfaces()
	{
		static const TArray<FHansaSurfaceBox> Surfaces {
			{ TEXT("ShoreNorth"), FVector(-50.0, 3100.0, 80.0), FVector(9.0, 35.0, 0.1), FRotator(0.0, -8.0, 0.0) },
			{ TEXT("ShoreCentral"), FVector(-180.0, -150.0, 80.0), FVector(9.0, 34.0, 0.1), FRotator::ZeroRotator },
			{ TEXT("ShoreSouth"), FVector(-420.0, -3550.0, 80.0), FVector(9.0, 34.0, 0.1), FRotator(0.0, 12.0, 0.0) }
		};
		return Surfaces;
	}

	Hansa::Simulation::EHansaPlacementTerrain TerrainAt(const FVector2D& LocalWorldLocation)
	{
		using namespace Hansa::Simulation;
		if (GetShoreSurfaces().ContainsByPredicate([&LocalWorldLocation](const FHansaSurfaceBox& Surface)
		{
			return InsideSurface(LocalWorldLocation, Surface);
		}))
		{
			return EHansaPlacementTerrain::Shore;
		}
		return GetLandSurfaces().ContainsByPredicate([&LocalWorldLocation](const FHansaSurfaceBox& Surface)
		{
			return InsideSurface(LocalWorldLocation, Surface);
		}) ? EHansaPlacementTerrain::Land : EHansaPlacementTerrain::Water;
	}

	Hansa::Simulation::FHansaGridCoordinate WorldToGrid(const FVector& LocalWorldLocation)
	{
		return {
			FMath::FloorToInt32((LocalWorldLocation.X - WorldOrigin.X) / CellSize),
			FMath::FloorToInt32((LocalWorldLocation.Y - WorldOrigin.Y) / CellSize)
		};
	}

	FVector GridToWorld(const Hansa::Simulation::FHansaGridCoordinate Coordinate, const double Height)
	{
		return FVector(
			WorldOrigin.X + (static_cast<double>(Coordinate.X) + 0.5) * CellSize,
			WorldOrigin.Y + (static_cast<double>(Coordinate.Y) + 0.5) * CellSize,
			Height);
	}

	Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaPlacementInitialization>
	TryBuildInitialization(
		const Hansa::Simulation::FHansaHouseId OwnerId,
		const Hansa::Simulation::FHansaEconomicRegistry& Definitions)
	{
		using namespace Hansa::Simulation;
		if (!OwnerId.IsValid() || Definitions.FindBuilding(TEXT("Building.Road")) == nullptr)
		{
			return THansaValueResult<FHansaPlacementInitialization>::Failure(EHansaValueError::InvalidFormat);
		}

		FHansaPlacementMapInitialization Map;
		const THansaValueResult<FHansaCityDefinitionId> CityId =
			FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
		const THansaValueResult<FHansaBuildingTypeId> RoadId =
			FHansaBuildingTypeId::TryParse(TEXT("Building.Road"));
		if (!CityId || !RoadId)
		{
			return THansaValueResult<FHansaPlacementInitialization>::Failure(EHansaValueError::InvalidFormat);
		}
		Map.CityId = CityId.Value;
		Map.BoundsMin = { 0, 0 };
		Map.BoundsMax = { WidthCells - 1, HeightCells - 1 };
		Map.RoadBuildingDefinitionId = RoadId.Value;
		Map.Cells.Reserve(WidthCells * HeightCells);
		for (int32 X = 0; X < WidthCells; ++X)
		{
			for (int32 Y = 0; Y < HeightCells; ++Y)
			{
				FHansaPlacementGridCell Cell;
				Cell.Coordinate = { X, Y };
				const FVector Center = GridToWorld(Cell.Coordinate);
				Cell.Terrain = TerrainAt(FVector2D(Center.X, Center.Y));
				Cell.OwnerId = OwnerId;
				Map.Cells.Add(Cell);
			}
		}

		FHansaPlacementInitialization Result;
		Result.Maps.Add(MoveTemp(Map));
		for (const FHansaCompiledBuildingDefinition& Building : Definitions.GetBuildings())
		{
			const THansaValueResult<FHansaBuildingTypeId> BuildingId =
				FHansaBuildingTypeId::TryParse(Building.StableId);
			if (!BuildingId)
			{
				return THansaValueResult<FHansaPlacementInitialization>::Failure(BuildingId.Error);
			}
			Result.Entitlements.Add({ OwnerId, BuildingId.Value });
		}
		return THansaValueResult<FHansaPlacementInitialization>::Success(MoveTemp(Result));
	}
}
