#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Definitions/HansaArtisanProductionDraft.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitions.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Model/HansaSimulationState.h"
#include "Systems/HansaSimulationPipeline.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Algo/Reverse.h"
#include "Save/HansaSaveEnvelope.h"
using namespace Hansa::Simulation;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaArtisanRecipeDraftTest,
 "Hansa.ArtisanProduction.DraftAndAuthoritativeCycles",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaArtisanRecipeDraftTest::RunTest(const FString&)
{
 auto Draft=Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
 TArray<const UHansaDefinitionBase*> Before;for(const auto& D:Draft) Before.Add(D.Get());
 const auto Baseline=FHansaEconomicDefinitionCompiler::Compile(Before);
 if(!TestTrue(TEXT("Baseline compiles"),Baseline.IsValid())) return false;
 FString Error;
 if(!TestTrue(TEXT("Draft can be authored"),Hansa::Editor::ArtisanProduction::ApplyDraft(Draft,Error))) { AddError(Error);return false; }
 TArray<const UHansaDefinitionBase*> Pointers;for(const auto& D:Draft)Pointers.Add(D.Get());
 const auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Pointers);
 for(const auto& I:Compiled.Issues)if(I.Severity==EHansaDefinitionValidationSeverity::Error)AddError(I.PropertyPath+TEXT(": ")+I.Cause.ToString());
 if(!TestTrue(TEXT("Extended registry compiles"),Compiled.IsValid()))return false;
 Algo::Reverse(Pointers);const auto Reversed=FHansaEconomicDefinitionCompiler::Compile(Pointers);
 TestEqual(TEXT("Definition order does not alter registry"),Reversed.Registry.GetRegistryHash(),Compiled.Registry.GetRegistryHash());
 TestEqual(TEXT("Only twelve new definitions"),Draft.Num(),Before.Num()+12);
 const int32 Count=Draft.Num();
 TestFalse(TEXT("Repeat application rejected before mutation"),Hansa::Editor::ArtisanProduction::ApplyDraft(Draft,Error));
 TestEqual(TEXT("No duplicate definitions added"),Draft.Num(),Count);
 const auto& Registry=Compiled.Registry;
 const auto Context=FHansaSimulationDefinitionContext::TryCreate(
  FHansaScenarioId::TryParse(TEXT("Scenario.ArtisanProductionTest")).Value,Registry.GetRegistryHash(),Registry);
 if(!TestTrue(TEXT("Simulation definition context"),Context.IsSuccess()))return false;
 struct Case {const TCHAR* Building;const TCHAR* Recipe;};
 const Case Cases[]={
  {TEXT("Building.CharcoalBurner"),TEXT("Recipe.BurnCharcoal")},
  {TEXT("Building.Smithy"),TEXT("Recipe.SmithTools")},
  {TEXT("Building.Tannery"),TEXT("Recipe.TanLeather")},
  {TEXT("Building.Shoemaker"),TEXT("Recipe.MakeShoes")}
 };
 for(const auto& C:Cases)
 {
  const auto* Recipe=Registry.FindRecipe(C.Recipe);
  if(!TestNotNull(C.Recipe,Recipe))return false;
  // -1 supplies one exact batch; other cases omit each input independently.
  // Last case supplies all goods but no required workforce.
  for(int32 Missing=-1;Missing<=Recipe->Inputs.Num();++Missing)
  {
   const bool NoWorkforce=Missing==Recipe->Inputs.Num();
   const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
   const auto InventoryId=FHansaInventoryId::TryCreate(1).Value;
   const auto ProductionId=FHansaProductionId::TryCreate(1).Value;
   const auto BuildingId=FHansaBuildingId::TryCreate(1).Value;
   FHansaSimulationInitialization Init;
   Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value).Value;
   Init.CampaignSeed=42;Init.Houses.Add({FHansaHouseId::TryCreate(1).Value,FHansaMoney::FromRaw(100000)});
   Init.Cities.Add({City,FHansaQuantity()});
   Init.Buildings.Add({BuildingId,FHansaBuildingTypeId::TryParse(C.Building).Value,FHansaHouseId::TryCreate(1).Value,FHansaRate::FromPartsPerMillion(FHansaRate::Scale)});
   FHansaInventoryInitialization Inventory;Inventory.Id=InventoryId;Inventory.CityId=City;
   Inventory.OwnerKind=EHansaInventoryOwnerKind::City;Inventory.Capacity=FHansaQuantity::FromRaw(100000);
   for(const auto& G:Registry.GetGoods())Inventory.AcceptedGoods.Add(FHansaGoodId::TryParse(G.StableId).Value);
   for(int32 I=0;I<Recipe->Inputs.Num();++I)if(I!=Missing)
    Inventory.InitialStock.Add({FHansaGoodId::TryParse(Recipe->Inputs[I].GoodId).Value,FHansaQuantity::FromRaw(Recipe->Inputs[I].QuantityMilliUnits)});
   Init.Inventories.Add(Inventory);
   FHansaProductionInitialization P;P.Id=ProductionId;P.BuildingId=BuildingId;
   P.RecipeId=FHansaRecipeId::TryParse(C.Recipe).Value;P.InputInventoryId=P.OutputInventoryId=InventoryId;
   P.AllocatedLaborerWorkforce=NoWorkforce?0:Recipe->LaborerWorkforce;
   P.AllocatedArtisanWorkforce=NoWorkforce?0:Recipe->ArtisanWorkforce;Init.Productions.Add(P);
   auto Made=FHansaSimulationState::TryCreate(Init);
   if(!TestTrue(TEXT("Valid production state"),Made.IsSuccess()))return false;
   auto State=Made.Value; FHansaSimulationTransientCache Cache;
   for(int32 Tick=0;Tick<Recipe->CycleTicks+1;++Tick)
    if(!TestTrue(TEXT("Authoritative tick succeeds"),FHansaGameplayCommandGateway::ExecuteTick(State,Context.Value,{},Cache).IsSuccess()))return false;
   // Persist blocked and completed batches, then compare future authoritative ticks.
   FHansaSaveSnapshot Saved;Saved.State=State;Saved.BuildVersion=TEXT("ArtisanProduction");
   Saved.SavedUtc=TEXT("2026-09-19T00:00:00Z");Saved.Players.Add({1,FHansaHouseId::TryCreate(1).Value});
   TArray<uint8> Bytes;
   const auto Encoded=FHansaSaveEnvelope::Encode(Saved,Context.Value,Bytes);
   if(!TestTrue(*Encoded.Message,Encoded.IsSuccess()))return false;
   FHansaSaveSnapshot Loaded;
   const auto Decoded=FHansaSaveEnvelope::Decode(Bytes,Context.Value,Loaded);
   if(!TestTrue(*Decoded.Message,Decoded.IsSuccess()))return false;
   TestEqual(TEXT("Save preserves campaign hash"),Decoded.CampaignHash,Encoded.CampaignHash);
   const auto OldContext=FHansaSimulationDefinitionContext::TryCreate(
    FHansaScenarioId::TryParse(TEXT("Scenario.ArtisanProductionTest")).Value,Baseline.Registry.GetRegistryHash(),Baseline.Registry);
   TestTrue(TEXT("Candidate saves reject an incompatible old catalog explicitly"),
    FHansaSaveEnvelope::Decode(Bytes,OldContext.Value,Loaded).Error==EHansaSaveError::IncompatibleContent);
   FHansaSimulationTransientCache RestoredCache;
   auto Original=State;
   for(int32 I=0;I<5;++I)
   {
    auto A=FHansaGameplayCommandGateway::ExecuteTick(Original,Context.Value,{},Cache);
    auto B=FHansaGameplayCommandGateway::ExecuteTick(Loaded.State,Context.Value,{},RestoredCache);
    TestTrue(TEXT("Saved production continues deterministically"),A.IsSuccess()&&B.IsSuccess()&&A.GetFingerprintAfter()==B.GetFingerprintAfter());
   }
   const auto View=State.CreateReadOnlyAccess(Context.Value);
   const auto Result=View.QueryProduction(ProductionId);
   if(!TestTrue(TEXT("Production projection exists"),Result.IsSet()))return false;
   TestEqual(*FString::Printf(TEXT("%s case %d completed cycles"),C.Recipe,Missing),Result->CompletedCycles,Missing==-1?uint64(1):uint64(0));
   if(Missing>=0 && !NoWorkforce)
   {
    TestTrue(TEXT("Specific input blocker"),Result->Blocker==EHansaProductionBlocker::MissingInput);
    TestTrue(TEXT("Missing good identity"),Result->BlockingGoodId==FHansaGoodId::TryParse(Recipe->Inputs[Missing].GoodId).Value);
   }
   if(NoWorkforce)TestTrue(TEXT("Workforce shortage blocks"),Result->Blocker==(Recipe->ArtisanWorkforce?EHansaProductionBlocker::InsufficientArtisanWorkforce:EHansaProductionBlocker::InsufficientLaborerWorkforce));
   for(int32 I=0;I<Recipe->Inputs.Num();++I)
   {
    const auto Stock=View.GetInventories().QueryStock(InventoryId,FHansaGoodId::TryParse(Recipe->Inputs[I].GoodId).Value);
    TestTrue(TEXT("Input stock query exists"),Stock.IsSet());
    if(Stock.IsSet())TestEqual(TEXT("Inputs are consumed once or retained while blocked"),Stock->Stock.GetRawValue(),Missing==-1||Missing==I?int64(0):Recipe->Inputs[I].QuantityMilliUnits);
   }
   for(const auto& Output:Recipe->Outputs)
   {
    const auto Stock=View.GetInventories().QueryStock(InventoryId,FHansaGoodId::TryParse(Output.GoodId).Value);
    if(Stock.IsSet())TestEqual(TEXT("Output conserved"),Stock->Stock.GetRawValue(),Missing==-1?Output.QuantityMilliUnits:int64(0));
   }
  }
 }
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaArtisanConnectedChainsTest,
 "Hansa.ArtisanProduction.ConnectedChainsAndMidBatchSave",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaArtisanConnectedChainsTest::RunTest(const FString&)
{
 auto Draft=Hansa::Editor::EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage());
 FString Error;if(!Hansa::Editor::ArtisanProduction::ApplyDraft(Draft,Error)){AddError(Error);return false;}
 TArray<const UHansaDefinitionBase*> Ptrs;for(const auto& D:Draft)Ptrs.Add(D.Get());
 const auto C=FHansaEconomicDefinitionCompiler::Compile(Ptrs);if(!C.IsValid())return false;
 const auto D=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.ArtisanChains")).Value,C.Registry.GetRegistryHash(),C.Registry).Value;
 const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 const auto House=FHansaHouseId::TryCreate(1).Value;const auto StockId=FHansaInventoryId::TryCreate(1).Value;
 FHansaSimulationInitialization Init;
 Init.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick()).Value;
 Init.CampaignSeed=123;Init.Houses.Add({House,FHansaMoney::FromRaw(100000)});Init.Cities.Add({City,FHansaQuantity()});
 FHansaInventoryInitialization Stock;Stock.Id=StockId;Stock.CityId=City;Stock.OwnerKind=EHansaInventoryOwnerKind::City;Stock.Capacity=FHansaQuantity::FromRaw(100000);
 for(const auto& Good:C.Registry.GetGoods())Stock.AcceptedGoods.Add(FHansaGoodId::TryParse(Good.StableId).Value);
 for(const auto& Pair:TArray<TPair<FString,int64>>{{TEXT("Good.Timber"),6000},{TEXT("Good.Iron"),4500},{TEXT("Good.RawHides"),3000},{TEXT("Good.TanningBark"),1000}})
  Stock.InitialStock.Add({FHansaGoodId::TryParse(Pair.Key).Value,FHansaQuantity::FromRaw(Pair.Value)});
 Init.Inventories.Add(Stock);
 const TCHAR* Buildings[]={TEXT("Building.CharcoalBurner"),TEXT("Building.Smithy"),TEXT("Building.Tannery"),TEXT("Building.Shoemaker")};
 const TCHAR* Recipes[]={TEXT("Recipe.BurnCharcoal"),TEXT("Recipe.SmithTools"),TEXT("Recipe.TanLeather"),TEXT("Recipe.MakeShoes")};
 for(int32 I=0;I<4;++I)
 {
  const auto Id=FHansaBuildingId::TryCreate(I+1).Value;const auto* R=C.Registry.FindRecipe(Recipes[I]);
  Init.Buildings.Add({Id,FHansaBuildingTypeId::TryParse(Buildings[I]).Value,House,FHansaRate::FromPartsPerMillion(FHansaRate::Scale)});
  FHansaProductionInitialization P;P.Id=FHansaProductionId::TryCreate(I+1).Value;P.BuildingId=Id;P.RecipeId=FHansaRecipeId::TryParse(Recipes[I]).Value;
  P.InputInventoryId=P.OutputInventoryId=StockId;P.AllocatedLaborerWorkforce=R->LaborerWorkforce;P.AllocatedArtisanWorkforce=R->ArtisanWorkforce;Init.Productions.Add(P);
 }
 const auto Created=FHansaSimulationState::TryCreate(Init);if(!TestTrue(TEXT("Connected state valid"),Created.IsSuccess()))return false;
 auto State=Created.Value;FHansaSimulationTransientCache Cache;
 for(int32 Tick=0;Tick<100;++Tick)if(!FHansaGameplayCommandGateway::ExecuteTick(State,D,{},Cache))return false;
 FHansaSaveSnapshot S;S.State=State;S.BuildVersion=TEXT("ArtisanChains");S.SavedUtc=TEXT("2026-09-19T00:00:00Z");S.Players.Add({1,House});
 TArray<uint8> Bytes;if(!TestTrue(TEXT("Encode mid-batch"),FHansaSaveEnvelope::Encode(S,D,Bytes).IsSuccess()))return false;
 FHansaSaveSnapshot Restored;if(!TestTrue(TEXT("Decode mid-batch"),FHansaSaveEnvelope::Decode(Bytes,D,Restored).IsSuccess()))return false;
 FHansaSimulationTransientCache RestoredCache;
 for(int32 Tick=100;Tick<550;++Tick)
 {
  const auto A=FHansaGameplayCommandGateway::ExecuteTick(State,D,{},Cache),B=FHansaGameplayCommandGateway::ExecuteTick(Restored.State,D,{},RestoredCache);
  if(!TestTrue(TEXT("Connected continuation deterministic"),A.IsSuccess()&&B.IsSuccess()&&A.GetFingerprintAfter()==B.GetFingerprintAfter()))return false;
 }
 const auto V=State.CreateReadOnlyAccess(D);
 for(const auto& Pair:TArray<TPair<FString,int64>>{{TEXT("Good.Tools"),3000},{TEXT("Good.Shoes"),2000},{TEXT("Good.Charcoal"),0},{TEXT("Good.Leather"),0},{TEXT("Good.Iron"),0},{TEXT("Good.Timber"),0},{TEXT("Good.RawHides"),0},{TEXT("Good.TanningBark"),0}})
 {
  const auto Q=V.GetInventories().QueryStock(StockId,FHansaGoodId::TryParse(Pair.Key).Value);
  if(!TestTrue(TEXT("Connected inventory query"),Q.IsSet()))return false;
  TestEqual(*Pair.Key,Q->Stock.GetRawValue(),Pair.Value);
 }
 return !HasAnyErrors();
}

#endif
