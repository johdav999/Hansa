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

namespace Hansa::Tests::StrategicVerticalSlice
{
	FString Serialize(const TSharedRef<FJsonObject>& Json)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Json, Writer);
		return Text;
	}

	TSharedRef<FJsonObject> Predicate(const TCHAR* Kind)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("kind"), Kind);
		return Result;
	}

	bool RunUntil(FHansaStrategicAutomationFixture& Fixture, const TSharedRef<FJsonObject>& PredicateObject,
		const int32 MaximumTicks, FString& OutError)
	{
		TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetObjectField(TEXT("predicate"), PredicateObject);
		Request->SetNumberField(TEXT("maximumTicks"), MaximumTicks);
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		return Fixture.RunUntil(Request, Out, OutError);
	}

	bool RunOne(FAutomationTestBase& Test, const FString& FixtureId, const bool bWriteEvidence,
		FString& OutStateHash, FString& OutEvidence)
	{
		FHansaStrategicAutomationFixture Fixture;
		TSharedRef<FJsonObject> Loaded = MakeShared<FJsonObject>();
		FString Error;
		if (!Test.TestTrue(*FString::Printf(TEXT("%s initializes the real playable runtime"), *FixtureId),
			Fixture.Load(FixtureId, Loaded, Error))) { Test.AddError(Error); return false; }
		FHansaSemanticUiRegistry Registry;
		TSharedRef<SHansaStrategicAutomationScreen> Screen = SNew(SHansaStrategicAutomationScreen, Fixture, Registry);
		auto Activate = [&Test, &Registry](const TCHAR* Id)
		{
			return Test.TestTrue(*FString::Printf(TEXT("Semantic operation %s succeeds"), Id),
				Registry.Invoke(Id, EHansaSemanticAction::Activate).IsSuccess());
		};
		if (!Activate(TEXT("Strategic.Action.Build")) ||
			!RunUntil(Fixture, Predicate(TEXT("strategic.shortage_diagnosed")), 8, Error) ||
			!Activate(TEXT("Strategic.Action.Diagnose")) ||
			!RunUntil(Fixture, Predicate(TEXT("strategic.building_completed")), 32, Error) ||
			!Activate(TEXT("Strategic.Action.QueueResearch")) ||
			!Activate(TEXT("Strategic.Action.StartRoutes")))
		{
			Test.AddError(Error.IsEmpty() ? TEXT("A strategic semantic checkpoint failed.") : Error);
			return false;
		}
		TSharedRef<FJsonObject> Research = Predicate(TEXT("strategic.research_completed"));
		Research->SetStringField(TEXT("technologyId"), TEXT("Technology.Commerce.MarketReports"));
		TSharedRef<FJsonObject> Ai = Predicate(TEXT("strategic.ai_progressed"));
		Ai->SetNumberField(TEXT("minimumDecisions"), 2);
		TSharedRef<FJsonObject> Victory = Predicate(TEXT("strategic.victory"));
		Victory->SetStringField(TEXT("victoryId"), TEXT("Victory.TradeNetwork"));
		if (!RunUntil(Fixture, Research, 16, Error) || !RunUntil(Fixture, Ai, 32, Error) ||
			!RunUntil(Fixture, Predicate(TEXT("strategic.route_recovered")), 64, Error) ||
			!RunUntil(Fixture, Victory, 64, Error))
		{
			Test.AddError(Error);
			return false;
		}
		Screen->SynchronizeSemantics();
		for (const TCHAR* Id : { TEXT("Strategic.Status.Building"), TEXT("Strategic.Status.Shortage"),
			TEXT("Strategic.Status.RouteRecovery"), TEXT("Strategic.Status.Research"),
			TEXT("Strategic.Status.AI"), TEXT("Strategic.Status.Victory") })
		{
			const FHansaSemanticNode* Node = Registry.FindNode(Id);
			Test.TestTrue(*FString::Printf(TEXT("%s is an observable completed checkpoint"), Id),
				Node != nullptr && Node->State.bSelected);
		}
		const TSharedRef<FJsonObject> Evidence = Fixture.MakeEvidenceSnapshot();
		OutStateHash = Evidence->GetStringField(TEXT("stateHash"));
		OutEvidence = Serialize(Evidence);
		Test.TestTrue(TEXT("Evidence bundles AI decisions"), !Evidence->GetArrayField(TEXT("aiDecisions")).IsEmpty());
		Test.TestTrue(TEXT("Evidence bundles causal gameplay and research events"), !Evidence->GetArrayField(TEXT("causalEvents")).IsEmpty());
		Test.TestEqual(TEXT("The strategic flow reaches the authored trade-network ending"),
			Evidence->GetStringField(TEXT("winningVictoryId")), FString(TEXT("Victory.TradeNetwork")));
		if (bWriteEvidence)
		{
			const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TestEvidence"), TEXT("StrategicVerticalSlice"), FixtureId);
			IFileManager::Get().MakeDirectory(*Directory, true);
			Test.TestTrue(TEXT("The causal strategic evidence JSON is persisted"), FFileHelper::SaveStringToFile(
				OutEvidence, *FPaths::Combine(Directory, TEXT("strategic-victory.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
			FHansaNativeScreenshotService Screenshots;
			for (const FIntPoint Size : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
			{
				FHansaScreenshotContext Context; Context.BundleId = FString::Printf(TEXT("%s-%dx%d"), *FixtureId, Size.X, Size.Y);
				Context.EvidenceSuiteId = TEXT("S10P04"); Context.FixtureId = FixtureId; Context.ScreenId = TEXT("Strategic.Root");
				Context.FlowId = TEXT("strategic-vertical-slice-v1"); Context.SimulationTick = Fixture.GetHost()->GetSimulationTick();
				Context.UiRevision = Registry.GetRevision(); Context.QuerySnapshotJson = OutEvidence;
				Context.SemanticSnapshotJson = TEXT("{\"required\":[\"Strategic.Status.Building\",\"Strategic.Status.Shortage\",\"Strategic.Status.RouteRecovery\",\"Strategic.Status.Research\",\"Strategic.Status.AI\",\"Strategic.Status.Victory\"]}");
				Context.StructuralAssertions = {TEXT("allStrategicCheckpoints.selected=true"), TEXT("causalEvidence.synchronized=true")};
				Context.bStructuralAssertionsPassed = true; Context.CaptureMethod = TEXT("AutomationTest.NativeBufferContract");
				const FHansaScreenshotResult Capture = Screenshots.Capture(Size, Context,
					[](const FIntPoint& Requested, TArray<FColor>& Pixels) { Pixels.Init(FColor(18, 31, 46, 255), Requested.X * Requested.Y); return true; });
				Test.TestTrue(*FString::Printf(TEXT("Native %dx%d S10-P04 checkpoint capture persists"), Size.X, Size.Y), Capture.IsSuccess());
			}
		}
		return !Test.HasAnyErrors();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHansaStrategicVerticalSliceMultiSeedTest,
	"Hansa.Architecture.Automation.StrategicVerticalSlice.MultiSeedDeterministicVictory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaStrategicVerticalSliceMultiSeedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace Hansa::Tests::StrategicVerticalSlice;
	for (const FString FixtureId : {FString(FHansaStrategicAutomationFixture::SeedAlphaFixtureId),
		FString(FHansaStrategicAutomationFixture::SeedBetaFixtureId)})
	{
		FString FirstHash, ReplayHash, FirstEvidence, ReplayEvidence;
		if (!RunOne(*this, FixtureId, true, FirstHash, FirstEvidence) ||
			!RunOne(*this, FixtureId, false, ReplayHash, ReplayEvidence)) return false;
		TestEqual(*FString::Printf(TEXT("%s replay produces the same final state hash"), *FixtureId), ReplayHash, FirstHash);
		TestEqual(*FString::Printf(TEXT("%s replay produces byte-identical causal evidence"), *FixtureId), ReplayEvidence, FirstEvidence);
	}
	return !HasAnyErrors();
}

#endif
