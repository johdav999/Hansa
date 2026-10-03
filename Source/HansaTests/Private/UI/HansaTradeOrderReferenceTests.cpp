#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HansaTradeLedgerTestSupport.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Network/HansaMultiplayerAuthority.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaOrderReferenceContract,"Hansa.UI.TradeMap.Orders.ReferenceContract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaOrderReferenceContract::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::UI;using namespace Hansa::Multiplayer;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareLedger(*Host,Error,EHansaTradeStationOperationalState::Active,false,true)){AddError(Error);return false;}
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
 Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));
 auto View=SNew(SHansaTradeMap).Model(Model.Get());View->ActivateSemanticId(TEXT("TradeMap.Navigate.Orders"));View->ActivateSemanticId(TEXT("TradeMap.Orders.New"));
 TestTrue(TEXT("Choose actual charcoal"),Model->SelectStationOrderGood(TEXT("Good.Charcoal")));
 TestTrue(TEXT("Explicit Buy is idempotent"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Buy"))&&View->ActivateSemanticId(TEXT("TradeMap.Orders.Buy"))&&Model->GetStationOrderEditor().bBuy);
 TestTrue(TEXT("Draft summary identifies chosen good"),Model->GetStationOrderEditor().Summary.ToString().Contains(TEXT("Charcoal")));
 const auto Before=Model->GetSnapshot();TestFalse(TEXT("Reject invalid number"),Model->SetStationOrderNumber(TEXT("Target"),TEXT("invalid")));TestEqual(TEXT("Rejected input preserves target"),Model->GetSnapshot().StationOrderTargetInput,Before.StationOrderTargetInput);
 TestTrue(TEXT("Fractional quantities accepted"),Model->SetStationOrderNumber(TEXT("Target"),TEXT("9.125")));
 TestTrue(TEXT("Sell changes intent without changing identity"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Sell"))&&!Model->GetStationOrderEditor().bBuy&&Model->GetStationOrderEditor().Good==TEXT("Good.Charcoal"));
 TestTrue(TEXT("Sell explains preserved reserve"),Model->GetStationOrderEditor().TargetLabel.ToString().Contains(TEXT("reserve")));
 TestFalse(TEXT("Sell budget controls unavailable"),Model->CanStationOrderAction(TEXT("Budget.Increase")));
 TestTrue(TEXT("Create goes through real station gateway"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Save")));
 const int64 Created=Model->GetSnapshot().SelectedStationOrderId;TestTrue(TEXT("Authoritative order selected after creation"),Created>3);
 TestFalse(TEXT("Existing native Buy button is disabled"),View->ResolveSemanticWidget(TEXT("TradeMap.Orders.Buy"))->IsEnabled());TestFalse(TEXT("Existing native Sell button is disabled"),View->ResolveSemanticWidget(TEXT("TradeMap.Orders.Sell"))->IsEnabled());
 TestFalse(TEXT("Existing good cannot change"),Model->SelectStationOrderGood(TEXT("Good.Timber")));
 TestFalse(TEXT("Existing direction cannot change"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Buy")));
 TestTrue(TEXT("Pause real order"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Pause")));TestTrue(TEXT("Paused state visible"),Model->GetStationOrderEditor().bPaused);
 TestTrue(TEXT("Resume real order"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Pause")));TestFalse(TEXT("Resumed state visible"),Model->GetStationOrderEditor().bPaused);
 TestTrue(TEXT("Cancellation opens review"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Cancel")));TestTrue(TEXT("First activation preserves order"),Model->GetSnapshot().bConfirmStationOrderCancel&&!Model->GetStationOrderEditor().bCancelled);
 TestTrue(TEXT("Reviewed cancellation"),View->ActivateSemanticId(TEXT("TradeMap.Orders.Cancel")));TestTrue(TEXT("Cancelled state retained"),Model->GetStationOrderEditor().bCancelled);
 FHansaMultiplayerAuthority Authority;Authority.Initialize(*Host);FHansaClientInterest Interest;Interest.CityIds={TEXT("City.Rostock")};FHansaClientProjectionSnapshot Wire;
 if(!Authority.RegisterAdmittedClient({1018,FHansaParticipantId::TryCreate(1018).Value,Host->GetHouseId(),EHansaAdmissionMode::LanOffline},Interest,Error)||!Authority.BuildProjection(1018,0,true,Wire,Error)){AddError(Error);return false;}
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Remote(NewObject<UHansaTradeMapPresentationModel>());Remote->InitializeDefaults();Remote->ApplyRemoteEstablishment(Wire);Remote->Open();Remote->SelectCityIntent(TEXT("City.Rostock"));
 TestTrue(TEXT("Remote selects same authoritative order"),Remote->SelectStationOrder(uint64(Created)));
 TestTrue(TEXT("Owner remote state matches cancelled order"),Remote->GetStationOrderEditor().bCancelled);
 TestEqual(TEXT("Owner remote summary matches local"),Remote->GetStationOrderEditor().Summary.ToString(),Model->GetStationOrderEditor().Summary.ToString());
 TestEqual(TEXT("Owner remote physical stock matches local"),Remote->GetStationOrderEditor().Stock.ToString(),Model->GetStationOrderEditor().Stock.ToString());
 return !HasAnyErrors();
}
#endif
