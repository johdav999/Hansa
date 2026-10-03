#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "World/HansaRuntimeSimulationHost.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityOverviewReferenceData,"Hansa.UI.CityOverviewReference.AuthoritativeData",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCityOverviewReferenceData::RunTest(const FString&) {
 using namespace Hansa::Simulation;
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());FString Error;
 if(!Host->InitializeForLubeck(nullptr,Error)){AddError(Error);return false;}
 const auto P=Host->BuildProjection();if(!P)return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> M(NewObject<UHansaTradeMapPresentationModel>());
 M->InitializeDefaults();M->BindRuntime(Host.Get());M->ApplyProjection(P.Value,*Host->GetEconomicRegistry());M->Open();M->SelectCityIntent(TEXT("City.Rostock"));
 const auto* City=M->GetSelectedCityPresentation();if(!TestNotNull(TEXT("Selected city"),City))return false;
 const auto CityId=FHansaCityDefinitionId::TryParse(TEXT("City.Rostock")).Value;
 const auto GoodId=FHansaGoodId::TryParse(M->GetSnapshot().PreferredGoodStableId.ToString()).Value;
 const auto Price=Host->QueryKnownMarketPrice(CityId,GoodId);
 const auto Supply=Host->QueryKnownMarketSupply(CityId,GoodId);
 TestEqual(TEXT("Quote uses lawful known price"),City->ReportedPriceMilliMarks,Price&&Price->PriceMilliMarks.IsSet()?Price->PriceMilliMarks.GetValue():int64(-1));
 TestEqual(TEXT("Quote uses lawful known stock"),City->ReportedStockMilliUnits,Supply&&Supply->Stock.IsSet()?Supply->Stock->GetRawValue():int64(-1));
 const auto& Summary=M->GetSnapshot().CityInspector;
 int32 Routes=0,Ships=0;
 for(const auto& R:P.Value.GetRoutes())if(R.OwnerId==Host->GetHouseId()&&R.Lifecycle!=EHansaRouteLifecycleState::Cancelled){
  if(R.Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId==CityId;}))++Routes;
  if(R.Lifecycle==EHansaRouteLifecycleState::Traveling&&R.Stops.IsValidIndex(R.NextStopIndex)&&R.Stops[R.NextStopIndex].CityId==CityId)++Ships;
 }
 TestEqual(TEXT("Owned route count"),Summary.OwnedRouteCount,Routes);TestEqual(TEXT("Approaching ship count"),Summary.ApproachingShipCount,Ships);
 TArray<uint8> Bytes;FMemoryWriter Writer(Bytes);FHansaTradeCityInspector Copy=Summary;
 FHansaTradeCityInspector::StaticStruct()->SerializeItem(Writer,&Copy,nullptr);
 Copy={};FMemoryReader Reader(Bytes);FHansaTradeCityInspector::StaticStruct()->SerializeItem(Reader,&Copy,nullptr);
 TestTrue(TEXT("Structured rights and counts survive reflected wire format"),Copy==Summary);
 FHansaTradeMapCityPresentation Unknown;TestEqual(TEXT("Missing price is unknown"),Unknown.ReportedPriceMilliMarks,int64(-1));TestEqual(TEXT("Missing stock is unknown"),Unknown.ReportedStockMilliUnits,int64(-1));
 auto Changed=*City;Changed.ReportedStockMilliUnits++;TestFalse(TEXT("Stock-only updates invalidate quote"),Changed==*City);
 auto Rights=Summary;Rights.PublicTradeAccess=Summary.PublicTradeAccess==1?0:1;TestFalse(TEXT("Rights-only changes invalidate display"),Rights==Summary);
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Private(NewObject<UHansaTradeMapPresentationModel>());Private->InitializeDefaults();Private->ApplyProjection(P.Value,*Host->GetEconomicRegistry());Private->SelectCityIntent(TEXT("City.Rostock"));
 TestEqual(TEXT("Unidentified viewer has no private route count"),Private->GetSnapshot().CityInspector.OwnedRouteCount,-1);
 TestEqual(TEXT("Unidentified viewer has no granted capability"),Private->GetSnapshot().CityInspector.PublicTradeAccess,-1);
 if(FSlateApplication::IsInitialized()){
  auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(M.Get());
  const auto Quote=Screen->ResolveSemanticWidget(TEXT("TradeMap.Overview.Price"));
  for(const TCHAR* Id:{TEXT("TradeMap.City.More"),TEXT("TradeMap.Navigate.Orders"),TEXT("TradeMap.Navigate.Ledger"),TEXT("TradeMap.Navigate.Construction"),TEXT("TradeMap.Navigate.Decisions"),TEXT("TradeMap.Navigate.Recovery")})
   TestTrue(FString(TEXT("Persistent navigation control: "))+Id,Screen->ResolveSemanticWidget(Id).IsValid());
  M->ApplyProjection(P.Value,*Host->GetEconomicRegistry());
  TestTrue(TEXT("Quote widget identity survives data refresh"),Quote==Screen->ResolveSemanticWidget(TEXT("TradeMap.Overview.Price")));
  TestFalse(TEXT("Whole reference is not the interactive widget"),Screen->ResolveSemanticWidget(TEXT("TradeMap.Navigate.Route")).IsValid());
 }
 return !HasAnyErrors();
}
#endif
