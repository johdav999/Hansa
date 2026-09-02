#include "Commands/HansaGameplayCommandGateway.h"
#include "Diagnostics/HansaDeterminismEvidence.h"
#include "Diagnostics/HansaDeterminismTrace.h"
#include "Diagnostics/HansaStateHash.h"
#include "Dom/JsonObject.h"
#include "Fixtures/HansaDeterministicFixture.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Queries/HansaSimulationProjectionDiff.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Diagnostics
{
	using namespace Hansa::Simulation;

	template <typename TValue>
	TValue RequireValue(const THansaValueResult<TValue>& Result)
	{
		check(Result.IsSuccess());
		return Result.Value;
	}

	template <typename TEntityId>
	TEntityId MakeEntityId(const uint64 Value)
	{
		const THansaValueResult<TEntityId> Result = TEntityId::TryCreate(Value);
		return RequireValue(Result);
	}

	FHansaSimulationTick MakeTick(const int64 Value)
	{
		const THansaValueResult<FHansaSimulationTick> Result = FHansaSimulationTick::TryCreate(Value);
		return RequireValue(Result);
	}

	FHansaSimulationClock MakeClock(const int64 Tick)
	{
		const THansaValueResult<FHansaSimulationVersion> Version = FHansaSimulationVersion::TryCreate(1);
		const THansaValueResult<FHansaSimulationClock> Clock =
			FHansaSimulationClock::TryCreate(Version.Value, MakeTick(Tick));
		return RequireValue(Clock);
	}

	FHansaSimulationDefinitionContext MakeDefinitions()
	{
		const THansaValueResult<FHansaScenarioId> Scenario =
			FHansaScenarioId::TryParse(TEXT("Scenario.FoundationDeterminismV1"));
		const THansaValueResult<FHansaSimulationDefinitionContext> Definitions =
			FHansaSimulationDefinitionContext::TryCreate(Scenario.Value, 0x1020304050607080ULL);
		return RequireValue(Definitions);
	}

	FHansaSimulationInitialization MakeInitialization(const int64 HouseOneMoney = 10'000)
	{
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = MakeClock(0);
		Initialization.CampaignSeed = 0x44556677;
		Initialization.Houses = {
			{ MakeEntityId<FHansaHouseId>(2), FHansaMoney::FromRaw(20'000) },
			{ MakeEntityId<FHansaHouseId>(1), FHansaMoney::FromRaw(HouseOneMoney) }
		};
		Initialization.Cities = {
			{ RequireValue(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))), FHansaQuantity::FromRaw(5'000) }
		};
		const THansaValueResult<FHansaRandomStream> Random =
			FHansaRandomStream::TryCreate(Initialization.CampaignSeed, TEXT("Fixture.Diagnostics"));
		Initialization.RandomStreams.Add(Random.Value);
		return Initialization;
	}

	FHansaSimulationState MakeState(const int64 HouseOneMoney = 10'000)
	{
		const THansaValueResult<FHansaSimulationState> State =
			FHansaSimulationState::TryCreate(MakeInitialization(HouseOneMoney));
		return RequireValue(State);
	}

	FHansaDeterministicFixtureDescriptor MakeDescriptor(const int64 HouseOneMoney = 10'000)
	{
		const THansaValueResult<FHansaDeterministicFixtureDescriptor> Descriptor =
			FHansaDeterministicFixtureDescriptor::TryCreate(
				TEXT("foundation_determinism_v1"),
				1,
				TEXT("Foundation"),
				MakeDefinitions(),
				MakeInitialization(HouseOneMoney));
		return RequireValue(Descriptor);
	}

	FHansaGameplayCommand MakeNoOpCommand(
		const uint64 Identity,
		const int64 Tick,
		const int64 CorrelationValue)
	{
		FHansaCommandHeader Header;
		Header.CommandId = MakeEntityId<FHansaCommandId>(Identity);
		Header.Authority.IssuingHouseId = MakeEntityId<FHansaHouseId>(1);
		Header.Authority.PrincipalId = 1001;
		Header.Authority.Origin = EHansaCommandOrigin::ControlledAutomation;
		Header.RequestedExecutionTick = MakeTick(Tick);
		Header.GlobalSequence = Identity;
		return FHansaGameplayCommand::Create(Header, FHansaNoOpTestCommand { CorrelationValue });
	}

	bool SaveAndParseEvidence(FAutomationTestBase& Test, const FString& Filename, const FString& Json)
	{
		const FString Directory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("TestEvidence"),
			TEXT("foundation_determinism_v1"),
			TEXT("automation"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, Filename);
		if (!FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			Test.AddError(FString::Printf(TEXT("Failed to write diagnostic evidence: %s"), *Path));
			return false;
		}

		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("Diagnostic evidence is not valid JSON: %s"), *Path));
			return false;
		}
		return true;
	}

	bool LoadExpectedFixtureChecksum(FAutomationTestBase& Test, FString& OutChecksum)
	{
		const FString Path = FPaths::Combine(
			FPaths::ProjectDir(), TEXT("Tests"), TEXT("Fixtures"), TEXT("foundation_determinism_v1.json"));
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			Test.AddError(FString::Printf(TEXT("Failed to load reviewed fixture descriptor: %s"), *Path));
			return false;
		}
		TSharedPtr<FJsonObject> Parsed;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("Reviewed fixture descriptor is invalid JSON: %s"), *Path));
			return false;
		}
		const TSharedPtr<FJsonObject>* Expected = nullptr;
		if (!Parsed->TryGetObjectField(TEXT("expected"), Expected) || Expected == nullptr ||
			!(*Expected)->TryGetStringField(TEXT("finalChecksum"), OutChecksum))
		{
			Test.AddError(TEXT("Reviewed fixture descriptor lacks expected.finalChecksum"));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaNormalizedStateHashTest,
	"Hansa.Simulation.Diagnostics.NormalizedStateHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaNormalizedStateHashTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Diagnostics;

	FHansaSimulationState State = MakeState();
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
	const FHansaStateHashReport Report = View.BuildStateHashReport();
	TestEqual(TEXT("Hash format is explicitly versioned"),
		Report.GetHashFormatVersion(), FHansaStateHashReport::CurrentHashFormatVersion);
	TestEqual(TEXT("Normalization rules are explicitly versioned"),
		Report.GetNormalizationVersion(), FHansaStateHashReport::CurrentNormalizationVersion);
	TestEqual(TEXT("All current authoritative subsystems are reported"), Report.GetSubsystems().Num(), 9);
	TestEqual(TEXT("Global fingerprint is derived from the normalized report"),
		View.GetFingerprint().Value, Report.GetOverallHash());
	TestEqual(TEXT("Fingerprint contract advanced for normalized subsystem hashing"),
		FHansaSimulationState::DeterminismFingerprintVersion, uint32(3));
	TestTrue(TEXT("A relevant subsystem can be located without parsing text"),
		Report.Find(EHansaStateHashSubsystem::Houses) != nullptr);
	TestTrue(TEXT("Compact summary names relevant subsystems"), Report.ToCompactDebugString().Contains(TEXT("Houses=")));

	FHansaSimulationTransientCache Cache;
	const FHansaCommandGatewayResult Step =
		FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache);
	TestTrue(TEXT("Empty headless tick succeeds"), Step.IsSuccess());
	const FHansaStateHashReport BeforeDiscard = State.CreateReadOnlyAccess(Definitions).BuildStateHashReport();
	Cache.Discard();
	const FHansaStateHashReport AfterDiscard = State.CreateReadOnlyAccess(Definitions).BuildStateHashReport();
	TestTrue(TEXT("Transient cache contents and rebuild counters are excluded from hashes"),
		BeforeDiscard == AfterDiscard);

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaProjectionDiffTest,
	"Hansa.Simulation.Diagnostics.ProjectionDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaProjectionDiffTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Diagnostics;

	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState BeforeState = MakeState(10'000);
	FHansaSimulationState AfterState = MakeState(10'001);
	const THansaValueResult<FHansaSimulationProjection> Before =
		BeforeState.CreateReadOnlyAccess(Definitions).BuildProjection();
	const THansaValueResult<FHansaSimulationProjection> After =
		AfterState.CreateReadOnlyAccess(Definitions).BuildProjection();
	TestTrue(TEXT("Projection setup succeeds"), Before && After);
	if (!Before || !After)
	{
		return false;
	}

	const FHansaSimulationProjectionDiff Equal =
		FHansaSimulationProjectionDiff::Compare(Before.Value, Before.Value);
	TestTrue(TEXT("Equal projections produce an empty diff"), Equal.IsEmpty());
	const FHansaSimulationProjectionDiff Difference =
		FHansaSimulationProjectionDiff::Compare(Before.Value, After.Value);
	TestTrue(TEXT("Changed authoritative projection is detected"), !Difference.IsEmpty());
	bool bFoundHouseMoney = false;
	for (const FHansaProjectionDiffEntry& Entry : Difference.GetEntries())
	{
		bFoundHouseMoney |= Entry.Field == EHansaProjectionDiffField::HouseMoney &&
			Entry.StableKey == TEXT("House#1@0.Money") &&
			Entry.BeforeValue == TEXT("10000") && Entry.AfterValue == TEXT("10001");
	}
	TestTrue(TEXT("Projection diff identifies the stable house field and values"), bFoundHouseMoney);
	TestTrue(TEXT("Projection diff exposes a bounded compact summary"),
		Difference.ToCompactDebugString(2).Contains(TEXT("HouseMoney")));

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaNamedFixtureEvidenceTest,
	"Hansa.Simulation.Diagnostics.NamedFixtureEvidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaNamedFixtureEvidenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Diagnostics;

	const FHansaDeterministicFixtureDescriptor Descriptor = MakeDescriptor();
	TArray<FHansaGameplayCommand> Commands = {
		MakeNoOpCommand(1, 2, 77),
		MakeNoOpCommand(2, 4, 88)
	};
	const FHansaFixtureRunResult Run =
		FHansaDeterministicFixtureHarness::RunExactTicks(Descriptor, 6, Commands);
	TestTrue(TEXT("Named descriptor runs headlessly"), Run.IsSuccess());
	if (!Run)
	{
		return false;
	}
	TestEqual(TEXT("Harness advances the exact requested ticks"),
		Run.GetFinalProjection().GetClock().GetTick().GetValue(), int64(6));
	TestEqual(TEXT("Trace retains one diagnostic record per processed tick"), Run.GetTrace().GetTicks().Num(), 6);
	TestEqual(TEXT("Named identity is retained in evidence"),
		Run.GetTrace().GetFixtureId(), FString(TEXT("foundation_determinism_v1")));
	TestEqual(TEXT("Scheduled typed commands use the normal gateway"),
		Run.GetFinalProjection().GetProcessedCommandCount(), uint64(2));
	FString ExpectedChecksum;
	if (LoadExpectedFixtureChecksum(*this, ExpectedChecksum))
	{
		const FString ActualChecksum = FString::Printf(
			TEXT("%016llX"),
			static_cast<unsigned long long>(Run.GetFinalProjection().GetFingerprint().Value));
		TestEqual(TEXT("Reviewed fixture descriptor locks the expected final checksum"),
			ActualChecksum, ExpectedChecksum);
	}

	const FString Json = FHansaDeterminismEvidenceWriter::WriteRunJson(Run);
	TestTrue(TEXT("Evidence contains versioned subsystem hashes"), Json.Contains(TEXT("\"subsystems\"")));
	TestTrue(TEXT("Machine-readable run evidence is written under Saved/TestEvidence"),
		SaveAndParseEvidence(*this, TEXT("determinism-run.json"), Json));

	const FHansaFixtureRunResult NegativeRun =
		FHansaDeterministicFixtureHarness::RunExactTicks(Descriptor, -1);
	TestTrue(TEXT("Invalid exact-tick requests fail structurally"),
		NegativeRun.GetError() == EHansaFixtureRunError::NegativeTickCount);

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaFirstDivergenceTest,
	"Hansa.Simulation.Diagnostics.FirstDivergence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaFirstDivergenceTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Diagnostics;

	const FHansaDeterministicFixtureDescriptor Descriptor = MakeDescriptor();
	TArray<FHansaGameplayCommand> LeftCommands = {
		MakeNoOpCommand(1, 3, 100)
	};
	TArray<FHansaGameplayCommand> RightCommands = {
		MakeNoOpCommand(1, 3, 101)
	};
	const FHansaFixtureRunResult Left =
		FHansaDeterministicFixtureHarness::RunExactTicks(Descriptor, 8, LeftCommands);
	const FHansaFixtureRunResult Right =
		FHansaDeterministicFixtureHarness::RunExactTicks(Descriptor, 8, RightCommands);
	TestTrue(TEXT("Both replay traces complete"), Left && Right);
	if (!Left || !Right)
	{
		return false;
	}

	const FHansaDeterminismComparison Comparison =
		FHansaDeterminismDiagnostics::Compare(Left.GetTrace(), Right.GetTrace());
	TestTrue(TEXT("Divergent replay is detected"), !Comparison.IsEqual());
	TestEqual(TEXT("First divergent processed tick is identified"),
		Comparison.GetFirstDivergentTick().GetValue(), int64(3));
	TestTrue(TEXT("The relevant event-order subsystem is identified"),
		Comparison.GetKind() == EHansaDeterminismDivergenceKind::DomainEventOrder);
	TestTrue(TEXT("Compact evidence contains first tick and cause"),
		Comparison.ToCompactDebugString().Contains(TEXT("firstTick=3")) &&
		Comparison.ToCompactDebugString().Contains(TEXT("DomainEventOrder")));

	const FString Json = FHansaDeterminismEvidenceWriter::WriteComparisonJson(
		Descriptor.GetFixtureId(), Comparison);
	TestTrue(TEXT("Machine-readable divergence evidence is written under Saved/TestEvidence"),
		SaveAndParseEvidence(*this, TEXT("first-divergence.json"), Json));

	const FHansaFixtureRunResult DifferentHouse =
		FHansaDeterministicFixtureHarness::RunExactTicks(MakeDescriptor(10'001), 1);
	const FHansaFixtureRunResult BaselineHouse =
		FHansaDeterministicFixtureHarness::RunExactTicks(Descriptor, 1);
	const FHansaDeterminismComparison HouseComparison =
		FHansaDeterminismDiagnostics::Compare(BaselineHouse.GetTrace(), DifferentHouse.GetTrace());
	TestTrue(TEXT("Initial divergence identifies the authoritative house subsystem"),
		HouseComparison.GetKind() == EHansaDeterminismDivergenceKind::StateSubsystem &&
		HouseComparison.GetSubsystem() == EHansaStateHashSubsystem::Houses);

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaPipelineOrderDriftTest,
	"Hansa.Simulation.Diagnostics.PipelineOrderDrift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaPipelineOrderDriftTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Diagnostics;

	const FHansaFixtureRunResult Run =
		FHansaDeterministicFixtureHarness::RunExactTicks(MakeDescriptor(), 7);
	TestTrue(TEXT("Baseline trace completes"), Run.IsSuccess());
	if (!Run)
	{
		return false;
	}

	TArray<FHansaDeterminismTickRecord> DriftedTicks;
	DriftedTicks.Append(Run.GetTrace().GetTicks().GetData(), Run.GetTrace().GetTicks().Num());
	TArray<EHansaSimulationPhase> DriftedOrder;
	DriftedOrder.Append(
		FHansaSimulationPipeline::GetOrderedPhases().GetData(),
		FHansaSimulationPipeline::GetOrderedPhases().Num());
	DriftedOrder.Swap(2, 3);
	DriftedTicks[4].PipelineOrderHash =
		FHansaDeterminismDiagnostics::ComputePipelineOrderHash(DriftedOrder);
	const THansaValueResult<FHansaDeterminismTrace> DriftedTrace = FHansaDeterminismTrace::TryCreate(
		Run.GetTrace().GetFixtureId(),
		Run.GetTrace().GetFixtureSchemaVersion(),
		Run.GetTrace().GetSeed(),
		Run.GetTrace().GetDefinitionHash(),
		Run.GetTrace().GetInitialState(),
		MoveTemp(DriftedTicks));
	TestTrue(TEXT("A structurally valid diagnostic trace can represent another implementation"), DriftedTrace.IsSuccess());
	if (!DriftedTrace)
	{
		return false;
	}

	const FHansaDeterminismComparison Comparison =
		FHansaDeterminismDiagnostics::Compare(Run.GetTrace(), DriftedTrace.Value);
	TestTrue(TEXT("Intentional phase-order drift is detected"),
		Comparison.GetKind() == EHansaDeterminismDivergenceKind::PipelineOrder);
	TestEqual(TEXT("Order drift reports its first affected tick"),
		Comparison.GetFirstDivergentTick().GetValue(), int64(4));
	TestTrue(TEXT("Order-drift values identify the two phase contracts"),
		Comparison.GetLeftValue() != Comparison.GetRightValue());

	return !HasAnyErrors();
}

#endif
