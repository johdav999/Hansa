#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HansaTradeEstablishmentTestSupport.h"
#include "UI/HansaTradeEstablishment.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPresenceCalendarTest,"Hansa.UI.TradeMap.Establishment.CalendarAndBlockedReference",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaPresenceCalendarTest::RunTest(const FString&) {
 using namespace Hansa::Simulation;using namespace Hansa::UI;
 const FString Culture=FInternationalization::Get().GetCurrentCulture()->GetName();
 FInternationalization::Get().SetCurrentCulture(TEXT("en"));
 ON_SCOPE_EXIT { FInternationalization::Get().SetCurrentCulture(Culture); };
 TestEqual(TEXT("Default duration in hours"),PresenceDuration(3,60).ToString(),FString(TEXT("3 hours")));
 TestEqual(TEXT("Calendar rate is not hardcoded"),PresenceDuration(3,30).ToString(),FString(TEXT("1 h 30 min")));
 TestEqual(TEXT("Daily duration"),PresenceDuration(48,60).ToString(),FString(TEXT("2 days")));
 TestEqual(TEXT("Unknown clock is not assumed"),PresenceDailyUpkeep(25,0).ToString(),FString(TEXT("Unavailable")));
 TestEqual(TEXT("Daily rate conversion"),PresenceDailyUpkeep(25,60).ToString(),FString(TEXT("600 pfennig / day")));
 TestEqual(TEXT("Half-hour rate conversion"),PresenceDailyUpkeep(25,30).ToString(),FString(TEXT("1,200 pfennig / day")));
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)||!Hansa::Tests::PrepareEstablishment(*Host,Error,100000,[](FHansaSimulationInitialization& I){
  I.Clock=FHansaSimulationClock::TryCreate(FHansaSimulationVersion::TryCreate(1).Value,FHansaSimulationTick::TryCreate(1).Value,30).Value;
  for(auto& P:I.ForeignPresences)P.Contributions={};
 })){AddError(Error);return false;}
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());Model->InitializeDefaults();Model->SetViewerHouse(Host->GetHouseId());Model->BindRuntime(Host.Get());Model->ApplyProjection(Host->BuildProjection().Value,*Host->GetEconomicRegistry());Model->Open();Model->SelectCityIntent(TEXT("City.Rostock"));
 auto View=SNew(SHansaTradeMap).Model(Model.Get());View->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence"));
 TestTrue(TEXT("Explicit site selection"),View->ActivateSemanticId(TEXT("TradeMap.Station.Site")));
 const auto& E=Model->GetSnapshot().Establishment;
 TestEqual(TEXT("Six requirements"),E.Requirements.Num(),6);TestEqual(TEXT("Six unmet requirements"),E.UnmetRequirements,6);
 TestEqual(TEXT("Actual clock reaches presentation"),E.MinutesPerTick,30);
 TestEqual(TEXT("Actual clock used for construction"),E.BuildDuration.ToString(),FString(TEXT("1 h 30 min to build")));
 TestTrue(TEXT("Actual clock used for operation"),E.Requirements.ContainsByPredicate([](const auto& Q){return Q.Id==TEXT("ReliableOperation")&&Q.Value.ToString()==TEXT("0 / 1.5 hours");}));
 TestTrue(TEXT("Volume uses human units, not milli-units"),E.Requirements.ContainsByPredicate([](const auto& Q){return Q.Id==TEXT("LawfulTradeVolume")&&Q.Value.ToString()==TEXT("0 / 50 units");}));
 TestTrue(TEXT("Owned public market available"),E.bCanOpenMarket);TestFalse(TEXT("Blocked proposal stays blocked"),E.bCanPropose);
 TestFalse(TEXT("Funding step unavailable before reservation"),Model->CanEstablishmentIntent(TEXT("Source")));
 TestTrue(TEXT("No implicit inventory"),E.SourceId.IsEmpty());TestFalse(TEXT("No developer time in visible terms"),E.Summary.ToString().Contains(TEXT("tick")));
 const auto Before=Host->BuildProjection().Value.GetFingerprint().Value;
 TestFalse(TEXT("Cannot submit blocked proposal"),View->ActivateSemanticId(TEXT("TradeMap.Station.Action")));
 FName RequestedCity;Model->MarketRequested=[&](FName City,FName){RequestedCity=City;return true;};
 TestTrue(TEXT("Useful next action works"),View->ActivateSemanticId(TEXT("TradeMap.Station.Market")));TestEqual(TEXT("Market retains selected city"),RequestedCity,FName(TEXT("City.Rostock")));
 TestEqual(TEXT("Inspection and market navigation spend nothing"),Host->BuildProjection().Value.GetFingerprint().Value,Before);
 FHansaTradeEstablishment Copy;TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaTradeEstablishment::StaticStruct()->SerializeItem(Writer,const_cast<FHansaTradeEstablishment*>(&E),nullptr);FMemoryReader Reader(Bytes);FHansaTradeEstablishment::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);
 TestFalse(TEXT("Structured review deserializes"),Reader.IsError());TestEqual(TEXT("Requirement rows survive wire"),Copy.Requirements.Num(),6);TestEqual(TEXT("Clock survives wire"),Copy.MinutesPerTick,30);TestEqual(TEXT("Daily upkeep survives wire"),Copy.DailyUpkeep.ToString(),E.DailyUpkeep.ToString());
 return !HasAnyErrors();
}
#endif
