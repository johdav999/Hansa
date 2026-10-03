#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaTradeLedgerTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStationMultipleOrders,"Hansa.UI.TradeMap.Orders.MultipleProducts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaStationMultipleOrders::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::UI;using namespace Hansa::Multiplayer;
 for(const bool Office:{false,true}) {
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error,EHansaTradeStationOperationalState::Active,false,Office,true,[](FHansaSimulationInitialization& Init){
   Init.MarketSettings.UpdateCadenceTicks=1;
   const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
   auto* Inventory=Init.Inventories.FindByPredicate([&](const auto& I){return I.OwnerKind==EHansaInventoryOwnerKind::City&&I.CityId==City;});
   for(const TCHAR* Name:{TEXT("Good.Grain"),TEXT("Good.Planks")}){
    const auto Good=FHansaGoodId::TryParse(Name).Value;Inventory->AcceptedGoods.Add(Good);Inventory->InitialStock.Add({Good,FHansaQuantity::FromRaw(20000)});
    FHansaCityMarketInitialization Market;Market.CityId=City;Market.GoodId=Good;Market.InventoryIds={Inventory->Id};Market.InitialPriceMilliMarks=1000;Market.InitialReportTick=1;Init.Markets.Add(Market);
   }
  })){AddError(Error);return false;}
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
  Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));Model->bWorldStationDetail=true;
  auto View=SNew(SHansaTradeMap).Model(Model.Get());
  TestTrue(TEXT("World Orders tab opens"),View->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Orders")));
  TestTrue(TEXT("Existing editor opens"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Row.2")));
  TestTrue(TEXT("All orders returns to list"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Back")));
  TestTrue(TEXT("List exposes create to controller"),View->GetControllerFocusOrder().Contains(TEXT("TradeMap.Orders.New")));
  TestFalse(TEXT("List hides editor save"),View->GetControllerFocusOrder().Contains(TEXT("TradeMap.Orders.Save")));
  TArray<int64> Created;
  for(const bool Buy:{true,false}) {
   TestTrue(TEXT("New draft opens while other orders exist"),View->ActivateSemanticId(TEXT("TradeMap.Orders.New")));
   TestEqual(TEXT("New draft has no existing order identity"),Model->GetSnapshot().SelectedStationOrderId,int64(0));
   TestTrue(TEXT("Choose independent product"),Model->SelectStationOrderGood(Buy?TEXT("Good.Grain"):TEXT("Good.Planks")));
   TestTrue(TEXT("Choose independent direction"),View->ActivateSemanticId(Buy?TEXT("TradeMap.Orders.Buy"):TEXT("TradeMap.Orders.Sell")));
   TestTrue(TEXT("Set target or reserve"),Model->SetStationOrderNumber(TEXT("Target"),Buy?TEXT("10"):TEXT("1")));
   TestTrue(TEXT("Create via authoritative gateway"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Save")));
   Created.Add(Model->GetSnapshot().SelectedStationOrderId);
   TestTrue(TEXT("All orders remains available after creation"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Back")));
  }
  TestEqual(TEXT("Two new orders retain the three existing orders"),Model->GetStationOrderRows().Num(),5);
  TestTrue(TEXT("Separate orders have distinct identities"),Created[0]>3&&Created[1]>Created[0]);
  FHansaManageStationOrderCommand Resume;Resume.StationId=FHansaTradeStationId::TryCreate(100).Value;Resume.OrderId=1;Resume.Action=EHansaStationOrderAction::Resume;
  TestTrue(TEXT("Existing sale can run beside new buy and sale"),Host->ManageStationOrder(Resume).IsSuccess());
  TestTrue(TEXT("Execute concurrent orders"),Host->AdvanceTicks(1));
  const auto Projection=Host->BuildProjection().Value;
  const auto* Station=Projection.GetTradeStations().FindByPredicate([](const auto& S){return S.Station.Id.GetValue()==100;});
  if(!TestNotNull(TEXT("Authoritative station retained"),Station))return false;
  TestEqual(TEXT("Every order remains live"),Station->Station.Orders.Num(),5);
  for(const auto& Order:Station->Station.Orders){TestFalse(TEXT("Order not implicitly cancelled"),Order.bCancelled);TestFalse(TEXT("Order not implicitly paused"),Order.bPaused);TestTrue(TEXT("Every order evaluated in same simulation update"),!Order.History.IsEmpty());if(Order.Id==uint64(Created[0])||Order.Id==uint64(Created[1]))TestTrue(TEXT("New buy and sale both transfer actual goods"),!Order.History.IsEmpty()&&Order.History.Last().AppliedMilliUnits>0);}
  TArray<uint8> Save;TestTrue(TEXT("Capture multiple orders"),Host->CaptureSaveBytes(Save,TEXT("Multiple orders isolated fixture"),TEXT("2026-10-02T00:00:00Z")).IsSuccess());TestTrue(TEXT("Reload multiple orders"),Host->RestoreSaveBytes(Save).IsSuccess());
  FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;Interest.CityIds={TEXT("City.Rostock")};FHansaClientProjectionSnapshot Wire;
  if(!Authority.RegisterAdmittedClient({1020,FHansaParticipantId::TryCreate(1020).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)||!Authority.BuildProjection(1020,0,true,Wire,Error)){AddError(Error);return false;}
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Remote(NewObject<UHansaTradeMapPresentationModel>());Remote->InitializeDefaults();Remote->ApplyRemoteEstablishment(Wire);Remote->Open();Remote->SelectCityIntent(TEXT("City.Rostock"));
  TestEqual(TEXT("Owner remote view receives all saved orders"),Remote->GetStationOrderRows().Num(),5);
  TestTrue(TEXT("Owner remote view can select independent buy"),Remote->SelectStationOrder(Created[0]));TestTrue(TEXT("Buy terms preserved"),Remote->GetStationOrderEditor().bBuy&&Remote->GetStationOrderEditor().Good==TEXT("Good.Grain"));
  TestTrue(TEXT("Owner remote view can select independent sale"),Remote->SelectStationOrder(Created[1]));TestTrue(TEXT("Sale terms preserved"),!Remote->GetStationOrderEditor().bBuy&&Remote->GetStationOrderEditor().Good==TEXT("Good.Planks"));
 }
 return !HasAnyErrors();
}
#endif
