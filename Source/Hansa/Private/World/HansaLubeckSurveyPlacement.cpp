#include "World/HansaLubeckPlacementGrid.h"
#include "Engine/World.h"

namespace Hansa::Game::LubeckPlacementGrid
{
	namespace
	{
		#include "HansaLubeckSurveyPlacement.generated.inl"
        #include "HansaLubeckTrees.generated.inl"
	}

	bool IsSurveyWorld(const UWorld* World)
	{
		return World && World->GetPackage()->GetName().Contains(TEXT("Lubeck_Terrain_Preview"));
	}

	bool IsCampaignWorld(const UWorld* World)
	{
		return World && World->GetPackage()->GetName().Contains(TEXT("L_HansaWorld_WP"));
	}

	FVector CampaignLubeckHistoricalCenter()
	{
		return FVector(3790434.456865096,4454879.4608833855,300.0);
	}

	FVector CampaignLubeckCenter()
	{
		// Dry source cell (2047, 2372), close to the Baltic shoreline.
		return FVector(3807663.6904761903, 4412202.380952381, 0.0);
	}

	FVector CampaignLubeckWaterAnchor()
	{
		// Sea-level water cell (2049, 2369), with a full 3x3 water neighbourhood.
		return FVector(3811383.928571428, 4406622.023809523, 0.0);
	}

	FVector CampaignLubeckNavigationOrigin()
	{
		return CampaignLubeckWaterAnchor() - GridToWorld({36, 22}, 0.0);
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
        for (const auto Tree : SurveyTreeCells)
        {
            const auto& Cell = Map.Cells[(Tree.X-SurveyMinX)*Height + Tree.Y-SurveyMinY];
            if (Cell.Terrain != EHansaPlacementTerrain::Water && !Cell.bBlocked)
                Map.TreeCells.Add(Tree);
        }
		return Result;
	}
}
