#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Definitions/HansaEconomicRegistry.h"
#include "Fixtures/HansaProductionFixture.h"
#include "Misc/FileHelper.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "UI/HansaTradeMapPresentationModel.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/SHansaRootHud.h"
#include "UI/SHansaTradeMap.h"
#include "World/HansaRuntimeSimulationHost.h"
#include "HansaTradeJourneySupport.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

namespace
{
	const Hansa::UI::FHansaHudSemanticNode* TradeMapTestsFindNode(const TArray<Hansa::UI::FHansaHudSemanticNode>& Nodes, const FString& Id)
	{
		return Nodes.FindByPredicate([&](const auto& Node){ return Node.Id == Id; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeMapProjectionAndEditorTest,
	"Hansa.UI.TradeMap.ProjectionAndSimpleEditor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTradeMapProjectionAndEditorTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	const auto Fixture = FHansaProductionFixture::TryCreateGrainShortage();
	if (!Fixture) return false;
	const auto Projection = Fixture.Value.BuildProjection();
	const FHansaEconomicRegistry* Registry = Fixture.Value.GetDefinitions().GetEconomicRegistry();
	if (!Projection || Registry == nullptr) return false;
	TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
	Model->InitializeDefaults();
	TestTrue(TEXT("Projection applies"), Model->ApplyProjection(Projection.Value, *Registry));
	TestEqual(TEXT("The MVP map has four cities"), Model->GetSnapshot().Cities.Num(), 4);
	TestEqual(TEXT("The MVP map has cog and wagon routes"), Model->GetSnapshot().Routes.Num(), 2);
	TestTrue(TEXT("Both route modes are represented"),
		Model->GetSnapshot().Routes.ContainsByPredicate([](const auto& R){return R.bSea;}) &&
		Model->GetSnapshot().Routes.ContainsByPredicate([](const auto& R){return !R.bSea;}));
	TestTrue(TEXT("Selected route exposes round trip, capacity, upkeep and profit uncertainty"),
		!Model->GetSnapshot().Routes[0].RoundTripTime.IsEmpty() && !Model->GetSnapshot().Routes[0].Capacity.IsEmpty() &&
		!Model->GetSnapshot().Routes[0].Upkeep.IsEmpty() && !Model->GetSnapshot().Routes[0].ExpectedProfitRange.IsEmpty() &&
		!Model->GetSnapshot().Routes[0].Uncertainty.IsEmpty());
	TestTrue(TEXT("Map opens from a selected market good"), Model->Open(TEXT("Market.Detail.Action.BeginRoute"), TEXT("Good.Grain")));
	const int64 Before = Model->GetDraftStops()[0].Actions[0].QuantityLimit.GetRawValue();
	TestTrue(TEXT("Quantity has a non-drag controller alternative"), Model->AdjustQuantityIntent(5000));
	TestEqual(TEXT("Quantity step is deterministic"), Model->GetDraftStops()[0].Actions[0].QuantityLimit.GetRawValue(), Before + 5000);
	TestTrue(TEXT("Cargo action can be changed without dragging"), Model->CycleCargoActionIntent());
	TestTrue(TEXT("Draft state is explicit"), Model->GetSnapshot().bDirty);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeMapSemanticsResponsiveEvidenceTest,
	"Hansa.UI.TradeMap.SemanticsAndResponsiveNativeEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTradeMapSemanticsResponsiveEvidenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Automation;
	(void)Parameters;
	const auto Fixture = FHansaProductionFixture::TryCreateGrainShortage();
	if (!Fixture) return false;
	const auto Projection = Fixture.Value.BuildProjection();
	const FHansaEconomicRegistry* Registry = Fixture.Value.GetDefinitions().GetEconomicRegistry();
	if (!Projection || Registry == nullptr) return false;
	TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
	Model->InitializeDefaults(); Model->ApplyProjection(Projection.Value, *Registry);
	TStrongObjectPtr<UHansaHudPresentationModel> HudModel(NewObject<UHansaHudPresentationModel>());
	HudModel->InitializeDefaults();
	TSharedRef<Hansa::UI::SHansaRootHud> Hud = SNew(Hansa::UI::SHansaRootHud).Model(HudModel.Get()).TradeMapModel(Model.Get()).InitialViewportSize(FIntPoint(1280,720));
	TestTrue(TEXT("HUD trade-map action opens the native screen"), Hud->ActivateSemanticId(TEXT("HUD.TopStatus.TradeMap")));
	TestTrue(TEXT("Close routes through the child screen"), Hud->ActivateSemanticId(TEXT("TradeMap.Close")));
	TestEqual(TEXT("Close restores focus to the HUD opener"), HudModel->GetSnapshot().FocusedSemanticId, FName(TEXT("HUD.TopStatus.TradeMap")));
	Model->Open();
	TSharedRef<Hansa::UI::SHansaTradeMap> Screen = SNew(Hansa::UI::SHansaTradeMap).Model(Model.Get()).InitialViewportSize(FIntPoint(1280,720));
	Screen->SetPresentationSize(FIntPoint(1280,720));
	auto Nodes = Screen->GetSemanticSnapshot();
	TestTrue(TEXT("Map geometry is native"), TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Canvas")) != nullptr && TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Canvas"))->State.Value == TEXT("native"));
	TestNotNull(TEXT("Lübeck has a stable semantic city ID"), TradeMapTestsFindNode(Nodes,TEXT("TradeMap.City.City_Lubeck")));
	TestNotNull(TEXT("Sea route has a stable semantic route ID"), TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Route.1")));
	TestNotNull(TEXT("City capability mode is an ordinary semantic control"), TradeMapTestsFindNode(Nodes,TEXT("TradeMap.City.Filter")));
	TestNotNull(TEXT("Selected-good mode is an ordinary semantic control"), TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Good.Filter")));
	TestNotNull(TEXT("City search is an ordinary semantic control"), TradeMapTestsFindNode(Nodes,TEXT("TradeMap.City.Search")));
	const auto* RouteStateNode = TradeMapTestsFindNode(Nodes, TEXT("TradeMap.Editor.RouteState"));
	const auto* RouteActionNode = TradeMapTestsFindNode(Nodes, TEXT("TradeMap.Editor.ToggleActive"));
	TestTrue(TEXT("Selected stopped route exposes an explicit state"),
		RouteStateNode != nullptr && RouteStateNode->Label == TEXT("Route stopped") &&
		RouteStateNode->State.Value.Contains(TEXT("Stopped")));
	TestTrue(TEXT("Stopped route exposes one unambiguous enabled action"),
		RouteActionNode != nullptr && RouteActionNode->Label == TEXT("Start route") &&
		RouteActionNode->State.bEnabled && RouteActionNode->bCanActivate);
	TestTrue(TEXT("Controller order reaches the complete Simple editor"),
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Editor.Action.Cycle")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Editor.Quantity.Increase")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Editor.Reserve.Increase")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Editor.Save")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Editor.ToggleActive")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.City.Filter")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Good.Filter")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.City.Search")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Route.Page.Previous")) &&
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Route.Page.Next")));
	TestTrue(TEXT("City search accepts a non-drag deterministic intent"),Model->SetCitySearchIntent(TEXT("Rostock")));
	TestEqual(TEXT("City search narrows the visible marker projection"),Model->GetSnapshot().Cities.Num(),1);
	TestEqual(TEXT("City search reports the complete matching count"),Model->GetSnapshot().MatchingCityCount,1);
	TestTrue(TEXT("City search can be cleared"),Model->SetCitySearchIntent(TEXT("")));
	TestEqual(TEXT("Clearing city search restores the four-city fixture"),Model->GetSnapshot().Cities.Num(),4);
	TestEqual(TEXT("720p uses compact composition"), Model->GetSnapshot().bCompact, true);
	Screen->SetPresentationSize(FIntPoint(1920,1080));
	TestEqual(TEXT("1080p restores wide schedule/legend composition"), Model->GetSnapshot().bCompact, false);

	FHansaNativeScreenshotService Screenshots;
	auto NativeBuffer=[](const FIntPoint& Size,TArray<FColor>& Pixels){Pixels.Init(FColor(21,42,53,255),Size.X*Size.Y);return true;};
	for(const FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1920,1080)})
	{
		FHansaScreenshotContext Context;Context.BundleId=FString::Printf(TEXT("contract-trade-map-%dx%d"),Size.X,Size.Y);Context.EvidenceSuiteId=TEXT("S09P03");Context.FixtureId=TEXT("lubeck_grain_shortage_v1");Context.ScreenId=TEXT("TradeMap.Root");Context.FlowId=TEXT("simple-route-editor-v1");Context.CaptureMethod=TEXT("AutomationTest.NativeBufferContract");Context.UiRevision=1;Context.SemanticSnapshotJson=TEXT("{\"schemaVersion\":1,\"nodes\":[{\"id\":\"TradeMap.Root\"},{\"id\":\"TradeMap.Canvas\"},{\"id\":\"TradeMap.Route.1\"},{\"id\":\"TradeMap.Editor.Save\"}]}");Context.StructuralAssertions={TEXT("cities.mvpFour=true"),TEXT("routes.seaAndLand=true"),TEXT("geometry.native=true"),TEXT("editor.simple=true"),TEXT("controller.noDrag=true"),TEXT("uncertainty.explicit=true"),TEXT("status.colorRedundant=true")};Context.bStructuralAssertionsPassed=true;
		const auto Result=Screenshots.Capture(Size,Context,NativeBuffer);TestTrue(TEXT("Responsive native evidence writes"),Result.IsSuccess());FString Metadata;TestTrue(TEXT("Evidence metadata is readable"),FFileHelper::LoadFileToString(Metadata,*Result.MetadataPath));TestTrue(TEXT("Evidence records native dimensions and no resize"),Metadata.Contains(FString::Printf(TEXT("\"width\":%d"),Size.X))&&Metadata.Contains(FString::Printf(TEXT("\"height\":%d"),Size.Y))&&Metadata.Contains(TEXT("\"postCaptureResized\":false")));
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeMapRuntimeCommandCommitTest,
	"Hansa.UI.TradeMap.RuntimeCommandCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTradeMapRuntimeCommandCommitTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	(void)Parameters;
	TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
	FString Error;
	if (!TestTrue(TEXT("Playable runtime initializes"), Host->InitializeForLubeck(nullptr, Error)))
	{
		AddError(Error); return false;
	}
	if (!TestTrue(TEXT("Reserve automation research completed for command fixture"), Hansa::Tests::TradeJourney::UnlockReserveAutomation(Host.Get()))) return false;
	const FHansaEconomicRegistry* Registry = Host->GetEconomicRegistry();
	const auto BeforeProjection = Host->BuildProjection();
	if (Registry == nullptr || !BeforeProjection) return false;
	TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
	Model->InitializeDefaults(); Model->BindRuntime(Host.Get()); Model->ApplyProjection(BeforeProjection.Value, *Registry); Model->Open();
	const auto* StoppedLandRoute = Model->GetSnapshot().Routes.FindByPredicate([](const auto& Route){ return Route.RouteValue == 2; });
	const auto* RivalRoute = Model->GetSnapshot().Routes.FindByPredicate([](const auto& Route){ return Route.RouteValue == 3; });
	TestTrue(TEXT("Player stopped route clearly offers Start route"),
		StoppedLandRoute != nullptr && StoppedLandRoute->bOwnedByPlayer && StoppedLandRoute->bCanToggleActive &&
		StoppedLandRoute->StateHeading.EqualTo(FText::FromString(TEXT("Route stopped"))) &&
		StoppedLandRoute->ToggleActionLabel.EqualTo(FText::FromString(TEXT("Start route"))));
	TestTrue(TEXT("Rival route is identified and cannot be operated"),
		RivalRoute != nullptr && !RivalRoute->bOwnedByPlayer && !RivalRoute->bCanToggleActive &&
		RivalRoute->Label.ToString().Contains(TEXT("Rival")) &&
		RivalRoute->ToggleActionHint.ToString().Contains(TEXT("only its owner")));
	TestTrue(TEXT("Cog route is selected"), Model->SelectRouteIntent(1));
	TestTrue(TEXT("Load stop is selected"), Model->SelectStopIntent(1));
	const int64 BeforeReserve = Model->GetDraftStops()[1].Actions[0].MinimumSourceReserve.GetRawValue();
	TestTrue(TEXT("Reserve edit changes only the draft"), Model->AdjustMinimumReserveIntent(5000));
	TestTrue(TEXT("Save submits the typed edit-route command"), Model->CommitIntent());
	const auto EditedProjection = Host->BuildProjection();
	if (!EditedProjection) return false;
	const auto* Edited = EditedProjection.Value.GetRoutes().FindByPredicate([](const auto& Route){ return Route.Id.GetValue() == 1; });
	TestTrue(TEXT("Authoritative route contains the committed reserve"), Edited != nullptr && Edited->Stops[1].Actions[0].MinimumSourceReserve.GetRawValue() == BeforeReserve + 5000);
	Model->ApplyProjection(EditedProjection.Value, *Registry);
	TestTrue(TEXT("Start submits the typed active-state command"), Model->ToggleActiveIntent());
	const auto ActiveProjection = Host->BuildProjection();
	const auto* Active = ActiveProjection ? ActiveProjection.Value.GetRoutes().FindByPredicate([](const auto& Route){ return Route.Id.GetValue() == 1; }) : nullptr;
	TestTrue(TEXT("Authoritative route enters an active lifecycle"), Active != nullptr &&
		(Active->Lifecycle == EHansaRouteLifecycleState::AtStop || Active->Lifecycle == EHansaRouteLifecycleState::Traveling));
	if (!ActiveProjection || !Host->AdvanceTicks(1)) return false;
	const auto TravelingProjection = Host->BuildProjection();
	if (!TravelingProjection) return false;
	Model->ApplyProjection(TravelingProjection.Value, *Registry);
	const auto* Traveling = Model->GetSnapshot().Routes.FindByPredicate([](const auto& Route){ return Route.RouteValue == 1; });
	TestTrue(TEXT("Traveling route reports destination and remaining time"),
		Traveling != nullptr && Traveling->bTraveling &&
		Traveling->StateHeading.ToString().Contains(TEXT("IN TRANSIT TO")) &&
		Traveling->StateHeading.ToString().Contains(TEXT("TICKS")));
	TestTrue(TEXT("Traveling route explains why pause is unavailable"),
		Traveling != nullptr && !Traveling->bCanToggleActive &&
		Traveling->ToggleActionLabel.EqualTo(FText::FromString(TEXT("Pause available at next stop"))) &&
		Traveling->ToggleActionHint.ToString().Contains(TEXT("Wait until")));
	TestFalse(TEXT("In-transit pause intent is rejected before entering the command gateway"), Model->ToggleActiveIntent());
	TestTrue(TEXT("Unavailable action publishes a plain-language remedy"),
		Model->GetSnapshot().EditorStatus.ToString().Contains(TEXT("Wait until")));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeMapSectionNavigationTest,
    "Hansa.UI.TradeMap.SectionNavigation.NativeScreens",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FHansaTradeMapSectionNavigationTest::RunTest(const FString& Parameters)
{
    using namespace Hansa::UI;
    using namespace Hansa::Automation;
    (void)Parameters;
    TStrongObjectPtr<UHansaRuntimeSimulationHost> Host(NewObject<UHansaRuntimeSimulationHost>());
    FString Error;
    if(!TestTrue(TEXT("Runtime catalog loads"),Host->InitializeForLubeck(nullptr,Error))){AddError(Error);return false;}
    const auto Projection=Host->BuildProjection();
    if(!Projection||!Host->GetEconomicRegistry()||!FSlateApplication::IsInitialized())return false;
    TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
    Model->InitializeDefaults();Model->BindRuntime(Host.Get());Model->ApplyProjection(Projection.Value,*Host->GetEconomicRegistry());Model->Open();
    TestFalse(TEXT("Office review is not offered before establishing a station"),Model->GetSnapshot().bCanPresenceUpgradeAction);
    TestTrue(TEXT("Office lock explains the required station"),Model->GetSnapshot().PresenceUpgradeAction.ToString().Contains(TEXT("active trade station")));
    auto Screen=SNew(SHansaTradeMap).Model(Model.Get());
    auto Window=SNew(SWindow).Title(FText::FromString(TEXT("Trade section regression"))).ClientSize(FVector2D(1280,720)).SizingRule(ESizingRule::FixedSize).SupportsMaximize(false).SupportsMinimize(false);
    Window->SetContent(Screen);FSlateApplication::Get().AddWindow(Window,true);
    auto Draw=[&]{FSlateApplication::Get().Tick();FSlateApplication::Get().ForceRedrawWindow(Window);};
    FHansaNativeScreenshotService Screenshots;
    for(const FIntPoint Size:{FIntPoint(1280,720),FIntPoint(1920,1080),FIntPoint(2730,888)})
    {
        Screen->SetPresentationSize(Size);Window->Resize(FVector2D(Size.X,Size.Y));Draw();Draw();
        for(const TCHAR* Section:{TEXT("Route"),TEXT("Presence"),TEXT("Specialization"),TEXT("Orders")})
        {
            const FString Id=TEXT("TradeMap.Navigate.")+FString(Section);
            TestTrue(TEXT("Section shortcut is controller reachable"),Screen->GetControllerFocusOrder().Contains(Id));
            TestTrue(TEXT("Section shortcut opens even when progression is locked"),Screen->ActivateSemanticId(Id));Draw();Draw();
            auto Nodes=Screen->GetSemanticSnapshot();
            for(const TCHAR* Other:{TEXT("Route"),TEXT("Presence"),TEXT("Specialization"),TEXT("Orders")})
            {
                const auto* Node=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Navigate.")+FString(Other));
                TestTrue(TEXT("All shortcuts stay visible after scrolling"),Node&&Node->State.bVisible&&Node->State.bEnabled);
            }
            if(FString(Section)==TEXT("Orders"))
            {
                const auto* Node=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Orders.Status"));
                TestTrue(TEXT("Unavailable orders reveal their prerequisite instead of disappearing"),Node&&Node->State.bVisible&&Node->State.Value.Contains(TEXT("Establish and fund")));
            }
            // The evidence service intentionally supports only the two MVP sizes.
            // Ultrawide still runs the same native layout, navigation and clipping assertions.
            if(!FHansaNativeScreenshotService::IsSupportedSize(Size))continue;
            FHansaScreenshotContext Context;
            Context.BundleId=FString::Printf(TEXT("trade-sections-%s-%dx%d"),Section,Size.X,Size.Y);
            Context.EvidenceSuiteId=TEXT("TradeSectionNavigation");Context.ScreenId=TEXT("TradeMap.Root");
            Context.CaptureMethod=TEXT("IsolatedSlate.TakeScreenshot.NativeSize");Context.bRequireVisualVariation=true;
            Context.StructuralAssertions={TEXT("persistentSectionNavigation=true")};Context.bStructuralAssertionsPassed=!HasAnyErrors();
            const auto Capture=Screenshots.Capture(Size,Context,[&](const FIntPoint& Requested,TArray<FColor>& Pixels){FIntVector CapturedSize;return FSlateApplication::Get().TakeScreenshot(Screen,FIntRect(0,0,Requested.X,Requested.Y),Pixels,CapturedSize)&&CapturedSize.X==Requested.X&&CapturedSize.Y==Requested.Y;});
            TestTrue(TEXT("Native section screenshot captured"),Capture.IsSuccess());
        }
        if(Model->CanPresenceSpecializationIntent(TEXT("Warehouse")))
        {
            TestTrue(TEXT("Specialization is keyboard focusable"),Screen->FocusSemanticId(TEXT("TradeMap.Presence.Specialization.Warehouse")));Draw();Draw();
            const auto Nodes=Screen->GetSemanticSnapshot();const auto* Node=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Presence.Specialization.Warehouse"));
            TestTrue(TEXT("Keyboard focus reveals a previously buried presence control"),Node&&Node->State.bVisible&&Node->State.bFocused);
        }
    }
    TestTrue(TEXT("Route creation begins normally"),Model->BeginCreateIntent());
    TestFalse(TEXT("Section navigation cannot abandon an unsaved route draft"),Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence")));
    FSlateApplication::Get().RequestDestroyWindow(Window);
    return !HasAnyErrors();
}

#endif
