#include "Misc/AutomationTest.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Tests::Preservation
{
using namespace Hansa::Simulation;
template<typename T> T Id(uint64 N){return T::TryCreate(N).Value;}
FHansaGoodId Good(const TCHAR* N){return FHansaGoodId::TryParse(N).Value;}
FHansaRecipeId Recipe(const TCHAR* N){return FHansaRecipeId::TryParse(N).Value;}
FHansaSimulationDefinitionContext Definitions(bool Spoilage=false)
{
 TArray<FHansaCompiledGoodDefinition> Goods;
 for(const auto N:{TEXT("Good.Fish"),TEXT("Good.PreservedFish"),TEXT("Good.Salt"),TEXT("Good.Barrels")})
 {FHansaCompiledGoodDefinition G;G.StableId=N;G.bSpoilageEnabled=Spoilage&&(G.StableId==TEXT("Good.Fish")||G.StableId==TEXT("Good.PreservedFish"));G.SpoilageBasisPointsPerDay=G.StableId==TEXT("Good.Fish")?500:10;Goods.Add(G);}
 FHansaCompiledRecipeDefinition Fresh;Fresh.StableId=TEXT("Recipe.CatchFish");Fresh.Outputs={{TEXT("Good.Fish"),20000}};Fresh.CycleTicks=3;Fresh.LaborerWorkforce=2;Fresh.bDeclaredSource=true;
 FHansaCompiledRecipeDefinition Salted;Salted.StableId=TEXT("Recipe.SaltedCatch");Salted.Inputs={{TEXT("Good.Salt"),4000},{TEXT("Good.Barrels"),200}};Salted.Outputs={{TEXT("Good.PreservedFish"),20000}};Salted.CycleTicks=4;Salted.LaborerWorkforce=4;Salted.InternalCatchRecipeId=Fresh.StableId;
 FHansaCompiledBuildingDefinition Fishery;Fishery.StableId=TEXT("Building.Fishery.SaltingShed");Fishery.RecipeIds={Fresh.StableId,Salted.StableId};Fishery.LaborerWorkforce=2;Fishery.FootprintWidthCells=1;Fishery.FootprintHeightCells=1;Fishery.BuildTicks=2;Fishery.ConstructionCostPfennig=100;Fishery.ConstructionCosts={{TEXT("Good.Salt"),1000}};
 FHansaCompiledBuildingDefinition Basic=Fishery;Basic.StableId=TEXT("Building.Fishery");Basic.RecipeIds={Fresh.StableId};Basic.UpgradeTargetBuildingId=Fishery.StableId;
 return FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.PreservationTest")).Value,0xF152,
  FHansaEconomicRegistry(Goods,{Fresh,Salted},{Basic,Fishery},0xF152)).Value;
}
FHansaSimulationState State(const FHansaSimulationDefinitionContext& D,int Stores=1,bool Production=true,bool Inputs=true,int64 Fish=0,bool Basic=false,bool Cargo=false)
{
 FHansaSimulationInitialization I;I.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value,10).Value;
 I.Houses.Add({Id<FHansaHouseId>(1),FHansaMoney::FromRaw(100000)});
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;I.Cities.Add({City,{}});
 for(int N=1;N<=Stores;++N)
 {
  FHansaInventoryInitialization V;V.Id=Id<FHansaInventoryId>(N);V.OwnerKind=EHansaInventoryOwnerKind::City;V.CityId=City;V.Capacity=FHansaQuantity::FromRaw(100000000);
  if(N>1){V.CityId=FHansaCityDefinitionId::TryParse(FString::Printf(TEXT("City.Test%d"),N)).Value;I.Cities.Add({V.CityId,{}});}
  for(const auto& G:D.GetEconomicRegistry()->GetGoods())V.AcceptedGoods.Add(Good(*G.StableId));
  V.InitialStock={{Good(TEXT("Good.Fish")),FHansaQuantity::FromRaw(Fish/Stores+(N==1?Fish%Stores:0))},
    {Good(TEXT("Good.PreservedFish")),FHansaQuantity::FromRaw(Fish/Stores+(N==1?Fish%Stores:0))},
    {Good(TEXT("Good.Salt")),FHansaQuantity::FromRaw(Inputs?8000:0)},{Good(TEXT("Good.Barrels")),FHansaQuantity::FromRaw(Inputs?400:0)}};
  if(Cargo){
   FHansaVehicleState Ship;Ship.Id=Id<FHansaVehicleId>(N);Ship.DefinitionId=FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;Ship.OwnerId=Id<FHansaHouseId>(1);Ship.CargoInventoryId=V.Id;Ship.Capacity=V.Capacity;Ship.CurrentCityId=City;I.Vehicles.Add(Ship);
   V.OwnerKind=EHansaInventoryOwnerKind::Vehicle;V.CityId=FHansaCityDefinitionId();V.VehicleId=Ship.Id;
  }
  I.Inventories.Add(V);
 }
 if(Production)
 {
  FHansaBuildingState B;B.Id=Id<FHansaBuildingId>(1);B.OwnerId=Id<FHansaHouseId>(1);B.DefinitionId=FHansaBuildingTypeId::TryParse(Basic?TEXT("Building.Fishery"):TEXT("Building.Fishery.SaltingShed")).Value;B.ConstructionState=EHansaConstructionState::Completed;B.ConstructionProgress=FHansaRate::FromPartsPerMillion(FHansaRate::Scale);I.Buildings.Add(B);
  FHansaProductionInitialization P;P.Id=Id<FHansaProductionId>(1);P.BuildingId=B.Id;P.RecipeId=Recipe(TEXT("Recipe.CatchFish"));P.InputInventoryId=Id<FHansaInventoryId>(1);P.OutputInventoryId=P.InputInventoryId;P.AllocatedLaborerWorkforce=4;I.Productions.Add(P);
  if(Basic)
  {
   FHansaPlacementMapInitialization Map;Map.CityId=City;Map.BoundsMin={0,0};Map.BoundsMax={2,2};Map.RoadBuildingDefinitionId=B.DefinitionId;
   for(int X=0;X<3;++X)for(int Y=0;Y<3;++Y)Map.Cells.Add({{X,Y},EHansaPlacementTerrain::Land,B.OwnerId,false});
   I.Placement.Maps.Add(Map);I.Placement.Entitlements.Add({B.OwnerId,B.DefinitionId});
   FHansaPlacedBuildingRecord Placed;Placed.BuildingId=B.Id;Placed.OwnerId=B.OwnerId;Placed.Spec={City,B.DefinitionId,{1,1},EHansaGridRotation::North};Placed.OccupiedCells={{1,1}};I.Placement.Placements.Add(Placed);
  }

 }
 const auto Created = FHansaSimulationState::TryCreate(I);
 checkf(Created.IsSuccess(), TEXT("Preservation fixture must initialize"));
 return Created.Value;
}
bool Step(FHansaSimulationState& S,const FHansaSimulationDefinitionContext& D,FHansaSimulationTransientCache& C){return FHansaGameplayCommandGateway::ExecuteTick(S,D,{},C).IsSuccess();}
FHansaCommandHeader Header(const FHansaSimulationState& S,const FHansaSimulationDefinitionContext& D,uint64 N)
{FHansaCommandHeader H;H.CommandId=Id<FHansaCommandId>(N);H.GlobalSequence=N;H.Authority.IssuingHouseId=Id<FHansaHouseId>(1);H.Authority.PrincipalId=1;H.RequestedExecutionTick=S.CreateReadOnlyAccess(D).GetClock().GetTick();return H;}
int64 Stock(const FHansaSimulationState& S,const FHansaSimulationDefinitionContext& D,const TCHAR* N)
{int64 R=0;for(const auto& V:S.CreateReadOnlyAccess(D).GetInventories().BuildProjection())for(const auto& G:V.Stocks)if(G.GoodId==Good(N))R+=G.Stock.GetRawValue();return R;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFishModesTest,"Hansa.Simulation.PreservedFish.BatchModesAndSave",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFishModesTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Tests::Preservation;
 const auto D=Definitions();auto S=State(D);FHansaSimulationTransientCache Cache;
 TestTrue(TEXT("Start fresh batch"),Step(S,D,Cache));
 auto Command=FHansaGameplayCommand::Create(Header(S,D,1),FHansaSetProductionModeCommand{Id<FHansaProductionId>(1),Recipe(TEXT("Recipe.SaltedCatch")),false});
 TestTrue(TEXT("Queue salted"),FHansaGameplayCommandGateway::ExecuteTick(S,D,MakeArrayView(&Command,1),Cache).IsSuccess());
 TestEqual(TEXT("Current batch stays fresh"),S.CreateReadOnlyAccess(D).QueryProduction(Id<FHansaProductionId>(1))->RecipeId.ToString(),FString(TEXT("Recipe.CatchFish")));
 Step(S,D,Cache);Step(S,D,Cache);
 TestEqual(TEXT("Fresh batch delivered once"),Stock(S,D,TEXT("Good.Fish")),int64(20000));
 TestEqual(TEXT("Salted batch starts next"),S.CreateReadOnlyAccess(D).QueryProduction(Id<FHansaProductionId>(1))->RecipeId.ToString(),FString(TEXT("Recipe.SaltedCatch")));
 FHansaSaveSnapshot Save;Save.State=S;Save.BuildVersion=TEXT("PreservationTest");Save.SavedUtc=TEXT("2026-09-16T00:00:00Z");Save.DisplayName=TEXT("Salted batch");Save.Players.Add({1,Id<FHansaHouseId>(1)});
 TArray<uint8> Bytes;TestTrue(TEXT("Save reserved salted batch"),FHansaSaveEnvelope::Encode(Save,D,Bytes).IsSuccess());FHansaSaveSnapshot Loaded;
 TestTrue(TEXT("Restore reserved salted batch"),FHansaSaveEnvelope::Decode(Bytes,D,Loaded).IsSuccess());
 auto Restored=Loaded.State;FHansaSimulationTransientCache Other;
 for(int N=0;N<3;++N){Step(S,D,Cache);Step(Restored,D,Other);}
 TestEqual(TEXT("Salt consumed once"),Stock(S,D,TEXT("Good.Salt")),int64(4000));
 TestEqual(TEXT("Barrel fraction consumed once"),Stock(S,D,TEXT("Good.Barrels")),int64(200));
 TestEqual(TEXT("No extra edible fish"),Stock(S,D,TEXT("Good.PreservedFish")),int64(20000));
 const auto Totals=S.CreateReadOnlyAccess(D).QueryProduction(Id<FHansaProductionId>(1))->OutputTotals;
 TestEqual(TEXT("Separate output totals survive mode changes"),Totals.Num(),2);
 for(const auto& Total:Totals) TestEqual(TEXT("Each product records only its own batch"),Total.QuantityMilliUnits,int64(20000));
 TestEqual(TEXT("Identical resumed state"),S.CreateReadOnlyAccess(D).GetFingerprint().Value,Restored.CreateReadOnlyAccess(D).GetFingerprint().Value);
 auto Short=State(D,1,true,false);FHansaSimulationTransientCache ShortCache;
 Command=FHansaGameplayCommand::Create(Header(Short,D,1),FHansaSetProductionModeCommand{Id<FHansaProductionId>(1),Recipe(TEXT("Recipe.SaltedCatch")),true});
 TestTrue(TEXT("Enable fallback"),FHansaGameplayCommandGateway::ExecuteTick(Short,D,MakeArrayView(&Command,1),ShortCache).IsSuccess());
 auto P=Short.CreateReadOnlyAccess(D).QueryProduction(Id<FHansaProductionId>(1));
 TestEqual(TEXT("Fallback fresh"),P->RecipeId.ToString(),FString(TEXT("Recipe.CatchFish")));TestEqual(TEXT("Selected remains salted"),P->RequestedRecipeId.ToString(),FString(TEXT("Recipe.SaltedCatch")));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFishUpgradeTest,"Hansa.Simulation.PreservedFish.UpgradeIdentityAndCosts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFishUpgradeTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Tests::Preservation;
 const auto D=Definitions();auto S=State(D,1,true,true,0,true);FHansaSimulationTransientCache Cache;
 if(!TestTrue(TEXT("Start original catch"),Step(S,D,Cache)))return false;
 auto C=FHansaGameplayCommand::Create(Header(S,D,1),FHansaUpgradeProductionCommand{Id<FHansaProductionId>(1)});
 if(!TestTrue(TEXT("Queue paid upgrade"),FHansaGameplayCommandGateway::ExecuteTick(S,D,MakeArrayView(&C,1),Cache).IsSuccess()))return false;
 TestEqual(TEXT("One upgrade material charge"),Stock(S,D,TEXT("Good.Salt")),int64(7000));
 C=FHansaGameplayCommand::Create(Header(S,D,2),FHansaUpgradeProductionCommand{Id<FHansaProductionId>(1)});
 TestFalse(TEXT("Duplicate upgrade rejected"),FHansaGameplayCommandGateway::ExecuteTick(S,D,MakeArrayView(&C,1),Cache).IsSuccess());
 for(int N=0;N<5;++N)if(!TestTrue(TEXT("Upgrade advances"),Step(S,D,Cache)))return false;
 const auto Read=S.CreateReadOnlyAccess(D);const auto P=Read.QueryProduction(Id<FHansaProductionId>(1));
 TestTrue(TEXT("Original production identity retained"),P.IsSet());
 TestEqual(TEXT("Original building retained"),P->BuildingId.GetValue(),uint64(1));
 TestEqual(TEXT("Material charge remains once"),Stock(S,D,TEXT("Good.Salt")),int64(7000));
 C=FHansaGameplayCommand::Create(Header(S,D,2),FHansaSetProductionModeCommand{Id<FHansaProductionId>(1),Recipe(TEXT("Recipe.SaltedCatch")),false});
 TestTrue(TEXT("Completed upgrade unlocks salted mode"),FHansaGameplayCommandGateway::ExecuteTick(S,D,MakeArrayView(&C,1),Cache).IsSuccess());
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFishCargoTest,"Hansa.Simulation.PreservedFish.VehicleCargoLoss",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFishCargoTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Tests::Preservation;
 const auto D=Definitions(true);auto City=State(D,1,false,false,100000);auto Ship=State(D,1,false,false,100000,false,true);FHansaSimulationTransientCache A,B;
 for(int N=0;N<144;++N)if(!Step(City,D,A)||!Step(Ship,D,B)){AddError(TEXT("Cargo spoilage tick failed"));return false;}
 TestEqual(TEXT("City and ship fresh losses agree"),Stock(City,D,TEXT("Good.Fish")),Stock(Ship,D,TEXT("Good.Fish")));
 TestEqual(TEXT("City and ship preserved losses agree"),Stock(City,D,TEXT("Good.PreservedFish")),Stock(Ship,D,TEXT("Good.PreservedFish")));
 TestEqual(TEXT("Vehicle cargo tracks surviving physical stock"),Ship.CreateReadOnlyAccess(D).GetVehicles()[0].Cargo.GetRawValue(),Stock(Ship,D,TEXT("Good.Fish"))+Stock(Ship,D,TEXT("Good.PreservedFish")));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaFishSpoilageTest,"Hansa.Simulation.PreservedFish.StorageLossAndSplitting",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaFishSpoilageTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;using namespace Hansa::Tests::Preservation;
 const auto D=Definitions(true);auto Whole=State(D,1,false,false,100000);auto Split=State(D,100,false,false,100000);FHansaSimulationTransientCache A,B;
 for(int N=0;N<144*30;++N){if(!Step(Whole,D,A)||!Step(Split,D,B)){AddError(TEXT("Spoilage tick failed"));return false;}}
 TestEqual(TEXT("Splitting cannot avoid fresh losses"),Stock(Whole,D,TEXT("Good.Fish")),Stock(Split,D,TEXT("Good.Fish")));
 TestEqual(TEXT("Splitting cannot avoid preserved losses"),Stock(Whole,D,TEXT("Good.PreservedFish")),Stock(Split,D,TEXT("Good.PreservedFish")));
 TestTrue(TEXT("Fresh losses meaningful after 30 days"),Stock(Whole,D,TEXT("Good.Fish"))<25000);
 TestTrue(TEXT("Preserved stock retains at least 97 percent"),Stock(Whole,D,TEXT("Good.PreservedFish"))>=97000);
 AddInfo(FString::Printf(TEXT("30-day measured stock: fresh %lld; preserved %lld milli-kg"),Stock(Whole,D,TEXT("Good.Fish")),Stock(Whole,D,TEXT("Good.PreservedFish"))));
 const auto Loss=Whole.CreateReadOnlyAccess(D).GetInventories().QuerySpoilage();TestEqual(TEXT("Separate loss report for each fish good"),Loss.Num(),2);
 return !HasAnyErrors();
}
#endif
