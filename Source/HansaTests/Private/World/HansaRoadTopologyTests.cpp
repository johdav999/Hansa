#include "World/HansaRoadTopology.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRoadTopologyTest, "Hansa.World.Road.Topology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaRoadTopologyTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Game::RoadTopology;
	for (uint8 Neighbors=0; Neighbors<16; ++Neighbors)
	{
		const auto Choice = Resolve(Neighbors);
		TestEqual(TEXT("Exact connected ports"), RotatePorts(CanonicalPorts(Choice.Tile),Choice.QuarterTurns),Neighbors);
		for (uint8 Turn=0; Turn<4; ++Turn)
		{
			const uint8 RotatedMask = RotatePorts(Neighbors,Turn);
			const auto Rotated = Resolve(RotatedMask);
			TestTrue(TEXT("Rotation preserves tile kind"), Rotated.Tile==Choice.Tile);
			TestEqual(TEXT("Rotation preserves exact ports"),RotatePorts(CanonicalPorts(Rotated.Tile),Rotated.QuarterTurns),RotatedMask);
		}
	}
	return true;
}
#endif
