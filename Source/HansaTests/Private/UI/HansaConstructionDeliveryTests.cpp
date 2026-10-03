#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaTradeEstablishmentTestSupport.h"
#include "Presence/HansaConstructionDelivery.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

using namespace Hansa::Simulation;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstructionPhysicalCargo,"Hansa.UI.TradeMap.Establishment.ConstructionPhysicalCargo",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FConstructionPhysicalCargo::RunTest(const FString&)
{
 const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value;
 const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value,Salt=FHansaGoodId::TryParse(TEXT("Good.Salt")).Value,Tools=FHansaGoodId::TryParse(TEXT("Good.Tools")).Value;
 FHansaInventoryInitialization Cargo;Cargo.Id=FHansaInventoryId::TryCreate(1).Value;Cargo.OwnerKind=EHansaInventoryOwnerKind::Vehicle;Cargo.VehicleId=FHansaVehicleId::TryCreate(1).Value;Cargo.Capacity=FHansaQuantity::FromRaw(60000);Cargo.AcceptedGoods={Timber,Planks,Grain,Salt,Tools};Cargo.InitialStock={{Grain,FHansaQuantity::FromRaw(1000)},{Salt,FHansaQuantity::FromRaw(1000)},{Tools,FHansaQuantity::FromRaw(1000)}};
 auto City=Cargo;City.Id=FHansaInventoryId::TryCreate(10).Value;City.OwnerKind=EHansaInventoryOwnerKind::City;City.VehicleId={};City.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;City.InitialStock={{Timber,FHansaQuantity::FromRaw(20000)},{Planks,FHansaQuantity::FromRaw(20000)}};
 auto Created=FHansaInventoryLedger::TryCreate({Cargo,City});if(!TestTrue(TEXT("Physical inventories initialized"),Created.IsSuccess()))return false;auto& Ledger=Created.Value;
 FHansaTradeStationState Station;Station.DeliveryMode=1;Station.FundingInventoryId=Cargo.Id;Station.Status=EHansaTradeStationStatus::Proposed;
 TArray<FHansaCompiledPresenceUpgradeGoodCost> Costs{{TEXT("Good.Timber"),8000},{TEXT("Good.Planks"),4000}};TArray<FHansaInventoryId> Sources{City.Id};
 auto Collect=[&](bool Destination){FHansaConstructionDelivery::Collect(Station,Ledger,Costs,FHansaSimulationTick(),Destination,Destination?TConstArrayView<FHansaInventoryId>():TConstArrayView<FHansaInventoryId>(Sources));};
 auto Stock=[&](FHansaInventoryId Inventory,FHansaGoodId Good){const auto S=Ledger.CreateReadOnlyAccess().QueryStock(Inventory,Good);return S?S->Stock.GetRawValue():0;};
 auto Remove=[&](FHansaGoodId Good,int64 Raw){return Ledger.TryTransfer(FHansaInventoryEndpoint::Inventory(Cargo.Id),FHansaInventoryEndpoint::Sink(TEXT("FixtureSale")),Good,FHansaQuantity::FromRaw(Raw),FHansaSimulationTick(),Ledger.CreateReadOnlyAccess().GetLastMovementSequence()+1).IsSuccess();};
 Collect(false);TestTrue(TEXT("Three occupied slots wait without discarding cargo"),Station.DeliveryReservations.IsEmpty());TestEqual(TEXT("Free capacity does not imply a free slot"),Stock(City.Id,Timber),int64(20000));
 TestTrue(TEXT("Ordinary unloading frees one slot"),Remove(Tools,1000));Collect(false);
 TestEqual(TEXT("First trip contains only exact missing timber"),Stock(Cargo.Id,Timber),int64(8000));TestEqual(TEXT("Planks wait for a later trip"),Stock(Cargo.Id,Planks),int64(0));TestTrue(TEXT("Cargo at source cannot start construction"),Station.SpentGoods.IsEmpty());
 TestFalse(TEXT("Construction reservation prevents spot or route sale"),Remove(Timber,8000));
 TestEqual(TEXT("Existing grain remains aboard"),Stock(Cargo.Id,Grain),int64(1000));
 Collect(true);TestEqual(TEXT("First physical destination delivery recorded"),Station.SpentGoods.Num(),1);TestTrue(TEXT("Delivered reservations consumed"),Station.DeliveryReservations.IsEmpty());TestEqual(TEXT("Delivery frees physical slot"),Stock(Cargo.Id,Timber),int64(0));
 Collect(false);TestEqual(TEXT("Second trip collects exact missing planks"),Stock(Cargo.Id,Planks),int64(4000));Collect(true);
 TestEqual(TEXT("Both materials delivered across two trips"),Station.SpentGoods.Num(),2);TestEqual(TEXT("No duplicate timber collected"),Stock(City.Id,Timber),int64(12000));TestEqual(TEXT("No excess planks collected"),Stock(City.Id,Planks),int64(16000));
 Station.SpentGoods.Reset();Collect(false);TestFalse(TEXT("Loaded materials protected before cancellation"),Remove(Timber,8000));
 TestTrue(TEXT("Cancellation releases protection"),FHansaConstructionDelivery::Release(Station,Ledger,FHansaSimulationTick()));TestEqual(TEXT("Cancellation keeps cargo aboard"),Stock(Cargo.Id,Timber),int64(8000));TestTrue(TEXT("Released cargo is available again"),Remove(Timber,8000));
 // Weight-limited pickup is also partial, even when all three product slots are empty.
 TestTrue(TEXT("Use small remaining capacity"),Ledger.TrySetCapacity(Cargo.Id,FHansaQuantity::FromRaw(5000)));
 Station.DeliveryMode=1;Collect(false);TestEqual(TEXT("Capacity permits only partial timber"),Stock(Cargo.Id,Timber),int64(3000));Collect(true);TestEqual(TEXT("Partial quantity delivered"),Station.SpentGoods[0].Quantity.GetRawValue(),int64(3000));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConstructionDeliveryJourney,"Hansa.UI.TradeMap.Establishment.AutomaticConstructionJourney",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FConstructionDeliveryJourney::RunTest(const FString&)
{
 using namespace Hansa::Multiplayer;
 for(bool Remote:{false,true})for(bool FullPlan:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,200000,[&](FHansaSimulationInitialization& Init){
   const auto Timber=FHansaGoodId::TryParse(TEXT("Good.Timber")).Value,Planks=FHansaGoodId::TryParse(TEXT("Good.Planks")).Value,Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
   auto& Cargo=Init.Inventories[1];Cargo.AcceptedGoods.Add(Grain);Cargo.InitialStock.Reset();
   auto& City=Init.Inventories[3];City.AcceptedGoods={Timber,Planks,Grain};City.InitialStock={{Timber,FHansaQuantity::FromRaw(100000)},{Planks,FHansaQuantity::FromRaw(100000)},{Grain,FHansaQuantity::FromRaw(500000)}};City.Capacity=FHansaQuantity::FromRaw(1000000);
   auto& Destination=Init.Inventories[4];Destination.AcceptedGoods.Add(Grain);Destination.Capacity=FHansaQuantity::FromRaw(1000000);FHansaCityMarketInitialization GrainMarket;GrainMarket.CityId=Destination.CityId;GrainMarket.GoodId=Grain;GrainMarket.InventoryIds={Destination.Id};GrainMarket.InitialPriceMilliMarks=1000;Init.Markets.Add(GrainMarket);
   FHansaRouteState R;const auto& V=Init.Vehicles[1];R.Id=FHansaRouteId::TryCreate(1).Value;R.OwnerId=V.OwnerId;R.VehicleId=V.Id;R.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;R.Mode=EHansaRouteMode::Sea;R.Lifecycle=EHansaRouteLifecycleState::Inactive;
   FHansaRouteCargoAction Load;Load.Kind=EHansaRouteCargoActionKind::OwnedCityLoad;Load.GoodId=Grain;Load.QuantityLimit=FHansaQuantity::FromRaw(FullPlan?60000:1000);Load.CargoSlotIndex=0;auto Unload=Load;Unload.Kind=EHansaRouteCargoActionKind::Unload;
   R.Stops={{V.CurrentCityId,{Load}},{FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value,{Unload}}};Init.Routes.Add(R);
  },true)){AddError(Error);return false;}
  FHansaPlacementSpec Spec;Spec.CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;Spec.BuildingDefinitionId=FHansaBuildingTypeId::TryParse(TEXT("Building.TradeHouse")).Value;
  bool Found=false;for(int32 X=8;X<19&&!Found;++X)for(int32 Y=8;Y<19;++Y){Spec.Anchor={X,Y};if(Host->TradeHousePlacementError(Spec).IsEmpty()){Found=true;break;}}
  if(!Found||!Host->PlaceBuildings({Spec}).IsSuccess()){AddError(TEXT("Could not place paid test site"));return false;}
  const auto Id=Host->BuildProjection().Value.GetTradeStations()[0].Station.Id;
  {
   TArray<uint8> Current,Legacy;FHansaSaveSnapshot Snapshot;int64 Tick=0;
   TestTrue(TEXT("Capture paid site before arranging"),Host->CaptureSaveBytes(Current,TEXT("Legacy delivery"),TEXT("2026-09-30T00:00:00Z")).IsSuccess());
   if(!Host->InspectSaveBytes(Current,Snapshot,Tick).IsSuccess())return false;
   const auto* R=Host->GetEconomicRegistry();const auto Base=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(Snapshot.Scenario.ScenarioId).Value,R->GetRegistryHash(),*R);
   const auto Context=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(Snapshot.Scenario.ScenarioId).Value,R->GetRegistryHash(),*R,*Snapshot.State.CreateReadOnlyAccess(Base.Value).GetPlacement().GetTopology());
   TestTrue(TEXT("Encode previous format 23 fingerprint 35"),FHansaSaveEnvelope::EncodeHistoricalFixtureForTests(Snapshot,Context.Value,23,35,Legacy).IsSuccess());
   const auto Restored=Host->RestoreSaveBytes(Legacy);TestTrue(TEXT("Migrate real previous format"),Restored.IsSuccess());TestTrue(TEXT("Named delivery migration"),Restored.AppliedMigrations.Contains(TEXT("Hansa.Save.23To24.OneTimeConstructionDelivery")));
   TestEqual(TEXT("Migration never silently arranges a shipment"),Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryMode,uint8(0));
   const auto Hash=Host->BuildProjection().Value.GetFingerprint().Value;
   TestFalse(TEXT("Unknown delivery policy rejected atomically"),Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(2).Value,3).IsSuccess());
   TestFalse(TEXT("Cannot use rival Cog"),Host->FundTradeStation(Id,FHansaInventoryId::TryCreate(3).Value,1).IsSuccess());
   TestEqual(TEXT("Rejected arrangements preserve state"),Host->BuildProjection().Value.GetFingerprint().Value,Hash);
  }
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->SetViewerHouse(Host->GetHouseId());
  FHansaMultiplayerAuthority Authority;int64 Nonce=0;
  if(Remote){Authority.Initialize(*Host);FHansaClientInterest Interest;if(!Authority.RegisterAdmittedClient({929,FHansaParticipantId::TryCreate(1929).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
   Model->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& Draft){auto I=Draft;I.ClientSequence=Authority.GetExpectedClientSequence(929);I.ClientNonce=++Nonce;TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientCommandIntent::StaticStruct()->SerializeItem(Writer,&I,nullptr);FHansaClientCommandIntent Copy;FMemoryReader Reader(Bytes);FHansaClientCommandIntent::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);TestEqual(TEXT("Delivery policy survives command transport"),Copy.ConstructionDeliveryMode,I.ConstructionDeliveryMode);FHansaClientCommandFeedback Pending;Pending.State=EHansaClientCommandState::Pending;Pending.ClientSequence=I.ClientSequence;Pending.ClientNonce=I.ClientNonce;Model->ReceiveCommandFeedback(Pending);const auto Ack=Authority.SubmitIntent(929,Copy);Model->ReceiveCommandFeedback(Ack);return Ack.bAccepted;});
  }else Model->BindRuntime(Host.Get());
  auto Refresh=[&](){if(Remote){FHansaClientProjectionSnapshot Wire;if(!Authority.BuildProjection(929,0,true,Wire,Error)){AddError(Error);return;}TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);Model->ApplyRemoteEstablishment(Copy);}else Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());};
  Refresh();Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));Model->SelectSectionIntent(TEXT("Presence"));
  TestEqual(TEXT("Cog suggested without assigning empty station storage"),Model->GetSnapshot().Establishment.SourceId,FString(TEXT("2")));
  auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());
  TestTrue(TEXT("Single Arrange delivery action"),Screen->ActivateSemanticId(TEXT("TradeMap.Station.Action")));Refresh();
  TestEqual(TEXT("Default arrangement uses spare space"),Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryMode,uint8(1));
  TestEqual(TEXT("Delivery does not charge construction again"),Host->BuildProjection().Value.GetHouses()[0].Money.GetRawValue(),int64(150000));
  if(FullPlan){
   TestTrue(TEXT("Run default policy"),Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true).IsSuccess());Host->AdvanceTicks(19);Refresh();const auto Waiting=Host->BuildProjection().Value;TestTrue(TEXT("Default full trade loads queue construction across loops"),Waiting.GetTradeStations()[0].Station.SpentGoods.IsEmpty()&&Waiting.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty());TestTrue(TEXT("Perpetually full plan reports attention"),Model->GetSnapshot().Establishment.SourceDetail.ToString().Contains(TEXT("Waiting alone")));
   TestTrue(TEXT("Priority opens separate review"),Model->EstablishmentIntent(TEXT("Priority")));TestEqual(TEXT("Review does not change default policy"),Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryMode,uint8(1));TestTrue(TEXT("Review discloses smaller ordinary loads"),Model->GetSnapshot().Establishment.Confirmation.ToString().Contains(TEXT("smaller or skipped")));
   TestTrue(TEXT("Cancel priority review"),Model->EstablishmentIntent(TEXT("Cancel")));TestTrue(TEXT("Reopen priority review"),Model->EstablishmentIntent(TEXT("Priority")));TestTrue(TEXT("Confirm priority"),Model->EstablishmentIntent(TEXT("Confirm")));Refresh();
   TestEqual(TEXT("Only explicit confirmation changes policy"),Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryMode,uint8(2));
  }
  if(!FullPlan)TestTrue(TEXT("Resume route"),Host->SetRouteActive(FHansaRouteId::TryCreate(1).Value,true).IsSuccess());if(FullPlan){for(int32 T=0;T<20&&Host->BuildProjection().Value.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty();++T)Host->AdvanceTicks(1);}else Host->AdvanceTicks(1);
  auto P=Host->BuildProjection().Value;TestTrue(TEXT("Exact materials stay physically reserved aboard"),!P.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty());TestTrue(TEXT("Source pickup does not start destination timer"),P.GetTradeStations()[0].Station.Status==EHansaTradeStationStatus::Proposed&&P.GetTradeStations()[0].Station.CompletionTick.GetValue()==0);
  TArray<uint8> Saved;TestTrue(TEXT("Save while materials aboard"),Host->CaptureSaveBytes(Saved,TEXT("Reserved construction cargo"),TEXT("2026-09-30T00:00:00Z")).IsSuccess());const auto Hash=P.GetFingerprint().Value;TestTrue(TEXT("Restore physical reservations"),Host->RestoreSaveBytes(Saved).IsSuccess());TestEqual(TEXT("Restore preserves deterministic hash"),Host->BuildProjection().Value.GetFingerprint().Value,Hash);
  TestTrue(TEXT("Cancel while cargo aboard"),Host->CloseTradeStation(Id).IsSuccess());auto Cancelled=Host->BuildProjection().Value;const auto* Cargo=Cancelled.GetInventories().FindByPredicate([](const auto& I){return I.Id.GetValue()==2;});TestTrue(TEXT("Cancellation releases reserved physical cargo"),Cargo&&Cargo->Stocks.ContainsByPredicate([](const auto& S){return S.GoodId.ToString()==TEXT("Good.Timber")&&S.Stock.GetRawValue()==8000&&S.Reserved.GetRawValue()==0;}));TestTrue(TEXT("Restore pending order after cancellation check"),Host->RestoreSaveBytes(Saved).IsSuccess());
  Host->AdvanceTicks(40);P=Host->BuildProjection().Value;TestEqual(TEXT("Automatically delivered and constructed"),P.GetTradeStations()[0].Station.Status,EHansaTradeStationStatus::Active);TestEqual(TEXT("Both required goods delivered"),P.GetTradeStations()[0].Station.SpentGoods.Num(),2);TestEqual(TEXT("Delivery policy automatically clears"),P.GetTradeStations()[0].Station.DeliveryMode,uint8(0));TestTrue(TEXT("No lingering reservations"),P.GetTradeStations()[0].Station.DeliveryReservations.IsEmpty());TestEqual(TEXT("Normal route instructions preserved"),P.GetRoutes()[0].Stops[0].Actions[0].QuantityLimit.GetRawValue(),int64(FullPlan?60000:1000));
 }
 return !HasAnyErrors();
}
#endif
