#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "World/HansaLubeckScenarioInitializer.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace {
using namespace Hansa::Simulation;
template<class T>T Entity(uint64 N){return T::TryCreate(N).Value;}
template<class T>T Stable(const TCHAR* N){return T::TryParse(N).Value;}
struct FPreservationRun {
 FHansaSimulationDefinitionContext Definitions;FHansaSimulationState State;FHansaSimulationTransientCache Cache;uint64 Sequence=0;
 template<class T>bool Command(const T& Payload){FHansaCommandHeader H;H.CommandId=Entity<FHansaCommandId>(++Sequence);H.GlobalSequence=Sequence;H.Authority.IssuingHouseId=Entity<FHansaHouseId>(1);H.Authority.PrincipalId=1;H.RequestedExecutionTick=State.CreateReadOnlyAccess(Definitions).GetClock().GetTick();auto C=FHansaGameplayCommand::Create(H,Payload);return FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,MakeArrayView(&C,1),Cache).IsSuccess();}
 bool Step(){return FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,{},Cache).IsSuccess();}
 int64 Stock(const TCHAR* Good,uint64 Inventory=1)const{auto V=State.CreateReadOnlyAccess(Definitions).GetInventories().QueryStock(Entity<FHansaInventoryId>(Inventory),Stable<FHansaGoodId>(Good));return V?V->Stock.GetRawValue():0;}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPreservationTradeBalance,"Hansa.Integration.PreservedFish.ModestStockTradeAndBarrels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaPreservationTradeBalance::RunTest(const FString&){
 using namespace Hansa::Simulation;
 FHansaEconomicRegistry Registry;FString Error;if(!TestTrue(TEXT("Load actual preservation catalog"),FHansaLubeckScenarioInitializer::TryLoadMvpRegistry(Registry,Error))){AddError(Error);return false;}
 FString Csv=TEXT("strategy,tick,fresh_kg,preserved_kg,salt_kg,barrels,beer,money_raw,salt_incoming,fish_cargo\n");
 for(int Mode=0;Mode<4;++Mode){
  FPreservationRun Run;auto Context=FHansaSimulationDefinitionContext::TryCreate(Stable<FHansaScenarioId>(TEXT("Scenario.PreservationBalance")),Registry.GetRegistryHash(),Registry);if(!Context)return false;Run.Definitions=Context.Value;
  FHansaSimulationInitialization I;I.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(0).Value,10).Value;
  I.Houses.Add({Entity<FHansaHouseId>(1),FHansaMoney::FromRaw(Mode==3?1000:100000)});
  const auto Local=Stable<FHansaCityDefinitionId>(TEXT("City.Lubeck")),Remote=Stable<FHansaCityDefinitionId>(TEXT("City.Rostock"));I.Cities={{Local,{}},{Remote,{}}};
  for(uint64 N=1;N<=3;++N){
   FHansaInventoryInitialization V;V.Id=Entity<FHansaInventoryId>(N);V.Capacity=FHansaQuantity::FromRaw(N==3?60000:500000);
   V.OwnerKind=N==3?EHansaInventoryOwnerKind::Vehicle:EHansaInventoryOwnerKind::City;
   if(N==3)V.VehicleId=Entity<FHansaVehicleId>(1);else V.CityId=N==1?Local:Remote;
   for(const auto& G:Registry.GetGoods()){
    const auto Id=Stable<FHansaGoodId>(*G.StableId);V.AcceptedGoods.Add(Id);int64 Quantity=0;
    if(N==1){if(G.StableId==TEXT("Good.Barrels"))Quantity=2000;else if(G.StableId==TEXT("Good.Malt"))Quantity=12000;else if(G.StableId==TEXT("Good.Hops"))Quantity=4000;else if(G.StableId==TEXT("Good.Planks")||G.StableId==TEXT("Good.Timber"))Quantity=12000;}
    const auto* Profile=N<3?Registry.FindCityMarket(N==1?TEXT("City.Lubeck"):TEXT("City.Rostock")):nullptr;
    const auto* P=Profile?Profile->Goods.FindByPredicate([&](const auto& A){return A.GoodId==G.StableId;}):nullptr;
    if(N==2&&P)Quantity=P->InitialStockMilliUnits;
    V.InitialStock.Add({Id,FHansaQuantity::FromRaw(Quantity)});
    if(P){FHansaCityMarketInitialization M;M.CityId=V.CityId;M.GoodId=Id;M.InventoryIds={V.Id};M.DesiredReserve=FHansaQuantity::FromRaw(P->DesiredReserveMilliUnits);M.InitialPriceMilliMarks=P->InitialPriceMilliMarks;M.MinimumPriceMilliMarks=P->MinimumPriceMilliMarks;M.MaximumPriceMilliMarks=P->MaximumPriceMilliMarks;M.bMarketOnly=N==2;M.BackgroundProductionPerUpdate=FHansaQuantity::FromRaw(N==2?P->BackgroundProductionMilliUnitsPerUpdate:0);M.BackgroundCitizenDemandPerUpdate=FHansaQuantity::FromRaw(N==2?P->BackgroundCitizenDemandMilliUnitsPerUpdate:0);M.BackgroundIndustrialDemandPerUpdate=FHansaQuantity::FromRaw(N==2?P->BackgroundIndustrialDemandMilliUnitsPerUpdate:0);I.Markets.Add(M);}
   }I.Inventories.Add(V);
  }
  FHansaVehicleState Ship;Ship.Id=Entity<FHansaVehicleId>(1);Ship.DefinitionId=Stable<FHansaVehicleDefinitionId>(TEXT("Vehicle.Cog"));Ship.OwnerId=Entity<FHansaHouseId>(1);Ship.CargoInventoryId=Entity<FHansaInventoryId>(3);Ship.Capacity=FHansaQuantity::FromRaw(60000);Ship.CurrentCityId=Local;Ship.UpkeepPfennigPerTravelTick=Registry.FindVehicle(TEXT("Vehicle.Cog"))->UpkeepPfennigPerTravelTick;I.Vehicles.Add(Ship);
  const TCHAR* Buildings[]={TEXT("Building.Fishery.SaltingShed"),TEXT("Building.Brewery"),TEXT("Building.Cooperage")};
  for(uint64 N=1;N<=3;++N){const auto* B=Registry.FindBuilding(Buildings[N-1]);if(!B)return false;FHansaBuildingState V;V.Id=Entity<FHansaBuildingId>(N);V.DefinitionId=Stable<FHansaBuildingTypeId>(Buildings[N-1]);V.OwnerId=Ship.OwnerId;V.ConstructionState=EHansaConstructionState::Completed;V.ConstructionProgress=FHansaRate::FromPartsPerMillion(FHansaRate::Scale);I.Buildings.Add(V);FHansaProductionInitialization P;P.Id=Entity<FHansaProductionId>(N);P.BuildingId=V.Id;P.RecipeId=Stable<FHansaRecipeId>(*B->RecipeIds[0]);P.InputInventoryId=Entity<FHansaInventoryId>(1);P.OutputInventoryId=P.InputInventoryId;P.AllocatedLaborerWorkforce=N==1?4:B->LaborerWorkforce;P.AllocatedArtisanWorkforce=B->ArtisanWorkforce;P.bActive=N!=3;I.Productions.Add(P);}
  auto Created=FHansaSimulationState::TryCreate(I);if(!TestTrue(TEXT("Explicit modest-stock profile initializes"),Created.IsSuccess()))return false;Run.State=Created.Value;
  if(Mode>0){if(!TestTrue(TEXT("Select salted with shortage fallback"),Run.Command(FHansaSetProductionModeCommand{Entity<FHansaProductionId>(1),Stable<FHansaRecipeId>(TEXT("Recipe.SaltedCatch")),true})))return false;
   FHansaRouteStop A,B;A.CityId=Local;B.CityId=Remote;
   A.Actions.Add({EHansaRouteCargoActionKind::Unload,EHansaRouteCargoCondition::Always,Stable<FHansaGoodId>(TEXT("Good.Salt")),FHansaQuantity::FromRaw(1000),{}});
   B.Actions.Add({EHansaRouteCargoActionKind::Load,EHansaRouteCargoCondition::Always,Stable<FHansaGoodId>(TEXT("Good.Salt")),FHansaQuantity::FromRaw(1000),FHansaQuantity::FromRaw(4000)});
   if(Mode==2){A.Actions.Insert({EHansaRouteCargoActionKind::Load,EHansaRouteCargoCondition::Always,Stable<FHansaGoodId>(TEXT("Good.PreservedFish")),FHansaQuantity::FromRaw(20000),FHansaQuantity::FromRaw(10000)},0);B.Actions.Insert({EHansaRouteCargoActionKind::Unload,EHansaRouteCargoCondition::Always,Stable<FHansaGoodId>(TEXT("Good.PreservedFish")),FHansaQuantity::FromRaw(20000),{}},0);}
   FHansaCreateRouteCommand Route;Route.RouteId=Entity<FHansaRouteId>(1);Route.VehicleId=Ship.Id;Route.RouteDefinitionId=Stable<FHansaRouteDefinitionId>(TEXT("Route.BalticSea"));Route.Stops={A,B};Route.bActivate=true;if(!TestTrue(TEXT("Normal route starts"),Run.Command(Route)))return false;
  }
  bool SawSalt=false,SawPreserved=false,SawCargo=false,SawFallback=false;int64 SaltPeakIncoming=0,PurchaseCost=0,Sales=0;
  for(int Tick=0;Tick<1440;++Tick){
   if(Tick==300&&!TestTrue(TEXT("Open cooperage to recover barrel shortage"),Run.Command(FHansaSetProductionActiveCommand{Entity<FHansaProductionId>(3),true})))return false;
   const auto Outcome=FHansaGameplayCommandGateway::ExecuteTick(Run.State,Run.Definitions,{},Run.Cache);
   if(!TestTrue(TEXT("Authoritative strategy tick"),Outcome.IsSuccess()))return false;
   for(const auto& Event:Outcome.GetEvents())if(Event.GetType()==EHansaDomainEventType::RouteTradeSettled){if(Event.GetValue()<0)PurchaseCost-=Event.GetValue();else Sales+=Event.GetValue();}
   auto Read=Run.State.CreateReadOnlyAccess(Run.Definitions);const auto P=Read.QueryProduction(Entity<FHansaProductionId>(1));SawSalt|=Run.Stock(TEXT("Good.Salt"))>0;SawPreserved|=Run.Stock(TEXT("Good.PreservedFish"))>0;SawCargo|=Run.Stock(TEXT("Good.PreservedFish"),3)>0;SawFallback|=Mode>0&&P->RecipeId.ToString()==TEXT("Recipe.CatchFish");
   auto Salt=Read.QueryMarket(Local,Stable<FHansaGoodId>(TEXT("Good.Salt")));if(Salt)SaltPeakIncoming=FMath::Max(SaltPeakIncoming,Salt->ExpectedIncomingSupply.GetRawValue());
   if(Tick%144==0||Tick==1439)Csv+=FString::Printf(TEXT("%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%lld,%lld,%lld\n"),Mode,Tick,Run.Stock(TEXT("Good.Fish"))/1000.,Run.Stock(TEXT("Good.PreservedFish"))/1000.,Run.Stock(TEXT("Good.Salt"))/1000.,Run.Stock(TEXT("Good.Barrels"))/1000.,Run.Stock(TEXT("Good.Beer"))/1000.,Read.GetHouses()[0].Money.GetRawValue(),SaltPeakIncoming,Run.Stock(TEXT("Good.PreservedFish"),3));
   if(Tick==420){FHansaSaveSnapshot Save;Save.State=Run.State;Save.BuildVersion=TEXT("PreservationBalance");Save.SavedUtc=TEXT("2026-09-16T00:00:00Z");Save.DisplayName=TEXT("Trade checkpoint");Save.Players.Add({1,Ship.OwnerId});TArray<uint8> Bytes;FHansaSaveSnapshot Loaded;if(!TestTrue(TEXT("Trade checkpoint saves"),FHansaSaveEnvelope::Encode(Save,Run.Definitions,Bytes).IsSuccess())||!TestTrue(TEXT("Trade checkpoint loads"),FHansaSaveEnvelope::Decode(Bytes,Run.Definitions,Loaded).IsSuccess()))return false;TestEqual(TEXT("Checkpoint state identical"),Read.GetFingerprint().Value,Loaded.State.CreateReadOnlyAccess(Run.Definitions).GetFingerprint().Value);}
  }
  if(Mode==0){TestTrue(TEXT("Fresh-only fishery supplies food without imported salt"),Run.Stock(TEXT("Good.Fish"))>0);TestEqual(TEXT("Fresh strategy consumes no salt"),Run.Stock(TEXT("Good.Salt")),int64(0));}
  else if(Mode<3){TestTrue(TEXT("Real salt delivery reaches local stock"),SawSalt);TestTrue(TEXT("Salted batches produce preserved fish"),SawPreserved);TestTrue(TEXT("Shortage recovery uses fresh fallback"),SawFallback);TestTrue(TEXT("Incoming salt tracks loaded cargo"),SaltPeakIncoming>0);}
  if(Mode>0)TestTrue(TEXT("Salt imports are paid purchases"),PurchaseCost>0);
  if(Mode==3){TestTrue(TEXT("Insufficient cash caps total purchases without free stock"),PurchaseCost<=1000);TestFalse(TEXT("Insufficient imported salt cannot create a salted batch"),SawPreserved);}
  if(Mode==2){TestTrue(TEXT("Fish export enters authoritative ship inventory"),SawCargo);TestTrue(TEXT("Foreign fish delivery pays sale proceeds"),Sales>0);TestTrue(TEXT("Demand-sized salt import leaves export strategy cash-positive after ship upkeep"),Run.State.CreateReadOnlyAccess(Run.Definitions).GetHouses()[0].Money.GetRawValue()>100000);}
  AddInfo(FString::Printf(TEXT("Strategy %d purchases=%lld sales=%lld endingMoney=%lld"),Mode,PurchaseCost,Sales,Run.State.CreateReadOnlyAccess(Run.Definitions).GetHouses()[0].Money.GetRawValue()));
 }
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("TestEvidence/PreservedFish");IFileManager::Get().MakeDirectory(*Dir,true);FFileHelper::SaveStringToFile(Csv,*(Dir/TEXT("modest-stock-strategies.csv")));
 return !HasAnyErrors();
}
#endif
