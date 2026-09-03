#include "Gameplay/HansaPlacementAutomationFixture.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPlacementSemanticFlowTest,
	"Hansa.Architecture.Automation.EmptyLubeckPlacementFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPlacementSemanticFlowTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;

	FHansaSemanticUiRegistry Registry;
	FHansaPlacementAutomationFixture Fixture;
	FString Error;
	TestTrue(TEXT("The exact empty Lübeck fixture loads"), Fixture.Load(Registry, Error));
	TestEqual(TEXT("The fixture begins without placed entities"), Fixture.GetPlacedBuildingCount(), 0);
	const FHansaSemanticNode* Camera = Registry.FindNode(TEXT("BuildMode.Camera"));
	TestTrue(TEXT("Camera state is stable, typed, and structurally observable"),
		Camera != nullptr && Camera->State.ValueType == TEXT("strategy-camera") &&
		Camera->State.Value.Contains(TEXT("zoomDistance=6500")));

	auto Activate = [this, &Registry](const TCHAR* Id)
	{
		const FHansaSemanticActionResult Result = Registry.Invoke(Id, EHansaSemanticAction::Activate);
		TestTrue(FString::Printf(TEXT("Normal input intent activates through %s"), Id), Result.IsSuccess());
		return Result.IsSuccess();
	};

	Activate(TEXT("BuildMode.Tool.Road"));
	Activate(TEXT("BuildMode.Map.RoadTarget"));
	const FHansaSemanticNode* RoadPreview = Registry.FindNode(TEXT("BuildMode.Placement.Preview"));
	TestTrue(TEXT("Road preview is typed and valid before confirmation"),
		RoadPreview != nullptr && RoadPreview->State.bSelected && !RoadPreview->State.bError &&
		RoadPreview->State.Value.Contains(TEXT("Building.Road")));
	Activate(TEXT("BuildMode.Action.Confirm"));
	TestEqual(TEXT("Road confirmation reaches authoritative occupancy"), Fixture.GetPlacedBuildingCount(), 1);

	Activate(TEXT("BuildMode.Tool.Warehouse"));
	Activate(TEXT("BuildMode.Map.InvalidTarget"));
	const FHansaSemanticNode* Invalid = Registry.FindNode(TEXT("BuildMode.Placement.Validation"));
	const FHansaSemanticNode* Cause = Registry.FindNode(TEXT("BuildMode.Placement.Validation.Cause"));
	const FHansaSemanticNode* Remedy = Registry.FindNode(TEXT("BuildMode.Placement.Validation.Remedy"));
	TestTrue(TEXT("Disconnected warehouse exposes a precise non-color-only reason and remedy"),
		Invalid != nullptr && Invalid->State.bError && Invalid->State.Value == TEXT("RoadRequired") &&
		Cause != nullptr && Cause->Label == TEXT("Road required") &&
		Remedy != nullptr && Remedy->Label == TEXT("Build next to a road"));
	TestFalse(TEXT("Invalid preview cannot be confirmed"), Fixture.CanConfirm());

	Activate(TEXT("BuildMode.Map.ValidTarget"));
	const FHansaSemanticNode* Valid = Registry.FindNode(TEXT("BuildMode.Placement.Validation"));
	TestTrue(TEXT("Road-adjacent warehouse becomes structurally valid"),
		Valid != nullptr && Valid->State.bSelected && !Valid->State.bError &&
		Valid->State.Value == TEXT("None") && Fixture.CanConfirm());
	Activate(TEXT("BuildMode.Action.Confirm"));
	TestEqual(TEXT("Warehouse confirmation reaches authoritative occupancy"), Fixture.GetPlacedBuildingCount(), 2);
	TestEqual(TEXT("The two accepted commands advance authoritative time"), Fixture.GetSimulationTick(), int64(2));
	const FHansaSemanticNode* Result = Registry.FindNode(TEXT("BuildMode.Result.Building"));
	TestTrue(TEXT("Resulting entity has stable semantic identity and typed entity state"),
		Result != nullptr && Result->State.bSelected && Result->State.bVisible &&
		Result->State.ValueType == TEXT("building-entity-id") && Result->State.Value == TEXT("Building:2:0"));

	FHansaNativeScreenshotService Screenshots;
	auto NativeBuffer = [](const FIntPoint& Size, TArray<FColor>& Pixels)
	{
		Pixels.Init(FColor(21, 42, 53, 255), Size.X * Size.Y);
		return true;
	};
	auto CaptureEvidence = [this, &Screenshots, &NativeBuffer](const FIntPoint Size, const TCHAR* Bundle)
	{
		FHansaScreenshotContext Context;
		Context.BundleId = Bundle;
		Context.EvidenceSuiteId = TEXT("S05P04");
		Context.FixtureId = FHansaPlacementAutomationFixture::StableFixtureId;
		Context.ScreenId = TEXT("BuildMode.Screen");
		Context.FlowId = TEXT("empty-lubeck-road-warehouse-v1");
		Context.SimulationTick = 2;
		Context.CaptureMethod = TEXT("AutomationTest.NativeBufferContract");
		Context.UiRevision = 1;
		Context.SemanticSnapshotJson = TEXT("{\"schemaVersion\":1,\"nodes\":[{\"id\":\"BuildMode.Camera\"},{\"id\":\"BuildMode.Placement.Validation\"},{\"id\":\"BuildMode.Result.Building\"}]}");
		Context.StructuralAssertions = {
			TEXT("fixture.loaded=true"), TEXT("authoritative.placedBuildingCount=2"),
			TEXT("semantic.BuildMode.Camera.exists=true"),
			TEXT("semantic.BuildMode.Placement.Validation.exists=true"),
			TEXT("semantic.BuildMode.Result.Building.selected=true")
		};
		Context.bStructuralAssertionsPassed = true;
		const FHansaScreenshotResult Capture = Screenshots.Capture(Size, Context, NativeBuffer);
		TestTrue(FString::Printf(TEXT("Native %dx%d placement evidence writes"), Size.X, Size.Y), Capture.IsSuccess());
		FString Metadata;
		FString Snapshot;
		TestTrue(TEXT("Placement evidence metadata is readable"), FFileHelper::LoadFileToString(Metadata, *Capture.MetadataPath));
		TestTrue(TEXT("Placement semantic snapshot is readable"), FFileHelper::LoadFileToString(Snapshot, *Capture.SemanticSnapshotPath));
		TestTrue(TEXT("Metadata records fixture, flow, structural assertions, and no resize"),
			Metadata.Contains(TEXT("\"fixtureId\":\"empty_lubeck_build_v1\"")) &&
			Metadata.Contains(TEXT("\"flowId\":\"empty-lubeck-road-warehouse-v1\"")) &&
			Metadata.Contains(TEXT("\"structuralAssertionCount\":5")) &&
			Metadata.Contains(TEXT("\"structuralAssertionsPassed\":true")) &&
			Metadata.Contains(TEXT("\"postCaptureResized\":false")));
		TestTrue(TEXT("Evidence is not pixel-only"),
			Snapshot.Contains(TEXT("BuildMode.Camera")) && Snapshot.Contains(TEXT("BuildMode.Result.Building")));
	};
	CaptureEvidence(FIntPoint(1280, 720), TEXT("contract-empty-lubeck-flow-720"));
	CaptureEvidence(FIntPoint(1920, 1080), TEXT("contract-empty-lubeck-flow-1080"));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaConstructionAutomationParityTest,
	"Hansa.Architecture.Automation.ConstructionQueryActionParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaConstructionAutomationParityTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;

	FHansaSemanticUiRegistry Registry;
	FHansaPlacementAutomationFixture Fixture;
	FString Error;
	if (!TestTrue(TEXT("Construction automation uses the reviewed placement fixture"), Fixture.Load(Registry, Error)))
	{
		return false;
	}
	TestTrue(TEXT("Road tool selection is accepted"), Fixture.SelectRoadIntent());
	TestTrue(TEXT("Road target intent is accepted"), Fixture.TargetRoadCellIntent());
	TestTrue(TEXT("Road placement action is accepted"), Fixture.ConfirmIntent());
	TestTrue(TEXT("Warehouse tool selection is accepted"), Fixture.SelectWarehouseIntent());
	TestTrue(TEXT("Warehouse target intent is accepted"), Fixture.TargetValidCellIntent());
	TestTrue(TEXT("Warehouse placement action is accepted"), Fixture.ConfirmIntent());

	const FHansaBuildingId BuildingId = Fixture.GetLastPlacedBuildingId();
	const auto Construction = Fixture.QueryConstruction(BuildingId);
	TestTrue(TEXT("Automation can query typed construction state"),
		Construction.IsSet() && Construction->State == EHansaConstructionState::UnderConstruction);
	const FHansaConstructionCostProjection Cost = Fixture.QueryConstructionCost(
		FHansaBuildingTypeId::TryParse(TEXT("Building.Warehouse")).Value);
	TestTrue(TEXT("Automation can inspect missing/available cost without mutating it"), Cost.IsAffordable());
	const FHansaSemanticNode* Status = Registry.FindNode(TEXT("BuildMode.Construction.Status"));
	const FHansaSemanticNode* CostNode = Registry.FindNode(TEXT("BuildMode.Construction.Cost"));
	TestTrue(TEXT("Construction status is semantically observable"),
		Status != nullptr && Status->State.ValueType == TEXT("construction-state"));
	TestTrue(TEXT("Construction affordability is semantically observable"),
		CostNode != nullptr && CostNode->State.ValueType == TEXT("construction-cost"));

	TestTrue(TEXT("Controlled stepping advances through the normal simulation pipeline"), Fixture.AdvanceTicks(30));
	const auto Completed = Fixture.QueryConstruction(BuildingId);
	TestTrue(TEXT("Automation observes exact completion rather than setting it"),
		Completed.IsSet() && Completed->State == EHansaConstructionState::Completed &&
		Completed->ElapsedTicks == Completed->TotalTicks);
	TestFalse(TEXT("Completed work rejects the cancellation action"), Fixture.CancelLastConstructionIntent());
	TestTrue(TEXT("Completed dependency-free work accepts safe removal"), Fixture.RemoveLastBuildingIntent());
	TestEqual(TEXT("Safe removal updates authoritative occupancy"), Fixture.GetPlacedBuildingCount(), 1);
	return !HasAnyErrors();
}

#endif
