#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/HansaLubeckWorldArt.h"
#include "World/HansaLubeckPlacementGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLubeckArtGrading,"Hansa.World.LubeckArt.GradingContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FLubeckArtGrading::RunTest(const FString&)
{
    using namespace Hansa::Game;
    for(int X=0;X<LubeckPlacementGrid::WidthCells;++X)for(int Y=0;Y<LubeckPlacementGrid::HeightCells;++Y)
    {
        const auto C=LubeckPlacementGrid::GridToWorld({X,Y});const FVector2D P(C.X,C.Y);
        const auto T=LubeckPlacementGrid::TerrainAt(P);const double H=LubeckWorldArt::GroundHeight(P);
        if(T!=Hansa::Simulation::EHansaPlacementTerrain::Water)
        {
            TestEqual(TEXT("Every legal cell retains its ground datum"),H,T==Hansa::Simulation::EHansaPlacementTerrain::Shore?85.:75.);
            TestFalse(TEXT("Ambient dressing cannot claim a buildable plot"),LubeckWorldArt::IsDressingLocation(P,100));
        }
        TestTrue(TEXT("Height remains finite within encoded Landscape range"),FMath::IsFinite(H)&&H>=-450&&H<=85);
    }
    TestEqual(TEXT("Far river bed remains below the water surface"),LubeckWorldArt::GroundHeight(FVector2D(20000,20000)),-450.);
    return !HasAnyErrors();
}
#endif
