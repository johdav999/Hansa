#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

#include "Fixtures/HansaProductionFixture.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Production/HansaProduction.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/SHansaContextInspector.h"
#include "UI/SHansaRootHud.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	const Hansa::UI::FHansaHudSemanticNode* FindNode(
		const TArray<Hansa::UI::FHansaHudSemanticNode>& Nodes,
		const TCHAR* Id)
	{
		return Nodes.FindByPredicate([Id](const Hansa::UI::FHansaHudSemanticNode& Node) { return Node.Id == Id; });
	}

	int32 FindNodeIndex(const TArray<Hansa::UI::FHansaHudSemanticNode>& Nodes, const TCHAR* Id)
	{
		return Nodes.IndexOfByPredicate([Id](const Hansa::UI::FHansaHudSemanticNode& Node) { return Node.Id == Id; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInspectorSharedCausalModelTest,
	"Hansa.UI.Inspector.SharedCausalModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInspectorSharedCausalModelTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!TestTrue(TEXT("The reviewed grain-shortage fixture initializes"), Created.IsSuccess())) return false;
	FHansaProductionFixture Fixture = Created.Value;
	if (!TestTrue(TEXT("The shortage advances through the normal command gateway"), Fixture.Step(5).IsSuccess())) return false;
	const auto Projection = Fixture.BuildProjection();
	if (!TestTrue(TEXT("The authoritative fixture supplies a presentation projection"), Projection.IsSuccess())) return false;
	const FHansaProductionProjection* Blocked = Projection.Value.GetProductions().FindByPredicate(
		[](const FHansaProductionProjection& Item) { return Item.Blocker == EHansaProductionBlocker::MissingInput; });
	if (!TestNotNull(TEXT("The projection contains a missing-input production"), Blocked)) return false;
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry();
	if (!TestNotNull(TEXT("The fixture exposes its compiled registry"), Registry)) return false;

	TStrongObjectPtr<UHansaInspectorPresentationModel> Model(NewObject<UHansaInspectorPresentationModel>());
	Model->InitializeDefaults();
	TestTrue(TEXT("A production projection opens the reusable inspector"),
		Model->ShowProduction(*Blocked, *Registry, Fixture.GetEvents(), TEXT("HUD.AlertStack.Alert.GrainWatch.OpenCause")));
	const FHansaInspectorSnapshot& Snapshot = Model->GetSnapshot();
	TestEqual(TEXT("The causal code is copied from the authoritative projection"), Snapshot.Causal.StableCode,
		FName(LexToString(Blocked->Blocker)));
	TestEqual(TEXT("The missing good is preserved in the causal explanation"), Snapshot.Causal.Severity,
		EHansaCausalSeverity::Warning);
	TestTrue(TEXT("The production inspector has inputs or outputs"), !Snapshot.Flows.IsEmpty());
	TestTrue(TEXT("The production inspector includes history"), !Snapshot.History.IsEmpty());
	TestTrue(TEXT("The presentation exposes evidence without widget-side formula evaluation"),
		Snapshot.Causal.Evidence.ToString().Contains(TEXT("Available")) && Snapshot.Causal.Evidence.ToString().Contains(TEXT("required")));

	if (Projection.Value.GetPopulationCohorts().IsEmpty())
	{
		AddError(TEXT("The reviewed fixture must expose a residence cohort"));
		return false;
	}
	TestTrue(TEXT("The same inspector accepts the residence projection"),
		Model->ShowResidence(Projection.Value.GetPopulationCohorts()[0], *Registry, TEXT("CityOverview.Population")));
	TestEqual(TEXT("Residence mode is explicit"), Model->GetSnapshot().Kind, EHansaInspectorObjectKind::Residence);
	TestTrue(TEXT("Residence needs are rendered from the shared projection"), !Model->GetSnapshot().Flows.IsEmpty());
	Model->ShowWorldBuilding(TEXT("Building.Bakery"), FText::FromString(TEXT("Bakery")),
		FText::FromString(TEXT("Flour → bread")), 42, TEXT("Ready"), TEXT("None"), TEXT("World.Selection"));
	TestTrue(TEXT("World-selected buildings retain their definition flow while awaiting a full simulation join"),
		!Model->GetSnapshot().Flows.IsEmpty() && Model->GetSnapshot().Flows[0].Value.ToString() == TEXT("Flour → bread"));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInspectorAlertNavigationTest,
	"Hansa.UI.Inspector.AlertNavigationAndFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInspectorAlertNavigationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaHudPresentationModel> HudModel(NewObject<UHansaHudPresentationModel>());
	TStrongObjectPtr<UHansaInspectorPresentationModel> InspectorModel(NewObject<UHansaInspectorPresentationModel>());
	HudModel->InitializeDefaults();
	InspectorModel->InitializeDefaults();
	TSharedRef<Hansa::UI::SHansaRootHud> Hud = SNew(Hansa::UI::SHansaRootHud)
		.Model(HudModel.Get())
		.InspectorModel(InspectorModel.Get())
		.InitialViewportSize(FIntPoint(1280, 720));

	const FString AlertBase = TEXT("HUD.AlertStack.Alert.GrainWatch");
	TestTrue(TEXT("Open-cause activates from the alert card"), Hud->ActivateSemanticId(AlertBase + TEXT(".OpenCause")));
	TestTrue(TEXT("Open-cause opens and expands the contextual inspector"),
		InspectorModel->GetSnapshot().bOpen && InspectorModel->GetSnapshot().bCauseExpanded);
	TestEqual(TEXT("The originating alert action is retained for focus restoration"),
		InspectorModel->GetSnapshot().FocusOriginSemanticId, FName(*(AlertBase + TEXT(".OpenCause"))));

	const TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Hud->GetSemanticSnapshot();
	const TCHAR* OrderedSections[] = {
		TEXT("Inspector.Identity"), TEXT("Inspector.Result"), TEXT("Inspector.Flows"),
		TEXT("Inspector.Problem"), TEXT("Inspector.Actions"), TEXT("Inspector.History")
	};
	int32 PriorIndex = INDEX_NONE;
	for (const TCHAR* Section : OrderedSections)
	{
		const int32 Index = FindNodeIndex(Nodes, Section);
		TestTrue(FString::Printf(TEXT("Stable section %s exists after its predecessor"), Section), Index > PriorIndex);
		PriorIndex = Index;
	}
	const Hansa::UI::FHansaHudSemanticNode* Alert = FindNode(Nodes, *AlertBase);
	TestTrue(TEXT("Alert semantics expose severity, age, affected object, cause, and remedy"), Alert != nullptr &&
		Alert->State.bWarning && Alert->State.Value.Contains(TEXT("age=3 min")) &&
		Alert->State.Value.Contains(TEXT("object=Lübeck bakery")) && Alert->State.Value.Contains(TEXT("cause=")) &&
		Alert->State.Value.Contains(TEXT("remedy=")));
	TestNotNull(TEXT("Cause details are machine-readable"), FindNode(Nodes, TEXT("Inspector.Problem.Cause")));
	TestNotNull(TEXT("The expanded tooltip layer is machine-readable"), FindNode(Nodes, TEXT("Inspector.Tooltip.OpenCause")));

	TestTrue(TEXT("An alert can be pinned"), Hud->ActivateSemanticId(AlertBase + TEXT(".Pin")));
	const TArray<Hansa::UI::FHansaHudSemanticNode> PinnedNodes = Hud->GetSemanticSnapshot();
	TestNotNull(TEXT("Pinned tracking is visible in the semantic tree"),
		FindNode(PinnedNodes, TEXT("HUD.PinnedTrackers.Tracker.GrainWatch")));
	TestTrue(TEXT("An alert can be snoozed"), Hud->ActivateSemanticId(AlertBase + TEXT(".Snooze")));
	const TArray<Hansa::UI::FHansaHudSemanticNode> SnoozedNodes = Hud->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Snoozed = FindNode(SnoozedNodes, *AlertBase);
	TestTrue(TEXT("Snoozed state removes the alert card without discarding the pinned tracker"),
		Snoozed != nullptr && !Snoozed->State.bVisible &&
		FindNode(SnoozedNodes, TEXT("HUD.PinnedTrackers.Tracker.GrainWatch")) != nullptr);

	// Restore a visible origin before exercising the close/focus contract.
	TestTrue(TEXT("Snooze can be toggled off"), Hud->ActivateSemanticId(AlertBase + TEXT(".Snooze")));
	TestTrue(TEXT("The inspector closes through its semantic action"), Hud->ActivateSemanticId(TEXT("Inspector.Close")));
	TestEqual(TEXT("Closing restores focus to the action that opened the inspector"),
		HudModel->GetSnapshot().FocusedSemanticId, FName(*(AlertBase + TEXT(".OpenCause"))));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInspectorStableFrameRefreshTest,
	"Hansa.UI.Inspector.FramePreservesWidgetStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInspectorStableFrameRefreshTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaInspectorPresentationModel> Model(NewObject<UHansaInspectorPresentationModel>());
	Model->InitializeDefaults();
	Model->ShowWorldBuilding(
		TEXT("Building.Market"), FText::FromString(TEXT("Market")), FText::FromString(TEXT("Storage → local service")),
		42, TEXT("Ready"), TEXT("None"), TEXT("World.Selection"));
	TSharedRef<Hansa::UI::SHansaContextInspector> Inspector =
		SNew(Hansa::UI::SHansaContextInspector).Model(Model.Get());
	const int32 RebuildCountBeforeFrame = Inspector->GetStructureRebuildCountForTesting();

	TestTrue(TEXT("Frame object is accepted for the selected market"), Model->FrameIntent());
	TestEqual(TEXT("Frame updates do not rebuild inspector flow, action, or history subtrees"),
		Inspector->GetStructureRebuildCountForTesting(), RebuildCountBeforeFrame);
	TestTrue(TEXT("Frame feedback still updates"), Model->GetSnapshot().LastActionResult.ToString().Contains(TEXT("Object framed")));
	TestTrue(TEXT("The selected building can remain pinned across simulation refreshes"), Model->TogglePinIntent());
	Model->SetFocusedSemanticId(TEXT("Inspector.Action.Pin"));
	const uint64 RevisionBeforeStableRefresh = Model->GetRevision();
	Model->ShowWorldBuilding(
		TEXT("Building.Market"), FText::FromString(TEXT("Market")), FText::FromString(TEXT("Storage → local service")),
		42, TEXT("Ready"), TEXT("None"), TEXT("World.Selection"));
	TestEqual(TEXT("An unchanged tick projection does not publish another inspector revision"),
		Model->GetRevision(), RevisionBeforeStableRefresh);
	TestTrue(TEXT("Tick refreshes preserve pinned state"), Model->GetSnapshot().bPinned);
	TestEqual(TEXT("Tick refreshes preserve inspector focus"),
		Model->GetSnapshot().FocusedSemanticId, FName(TEXT("Inspector.Action.Pin")));
	Model->ShowWorldBuilding(
		TEXT("Building.Market"), FText::FromString(TEXT("Market")), FText::FromString(TEXT("Storage → local service")),
		42, TEXT("UnderConstruction"), TEXT("ConstructionIncomplete"), TEXT("World.Selection"));
	TestTrue(TEXT("A changed authoritative state publishes one inspector refresh"),
		Model->GetRevision() > RevisionBeforeStableRefresh);
	TestTrue(TEXT("Changed tick refreshes also preserve pinned state"), Model->GetSnapshot().bPinned);
	TestEqual(TEXT("Changed tick refreshes also preserve inspector focus"),
		Model->GetSnapshot().FocusedSemanticId, FName(TEXT("Inspector.Action.Pin")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaInspectorLocalizationAndScreenshotContractTest,
	"Hansa.UI.Inspector.LocalizationAndScreenshotContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaInspectorLocalizationAndScreenshotContractTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	(void)Parameters;
	TStrongObjectPtr<UHansaHudPresentationModel> HudModel(NewObject<UHansaHudPresentationModel>());
	TStrongObjectPtr<UHansaInspectorPresentationModel> InspectorModel(NewObject<UHansaInspectorPresentationModel>());
	HudModel->InitializeDefaults();
	InspectorModel->InitializeDefaults();
	FHansaCausalPresentation LongCause;
	LongCause.StableCode = TEXT("LocalizationExpansion");
	LongCause.Problem = FText::FromString(TEXT("△ Die Produktionsversorgung ist vorübergehend unterbrochen"));
	LongCause.Cause = FText::FromString(TEXT("Im verbundenen Lagerhaus stehen derzeit keine ausreichenden Getreidevorräte zur Verfügung."));
	LongCause.Evidence = FText::FromString(TEXT("Verfügbar 0,0; erforderlich 0,1 je Produktionszyklus."));
	LongCause.Remedy = FText::FromString(TEXT("Stellen Sie die Getreidereserve wieder her oder richten Sie eine eingehende Handelsroute ein."));
	LongCause.RelatedSemanticId = TEXT("CityOverview.Storage");
	LongCause.Severity = EHansaCausalSeverity::Warning;
	InspectorModel->OpenFromAlert(TEXT("LocalizationExpansion"),
		FText::FromString(TEXT("Lübecker Handwerksbäckerei am nördlichen Speicherbezirk")),
		FText::FromString(TEXT("seit 23 Minuten")), 2, LongCause, TEXT("HUD.AlertStack.Alert.GrainWatch.OpenCause"));
	TSharedRef<Hansa::UI::SHansaRootHud> Hud = SNew(Hansa::UI::SHansaRootHud)
		.Model(HudModel.Get()).InspectorModel(InspectorModel.Get()).InitialViewportSize(FIntPoint(1280, 720));
	const TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Hud->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Identity = FindNode(Nodes, TEXT("Inspector.Identity"));
	const Hansa::UI::FHansaHudSemanticNode* Cause = FindNode(Nodes, TEXT("Inspector.Problem.Cause"));
	TestTrue(TEXT("Long localized identity text remains intact"), Identity != nullptr && Identity->Label.Contains(TEXT("Handwerksbäckerei")));
	TestTrue(TEXT("Long localized causal text remains intact"), Cause != nullptr && Cause->State.Value.Contains(TEXT("Handelsroute")));

	FHansaNativeScreenshotService Screenshots;
	auto NativeBuffer = [](const FIntPoint& Size, TArray<FColor>& Pixels)
	{
		Pixels.Init(FColor(20, 40, 51, 255), Size.X * Size.Y);
		return true;
	};
	auto Capture = [this, &Screenshots, &NativeBuffer](const FIntPoint Size, const TCHAR* Bundle)
	{
		FHansaScreenshotContext Context;
		Context.BundleId = Bundle;
		Context.EvidenceSuiteId = TEXT("S07P04");
		Context.FixtureId = TEXT("inspector-causal-navigation-v1");
		Context.ScreenId = TEXT("HUD.Root");
		Context.FlowId = TEXT("alert-open-cause-pin-snooze-focus-v1");
		Context.CaptureMethod = TEXT("AutomationTest.NativeBufferContract");
		Context.UiRevision = 1;
		Context.SemanticSnapshotJson = TEXT("{\"schemaVersion\":1,\"nodes\":[{\"id\":\"HUD.AlertStack.Group.Production\"},{\"id\":\"HUD.AlertStack.Alert.GrainWatch\"},{\"id\":\"Inspector.Identity\"},{\"id\":\"Inspector.Result\"},{\"id\":\"Inspector.Flows\"},{\"id\":\"Inspector.Problem\"},{\"id\":\"Inspector.Actions\"},{\"id\":\"Inspector.History\"},{\"id\":\"Inspector.Tooltip.OpenCause\"}]}" );
		Context.StructuralAssertions = {
			TEXT("inspector.stableSectionOrder=true"), TEXT("alert.grouped=true"), TEXT("alert.causeAction=true"),
			TEXT("alert.snooze=true"), TEXT("alert.pin=true"), TEXT("focus.restore=true"), TEXT("localization.expansion=true")
		};
		Context.bStructuralAssertionsPassed = true;
		const FHansaScreenshotResult Result = Screenshots.Capture(Size, Context, NativeBuffer);
		TestTrue(FString::Printf(TEXT("Native %dx%d inspector evidence writes"), Size.X, Size.Y), Result.IsSuccess());
		FString Metadata;
		FString SemanticSnapshot;
		TestTrue(TEXT("Inspector evidence metadata is readable"), FFileHelper::LoadFileToString(Metadata, *Result.MetadataPath));
		TestTrue(TEXT("Inspector semantic evidence is readable"), FFileHelper::LoadFileToString(SemanticSnapshot, *Result.SemanticSnapshotPath));
		TestTrue(TEXT("Inspector evidence proves the native size, assertions, and no resize"),
			Metadata.Contains(FString::Printf(TEXT("\"width\":%d"), Size.X)) &&
			Metadata.Contains(FString::Printf(TEXT("\"height\":%d"), Size.Y)) &&
			Metadata.Contains(TEXT("\"structuralAssertionCount\":7")) &&
			Metadata.Contains(TEXT("\"postCaptureResized\":false")));
		TestTrue(TEXT("Inspector evidence is not pixel-only"),
			SemanticSnapshot.Contains(TEXT("Inspector.Problem")) && SemanticSnapshot.Contains(TEXT("Inspector.History")));
	};
	Capture(FIntPoint(1280, 720), TEXT("contract-inspector-causal-navigation-720"));
	Capture(FIntPoint(1920, 1080), TEXT("contract-inspector-causal-navigation-1080"));
	return !HasAnyErrors();
}

#endif
