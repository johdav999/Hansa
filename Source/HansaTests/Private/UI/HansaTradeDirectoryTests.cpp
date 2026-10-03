#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Fixtures/HansaProductionFixture.h"
#include "Definitions/HansaEconomicRegistry.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaTradeMap.h"
#include "Framework/Application/SlateApplication.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeDirectoryProjectionTest,"Hansa.UI.TradeMap.Directory.PrivacySelectionAndFilters",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FTradeDirectoryProjectionTest::RunTest(const FString&)
{
 using namespace Hansa::Simulation;
 const auto Fixture=FHansaProductionFixture::TryCreateGrainShortage();if(!Fixture)return false;
 const auto P=Fixture.Value.BuildProjection();const auto* Registry=Fixture.Value.GetDefinitions().GetEconomicRegistry();if(!P||!Registry||P.Value.GetRoutes().IsEmpty())return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
 Model->InitializeDefaults();Model->ApplyProjection(P.Value,*Registry);
 TestTrue(TEXT("Unknown viewer never owns directory rows"),!Model->GetSnapshot().Directory.ContainsByPredicate([](const auto& E){return E.bOwned;}));
 TestTrue(TEXT("Unknown viewer has no cargo instructions"),Model->GetSnapshot().Stops.IsEmpty());
 TestTrue(TEXT("Unknown viewer sees explicit privacy"),Model->GetSnapshot().Directory[0].Detail.ToString().Contains(TEXT("Cargo private")));
 TestTrue(TEXT("Unverified owner cannot change routes"),!Model->GetSnapshot().Routes.ContainsByPredicate([](const auto& R){return R.bCanCancel||R.bCanToggleActive;}));
 Model->SetViewerHouse(P.Value.GetRoutes()[0].OwnerId);Model->ApplyProjection(P.Value,*Registry);Model->Open();
 const auto Selected=Model->GetSnapshot().SelectedRouteValue;
 TestTrue(TEXT("Owned route has editable instructions"),!Model->GetDraftStops().IsEmpty());
 Model->AdjustQuantityIntent(5000);const auto Draft=Model->GetDraftStops();const auto Revision=Model->GetRevision();
 TestTrue(TEXT("Reselect handled"),Model->SelectRouteIntent(Selected));
 TestEqual(TEXT("Reselect does not publish"),Model->GetRevision(),Revision);
 TestTrue(TEXT("Reselect preserves dirty draft"),Model->GetSnapshot().bDirty&&Model->GetDraftStops()[0].Actions[0].QuantityLimit==Draft[0].Actions[0].QuantityLimit);
 TestFalse(TEXT("Changing views cannot discard edits"),Model->DirectoryIntent(TEXT("Fleet")));
 Model->InitializeDefaults();Model->ApplyProjection(P.Value,*Registry);Model->Open();
 TestTrue(TEXT("Fleet view available"),Model->DirectoryIntent(TEXT("Fleet")));
 TestEqual(TEXT("Each projected vehicle has one fleet row"),Model->GetSnapshot().Directory.Num(),P.Value.GetVehicles().Num());
 TSet<int64> Ids;for(const auto& E:Model->GetSnapshot().Directory){TestFalse(TEXT("No duplicated fleet identities"),Ids.Contains(E.VehicleValue));Ids.Add(E.VehicleValue);}
 const auto Vehicle=Model->GetSnapshot().Directory[0].VehicleValue;
 TestTrue(TEXT("Fleet selects connected route"),Model->SelectFleetIntent(Vehicle));
 TestEqual(TEXT("Fleet selection persists"),Model->GetSnapshot().SelectedVehicleValue,Vehicle);
 Model->ApplyProjection(P.Value,*Registry);TestEqual(TEXT("Live update retains selection"),Model->GetSnapshot().SelectedVehicleValue,Vehicle);
 Model->SetCitySearchIntent(TEXT("no-such-directory-entry"));TestTrue(TEXT("Search filters directory"),Model->GetSnapshot().Directory.IsEmpty());
 TestEqual(TEXT("Search leaves selected route intact"),Model->GetSnapshot().SelectedRouteValue,Selected);
 Model->SetCitySearchIntent(TEXT(""));Model->DirectoryIntent(TEXT("Routes"));
 if(FSlateApplication::IsInitialized()) {
  auto Screen=SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get());
  const auto Id=Model->GetSnapshot().Directory[0].SemanticId;
  const auto Row=Screen->ResolveSemanticWidget(Id);
  Model->ApplyProjection(P.Value,*Registry);
  TestTrue(TEXT("Live refresh preserves row widget identity"),Row.IsValid()&&Row==Screen->ResolveSemanticWidget(Id));
  TestTrue(TEXT("Fleet switch has stable semantic control"),Screen->ResolveSemanticWidget(TEXT("TradeMap.Directory.Fleet")).IsValid());
 }
 return !HasAnyErrors();
}
#endif
