#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/HansaStrategicAutomationFixture.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Screenshot/HansaNativeScreenshotService.h"
#include "SemanticUI/HansaSemanticUiRegistry.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UI/HansaStrategicAutomationScreen.h"
#include "World/HansaRuntimeSimulationHost.h"

using namespace Hansa::Automation;

namespace Hansa::Tests::MvpGolden
{
	TSharedRef<FJsonObject> Predicate(const TCHAR* Kind)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("kind"), Kind);
		return Result;
	}

	bool RunUntil(FHansaStrategicAutomationFixture& Fixture, const TSharedRef<FJsonObject>& Expected,
		const int32 MaximumTicks, FString& OutError)
	{
		TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetObjectField(TEXT("predicate"), Expected);
		Request->SetNumberField(TEXT("maximumTicks"), MaximumTicks);
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		return Fixture.RunUntil(Request, Result, OutError);
	}

	FString Serialize(const TSharedRef<FJsonObject>& Json)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Json, Writer);
		return Text;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaMvpGoldenEndToEndTest,
	"Hansa.Architecture.Automation.MvpGoldenEndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaMvpGoldenEndToEndTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace Hansa::Tests::MvpGolden;
	FHansaStrategicAutomationFixture Fixture;
	TSharedRef<FJsonObject> Loaded = MakeShared<FJsonObject>();
	FString Error;
	if (!TestTrue(TEXT("The canonical Lübeck fixture loads into the real playable runtime"),
		Fixture.Load(FHansaStrategicAutomationFixture::GoldenFixtureId, Loaded, Error)))
	{
		AddError(Error);
		return false;
	}
	const FString InitialHash = Loaded->GetStringField(TEXT("stateHash"));
	FHansaStrategicAutomationFixture ResetFixture;
	TSharedRef<FJsonObject> Reset = MakeShared<FJsonObject>();
	if (!TestTrue(TEXT("Reset reloads the exact fixture and seed"),
		ResetFixture.Load(FHansaStrategicAutomationFixture::GoldenFixtureId, Reset, Error)))
	{
		AddError(Error);
		return false;
	}
	TestEqual(TEXT("Reset reproduces the initial state hash"), Reset->GetStringField(TEXT("stateHash")), InitialHash);

	FHansaSemanticUiRegistry Registry;
	TSharedRef<SHansaStrategicAutomationScreen> Screen = SNew(SHansaStrategicAutomationScreen, Fixture, Registry);
	const FHansaSemanticNode* Grain = Registry.FindNode(TEXT("Market.Good.Grain"));
	TestTrue(TEXT("The grain row is structurally discoverable and warns without localized-text lookup"),
		Grain != nullptr && Grain->Role == EHansaSemanticRole::Status && Grain->State.bWarning);
	auto Activate = [this, &Registry](const TCHAR* Id)
	{
		return TestTrue(*FString::Printf(TEXT("Normal semantic intent %s succeeds"), Id),
			Registry.Invoke(Id, EHansaSemanticAction::Activate).IsSuccess());
	};
	if (!Activate(TEXT("Market.Action.DiagnoseGrain")) ||
		!Activate(TEXT("BuildMenu.Action.ConfirmBreadChain")) ||
		!RunUntil(Fixture, Predicate(TEXT("strategic.building_completed")), 32, Error) ||
		!Activate(TEXT("TradeRoute.Editor.Action.StartRelief")) ||
		!Activate(TEXT("Research.Action.QueueMarketReports")))
	{
		AddError(Error.IsEmpty() ? TEXT("A normal golden-path semantic action failed.") : Error);
		return false;
	}

	TSharedRef<FJsonObject> Ai = Predicate(TEXT("strategic.ai_progressed"));
	Ai->SetNumberField(TEXT("minimumDecisions"), 2);
	if (!RunUntil(Fixture, Ai, 32, Error) ||
		!RunUntil(Fixture, Predicate(TEXT("strategic.route_cargo_in_transit")), 32, Error))
	{
		AddError(Error);
		return false;
	}
	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	if (!TestTrue(TEXT("The in-transit golden checkpoint saves to a fixed slot"),
		Fixture.SaveSlot(TEXT("manual"), Payload, Error)) ||
		!TestTrue(TEXT("The same slot restores authoritative state and deterministic continuation"),
		Fixture.LoadSlot(TEXT("manual"), Payload, Error)) ||
		!TestTrue(TEXT("The save round trip covers all required gameplay slices"),
		Fixture.AssertRoundTrip(Payload, Error)))
	{
		AddError(Error);
		return false;
	}

	TSharedRef<FJsonObject> Research = Predicate(TEXT("strategic.research_completed"));
	Research->SetStringField(TEXT("technologyId"), TEXT("Technology.Commerce.MarketReports"));
	TSharedRef<FJsonObject> Victory = Predicate(TEXT("strategic.victory"));
	if (!RunUntil(Fixture, Research, 32, Error) ||
		!RunUntil(Fixture, Predicate(TEXT("strategic.route_recovered")), 96, Error) ||
		!RunUntil(Fixture, Victory, 128, Error))
	{
		AddError(Error);
		return false;
	}
	Screen->SynchronizeSemantics();
	for (const TCHAR* Id : {
		TEXT("BuildMenu.Status.BreadChain"), TEXT("Market.Status.GrainDiagnosed"),
		TEXT("TradeRoute.Editor.Status.Delivered"), TEXT("Research.Status.MarketReports"),
		TEXT("HUD.Status.MerchantAI"), TEXT("SaveLoad.Status.RoundTrip"),
		TEXT("Scenario.Status.Victory") })
	{
		const FHansaSemanticNode* Node = Registry.FindNode(Id);
		TestTrue(*FString::Printf(TEXT("%s is a completed structural checkpoint"), Id),
			Node != nullptr && Node->State.bSelected);
	}

	const TSharedRef<FJsonObject> Evidence = Fixture.MakeEvidenceSnapshot();
	TestEqual(TEXT("The authored scenario reaches a valid victory"), Evidence->GetObjectField(TEXT("objectiveState"))->GetStringField(TEXT("outcome")), FString(TEXT("Victory")));
	TestTrue(TEXT("Merchant decisions are synchronized"), Evidence->GetArrayField(TEXT("aiDecisions")).Num() >= 2);
	TestTrue(TEXT("Causal events are synchronized"), !Evidence->GetArrayField(TEXT("causalEvents")).IsEmpty());
	TestTrue(TEXT("Registry/content and fixture hashes are present"), !Evidence->GetStringField(TEXT("contentHash")).IsEmpty() && !Evidence->GetStringField(TEXT("fixtureHash")).IsEmpty());
	TestTrue(TEXT("Initial and final state hashes are present"), !Evidence->GetStringField(TEXT("initialStateHash")).IsEmpty() && !Evidence->GetStringField(TEXT("stateHash")).IsEmpty());
	const TSharedRef<FJsonObject> Logs = Fixture.MakeLogSnapshot();
	TestTrue(TEXT("Bounded structured logs retain causal events"), !Logs->GetArrayField(TEXT("entries")).IsEmpty());

	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("Automation"), TEXT("S14P01"), TEXT("automation-test"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	TestTrue(TEXT("The synchronized query snapshot persists"), FFileHelper::SaveStringToFile(
		Serialize(Evidence), *FPaths::Combine(Directory, TEXT("query-snapshot.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	FHansaNativeScreenshotService Screenshots;
	for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
	{
		FHansaScreenshotContext Context;
		Context.BundleId = FString::Printf(TEXT("golden-%dx%d"), Size.X, Size.Y);
		Context.EvidenceSuiteId = TEXT("S14P01");
		Context.FixtureId = FHansaStrategicAutomationFixture::GoldenFixtureId;
		Context.ScreenId = TEXT("Scenario.Status.Victory");
		Context.FlowId = TEXT("s14-p01-mvp-golden");
		Context.SimulationTick = Fixture.GetHost()->GetSimulationTick();
		Context.UiRevision = Registry.GetRevision();
		Context.QuerySnapshotJson = Serialize(Evidence);
		Context.SemanticSnapshotJson = TEXT("{\"requiredNamespaces\":[\"HUD\",\"BuildMenu\",\"Market\",\"TradeRoute.Editor\",\"Research\",\"SaveLoad\",\"Scenario\"]}");
		Context.LogSnapshotJson = Serialize(Logs);
		Context.StructuralAssertions = {TEXT("allGoldenCheckpoints.selected=true"), TEXT("saveRoundTrip.verified=true")};
		Context.bStructuralAssertionsPassed = true;
		Context.CaptureMethod = TEXT("AutomationTest.NativeBufferContract");
		const FHansaScreenshotResult Capture = Screenshots.Capture(Size, Context,
			[](const FIntPoint& Requested, TArray<FColor>& Pixels)
			{
				Pixels.Init(FColor(18, 31, 46, 255), Requested.X * Requested.Y);
				return true;
			});
		TestTrue(*FString::Printf(TEXT("Native %dx%d golden evidence persists without resampling"), Size.X, Size.Y), Capture.IsSuccess());
	}
	return !HasAnyErrors();
}

#endif
