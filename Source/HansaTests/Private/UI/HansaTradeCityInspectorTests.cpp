#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Definitions/HansaEconomicRegistry.h"
#include "Fixtures/HansaProductionFixture.h"
#include "UI/HansaTradeCityInspector.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeCityInspectorStates,"Hansa.UI.TradeMap.CityInspector.AuthoritativeStates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeCityInspectorStates::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::UI;
 const auto Fixture=FHansaProductionFixture::TryCreateGrainShortage();if(!Fixture)return false;
 const auto P=Fixture.Value.BuildProjection();if(!P)return false;
 FHansaEconomicRegistry Registry;
 FHansaCompiledForeignPresenceStageDefinition Stage;Stage.StableId=TEXT("PresenceStage.Station");Stage.GrantedCapabilityIds={TEXT("PresenceCapability.TradeStation")};
 FHansaCompiledCityTradePolicyDefinition Policy;Policy.StableId=TEXT("CityTradePolicy.Test");Policy.CityId=TEXT("City.Test");Policy.AllowedStageIds={Stage.StableId};Policy.TradeStationSites.AddDefaulted_GetRef().SiteId=TEXT("Harbor");
 Registry.SetPresenceDefinitions({}, {Stage}, {Policy});
 const auto Viewer=FHansaHouseId::TryCreate(1).Value;
 FHansaTradeMapCityPresentation City;City.StableId=TEXT("City.Test");City.Label=FText::FromString(TEXT("A long localized city name without any Rostock assumptions"));City.bRendered=true;City.bMarketOnly=false;
 auto Build=[&]{return BuildTradeCityInspector(City.StableId,&City,P.Value,Registry,Viewer);};
 TestEqual(TEXT("Supported rendered foreign city"),Build().State,EHansaTradeCityState::RenderedForeign);
 TestTrue(TEXT("Site supported by stage and policy"),Build().bStationSupported);
 City.bBuildable=true;TestEqual(TEXT("Home/founded classification is not a city-name check"),Build().State,EHansaTradeCityState::Home);TestFalse(TEXT("Home has no foreign station action"),Build().bStationSupported);
 City.bBuildable=false;City.bUnknown=true;TestEqual(TEXT("Unknown reports are explicit"),Build().State,EHansaTradeCityState::Unknown);TestTrue(TEXT("Unknown gives remedy"),!Build().Issue.IsEmpty());
 City.bUnknown=false;City.bRendered=false;City.bMarketOnly=true;TestEqual(TEXT("Abstract city is visiting-only"),Build().State,EHansaTradeCityState::MarketOnly);TestFalse(TEXT("Abstract city cannot advertise rendered station"),Build().bStationSupported);
 City.bRendered=true;City.bMarketOnly=false;Policy.TradeStationSites.Reset();Registry.SetPresenceDefinitions({}, {Stage}, {Policy});TestEqual(TEXT("Missing site is distinct"),Build().State,EHansaTradeCityState::UnsupportedSite);
 Policy.TradeStationSites.AddDefaulted_GetRef().SiteId=TEXT("Harbor");Policy.DeniedCapabilityIds={TEXT("PresenceCapability.TradeStation")};Registry.SetPresenceDefinitions({}, {Stage}, {Policy});TestEqual(TEXT("Policy restriction is distinct"),Build().State,EHansaTradeCityState::PolicyLocked);TestFalse(TEXT("Denied station cannot be established"),Build().bStationSupported);
 TestEqual(TEXT("Missing city is unknown"),BuildTradeCityInspector(TEXT("City.Missing"),nullptr,P.Value,Registry,Viewer).State,EHansaTradeCityState::Unknown);
 TestFalse(TEXT("Generic summary never names another city"),(Build().Overview.ToString()+Build().Issue.ToString()+Build().MarketAccess.ToString()).Contains(TEXT("Rostock")));
 return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeCityInspectorSelection,"Hansa.UI.TradeMap.CityInspector.SelectionPrivacyAndRefresh",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeCityInspectorSelection::RunTest(const FString&) {
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
 const auto P=Host->BuildProjection();const auto* Registry=Host->GetEconomicRegistry();if(!P||!Registry)return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(P.Value,*Registry);Model->Open();
 const auto Cities=Model->GetSnapshot().Cities;
 for(int Pass=0;Pass<2;++Pass)for(int I=0;I<Cities.Num();++I){
  const auto& City=Cities[Pass?Cities.Num()-1-I:I];TestTrue(TEXT("Select city"),Model->SelectCityIntent(City.StableId));const auto& S=Model->GetSnapshot();
  TestEqual(TEXT("Inspector always matches selected identity"),S.CityInspector.CityId,City.StableId);
  const auto* Station=P.Value.GetTradeStations().FindByPredicate([&](const auto& X){return X.Station.CityId.ToString()==City.StableId.ToString()&&X.Station.OwnerId==Host->GetHouseId();});
  TestEqual(TEXT("Station belongs to selected city and viewer"),S.TradeStationValue,Station?int64(Station->Station.Id.GetValue()):int64(0));
  if(City.StableId!=TEXT("City.Rostock"))TestFalse(TEXT("No Rostock lock leaks to another city"),(S.StationOrderText.ToString()+S.TradeStationState.ToString()+S.TradeStationDetail.ToString()).Contains(TEXT("Rostock")));
  if(City.bBuildable){TestEqual(TEXT("Home state"),S.CityInspector.State,EHansaTradeCityState::Home);TestFalse(TEXT("No foreign establishment at home"),S.bCanTradeStationAction);}
  const auto Before=S.CityInspector;Model->ApplyProjection(P.Value,*Registry);TestTrue(TEXT("Same projection preserves summary"),Before==Model->GetSnapshot().CityInspector);
 }
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Private(NewObject<UHansaTradeMapPresentationModel>());Private->InitializeDefaults();Private->ApplyProjection(P.Value,*Registry);Private->Open();
 TestEqual(TEXT("Unidentified viewer never receives station"),Private->GetSnapshot().TradeStationValue,int64(0));
 TestFalse(TEXT("Unidentified viewer cannot upgrade"),Private->GetSnapshot().bCanPresenceUpgradeAction);
 TestTrue(TEXT("Unidentified viewer does not receive private stage"),Private->GetSnapshot().CityInspector.Presence.ToString().Contains(TEXT("unavailable for this viewer")));
 if(FSlateApplication::IsInitialized()){
  Model->SelectCityIntent(TEXT("City.Lubeck"));auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());
  const auto Identity=Screen->ResolveSemanticWidget(TEXT("TradeMap.Overview.Identity"));const auto Remedy=Screen->ResolveSemanticWidget(TEXT("TradeMap.Overview.Presence"));
  Model->ApplyProjection(P.Value,*Registry);TestTrue(TEXT("Identity widget survives update"),Identity==Screen->ResolveSemanticWidget(TEXT("TradeMap.Overview.Identity")));TestTrue(TEXT("Primary remedy survives update"),Remedy==Screen->ResolveSemanticWidget(TEXT("TradeMap.Overview.Presence")));
  TestTrue(TEXT("Home remedy uses ordinary semantic intent"),Screen->ActivateSemanticId(TEXT("TradeMap.Overview.Presence")));TestEqual(TEXT("Home remedy opens city-filtered directory"),Model->GetSnapshot().WorkspacePage,FString(TEXT("Routes")));
 }
 return !HasAnyErrors();
}
#endif
