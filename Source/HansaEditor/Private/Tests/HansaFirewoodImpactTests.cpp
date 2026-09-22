#include "Misc/AutomationTest.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicImpact.h"
#include "Algo/Reverse.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFirewoodImpactTest,"Hansa.Integration.Firewood.AuthoringImpact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFirewoodImpactTest::RunTest(const FString&)
{
 using namespace Hansa::Editor::EconomicDefinitions;
 const auto Definitions=CreateMvpDefinitionSet(GetTransientPackage());
 TArray<const UHansaDefinitionBase*> Raw;for(const auto& D:Definitions)Raw.Add(D.Get());
 const auto Fuel=DescribeEconomicImpact(TEXT("Good.Firewood"),Raw);
 for(const TCHAR* Id:{TEXT("Recipe.BakeBread.Inputs"),TEXT("Recipe.MaltGrain.Inputs"),TEXT("Recipe.BrewBeer.Inputs"),TEXT("Recipe.SplitFirewood.Outputs"),TEXT("Need.Heating.GoodId")})
  TestTrue(Id,Fuel.Contains(Id));
 const auto Timber=DescribeEconomicImpact(TEXT("Good.Timber"),Raw);
 for(const TCHAR* Id:{TEXT("Recipe.SplitFirewood.Inputs"),TEXT("Recipe.SawPlanks.Inputs"),TEXT("Recipe.MakeBarrels.Inputs")})TestTrue(Id,Timber.Contains(Id));
 const auto Heating=DescribeEconomicImpact(TEXT("Need.Heating"),Raw);
 TestTrue(TEXT("Heating impact includes laborers"),Heating.Contains(TEXT("PopulationTier.Laborer.Needs")));
 TestTrue(TEXT("Heating impact includes artisans"),Heating.Contains(TEXT("PopulationTier.Artisan.Needs")));
 Algo::Reverse(Raw);TestTrue(TEXT("Fuel impact discovery order is deterministic"),Fuel==DescribeEconomicImpact(TEXT("Good.Firewood"),Raw));
 return !HasAnyErrors();
}
#endif
