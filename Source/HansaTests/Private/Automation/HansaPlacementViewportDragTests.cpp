#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "UI/HansaBuildMenuPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaRootHud.h"
#include "World/HansaBuildingWorldProjection.h"
#include "World/HansaLubeckWorldFoundation.h"
#include "World/HansaStrategyCameraPawn.h"
#include "World/HansaStrategyPlayerController.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FHansaVerifyViewportPlacementDrag, FAutomationTestBase*, Test);

bool FHansaVerifyViewportPlacementDrag::Update()
{
	UWorld* PieWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
		{
			PieWorld = Context.World();
			break;
		}
	}
	if (PieWorld == nullptr)
	{
		Test->AddError(TEXT("PIE did not expose a playable world viewport."));
		return true;
	}
	AHansaStrategyPlayerController* Controller = Cast<AHansaStrategyPlayerController>(PieWorld->GetFirstPlayerController());
	AHansaRootHud* Hud = Controller != nullptr ? Cast<AHansaRootHud>(Controller->GetHUD()) : nullptr;
	UHansaBuildMenuPresentationModel* Model = Hud != nullptr ? Hud->GetBuildMenuPresentationModel() : nullptr;
	AHansaLubeckWorldFoundation* Foundation = nullptr;
	for (TActorIterator<AHansaLubeckWorldFoundation> It(PieWorld); It; ++It) { Foundation = *It; break; }
	if (Controller == nullptr || Model == nullptr || Foundation == nullptr)
	{
		Test->AddError(TEXT("Playable Lübeck viewport did not create its controller, HUD model, and foundation."));
		return true;
	}

	Model->SetOpen(true);
	if (!Model->BeginCardDrag(TEXT("Building.Road")))
	{
		Test->AddError(TEXT("Road card could not create a viewport placement session."));
		return true;
	}
	FIntPoint ValidCell = FIntPoint::ZeroValue;
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	bool bFoundVisibleValidCell = false;
	int32 Width = 0, Height = 0;
	Controller->GetViewportSize(Width, Height);
	for (int32 Y = 0; Y < 40 && !bFoundVisibleValidCell; ++Y)
	{
		for (int32 X = 0; X < 60; ++X)
		{
			Model->UpdateCardDragTarget(X, Y);
			FVector2D Candidate;
			if (Model->GetSnapshot().bCanConfirm && Controller->ProjectWorldLocationToScreen(
				Foundation->PlacementCellToWorld(X, Y, 106.0f), Candidate, true) &&
				Candidate.X >= 0.0 && Candidate.Y >= 0.0 && Candidate.X < Width && Candidate.Y < Height)
			{
				ValidCell = FIntPoint(X, Y);
				ScreenPosition = Candidate;
				bFoundVisibleValidCell = true;
				break;
			}
		}
	}
	Test->TestTrue(TEXT("A valid construction cell is visible in the real PIE viewport"), bFoundVisibleValidCell);
	if (!bFoundVisibleValidCell) return true;
	Model->CancelIntent();

	FIntPoint ResolvedCell;
	FVector ResolvedWorld;
	Test->TestTrue(TEXT("The real viewport deprojects the visible construction site"),
		Controller->ResolvePlacementCellAtScreenPosition(ScreenPosition, ResolvedCell, ResolvedWorld));
	Test->TestEqual(TEXT("Viewport deprojection returns the projected placement cell"), ResolvedCell, ValidCell);
	const int32 BeforeCount = Model->GetPlacedBuildingCount();
	Test->TestTrue(TEXT("A card drag can begin at the real viewport coordinate"),
		Controller->BeginBuildingPlacementDrag(TEXT("Building.Road"), ScreenPosition, false, false));
	AHansaBuildingPlacementGhost* Ghost = Controller->GetPlacementGhost();
	Test->TestTrue(TEXT("Pointer motion materializes the world-space placement ghost"),
		Ghost != nullptr && Ghost->IsPreviewVisible() && Ghost->GetPreviewCellCount() > 0);
	Test->TestTrue(TEXT("Releasing at the same viewport coordinate commits construction"),
		Controller->EndBuildingPlacementDrag(ScreenPosition, false, false));
	Test->TestEqual(TEXT("The viewport drop reached the normal authoritative command gateway"),
		Model->GetPlacedBuildingCount(), BeforeCount + 1);

	Test->TestTrue(TEXT("Selecting Roads enables direct world drawing"),
		Model->SelectBuilding(TEXT("Building.Road")));
	FIntPoint EndCell = FIntPoint::ZeroValue;
	FVector2D EndScreen = FVector2D::ZeroVector;
	bool bFoundRoadEnd = false;
	Test->TestTrue(TEXT("Direct drawing may start on the existing owned road"),
		Model->BeginRoadDraw(ValidCell.X, ValidCell.Y));
	for (const FIntPoint Offset : {
		FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
	{
		const FIntPoint CandidateCell = ValidCell + Offset;
		FVector2D CandidateScreen;
		Model->UpdateRoadDraw(CandidateCell.X, CandidateCell.Y);
		if (Model->GetSnapshot().bCanConfirm && Controller->ProjectWorldLocationToScreen(
			Foundation->PlacementCellToWorld(CandidateCell.X, CandidateCell.Y, 106.0f),
			CandidateScreen, true) && CandidateScreen.X >= 0.0 && CandidateScreen.Y >= 0.0 &&
			CandidateScreen.X < Width && CandidateScreen.Y < Height)
		{
			EndCell = CandidateCell;
			EndScreen = CandidateScreen;
			bFoundRoadEnd = true;
			break;
		}
	}
	Test->TestTrue(TEXT("A visible adjacent road endpoint exists"), bFoundRoadEnd);
	Model->EndRoadDraw(false);
	if (!bFoundRoadEnd) return true;

	const int32 BeforeRoadDraw = Model->GetPlacedBuildingCount();
	Test->TestTrue(TEXT("Viewport press starts direct road drawing"),
		Controller->BeginRoadDrawing(ScreenPosition, false));
	Test->TestTrue(TEXT("Viewport pointer motion replaces the live connected road path"),
		Controller->UpdateRoadDrawing(EndScreen, false));
	Ghost = Controller->GetPlacementGhost();
	Test->TestTrue(TEXT("Live road preview renders both the reused start and new endpoint"),
		Ghost != nullptr && Ghost->IsPreviewVisible() && Ghost->GetPreviewCellCount() == 2);
	Test->TestTrue(TEXT("Road preview exposes reused and new per-cell states"),
		Model->GetSnapshot().RoadExistingCellCount == 1 &&
		Model->GetSnapshot().RoadNewCellCount == 1 &&
		Model->GetSnapshot().RoadEndCell == EndCell);
	Test->TestTrue(TEXT("Viewport release submits the ordinary road construction command"),
		Controller->EndRoadDrawing(EndScreen, true, false));
	Test->TestEqual(TEXT("Existing intersection is reused and only the new road cell is created"),
		Model->GetPlacedBuildingCount(), BeforeRoadDraw + 1);

	Model->CancelIntent();
	AHansaBuildingWorldProjectionActor* SelectableBuilding = nullptr;
	FVector2D SelectionScreen = FVector2D::ZeroVector;
	for (TActorIterator<AHansaBuildingWorldProjectionActor> It(PieWorld); It; ++It)
	{
		if (It->IsRoad()) continue;
		FVector2D CandidateScreen;
		if (!Controller->ProjectWorldLocationToScreen(It->GetComponentsBoundingBox(true).GetCenter(), CandidateScreen, true) ||
			CandidateScreen.X < 0.0 || CandidateScreen.Y < 0.0 || CandidateScreen.X >= Width || CandidateScreen.Y >= Height)
		{
			continue;
		}
		Controller->SetMouseLocation(FMath::RoundToInt(CandidateScreen.X), FMath::RoundToInt(CandidateScreen.Y));
		FHitResult CandidateHit;
		if (Controller->TraceWorldSelection(CandidateHit) && CandidateHit.GetActor() == *It)
		{
			SelectableBuilding = *It;
			SelectionScreen = CandidateScreen;
			break;
		}
	}
	Test->TestTrue(TEXT("An authored building model is selectable through the real PIE viewport"),
		SelectableBuilding != nullptr);
	if (SelectableBuilding == nullptr) return true;

	Controller->PerformWorldSelection();
	UHansaInspectorPresentationModel* Inspector = Hud->GetInspectorPresentationModel();
	Test->TestTrue(TEXT("Viewport selection outlines the authored model and opens its contextual inspector"),
		Controller->GetSelectedWorldActor() == SelectableBuilding && SelectableBuilding->IsSelected() &&
		Inspector != nullptr && Inspector->GetSnapshot().bOpen &&
		Inspector->GetSnapshot().BuildingValue == static_cast<int64>(SelectableBuilding->GetBuildingId().GetValue()));
	AHansaStrategyCameraPawn* Camera = Cast<AHansaStrategyCameraPawn>(Controller->GetPawn());
	const FVector BuildingLocation = SelectableBuilding->GetActorLocation();
	Test->TestTrue(TEXT("Viewport selection immediately frames the selected authored model"),
		Camera != nullptr && Camera->GetFocusLocation2D().Equals(FVector2D(BuildingLocation.X, BuildingLocation.Y), 1.0));
	if (Inspector != nullptr) Inspector->FrameIntent();
	Test->TestTrue(TEXT("The inspector frame action can return the camera to the selected authored model"),
		Camera != nullptr && Camera->GetFocusLocation2D().Equals(FVector2D(BuildingLocation.X, BuildingLocation.Y), 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementViewportDragTest,
	"Hansa.UI.BuildMenu.RealViewportDrag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FHansaPlacementViewportDragTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FHansaVerifyViewportPlacementDrag(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
