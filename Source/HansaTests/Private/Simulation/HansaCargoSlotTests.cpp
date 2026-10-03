#include "Misc/AutomationTest.h"
#include "Inventory/HansaInventory.h"
#include "Trade/HansaCargoPlan.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Save/HansaSaveEnvelope.h"
#include "Queries/HansaSimulationReadOnly.h"
#if WITH_DEV_AUTOMATION_TESTS
using namespace Hansa::Simulation;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCargoSlotsTest,"Hansa.TradeRoute.Slots.AtomicTransfers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCargoSlotsTest::RunTest(const FString&)
{
 const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value, Salt=FHansaGoodId::TryParse(TEXT("Good.Salt")).Value, Tools=FHansaGoodId::TryParse(TEXT("Good.Tools")).Value;
 FHansaInventoryInitialization I;I.Id=FHansaInventoryId::TryCreate(1).Value;I.OwnerKind=EHansaInventoryOwnerKind::Vehicle;I.VehicleId=FHansaVehicleId::TryCreate(1).Value;I.Capacity=FHansaQuantity::FromRaw(60000);I.AcceptedGoods={Grain,Salt,Tools};
 auto Created=FHansaInventoryLedger::TryCreate({I});if(!TestTrue(TEXT("Cargo initialized"),Created.IsSuccess()))return false;auto& L=Created.Value;
 auto Read=[&]{return L.CreateReadOnlyAccess().QueryInventory(I.Id).GetValue();};
 uint64 Seq=0;auto Load=[&](int32 Slot,FHansaGoodId G,int64 Q){return L.TryTransfer(FHansaInventoryEndpoint::Source(TEXT("Fixture")),FHansaInventoryEndpoint::CargoSlot(I.Id,Slot),G,FHansaQuantity::FromRaw(Q),FHansaSimulationTick(),++Seq).IsSuccess();};
 auto Unload=[&](int32 Slot,FHansaGoodId G,int64 Q){return L.TryTransfer(FHansaInventoryEndpoint::CargoSlot(I.Id,Slot),FHansaInventoryEndpoint::Sink(TEXT("Fixture")),G,FHansaQuantity::FromRaw(Q),FHansaSimulationTick(),++Seq).IsSuccess();};
 TestEqual(TEXT("Three physical slots"),Read().CargoSlots.Num(),3);
 TestTrue(TEXT("Slot 1 can use more than one third of total capacity"),Load(0,Grain,40000));
 TestTrue(TEXT("Second independent product"),Load(1,Tools,10000));TestTrue(TEXT("Third independent product"),Load(2,Salt,10000));
 TestFalse(TEXT("Total capacity is authoritative"),Load(0,Grain,1));
 TestTrue(TEXT("Partial unloading"),Unload(0,Grain,15000));
 TestFalse(TEXT("Cannot replace remaining grain with salt"),Load(0,Salt,1000));
 TestEqual(TEXT("Failed load preserves slot"),Read().CargoSlots[0].Quantity.GetRawValue(),int64(25000));
 TestFalse(TEXT("Cannot unload another column"),Unload(1,Grain,1000));
 TestTrue(TEXT("Empty first slot"),Unload(0,Grain,25000));TestTrue(TEXT("Reuse emptied slot"),Load(0,Salt,20000));
 TestTrue(TEXT("Unload only selected same-product slot"),Unload(2,Salt,5000));
 TestEqual(TEXT("Slot 1 salt is untouched"),Read().CargoSlots[0].Quantity.GetRawValue(),int64(20000));
 TestFalse(TEXT("Invalid slot rejected"),Load(3,Grain,1));
 const auto Snapshot=L.CreateReadOnlyAccess().CaptureSnapshot();TestTrue(TEXT("Allocation equals aggregate stock"),FHansaInventoryLedger::ValidateCargoSlots(Snapshot.GetInventories()[0]));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCargoPlanTest,"Hansa.TradeRoute.Slots.PlannedContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCargoPlanTest::RunTest(const FString&)
{
 const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value,Salt=FHansaGoodId::TryParse(TEXT("Good.Salt")).Value;
 FHansaRouteCargoAction A;A.CargoSlotIndex=0;A.GoodId=Grain;A.QuantityLimit=FHansaQuantity::FromRaw(20000);
 TArray<FHansaRouteStop> Stops;Stops.SetNum(2);Stops[0].Actions.Add(A);A.Kind=EHansaRouteCargoActionKind::Unload;Stops[1].Actions.Add(A);
 Stops[0].CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 Stops[1].CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 TestTrue(TEXT("Author before loading"),FHansaCargoPlan::Validate(Stops,60000));
 TestEqual(TEXT("Planned arrival"),FHansaCargoPlan::Before(Stops,1,false,60000)[0].Quantity.GetRawValue(),int64(20000));
 Stops[1].Actions[0].QuantityLimit=FHansaQuantity::FromRaw(10000);A.Kind=EHansaRouteCargoActionKind::Load;A.GoodId=Salt;Stops[1].Actions.Add(A);
 TestFalse(TEXT("Partial unload blocks reuse"),FHansaCargoPlan::Validate(Stops,60000));
 Stops[1].Actions[0].QuantityLimit=FHansaQuantity::FromRaw(20000);A.Kind=EHansaRouteCargoActionKind::Unload;Stops[0].Actions.Add(A);
 TestTrue(TEXT("Unload before load regardless of array order"),FHansaCargoPlan::Validate(Stops,60000));
 TArray<FHansaCargoSlot> SameGoods={{Grain,FHansaQuantity::FromRaw(20000)},{Grain,FHansaQuantity::FromRaw(20000)},{}};
 A.Kind=EHansaRouteCargoActionKind::OwnedCityUnload;A.GoodId=Grain;A.MinimumSourceReserve=FHansaQuantity::FromRaw(10000);
 TestTrue(TEXT("Reserve applies to total product stock"),FHansaCargoPlan::Apply(SameGoods,A,60000));
 TestEqual(TEXT("Selected slot may empty while other slot protects reserve"),SameGoods[0].Quantity.GetRawValue(),int64(0));
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoCityDirectionTest,"Hansa.TradeRoute.Slots.CityDirection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoCityDirectionTest::RunTest(const FString&)
{
 const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
 const auto Salt=FHansaGoodId::TryParse(TEXT("Good.Salt")).Value;
 FHansaRouteStop Home;Home.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 FHansaRouteCargoAction Load;Load.GoodId=Grain;Load.QuantityLimit=FHansaQuantity::FromRaw(1000);Load.CargoSlotIndex=0;
 for(auto LoadKind:{EHansaRouteCargoActionKind::Load,EHansaRouteCargoActionKind::StationLoad,EHansaRouteCargoActionKind::OwnedCityLoad})
 for(auto UnloadKind:{EHansaRouteCargoActionKind::Unload,EHansaRouteCargoActionKind::StationUnload,EHansaRouteCargoActionKind::OwnedCityUnload})
 for(int32 Slot:{int32(INDEX_NONE),0,1,2})
 {
  Load.Kind=LoadKind;auto Unload=Load;Unload.Kind=UnloadKind;Unload.CargoSlotIndex=Slot;
  Home.Actions={Load,Unload};TArray<FHansaRouteStop> Stops{Home};
  TestFalse(TEXT("All transfer sources and slots reject opposite directions"),FHansaCargoPlan::Validate(Stops,60000));
  Stops[0].Actions.Pop();auto Return=Home;Return.Actions={Unload};Stops.Add(Return);
  TestTrue(TEXT("Repeated visit to same city cannot bypass restriction"),FHansaCargoPlan::HasConflictingCityActions(Stops));
  Stops[1].CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
  TestFalse(TEXT("Different city may unload the product"),FHansaCargoPlan::HasConflictingCityActions(Stops));
  Stops[1].CityId=Home.CityId;Stops[1].Actions[0].GoodId=Salt;
  TestFalse(TEXT("Different product may unload in loading city"),FHansaCargoPlan::HasConflictingCityActions(Stops));
 }
 return !HasAnyErrors();
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCargoRecoveryTest,"Hansa.TradeRoute.Slots.LegacyOverflowRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCargoRecoveryTest::RunTest(const FString&)
{
 FHansaInventoryInitialization I;I.Id=FHansaInventoryId::TryCreate(77).Value;I.OwnerKind=EHansaInventoryOwnerKind::Vehicle;I.VehicleId=FHansaVehicleId::TryCreate(77).Value;I.Capacity=FHansaQuantity::FromRaw(60000);
 for(const TCHAR* Name:{TEXT("Good.Grain"),TEXT("Good.Salt"),TEXT("Good.Tools"),TEXT("Good.Wool")}){const auto G=FHansaGoodId::TryParse(Name).Value;I.AcceptedGoods.Add(G);I.InitialStock.Add({G,FHansaQuantity::FromRaw(10000)});}
 auto Result=FHansaInventoryLedger::TryCreate({I});if(!TestTrue(TEXT("Legacy inventory restored"),Result.IsSuccess()))return false;
 auto& Ledger=Result.Value;const auto Before=Ledger.CreateReadOnlyAccess().QueryInventory(I.Id).GetValue();
 TestEqual(TEXT("Overflow retained without loss"),Before.CargoSlots.Num(),4);TestEqual(TEXT("All original goods retained"),Before.UsedCapacity.GetRawValue(),int64(40000));
 const auto Recovery=Before.CargoSlots[3];
 TestFalse(TEXT("Recovery slot cannot receive new cargo"),Ledger.TryTransfer(FHansaInventoryEndpoint::Source(TEXT("Fixture")),FHansaInventoryEndpoint::CargoSlot(I.Id,3),Recovery.GoodId,FHansaQuantity::FromRaw(1000),FHansaSimulationTick(),1).IsSuccess());
 TestTrue(TEXT("Existing pooled unload can recover old cargo"),Ledger.TryTransfer(FHansaInventoryEndpoint::Inventory(I.Id),FHansaInventoryEndpoint::Sink(TEXT("Fixture")),Recovery.GoodId,Recovery.Quantity,FHansaSimulationTick(),2).IsSuccess());
 const auto After=Ledger.CreateReadOnlyAccess().QueryInventory(I.Id).GetValue();TestEqual(TEXT("Only recovered goods removed"),After.UsedCapacity.GetRawValue(),int64(30000));
 TestFalse(TEXT("Recovery does not duplicate goods"),Ledger.TryTransfer(FHansaInventoryEndpoint::Inventory(I.Id),FHansaInventoryEndpoint::Sink(TEXT("Fixture")),Recovery.GoodId,Recovery.Quantity,FHansaSimulationTick(),3).IsSuccess());
 return !HasAnyErrors();
}
#if WITH_HANSA_AUTOMATION
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaCargoSaveTest,"Hansa.TradeRoute.Slots.SaveMigration21",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaCargoSaveTest::RunTest(const FString&)
{
 auto Fixture=FHansaProductionFixture::TryCreateRouteDelivery();if(!Fixture)return false;
 auto& F=Fixture.Value;TestTrue(TEXT("Advance cargo fixture"),F.Step(3).IsSuccess());
 FHansaSaveSnapshot S;S.State=F.GetState();S.BuildVersion=TEXT("cargo-slots");S.DisplayName=TEXT("Migration");S.SavedUtc=TEXT("2026-09-24T00:00:00Z");
 for(const auto& H:S.State.CreateReadOnlyAccess(F.GetDefinitions()).GetHouses())S.Players.Add({H.Id.GetValue(),H.Id});
 TArray<uint8> Bytes;auto Written=FHansaSaveEnvelope::EncodeHistoricalFixtureForTests(S,F.GetDefinitions(),21,33,Bytes);
 if(!TestTrue(*Written.Message,Written.IsSuccess()))return false;
 FHansaSaveSnapshot Restored;auto Read=FHansaSaveEnvelope::Decode(Bytes,F.GetDefinitions(),Restored);if(!TestTrue(*Read.Message,Read.IsSuccess()))return false;
 TestTrue(TEXT("Named migration"),Read.AppliedMigrations.Contains(TEXT("Hansa.Save.21To22.PreserveCargoSlotAllocations")));
 auto Before=S.State.CreateReadOnlyAccess(F.GetDefinitions()).GetInventories().BuildProjection();auto After=Restored.State.CreateReadOnlyAccess(F.GetDefinitions()).GetInventories().BuildProjection();
 TestEqual(TEXT("No inventories lost"),After.Num(),Before.Num());
 for(int32 I=0;I<Before.Num();++I){TestEqual(TEXT("No goods lost or created"),After[I].UsedCapacity.GetRawValue(),Before[I].UsedCapacity.GetRawValue());if(After[I].OwnerKind==EHansaInventoryOwnerKind::Vehicle)TestTrue(TEXT("Physical slots restored"),After[I].CargoSlots.Num()>=3);}
 TestTrue(TEXT("Current save encodes"),FHansaSaveEnvelope::Encode(Restored,F.GetDefinitions(),Bytes).IsSuccess());
 FHansaSaveSnapshot Again;auto Repeat=FHansaSaveEnvelope::Decode(Bytes,F.GetDefinitions(),Again);TestTrue(TEXT("Repeat load has no migration"),Repeat.IsSuccess()&&Repeat.AppliedMigrations.IsEmpty());TestEqual(TEXT("Round trip hash"),Repeat.AuthoritativeHash,Read.AuthoritativeHash);
 return !HasAnyErrors();
}
#endif
#endif
