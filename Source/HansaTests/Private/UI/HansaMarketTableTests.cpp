#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

#include "Fixtures/HansaProductionFixture.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/SHansaCityOverview.h"
#include "UI/SHansaMarketTable.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	const Hansa::UI::FHansaHudSemanticNode* FindNode(const TArray<Hansa::UI::FHansaHudSemanticNode>& Nodes, const FString& Id)
	{
		return Nodes.FindByPredicate([&Id](const Hansa::UI::FHansaHudSemanticNode& Node) { return Node.Id == Id; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaMarketTableProjectionStateTest,
	"Hansa.UI.MarketTable.TenGoodsProjectionAndReportStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketTableProjectionStateTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaMarketTablePresentationModel> Model(NewObject<UHansaMarketTablePresentationModel>());
	Model->InitializeDefaults();
	TestEqual(TEXT("The empty runtime still exposes exactly the ten canonical MVP goods"), Model->GetSnapshot().AllRows.Num(), 10);
	TestTrue(TEXT("Unavailable market data is explicit instead of fabricated zeroes"), Model->GetSnapshot().AllRows.ContainsByPredicate([](const auto& Row)
	{
		return Row.GoodStableId == TEXT("Good.Grain") && Row.bUnknown && Row.Stock.ToString() == TEXT("—") && Row.Status.ToString().Contains(TEXT("No recent report"));
	}));

	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!TestTrue(TEXT("The deterministic grain-shortage fixture initializes"), Created.IsSuccess())) return false;
	FHansaProductionFixture Fixture = Created.Value;
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry();
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const auto InitialProjection = Fixture.BuildProjection();
	if (Registry == nullptr || !CityId || !InitialProjection) return false;
	TestTrue(TEXT("The initial authoritative stale report is accepted"), Model->ApplyProjection(InitialProjection.Value, *Registry, CityId.Value));
	TestTrue(TEXT("Stale report rows are explicitly marked estimated with their age"), Model->GetSnapshot().AllRows.ContainsByPredicate([](const auto& Row)
	{
		return Row.bStale && Row.bEstimated && Row.Price.ToString().StartsWith(TEXT("≈")) && Row.Status.ToString().Contains(TEXT("estimated")) && Row.Status.ToString().Contains(TEXT("ticks"));
	}));

	if (!TestTrue(TEXT("The fixture advances through the authoritative command gateway"), Fixture.Step(5).IsSuccess())) return false;
	const auto CurrentProjection = Fixture.BuildProjection();
	if (!CurrentProjection) return false;
	TestTrue(TEXT("The current market projection is accepted"), Model->ApplyProjection(CurrentProjection.Value, *Registry, CityId.Value));
	TestEqual(TEXT("Projection refresh preserves exactly ten rows"), Model->GetSnapshot().AllRows.Num(), 10);
	const auto* Grain = Model->FindRow(TEXT("Good.Grain"));
	TestTrue(TEXT("The grain row is typed and fully labelled"), Grain != nullptr && !Grain->bUnknown && !Grain->GoodGlyph.IsEmpty() &&
		!Grain->Stock.IsEmpty() && !Grain->Reserve.IsEmpty() && !Grain->Demand.IsEmpty() && !Grain->Price.IsEmpty() &&
		!Grain->Trend.IsEmpty() && !Grain->Incoming.IsEmpty() && Grain->AccessibleLabel.ToString().Contains(TEXT("Reserve target")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaMarketTableIntentTest,
	"Hansa.UI.MarketTable.SortFilterSearchAndStableSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketTableIntentTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage(); if (!Created) return false;
	FHansaProductionFixture Fixture = Created.Value; if (!Fixture.Step(5)) return false;
	const auto Projection = Fixture.BuildProjection(); const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry(); if (!Projection || !CityId || Registry == nullptr) return false;
	TStrongObjectPtr<UHansaMarketTablePresentationModel> Model(NewObject<UHansaMarketTablePresentationModel>());
	Model->InitializeDefaults(); Model->ApplyProjection(Projection.Value, *Registry, CityId.Value);

	TestTrue(TEXT("Selection uses the canonical stable ID"), Model->SelectGoodIntent(TEXT("Good.Grain")));
	TestTrue(TEXT("Search matches localized labels without mutating source rows"), Model->SetSearchTextIntent(FText::FromString(TEXT("grain"))));
	TestEqual(TEXT("Search leaves one matching result"), Model->GetSnapshot().VisibleRows.Num(), 1);
	TestEqual(TEXT("Search preserves the selected stable ID"), Model->GetSnapshot().SelectedGoodStableId, FName(TEXT("Good.Grain")));
	TestTrue(TEXT("Clear filters restores all canonical rows"), Model->ClearFiltersIntent());
	TestEqual(TEXT("All ten goods return after clearing"), Model->GetSnapshot().VisibleRows.Num(), 10);

	TestTrue(TEXT("Category filtering advances through a typed intent"), Model->CycleCategoryFilterIntent());
	TestTrue(TEXT("Food category returns only food goods"), !Model->GetSnapshot().VisibleRows.IsEmpty() && Model->GetSnapshot().VisibleRows.ContainsByPredicate([](const auto& Row)
	{
		return Row.Category == EHansaMarketGoodCategory::Food;
	}) && !Model->GetSnapshot().VisibleRows.ContainsByPredicate([](const auto& Row)
	{
		return Row.Category != EHansaMarketGoodCategory::Food;
	}));
	Model->ClearFiltersIntent();

	TestTrue(TEXT("Price header selects ascending price order"), Model->SortByIntent(EHansaMarketSortColumn::Price));
	const auto& Ascending = Model->GetSnapshot().VisibleRows;
	bool bAscending = true;
	for (int32 Index = 1; Index < Ascending.Num(); ++Index) bAscending &= Ascending[Index - 1].PriceRaw <= Ascending[Index].PriceRaw;
	TestTrue(TEXT("Price order is ascending"), bAscending);
	TestTrue(TEXT("Activating the same header reverses order"), Model->SortByIntent(EHansaMarketSortColumn::Price));
	const auto& Descending = Model->GetSnapshot().VisibleRows;
	bool bDescending = true;
	for (int32 Index = 1; Index < Descending.Num(); ++Index) bDescending &= Descending[Index - 1].PriceRaw >= Descending[Index].PriceRaw;
	TestTrue(TEXT("Price order is descending"), bDescending);

	const uint64 Revision = Model->GetRevision();
	TestTrue(TEXT("An identical authoritative refresh is accepted"), Model->ApplyProjection(Projection.Value, *Registry, CityId.Value));
	TestEqual(TEXT("An identical refresh emits no redundant revision"), Model->GetRevision(), Revision);
	TestEqual(TEXT("Selection survives sort and authoritative refresh"), Model->GetSnapshot().SelectedGoodStableId, FName(TEXT("Good.Grain")));
	Model->SetSearchTextIntent(FText::FromString(TEXT("does-not-exist")));
	TestTrue(TEXT("A filtered-empty state explains cause and remedy"), Model->GetSnapshot().VisibleRows.IsEmpty() &&
		Model->GetSnapshot().EmptyTitle.ToString().Contains(TEXT("No goods")) && Model->GetSnapshot().EmptyDetail.ToString().Contains(TEXT("filters")));
	TestEqual(TEXT("Hidden selection remains stable for filter recovery"), Model->GetSnapshot().SelectedGoodStableId, FName(TEXT("Good.Grain")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaMarketTableSemanticWidgetTest,
	"Hansa.UI.MarketTable.VirtualizationSemanticsAndCityIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketTableSemanticWidgetTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage(); if (!Created) return false;
	const auto Projection = Created.Value.BuildProjection(); const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const FHansaEconomicRegistry* Registry = Created.Value.GetDefinitions().GetEconomicRegistry(); if (!Projection || !CityId || Registry == nullptr) return false;
	TStrongObjectPtr<UHansaMarketTablePresentationModel> MarketModel(NewObject<UHansaMarketTablePresentationModel>());
	MarketModel->InitializeDefaults(); MarketModel->ApplyProjection(Projection.Value, *Registry, CityId.Value);
	TSharedRef<Hansa::UI::SHansaMarketTable> Table = SNew(Hansa::UI::SHansaMarketTable).Model(MarketModel.Get());
	const TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Table->GetSemanticSnapshot();
	TestTrue(TEXT("The table is exposed as a virtualized list"), FindNode(Nodes, TEXT("Market.List")) != nullptr && FindNode(Nodes, TEXT("Market.List"))->Role == Hansa::UI::EHansaHudSemanticRole::List);
	TestTrue(TEXT("Search and all three filters are semantic controls"), FindNode(Nodes, TEXT("Market.Search")) != nullptr &&
		FindNode(Nodes, TEXT("Market.Filter.Category")) != nullptr && FindNode(Nodes, TEXT("Market.Filter.Trend")) != nullptr && FindNode(Nodes, TEXT("Market.Filter.Quick")) != nullptr);
	const FString GrainRowId = TEXT("Market.Row.Good_Grain");
	const auto* GrainNode = FindNode(Nodes, GrainRowId);
	TestTrue(TEXT("A market row has a screen-reader-equivalent full label"), GrainNode != nullptr && GrainNode->Label.Contains(TEXT("Stock")) && GrainNode->Label.Contains(TEXT("Price")) && GrainNode->Label.Contains(TEXT("Report")));
	for (const TCHAR* Cell : { TEXT("Stock"), TEXT("Reserve"), TEXT("Demand"), TEXT("Price"), TEXT("Trend"), TEXT("Incoming"), TEXT("Status") })
	{
		TestNotNull(FString::Printf(TEXT("Grain %s cell has a semantic node"), Cell), FindNode(Nodes, GrainRowId + TEXT(".") + Cell));
	}
	TestTrue(TEXT("A mouse-style click anywhere on a list row selects that good"), Table->SelectListItemForTesting(TEXT("Good.Beer")));
	TestEqual(TEXT("Row-wide selection reaches the presentation model"), MarketModel->GetSnapshot().SelectedGoodStableId, FName(TEXT("Good.Beer")));
	TestTrue(TEXT("Row-wide selection opens the selected-good details state"), MarketModel->GetSnapshot().SelectedGood.bHasSelection);
	const auto* SelectedDetailNode = FindNode(Table->GetSemanticSnapshot(), TEXT("Market.Detail"));
	TestTrue(TEXT("Row-wide selection makes the details panel visible"), SelectedDetailNode != nullptr && SelectedDetailNode->State.bVisible);
	TestTrue(TEXT("Virtualized rows can receive focus before row widgets are materialized"), Table->FocusSemanticId(GrainRowId));
	TestTrue(TEXT("Semantic activation selects by stable ID"), Table->ActivateSemanticId(GrainRowId));
	TestEqual(TEXT("Selection is recorded independently of focus"), MarketModel->GetSnapshot().SelectedGoodStableId, FName(TEXT("Good.Grain")));
	const int32 Refreshes = Table->GetListRefreshCountForTesting();
	Table->ActivateSemanticId(TEXT("Market.Header.Price"));
	TestTrue(TEXT("Semantic sorting refreshes the virtualized item source"), Table->GetListRefreshCountForTesting() > Refreshes);

	TStrongObjectPtr<UHansaCityOverviewPresentationModel> CityModel(NewObject<UHansaCityOverviewPresentationModel>());
	CityModel->InitializeDefaults(); CityModel->Open();
	CityModel->ApplyProjection(Projection.Value, *Registry, CityId.Value, FText::FromString(TEXT("Lübeck")));
	TSharedRef<Hansa::UI::SHansaCityOverview> City = SNew(Hansa::UI::SHansaCityOverview).Model(CityModel.Get()).MarketTableModel(MarketModel.Get());
	TestTrue(TEXT("The normal City Overview intent opens Market"), City->ActivateSemanticId(TEXT("CityOverview.Tab.Market")));
	TestNotNull(TEXT("The Market table is included in City Overview semantics"), FindNode(City->GetSemanticSnapshot(), TEXT("Market.Root")));
	TestTrue(TEXT("City Overview routes market row actions to the typed market model"), City->ActivateSemanticId(TEXT("Market.Row.Good_Salt")));
	TestEqual(TEXT("The routed action selects Salt by stable ID"), MarketModel->GetSnapshot().SelectedGoodStableId, FName(TEXT("Good.Salt")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaMarketTableRefreshBudgetTest,
	"Hansa.Integration.Performance.MarketListRefreshBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketTableRefreshBudgetTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!Created) return false;
	FHansaProductionFixture Fixture = Created.Value;
	if (!Fixture.Step(5)) return false;
	const auto Projection = Fixture.BuildProjection();
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry();
	if (!Projection || !CityId || Registry == nullptr) return false;

	TStrongObjectPtr<UHansaMarketTablePresentationModel> Model(NewObject<UHansaMarketTablePresentationModel>());
	Model->InitializeDefaults();
	Model->ApplyProjection(Projection.Value, *Registry, CityId.Value);
	TSharedRef<Hansa::UI::SHansaMarketTable> Table = SNew(Hansa::UI::SHansaMarketTable).Model(Model.Get());
	const uint64 InitialRevision = Model->GetRevision();
	const int32 InitialRefreshes = Table->GetListRefreshCountForTesting();
	constexpr int32 IdenticalUpdates = 1'000;
	bool bAllUpdatesAccepted = true;
	const double IdenticalStartedAt = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < IdenticalUpdates; ++Index)
	{
		bAllUpdatesAccepted &= Model->ApplyProjection(Projection.Value, *Registry, CityId.Value);
	}
	const double IdenticalMilliseconds = (FPlatformTime::Seconds() - IdenticalStartedAt) * 1000.0;
	TestTrue(TEXT("Every repeated authoritative list update is accepted"), bAllUpdatesAccepted);
	TestEqual(TEXT("Unchanged projections publish no presentation revision"), Model->GetRevision(), InitialRevision);
	TestEqual(TEXT("Unchanged projections trigger no virtualized-list refresh"),
		Table->GetListRefreshCountForTesting(), InitialRefreshes);

	constexpr int32 SortUpdates = 200;
	const double SortStartedAt = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < SortUpdates; ++Index)
	{
		Model->SortByIntent(EHansaMarketSortColumn::Price);
	}
	const double SortMilliseconds = (FPlatformTime::Seconds() - SortStartedAt) * 1000.0;
	TestEqual(TEXT("Each real sort change refreshes the virtualized source exactly once"),
		Table->GetListRefreshCountForTesting(), InitialRefreshes + SortUpdates);
	AddInfo(FString::Printf(
		TEXT("S14-P03 market list: %d identical updates in %.3f ms (zero refreshes); %d sorts in %.3f ms"),
		IdenticalUpdates, IdenticalMilliseconds, SortUpdates, SortMilliseconds));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaMarketTableNativeEvidenceTest,
	"Hansa.UI.MarketTable.NativeResolutionEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMarketTableNativeEvidenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	(void)Parameters;
	FHansaNativeScreenshotService Screenshots;
	auto NativeBuffer = [](const FIntPoint& Size, TArray<FColor>& Pixels)
	{
		Pixels.Init(FColor(21, 42, 53, 255), Size.X * Size.Y); return true;
	};
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		FHansaScreenshotContext Context;
		Context.BundleId = FString::Printf(TEXT("contract-market-table-%dx%d"), Size.X, Size.Y);
		Context.EvidenceSuiteId = TEXT("S08P02"); Context.FixtureId = TEXT("lubeck_grain_shortage_v1");
		Context.ScreenId = TEXT("Market.Root"); Context.FlowId = TEXT("search-sort-filter-selection-v1");
		Context.CaptureMethod = TEXT("AutomationTest.NativeBufferContract"); Context.UiRevision = 1;
		Context.SemanticSnapshotJson = TEXT("{\"schemaVersion\":1,\"nodes\":[{\"id\":\"Market.Search\"},{\"id\":\"Market.Header.Price\"},{\"id\":\"Market.List\"},{\"id\":\"Market.Row.Good_Grain\"},{\"id\":\"Market.Empty\"}]}");
		Context.StructuralAssertions = { TEXT("goods.exactlyTen=true"), TEXT("list.virtualized=true"), TEXT("header.stickySortable=true"),
			TEXT("selection.stableId=true"), TEXT("reports.staleEstimatedUnknown=true"), TEXT("status.colorRedundant=true"), TEXT("semantics.rowsCellsActions=true") };
		Context.bStructuralAssertionsPassed = true;
		const FHansaScreenshotResult Result = Screenshots.Capture(Size, Context, NativeBuffer);
		TestTrue(FString::Printf(TEXT("Native %dx%d market evidence writes"), Size.X, Size.Y), Result.IsSuccess());
		FString Metadata; TestTrue(TEXT("Market evidence metadata is readable"), FFileHelper::LoadFileToString(Metadata, *Result.MetadataPath));
		TestTrue(TEXT("Evidence records native dimensions without resampling"), Metadata.Contains(FString::Printf(TEXT("\"width\":%d"), Size.X)) &&
			Metadata.Contains(FString::Printf(TEXT("\"height\":%d"), Size.Y)) && Metadata.Contains(TEXT("\"postCaptureResized\":false")));
	}
	return !HasAnyErrors();
}

#endif
