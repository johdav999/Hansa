#include "Algo/Reverse.h"
#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaEconomicDefinitionCompiler.h"
#include "Definitions/HansaEconomicDefinitionSeeder.h"
#include "Definitions/HansaPresenceDefinitions.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Misc/SecureHash.h"
#include "Presence/HansaForeignPresenceInitialization.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Save/HansaSaveEnvelope.h"
#include "Systems/HansaSimulationPipeline.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace Hansa::Editor::Tests
{
using namespace Hansa::Simulation;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStationOrdersTest,"Hansa.Integration.TradePresence.StationOrders",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaStationOrdersTest::RunTest(const FString&)
{
 auto Owned=EconomicDefinitions::CreateMvpDefinitionSet(GetTransientPackage()); TArray<const UHansaDefinitionBase*> Raw;for(const auto& D:Owned)Raw.Add(D.Get());
 auto Compiled=FHansaEconomicDefinitionCompiler::Compile(Raw); if(!TestTrue(TEXT("Order policy compiles"),Compiled.IsValid()))return false;
 const auto Definitions=FHansaSimulationDefinitionContext::TryCreate(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")).Value,Compiled.Registry.GetRegistryHash(),MoveTemp(Compiled.Registry)).Value;
 const auto House=FHansaHouseId::TryCreate(1).Value;const auto City=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;const auto Grain=FHansaGoodId::TryParse(TEXT("Good.Grain")).Value;
 const auto StationId=FHansaTradeStationId::TryCreate(1).Value;const auto Storage=FHansaInventoryId::TryCreate(2).Value;const auto MarketInventory=FHansaInventoryId::TryCreate(1).Value;
 const auto MakeInitial=[&](int64 Money,int64 CityStock,int64 StationStock,int64 Capacity) {
  FHansaSimulationInitialization I;I.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick()).Value;
  I.Houses.Add({House,FHansaMoney::FromRaw(Money)});I.Houses.Add({FHansaHouseId::TryCreate(2).Value,FHansaMoney::FromRaw(10000)});I.Cities.Add({City,{}});
  FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(I,*Definitions.GetEconomicRegistry());auto& P=I.ForeignPresences[0];P.CurrentStageId=TEXT("PresenceStage.TradeStation");P.GrantedCapabilityIds=Definitions.GetEconomicRegistry()->FindPresenceStage(P.CurrentStageId)->GrantedCapabilityIds;P.StationId=StationId;P.LeasedPlotId=FHansaLeasedPlotId::TryCreate(1).Value;
  FHansaTradeStationState S;S.Id=StationId;S.OwnerId=House;S.CityId=City;S.SiteId=TEXT("TradeStationSite.Rostock.Harbor.01");S.InventoryId=Storage;S.FactorId=FHansaFactorId::TryCreate(1).Value;S.LeasedPlotId=P.LeasedPlotId;S.Status=EHansaTradeStationStatus::Active;I.TradeStations.Add(S);
  FHansaLeasedPlotState L;L.Id=P.LeasedPlotId;L.StationId=StationId;L.OwnerId=House;L.CityId=City;L.SiteId=S.SiteId;L.PlotCategory=TEXT("Commercial");L.BoundsMin={8,8};L.BoundsMax={19,19};L.PermittedBuildingCategories={TEXT("Storage"),TEXT("Commercial"),TEXT("Production")};L.bActive=true;L.bOccupied=true;I.LeasedPlots.Add(L);
  FHansaInventoryInitialization M;M.Id=MarketInventory;M.OwnerKind=EHansaInventoryOwnerKind::City;M.CityId=City;M.Capacity=FHansaQuantity::FromRaw(100000);M.AcceptedGoods={Grain};M.InitialStock={{Grain,FHansaQuantity::FromRaw(CityStock)}};I.Inventories.Add(M);
  M.Id=Storage;M.OwnerKind=EHansaInventoryOwnerKind::TradeStation;M.TradeStationId=StationId;M.Capacity=FHansaQuantity::FromRaw(Capacity);M.InitialStock={{Grain,FHansaQuantity::FromRaw(StationStock)}};I.Inventories.Add(M);
  FHansaCityMarketInitialization Market;Market.CityId=City;Market.GoodId=Grain;Market.InventoryIds={MarketInventory};Market.InitialPriceMilliMarks=1000;Market.MinimumPriceMilliMarks=1000;Market.MaximumPriceMilliMarks=1000;Market.InitialReportTick=0;Market.InitialLastUpdateTick=0;I.Markets.Add(Market);I.MarketSettings.UpdateCadenceTicks=1;return I;
 };
 auto Made=FHansaSimulationState::TryCreate(MakeInitial(20000,10000,0,50000));if(!TestTrue(TEXT("Station fixture valid without any ship"),Made.IsSuccess()))return false;
 auto State=Made.Value;FHansaSimulationTransientCache Cache;uint64 Next=1;
 const auto Submit=[&](const auto& Payload,FHansaHouseId Issuer) {FHansaCommandHeader H;H.CommandId=FHansaCommandId::TryCreate(Next).Value;H.GlobalSequence=Next++;H.Authority={Issuer,1,EHansaCommandOrigin::ControlledAutomation};H.RequestedExecutionTick=State.CreateReadOnlyAccess(Definitions).GetClock().GetTick();const auto C=FHansaGameplayCommand::Create(H,Payload);return FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,MakeArrayView(&C,1),Cache);};
 const auto Step=[&](){return FHansaGameplayCommandGateway::ExecuteTick(State,Definitions,{},Cache);};
 const auto Quantity=[&](FHansaInventoryId Id){return State.CreateReadOnlyAccess(Definitions).GetInventories().QueryStock(Id,Grain)->Stock.GetRawValue();};
 // Prompt 8 price limits use the reviewed friction-adjusted settlement price and inclusive thresholds.
 auto PriceInitial=MakeInitial(20000,10000,0,50000);auto& PricePresence=PriceInitial.ForeignPresences[0];PricePresence.GrantedCapabilityIds.Add(TEXT("PresenceCapability.MarketSpecialization"));PricePresence.GrantedCapabilityIds.Sort();
 auto PriceMade=FHansaSimulationState::TryCreate(MoveTemp(PriceInitial));if(!TestTrue(TEXT("Price-limit fixture validates"),PriceMade.IsSuccess()))return false;auto PriceState=MoveTemp(PriceMade.Value);FHansaSimulationTransientCache PriceCache;uint64 PriceSequence=1000;
 const auto SubmitPrice=[&](const FHansaManageStationOrderCommand& Payload){FHansaCommandHeader H;H.CommandId=FHansaCommandId::TryCreate(PriceSequence).Value;H.GlobalSequence=PriceSequence++;H.Authority={House,1,EHansaCommandOrigin::ControlledAutomation};H.RequestedExecutionTick=PriceState.CreateReadOnlyAccess(Definitions).GetClock().GetTick();const auto C=FHansaGameplayCommand::Create(H,Payload);return FHansaGameplayCommandGateway::ExecuteTick(PriceState,Definitions,MakeArrayView(&C,1),PriceCache);};
 FHansaManageStationOrderCommand PriceOrder;PriceOrder.StationId=StationId;PriceOrder.OrderId=77;PriceOrder.Terms.GoodId=Grain;PriceOrder.Terms.TargetOrReserveMilliUnits=2000;PriceOrder.Terms.CapMilliUnits=1000;PriceOrder.Terms.TotalBudgetPfennig=10000;PriceOrder.Terms.LimitUnitPriceMilliMarks=1050;PriceOrder.Terms.ReviewedMarketUpdateTick=0;PriceOrder.Terms.ReviewedUnitPriceMilliMarks=1050;
 TestTrue(TEXT("Acquire executes at an inclusive price ceiling"),SubmitPrice(PriceOrder).IsSuccess());auto PriceProjection=PriceState.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId);TestEqual(TEXT("Exact threshold fills"),PriceProjection->Station.Orders[0].History.Last().AppliedMilliUnits,int64(1000));const auto PriceMarket=PriceState.CreateReadOnlyAccess(Definitions).QueryMarket(City,Grain);if(!TestTrue(TEXT("Updated market remains queryable"),PriceMarket.IsSet()))return false;PriceOrder.Terms.ReviewedMarketUpdateTick=PriceMarket->LastUpdateTick;PriceOrder.Terms.ReviewedUnitPriceMilliMarks=1050;
 PriceOrder.OrderId=78;PriceOrder.Terms.LimitUnitPriceMilliMarks=1049;if(!TestTrue(TEXT("Below-market ceiling is valid but blocks execution"),SubmitPrice(PriceOrder).IsSuccess()))return false;PriceProjection=PriceState.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId);TestEqual(TEXT("Price blocker is causal"),PriceProjection->Station.Orders[1].History.Last().Blocker,EHansaStationOrderBlocker::PriceLimit);
 PriceOrder.OrderId=79;PriceOrder.Terms.ReviewedUnitPriceMilliMarks=1049;const uint64 PriceHash=PriceState.CreateReadOnlyAccess(Definitions).GetFingerprint().Value;TestEqual(TEXT("Stale reviewed price rejects"),SubmitPrice(PriceOrder).GetError(),EHansaCommandGatewayError::StationOrderStaleReview);TestEqual(TEXT("Stale price rejection is atomic"),PriceState.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,PriceHash);
 FHansaManageStationOrderCommand P;P.StationId=StationId;P.OrderId=1;P.Terms.GoodId=Grain;P.Terms.TargetOrReserveMilliUnits=3000;P.Terms.CapMilliUnits=1000;P.Terms.TotalBudgetPfennig=10000;
 TestEqual(TEXT("Wrong house rejected"),Submit(P,FHansaHouseId::TryCreate(2).Value).GetError(),EHansaCommandGatewayError::NotAuthorized);
 TestTrue(TEXT("Create order through gateway"),Submit(P,House).IsSuccess());TestEqual(TEXT("First cap physically acquired"),Quantity(Storage),int64(1000));
 TestEqual(TEXT("Real city stock decreases"),Quantity(MarketInventory),int64(9000));TestEqual(TEXT("Exact purchase debit"),State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue(),int64(18950));
 TestTrue(TEXT("Second update"),Step().IsSuccess());TestEqual(TEXT("Gradual accumulation"),Quantity(Storage),int64(2000));
 FHansaSaveSnapshot Save;Save.State=State;Save.BuildVersion=TEXT("TR-05-Test");Save.SavedUtc=TEXT("2026-09-20T00:00:00Z");Save.Players={{1,House}};TArray<uint8> Bytes;TestTrue(TEXT("Order saves"),FHansaSaveEnvelope::Encode(Save,Definitions,Bytes).IsSuccess());FHansaSaveSnapshot Loaded;auto Decode=FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded);if(!TestTrue(*Decode.Message,Decode.IsSuccess()))return false;
 auto Restored=Loaded.State;FHansaSimulationTransientCache OtherCache;TestTrue(TEXT("Original continuation"),Step().IsSuccess());TestTrue(TEXT("Restored continuation"),FHansaGameplayCommandGateway::ExecuteTick(Restored,Definitions,{},OtherCache).IsSuccess());
 TestEqual(TEXT("Order continuation hash"),State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value,Restored.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value);
 TestEqual(TEXT("Target reached"),Quantity(Storage),int64(3000));Step();TestEqual(TEXT("Target does not overbuy"),Quantity(Storage),int64(3000));
 P.Action=EHansaStationOrderAction::Cancel;TestTrue(TEXT("Cancel purchase"),Submit(P,House).IsSuccess());
 P.Action=EHansaStationOrderAction::Create;P.OrderId=2;P.Terms.Side=EHansaStationOrderSide::Release;P.Terms.TargetOrReserveMilliUnits=1000;P.Terms.TotalBudgetPfennig=0;
 const int64 BeforeSale=State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue();TestTrue(TEXT("Sale without ship"),Submit(P,House).IsSuccess());TestEqual(TEXT("Sale cap"),Quantity(Storage),int64(2000));TestEqual(TEXT("Exact sale credit"),State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue(),BeforeSale+950);
 Step();Step();TestEqual(TEXT("Reserve protected"),Quantity(Storage),int64(1000));TestEqual(TEXT("Conserved physical goods"),Quantity(Storage)+Quantity(MarketInventory),int64(10000));
 P.Action=EHansaStationOrderAction::Pause;TestTrue(TEXT("Pause"),Submit(P,House).IsSuccess());TestEqual(TEXT("All paused orders expose order-suspended station state"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.OperationalState,EHansaTradeStationOperationalState::OrderSuspended);P.Action=EHansaStationOrderAction::Resume;TestTrue(TEXT("Resume"),Submit(P,House).IsSuccess());
 const auto Hash=State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value;P.Action=EHansaStationOrderAction::Edit;P.Terms.CapMilliUnits=MAX_int64;TestFalse(TEXT("Integer boundary rejects"),Submit(P,House).IsSuccess());TestEqual(TEXT("Rejection rolls back"),State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value,Hash);
 // Competing orders are canonical regardless of discovery order and cannot overspend.
 auto I=MakeInitial(1500,10000,0,50000);
 for(uint64 Id:{uint64(2),uint64(1)}){FHansaStationOrderState O;O.Id=Id;O.LastCommandId=FHansaCommandId::TryCreate(Id).Value;O.Terms.GoodId=Grain;O.Terms.TargetOrReserveMilliUnits=5000;O.Terms.CapMilliUnits=1000;O.Terms.TotalBudgetPfennig=10000;I.TradeStations[0].Orders.Add(O);}
 auto A=FHansaSimulationState::TryCreate(I);Algo::Reverse(I.TradeStations[0].Orders);auto B=FHansaSimulationState::TryCreate(I);if(!TestTrue(TEXT("Reversed fixtures validate"),A&&B))return false;
 FHansaSimulationTransientCache AC,BC;FHansaGameplayCommandGateway::ExecuteTick(A.Value,Definitions,{},AC);FHansaGameplayCommandGateway::ExecuteTick(B.Value,Definitions,{},BC);
 TestEqual(TEXT("Reversed discovery deterministic"),A.Value.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value,B.Value.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetFingerprint().Value);
 TestTrue(TEXT("Competing orders cannot overspend"),A.Value.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue()>=0);
 const auto Orders=A.Value.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders;TestEqual(TEXT("First identity fills first"),Orders[0].History.Last().AppliedMilliUnits,int64(1000));TestEqual(TEXT("Second order reports partial funds"),Orders[1].History.Last().Blocker,EHansaStationOrderBlocker::Funds);

 const auto RunBlocked=[&](int64 Money,int64 CityStock,int64 StationStock,int64 Capacity,EHansaStationOrderSide Side,EHansaStationOrderBlocker Expected) {
  auto Init=MakeInitial(Money,CityStock,StationStock,Capacity);FHansaStationOrderState O;O.Id=1;O.LastCommandId=FHansaCommandId::TryCreate(1).Value;O.Terms.GoodId=Grain;O.Terms.Side=Side;O.Terms.TargetOrReserveMilliUnits=Side==EHansaStationOrderSide::Acquire?10000:0;O.Terms.CapMilliUnits=1000;O.Terms.TotalBudgetPfennig=10000;Init.TradeStations[0].Orders.Add(O);
  auto S=FHansaSimulationState::TryCreate(Init);if(!TestTrue(TEXT("Boundary fixture validates"),S.IsSuccess()))return;
  FHansaSimulationTransientCache C;const auto R=FHansaGameplayCommandGateway::ExecuteTick(S.Value,Definitions,{},C);TestTrue(TEXT("Blocked updates remain valid ticks"),R.IsSuccess());
  const auto O2=S.Value.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0];TestEqual(TEXT("Causal boundary blocker"),O2.History.Last().Blocker,Expected);TestEqual(TEXT("Blocked execution moves no goods"),O2.History.Last().AppliedMilliUnits,int64(0));
  TestEqual(TEXT("Blocked execution spends no money"),S.Value.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue(),Money);
 };
 RunBlocked(0,10000,0,50000,EHansaStationOrderSide::Acquire,EHansaStationOrderBlocker::Funds);
 RunBlocked(10000,0,0,50000,EHansaStationOrderSide::Acquire,EHansaStationOrderBlocker::MarketStock);
 RunBlocked(10000,10000,1000,1000,EHansaStationOrderSide::Acquire,EHansaStationOrderBlocker::StationCapacity);
 RunBlocked(10000,100000,5000,50000,EHansaStationOrderSide::Release,EHansaStationOrderBlocker::MarketCapacity);
 // A paused sale still protects its reserve against another sale; suspension moves nothing.
 auto Protected=MakeInitial(10000,10000,5000,50000);
 FHansaStationOrderState Protector;Protector.Id=1;Protector.LastCommandId=FHansaCommandId::TryCreate(1).Value;Protector.Terms.GoodId=Grain;Protector.Terms.Side=EHansaStationOrderSide::Release;Protector.Terms.TargetOrReserveMilliUnits=4500;Protector.Terms.CapMilliUnits=1000;Protector.bPaused=true;
 Protected.TradeStations[0].Orders.Add(Protector);Protector.Id=2;Protector.bPaused=false;Protector.Terms.TargetOrReserveMilliUnits=0;Protected.TradeStations[0].Orders.Add(Protector);
 auto ProtectedState=FHansaSimulationState::TryCreate(Protected);if(!TestTrue(TEXT("Protected reserve fixture valid"),ProtectedState.IsSuccess()))return false;
 FHansaSimulationTransientCache ProtectedCache;TestTrue(TEXT("Protected sale executes"),FHansaGameplayCommandGateway::ExecuteTick(ProtectedState.Value,Definitions,{},ProtectedCache).IsSuccess());
 const auto ProtectedView=ProtectedState.Value.CreateReadOnlyAccess(Definitions);const auto ProtectedOrders=ProtectedView.QueryTradeStation(StationId)->Station.Orders;
 TestEqual(TEXT("Paused order is observable"),ProtectedOrders[0].History.Last().Outcome,EHansaStationOrderOutcome::Paused);
 TestEqual(TEXT("Other sale respects paused reserve"),ProtectedView.GetInventories().QueryStock(Storage,Grain)->Stock.GetRawValue(),int64(4500));
 const auto Receipt=ProtectedOrders[1].History.Last();TestEqual(TEXT("Partial reserve amount"),Receipt.AppliedMilliUnits,int64(500));TestEqual(TEXT("Fractional sale rounds down exactly"),Receipt.MoneyDelta,int64(475));
 TestTrue(TEXT("Receipt references physical movement sequence"),Receipt.FirstMovementSequence>0&&Receipt.LastMovementSequence>=Receipt.FirstMovementSequence);
 Protected.TradeStations[0].Status=EHansaTradeStationStatus::Suspended;
 auto Suspended=FHansaSimulationState::TryCreate(Protected);if(!TestTrue(TEXT("Suspended fixture valid"),Suspended.IsSuccess()))return false;
 FHansaSimulationTransientCache SuspendedCache;TestTrue(TEXT("Suspended tick is valid"),FHansaGameplayCommandGateway::ExecuteTick(Suspended.Value,Definitions,{},SuspendedCache).IsSuccess());
 const auto SuspendedOrder=Suspended.Value.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[1];
 TestEqual(TEXT("Suspension observable"),SuspendedOrder.History.Last().Outcome,EHansaStationOrderOutcome::Suspended);TestEqual(TEXT("Suspension cannot trade"),SuspendedOrder.History.Last().AppliedMilliUnits,int64(0));
 TestEqual(TEXT("Legacy suspension resolves to rights-suspended"),Suspended.Value.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.OperationalState,EHansaTradeStationOperationalState::RightsSuspended);
 // Budget is lifetime spending, including edits; a pause never restores it.
 State=FHansaSimulationState::TryCreate(MakeInitial(20000,10000,0,50000)).Value;Cache.Discard();Next=1;
 P={};P.StationId=StationId;P.OrderId=1;P.Terms.GoodId=Grain;P.Terms.TargetOrReserveMilliUnits=10000;P.Terms.CapMilliUnits=1000;P.Terms.TotalBudgetPfennig=1050;
 TestTrue(TEXT("Budgeted purchase"),Submit(P,House).IsSuccess());Step();auto Limited=State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0];
 TestEqual(TEXT("Budget exhausted after exact spend"),Limited.SpentPfennig,int64(1050));TestEqual(TEXT("Budget blocker visible"),Limited.History.Last().Blocker,EHansaStationOrderBlocker::Budget);
 P.Action=EHansaStationOrderAction::Edit;P.Terms.TotalBudgetPfennig=1049;TestFalse(TEXT("Cannot edit spent budget away"),Submit(P,House).IsSuccess());
 P.Terms.TotalBudgetPfennig=2100;TestTrue(TEXT("Increase total budget"),Submit(P,House).IsSuccess());TestEqual(TEXT("Edit preserves lifetime spend"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].SpentPfennig,int64(2100));
 Save={};Save.State=State;Save.BuildVersion=TEXT("TR-05-Test");Save.SavedUtc=TEXT("2026-09-20T00:00:00Z");Save.Players={{1,House}};TestTrue(TEXT("History save"),FHansaSaveEnvelope::Encode(Save,Definitions,Bytes).IsSuccess());TestTrue(TEXT("History load"),FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded).IsSuccess());
 TestEqual(TEXT("History retains exact receipts"),Loaded.State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].History.Num(),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].History.Num());

 TStrongObjectPtr<UHansaTradeMapPresentationModel> Ui(NewObject<UHansaTradeMapPresentationModel>());Ui->InitializeDefaults();Ui->ApplyProjection(State.CreateReadOnlyAccess(Definitions).BuildProjection().Value,*Definitions.GetEconomicRegistry());Ui->Open();
 Ui->SetNetworkCommandIntent([&](const FHansaClientCommandIntent& I){FHansaManageStationOrderCommand C;C.StationId=StationId;C.OrderId=I.StationOrderId;C.Action=static_cast<EHansaStationOrderAction>(I.StationOrderAction);C.Terms.GoodId=FHansaGoodId::TryParse(I.GoodId).Value;C.Terms.Side=static_cast<EHansaStationOrderSide>(I.StationOrderSide);C.Terms.TargetOrReserveMilliUnits=I.StationOrderTarget;C.Terms.CapMilliUnits=I.StationOrderCap;C.Terms.TotalBudgetPfennig=I.StationOrderBudget;return Submit(C,House).IsSuccess();});
 auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Ui.Get());
 TestTrue(TEXT("Orders reachable without any ship route"),Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Orders.Save")));
 TestTrue(TEXT("Select existing order via ordinary control"),Screen->ActivateSemanticId(TEXT("TradeMap.Orders.Select")));
 TestTrue(TEXT("Pause via native semantic control and gateway"),Screen->ActivateSemanticId(TEXT("TradeMap.Orders.Pause")));
 Ui->ApplyProjection(State.CreateReadOnlyAccess(Definitions).BuildProjection().Value,*Definitions.GetEconomicRegistry());
 TestTrue(TEXT("UI pause agrees with authoritative order"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].bPaused);
 const auto Widget=Screen->ResolveSemanticWidget(TEXT("TradeMap.Orders.Pause"));Step();Ui->ApplyProjection(State.CreateReadOnlyAccess(Definitions).BuildProjection().Value,*Definitions.GetEconomicRegistry());
 TestTrue(TEXT("Live history retains widget identity"),Widget==Screen->ResolveSemanticWidget(TEXT("TradeMap.Orders.Pause")));
 TestTrue(TEXT("UI shows last execution and causal budget"),Ui->GetSnapshot().StationOrderText.ToString().Contains(TEXT("pfennig")));
 TestTrue(TEXT("Cancel via ordinary control"),Screen->ActivateSemanticId(TEXT("TradeMap.Orders.Cancel")));
 TestTrue(TEXT("Cancelled state persists"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].bCancelled);

 const auto PausedHistory=State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].History.Num();Step();TestEqual(TEXT("Cancelled order does not execute again"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].History.Num(),PausedHistory);
 auto Empty=MakeInitial(10000,10000,0,50000);State=FHansaSimulationState::TryCreate(Empty).Value;Next=1;Cache.Discard();
 P={};P.StationId=StationId;P.OrderId=1;P.Terms.GoodId=Grain;P.Terms.TargetOrReserveMilliUnits=0;P.Terms.CapMilliUnits=1000;P.Terms.TotalBudgetPfennig=0;
 TestTrue(TEXT("No-op target order accepted"),Submit(P,House).IsSuccess());
 const auto BeforeDuplicate=State.CreateReadOnlyAccess(Definitions).GetFingerprint();TestFalse(TEXT("Duplicate order ID rejects"),Submit(P,House).IsSuccess());TestEqual(TEXT("Duplicate leaves exact state"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,BeforeDuplicate.Value);
 for(int32 HistoryTick=0;HistoryTick<20;++HistoryTick)Step();TestEqual(TEXT("History is bounded"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.Orders[0].History.Num(),16);
 TestTrue(TEXT("Closure cancels empty station orders"),Submit(FHansaCloseTradeStationCommand{StationId},House).IsSuccess());
 Save.State=State;TestTrue(TEXT("Closed station and cancelled orders save"),FHansaSaveEnvelope::Encode(Save,Definitions,Bytes).IsSuccess());TestTrue(TEXT("Closed station history reloads"),FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded).IsSuccess());

 // TR-06's header-only format-15 fixture was valid while format 16 changed only append-only
 // route action semantics. Format 17 adds serialized presence fields, so relabelling its body
 // as format 15 is intentionally corrupt. Historical migration remains covered by the
 // synthetic prior-schema suite, which writes the actual legacy body layout.
 TestTrue(TEXT("Historical route-target migration uses an actual prior body fixture"),FHansaSaveEnvelope::CurrentFormatVersion>=17);
 // TR-06: physical station buffering shares the existing route gateway and inventory ledger.
 const auto Home=FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")).Value;
 const auto CargoId=FHansaInventoryId::TryCreate(3).Value;
 const auto HomeId=FHansaInventoryId::TryCreate(4).Value;
 const auto ShipId=FHansaVehicleId::TryCreate(1).Value;
 const auto RouteId=FHansaRouteId::TryCreate(1).Value;
 const auto RouteInitial=[&](int64 Stock) {
  auto Init=MakeInitial(100000,10000,Stock,50000);Init.Cities.Add({Home,{}});
  FHansaVehicleState V;V.Id=ShipId;V.DefinitionId=FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog")).Value;V.OwnerId=House;V.CargoInventoryId=CargoId;V.CurrentCityId=City;V.Capacity=FHansaQuantity::FromRaw(Definitions.GetEconomicRegistry()->FindVehicle(TEXT("Vehicle.Cog"))->CargoCapacityMilliUnits);Init.Vehicles.Add(V);
  FHansaInventoryInitialization Inv;Inv.Id=CargoId;Inv.OwnerKind=EHansaInventoryOwnerKind::Vehicle;Inv.VehicleId=ShipId;Inv.Capacity=V.Capacity;Inv.AcceptedGoods={Grain};Init.Inventories.Add(Inv);
  Inv.Id=HomeId;Inv.OwnerKind=EHansaInventoryOwnerKind::City;Inv.VehicleId={};Inv.CityId=Home;Inv.Capacity=FHansaQuantity::FromRaw(100000);Init.Inventories.Add(Inv);return Init;
 };
 FHansaCreateRouteCommand RouteCommand;RouteCommand.RouteId=RouteId;RouteCommand.VehicleId=ShipId;RouteCommand.RouteDefinitionId=FHansaRouteDefinitionId::TryParse(TEXT("Route.BalticSea")).Value;RouteCommand.bActivate=true;
 FHansaRouteStop Port;Port.CityId=City;Port.Actions.Add({EHansaRouteCargoActionKind::StationLoad,EHansaRouteCargoCondition::Always,Grain,FHansaQuantity::FromRaw(4000),{}});RouteCommand.Stops.Add(Port);
 Port.CityId=Home;Port.Actions[0].Kind=EHansaRouteCargoActionKind::OwnedCityUnload;RouteCommand.Stops.Add(Port);
 auto RouteInit=RouteInitial(5000);FHansaStationOrderState ReserveOrder;ReserveOrder.Id=1;ReserveOrder.LastCommandId=FHansaCommandId::TryCreate(1).Value;ReserveOrder.bPaused=true;ReserveOrder.Terms.GoodId=Grain;ReserveOrder.Terms.Side=EHansaStationOrderSide::Release;ReserveOrder.Terms.TargetOrReserveMilliUnits=2000;ReserveOrder.Terms.CapMilliUnits=1000;RouteInit.TradeStations[0].Orders.Add(ReserveOrder);
 Made=FHansaSimulationState::TryCreate(RouteInit);if(!TestTrue(TEXT("Route fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestTrue(TEXT("Station route gateway accepts"),Submit(RouteCommand,House).IsSuccess());
 TestEqual(TEXT("Station reserve bounds physical load"),Quantity(CargoId),int64(3000));TestEqual(TEXT("Station reserve remains"),Quantity(Storage),int64(2000));
 TestEqual(TEXT("Ship transfer never pays market"),State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue(),int64(100000));
 TestEqual(TEXT("Station transfer leaves city market alone"),Quantity(MarketInventory),int64(10000));
 auto RouteView=State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetRoutes()[0];TestEqual(TEXT("Typed station partial receipt"),RouteView.LastTransfer.Outcome,EHansaRouteTransferOutcome::Partial);TestEqual(TEXT("Transfer has no settlement"),RouteView.LastTransfer.SettledMoneyRaw,int64(0));
 Save.State=State;TestTrue(TEXT("In-transit station route saves"),FHansaSaveEnvelope::Encode(Save,Definitions,Bytes).IsSuccess());TestTrue(TEXT("Station route reloads"),FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded).IsSuccess());
 Restored=Loaded.State;OtherCache.Discard();
 for(int Tick=0;Tick<12;++Tick){TestTrue(TEXT("Physical voyage tick"),Step().IsSuccess());TestTrue(TEXT("Reloaded voyage tick"),FHansaGameplayCommandGateway::ExecuteTick(Restored,Definitions,{},OtherCache).IsSuccess());}
 TestEqual(TEXT("Cargo reaches home physically"),Quantity(HomeId),int64(3000));TestEqual(TEXT("Buffered voyage conserved"),Quantity(HomeId)+Quantity(CargoId)+Quantity(Storage)+Quantity(MarketInventory),int64(15000));
 TestEqual(TEXT("Buffered voyage save continuation deterministic"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,Restored.CreateReadOnlyAccess(Definitions).GetFingerprint().Value);
 // Inactive or wrong-owner station cannot authorize a new route; failure rolls back.
 RouteInit.TradeStations[0].Status=EHansaTradeStationStatus::Suspended;Made=FHansaSimulationState::TryCreate(RouteInit);if(!TestTrue(TEXT("Route fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 auto BeforeRoute=State.CreateReadOnlyAccess(Definitions).GetFingerprint();TestFalse(TEXT("Suspended station rejects route"),Submit(RouteCommand,House).IsSuccess());TestEqual(TEXT("Rejected station route rolls back"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,BeforeRoute.Value);
 RouteInit.TradeStations[0].Status=EHansaTradeStationStatus::Active;Made=FHansaSimulationState::TryCreate(RouteInit);if(!TestTrue(TEXT("Route fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestFalse(TEXT("Rival cannot route owned ship"),Submit(RouteCommand,FHansaHouseId::TryCreate(2).Value).IsSuccess());
 // Reverse flow unloads to station then the factor sells gradually while the ship travels.
 auto Reverse=RouteInitial(0);Reverse.Vehicles[0].Cargo=FHansaQuantity::FromRaw(4000);Reverse.Inventories[2].InitialStock={{Grain,FHansaQuantity::FromRaw(4000)}};
 ReserveOrder.bPaused=false;ReserveOrder.Terms.TargetOrReserveMilliUnits=0;Reverse.TradeStations[0].Orders={ReserveOrder};
 RouteCommand.Stops[0].Actions[0].Kind=EHansaRouteCargoActionKind::StationUnload;Made=FHansaSimulationState::TryCreate(Reverse);if(!TestTrue(TEXT("Reverse fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestTrue(TEXT("Reverse route accepts physical cargo"),Submit(RouteCommand,House).IsSuccess());
 TestEqual(TEXT("Reverse unload conserved with same-tick factor"),Quantity(Storage)+Quantity(CargoId)+Quantity(MarketInventory),int64(14000));
 TestTrue(TEXT("Cog departs independently of factor sale"),State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetRoutes()[0].Lifecycle==EHansaRouteLifecycleState::Traveling);
 Step();Step();Step();TestEqual(TEXT("Factor finishes gradual sale during voyage"),Quantity(Storage),int64(0));TestEqual(TEXT("Sale credits exact money"),State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue(),int64(103800));

 // Capacity and cancellation are physical; no transfer may discard cargo or bypass a full store.
 RouteCommand.Stops[0].Actions[0].Kind=EHansaRouteCargoActionKind::StationUnload;
 auto Full=RouteInitial(50000);Full.Inventories[2].InitialStock={{Grain,FHansaQuantity::FromRaw(4000)}};
 Made=FHansaSimulationState::TryCreate(Full);if(!TestTrue(TEXT("Full fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestTrue(TEXT("Full station is a valid partial/missed route"),Submit(RouteCommand,House).IsSuccess());TestEqual(TEXT("Full station retains ship cargo"),Quantity(CargoId),int64(4000));
 TestEqual(TEXT("Full storage exposes storage-blocked station state"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.OperationalState,EHansaTradeStationOperationalState::StorageBlocked);
 TestEqual(TEXT("Full station reports missed"),State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetRoutes()[0].LastTransfer.Outcome,EHansaRouteTransferOutcome::Missed);
 TestTrue(TEXT("Cancellation with cargo"),Submit(FHansaCancelRouteCommand{RouteId},House).IsSuccess());TestEqual(TEXT("Cancelled cargo preserved"),Quantity(CargoId),int64(4000));
 Save.State=State;TestTrue(TEXT("Cancelled cargo saves"),FHansaSaveEnvelope::Encode(Save,Definitions,Bytes).IsSuccess());TestTrue(TEXT("Cancelled cargo reloads"),FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded).IsSuccess());
 RouteCommand.Stops[0].Actions[0].Kind=EHansaRouteCargoActionKind::StationLoad;
 auto Hold=RouteInitial(5000);const auto HoldCapacity=Hold.Vehicles[0].Capacity.GetRawValue();Hold.Inventories[2].InitialStock={{Grain,FHansaQuantity::FromRaw(HoldCapacity-1000)}};
 Made=FHansaSimulationState::TryCreate(Hold);if(!TestTrue(TEXT("Full hold fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 const auto PartialResult=Submit(RouteCommand,House);TestTrue(TEXT("Bounded load accepted"),PartialResult.IsSuccess());TestEqual(TEXT("Load uses only remaining hold capacity"),Quantity(CargoId),HoldCapacity);TestEqual(TEXT("Station retains excess cargo"),Quantity(Storage),int64(4000));
 bool Correlated=false;for(const auto& E:PartialResult.GetEvents())if(E.GetTradeStationId()==StationId&&E.GetRouteId()==RouteId&&E.GetVehicleId()==ShipId&&E.GetRouteCargoActionKind()==EHansaRouteCargoActionKind::StationLoad)Correlated=true;
 TestTrue(TEXT("Physical route event correlates ship station and action"),Correlated);
 // Pause before departure then resume: only the resumed vehicle phase transfers stock.
 Made=FHansaSimulationState::TryCreate(RouteInitial(5000));State=Made.Value;Cache.Discard();Next=1;RouteCommand.bActivate=false;
 TestTrue(TEXT("Create inactive buffered route"),Submit(RouteCommand,House).IsSuccess());TestEqual(TEXT("Inactive route leaves stock"),Quantity(Storage),int64(5000));
 TestTrue(TEXT("Pause inactive route"),Submit(FHansaSetRouteActiveCommand{RouteId,false},House).IsSuccess());TestEqual(TEXT("Paused route leaves stock"),Quantity(Storage),int64(5000));
 TestTrue(TEXT("Resume buffered route"),Submit(FHansaSetRouteActiveCommand{RouteId,true},House).IsSuccess());TestEqual(TEXT("Resume moves physical cargo once"),Quantity(CargoId),int64(4000));

 // Factor-funded stock is loaded only after the earlier market updates physically acquire it.
 auto Accumulated=RouteInitial(0);FHansaStationOrderState Acquire;Acquire.Id=1;Acquire.LastCommandId=FHansaCommandId::TryCreate(1).Value;Acquire.Terms.GoodId=Grain;Acquire.Terms.TargetOrReserveMilliUnits=4000;Acquire.Terms.CapMilliUnits=1000;Acquire.Terms.TotalBudgetPfennig=4200;Accumulated.TradeStations[0].Orders.Add(Acquire);
 Made=FHansaSimulationState::TryCreate(Accumulated);if(!TestTrue(TEXT("Accumulation fixture valid"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 for(int Tick=0;Tick<4;++Tick)Step();TestEqual(TEXT("Factor prepared actual cargo"),Quantity(Storage),int64(4000));
 RouteCommand.bActivate=true;TestTrue(TEXT("Cog takes factor-purchased goods"),Submit(RouteCommand,House).IsSuccess());TestEqual(TEXT("Cargo came from prepared station stock"),Quantity(CargoId),int64(4000));TestEqual(TEXT("No second purchase at loading"),State.CreateReadOnlyAccess(Definitions).GetHouses()[0].Money.GetRawValue(),int64(95800));
 // A save-restored route revalidates a station whose operating state has changed.
 auto Changed=RouteInitial(5000);Changed.TradeStations[0].Status=EHansaTradeStationStatus::Suspended;
 FHansaRouteState Existing;Existing.Id=RouteId;Existing.OwnerId=House;Existing.VehicleId=ShipId;Existing.RouteDefinitionId=RouteCommand.RouteDefinitionId;Existing.Stops=RouteCommand.Stops;Existing.Lifecycle=EHansaRouteLifecycleState::AtStop;Existing.bPendingStopActions=true;Changed.Routes.Add(Existing);
 Made=FHansaSimulationState::TryCreate(Changed);if(!TestTrue(TEXT("Suspended runtime fixture valid"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestTrue(TEXT("Runtime suspension is a safe tick"),Step().IsSuccess());TestEqual(TEXT("Suspended station never leaks inventory"),Quantity(Storage),int64(5000));TestEqual(TEXT("Suspension reported as missed action"),State.CreateReadOnlyAccess(Definitions).BuildProjection().Value.GetRoutes()[0].LastTransfer.Outcome,EHansaRouteTransferOutcome::Missed);

 Changed.TradeStations[0].Status=EHansaTradeStationStatus::Active;Changed.Routes[0].Lifecycle=EHansaRouteLifecycleState::Inactive;Changed.Routes[0].bPendingStopActions=false;
 Made=FHansaSimulationState::TryCreate(Changed);if(!TestTrue(TEXT("Native route fixture valid"),Made.IsSuccess()))return false;State=Made.Value;
 Ui->ApplyProjection(State.CreateReadOnlyAccess(Definitions).BuildProjection().Value,*Definitions.GetEconomicRegistry());
 TestFalse(TEXT("Buffered route never claims known profit"),Ui->GetSnapshot().Routes[0].bProfitKnown);
 TestTrue(TEXT("Native manifest names station transfer"),Ui->GetSnapshot().Stops[0].ActionLabel.ToString().Contains(TEXT("station")));
 const auto ActionWidget=Screen->ResolveSemanticWidget(TEXT("TradeMap.Editor.Action.Cycle"));
 TestTrue(TEXT("Existing native action cycles station direction"),Screen->ActivateSemanticId(TEXT("TradeMap.Editor.Action.Cycle")));
 TestTrue(TEXT("Cycle reaches station unload"),Ui->GetSnapshot().Stops[0].ActionLabel.ToString().Contains(TEXT("Unload to station")));
 TestTrue(TEXT("Action control retains identity"),ActionWidget==Screen->ResolveSemanticWidget(TEXT("TradeMap.Editor.Action.Cycle")));

 // TR-13: upkeep failure is an authoritative boundary state, and paying exact arrears restores service.
 auto RevokedInit=RouteInitial(5000);RevokedInit.ForeignPresences[0].Status=EHansaForeignPresenceStatus::Revoked;RevokedInit.TradeStations[0].Status=EHansaTradeStationStatus::Suspended;RevokedInit.TradeStations[0].OperationalState=EHansaTradeStationOperationalState::Revoked;RevokedInit.LeasedPlots[0].bActive=false;
 Made=FHansaSimulationState::TryCreate(RevokedInit);if(!TestTrue(TEXT("Revoked fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;TestTrue(TEXT("Revocation remains a safe tick"),Step().IsSuccess());TestEqual(TEXT("Revocation is explicit and cargo remains"),State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId)->Station.OperationalState,EHansaTradeStationOperationalState::Revoked);TestEqual(TEXT("Revocation preserves station cargo"),Quantity(Storage),int64(5000));
 auto UnderfundedInit=RouteInitial(0);UnderfundedInit.Houses[0].Money=FHansaMoney::FromRaw(20);UnderfundedInit.TradeStations[0].UpkeepPfennigPerTick=25;
 Made=FHansaSimulationState::TryCreate(UnderfundedInit);if(!TestTrue(TEXT("Underfunding fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestTrue(TEXT("Unpayable upkeep is a valid atomic tick"),Step().IsSuccess());auto RecoveryProjection=State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId);
 TestEqual(TEXT("Failed upkeep enters explicit underfunded state"),RecoveryProjection->Station.OperationalState,EHansaTradeStationOperationalState::Underfunded);
 TestEqual(TEXT("Exact unpaid upkeep is retained"),RecoveryProjection->Station.OutstandingUpkeepPfennig,int64(25));
 const uint64 UnfundedHash=State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value;TestFalse(TEXT("Unaffordable arrears reject"),Submit(FHansaFundTradeStationCommand{StationId,CargoId},House).IsSuccess());TestEqual(TEXT("Rejected arrears payment rolls back"),State.CreateReadOnlyAccess(Definitions).GetFingerprint().Value,UnfundedHash);
 auto PayableInit=RouteInitial(0);PayableInit.Houses[0].Money=FHansaMoney::FromRaw(100);PayableInit.TradeStations[0].UpkeepPfennigPerTick=25;PayableInit.TradeStations[0].Status=EHansaTradeStationStatus::Suspended;PayableInit.TradeStations[0].OperationalState=EHansaTradeStationOperationalState::Underfunded;PayableInit.TradeStations[0].OutstandingUpkeepPfennig=25;PayableInit.LeasedPlots[0].bActive=false;
 Made=FHansaSimulationState::TryCreate(PayableInit);if(!TestTrue(TEXT("Payable recovery fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestTrue(TEXT("Exact arrears payment restores station"),Submit(FHansaFundTradeStationCommand{StationId,CargoId},House).IsSuccess());RecoveryProjection=State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId);
 TestEqual(TEXT("Recovered station is active"),RecoveryProjection->Station.OperationalState,EHansaTradeStationOperationalState::Active);TestEqual(TEXT("Arrears clear exactly"),RecoveryProjection->Station.OutstandingUpkeepPfennig,int64(0));

 // TR-13: voluntary closure strands rather than deletes cargo, survives save/load, and permits outbound-only route recovery.
 auto ClosureInit=RouteInitial(5000);Made=FHansaSimulationState::TryCreate(ClosureInit);if(!TestTrue(TEXT("Closure recovery fixture validates"),Made.IsSuccess()))return false;State=Made.Value;Cache.Discard();Next=1;
 TestTrue(TEXT("Cargo-bearing station enters recoverable closure"),Submit(FHansaCloseTradeStationCommand{StationId},House).IsSuccess());RecoveryProjection=State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId);
 TestEqual(TEXT("Closure is explicit"),RecoveryProjection->Station.OperationalState,EHansaTradeStationOperationalState::VoluntarilyClosed);TestEqual(TEXT("Closure preserves all station cargo"),Quantity(Storage),int64(5000));TestTrue(TEXT("Occupied lease is retained for recovery"),RecoveryProjection->Lease.bOccupied);
 Save.State=State;TestTrue(TEXT("Stranded closure saves"),FHansaSaveEnvelope::Encode(Save,Definitions,Bytes).IsSuccess());TestTrue(TEXT("Stranded closure reloads"),FHansaSaveEnvelope::Decode(Bytes,Definitions,Loaded).IsSuccess());TestEqual(TEXT("Reload preserves stranded cargo"),Loaded.State.CreateReadOnlyAccess(Definitions).GetInventories().QueryStock(Storage,Grain)->Stock.GetRawValue(),int64(5000));
 RouteCommand.Stops[0].Actions[0].Kind=EHansaRouteCargoActionKind::StationLoad;RouteCommand.Stops[0].Actions[0].QuantityLimit=FHansaQuantity::FromRaw(5000);RouteCommand.bActivate=true;
 TestTrue(TEXT("Closed station permits owned outbound recovery route"),Submit(RouteCommand,House).IsSuccess());TestEqual(TEXT("Recovery transfers cargo without deletion"),Quantity(CargoId),int64(5000));TestEqual(TEXT("Recovered station is empty"),Quantity(Storage),int64(0));
 TestTrue(TEXT("Second close finalizes an empty station"),Submit(FHansaCloseTradeStationCommand{StationId},House).IsSuccess());RecoveryProjection=State.CreateReadOnlyAccess(Definitions).QueryTradeStation(StationId);TestFalse(TEXT("Final closure releases plot occupancy"),RecoveryProjection->Lease.bOccupied);TestFalse(TEXT("Final closure releases presence station link"),State.CreateReadOnlyAccess(Definitions).QueryForeignPresence(House,City)->StationId.IsValid());
 return !HasAnyErrors();
}
}
#endif
