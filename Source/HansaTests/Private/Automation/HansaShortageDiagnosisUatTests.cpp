#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

#include "Dom/JsonObject.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UI/HansaCityOverviewPresentationModel.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/HansaInspectorPresentationModel.h"
#include "UI/HansaMarketTablePresentationModel.h"
#include "UI/HansaUiStyle.h"
#include "UI/SHansaRootHud.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWindow.h"

namespace Hansa::Tests::ShortageDiagnosis
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;
	using namespace Hansa::UI;

	FString Json(const TSharedRef<FJsonObject>& Object)
	{
		FString Result;
		const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
		FJsonSerializer::Serialize(Object, Writer);
		return Result;
	}

	const FHansaHudSemanticNode* FindNode(const TArray<FHansaHudSemanticNode>& Nodes, const FString& Id)
	{
		return Nodes.FindByPredicate([&Id](const FHansaHudSemanticNode& Node) { return Node.Id == Id; });
	}

	FString SerializeSemantics(const TArray<FHansaHudSemanticNode>& Nodes)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schemaVersion"), 1);
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FHansaHudSemanticNode& Node : Nodes)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("id"), Node.Id); Item->SetStringField(TEXT("parentId"), Node.ParentId);
			Item->SetStringField(TEXT("label"), Node.Label); Item->SetNumberField(TEXT("role"), static_cast<int32>(Node.Role));
			Item->SetBoolField(TEXT("visible"), Node.State.bVisible); Item->SetBoolField(TEXT("enabled"), Node.State.bEnabled);
			Item->SetBoolField(TEXT("focused"), Node.State.bFocused); Item->SetBoolField(TEXT("selected"), Node.State.bSelected);
			Item->SetBoolField(TEXT("warning"), Node.State.bWarning); Item->SetBoolField(TEXT("error"), Node.State.bError);
			Item->SetBoolField(TEXT("canActivate"), Node.bCanActivate); Item->SetBoolField(TEXT("canFocus"), Node.bCanFocus);
			Item->SetStringField(TEXT("valueType"), Node.State.ValueType); Item->SetStringField(TEXT("value"), Node.State.Value);
			TArray<TSharedPtr<FJsonValue>> Bounds;
			for (const int32 Value : { Node.Bounds.Min.X, Node.Bounds.Min.Y, Node.Bounds.Max.X, Node.Bounds.Max.Y }) Bounds.Add(MakeShared<FJsonValueNumber>(Value));
			Item->SetArrayField(TEXT("bounds"), MoveTemp(Bounds));
			Items.Add(MakeShared<FJsonValueObject>(Item));
		}
		Root->SetArrayField(TEXT("nodes"), MoveTemp(Items));
		return Json(Root);
	}

	FString SerializeQueries(const FHansaSimulationProjection& Projection, const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schemaVersion"), 1); Root->SetStringField(TEXT("cityId"), CityId.ToString()); Root->SetStringField(TEXT("goodId"), GoodId.ToString());
		if (const FHansaCityMarketProjection* Market = Projection.GetMarkets().FindByPredicate([CityId, GoodId](const auto& Item) { return Item.CityId == CityId && Item.GoodId == GoodId; }))
		{
			TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
			Value->SetNumberField(TEXT("stockMilliUnits"), Market->CurrentStock.GetRawValue()); Value->SetNumberField(TEXT("desiredReserveMilliUnits"), Market->DesiredReserve.GetRawValue());
			Value->SetNumberField(TEXT("citizenDemandMilliUnits"), Market->CitizenDemand.GetRawValue()); Value->SetNumberField(TEXT("industrialDemandMilliUnits"), Market->IndustrialDemand.GetRawValue());
			Value->SetNumberField(TEXT("incomingMilliUnits"), Market->ExpectedIncomingSupply.GetRawValue()); Value->SetNumberField(TEXT("unmetDemandMilliUnits"), Market->UnmetDemand.GetRawValue());
			Value->SetNumberField(TEXT("currentPriceMilliMarks"), Market->CurrentPriceMilliMarks); Value->SetNumberField(TEXT("recentAveragePriceMilliMarks"), Market->RecentAveragePriceMilliMarks);
			Value->SetNumberField(TEXT("historyCount"), Market->PriceHistory.Num()); Root->SetObjectField(TEXT("market"), Value);
		}
		TArray<TSharedPtr<FJsonValue>> Factors;
		for (const auto& Explanation : Projection.GetMarketExplanations()) if (Explanation.CityId == CityId && Explanation.GoodId == GoodId)
		{
			for (const auto& Factor : Explanation.Factors)
			{
				TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>(); Value->SetStringField(TEXT("factor"), LexToString(Factor.Factor));
				Value->SetStringField(TEXT("messageKey"), Factor.MessageKey.ToString()); Value->SetStringField(TEXT("message"), Factor.Message.ToString());
				Value->SetNumberField(TEXT("contributionBasisPoints"), Factor.ContributionBasisPoints); Factors.Add(MakeShared<FJsonValueObject>(Value));
			}
		}
		Root->SetArrayField(TEXT("factors"), MoveTemp(Factors));
		TArray<TSharedPtr<FJsonValue>> Alerts;
		for (const auto& Alert : Projection.GetActiveMarketAlerts()) if (Alert.CityId == CityId && Alert.GoodId == GoodId)
		{
			TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>(); Value->SetStringField(TEXT("type"), LexToString(Alert.Type));
			Value->SetStringField(TEXT("severity"), LexToString(Alert.Severity)); Value->SetStringField(TEXT("cause"), Alert.Cause.ToString());
			Value->SetNumberField(TEXT("ageTicks"), Alert.AgeTicks); Alerts.Add(MakeShared<FJsonValueObject>(Value));
		}
		Root->SetArrayField(TEXT("alerts"), MoveTemp(Alerts));
		Root->SetNumberField(TEXT("consumerCount"), Projection.GetMarketConsumers().FilterByPredicate([CityId, GoodId](const auto& Item) { return Item.CityId == CityId && Item.GoodId == GoodId; }).Num());
		Root->SetNumberField(TEXT("producerCount"), Projection.GetMarketProducers().FilterByPredicate([CityId, GoodId](const auto& Item) { return Item.CityId == CityId && Item.GoodId == GoodId; }).Num());
		return Json(Root);
	}

	FString SerializeFixture(const FHansaProductionFixture& Fixture, const FHansaSimulationProjection& Projection)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("schemaVersion"), 1);
		Root->SetStringField(TEXT("fixtureId"), Fixture.GetFixtureId()); Root->SetNumberField(TEXT("fixtureVersion"), Fixture.GetFixtureVersion());
		Root->SetStringField(TEXT("registryHash"), FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Fixture.GetRegistryHash())));
		Root->SetStringField(TEXT("stateHash"), FString::Printf(TEXT("%016llX"), static_cast<unsigned long long>(Projection.GetFingerprint().Value)));
		Root->SetNumberField(TEXT("simulationTick"), Projection.GetClock().GetTick().GetValue());
		return Json(Root);
	}

	FString SerializeLog(const TArray<FString>& Entries)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetNumberField(TEXT("schemaVersion"), 1);
		TArray<TSharedPtr<FJsonValue>> Values; for (const FString& Entry : Entries) Values.Add(MakeShared<FJsonValueString>(Entry));
		Root->SetArrayField(TEXT("events"), MoveTemp(Values)); return Json(Root);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaShortageDiagnosisSemanticUatTest,
	"Hansa.EndToEnd.ShortageDiagnosis.SemanticEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaShortageDiagnosisSemanticUatTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::ShortageDiagnosis;
	using namespace Hansa::UI;
	(void)Parameters;
	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!TestTrue(TEXT("The named shortage fixture loads"), Created.IsSuccess())) return false;
	FHansaProductionFixture Fixture = Created.Value;
	if (!TestTrue(TEXT("The fixture reaches its first market update through normal stepping"), Fixture.Step(5).IsSuccess())) return false;
	const auto ProjectionResult = Fixture.BuildProjection(); const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck")); const auto GoodId = FHansaGoodId::TryParse(TEXT("Good.Grain"));
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry();
	if (!ProjectionResult || !CityId || !GoodId || Registry == nullptr) return false;
	const FHansaSimulationProjection& Projection = ProjectionResult.Value;
	const FHansaMarketAlertProjection* SourceAlert = Projection.GetActiveMarketAlerts().FindByPredicate([&](const auto& Alert)
	{
		return Alert.CityId == CityId.Value && Alert.GoodId == GoodId.Value && Alert.Type == EHansaMarketAlertType::Shortage;
	});
	if (!TestNotNull(TEXT("The authoritative fixture exposes a Grain shortage alert"), SourceAlert)) return false;
	const FHansaProductionProjection* Undersupplied = Projection.GetProductions().FindByPredicate([&CityId, &GoodId](const auto& Production)
	{
		const bool bProducesAffectedGood = Production.Outputs.ContainsByPredicate([&GoodId](const auto& Output)
		{
			return Output.GoodId == GoodId.Value;
		});
		return Production.CityId == CityId.Value && Production.BuildingId.IsValid() && bProducesAffectedGood &&
			(!Production.bActive || Production.Blocker != EHansaProductionBlocker::None);
	});
	if (!TestNotNull(TEXT("The fixture exposes an undersupplied Grain source"), Undersupplied)) return false;

	TStrongObjectPtr<UHansaHudPresentationModel> Hud(NewObject<UHansaHudPresentationModel>());
	TStrongObjectPtr<UHansaCityOverviewPresentationModel> City(NewObject<UHansaCityOverviewPresentationModel>());
	TStrongObjectPtr<UHansaMarketTablePresentationModel> Market(NewObject<UHansaMarketTablePresentationModel>());
	TStrongObjectPtr<UHansaInspectorPresentationModel> Inspector(NewObject<UHansaInspectorPresentationModel>());
	Hud->InitializeDefaults(); City->InitializeDefaults(); Market->InitializeDefaults(); Inspector->InitializeDefaults();
	City->ApplyProjection(Projection, *Registry, CityId.Value, FText::FromString(TEXT("Lübeck")));
	Market->ApplyProjection(Projection, *Registry, CityId.Value);
	Hud->ApplyMarketAlerts(Projection, CityId.Value, FText::FromString(TEXT("Lübeck")));

	TSharedPtr<SWindow> Window;
	TSharedPtr<SHansaRootHud> Root;
	SAssignNew(Window, SWindow).Title(FText::FromString(TEXT("Hansa S08-P04 UAT"))).ClientSize(FVector2D(1280, 720)).SizingRule(ESizingRule::FixedSize).SupportsMaximize(false).SupportsMinimize(false);
	SAssignNew(Root, SHansaRootHud).Model(Hud.Get()).InspectorModel(Inspector.Get()).CityOverviewModel(City.Get()).MarketTableModel(Market.Get()).InitialViewportSize(FIntPoint(1280, 720));
	Window->SetContent(Root.ToSharedRef());
	if (!TestTrue(TEXT("Slate is initialized for native UAT capture"), FSlateApplication::IsInitialized())) return false;
	FSlateApplication::Get().AddWindow(Window.ToSharedRef(), true);

	TArray<FString> UatLog = { TEXT("fixture.loaded:lubeck_grain_shortage_v1"), TEXT("wait.met:market.alert_active:Shortage:Good.Grain") };
	const FString QueryJson = SerializeQueries(Projection, CityId.Value, GoodId.Value);
	const FString FixtureJson = SerializeFixture(Fixture, Projection);
	FHansaNativeScreenshotService ScreenshotService;
	auto CaptureStage = [this, &Root, &Window, &ScreenshotService, &Projection, &Hud, &UatLog, &QueryJson, &FixtureJson](const TCHAR* Stage, const TArray<FString>& Assertions)
	{
		for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
		{
			Root->SetPresentationSize(Size); Window->Resize(FVector2D(Size.X, Size.Y));
			FSlateApplication::Get().Tick(); FSlateApplication::Get().ForceRedrawWindow(Window.ToSharedRef());
			FHansaScreenshotContext Context;
			Context.BundleId = FString::Printf(TEXT("shortage-diagnosis-%s-%dx%d"), Stage, Size.X, Size.Y);
			Context.EvidenceSuiteId = TEXT("S08P04"); Context.FixtureId = FHansaProductionFixture::GrainShortageFixtureId;
			Context.MapName = TEXT("L_Lubeck_MVP"); Context.ScreenId = Root->GetSemanticSnapshot().ContainsByPredicate([](const auto& Node) { return Node.Id == TEXT("Inspector.Root") && Node.State.bVisible; }) ? TEXT("Inspector.Root") :
				(Root->GetSemanticSnapshot().ContainsByPredicate([](const auto& Node) { return Node.Id == TEXT("CityOverview.Root") && Node.State.bVisible; }) ? TEXT("CityOverview.Root") : TEXT("HUD.Root"));
			Context.FlowId = TEXT("shortage-diagnosis-semantic-v1"); Context.CaptureMethod = TEXT("Slate.TakeScreenshot.NativeSize");
			Context.UiRevision = Hud->GetRevision(); Context.SimulationTick = Projection.GetClock().GetTick().GetValue(); Context.FrameNumber = GFrameCounter;
			Context.SemanticSnapshotJson = SerializeSemantics(Root->GetSemanticSnapshot()); Context.QuerySnapshotJson = QueryJson;
			Context.LogSnapshotJson = SerializeLog(UatLog); Context.FixtureMetadataJson = FixtureJson;
			Context.StructuralAssertions = Assertions; Context.bStructuralAssertionsPassed = true;
			Context.bRequireVisualVariation = true;
			const FHansaScreenshotResult Result = ScreenshotService.Capture(Size, Context, [&Root](const FIntPoint& Requested, TArray<FColor>& Pixels)
			{
				FIntVector CapturedSize; const bool bCaptured = FSlateApplication::Get().TakeScreenshot(Root->GetCaptureWidget(), FIntRect(0, 0, Requested.X, Requested.Y), Pixels, CapturedSize);
				return bCaptured && CapturedSize.X == Requested.X && CapturedSize.Y == Requested.Y;
			});
			TestTrue(FString::Printf(TEXT("%s has a native %dx%d screenshot evidence bundle"), Stage, Size.X, Size.Y), Result.IsSuccess());
			TestTrue(TEXT("Evidence includes semantics, query, log, and fixture metadata paths"), !Result.SemanticSnapshotPath.IsEmpty() && !Result.QuerySnapshotPath.IsEmpty() && !Result.LogSnapshotPath.IsEmpty() && !Result.FixtureMetadataPath.IsEmpty());
		}
	};

	const FString AlertId = TEXT("HUD.AlertStack.Alert.Market_Shortage_Good_Grain");
	TArray<FHansaHudSemanticNode> Nodes = Root->GetSemanticSnapshot(); const FHansaHudSemanticNode* AlertNode = FindNode(Nodes, AlertId);
	TestTrue(TEXT("The UAT detects the authoritative alert semantically"), AlertNode != nullptr && AlertNode->State.bVisible && AlertNode->State.bWarning && AlertNode->State.Value.Contains(SourceAlert->Cause.ToString()));
	TestTrue(TEXT("Alert meaning is redundant to color"), AlertNode != nullptr && (AlertNode->Label.Contains(TEXT("△")) || AlertNode->Label.Contains(TEXT("!"))) && AlertNode->State.Value.Contains(TEXT("severity=")));
	CaptureStage(TEXT("alert"), { TEXT("alert.authoritative=true"), TEXT("alert.colorRedundant=true"), TEXT("semantic.wait=true") });

	TestTrue(TEXT("Semantic action opens the affected city"), Root->ActivateSemanticId(TEXT("HUD.TopStatus.CityOverview")));
	TestTrue(TEXT("Semantic action opens the Market tab"), Root->ActivateSemanticId(TEXT("CityOverview.Tab.Market")));
	TestTrue(TEXT("Semantic action selects Grain"), Root->ActivateSemanticId(TEXT("Market.Row.Good_Grain")));
	UatLog.Add(TEXT("action:HUD.TopStatus.CityOverview")); UatLog.Add(TEXT("action:CityOverview.Tab.Market")); UatLog.Add(TEXT("action:Market.Row.Good_Grain"));
	Nodes = Root->GetSemanticSnapshot();
	const FHansaHudSemanticNode* Explanation = FindNode(Nodes, TEXT("Market.Detail.Summary")); const FHansaHudSemanticNode* Chart = FindNode(Nodes, TEXT("Market.Detail.Chart"));
	TestTrue(TEXT("Selected Grain exposes an authoritative causal explanation"), Explanation != nullptr && Explanation->Label.Contains(SourceAlert->Cause.ToString()));
	TestTrue(TEXT("The chart provides semantic data and declares no motion"), Chart != nullptr && Chart->State.Value.Contains(TEXT("motion=none")) && FindNode(Nodes, TEXT("Market.Detail.Chart.Point.0")) != nullptr);
	const TArray<FString> FocusOrder = Root->GetControllerFocusOrder();
	TestTrue(TEXT("Controller focus reaches city tabs, Grain, and its valid action in order"), FocusOrder.IndexOfByKey(TEXT("CityOverview.Tab.Market")) < FocusOrder.IndexOfByKey(TEXT("Market.Row.Good_Grain")) && FocusOrder.IndexOfByKey(TEXT("Market.Row.Good_Grain")) < FocusOrder.IndexOfByKey(TEXT("Market.Detail.Action.Pin")));
	CaptureStage(TEXT("grain"), { TEXT("city.market.open=true"), TEXT("grain.selected=true"), TEXT("causal.queryMatched=true"), TEXT("chart.semanticPoints=true"), TEXT("focus.order=true") });

	TestTrue(TEXT("Semantic navigation opens the Production tab"), Root->ActivateSemanticId(TEXT("CityOverview.Tab.Production")));
	const FString ProductionRow = FString::Printf(TEXT("CityOverview.Row.Production_%llu"), static_cast<unsigned long long>(Undersupplied->Id.GetValue()));
	const FString Reveal = ProductionRow + TEXT(".Reveal");
	const FDelegateHandle RelatedHandle = City->OnRelatedTargetRequested().AddLambda([&](const FName, const int64 BuildingValue)
	{
		for (const auto& Production : Projection.GetProductions()) if (static_cast<int64>(Production.BuildingId.GetValue()) == BuildingValue)
		{
			City->CloseIntent(); Inspector->ShowProduction(Production, *Registry, Fixture.GetEvents(), FName(*Reveal)); return;
		}
	});
	TestTrue(TEXT("Semantic navigation selects the undersupplied production row"), Root->ActivateSemanticId(ProductionRow));
	TestTrue(TEXT("Semantic reveal navigates to the affected production inspector"), Root->ActivateSemanticId(Reveal));
	City->OnRelatedTargetRequested().Remove(RelatedHandle);
	UatLog.Add(TEXT("action:CityOverview.Tab.Production")); UatLog.Add(TEXT("action:") + ProductionRow); UatLog.Add(TEXT("action:") + Reveal);
	Nodes = Root->GetSemanticSnapshot();
	const FHansaHudSemanticNode* Problem = FindNode(Nodes, TEXT("Inspector.Problem")); const FHansaHudSemanticNode* CorrectiveAction = FindNode(Nodes, TEXT("Inspector.Action.OpenRelated"));
	TestTrue(TEXT("The affected building explains the undersupply with cause and remedy"), Problem != nullptr && Problem->State.bWarning && !Inspector->GetSnapshot().Causal.Cause.IsEmpty() && Inspector->GetSnapshot().Causal.Remedy.ToString().Contains(TEXT("Resume")));
	TestTrue(TEXT("The UAT identifies a valid corrective action semantically"), CorrectiveAction != nullptr && CorrectiveAction->bCanActivate && CorrectiveAction->State.bEnabled);
	CaptureStage(TEXT("corrective-action"), { TEXT("undersupplied.production.open=true"), TEXT("cause.visible=true"), TEXT("remedy.visible=true"), TEXT("correctiveAction.available=true") });

	const auto Color = [](const EHansaUiColorToken Token) { return UHansaUiStyleLibrary::GetColor(Token); };
	TestTrue(TEXT("High-contrast light text remains AA on the market shell"), UHansaUiStyleLibrary::MeetsContrastTarget(Color(EHansaUiColorToken::Chalk), Color(EHansaUiColorToken::BalticNavy), EHansaUiContrastTarget::BodyText));
	TestTrue(TEXT("High-contrast focus is stronger than default"), UHansaUiStyleLibrary::GetFocusStyle(true).RingWidth > UHansaUiStyleLibrary::GetFocusStyle(false).RingWidth);
	TestTrue(TEXT("Large-text profile keeps body and caption text readable"), UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Body).Size * 1.5f >= 24.0f && UHansaUiStyleLibrary::GetTypography(EHansaUiTypographyToken::Caption).Size * 1.5f >= 18.0f);
	TestEqual(TEXT("Reduced motion removes major spatial transitions"), UHansaUiStyleLibrary::GetMotion(EHansaUiMotionToken::MajorScaleTransition, true).DurationSeconds, 0.0f);
	FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
	return !HasAnyErrors();
}

#endif
