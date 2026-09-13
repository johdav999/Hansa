#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaBuildingStrokeTest,"Hansa.UI.BuildMenu.ClickStrokeValidation",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FHansaBuildingStrokeTest::RunTest(const FString&)
{
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> M(NewObject<UHansaBuildMenuPresentationModel>());
 FString Error;if(!TestTrue(TEXT("Initialize"),M->InitializeForLubeck(nullptr,Error)))return false;
 M->SelectBuilding(TEXT("Building.Road"));
 M->BeginRoadDraw(18,10);M->UpdateRoadDraw(18,30);
 if(!TestTrue(TEXT("Prepare a road for adjacent building strokes"),M->EndRoadDraw(true)))return false;
 M->SelectBuilding(TEXT("Building.GrainFarm"));
 TestFalse(TEXT("Invalid press cannot begin a stroke"),M->BeginBuildingStroke(-100,-100));
 const int32 Before=M->GetPlacedBuildingCount();
 M->TargetGridCell(14,12);
 TestEqual(TEXT("Passive preview spends nothing"),M->GetPlacedBuildingCount(),Before);
 if(!TestTrue(TEXT("Press commits the first building"),M->BeginBuildingStroke(14,12)))return false;
 TestTrue(TEXT("Stroke is active after successful press"),M->IsBuildingStrokeActive());
 TestEqual(TEXT("First press adds exactly one building"),M->GetPlacedBuildingCount(),Before+1);
 M->UpdateBuildingStroke(14,12);
 TestEqual(TEXT("Stationary hold never duplicates a building"),M->GetPlacedBuildingCount(),Before+1);
 M->UpdateBuildingStroke(14,24);
 const int32 After=M->GetPlacedBuildingCount();
 TestTrue(TEXT("Skipped pointer samples fill multiple valid sites along the line"),After>Before+2);
 M->UpdateBuildingStroke(14,12);
 TestEqual(TEXT("Backtracking cannot duplicate or repay footprints"),M->GetPlacedBuildingCount(),After);
 M->EndBuildingStroke();M->UpdateBuildingStroke(14,28);
 TestEqual(TEXT("Release stops construction"),M->GetPlacedBuildingCount(),After);
 TestEqual(TEXT("Selection remains available for the next click"),M->GetSnapshot().SelectedBuildingId,FName(TEXT("Building.GrainFarm")));
 M->CancelIntent();
 TestFalse(TEXT("Cancel stops stroke"),M->IsBuildingStrokeActive());
 return !HasAnyErrors();
}
#endif
