#include "World/HansaLubeckPlacementGrid.h"

#include "Definitions/HansaEconomicRegistry.h"

namespace Hansa::Game::LubeckPlacementGrid
{
	namespace
	{
		const FVector2D WorldOrigin(-12000.0, -8000.0);

		bool InsideRectangle(
			const FVector2D Point,
			const FVector2D Center,
			const FVector2D HalfExtent)
		{
			return FMath::Abs(Point.X - Center.X) <= HalfExtent.X &&
				FMath::Abs(Point.Y - Center.Y) <= HalfExtent.Y;
		}

		Hansa::Simulation::EHansaPlacementTerrain TerrainAt(const FVector2D Point)
		{
			using namespace Hansa::Simulation;
			const bool bShore =
				InsideRectangle(Point, FVector2D(-50.0, 3100.0), FVector2D(450.0, 3500.0)) ||
				InsideRectangle(Point, FVector2D(-180.0, -150.0), FVector2D(450.0, 3400.0)) ||
				InsideRectangle(Point, FVector2D(-420.0, -3550.0), FVector2D(450.0, 3400.0));
			if (bShore)
			{
				return EHansaPlacementTerrain::Shore;
			}
			const bool bLand =
				InsideRectangle(Point, FVector2D(-4200.0, 500.0), FVector2D(3900.0, 5250.0)) ||
				InsideRectangle(Point, FVector2D(-900.0, 4300.0), FVector2D(3100.0, 1750.0)) ||
				InsideRectangle(Point, FVector2D(-1700.0, -4300.0), FVector2D(2750.0, 1700.0));
			return bLand ? EHansaPlacementTerrain::Land : EHansaPlacementTerrain::Water;
		}
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
