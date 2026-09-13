#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/HansaRostockQuarter.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRostockContract,"Hansa.World.Rostock.Contract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRostockContract::RunTest(const FString&)
{
    TestEqual(TEXT("Dry street datum"),AHansaRostockQuarter::GroundHeight(0),100.);
    TestEqual(TEXT("Warnow bed datum"),AHansaRostockQuarter::GroundHeight(4500),-450.);
    UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);auto* Q=W->SpawnActor<AHansaRostockQuarter>();
    TestEqual(TEXT("Bounded shared modules"),Q->Modules.Num(),11);
    for(auto C:Q->Modules){TestNotNull(TEXT("Approved module resolves"),C->GetStaticMesh().Get());TestFalse(TEXT("Art never affects navigation"),C->CanEverAffectNavigation());if(C->GetStaticMesh())TestFalse(TEXT("No Engine placeholder"),C->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine/")));}
    auto* Build=NewObject<UHansaBuildMenuPresentationModel>();Build->SetConstructionAllowed(false);
    TestFalse(TEXT("Remote blocks direct card selection"),Build->SelectBuilding(TEXT("Building.Road")));
    TestFalse(TEXT("Remote blocks road start"),Build->BeginRoadDraw(18,16));TestFalse(TEXT("Remote blocks confirm"),Build->ConfirmIntent());
    Build->SetOpen(true);TestFalse(TEXT("Remote blocks opening construction"),Build->GetSnapshot().bOpen);
    W->DestroyWorld(false);return true;
}
#if WITH_HANSA_AUTOMATION
#include "Fixtures/HansaProductionFixture.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRostockTravelIntents,"Hansa.World.Rostock.TravelIntents",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRostockTravelIntents::RunTest(const FString&)
{
    auto* City=NewObject<UHansaCityOverviewPresentationModel>();City->InitializeDefaults();
    FName Visited;City->VisitRequested=[&](FName Id){Visited=Id;return true;};
    TestFalse(TEXT("Closed city report cannot travel"),City->VisitCityIntent());
    City->Open(TEXT("Test"));City->SelectCityIntent(TEXT("City.Rostock"));
    TestTrue(TEXT("City report dispatches travel"),City->VisitCityIntent());TestEqual(TEXT("Selected city identity"),Visited,FName(TEXT("City.Rostock")));
    const auto Fixture=Hansa::Simulation::FHansaProductionFixture::TryCreateGrainShortage();if(!Fixture)return false;
    const auto P=Fixture.Value.BuildProjection();const auto* Registry=Fixture.Value.GetDefinitions().GetEconomicRegistry();if(!P||!Registry)return false;
    auto* Trade=NewObject<UHansaTradeMapPresentationModel>();Trade->InitializeDefaults();Trade->ApplyProjection(P.Value,*Registry);
    Trade->VisitRequested=[&](FName Id){Visited=Id;return true;};TestFalse(TEXT("Closed route cannot travel"),Trade->VisitSelectedStopIntent());
    Trade->Open(TEXT("Test"));Trade->SelectStopIntent(1);TestTrue(TEXT("Route stop dispatches travel"),Trade->VisitSelectedStopIntent());
    TestEqual(TEXT("Visit targets selected route stop"),Visited,Trade->GetSnapshot().Stops[1].CityStableId);
    TestEqual(TEXT("Travel leaves projection unchanged"),Fixture.Value.BuildProjection().Value.GetFingerprint().Value,P.Value.GetFingerprint().Value);
    return !HasAnyErrors();
}
#endif
#endif
