#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaBuildMenu.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaDemolitionJourneyTest,
 "Hansa.UI.Demolition.ToolbarJourney", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaDemolitionJourneyTest::RunTest(const FString&)
{
 TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
 FString Error;
 if (!TestTrue(TEXT("Empty authoritative city initializes"), Host->InitializeForLubeck(nullptr, Error, EHansaRuntimeScenario::EmptyLubeckBuild))) { AddError(Error); return false; }
 if (!TestTrue(TEXT("Presenter initializes"), Model->InitializeForLubeck(nullptr, Host.Get(), Error))) return false;
 auto Menu = SNew(Hansa::UI::SHansaBuildMenu).Model(Model.Get());
 TestTrue(TEXT("Demolition is reachable in collapsed toolbar"), Menu->GetControllerFocusOrder().Contains(TEXT("BuildMenu.Demolition")));
 TestFalse(TEXT("Inactive tool cannot remove"), Model->DemolishBuildingIntent(1));
 TestTrue(TEXT("Select a construction tool"), Model->SelectBuilding(TEXT("Building.Road")));
 TestTrue(TEXT("Enable demolition through native semantic action"), Menu->ActivateSemanticId(TEXT("BuildMenu.Demolition")));
 TestTrue(TEXT("Demolition replaces construction"), Model->GetSnapshot().bDemolitionMode && Model->GetSnapshot().SelectedBuildingId.IsNone());
 TestFalse(TEXT("Empty ground never changes simulation"), Model->DemolishBuildingIntent(0));
 TestFalse(TEXT("Missing building is rejected"), Model->DemolishBuildingIntent(999999));
 TestFalse(TEXT("Feedback explains failure"), Model->GetSnapshot().ValidationCause.IsEmpty());
 TestTrue(TEXT("Cancel clears demolition"), Model->CancelIntent() && !Model->GetSnapshot().bDemolitionMode);
 TestTrue(TEXT("Road can be placed after cancel"), Model->SelectBuilding(TEXT("Building.Road")) && Model->TargetGridCell(18,16) && Model->ConfirmIntent());
 auto Projection = Host->BuildProjection();
 if (!TestTrue(TEXT("Placed road appears in projection"), Projection && Projection.Value.GetPlacements().Num() == 1)) return false;
 const int64 Road = static_cast<int64>(Projection.Value.GetPlacements()[0].BuildingId.GetValue());
 Model->ToggleDemolitionIntent();
 TestTrue(TEXT("Click removes the unfinished road via construction cancellation"), Model->DemolishBuildingIntent(Road));
 TestEqual(TEXT("Authoritative placement removed"), Host->GetPlacedBuildingCount(), 0);
 TestTrue(TEXT("Demolition stays active"), Model->GetSnapshot().bDemolitionMode);
 TestTrue(TEXT("Building selection exits demolition"), Model->SelectBuilding(TEXT("Building.Road")) && !Model->GetSnapshot().bDemolitionMode);
 TestTrue(TEXT("Freed footprint can be rebuilt"), Model->TargetGridCell(18,16) && Model->ConfirmIntent());
 Host->AdvanceTicks(100);
 Projection = Host->BuildProjection();
 const int64 CompletedRoad = static_cast<int64>(Projection.Value.GetPlacements()[0].BuildingId.GetValue());
 Model->ToggleDemolitionIntent();
 TestTrue(TEXT("Completed building removes through normal gateway"), Model->DemolishBuildingIntent(CompletedRoad));
 TestEqual(TEXT("Completed placement removed"), Host->GetPlacedBuildingCount(), 0);
 TArray<uint8> Bytes;
 TestTrue(TEXT("Demolished state saves"), Host->CaptureSaveBytes(Bytes,TEXT("Demolition test"),TEXT("2026-09-17T12:00:00Z")).IsSuccess());
 TestTrue(TEXT("Demolished state reloads"), Host->RestoreSaveBytes(Bytes).IsSuccess());
 TestEqual(TEXT("Reload does not resurrect building"), Host->GetPlacedBuildingCount(), 0);
 Model->SetConstructionAllowed(false);
 TestFalse(TEXT("Remote city lock cancels demolition"), Model->GetSnapshot().bDemolitionMode);
 TestFalse(TEXT("Remote city lock blocks activation"), Model->ToggleDemolitionIntent());
 Model->SetConstructionAllowed(true);
 Model->ToggleDemolitionIntent();
 Model->SelectCategory(EHansaBuildCategory::Roads);
 TestFalse(TEXT("Category selection exits demolition"), Model->GetSnapshot().bDemolitionMode);
 Model->ToggleDemolitionIntent(); Model->SetOpen(false);
 TestFalse(TEXT("Closing toolbar exits demolition"), Model->GetSnapshot().bDemolitionMode);
 return !HasAnyErrors();
}
#endif
