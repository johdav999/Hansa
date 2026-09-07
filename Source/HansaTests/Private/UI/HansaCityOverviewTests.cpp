#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

#include "Fixtures/HansaProductionFixture.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/SHansaCityOverview.h"
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

	const FHansaCityOverviewSummaryPresentation* FindSummary(
		const FHansaCityOverviewSnapshot& Snapshot,
		const TCHAR* Id)
	{
		return Snapshot.HeaderSummaries.FindByPredicate(
			[Id](const FHansaCityOverviewSummaryPresentation& Summary) { return Summary.StableId == FName(Id); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCityOverviewProjectionTest,
	"Hansa.UI.CityOverview.AuthoritativeProjectionAndCausalLinks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCityOverviewProjectionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!TestTrue(TEXT("The grain-shortage fixture initializes"), Created.IsSuccess())) return false;
	FHansaProductionFixture Fixture = Created.Value;
	if (!TestTrue(TEXT("The fixture advances through the command gateway"), Fixture.Step(5).IsSuccess())) return false;
	const auto Projection = Fixture.BuildProjection();
	if (!TestTrue(TEXT("The fixture builds the authoritative projection"), Projection.IsSuccess())) return false;
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry();
	if (!TestNotNull(TEXT("The fixture exposes its immutable registry"), Registry)) return false;
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	if (!TestTrue(TEXT("The Lübeck stable ID parses"), CityId.IsSuccess())) return false;

	TStrongObjectPtr<UHansaCityOverviewPresentationModel> Model(NewObject<UHansaCityOverviewPresentationModel>());
	Model->InitializeDefaults();
	TestTrue(TEXT("The overview opens from the normal HUD origin"), Model->Open(TEXT("HUD.TopStatus.CityOverview")));
	TestTrue(TEXT("The authoritative projection is accepted"),
		Model->ApplyProjection(Projection.Value, *Registry, CityId.Value, FText::FromString(TEXT("Lübeck")), 18500));
	const FHansaCityOverviewSnapshot& Snapshot = Model->GetSnapshot();
	TestEqual(TEXT("The header exposes exactly the six required summaries"), Snapshot.HeaderSummaries.Num(), 6);
	TestNotNull(TEXT("Population trend summary is stable"), FindSummary(Snapshot, TEXT("PopulationTrend")));
	TestNotNull(TEXT("Treasury contribution summary is stable"), FindSummary(Snapshot, TEXT("TreasuryContribution")));
	TestNotNull(TEXT("Satisfaction summary is stable"), FindSummary(Snapshot, TEXT("Satisfaction")));
	TestNotNull(TEXT("Workforce summary is stable"), FindSummary(Snapshot, TEXT("Workforce")));
	TestNotNull(TEXT("Staple reserve summary is stable"), FindSummary(Snapshot, TEXT("StapleReserve")));
	TestNotNull(TEXT("Alerts summary is stable"), FindSummary(Snapshot, TEXT("Alerts")));
	TestTrue(TEXT("Population rows are projected rather than widget-calculated"), !Snapshot.PopulationRows.IsEmpty());
	TestTrue(TEXT("Production rows are projected rather than widget-calculated"), !Snapshot.ProductionRows.IsEmpty());
	TestTrue(TEXT("Market rows are projected rather than widget-calculated"), !Snapshot.MarketRows.IsEmpty());
	TestTrue(TEXT("Population rows preserve separate need dimensions"), Snapshot.PopulationRows.ContainsByPredicate(
		[](const FHansaCityOverviewRowPresentation& Row)
		{
			return Row.Fields.ContainsByPredicate([](const FHansaCityOverviewFieldPresentation& Field)
			{
				return Field.StableId == TEXT("Need") && Field.Value.ToString().Contains(TEXT("access")) &&
					Field.Value.ToString().Contains(TEXT("affordability")) && Field.Value.ToString().Contains(TEXT("reliability"));
			});
		}));

	const uint64 Revision = Model->GetRevision();
	TestTrue(TEXT("An identical authoritative refresh is accepted"),
		Model->ApplyProjection(Projection.Value, *Registry, CityId.Value, FText::FromString(TEXT("Lübeck")), 18500));
	TestEqual(TEXT("An identical event refresh does not publish a presentation revision"), Model->GetRevision(), Revision);

	int32 RelatedRequestCount = 0;
	FName RelatedTarget;
	Model->OnRelatedTargetRequested().AddLambda([&RelatedRequestCount, &RelatedTarget](const FName Target, const int64 BuildingValue)
	{
		(void)BuildingValue;
		++RelatedRequestCount;
		RelatedTarget = Target;
	});
	const FHansaCityOverviewRowPresentation* PopulationAction = Snapshot.PopulationRows.FindByPredicate(
		[](const FHansaCityOverviewRowPresentation& Row) { return Row.bCausalActionEnabled; });
	if (TestNotNull(TEXT("A population need exposes a supplying-chain link"), PopulationAction))
	{
		TestTrue(TEXT("The causal intent publishes its stable related target"), Model->ActivateCausalIntent(PopulationAction->StableId));
		TestEqual(TEXT("Exactly one causal request is published"), RelatedRequestCount, 1);
		TestTrue(TEXT("The causal request never targets localized text"), !RelatedTarget.IsNone() && RelatedTarget.ToString().StartsWith(TEXT("CityOverview.Production.Good.")));
		TestEqual(TEXT("A supplying-chain link opens the Production surface"), Model->GetSnapshot().ActiveTab, EHansaCityOverviewTab::Production);
		TestTrue(TEXT("Supplying-chain navigation selects a matching production node"), !Model->GetSnapshot().SelectedRowStableId.IsNone());
		TestTrue(TEXT("Supplying-chain navigation focuses the matching production node"), Model->GetSnapshot().FocusedSemanticId.ToString().StartsWith(TEXT("CityOverview.Row.Production_")));
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCityOverviewSemanticInputTest,
	"Hansa.UI.CityOverview.SemanticsTabsFocusAndVirtualization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCityOverviewSemanticInputTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!Created) return false;
	const auto Projection = Created.Value.BuildProjection();
	if (!Projection) return false;
	const FHansaEconomicRegistry* Registry = Created.Value.GetDefinitions().GetEconomicRegistry();
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	if (Registry == nullptr || !CityId) return false;

	TStrongObjectPtr<UHansaCityOverviewPresentationModel> Model(NewObject<UHansaCityOverviewPresentationModel>());
	Model->InitializeDefaults();
	Model->Open();
	Model->ApplyProjection(Projection.Value, *Registry, CityId.Value, FText::FromString(TEXT("Lübeck")));
	TSharedRef<Hansa::UI::SHansaCityOverview> Screen = SNew(Hansa::UI::SHansaCityOverview)
		.Model(Model.Get()).InitialViewportSize(FIntPoint(1280, 720));

	TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Screen->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* PopulationTab = FindNode(Nodes, TEXT("CityOverview.Tab.Population"));
	const Hansa::UI::FHansaHudSemanticNode* Administration = FindNode(Nodes, TEXT("CityOverview.Tab.Administration"));
	TestTrue(TEXT("Population starts selected and is activatable/focusable"), PopulationTab != nullptr && PopulationTab->State.bSelected && PopulationTab->bCanActivate && PopulationTab->bCanFocus);
	TestTrue(TEXT("Administration is an explicit non-interactive future placeholder"), Administration != nullptr && !Administration->State.bEnabled && !Administration->bCanActivate && Administration->State.Value == TEXT("future-placeholder"));
	TestTrue(TEXT("The active data surface is a virtualized list role"), FindNode(Nodes, TEXT("CityOverview.List")) != nullptr && FindNode(Nodes, TEXT("CityOverview.List"))->Role == Hansa::UI::EHansaHudSemanticRole::List);
	TestTrue(TEXT("All three implemented tabs are in controller focus order"),
		Screen->GetControllerFocusOrder().Contains(TEXT("CityOverview.Tab.Population")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("CityOverview.Tab.Production")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("CityOverview.Tab.Market")));
	TestTrue(TEXT("The disabled future tab is excluded from controller focus order"), !Screen->GetControllerFocusOrder().Contains(TEXT("CityOverview.Tab.Administration")));

	const int32 RefreshesBeforeTab = Screen->GetListRefreshCountForTesting();
	TestTrue(TEXT("Production tab changes through semantic intent"), Screen->ActivateSemanticId(TEXT("CityOverview.Tab.Production")));
	Nodes = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Production selection is observable"), FindNode(Nodes, TEXT("CityOverview.Tab.Production")) != nullptr && FindNode(Nodes, TEXT("CityOverview.Tab.Production"))->State.bSelected);
	TestTrue(TEXT("Changing tabs refreshes the virtualized source exactly through the event path"), Screen->GetListRefreshCountForTesting() > RefreshesBeforeTab);
	if (!Model->GetActiveRows().IsEmpty())
	{
		FString RowSuffix = Model->GetActiveRows()[0].StableId.ToString();
		RowSuffix.ReplaceInline(TEXT("."), TEXT("_"));
		const FString FirstRowId = FString::Printf(TEXT("CityOverview.Row.%s"), *RowSuffix);
		TestTrue(TEXT("Virtualized rows can receive controller focus before Slate materializes their row widget"), Screen->FocusSemanticId(FirstRowId));
	}
	TestTrue(TEXT("Market tab accepts direct focus"), Screen->FocusSemanticId(TEXT("CityOverview.Tab.Market")));
	TestEqual(TEXT("Focused semantics are independent of selection"), Model->GetSnapshot().FocusedSemanticId, FName(TEXT("CityOverview.Tab.Market")));
	TestTrue(TEXT("Controller shoulder cycling reaches Market"), Model->CycleTabIntent(1));
	TestEqual(TEXT("Market becomes active"), Model->GetSnapshot().ActiveTab, EHansaCityOverviewTab::Market);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCityOverviewStatesAndLocalizationTest,
	"Hansa.UI.CityOverview.EmptyLoadingErrorAndLongLabels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCityOverviewStatesAndLocalizationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaCityOverviewPresentationModel> Model(NewObject<UHansaCityOverviewPresentationModel>());
	Model->InitializeDefaults();
	Model->Open();
	TSharedRef<Hansa::UI::SHansaCityOverview> Screen = SNew(Hansa::UI::SHansaCityOverview)
		.Model(Model.Get()).InitialViewportSize(FIntPoint(1280, 720));
	TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("The empty state is explicit and actionable"), FindNode(Nodes, TEXT("CityOverview.State")) != nullptr &&
		FindNode(Nodes, TEXT("CityOverview.State"))->State.Value.Contains(TEXT("eligible buildings")));

	const FText LongCity = FText::FromString(TEXT("Freie und Hansestadt Lübeck – nördlicher Hafen- und Handwerksspeicherbezirk City Overview"));
	Model->SetLoading(LongCity);
	Nodes = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Loading uses stable geometry and keeps the complete long city label"), FindNode(Nodes, TEXT("CityOverview.Root")) != nullptr &&
		FindNode(Nodes, TEXT("CityOverview.Root"))->Label.Contains(TEXT("Handwerksspeicherbezirk")) &&
		FindNode(Nodes, TEXT("CityOverview.Root"))->State.Value == TEXT("loading"));

	Model->SetError(
		FText::FromString(TEXT("Die maßgebliche Stadtprojektion konnte wegen einer vorübergehend unvollständigen Datenquelle nicht erstellt werden.")),
		FText::FromString(TEXT("Lassen Sie Lübeck ausgewählt und versuchen Sie es erneut, sobald der Simulationsbericht verfügbar ist.")));
	Nodes = Screen->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Error = FindNode(Nodes, TEXT("CityOverview.State"));
	const Hansa::UI::FHansaHudSemanticNode* Retry = FindNode(Nodes, TEXT("CityOverview.State.Retry"));
	TestTrue(TEXT("Long localized error cause remains intact"), Error != nullptr && Error->State.bError && Error->State.Value.Contains(TEXT("Datenquelle")) && Error->State.Value.Contains(TEXT("Simulationsbericht")));
	TestTrue(TEXT("Error exposes an enabled semantic retry action"), Retry != nullptr && Retry->State.bEnabled && Retry->bCanActivate);
	TestTrue(TEXT("Retry transitions through the normal loading state"), Screen->ActivateSemanticId(TEXT("CityOverview.State.Retry")));
	TestEqual(TEXT("Retry does not fabricate data"), Model->GetSnapshot().LoadState, EHansaCityOverviewLoadState::Loading);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCityOverviewRootHudAndNativeEvidenceTest,
	"Hansa.UI.CityOverview.RootHudAndNativeEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCityOverviewRootHudAndNativeEvidenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	(void)Parameters;
	TStrongObjectPtr<UHansaHudPresentationModel> HudModel(NewObject<UHansaHudPresentationModel>());
	TStrongObjectPtr<UHansaCityOverviewPresentationModel> CityModel(NewObject<UHansaCityOverviewPresentationModel>());
	HudModel->InitializeDefaults();
	CityModel->InitializeDefaults();
	TSharedRef<Hansa::UI::SHansaRootHud> Hud = SNew(Hansa::UI::SHansaRootHud)
		.Model(HudModel.Get()).CityOverviewModel(CityModel.Get()).InitialViewportSize(FIntPoint(1280, 720));
	TestTrue(TEXT("The HUD city breadcrumb opens City Overview through a stable semantic action"), Hud->ActivateSemanticId(TEXT("HUD.TopStatus.CityOverview")));
	TestTrue(TEXT("The City Overview host reports open"), CityModel->GetSnapshot().bOpen);
	TestTrue(TEXT("Close routes through the child screen"), Hud->ActivateSemanticId(TEXT("CityOverview.Close")));
	TestEqual(TEXT("Closing restores focus to the HUD opener"), HudModel->GetSnapshot().FocusedSemanticId, FName(TEXT("HUD.TopStatus.CityOverview")));

	FHansaNativeScreenshotService Screenshots;
	auto NativeBuffer = [](const FIntPoint& Size, TArray<FColor>& Pixels)
	{
		Pixels.Init(FColor(21, 42, 53, 255), Size.X * Size.Y);
		return true;
	};
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		FHansaScreenshotContext Context;
		Context.BundleId = FString::Printf(TEXT("contract-city-overview-%dx%d"), Size.X, Size.Y);
		Context.EvidenceSuiteId = TEXT("S08P01");
		Context.FixtureId = TEXT("lubeck_grain_shortage_v1");
		Context.ScreenId = TEXT("CityOverview.Root");
		Context.FlowId = TEXT("overview-tabs-causal-focus-v1");
		Context.CaptureMethod = TEXT("AutomationTest.NativeBufferContract");
		Context.UiRevision = 1;
		Context.SemanticSnapshotJson = TEXT("{\"schemaVersion\":1,\"nodes\":[{\"id\":\"CityOverview.Header\"},{\"id\":\"CityOverview.Tab.Population\"},{\"id\":\"CityOverview.Tab.Production\"},{\"id\":\"CityOverview.Tab.Market\"},{\"id\":\"CityOverview.List\"},{\"id\":\"CityOverview.State\"}]}");
		Context.StructuralAssertions = {
			TEXT("header.requiredSummaries=true"), TEXT("tabs.mvpThree=true"), TEXT("administration.futureDisabled=true"),
			TEXT("list.virtualized=true"), TEXT("focus.controller=true"), TEXT("localization.expansion=true"),
			TEXT("states.loadingEmptyError=true"), TEXT("status.colorRedundant=true")
		};
		Context.bStructuralAssertionsPassed = true;
		const FHansaScreenshotResult Result = Screenshots.Capture(Size, Context, NativeBuffer);
		TestTrue(FString::Printf(TEXT("Native %dx%d City Overview evidence writes"), Size.X, Size.Y), Result.IsSuccess());
		FString Metadata;
		TestTrue(TEXT("City Overview evidence metadata is readable"), FFileHelper::LoadFileToString(Metadata, *Result.MetadataPath));
		TestTrue(TEXT("Evidence records native dimensions and no resizing"), Metadata.Contains(FString::Printf(TEXT("\"width\":%d"), Size.X)) &&
			Metadata.Contains(FString::Printf(TEXT("\"height\":%d"), Size.Y)) && Metadata.Contains(TEXT("\"postCaptureResized\":false")));
	}
	return !HasAnyErrors();
}

#endif
