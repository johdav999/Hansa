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

	const FHansaBuildCardPresentation* FindCard(const FHansaBuildMenuSnapshot& Snapshot, const TCHAR* StableId)
	{
		return Snapshot.Cards.FindByPredicate(
			[StableId](const FHansaBuildCardPresentation& Card) { return Card.StableId == StableId; });
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
	TestEqual(TEXT("The catalogue contains the sixteen authored construction cards"), Snapshot.Cards.Num(), 16);
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
	TestEqual(TEXT("Only the five authored MVP categories are represented"), Categories.Num(), 5);
	TestEqual(TEXT("Slate category presentation is derived from those five authored categories"), Snapshot.Categories.Num(), 5);
	TestTrue(TEXT("Locked authored cards explain the causal prerequisite"), bHasLockedReason);
	TestNull(TEXT("Hidden post-MVP Smithy is not materialized as a Slate card"), FindCard(Snapshot, TEXT("Building.Smithy")));
	TestNotNull(TEXT("The player-buildable Brewery is materialized as a Slate card"), FindCard(Snapshot, TEXT("Building.Brewery")));
	TestEqual(TEXT("The production selector exposes beer, bread, fish, and planks"), Snapshot.ProductionChains.Num(), 4);
	TestEqual(TEXT("Bread is the deterministic initial chain"), Snapshot.SelectedProductionChainOutputGoodId, FName(TEXT("Good.Bread")));
	TestEqual(TEXT("Beer is first in stable output-good order"), Snapshot.ProductionChains[0].OutputGoodId, FName(TEXT("Good.Beer")));
	TestEqual(TEXT("Bread is second in stable output-good order"), Snapshot.ProductionChains[1].OutputGoodId, FName(TEXT("Good.Bread")));
	TestEqual(TEXT("Fish is third in stable output-good order"), Snapshot.ProductionChains[2].OutputGoodId, FName(TEXT("Good.Fish")));
	TestEqual(TEXT("Planks is fourth in stable output-good order"), Snapshot.ProductionChains[3].OutputGoodId, FName(TEXT("Good.Planks")));

	const FHansaBuildCardPresentation* GrainFarm = FindCard(Snapshot, TEXT("Building.GrainFarm"));
	const FHansaBuildCardPresentation* Mill = FindCard(Snapshot, TEXT("Building.Mill"));
	const FHansaBuildCardPresentation* Bakery = FindCard(Snapshot, TEXT("Building.Bakery"));
	TestTrue(TEXT("Bread expands to Grain Farm, Mill, Bakery in authored stage order"),
		GrainFarm != nullptr && GrainFarm->MenuOrder == 0 && GrainFarm->ProductionChainOutputGoodId == TEXT("Good.Bread") &&
		Mill != nullptr && Mill->MenuOrder == 1 && Mill->ProductionChainOutputGoodId == TEXT("Good.Bread") &&
		Bakery != nullptr && Bakery->MenuOrder == 2 && Bakery->ProductionChainOutputGoodId == TEXT("Good.Bread"));
	const FHansaBuildCardPresentation* HopFarm = FindCard(Snapshot, TEXT("Building.HopFarm"));
	const FHansaBuildCardPresentation* MaltHouse = FindCard(Snapshot, TEXT("Building.MaltHouse"));
	const FHansaBuildCardPresentation* Cooperage = FindCard(Snapshot, TEXT("Building.Cooperage"));
	const FHansaBuildCardPresentation* Brewery = FindCard(Snapshot, TEXT("Building.Brewery"));
	const FHansaBuildChainPresentation* BeerChain = Snapshot.ProductionChains.FindByPredicate(
		[](const FHansaBuildChainPresentation& Chain) { return Chain.OutputGoodId == TEXT("Good.Beer"); });
	TestTrue(TEXT("Beer expands to four authored construction stages"),
		HopFarm != nullptr && HopFarm->MenuOrder == 0 &&
		MaltHouse != nullptr && MaltHouse->MenuOrder == 1 &&
		Cooperage != nullptr && Cooperage->MenuOrder == 2 &&
		Brewery != nullptr && Brewery->MenuOrder == 3 && Brewery->ProductionChainOutputGoodId == TEXT("Good.Beer") &&
		BeerChain != nullptr && BeerChain->StageCount == 4);
	TestTrue(TEXT("Brewery card exposes all three inputs and its beer output"), Brewery != nullptr &&
		Brewery->InputOutput.ToString().Contains(TEXT("3 malt")) &&
		Brewery->InputOutput.ToString().Contains(TEXT("1 hops")) &&
		Brewery->InputOutput.ToString().Contains(TEXT("1 barrels")) &&
		Brewery->InputOutput.ToString().Contains(TEXT("5 beer")));
	TestTrue(TEXT("Grain Farm cost is derived from its current definition"), GrainFarm != nullptr &&
		GrainFarm->Cost.ToString().Contains(TEXT("1200 pf")) && GrainFarm->Cost.ToString().Contains(TEXT("3 timber")) &&
		GrainFarm->Cost.ToString().Contains(TEXT("0.5 tools")));
	const FHansaBuildCardPresentation* ArtisanResidence = FindCard(Snapshot, TEXT("Building.Residence.Artisan"));
	TestTrue(TEXT("Upgrade-only residence is locked with a causal reason"), ArtisanResidence != nullptr &&
		ArtisanResidence->bLocked && ArtisanResidence->LockedReason.ToString().Contains(TEXT("Upgrade")));
	TestFalse(TEXT("Locked content cannot begin a direct placement intent"), Model->SelectBuilding(TEXT("Building.Residence.Artisan")));
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
	TestTrue(TEXT("Production category activation exposes the default bread chain"), Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Production")));
	TestTrue(TEXT("End product click opens bread recipe"),Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Bread")));
	TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Menu->GetSemanticSnapshot();
	TestNotNull(TEXT("Bread chain has a semantic selector"), FindBuildNode(Nodes, TEXT("BuildMenu.Chain.Good_Bread")));
	TestNotNull(TEXT("Bread selection exposes Grain Farm"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_GrainFarm")));
	TestNotNull(TEXT("Bread selection exposes Mill"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Mill")));
	TestNotNull(TEXT("Bread selection exposes Bakery"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Bakery")));
	TestNull(TEXT("Bread selection does not expose Fishery"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Fishery")));
	TestTrue(TEXT("Beer chain activation is semantic and device-neutral"), Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Beer")));
	Nodes = Menu->GetSemanticSnapshot();
	TestNotNull(TEXT("Beer selection exposes Hop Farm"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_HopFarm")));
	TestNotNull(TEXT("Beer selection exposes Malt House"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_MaltHouse")));
	TestNotNull(TEXT("Beer selection exposes Cooperage"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Cooperage")));
	TestNotNull(TEXT("Beer selection exposes Brewery"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Brewery")));
	TestNull(TEXT("Beer selection does not expose Bakery"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Bakery")));
	TestTrue(TEXT("Fish chain activation is semantic and device-neutral"), Menu->ActivateSemanticId(TEXT("BuildMenu.Chain.Good_Fish")));
	Nodes = Menu->GetSemanticSnapshot();
	TestNotNull(TEXT("Fish selection exposes Fishery"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Fishery")));
	TestNull(TEXT("Fish selection no longer exposes Bakery"), FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Bakery")));

	TestTrue(TEXT("Category activation is semantic and device-neutral"), Menu->ActivateSemanticId(TEXT("BuildMenu.Category.Storage")));
	TestTrue(TEXT("Warehouse card is a visible semantic action"), Menu->ActivateSemanticId(TEXT("BuildMenu.Card.Building_Warehouse")));
	TestTrue(TEXT("A card can receive controller focus"), Menu->FocusSemanticId(TEXT("BuildMenu.Card.Building_Warehouse")));
	Nodes = Menu->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Warehouse = FindBuildNode(Nodes, TEXT("BuildMenu.Card.Building_Warehouse"));
	TestTrue(TEXT("Selected/focused card exposes typed cost, workforce, footprint and flow"), Warehouse != nullptr &&
		Warehouse->State.bSelected && Warehouse->State.bFocused && Warehouse->State.Value.Contains(TEXT("cost=")) &&
		Warehouse->State.Value.Contains(TEXT("workforce=")) && Warehouse->State.Value.Contains(TEXT("footprint=")) &&
		Warehouse->State.Value.Contains(TEXT("flow=")));

	TestFalse(TEXT("Shipping semantics expose no fixed disconnected test target"),
		Menu->ActivateSemanticId(TEXT("Placement.Target.Disconnected")));
	TestTrue(TEXT("A real card drag creates the selected stable-definition placement session"),
		Model->BeginCardDrag(TEXT("Building.Warehouse")));
	TestTrue(TEXT("Pointer movement supplies the invalid viewport cell"), Model->UpdateCardDragTarget(10, 10));
	Nodes = Menu->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Validation = FindBuildNode(Nodes, TEXT("Placement.Validation"));
	const Hansa::UI::FHansaHudSemanticNode* Cause = FindBuildNode(Nodes, TEXT("Placement.Validation.Cause"));
	const Hansa::UI::FHansaHudSemanticNode* Remedy = FindBuildNode(Nodes, TEXT("Placement.Validation.Remedy"));
	TestTrue(TEXT("Validation exposes error role/state, cause and remedy without pixel inference"), Validation != nullptr &&
		Validation->Role == Hansa::UI::EHansaHudSemanticRole::Alert && Validation->State.bError &&
		Cause != nullptr && Cause->State.Value.Contains(TEXT("Road required")) &&
		Remedy != nullptr && Remedy->State.Value.Contains(TEXT("Build next to a road")));
	const Hansa::UI::FHansaHudSemanticNode* Viewport = FindBuildNode(Nodes, TEXT("Placement.Viewport"));
	TestTrue(TEXT("Viewport placement is semantically inspectable without pixel inference"), Viewport != nullptr &&
		Viewport->State.Value.Contains(TEXT("dragging=true")) && Viewport->State.Value.Contains(TEXT("anchor=10,10")) &&
		Viewport->State.Value.Contains(TEXT("cells=")));

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
	FHansaBuildMenuDragJourneyTest,
	"Hansa.UI.BuildMenu.ViewportDragJourney",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaBuildMenuDragJourneyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
	FString Error;
	if (!TestTrue(TEXT("The drag journey initializes an authoritative build session"), Model->InitializeForLubeck(nullptr, Error))) return false;
	Model->SetOpen(true);
	TestTrue(TEXT("Dragging a stable card creates a placement session"), Model->BeginCardDrag(TEXT("Building.Road")));
	TestTrue(TEXT("The model reports an active card drag"), Model->GetSnapshot().bDraggingCard);
	TestTrue(TEXT("Moving across the viewport targets the snapped grid cell"), Model->UpdateCardDragTarget(18, 16));
	TestEqual(TEXT("The viewport target is retained as a typed grid coordinate"), Model->GetSnapshot().AnchorCell, FIntPoint(18, 16));
	TestTrue(TEXT("The authoritative footprint is exposed to the world ghost"), !Model->GetSnapshot().FootprintCells.IsEmpty());
	TestTrue(TEXT("The valid footprint can commit on release"), Model->GetSnapshot().bCanConfirm);
	TestTrue(TEXT("Releasing on the valid site submits the normal construction command"), Model->EndCardDrag(true));
	TestEqual(TEXT("One accepted drag creates one authoritative building"), Model->GetPlacedBuildingCount(), 1);
	TestFalse(TEXT("A completed non-repeat drag ends the placement session"), Model->GetSnapshot().bDraggingCard);
	TestEqual(TEXT("Focus returns to the originating card"), Model->GetSnapshot().FocusedSemanticId,
		FName(TEXT("BuildMenu.Card.Building_Road")));

	TestTrue(TEXT("Warehouse can begin a second real drag"), Model->BeginCardDrag(TEXT("Building.Warehouse")));
	TestTrue(TEXT("The disconnected viewport site is still accepted as preview input"), Model->UpdateCardDragTarget(10, 10));
	TestEqual(TEXT("The obstruction/rule result is invalid"), Model->GetSnapshot().Feedback, EHansaPlacementFeedback::Invalid);
	TestFalse(TEXT("Releasing an invalid footprint does not submit construction"), Model->EndCardDrag(true));
	TestEqual(TEXT("Invalid release leaves authoritative building count unchanged"), Model->GetPlacedBuildingCount(), 1);
	TestTrue(TEXT("Invalid release preserves concise causal result text"), Model->GetSnapshot().LastResult.ToString().Contains(TEXT("Road required")));
	TestEqual(TEXT("Invalid release restores focus to the warehouse card"), Model->GetSnapshot().FocusedSemanticId,
		FName(TEXT("BuildMenu.Card.Building_Warehouse")));

	TestTrue(TEXT("A new drag can be cancelled outside the world"), Model->BeginCardDrag(TEXT("Building.Road")));
	TestTrue(TEXT("Leaving the viewport clears the ghost target"), Model->ClearCardDragTarget());
	TestFalse(TEXT("Release over the menu cancels rather than commits"), Model->EndCardDrag(false));
	TestEqual(TEXT("Outside release leaves authoritative state unchanged"), Model->GetPlacedBuildingCount(), 1);
	TestTrue(TEXT("Outside release explains how to place"), Model->GetSnapshot().LastResult.ToString().Contains(TEXT("release over a valid")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaRoadDrawingJourneyTest,
	"Hansa.UI.BuildMenu.DirectRoadDrawing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaRoadDrawingJourneyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaBuildMenuPresentationModel> Model(NewObject<UHansaBuildMenuPresentationModel>());
	FString Error;
	if (!TestTrue(TEXT("The road drawing session initializes"), Model->InitializeForLubeck(nullptr, Error))) return false;
	Model->SetOpen(true);
	TestTrue(TEXT("Road card selection activates the road tool"), Model->SelectBuilding(TEXT("Building.Road")));
	TestTrue(TEXT("Pointer press starts a road at the selected world cell"), Model->BeginRoadDraw(18, 16));
	TestTrue(TEXT("Pointer drag builds a Manhattan corner path"), Model->UpdateRoadDraw(21, 18));
	const FHansaBuildMenuSnapshot& Corner = Model->GetSnapshot();
	TestTrue(TEXT("Road drawing is explicitly observable"), Corner.bRoadDrawing);
	TestEqual(TEXT("Inclusive corner path contains six cells"), Corner.RoadPreviewCells.Num(), 6);
	TestTrue(TEXT("Tie-break is stable horizontal-first"), Corner.RoadPreviewCells.Num() == 6 &&
		Corner.RoadPreviewCells[0].Cell == FIntPoint(18, 16) &&
		Corner.RoadPreviewCells[3].Cell == FIntPoint(21, 16) &&
		Corner.RoadPreviewCells[5].Cell == FIntPoint(21, 18));
	TestTrue(TEXT("Total path cost reports cell count and currency"), Corner.RoadTotalCost.ToString().Contains(TEXT("6 new cells")) &&
		Corner.RoadTotalCost.ToString().Contains(TEXT("150 pf")));
	TestTrue(TEXT("Every empty path cell is individually valid"), Corner.RoadInvalidCellCount == 0 && Corner.RoadNewCellCount == 6);
	TSharedRef<Hansa::UI::SHansaBuildMenu> Menu = SNew(Hansa::UI::SHansaBuildMenu).Model(Model.Get());
	const TArray<Hansa::UI::FHansaHudSemanticNode> RoadNodes = Menu->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* RoadPath = FindBuildNode(RoadNodes, TEXT("Placement.RoadPath"));
	TestTrue(TEXT("Road path semantics expose the deterministic route, typed counts and cost"), RoadPath != nullptr &&
		RoadPath->State.Value.Contains(TEXT("tieBreak=horizontal-first")) &&
		RoadPath->State.Value.Contains(TEXT("new=6")) &&
		RoadPath->State.Value.Contains(TEXT("cost=")));

	TestTrue(TEXT("Moving the pointer replaces the prior preview path"), Model->UpdateRoadDraw(20, 16));
	const FHansaBuildMenuSnapshot& Replaced = Model->GetSnapshot();
	TestEqual(TEXT("Replacement path contains only the new straight span"), Replaced.RoadPreviewCells.Num(), 3);
	TestFalse(TEXT("Superseded corner cells are absent"), Replaced.RoadPreviewCells.ContainsByPredicate(
		[](const FHansaRoadPreviewCell& Cell) { return Cell.Cell == FIntPoint(21, 18); }));
	TestTrue(TEXT("Release submits the ordinary transactional construction batch"), Model->EndRoadDraw(true));
	TestEqual(TEXT("The entire straight road becomes authoritative"), Model->GetPlacedBuildingCount(), 3);

	TestTrue(TEXT("Road tool can be selected again for an intersection"), Model->SelectBuilding(TEXT("Building.Road")));
	TestTrue(TEXT("A continuation can start away from the existing road"), Model->BeginRoadDraw(19, 14));
	TestTrue(TEXT("The preview may cross the player's existing road"), Model->UpdateRoadDraw(19, 18));
	const FHansaBuildMenuSnapshot& Intersection = Model->GetSnapshot();
	TestEqual(TEXT("Intersection reuses one existing cell"), Intersection.RoadExistingCellCount, 1);
	TestEqual(TEXT("Only new cells are charged"), Intersection.RoadNewCellCount, 4);
	TestEqual(TEXT("Existing intersection has its own typed state"),
		Intersection.RoadPreviewCells[2].State, EHansaRoadPreviewCellState::ExistingRoad);
	TestTrue(TEXT("Existing road continuation is warning-shaped but confirmable"),
		Intersection.Feedback == EHansaPlacementFeedback::Warning && Intersection.bCanConfirm);
	TestTrue(TEXT("Intersection release commits only new cells"), Model->EndRoadDraw(true));
	TestEqual(TEXT("Reused intersection does not duplicate occupancy"), Model->GetPlacedBuildingCount(), 7);

	TestTrue(TEXT("Road tool can preview an invalid span"), Model->SelectBuilding(TEXT("Building.Road")));
	TestTrue(TEXT("Invalid road press is retained for causal feedback"), Model->BeginRoadDraw(18, 16));
	TestTrue(TEXT("Dragging into water updates the complete path"), Model->UpdateRoadDraw(18, 0));
	TestTrue(TEXT("Water cells are individually marked invalid"),
		Model->GetSnapshot().RoadInvalidCellCount > 0 && !Model->GetSnapshot().bCanConfirm);
	const int32 BeforeInvalidRelease = Model->GetPlacedBuildingCount();
	TestFalse(TEXT("Invalid release submits no command"), Model->EndRoadDraw(true));
	TestEqual(TEXT("Invalid road span leaves state unchanged"), Model->GetPlacedBuildingCount(), BeforeInvalidRelease);

	TestTrue(TEXT("Road tool can begin a cancellable draw"), Model->BeginRoadDraw(22, 16));
	TestTrue(TEXT("Cancellable path extends normally"), Model->UpdateRoadDraw(24, 16));
	TestFalse(TEXT("Release outside the world cancels"), Model->EndRoadDraw(false));
	TestEqual(TEXT("Cancellation creates no roads"), Model->GetPlacedBuildingCount(), BeforeInvalidRelease);
	TestTrue(TEXT("Cancellation returns focus to the road card"),
		Model->GetSnapshot().FocusedSemanticId == TEXT("BuildMenu.Card.Building_Road"));
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
