#include "World/HansaRoadTopology.h"

namespace Road = Hansa::Game::RoadTopology;
constexpr bool VerifyRoadTopology()
{
	unsigned int Counts[6] = {};
	for (unsigned char Mask=0; Mask<16; ++Mask)
	{
		const auto Choice = Road::Resolve(Mask);
		if (Choice.QuarterTurns>3 || Road::RotatePorts(Road::CanonicalPorts(Choice.Tile), Choice.QuarterTurns)!=Mask) return false;
		++Counts[static_cast<unsigned char>(Choice.Tile)];
		for (unsigned char Turn=0; Turn<4; ++Turn)
		{
			const auto RotatedMask = Road::RotatePorts(Mask,Turn);
			const auto Rotated = Road::Resolve(RotatedMask);
			if (Rotated.Tile!=Choice.Tile || Road::RotatePorts(Road::CanonicalPorts(Rotated.Tile),Rotated.QuarterTurns)!=RotatedMask) return false;
		}
	}
	return Counts[0]==1 && Counts[1]==4 && Counts[2]==2 && Counts[3]==4 && Counts[4]==4 && Counts[5]==1;
}
static_assert(VerifyRoadTopology(), "All sixteen neighbor masks and 64 rotations must preserve exact ports");
static_assert(Road::Mask(true,false,true,false)==5);
static_assert(Road::Resolve(0).Tile==Road::ETile::Isolated);
static_assert(Road::Resolve(15).Tile==Road::ETile::Crossroads);
static_assert(Road::Resolve(255).Tile==Road::ETile::Crossroads);
