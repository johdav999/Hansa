#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/SHansaBuildMenu.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	const Hansa::UI::FHansaHudSemanticNode* FindBuildNode(
		const TArray<Hansa::UI::FHansaHudSemanticNode>& Nodes, const TCHAR* Id)
	{
		return Nodes.FindByPredicate([Id](const Hansa::UI::FHansaHudSemanticNode& Node) { return Node.Id == Id; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaBuildMenuCatalogTest,
	"Hansa.UI.BuildMenu.CatalogAndLockedReasons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBuildMenuCatalogTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
	FString Error;
	if (!TestTrue(TEXT("The runtime Lübeck build session initializes"), Model->InitializeForLubeck(nullptr, Error))) return false;
	const FHansaBuildMenuSnapshot& Snapshot = Model->GetSnapshot();
	TestTrue(TEXT("The catalogue covers every required MVP building plus explicit category placeholders"), Snapshot.Cards.Num() >= 16);
	TSet<EHansaBuildCategory> Categories;
	bool bHasLockedReason = false;
	for (const FHansaBuildCardPresentation& Card : Snapshot.Cards)
	{
		Categories.Add(Card.Category);
		bHasLockedReason |= Card.bLocked && !Card.LockedReason.IsEmpty();
		TestFalse(FString::Printf(TEXT("%s has cost"), *Card.StableId.ToString()), Card.Cost.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has workforce/upkeep"), *Card.StableId.ToString()), Card.WorkforceAndUpkeep.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has footprint"), *Card.StableId.ToString()), Card.Footprint.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has an input/output or purpose summary"), *Card.StableId.ToString()), Card.InputOutput.IsEmpty());
	}
	TestEqual(TEXT("All seven build categories are represented"), Categories.Num(), 7);
	TestTrue(TEXT("Unavailable cards explain why they are locked"), bHasLockedReason);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaBuildMenuGatewayFlowTest,
	"Hansa.UI.BuildMenu.PlayerIntentCommandGatewayFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBuildMenuGatewayFlowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
	FString Error;
	if (!TestTrue(TEXT("The build session initializes"), Model->InitializeForLubeck(nullptr, Error))) return false;
	Model->SetOpen(true);
	TestTrue(TEXT("Road card selection is a normal presenter intent"), Model->SelectBuilding(TEXT("Building.Road")));
	TestTrue(TEXT("Click targeting provides a non-drag placement path"), Model->TargetGridCell(18, 16));
	TestTrue(TEXT("Road target is validated before submission"), Model->GetSnapshot().bCanConfirm);
	TestTrue(TEXT("Road confirmation reaches the authoritative gameplay command gateway"), Model->ConfirmIntent());
	TestEqual(TEXT("Gateway state owns the accepted road"), Model->GetPlacedBuildingCount(), 1);
	TestEqual(TEXT("Accepted player command advances authoritative time"), Model->GetSimulationTick(), int64(1));

	TestTrue(TEXT("Storage category opens"), Model->SelectCategory(EHansaBuildCategory::Storage));
	TestTrue(TEXT("Warehouse card selection begins placement"), Model->SelectBuilding(TEXT("Building.Warehouse")));
	TestTrue(TEXT("Disconnected click target is accepted as preview input"), Model->TargetGridCell(10, 10));
	TestFalse(TEXT("Disconnected warehouse cannot confirm"), Model->GetSnapshot().bCanConfirm);
	TestEqual(TEXT("Invalid footprint exposes structural error state"), Model->GetSnapshot().Feedback, EHansaPlacementFeedback::Invalid);
	TestTrue(TEXT("Invalid state has a non-color road glyph/label"), Model->GetSnapshot().ValidationCause.ToString().Contains(TEXT("Road required")));
	TestTrue(TEXT("Invalid state gives the precise remedy"), Model->GetSnapshot().ValidationRemedy.ToString().Contains(TEXT("Build next to a road")));
	TestFalse(TEXT("Rejected preview never enters the command gateway"), Model->ConfirmIntent());
	TestEqual(TEXT("Rejected preview leaves authoritative state unchanged"), Model->GetPlacedBuildingCount(), 1);

	TestTrue(TEXT("Road-adjacent targeting uses the authored warehouse footprint"), Model->TargetRoadAdjacentIntent());
	TestTrue(TEXT("Road-adjacent footprint is structurally valid"), Model->GetSnapshot().bCanConfirm);
	TestTrue(TEXT("Warehouse confirmation submits the normal player command"), Model->ConfirmIntent());
	TestEqual(TEXT("Gateway state owns road and warehouse"), Model->GetPlacedBuildingCount(), 2);
	TestEqual(TEXT("Two accepted commands advance two ticks"), Model->GetSimulationTick(), int64(2));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaBuildMenuSemanticInputTest,
	"Hansa.UI.BuildMenu.SemanticsShortcutsAndFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBuildMenuSemanticInputTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
	FString Error;
	if (!TestTrue(TEXT("The build session initializes"), Model->InitializeForLubeck(nullptr, Error))) return false;
	Model->SetOpen(true);
	TSharedRef<Hansa::UI::SHansaBuildMenu> Menu = SNew(Hansa::UI::SHansaBuildMenu).Model(Model.Get());

	TestTrue(TEXT("Category activation is semantic and device-neutral"), Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Storage")));
	TestTrue(TEXT("Warehouse card is a visible semantic action"), Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Warehouse")));
	TestTrue(TEXT("A card can receive controller focus"), Menu->FocusSemanticId(TEXT("BuildMenu.Card.Building_Warehouse")));
	TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Menu->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Warehouse = FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Warehouse"));
	TestTrue(TEXT("Selected/focused card exposes typed cost, workforce, footprint and flow"), Warehouse != nullptr &&
		Warehouse->State.bSelected && Warehouse->State.bFocused && Warehouse->State.Value.Contains(TEXT("cost=")) &&
		Warehouse->State.Value.Contains(TEXT("workforce=")) && Warehouse->State.Value.Contains(TEXT("footprint=")) &&
		Warehouse->State.Value.Contains(TEXT("flow=")));

	TestTrue(TEXT("Semantic target exposes the non-drag invalid path"), Menu->ActivateSemanticId(TEXT("Placement.Target.Disconnected")));
	Nodes = Menu->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Validation = FindBuildNode(Nodes, TEXT("Placement.Validation"));
	const Hansa::UI::FHansaHudSemanticNode* Cause = FindBuildNode(Nodes, TEXT("Placement.Validation.Cause"));
	const Hansa::UI::FHansaHudSemanticNode* Remedy = FindBuildNode(Nodes, TEXT("Placement.Validation.Remedy"));
	TestTrue(TEXT("Validation exposes error role/state, cause and remedy without pixel inference"), Validation != nullptr &&
		Validation->Role == Hansa::UI::EHansaHudSemanticRole::Alert && Validation->State.bError &&
		Cause != nullptr && Cause->State.Value.Contains(TEXT("Road required")) &&
		Remedy != nullptr && Remedy->State.Value.Contains(TEXT("Build next to a road")));

	TestTrue(TEXT("Controller focus order includes every action family"), Menu->GetControllerFocusOrder().Contains(TEXT("Placement.Action.Confirm")) &&
		Menu->GetControllerFocusOrder().Contains(TEXT("Placement.Action.Cancel")) &&
		Menu->GetControllerFocusOrder().Contains(TEXT("Placement.Overlay.Grid")));
	TestTrue(TEXT("Repeat toggle is a semantic non-drag action"), Menu->ActivateSemanticId(TEXT("Placement.Action.Repeat")));
	TestTrue(TEXT("Grid overlay state toggles semantically"), Menu->ActivateSemanticId(TEXT("Placement.Overlay.Grid")));
	Nodes = Menu->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Grid = FindBuildNode(Nodes, TEXT("Placement.Overlay.Grid"));
	TestTrue(TEXT("Overlay state has a typed boolean value"), Grid != nullptr && Grid->State.ValueType == TEXT("boolean") && Grid->State.Value == TEXT("false"));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaBuildMenuBakeryAdjacentTargetTest,
	"Hansa.UI.BuildMenu.BakeryFitsBesideRoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBuildMenuBakeryAdjacentTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
	FString Error;
	if (!TestTrue(TEXT("The build session initializes"), Model->InitializeForLubeck(nullptr, Error))) return false;
	Model->SetOpen(true);
	TestTrue(TEXT("Road card can be selected"), Model->SelectBuilding(TEXT("Building.Road")));
	TestTrue(TEXT("Road target can be selected"), Model->TargetGridCell(18, 16));
	TestTrue(TEXT("Road construction succeeds"), Model->ConfirmIntent());

	TestTrue(TEXT("Production category opens"), Model->SelectCategory(EHansaBuildCategory::Production));
	TestTrue(TEXT("Bakery card begins placement"), Model->SelectBuilding(TEXT("Building.Bakery")));
	TestEqual(TEXT("The 3-cell-wide bakery is anchored immediately left of the road"), Model->GetRoadAdjacentTarget(), FIntPoint(15, 16));
	TestTrue(TEXT("Footprint-aware adjacent targeting succeeds"), Model->TargetRoadAdjacentIntent());
	TestTrue(TEXT("The adjacent bakery enables Confirm"), Model->GetSnapshot().bCanConfirm);
	TestTrue(TEXT("Bakery construction reaches the command gateway"), Model->ConfirmIntent());
	TestEqual(TEXT("The road and bakery are both placed"), Model->GetPlacedBuildingCount(), 2);
	TestEqual(TEXT("A newly placed bakery starts under construction"),
		Model->GetBuildingWorldStatus(2), FString(TEXT("UnderConstruction")));
	TestTrue(TEXT("The runtime host advances the first 149 construction ticks"), Model->AdvanceSimulationTicks(149));
	TestEqual(TEXT("The bakery remains under construction until its authored duration elapses"),
		Model->GetBuildingWorldStatus(2), FString(TEXT("UnderConstruction")));
	TestTrue(TEXT("The runtime host advances the completion tick"), Model->AdvanceSimulationTicks(1));
	TestEqual(TEXT("The bakery becomes ready after its 150 authored construction ticks"),
		Model->GetBuildingWorldStatus(2), FString(TEXT("Ready")));
	TestEqual(TEXT("Placement ticks and construction ticks share one authoritative clock"),
		Model->GetSimulationTick(), int64(152));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaBuildMenuShorelineTargetTest,
	"Hansa.UI.BuildMenu.ShorelineBuildingsUseAuthoritativeTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBuildMenuShorelineTargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
	FString Error;
	if (!TestTrue(TEXT("The build session initializes"), Model->InitializeForLubeck(nullptr, Error))) return false;
	Model->SetOpen(true);

	TestTrue(TEXT("Harbor category opens"), Model->SelectCategory(EHansaBuildCategory::Harbor));
	TestTrue(TEXT("Dock card begins placement"), Model->SelectBuilding(TEXT("Building.Dock")));
	const TOptional<FIntPoint> DockTarget = Model->FindValidShorelineTarget();
	TestTrue(TEXT("The authoritative validator finds a shoreline footprint for the Dock"), DockTarget.IsSet());
	TestTrue(TEXT("The invalid legacy target is not reused"), !DockTarget.IsSet() || DockTarget.GetValue() != FIntPoint(23, 8));
	TestTrue(TEXT("The preferred Dock target remains beside the visible central quay"),
		!DockTarget.IsSet() ||
		FMath::Abs(DockTarget->X - 27) + FMath::Abs(DockTarget->Y - 17) <= 3);
	TestTrue(TEXT("Dock rotation can be changed before automatic shoreline targeting"), Model->RotateIntent());
	TestTrue(TEXT("Shoreline targeting enables Dock confirmation without requiring a road"), Model->TargetShorelineIntent());
	TestEqual(TEXT("Shoreline targeting derives the quay-facing orientation"),
		Model->GetSnapshot().RotationQuarterTurns, 0);
	TestTrue(TEXT("The Dock shoreline footprint is valid"), Model->GetSnapshot().bCanConfirm);
	TestTrue(TEXT("Dock construction reaches the command gateway"), Model->ConfirmIntent());

	TestTrue(TEXT("Production category opens"), Model->SelectCategory(EHansaBuildCategory::Production));
	TestTrue(TEXT("Fishery card begins placement"), Model->SelectBuilding(TEXT("Building.Fishery")));
	TestTrue(TEXT("The finder locates another unoccupied shoreline footprint"), Model->FindValidShorelineTarget().IsSet());
	TestTrue(TEXT("Fishery shoreline targeting succeeds around existing occupancy"), Model->TargetShorelineIntent());
	TestTrue(TEXT("The Fishery shoreline footprint is valid"), Model->GetSnapshot().bCanConfirm);
	TestTrue(TEXT("Fishery construction reaches the command gateway"), Model->ConfirmIntent());
	TestEqual(TEXT("The Dock and Fishery are both placed without a road"), Model->GetPlacedBuildingCount(), 2);
	return !HasAnyErrors();
}

#endif
