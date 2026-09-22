#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaBuildMenu.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaRandomLabourHouseTest,
    "Hansa.Compound.RandomLabourConstruction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRandomLabourHouseTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
    FString Error;
    if (!Model->InitializeForLubeck(nullptr, Error)) { AddError(Error); return false; }
    for (int32 Cycle = 0; Cycle < 2; ++Cycle)
    {
        TSet<FName> Families;
        for (int32 Choice = 0; Choice < 4; ++Choice)
        {
            if (!TestTrue(TEXT("Ordinary labour-house card chooses a compound"), Model->SelectBuilding(TEXT("Building.Residence.Laborer")))) return false;
            const FName Selected = Model->GetSnapshot().SelectedBuildingId;
            TestTrue(TEXT("Only approved starting compounds are chosen"), Selected.ToString().StartsWith(TEXT("Building.Residence.Laborer.")) && Selected.ToString().EndsWith(TEXT(".Stage1")));
            Families.Add(Selected);
            Model->TargetGridCell(-100, -100);
            TestFalse(TEXT("Invalid construction is rejected"), Model->ConfirmIntent());
            TestEqual(TEXT("Rejected placement does not reroll"), Model->GetSnapshot().SelectedBuildingId, Selected);
            Model->RotateIntent();
            TestEqual(TEXT("Rotation does not reroll"), Model->GetSnapshot().SelectedBuildingId, Selected);
            Model->CancelIntent();
        }
        TestEqual(TEXT("Every shuffle cycle includes all four families"), Families.Num(), 4);
    }
    Model->SelectBuilding(TEXT("Building.Road"));
    Model->TargetGridCell(18, 16);
    if (!TestTrue(TEXT("Place access road"), Model->ConfirmIntent())) return false;
    Model->SelectBuilding(TEXT("Building.Residence.Laborer"));
    const FName First = Model->GetSnapshot().SelectedBuildingId;
    if (!TestTrue(TEXT("Find full-sized road-facing compound footprint"), Model->TargetRoadAdjacentIntent())) return false;
    TestTrue(TEXT("Preview reserves twelve or sixteen cells, never the old four"), Model->GetSnapshot().FootprintCells.Num() == 12 || Model->GetSnapshot().FootprintCells.Num() == 16);
    const int32 Rotation = Model->GetSnapshot().RotationQuarterTurns;
    if (!TestTrue(TEXT("Continuous build commits through the normal gateway"), Model->ConfirmIntent(true))) return false;
    TestEqual(TEXT("Only road and one compound were committed"), Model->GetPlacedBuildingCount(), 2);
    TestTrue(TEXT("Next continuous placement chooses another family"), Model->GetSnapshot().SelectedBuildingId != First && !Model->GetSnapshot().SelectedBuildingId.IsNone());
    TestEqual(TEXT("Continuous placement retains road orientation"), Model->GetSnapshot().RotationQuarterTurns, Rotation);
    const FName Next = Model->GetSnapshot().SelectedBuildingId;
    Model->ToggleRepeatIntent();
    TestEqual(TEXT("Repeat toggle keeps the current preview choice"), Model->GetSnapshot().SelectedBuildingId, Next);
    Model->CancelIntent();
    const FName Explicit(TEXT("Building.Residence.Laborer.NarrowGang.Stage1"));
    TestTrue(TEXT("Explicit family remains selectable"), Model->SelectBuilding(Explicit));
    TestEqual(TEXT("Explicit family is not randomized"), Model->GetSnapshot().SelectedBuildingId, Explicit);
    Model->SelectCategory(EHansaBuildCategory::Residences);
    TSharedRef<Hansa::UI::SHansaBuildMenu> Menu = SNew(Hansa::UI::SHansaBuildMenu).Model(Model.Get());
    int32 LabourButtons = 0;
    for (const auto& Node : Menu->GetSemanticSnapshot())
        if (Node.Id.StartsWith(TEXT("BuildMenu.Card.Building_Residence_Laborer")))
            ++LabourButtons;
    TestEqual(TEXT("The tray exposes exactly one labour-house button"), LabourButtons, 1);
    TestFalse(TEXT("Hidden family cannot be activated through GUI semantics"),
        Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Residence_Laborer_NarrowGang_Stage1")));
    TestFalse(TEXT("Hidden family has no rendered button"),
        Menu->ResolveSemanticWidget(TEXT("BuildMenu.Card.Building_Residence_Laborer_NarrowGang_Stage1")).IsValid());
    Model->BeginCardDrag(TEXT("Building.Residence.Laborer"));
    TestEqual(TEXT("Random preview keeps the ordinary card selected"),
        Model->GetSelectedCardId(), FName(TEXT("Building.Residence.Laborer")));
    Model->EndCardDrag(false);
    TestEqual(TEXT("Cancelled drag returns focus to the visible card"),
        Model->GetSnapshot().FocusedSemanticId, FName(TEXT("BuildMenu.Card.Building_Residence_Laborer")));
    return !HasAnyErrors();
}
#endif
