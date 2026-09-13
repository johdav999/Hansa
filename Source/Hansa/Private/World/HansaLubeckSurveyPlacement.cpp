#include "World/HansaLubeckPlacementGrid.h"
#include "Engine/World.h"

namespace Hansa::Game::LubeckPlacementGrid
{
	namespace
	{
		#include "HansaLubeckSurveyPlacement.generated.inl"
	}

	bool IsSurveyWorld(const UWorld* World)
	{
		return World && World->GetPackage()->GetName().Contains(TEXT("Lubeck_Terrain_Preview"));
	}

	Hansa::Simulation::FHansaGridCoordinate SurveyFisheryAnchor()
	{
		return {SurveyFisheryX, SurveyFisheryY};
	}

	FVector SurveyStartLocation()
	{
		return GridToWorld(SurveyFisheryAnchor(), 150.0) - FVector(2000.0, 0.0, 0.0);
	}

	Hansa::Simulation::THansaValueResult<Hansa::Simulation::FHansaPlacementInitialization>
	TryBuildSurveyInitialization(const Hansa::Simulation::FHansaHouseId OwnerId,
		const Hansa::Simulation::FHansaEconomicRegistry& Definitions)
	{
		using namespace Hansa::Simulation;
		auto Result = TryBuildInitialization(OwnerId, Definitions);
		if (!Result) return Result;
		auto& Map = Result.Value.Maps[0];
		Map.BoundsMin = {SurveyMinX, SurveyMinY};
		Map.BoundsMax = {SurveyMaxX, SurveyMaxY};
		constexpr int32 Height = SurveyMaxY - SurveyMinY + 1;
		Map.Cells.Reset((SurveyMaxX-SurveyMinX+1)*Height);
		for (int32 X=SurveyMinX; X<=SurveyMaxX; ++X)
			for (int32 Y=SurveyMinY; Y<=SurveyMaxY; ++Y)
				Map.Cells.Add({{X,Y}, EHansaPlacementTerrain::Land, OwnerId, false});
		for (const auto& Run : SurveyTerrainRuns)
			for (int32 Y=Run.FirstY; Y<=Run.LastY; ++Y)
				Map.Cells[(Run.X-SurveyMinX)*Height + Y-SurveyMinY].Terrain =
					static_cast<EHansaPlacementTerrain>(Run.Terrain);
		return Result;
	}
}
