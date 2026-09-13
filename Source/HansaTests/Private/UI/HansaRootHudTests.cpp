#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UI/HansaHudLayout.h"
#include "UI/HansaHudPresentationModel.h"
#include "UI/SHansaRootHud.h"

namespace
{
	const Hansa::UI::FHansaHudSemanticNode* RootHudTestsFindNode(
		const TArray<Hansa::UI::FHansaHudSemanticNode>& Nodes,
		const TCHAR* Id)
	{
		return Nodes.FindByPredicate([Id](const Hansa::UI::FHansaHudSemanticNode& Node) { return Node.Id == Id; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaHudPresentationEventsTest,
	"Hansa.UI.HUD.PresentationModel.EventUpdates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaHudPresentationEventsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaHudPresentationModel> Model(NewObject<UHansaHudPresentationModel>());
	int32 EventCount = 0;
	uint64 LastRevision = 0;
	Model->OnChanged().AddLambda([&EventCount, &LastRevision](const FHansaHudPresentationSnapshot&, const uint64 Revision)
	{
		++EventCount;
		LastRevision = Revision;
	});
	Model->InitializeDefaults();
	TestEqual(TEXT("Initialization publishes exactly one complete snapshot"), EventCount, 1);
	TestEqual(TEXT("First event has revision one"), LastRevision, uint64(1));
	TestFalse(TEXT("Reapplying an identical snapshot is silent"), Model->ApplySnapshot(Model->GetSnapshot()));
	TestEqual(TEXT("No raw refresh is published for unchanged state"), EventCount, 1);
	Model->SetSpeed(EHansaHudGameSpeed::Fast);
	TestEqual(TEXT("A gameplay intent publishes one event"), EventCount, 2);
	TestEqual(TEXT("Speed state is authoritative in the model"), Model->GetSnapshot().Speed, EHansaHudGameSpeed::Fast);
	Model->SetSpeed(EHansaHudGameSpeed::Fast);
	TestEqual(TEXT("Repeating the same speed does not refresh the HUD"), EventCount, 2);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaHudResponsiveLayoutTest,
	"Hansa.UI.HUD.Layout.NativeSafeAreas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaHudResponsiveLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	for (const FIntPoint Size : { FIntPoint(1280, 720), FIntPoint(1920, 1080) })
	{
		const Hansa::UI::FHansaHudLayoutMetrics Layout = Hansa::UI::MakeHudLayoutMetrics(Size);
		TestEqual(FString::Printf(TEXT("Viewport remains native %dx%d"), Size.X, Size.Y), Layout.ViewportSize, Size);
		TestTrue(FString::Printf(TEXT("All anchored clusters fit the %dx%d safe area"), Size.X, Size.Y), Layout.FitsSafeArea());
		TestTrue(FString::Printf(TEXT("At least 70 percent of %dx%d remains unobstructed by the default shell"), Size.X, Size.Y),
			Layout.GetDefaultOcclusionRatio() <= 0.30f);
	}
	const Hansa::UI::FHansaHudLayoutMetrics Hd = Hansa::UI::MakeHudLayoutMetrics(FIntPoint(1280, 720));
	const Hansa::UI::FHansaHudLayoutMetrics FullHd = Hansa::UI::MakeHudLayoutMetrics(FIntPoint(1920, 1080));
	TestTrue(TEXT("1080p uses the approved 360-440 pixel inspector range"),
		FullHd.InspectorWidth >= 360.0f && FullHd.InspectorWidth <= 440.0f);
	TestTrue(TEXT("The build menu reserves enough height for cards, target controls, two action rows, and feedback"),
		Hd.BuildMenuHeight >= 440.0f && FullHd.BuildMenuHeight >= 440.0f);
	TestTrue(TEXT("Safe margins grow at the 1080p breakpoint"), FullHd.SafeArea > Hd.SafeArea);
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaHudOpenCloseFocusTest,
	"Hansa.UI.HUD.Semantics.OpenCloseFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaHudOpenCloseFocusTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<UHansaHudPresentationModel> Model(NewObject<UHansaHudPresentationModel>());
	Model->InitializeDefaults();
	TSharedRef<Hansa::UI::SHansaRootHud> Hud = SNew(Hansa::UI::SHansaRootHud)
		.Model(Model.Get())
		.InitialViewportSize(FIntPoint(1280, 720));

	TArray<Hansa::UI::FHansaHudSemanticNode> Nodes = Hud->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Fps = RootHudTestsFindNode(Nodes, TEXT("HUD.TopStatus.FPS"));
	TestTrue(TEXT("The running HUD exposes an FPS count"), Fps != nullptr && Fps->State.Value == TEXT("FPS —"));
	for (int32 Frame = 0; Frame < 31; ++Frame)
	{
		Hud->Tick(FGeometry(), static_cast<double>(Frame) / 60.0, 1.0f / 60.0f);
	}
	Nodes = Hud->GetSemanticSnapshot();
	Fps = RootHudTestsFindNode(Nodes, TEXT("HUD.TopStatus.FPS"));
	TestTrue(TEXT("The FPS count updates from a half-second frame sample"),
		Fps != nullptr && Fps->State.Value == TEXT("FPS 60"));
	const Hansa::UI::FHansaHudSemanticNode* AlertStack = RootHudTestsFindNode(Nodes, TEXT("HUD.AlertStack"));
	TestNotNull(TEXT("Alert stack exposes a semantic role and state"), AlertStack);
	if (AlertStack != nullptr)
	{
		TestEqual(TEXT("Alert stack has an alert role"), AlertStack->Role, Hansa::UI::EHansaHudSemanticRole::Alert);
		TestEqual(TEXT("Alert stack begins expanded"), AlertStack->State.Value, FString(TEXT("true")));
	}

	TestTrue(TEXT("Semantic activation closes the alert stack"), Hud->ActivateSemanticId(TEXT("HUD.AlertStack.Toggle")));
	Nodes = Hud->GetSemanticSnapshot();
	AlertStack = RootHudTestsFindNode(Nodes, TEXT("HUD.AlertStack"));
	TestTrue(TEXT("Collapsed alert state is observable"), AlertStack != nullptr && AlertStack->State.Value == TEXT("false"));
	TestTrue(TEXT("Semantic activation reopens the alert stack"), Hud->ActivateSemanticId(TEXT("HUD.AlertStack.Toggle")));

	Model->SetSelection(FText::FromString(TEXT("Selected · Bakery")), FText::FromString(TEXT("Bakery")),
		FText::FromString(TEXT("Produces bread")), true);
	Nodes = Hud->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Inspector = RootHudTestsFindNode(Nodes, TEXT("HUD.InspectorHost"));
	TestTrue(TEXT("Selection events open the inspector host"), Inspector != nullptr && Inspector->State.bVisible && Inspector->State.Value == TEXT("true"));
	TestTrue(TEXT("Semantic close uses the normal inspector intent"), Hud->ActivateSemanticId(TEXT("Inspector.Close")));
	Nodes = Hud->GetSemanticSnapshot();
	Inspector = RootHudTestsFindNode(Nodes, TEXT("HUD.InspectorHost"));
	TestTrue(TEXT("Closed inspector state is observable"), Inspector != nullptr && !Inspector->State.bVisible && Inspector->State.Value == TEXT("false"));

	TestTrue(TEXT("Fast speed accepts semantic focus"), Hud->FocusSemanticId(TEXT("HUD.TopStatus.Speed.Fast")));
	Nodes = Hud->GetSemanticSnapshot();
	const Hansa::UI::FHansaHudSemanticNode* Fast = RootHudTestsFindNode(Nodes, TEXT("HUD.TopStatus.Speed.Fast"));
	const Hansa::UI::FHansaHudSemanticNode* FocusLayer = RootHudTestsFindNode(Nodes, TEXT("HUD.FocusLayer"));
	TestTrue(TEXT("Focused state is independent of hover"), Fast != nullptr && Fast->State.bFocused);
	TestTrue(TEXT("Hidden focus metadata retains the target without showing developer IDs"), FocusLayer != nullptr && !FocusLayer->State.bVisible &&
		FocusLayer->State.Value == TEXT("HUD.TopStatus.Speed.Fast"));
	TestTrue(TEXT("Focused speed can be activated through the semantic intent"), Hud->ActivateSemanticId(TEXT("HUD.TopStatus.Speed.Fast")));
	TestEqual(TEXT("Activation updates speed without polling"), Model->GetSnapshot().Speed, EHansaHudGameSpeed::Fast);
	return !HasAnyErrors();
}

#endif
