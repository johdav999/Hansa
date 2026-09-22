#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/SHansaBuildMenu.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaPlacementFeedbackStability,
 "Hansa.UI.BuildMenu.PointerFeedbackKeepsTrayGeometry",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaPlacementFeedbackStability::RunTest(const FString&)
{
 TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
 FString Error;
 if (!TestTrue(TEXT("Catalog initializes"), Model->InitializeForLubeck(nullptr, Error))) return false;
 auto Menu = SNew(Hansa::UI::SHansaBuildMenu).Model(Model.Get());
 Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production"));
 Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Bread"));
 if (!TestTrue(TEXT("Farm selects"), Model->SelectBuilding(TEXT("Building.GrainFarm")))) return false;
 Model->TargetGridCell(10, 10);
 Menu->SlatePrepass(1.f);
 const FVector2D TargetSize = Menu->GetDesiredSize();
 const auto Feedback = Menu->ResolveSemanticWidget(TEXT("Placement.Validation"));
 const auto Cause = Menu->ResolveSemanticWidget(TEXT("Placement.Validation.Cause"));
 if (!TestTrue(TEXT("Feedback widgets exist"), Feedback.IsValid() && Cause.IsValid())) return false;
 TestTrue(TEXT("World target displays feedback"), Cause->GetVisibility().IsVisible());
 for (int32 Frame = 0; Frame < 8; ++Frame)
 {
  Model->ClearPointerTarget();
  Menu->SlatePrepass(1.f);
  TestTrue(TEXT("UI overlap preserves tray bounds"), Menu->GetDesiredSize().Equals(TargetSize));
  TestTrue(TEXT("Feedback surface continues blocking world input"), Feedback->GetVisibility().IsHitTestVisible());
  TestTrue(TEXT("Stale target text is hidden without collapsing"), Cause->GetVisibility() == EVisibility::Hidden);
  TestFalse(TEXT("UI overlap cannot confirm construction"), Model->GetSnapshot().bCanConfirm);
  TestFalse(TEXT("UI overlap has no world target"), Model->GetSnapshot().bHasTarget);
 }
 Model->TargetGridCell(10, 10);
 Menu->SlatePrepass(1.f);
 TestTrue(TEXT("Returning to world restores feedback"), Cause->GetVisibility().IsVisible());
 TestTrue(TEXT("Returning to same site keeps layout"), Menu->GetDesiredSize().Equals(TargetSize));
 Model->CancelIntent();
 TestTrue(TEXT("Cancel releases feedback space"), Feedback->GetVisibility() == EVisibility::Collapsed);
 return !HasAnyErrors();
}
#endif