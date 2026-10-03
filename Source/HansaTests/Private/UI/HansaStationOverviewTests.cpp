#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaTradeLedgerTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStationOverview,"Hansa.UI.TradeMap.Establishment.StationOverview",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaStationOverview::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::Multiplayer;using namespace Hansa::UI;
 for(bool Office:{false,true})for(bool Arrears:{false,true}){
  TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
  if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error,Arrears?EHansaTradeStationOperationalState::Underfunded:EHansaTradeStationOperationalState::Active,false,Office,true)){AddError(Error);return false;}
  FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;
  if(!Authority.RegisterAdmittedClient({848,FHansaParticipantId::TryCreate(1848).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)){AddError(Error);return false;}
  TStrongObjectPtr<UHansaTradeMapPresentationModel> Local(NewObject<UHansaTradeMapPresentationModel>()),Remote(NewObject<UHansaTradeMapPresentationModel>());
  Local->InitializeDefaults();Local->BindRuntime(Host.Get());Remote->InitializeDefaults();Remote->SetViewerHouse(Host->GetHouseId());
  auto Refresh=[&](){
   Local->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());FHansaClientProjectionSnapshot Wire;
   if(!Authority.BuildProjection(848,0,true,Wire,Error)){AddError(Error);return false;}
   TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Writer,&Wire,nullptr);
   FHansaClientProjectionSnapshot Copy;FMemoryReader Reader(Bytes);FHansaClientProjectionSnapshot::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);
   if(Reader.IsError()){AddError(TEXT("Owner report wire roundtrip failed"));return false;}Remote->ApplyRemoteEstablishment(Copy);return true;
  };
  if(!Refresh())return false;
  TestTrue(TEXT("Local inspector opens"),Local->OpenWorldStation(TEXT("City.Rostock"),100));TestTrue(TEXT("Remote inspector opens"),Remote->OpenWorldStation(TEXT("City.Rostock"),100));
  auto A=Local->GetStationOverviewPresentation(),B=Remote->GetStationOverviewPresentation();
  TestEqual(TEXT("Used storage comes from actual station inventory"),A.Ledger.Used,int64(15000));
  TestEqual(TEXT("Storage survives serialized owner report"),A.Storage.ToString(),B.Storage.ToString());
  TestEqual(TEXT("Order counts survive serialized owner report"),A.Trading.ToString(),B.Trading.ToString());
  TestEqual(TEXT("Paused order is separate from blocked orders"),A.Paused,1);
  TestEqual(TEXT("Active access determines running count"),A.Running,Arrears?0:2);TestEqual(TEXT("Suspended access blocks active orders"),A.Blocked,Arrears?2:0);
  TestEqual(TEXT("Suspension exposes cause and remedy"),A.Blocker.IsEmpty(),!Arrears);
  TestTrue(TEXT("No planned trade is invented as activity"),A.Activity.IsEmpty());
  TestEqual(TEXT("Owner route appears once"),A.Transport.Num(),1);TestEqual(TEXT("Owner routes survive wire roundtrip"),B.Transport.Num(),A.Transport.Num());
  if(A.Transport.Num()&&B.Transport.Num()){
   TestEqual(TEXT("Arrival availability survives wire roundtrip"),A.Transport[0].Detail.ToString(),B.Transport[0].Detail.ToString());
   TestTrue(TEXT("Inactive plan has no invented arrival"),A.Transport[0].Detail.ToString().Contains(TEXT("Arrival time unavailable")));
  }
  const auto Before=Host->BuildProjection().Value.GetFingerprint().Value;
  auto Widget=SNew(SHansaTradeMap).Model(Local.Get()).InitialViewportSize(FIntPoint(580,900));
  TestFalse(TEXT("Overview does not expose upgrade spending"),Widget->GetControllerFocusOrder().Contains(TEXT("TradeMap.Presence.Upgrade")));
  TestFalse(TEXT("Hidden upgrade action cannot spend from Overview"),Widget->ActivateSemanticId(TEXT("TradeMap.Presence.Upgrade")));
  TestTrue(TEXT("Upgrade has a direct tab"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Upgrade")));
  TestEqual(TEXT("Upgrade has a distinct section"),Local->GetSnapshot().ActiveSection,FString(TEXT("StationUpgrade")));
  TestFalse(TEXT("Upgrade hides overview stock controls"),Widget->GetControllerFocusOrder().Contains(TEXT("TradeMap.Station.Overview.Stock")));
  TestTrue(TEXT("Escape returns from Upgrade to Overview"),Widget->ActivateSemanticId(TEXT("TradeMap.Back")));
  TestEqual(TEXT("Upgrade Back keeps world inspector open"),Local->GetSnapshot().ActiveSection,FString(TEXT("Presence")));
  TestTrue(TEXT("Overview reaches actual orders"),Widget->ActivateSemanticId(TEXT("TradeMap.Station.Overview.Orders")));
  TestEqual(TEXT("Orders shortcut uses shared section"),Local->GetSnapshot().ActiveSection,FString(TEXT("Orders")));
  TestTrue(TEXT("Overview returns from Orders"),Widget->ActivateSemanticId(TEXT("TradeMap.WorldStation.Tab.Details")));
  TestTrue(TEXT("Lease rights disclose independently"),Widget->ActivateSemanticId(TEXT("TradeMap.Station.Terms")));
  TestTrue(TEXT("Back returns from lease without closing inspector"),Widget->ActivateSemanticId(TEXT("TradeMap.Back"))&&Local->bWorldStationDetail&&Local->GetSnapshot().bOpen);
  TestEqual(TEXT("Inspection and tab changes preserve simulation"),Host->BuildProjection().Value.GetFingerprint().Value,Before);
  TestTrue(TEXT("Overview stock shortcut reaches full ledger"),Widget->ActivateSemanticId(TEXT("TradeMap.Station.Overview.Stock")));
  TestFalse(TEXT("Full ledger leaves narrow world inspector"),Local->bWorldStationDetail);TestEqual(TEXT("Stock shortcut preserves read-only ledger context"),Local->GetSnapshot().ActiveSection,FString(TEXT("Ledger")));
  TestTrue(TEXT("Reopen same world inspector"),Local->OpenWorldStation(TEXT("City.Rostock"),100));
  if(!Arrears){FHansaManageStationOrderCommand Resume;Resume.StationId=FHansaTradeStationId::TryCreate(100).Value;Resume.OrderId=1;Resume.Action=EHansaStationOrderAction::Resume;TestTrue(TEXT("Resume physical sale through authority"),Host->ManageStationOrder(Resume).IsSuccess());}
  Host->AdvanceTicks(10);if(!Refresh())return false;A=Local->GetStationOverviewPresentation();B=Remote->GetStationOverviewPresentation();
  if(!Arrears)TestTrue(TEXT("Executed physical sales create recent activity"),!A.Activity.IsEmpty());
  TestEqual(TEXT("Live order results have local and remote parity"),A.Trading.ToString(),B.Trading.ToString());TestEqual(TEXT("Live receipt count has wire parity"),A.Activity.Num(),B.Activity.Num());
  for(int32 I=0;I<A.Activity.Num();++I){TestEqual(TEXT("Executed receipt has wire parity"),A.Activity[I].Detail.ToString(),B.Activity[I].Detail.ToString());TestTrue(TEXT("Receipts never use future timestamps"),A.Activity[I].Tick<=A.Ledger.Tick);}
  for(const auto& R:A.Ledger.Rows)TestEqual(TEXT("Available excludes reserved stock"),R.Available,FMath::Max<int64>(0,R.Physical-R.Reserved));
 }
 return !HasAnyErrors();
}
#endif
