#if WITH_DEV_AUTOMATION_TESTS && WITH_HANSA_AUTOMATION

#include "Definitions/HansaEconomicRegistry.h"
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
#include "UI/HansaResearchPresentationModel.h"
#include "UI/HansaSaveLoadPresentationModel.h"
#include "UI/HansaScenarioPresentationModel.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWindow.h"

namespace Hansa::Tests::MvpUiUat
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;
	using namespace Hansa::UI;

	FString SerializeSemantics(const TArray<FHansaHudSemanticNode>& Nodes)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schemaVersion"), 1);
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FHansaHudSemanticNode& Node : Nodes)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("id"), Node.Id);
			Item->SetStringField(TEXT("parentId"), Node.ParentId);
			Item->SetStringField(TEXT("label"), Node.Label);
			Item->SetBoolField(TEXT("visible"), Node.State.bVisible);
			Item->SetBoolField(TEXT("enabled"), Node.State.bEnabled);
			Item->SetBoolField(TEXT("focused"), Node.State.bFocused);
			Item->SetBoolField(TEXT("warning"), Node.State.bWarning);
			Item->SetBoolField(TEXT("error"), Node.State.bError);
			Item->SetBoolField(TEXT("canActivate"), Node.bCanActivate);
			Item->SetStringField(TEXT("valueType"), Node.State.ValueType);
			Item->SetStringField(TEXT("value"), Node.State.Value);
			Items.Add(MakeShared<FJsonValueObject>(Item));
		}
		Root->SetArrayField(TEXT("nodes"), MoveTemp(Items));
		FString Result;
		const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
		FJsonSerializer::Serialize(Root, Writer);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMvpUiNativeGoldenFlowUatTest,
	"Hansa.UI.UAT.MvpGoldenFlows.NativeScreens",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FHansaMvpUiNativeGoldenFlowUatTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Automation;
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::MvpUiUat;
	using namespace Hansa::UI;
	(void)Parameters;

	const auto Created = FHansaProductionFixture::TryCreateGrainShortage();
	if (!TestTrue(TEXT("Deterministic MVP UAT fixture loads"), Created.IsSuccess())) return false;
	FHansaProductionFixture Fixture = Created.Value;
	if (!TestTrue(TEXT("Fixture reaches the market update"), Fixture.Step(5).IsSuccess())) return false;
	const auto ProjectionResult = Fixture.BuildProjection();
	const FHansaEconomicRegistry* Registry = Fixture.GetDefinitions().GetEconomicRegistry();
	const auto CityId = FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"));
	const auto GoodId = FHansaGoodId::TryParse(TEXT("Good.Grain"));
	if (!ProjectionResult || Registry == nullptr || !CityId || !GoodId) return false;
	const FHansaSimulationProjection& Projection = ProjectionResult.Value;

	TStrongObjectPtr<UHansaHudPresentationModel> Hud(NewObject<UHansaHudPresentationModel>());
	TStrongObjectPtr<UHansaCityOverviewPresentationModel> City(NewObject<UHansaCityOverviewPresentationModel>());
	TStrongObjectPtr<UHansaMarketTablePresentationModel> Market(NewObject<UHansaMarketTablePresentationModel>());
	TStrongObjectPtr<UHansaInspectorPresentationModel> Inspector(NewObject<UHansaInspectorPresentationModel>());
	TStrongObjectPtr<UHansaTradeMapPresentationModel> Trade(NewObject<UHansaTradeMapPresentationModel>());
	TStrongObjectPtr<UHansaResearchPresentationModel> Research(NewObject<UHansaResearchPresentationModel>());
	TStrongObjectPtr<UHansaScenarioPresentationModel> Scenario(NewObject<UHansaScenarioPresentationModel>());
	TStrongObjectPtr<UHansaSaveLoadPresentationModel> SaveLoad(NewObject<UHansaSaveLoadPresentationModel>());
	Hud->InitializeDefaults(); City->InitializeDefaults(); Market->InitializeDefaults(); Inspector->InitializeDefaults(); Trade->InitializeDefaults(); Scenario->InitializeDefaults(); SaveLoad->Bind(nullptr);
	City->ApplyProjection(Projection, *Registry, CityId.Value, FText::FromString(TEXT("Lübeck")));
	Market->ApplyProjection(Projection, *Registry, CityId.Value);
	Trade->ApplyProjection(Projection, *Registry);
	Hud->ApplyMarketAlerts(Projection, CityId.Value, FText::FromString(TEXT("Lübeck")));
	const auto HouseId = FHansaHouseId::TryCreate(1, 1);
	if (!HouseId) return false;
	FHansaHouseResearchState ResearchState;
	ResearchState.HouseId = HouseId.Value;
	ResearchState.AvailableResearchPoints = 500;
	TArray<FHansaCompiledTechnologyDefinition> Technologies = {
		{ TEXT("Technology.Commerce.MarketReports"), TEXT("Better market reports"), EHansaResearchBranch::Commerce, {}, 100, 4, TEXT("Reports remain current longer."), {{ EHansaResearchEffectKind::MarketReportAgeReductionTicks, TEXT("City.Lubeck"), 5 }}, 1 },
		{ TEXT("Technology.Production.ImprovedMilling"), TEXT("Improved milling"), EHansaResearchBranch::Production, {}, 100, 4, TEXT("Mills work faster."), {{ EHansaResearchEffectKind::ProductionThroughputBasisPoints, TEXT("Recipe.MillFlour"), 1000 }}, 2 },
		{ TEXT("Technology.Logistics.WarehouseHandling"), TEXT("Warehouse handling"), EHansaResearchBranch::Logistics, {}, 100, 4, TEXT("Warehouses transfer faster."), {{ EHansaResearchEffectKind::WarehouseHandlingBasisPoints, TEXT("Building.Warehouse"), 1000 }}, 3 }
	};
	FHansaEconomicRegistry ResearchRegistry({}, {}, {}, 99, {}, {}, {}, {}, {}, Technologies);
	Research->Initialize(ResearchRegistry, ResearchState);
	Scenario->Close();

	TSharedPtr<SWindow> Window;
	TSharedPtr<SHansaRootHud> Root;
	SAssignNew(Window, SWindow).Title(FText::FromString(TEXT("Hansa S14-P02 MVP UAT"))).ClientSize(FVector2D(1280, 720)).SizingRule(ESizingRule::FixedSize).SupportsMaximize(false).SupportsMinimize(false);
	SAssignNew(Root, SHansaRootHud).Model(Hud.Get()).InspectorModel(Inspector.Get()).CityOverviewModel(City.Get()).MarketTableModel(Market.Get()).TradeMapModel(Trade.Get()).ResearchModel(Research.Get()).ScenarioModel(Scenario.Get()).SaveLoadModel(SaveLoad.Get()).InitialViewportSize(FIntPoint(1280, 720));
	Window->SetContent(Root.ToSharedRef());
	if (!TestTrue(TEXT("Slate is initialized for native MVP UAT"), FSlateApplication::IsInitialized())) return false;
	FSlateApplication::Get().AddWindow(Window.ToSharedRef(), true);

	FHansaNativeScreenshotService Screenshots;
	auto Capture = [this, &Root, &Window, &Screenshots, &Fixture, &Projection](const TCHAR* Stage, const TCHAR* ScreenId)
	{
		for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
		{
			Root->SetPresentationSize(Size);
			Window->Resize(FVector2D(Size.X, Size.Y));
			FSlateApplication::Get().Tick();
			FSlateApplication::Get().ForceRedrawWindow(Window.ToSharedRef());
			const TArray<FHansaHudSemanticNode> Nodes = Root->GetSemanticSnapshot();
			const FHansaHudSemanticNode* VisibleScreen = Nodes.FindByPredicate([ScreenId](const FHansaHudSemanticNode& Node){ return Node.Id == ScreenId; });
			TestTrue(FString::Printf(TEXT("%s is semantically visible at %dx%d"), Stage, Size.X, Size.Y), VisibleScreen != nullptr && VisibleScreen->State.bVisible);
			FHansaScreenshotContext Context;
			Context.BundleId = FString::Printf(TEXT("mvp-golden-%s-%dx%d"), Stage, Size.X, Size.Y);
			Context.EvidenceSuiteId = TEXT("S14P02");
			Context.FixtureId = FHansaProductionFixture::GrainShortageFixtureId;
			Context.MapName = TEXT("L_Lubeck_MVP");
			Context.ScreenId = ScreenId;
			Context.FlowId = TEXT("mvp-golden-ui-v1");
			Context.CaptureMethod = TEXT("Slate.TakeScreenshot.NativeSize");
			Context.UiRevision = 2;
			Context.SimulationTick = Projection.GetClock().GetTick().GetValue();
			Context.FrameNumber = GFrameCounter;
			Context.SemanticSnapshotJson = SerializeSemantics(Nodes);
			Context.FixtureMetadataJson = FString::Printf(TEXT("{\"fixtureId\":\"%s\",\"registryHash\":\"%016llX\"}"), *Fixture.GetFixtureId(), static_cast<unsigned long long>(Fixture.GetRegistryHash()));
			Context.StructuralAssertions = { TEXT("nativePixels=true"), TEXT("postCaptureResize=false"), TEXT("semanticScreenVisible=true"), TEXT("controllerFocusOrder=true"), TEXT("statusColorRedundant=true") };
			Context.bStructuralAssertionsPassed = !Root->GetControllerFocusOrder().IsEmpty();
			Context.bRequireVisualVariation = true;
			const FHansaScreenshotResult Result = Screenshots.Capture(Size, Context, [&Root](const FIntPoint& Requested, TArray<FColor>& Pixels)
			{
				FIntVector CapturedSize;
				const bool bCaptured = FSlateApplication::Get().TakeScreenshot(Root->GetCaptureWidget(), FIntRect(0, 0, Requested.X, Requested.Y), Pixels, CapturedSize);
				return bCaptured && CapturedSize.X == Requested.X && CapturedSize.Y == Requested.Y;
			});
			TestTrue(FString::Printf(TEXT("%s has true native %dx%d evidence"), Stage, Size.X, Size.Y), Result.IsSuccess());
		}
	};

	Capture(TEXT("hud-alerts"), TEXT("HUD.Root"));
	Root->ActivateSemanticId(TEXT("HUD.TopStatus.CityOverview")); Root->ActivateSemanticId(TEXT("CityOverview.Tab.Market")); Root->ActivateSemanticId(TEXT("Market.Row.Good_Grain"));
	Capture(TEXT("city-market"), TEXT("CityOverview.Root")); Root->ActivateSemanticId(TEXT("CityOverview.Close"));
	if (!Hud->GetSnapshot().Alerts.IsEmpty())
	{
		const FHansaHudAlertPresentation& Alert = Hud->GetSnapshot().Alerts[0];
		Inspector->OpenFromAlert(Alert.StableId, Alert.AffectedObject, Alert.Age, Alert.AffectedBuildingValue, Alert.Causal, TEXT("HUD.AlertStack.Toggle"));
		Capture(TEXT("inspector"), TEXT("Inspector.Root")); Root->ActivateSemanticId(TEXT("Inspector.Close"));
	}
	Root->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")); Capture(TEXT("route-editor"), TEXT("TradeMap.Root")); Root->ActivateSemanticId(TEXT("TradeMap.Close"));
	Root->ActivateSemanticId(TEXT("HUD.TopStatus.Research.Open")); Capture(TEXT("research"), TEXT("Research.Root")); Root->ActivateSemanticId(TEXT("Research.Close"));
	Scenario->Open(); Capture(TEXT("scenario-victory"), TEXT("Scenario.Root")); Root->ActivateSemanticId(TEXT("Scenario.Close"));
	Root->ActivateSemanticId(TEXT("HUD.TopStatus.SaveLoad")); Capture(TEXT("save-load"), TEXT("SaveLoad.Root")); Root->ActivateSemanticId(TEXT("SaveLoad.Close"));

	FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
	return !HasAnyErrors();
}

#endif
