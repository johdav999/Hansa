#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

#include "Fixtures/HansaProductionFixture.h"
#include "Misc/AutomationTest.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/SHansaMarketTable.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	const Hansa::UI::FHansaHudSemanticNode* FindSelectedGoodNode(const TArray<Hansa::UI::FHansaHudSemanticNode>& Nodes, const FString& Id)
	{
		return Nodes.FindByPredicate([&Id](const Hansa::UI::FHansaHudSemanticNode& Node) { return Node.Id == Id; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSelectedGoodCausalProjectionTest,
	"Hansa.UI.SelectedGood.AuthoritativeCausalProjectionAndBoundedHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSelectedGoodCausalProjectionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!TestTrue(TEXT("The deterministic grain-shortage fixture initializes"), Created.IsSuccess())) return false;
	FHansaProductionFixture Fixture = Created.Value;
	if (!TestTrue(TEXT("The fixture produces several deterministic market samples"), Fixture.Step(25).IsSuccess())) return false;
	const auto Projection = Fixture.BuildProjection();
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const auto GoodId = FHansaGoodId::TryParse(TEXT("Good.Grain"));
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry();
	if (!Projection || !CityId || !GoodId || Registry == nullptr) return false;

	const FHansaCityMarketProjection* SourceMarket = nullptr;
	const FHansaMarketExplanationProjection* SourceExplanation = nullptr;
	for (const auto& Item : Projection.Value.GetMarkets()) if (Item.CityId == CityId.Value && Item.GoodId == GoodId.Value) SourceMarket = &Item;
	for (const auto& Item : Projection.Value.GetMarketExplanations()) if (Item.CityId == CityId.Value && Item.GoodId == GoodId.Value) SourceExplanation = &Item;
	if (!TestNotNull(TEXT("The aggregate read projection contains the Grain market"), SourceMarket) ||
		!TestNotNull(TEXT("The aggregate read projection contains authoritative causal factors"), SourceExplanation)) return false;

	TStrongObjectPtr<UHansaMarketTablePresentationModel> Model(NewObject<UHansaMarketTablePresentationModel>());
	Model->InitializeDefaults(); Model->ApplyProjection(Projection.Value, *Registry, CityId.Value); Model->SelectGoodIntent(TEXT("Good.Grain"));
	const FHansaSelectedGoodPresentation& Detail = Model->GetSnapshot().SelectedGood;
	TestTrue(TEXT("The selected panel exposes all required market metrics"), Detail.bHasSelection && Detail.bHasReport &&
		!Detail.BaseValue.IsEmpty() && !Detail.LocalPrice.IsEmpty() && !Detail.RecentAverageDifference.IsEmpty() &&
		!Detail.StockVersusReserve.IsEmpty() && !Detail.ReserveDays.IsEmpty() && !Detail.CitizenDemand.IsEmpty() &&
		!Detail.IndustrialDemand.IsEmpty() && !Detail.IncomingSupply.IsEmpty());
	TestEqual(TEXT("The chart is bounded to the simulation history contract"), Detail.History.Num(), FMath::Min(64, SourceMarket->PriceHistory.Num()));
	for (int32 Index = 0; Index < Detail.History.Num(); ++Index)
	{
		const int32 SourceIndex = SourceMarket->PriceHistory.Num() - Detail.History.Num() + Index;
		TestEqual(FString::Printf(TEXT("History sample %d keeps its authoritative tick"), Index), Detail.History[Index].Tick, SourceMarket->PriceHistory[SourceIndex].Tick.GetValue());
		TestEqual(FString::Printf(TEXT("History sample %d keeps its authoritative price"), Index), Detail.History[Index].PriceMilliMarks, SourceMarket->PriceHistory[SourceIndex].PriceMilliMarks);
		TestTrue(FString::Printf(TEXT("History sample %d has a bounded plot coordinate"), Index), Detail.History[Index].NormalizedPrice >= 0.0f && Detail.History[Index].NormalizedPrice <= 1.0f);
	}
	TestEqual(TEXT("Every authoritative causal factor is represented"), Detail.Factors.Num(), SourceExplanation->Factors.Num());
	for (int32 Index = 0; Index < Detail.Factors.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Factor %d keeps its signed authoritative contribution"), Index), Detail.Factors[Index].ContributionBasisPoints, SourceExplanation->Factors[Index].ContributionBasisPoints);
	}
	TestTrue(TEXT("Consumers are included from the aggregate projection"), !Detail.Consumers.IsEmpty());
	TestTrue(TEXT("Producers are included from the aggregate projection"), !Detail.Producers.IsEmpty());
	TestTrue(TEXT("The accessible chart summary states current and average values"), Detail.ChartSummary.ToString().Contains(TEXT("Current")) && Detail.ChartSummary.ToString().Contains(TEXT("average")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSelectedGoodActionsAndSemanticsTest,
	"Hansa.UI.SelectedGood.ActionsPersistenceAndSemanticData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSelectedGoodActionsAndSemanticsTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage(); if (!Created) return false;
	FHansaProductionFixture Fixture = Created.Value; if (!Fixture.Step(10)) return false;
	const auto Projection = Fixture.BuildProjection(); const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry(); if (!Projection || !CityId || Registry == nullptr) return false;
	TStrongObjectPtr<UHansaMarketTablePresentationModel> Model(NewObject<UHansaMarketTablePresentationModel>());
	Model->InitializeDefaults(); Model->ApplyProjection(Projection.Value, *Registry, CityId.Value); Model->SelectGoodIntent(TEXT("Good.Grain"));
	TestTrue(TEXT("Pin is a direct typed action"), Model->TogglePinIntent());
	TestTrue(TEXT("Pin state and feedback are explicit"), Model->GetSnapshot().SelectedGood.bPinned && !Model->GetSnapshot().SelectedGood.LastActionResult.IsEmpty());
	Model->ApplyProjection(Projection.Value, *Registry, CityId.Value);
	TestTrue(TEXT("Pin state survives authoritative refresh by stable good ID"), Model->GetSnapshot().SelectedGood.bPinned);
	bool bRouteRequested = false;
	Model->OnRouteRequested().AddLambda([&bRouteRequested](const FName GoodId){ bRouteRequested = GoodId == TEXT("Good.Grain"); });
	TestTrue(TEXT("Route workflow opens from the selected reported good in S09"), Model->BeginRouteIntent());
	TestTrue(TEXT("Route request preserves the selected good"), bRouteRequested);
	TestTrue(TEXT("Available route action has no obsolete prerequisite"), Model->GetSnapshot().SelectedGood.bRouteEnabled && Model->GetSnapshot().SelectedGood.RouteDisabledReason.IsEmpty());

	TSharedRef<Hansa::UI::SHansaMarketTable> Table = SNew(Hansa::UI::SHansaMarketTable).Model(Model.Get());
	const auto Nodes = Table->GetSemanticSnapshot();
	const auto* Chart = FindSelectedGoodNode(Nodes, TEXT("Market.Detail.Chart"));
	const auto* Point = FindSelectedGoodNode(Nodes, TEXT("Market.Detail.Chart.Point.0"));
	const auto* Pin = FindSelectedGoodNode(Nodes, TEXT("Market.Detail.Action.Pin"));
	const auto* Route = FindSelectedGoodNode(Nodes, TEXT("Market.Detail.Action.BeginRoute"));
	TestTrue(TEXT("The chart has an accessible summary and per-point semantic data"), Chart != nullptr && Chart->State.Value.Contains(TEXT("Current")) && Point != nullptr && Point->State.ValueType == TEXT("price-milli-marks"));
	TestTrue(TEXT("The pin action is semantic, focusable and selected"), Pin != nullptr && Pin->bCanActivate && Pin->bCanFocus && Pin->State.bSelected);
	TestTrue(TEXT("The route action is semantic and focusable"), Route != nullptr && Route->bCanActivate && Route->State.bEnabled && Route->bCanFocus);
	TestTrue(TEXT("Controller order reaches the enabled selected-good action"), Table->GetControllerFocusOrder().Contains(TEXT("Market.Detail.Action.Pin")));
	TestTrue(TEXT("Controller order reaches the route action"), Table->GetControllerFocusOrder().Contains(TEXT("Market.Detail.Action.BeginRoute")));
	return !HasAnyErrors();
}

#endif
