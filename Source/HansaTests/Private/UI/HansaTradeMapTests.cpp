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
#include "Widgets/Layout/SScrollBox.h"

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
	TestTrue(TEXT("Projection applies"), (Model->SetViewerHouse(Projection.Value.GetRoutes()[0].OwnerId), Model->ApplyProjection(Projection.Value, *Registry)));
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
	Model->InitializeDefaults(); (Model->SetViewerHouse(Projection.Value.GetRoutes()[0].OwnerId), Model->ApplyProjection(Projection.Value, *Registry));
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
	Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Route"));Nodes=Screen->GetSemanticSnapshot();
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
		Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Editor.ToggleActive")));
    Screen->ActivateSemanticId(TEXT("TradeMap.Page.Routes"));
    TestTrue(TEXT("Routes page exposes filters menu and search"),
        Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.Directory.More")) &&
        Screen->GetControllerFocusOrder().Contains(TEXT("TradeMap.City.Search")));
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
            if(Model->GetSnapshot().bCompact)Screen->ActivateSemanticId(TEXT("TradeMap.Page.Workspace"));
            TestTrue(TEXT("Section shortcut is controller reachable"),Screen->GetControllerFocusOrder().Contains(Id));
            TestTrue(TEXT("Section shortcut opens even when progression is locked"),Screen->ActivateSemanticId(Id));Draw();Draw();
            auto Nodes=Screen->GetSemanticSnapshot();
            const auto* ScheduleNode=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Schedule"));
            TestTrue(TEXT("Schedule reflows into a bounded native region"),ScheduleNode&&(Model->GetSnapshot().bCompact?!ScheduleNode->State.bVisible:(ScheduleNode->State.bVisible&&ScheduleNode->Bounds.Height()<=180)));
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
    // Virtualized offscreen routes must reveal through the stable semantic ID.
    const FString LastRouteId=FString::Printf(TEXT("TradeMap.Route.%lld"),Model->GetSnapshot().Routes.Last().RouteValue);
    TestTrue(TEXT("Offscreen route accepts semantic focus"),Screen->FocusSemanticId(LastRouteId));Draw();Draw();
    const auto FocusedRows=Screen->GetSemanticSnapshot();
    const auto* LastRouteNode=TradeMapTestsFindNode(FocusedRows,LastRouteId);
    TestTrue(TEXT("Virtual list reveals and focuses the requested route"),LastRouteNode&&LastRouteNode->State.bVisible&&LastRouteNode->State.bFocused);
    TestTrue(TEXT("Stop accepts semantic focus"),Screen->FocusSemanticId(TEXT("TradeMap.Stop.0")));Draw();Draw();
    const auto FocusedStop=FSlateApplication::Get().GetKeyboardFocusedWidget();
    Model->AdjustQuantityIntent(5000);Draw();Draw();
    TestTrue(TEXT("Live refresh retains actual Slate keyboard focus"),FocusedStop==FSlateApplication::Get().GetKeyboardFocusedWidget());
    // A tab switch must retain its own bounded viewport offset.
    auto FindScroll=[](TSharedPtr<SWidget> W)->TSharedPtr<SScrollBox>{while(W){if(W->GetType()==FName(TEXT("SScrollBox")))return StaticCastSharedPtr<SScrollBox>(W);W=W->GetParentWidget();}return nullptr;};
    Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Route"));Draw();Draw();
    const auto RouteScroll=FindScroll(Screen->ResolveSemanticWidget(TEXT("TradeMap.Stop.0")));
    const auto PresenceScroll=FindScroll(Screen->ResolveSemanticWidget(TEXT("TradeMap.Station.Action")));
    TestTrue(TEXT("Unrelated workflows have separate scroll owners"),RouteScroll&&PresenceScroll&&RouteScroll!=PresenceScroll);
    if(RouteScroll){RouteScroll->SetScrollOffset(80);Draw();Draw();const float Before=RouteScroll->GetScrollOffset();
      Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence"));Draw();Draw();
      Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Route"));Draw();Draw();
      TestEqual(TEXT("Switching tabs retains the route scroll offset"),RouteScroll->GetScrollOffset(),Before);}
    TestFalse(TEXT("New route cannot overwrite unsaved edits"),Model->BeginCreateIntent());
    TestTrue(TEXT("Rejected creation preserves edited draft"),Model->GetSnapshot().bDirty);
    // Begin the independent creator-navigation check from the original clean fixture.
    Model->InitializeDefaults();Model->ApplyProjection(Projection.Value,*Host->GetEconomicRegistry());Model->Open();
    TestTrue(TEXT("Route creation begins normally"),Model->BeginCreateIntent());
    TestFalse(TEXT("Section navigation cannot abandon an unsaved route draft"),Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Presence")));
    FSlateApplication::Get().RequestDestroyWindow(Window);
    return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeComponentOwnershipTest,
 "Hansa.UI.TradeMap.Components.OwnershipAndRefresh",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaTradeComponentOwnershipTest::RunTest(const FString&)
{
 using namespace Hansa::UI;
 const auto Fixture=Hansa::Simulation::FHansaProductionFixture::TryCreateGrainShortage();
 if(!Fixture||!FSlateApplication::IsInitialized())return false;
 const auto Projection=Fixture.Value.BuildProjection();
 const auto* Registry=Fixture.Value.GetDefinitions().GetEconomicRegistry();
 if(!Projection||!Registry)return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
 Model->InitializeDefaults();(Model->SetViewerHouse(Projection.Value.GetRoutes()[0].OwnerId),Model->ApplyProjection(Projection.Value,*Registry));
 Model->Open(TEXT("TG01.Origin"));
 auto Screen=SNew(SHansaTradeMap).Model(Model.Get());
 const FString RouteId=FString::Printf(TEXT("TradeMap.Route.%lld"),Model->GetSnapshot().SelectedRouteValue);
 const auto Route=Screen->ResolveSemanticWidget(RouteId);
 const auto Stop=Screen->ResolveSemanticWidget(TEXT("TradeMap.Stop.0"));
 TestTrue(TEXT("Directory and route editor preserve semantic controls"),Route.IsValid()&&Stop.IsValid());
 Model->AdjustQuantityIntent(5000);
 TestTrue(TEXT("Live draft updates preserve route widget identity"),Route==Screen->ResolveSemanticWidget(RouteId));
 TestTrue(TEXT("Live draft updates preserve stop widget identity"),Stop==Screen->ResolveSemanticWidget(TEXT("TradeMap.Stop.0")));
 const auto Draft=Model->GetDraftStops();
 TestTrue(TEXT("Presenter accepts office tab"),Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Specialization")));
 TestEqual(TEXT("Presenter owns tab selection"),Model->GetSnapshot().ActiveSection,FString(TEXT("Specialization")));
 auto Reopened=SNew(SHansaTradeMap).Model(Model.Get());
 const auto Nodes=Reopened->GetSemanticSnapshot();
 const auto* Office=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Navigate.Specialization"));
 TestTrue(TEXT("A second view observes the same selected tab"),Office&&Office->State.bSelected);
 TestEqual(TEXT("Navigation retains route draft quantity"),Model->GetDraftStops()[0].Actions[0].QuantityLimit.GetRawValue(),Draft[0].Actions[0].QuantityLimit.GetRawValue());
 const auto* OtherCity=Model->GetSnapshot().Cities.FindByPredicate([&](const auto& C){return C.StableId!=Model->GetSnapshot().SelectedCityStableId;});
 if(!OtherCity)return false;
 const FName City=OtherCity->StableId;
 TestTrue(TEXT("Map city intent is accepted"),Model->SelectCityIntent(City));
 TestEqual(TEXT("City intent updates shared inspector selection"),Model->GetSnapshot().SelectedCityStableId,City);
 TestEqual(TEXT("City intent opens overview through presenter"),Model->GetSnapshot().ActiveSection,FString(TEXT("Overview")));
 TestTrue(TEXT("Compact workspace page accepted"),Screen->ActivateSemanticId(TEXT("TradeMap.Page.Workspace")));
 TestTrue(TEXT("Back returns compact workspace to Map"),Screen->ActivateSemanticId(TEXT("TradeMap.Back")));
 TestEqual(TEXT("Map page selected by Back"),Model->GetSnapshot().WorkspacePage,FString(TEXT("Map")));
 TestTrue(TEXT("First Back retains open workspace"),Model->GetSnapshot().bOpen);
 TestEqual(TEXT("Page Back restores its origin"),Model->GetSnapshot().FocusedSemanticId,FName(TEXT("TradeMap.Page.Workspace")));
 TestFalse(TEXT("Invalid page rejected"),Model->SelectWorkspacePageIntent(TEXT("Unsupported")));
 TestFalse(TEXT("Invalid tab cannot mutate presenter"),Model->SelectSectionIntent(TEXT("Unsupported")));
 Screen->SetPresentationSize(FIntPoint(1280,720));TestTrue(TEXT("Compact projection propagates"),Model->GetSnapshot().bCompact);
 Screen->SetPresentationSize(FIntPoint(1920,1080));TestFalse(TEXT("Wide projection propagates"),Model->GetSnapshot().bCompact);
 FName Restored;Model->OnFocusRestoreRequested().AddLambda([&](FName Id){Restored=Id;});
 TestTrue(TEXT("Close accepted"),Model->CloseIntent());TestEqual(TEXT("Close restores the original caller"),Restored,FName(TEXT("TG01.Origin")));
 return !HasAnyErrors();
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaTradeShellNavigationTest,
 "Hansa.UI.TradeMap.Shell.InputAndFeedback",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHansaTradeShellNavigationTest::RunTest(const FString&)
{
 using namespace Hansa::UI;
 if(!FSlateApplication::IsInitialized())return false;
 TStrongObjectPtr<UHansaTradeMapPresentationModel> Model(NewObject<UHansaTradeMapPresentationModel>());
 Model->InitializeDefaults();Model->Open();
 FUiPreferences Preferences;Preferences.bLargeText=true;
 auto Screen=SNew(SHansaTradeMap).Model(Model.Get()).Preferences(Preferences);
 Screen->SetPresentationSize(FIntPoint(1920,1080));
 TestTrue(TEXT("Large text uses bounded pages even at wide size"),Model->GetSnapshot().bCompact);
 auto Nodes=Screen->GetSemanticSnapshot();
 const auto* Loading=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Workspace.Status"));
 TestTrue(TEXT("Before projection the shell explains loading"),Loading&&Loading->State.ValueType==TEXT("loading")&&Loading->State.Value.Contains(TEXT("authoritative")));
 const auto Fixture=Hansa::Simulation::FHansaProductionFixture::TryCreateGrainShortage();
 if(!Fixture)return false;
 const auto Projection=Fixture.Value.BuildProjection();const auto* Registry=Fixture.Value.GetDefinitions().GetEconomicRegistry();
 if(!Projection||!Registry)return false;
 (Model->SetViewerHouse(Projection.Value.GetRoutes()[0].OwnerId),Model->ApplyProjection(Projection.Value,*Registry));
 const auto City=Model->GetSnapshot().SelectedCityStableId;const auto Route=Model->GetSnapshot().SelectedRouteValue;
 Screen->ActivateSemanticId(TEXT("TradeMap.Page.Workspace"));
 Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Route"));
 TestTrue(TEXT("Right arrow selects next workflow"),Screen->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Right,FModifierKeysState(),0,false,0,0)).IsEventHandled());
 TestEqual(TEXT("Presence follows Route"),Model->GetSnapshot().ActiveSection,FString(TEXT("Presence")));
 Screen->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Gamepad_LeftShoulder,FModifierKeysState(),0,false,0,0));
 TestEqual(TEXT("Controller shoulder selects previous workflow"),Model->GetSnapshot().ActiveSection,FString(TEXT("Route")));
 TestEqual(TEXT("Tab input retains selected city"),Model->GetSnapshot().SelectedCityStableId,City);
 TestEqual(TEXT("Tab input retains selected route"),Model->GetSnapshot().SelectedRouteValue,Route);
 Nodes=Screen->GetSemanticSnapshot();const auto* Tab=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Navigate.Route"));
 TestTrue(TEXT("Workflow declares selected Tab semantics"),Tab&&Tab->Role==EHansaHudSemanticRole::Tab&&Tab->State.bSelected);
 Screen->OnKeyDown(FGeometry(),FKeyEvent(EKeys::Escape,FModifierKeysState(),0,false,0,0));
 TestEqual(TEXT("Escape returns to compact map"),Model->GetSnapshot().WorkspacePage,FString(TEXT("Map")));
 TestFalse(TEXT("Unavailable action rejected"),Screen->ActivateSemanticId(TEXT("TradeMap.Navigate.Unsupported")));
 Nodes=Screen->GetSemanticSnapshot();const auto* Error=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Workspace.Status"));
 TestTrue(TEXT("Rejected action has explicit error semantics and remedy"),Error&&Error->State.bError&&Error->State.Value.Contains(TEXT("Action unavailable")));
 Screen->ActivateSemanticId(TEXT("TradeMap.Page.Map"));
 Nodes=Screen->GetSemanticSnapshot();Error=TradeMapTestsFindNode(Nodes,TEXT("TradeMap.Workspace.Status"));
 TestTrue(TEXT("Successful navigation clears command error"),Error&&!Error->State.bError);
 return !HasAnyErrors();
}

#endif
