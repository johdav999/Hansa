#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Math/NumericLimits.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Queries/HansaSimulationReadOnly.h"
#include "Systems/HansaSimulationPipeline.h"

#include <type_traits>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Kernel
{
	using namespace Hansa::Simulation;

	using FReadOnlyHouseElement = std::remove_reference_t<decltype(
		std::declval<FHansaSimulationReadOnlyAccess>().GetHouses()[0])>;
	static_assert(std::is_const_v<FReadOnlyHouseElement>);
	static_assert(!std::is_pointer_v<FHansaSimulationState>);

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

	FHansaSimulationVersion MakeVersion(const uint32 Value = 1)
	{
		const THansaValueResult<FHansaSimulationVersion> Result = FHansaSimulationVersion::TryCreate(Value);
		return RequireValue(Result);
	}

	FHansaSimulationTick MakeTick(const int64 Value)
	{
		const THansaValueResult<FHansaSimulationTick> Result = FHansaSimulationTick::TryCreate(Value);
		return RequireValue(Result);
	}

	FHansaSimulationClock MakeClock(const FHansaSimulationTick Tick)
	{
		const THansaValueResult<FHansaSimulationClock> Result = FHansaSimulationClock::TryCreate(MakeVersion(), Tick);
		return RequireValue(Result);
	}

	FHansaRandomStream MakeRandomStream(const uint64 Seed, const FString& Name)
	{
		const THansaValueResult<FHansaRandomStream> Result = FHansaRandomStream::TryCreate(Seed, Name);
		return RequireValue(Result);
	}

	FHansaSimulationState MakeInitializedState(FHansaSimulationInitialization Initialization)
	{
		const THansaValueResult<FHansaSimulationState> Result = FHansaSimulationState::TryCreate(MoveTemp(Initialization));
		return RequireValue(Result);
	}

	FHansaSimulationDefinitionContext MakeDefinitions(const uint64 DefinitionHash = 0x4a3b2c1d88776655ULL)
	{
		const FHansaScenarioId ScenarioId = RequireValue(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")));
		const THansaValueResult<FHansaSimulationDefinitionContext> Result =
			FHansaSimulationDefinitionContext::TryCreate(ScenarioId, DefinitionHash);
		return RequireValue(Result);
	}

	FHansaSimulationInitialization MakeInitialization(const uint64 Seed, const bool bReverseDiscoveryOrder = false)
	{
		FHansaSimulationInitialization Initialization;
		const FHansaSimulationTick Tick = MakeTick(0);
		Initialization.Clock = MakeClock(Tick);
		Initialization.CampaignSeed = Seed;

		const FHansaHouseState HouseOne {
			MakeEntityId<FHansaHouseId>(1),
			FHansaMoney::FromRaw(125'000)
		};
		const FHansaHouseState HouseTwo {
			MakeEntityId<FHansaHouseId>(2),
			FHansaMoney::FromRaw(75'000)
		};
		const FHansaCityState Lubeck {
			RequireValue(FHansaCityDefinitionId::TryParse(TEXT("City.Lubeck"))),
			FHansaQuantity::FromRaw(4'000)
		};
		const FHansaCityState Hamburg {
			RequireValue(FHansaCityDefinitionId::TryParse(TEXT("City.Hamburg"))),
			FHansaQuantity::FromRaw(9'000)
		};

		if (bReverseDiscoveryOrder)
		{
			Initialization.Houses = { HouseTwo, HouseOne };
			Initialization.Cities = { Lubeck, Hamburg };
			Initialization.RandomStreams = {
				MakeRandomStream(Seed, TEXT("Market.Price")),
				MakeRandomStream(Seed, TEXT("Kernel.Events"))
			};
		}
		else
		{
			Initialization.Houses = { HouseOne, HouseTwo };
			Initialization.Cities = { Hamburg, Lubeck };
			Initialization.RandomStreams = {
				MakeRandomStream(Seed, TEXT("Kernel.Events")),
				MakeRandomStream(Seed, TEXT("Market.Price"))
			};
		}

		Initialization.Buildings.Add({
			MakeEntityId<FHansaBuildingId>(20),
			RequireValue(FHansaBuildingTypeId::TryParse(TEXT("Building.Bakery.Small"))),
			HouseOne.Id,
			FHansaRate::FromPartsPerMillion(500'000)
		});
		Initialization.Vehicles.Add({
			MakeEntityId<FHansaVehicleId>(30),
			RequireValue(FHansaVehicleDefinitionId::TryParse(TEXT("Vehicle.Cog.Small"))),
			HouseOne.Id,
			FHansaQuantity::FromRaw(2'500)
		});
		Initialization.Routes.Add({
			MakeEntityId<FHansaRouteId>(40),
			HouseOne.Id,
			Initialization.Vehicles[0].Id,
			FHansaRate::FromPartsPerMillion(250'000)
		});
		return Initialization;
	}

	FHansaSimulationState MakeState(const uint64 Seed, const bool bReverseDiscoveryOrder = false)
	{
		return MakeInitializedState(MakeInitialization(Seed, bReverseDiscoveryOrder));
	}

	FHansaCommandHeader MakeCommandHeader(
		const uint64 CommandId,
		const uint64 Sequence,
		const FHansaSimulationTick Tick,
		const uint64 HouseId = 1,
		const EHansaCommandOrigin Origin = EHansaCommandOrigin::PlayerInput)
	{
		FHansaCommandHeader Header;
		Header.CommandId = MakeEntityId<FHansaCommandId>(CommandId);
		Header.Authority.IssuingHouseId = MakeEntityId<FHansaHouseId>(HouseId);
		Header.Authority.PrincipalId = HouseId * 100;
		Header.Authority.Origin = Origin;
		Header.RequestedExecutionTick = Tick;
		Header.GlobalSequence = Sequence;
		return Header;
	}

	FHansaGameplayCommand MakeNoOpCommand(
		const uint64 CommandId,
		const uint64 Sequence,
		const FHansaSimulationTick Tick,
		const int64 CorrelationValue)
	{
		return FHansaGameplayCommand::Create(
			MakeCommandHeader(CommandId, Sequence, Tick),
			FHansaNoOpTestCommand { CorrelationValue });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSimulationStateCanonicalizationTest,
	"Hansa.Simulation.Kernel.StateCanonicalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSimulationStateCanonicalizationTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Kernel;

	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	const THansaValueResult<FHansaSimulationState> ForwardResult = FHansaSimulationState::TryCreate(MakeInitialization(77, false));
	const THansaValueResult<FHansaSimulationState> ReverseResult = FHansaSimulationState::TryCreate(MakeInitialization(77, true));
	TestTrue(TEXT("Valid plain-record state initializes"), ForwardResult && ReverseResult);
	if (!ForwardResult || !ReverseResult)
	{
		return false;
	}

	FHansaSimulationState Forward = ForwardResult.Value;
	FHansaSimulationState Reverse = ReverseResult.Value;
	const FHansaSimulationReadOnlyAccess ForwardView = Forward.CreateReadOnlyAccess(Definitions);
	const FHansaSimulationReadOnlyAccess ReverseView = Reverse.CreateReadOnlyAccess(Definitions);
	TestEqual(TEXT("House records are canonicalized by numeric ID"), ForwardView.GetHouses()[0].Id.GetValue(), uint64(1));
	TestEqual(TEXT("City records are canonicalized by definition ID"),
		ForwardView.GetCities()[0].DefinitionId.ToString(), FString(TEXT("City.Hamburg")));
	TestTrue(TEXT("Discovery order does not affect state fingerprint"),
		ForwardView.GetFingerprint() == ReverseView.GetFingerprint());

	FHansaSimulationInitialization DuplicateHouse = MakeInitialization(77);
	const FHansaHouseState DuplicateHouseRecord = DuplicateHouse.Houses[0];
	DuplicateHouse.Houses.Add(DuplicateHouseRecord);
	TestTrue(TEXT("Duplicate runtime IDs are rejected"),
		FHansaSimulationState::TryCreate(DuplicateHouse).Error == EHansaValueError::InvalidFormat);

	FHansaSimulationInitialization MissingOwner = MakeInitialization(77);
	MissingOwner.Buildings[0].OwnerId = MakeEntityId<FHansaHouseId>(999);
	TestTrue(TEXT("Missing entity references are rejected"),
		FHansaSimulationState::TryCreate(MissingOwner).Error == EHansaValueError::InvalidFormat);

	FHansaSimulationInitialization NegativeCargo = MakeInitialization(77);
	NegativeCargo.Vehicles[0].Cargo = FHansaQuantity::FromRaw(-1);
	TestTrue(TEXT("Invalid authoritative ranges are rejected"),
		FHansaSimulationState::TryCreate(NegativeCargo).Error == EHansaValueError::OutOfRange);

	FHansaSimulationInitialization InvalidCommandRestore = MakeInitialization(77);
	InvalidCommandRestore.LastProcessedCommandSequence = 1;
	TestTrue(TEXT("Inconsistent restored command metadata is rejected"),
		FHansaSimulationState::TryCreate(InvalidCommandRestore).Error == EHansaValueError::InvalidFormat);

	const FHansaScenarioId ScenarioId = RequireValue(FHansaScenarioId::TryParse(TEXT("Scenario.LubeckGrainShortageV1")));
	TestTrue(TEXT("Definition context hash zero is invalid"),
		FHansaSimulationDefinitionContext::TryCreate(ScenarioId, 0).Error == EHansaValueError::InvalidZero);

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSimulationPipelineOrderTest,
	"Hansa.Simulation.Kernel.PipelineOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSimulationPipelineOrderTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Kernel;

	const TConstArrayView<EHansaSimulationPhase> OrderedPhases = FHansaSimulationPipeline::GetOrderedPhases();
	TestEqual(TEXT("The versioned kernel exposes eleven explicit phases"), OrderedPhases.Num(), 11);
	TestTrue(TEXT("Commands are always the first phase"), OrderedPhases[0] == EHansaSimulationPhase::ApplyCommands);
	TestTrue(TEXT("Publishing/checksum is always the final phase"), OrderedPhases.Last() == EHansaSimulationPhase::PublishAndChecksum);
	TestEqual(TEXT("Pipeline version matches the state fingerprint contract"),
		FHansaSimulationPipeline::CurrentPipelineVersion, FHansaSimulationState::CurrentSystemPipelineVersion);

	FHansaSimulationState State = MakeState(100);
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	const FHansaDeterminismFingerprint Before = State.CreateReadOnlyAccess(Definitions).GetFingerprint();
	TArray<FHansaGameplayCommand> Commands = {
		MakeNoOpCommand(1, 1, MakeTick(0), 0x1111),
		MakeNoOpCommand(2, 2, MakeTick(0), 0x2222)
	};
	FHansaSimulationTransientCache Cache;
	const FHansaCommandGatewayResult Result = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
	TestTrue(TEXT("One fixed step succeeds"), Result.IsSuccess());
	TestEqual(TEXT("A fixed step advances exactly one tick"), Result.GetTickAfter().GetValue(), int64(1));
	TestTrue(TEXT("A successful step changes the authoritative fingerprint"), !(Result.GetFingerprintAfter() == Before));

	const FHansaSimulationReadOnlyAccess View = State.CreateReadOnlyAccess(Definitions);
	TestEqual(TEXT("Accepted typed commands are counted"), View.GetProcessedCommandCount(), uint64(2));
	TestEqual(TEXT("Global command sequence is retained"), View.GetLastProcessedCommandSequence(), uint64(2));
	TestEqual(TEXT("Stable command identity is retained"), View.GetLastProcessedCommandId().GetValue(), uint64(2));
	TestEqual(TEXT("Commands publish ordered immutable events"), Result.GetEvents().Num(), 2);
	TestEqual(TEXT("Transient cache is prepared for the tick being processed"), Cache.GetPreparedForTick().GetValue(), int64(0));
	TestEqual(TEXT("Transient cache contains only derived entity count"), Cache.GetCachedEntityCount(), int64(7));
	TestEqual(TEXT("Every configured phase ran once"), Cache.GetLastPhaseOrder().Num(), OrderedPhases.Num());
	for (int32 Index = 0; Index < OrderedPhases.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Phase %d ran in versioned order"), Index), Cache.GetLastPhaseOrder()[Index] == OrderedPhases[Index]);
	}

	const FHansaDeterminismFingerprint BeforeDiscard = View.GetFingerprint();
	Cache.Discard();
	TestTrue(TEXT("Discarding transient caches cannot affect authoritative state"),
		State.CreateReadOnlyAccess(Definitions).GetFingerprint() == BeforeDiscard);

	FHansaSimulationState DifferentCommandState = MakeState(100);
	TArray<FHansaGameplayCommand> DifferentCommands = {
		MakeNoOpCommand(1, 1, MakeTick(0), 0x9999),
		MakeNoOpCommand(2, 2, MakeTick(0), 0x2222)
	};
	FHansaSimulationTransientCache DifferentCache;
	const FHansaCommandGatewayResult DifferentResult = FHansaGameplayCommandGateway::ExecuteTick(
		DifferentCommandState, Definitions, DifferentCommands, DifferentCache);
	TestTrue(TEXT("Different accepted command history produces a different fingerprint"),
		DifferentResult.IsSuccess() && DifferentResult.GetFingerprintAfter() != Result.GetFingerprintAfter());

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSimulationLongRunDeterminismTest,
	"Hansa.Simulation.Kernel.LongRunDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSimulationLongRunDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Kernel;

	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState First = MakeState(0xabcdef, false);
	FHansaSimulationState Second = MakeState(0xabcdef, true);
	FHansaSimulationState DifferentSeed = MakeState(0xabcdee, false);
	FHansaSimulationTransientCache FirstCache;
	FHansaSimulationTransientCache SecondCache;
	FHansaSimulationTransientCache DifferentCache;
	uint64 NextSequence = 1;
	bool bAllEqual = true;
	bool bDifferentSeedDiverged = First.CreateReadOnlyAccess(Definitions).GetFingerprint() !=
		DifferentSeed.CreateReadOnlyAccess(Definitions).GetFingerprint();

	for (int32 TickIndex = 0; TickIndex < 1'000; ++TickIndex)
	{
		TArray<FHansaGameplayCommand> Commands;
		if (TickIndex % 7 == 0)
		{
			const FHansaSimulationTick ScheduledTick = MakeTick(TickIndex);
			const uint64 FirstSequence = NextSequence++;
			Commands.Add(MakeNoOpCommand(FirstSequence, FirstSequence, ScheduledTick, static_cast<int64>(TickIndex) * 17 + 1));
			const uint64 SecondSequence = NextSequence++;
			Commands.Add(MakeNoOpCommand(SecondSequence, SecondSequence, ScheduledTick, static_cast<int64>(TickIndex) * 17 + 2));
		}

		const FHansaCommandGatewayResult FirstStep = FHansaGameplayCommandGateway::ExecuteTick(First, Definitions, Commands, FirstCache);
		const FHansaCommandGatewayResult SecondStep = FHansaGameplayCommandGateway::ExecuteTick(Second, Definitions, Commands, SecondCache);
		const FHansaCommandGatewayResult DifferentStep = FHansaGameplayCommandGateway::ExecuteTick(DifferentSeed, Definitions, Commands, DifferentCache);
		bAllEqual &= FirstStep && SecondStep && FirstStep.GetFingerprintAfter() == SecondStep.GetFingerprintAfter();
		bDifferentSeedDiverged |= DifferentStep.GetFingerprintAfter() != FirstStep.GetFingerprintAfter();
	}

	TestTrue(TEXT("Equal canonical state, seed and command stream remain identical for 1000 ticks"), bAllEqual);
	TestTrue(TEXT("Campaign seed participates in authoritative fingerprints"), bDifferentSeedDiverged);
	TestEqual(TEXT("Long run advances the exact requested tick count"),
		First.CreateReadOnlyAccess(Definitions).GetClock().GetTick().GetValue(), int64(1'000));
	TestTrue(TEXT("Transient cache lifecycle cannot create divergence"),
		First.CreateReadOnlyAccess(Definitions).GetFingerprint() == Second.CreateReadOnlyAccess(Definitions).GetFingerprint());

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSimulationReadOnlyProjectionTest,
	"Hansa.Simulation.Kernel.ReadOnlyProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSimulationReadOnlyProjectionTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Kernel;

	FHansaSimulationState State = MakeState(55, true);
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	const FHansaSimulationReadOnlyAccess InitialView = State.CreateReadOnlyAccess(Definitions);
	const FHansaSimulationSnapshot Snapshot = InitialView.CaptureSnapshot();
	const THansaValueResult<FHansaSimulationProjection> Projection = InitialView.BuildProjection();
	TestTrue(TEXT("Purpose-built projection succeeds"), Projection.IsSuccess());
	if (Projection)
	{
		TestEqual(TEXT("Projection reports canonical house summaries"), Projection.Value.GetHouses().Num(), 2);
		TestEqual(TEXT("Projection preserves canonical house order"), Projection.Value.GetHouses()[0].Id.GetValue(), uint64(1));
		TestEqual(TEXT("Projection reports cities without exposing their container"), Projection.Value.GetCityCount(), 2);
		TestEqual(TEXT("Projection reports buildings"), Projection.Value.GetBuildingCount(), 1);
		TestEqual(TEXT("Projection reports vehicles"), Projection.Value.GetVehicleCount(), 1);
		TestEqual(TEXT("Projection reports routes"), Projection.Value.GetRouteCount(), 1);
		TestTrue(TEXT("Projection correlates to the same state fingerprint"), Projection.Value.GetFingerprint() == Snapshot.GetFingerprint());
	}

	FHansaSimulationTransientCache Cache;
	const FHansaCommandGatewayResult Step = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, {}, Cache);
	TestTrue(TEXT("Live state advances after snapshot capture"), Step.IsSuccess());
	TestEqual(TEXT("Owning snapshot remains at its capture tick"), Snapshot.GetClock().GetTick().GetValue(), int64(0));
	TestEqual(TEXT("Live read-only view observes the new tick"),
		State.CreateReadOnlyAccess(Definitions).GetClock().GetTick().GetValue(), int64(1));
	TestEqual(TEXT("Snapshot contains the canonical runtime records"), Snapshot.GetHouses()[0].Id.GetValue(), uint64(1));

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaSimulationPipelineFailureTest,
	"Hansa.Simulation.Kernel.TransactionalFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaSimulationPipelineFailureTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Kernel;

	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationState State = MakeState(99);
	FHansaSimulationTransientCache Cache;
	const FHansaDeterminismFingerprint InitialFingerprint = State.CreateReadOnlyAccess(Definitions).GetFingerprint();

	TArray<FHansaGameplayCommand> WrongTickCommands = {
		MakeNoOpCommand(1, 1, MakeTick(1), 1)
	};
	const FHansaCommandGatewayResult WrongTick = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, WrongTickCommands, Cache);
	TestTrue(TEXT("Wrong scheduled tick fails explicitly"), WrongTick.GetError() == EHansaCommandGatewayError::ExecutionTickMismatch);
	TestTrue(TEXT("Wrong-tick failure leaves state unchanged"), WrongTick.GetFingerprintAfter() == InitialFingerprint);
	TestEqual(TEXT("Failed preflight does not rebuild transient caches"), Cache.GetRebuildCount(), uint64(0));

	TArray<FHansaGameplayCommand> UnorderedCommands = {
		MakeNoOpCommand(2, 2, MakeTick(0), 2),
		MakeNoOpCommand(3, 1, MakeTick(0), 1)
	};
	const FHansaCommandGatewayResult Unordered = FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, UnorderedCommands, Cache);
	TestTrue(TEXT("Unordered command metadata fails explicitly"), Unordered.GetError() == EHansaCommandGatewayError::CommandOrderInvalid);
	TestTrue(TEXT("Order failure leaves state unchanged"), Unordered.GetFingerprintAfter() == InitialFingerprint);

	FHansaSimulationState Uninitialized;
	TestTrue(TEXT("Default state cannot be stepped"),
		FHansaGameplayCommandGateway::ExecuteTick(Uninitialized, Definitions, {}, Cache).GetError() ==
			EHansaCommandGatewayError::UninitializedState);

	const FHansaSimulationDefinitionContext InvalidDefinitions;
	TestTrue(TEXT("Invalid immutable definition context cannot be stepped"),
		FHansaGameplayCommandGateway::ExecuteTick(State, InvalidDefinitions, {}, Cache).GetError() ==
			EHansaCommandGatewayError::InvalidDefinitionContext);

	FHansaSimulationInitialization MaximumTickInitialization = MakeInitialization(99);
	MaximumTickInitialization.Clock = MakeClock(MakeTick(TNumericLimits<int64>::Max()));
	FHansaSimulationState MaximumTickState = MakeInitializedState(MaximumTickInitialization);
	const FHansaDeterminismFingerprint MaximumBefore = MaximumTickState.CreateReadOnlyAccess(Definitions).GetFingerprint();
	const FHansaCommandGatewayResult ClockOverflow = FHansaGameplayCommandGateway::ExecuteTick(
		MaximumTickState, Definitions, {}, Cache);
	TestTrue(TEXT("Tick overflow fails explicitly"), ClockOverflow.GetError() == EHansaCommandGatewayError::ClockOverflow);
	TestTrue(TEXT("Tick overflow is transactional"), ClockOverflow.GetFingerprintAfter() == MaximumBefore);

	FHansaSimulationInitialization CountOverflowInitialization = MakeInitialization(99);
	CountOverflowInitialization.ProcessedCommandCount = TNumericLimits<uint64>::Max();
	CountOverflowInitialization.LastProcessedCommandSequence = 1;
	CountOverflowInitialization.LastProcessedCommandId = MakeEntityId<FHansaCommandId>(1);
	FHansaSimulationState CountOverflowState = MakeInitializedState(CountOverflowInitialization);
	TArray<FHansaGameplayCommand> OneMoreCommand = {
		MakeNoOpCommand(2, 2, MakeTick(0), 2)
	};
	TestTrue(TEXT("Command count overflow fails explicitly"),
		FHansaGameplayCommandGateway::ExecuteTick(CountOverflowState, Definitions, OneMoreCommand, Cache).GetError() ==
			EHansaCommandGatewayError::CommandCountOverflow);

	return !HasAnyErrors();
}

#endif
