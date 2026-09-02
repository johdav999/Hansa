#include "Commands/HansaGameplayCommandGateway.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Misc/AutomationTest.h"
#include "Model/HansaSimulationState.h"
#include "Systems/HansaSimulationPipeline.h"

#include <type_traits>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace Hansa::Tests::Commands
{
	using namespace Hansa::Simulation;

	using FReadOnlyEventElement = std::remove_reference_t<decltype(
		std::declval<FHansaCommandGatewayResult>().GetEvents()[0])>;
	static_assert(std::is_const_v<FReadOnlyEventElement>);

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

	FHansaSimulationState MakeState()
	{
		const THansaValueResult<FHansaSimulationVersion> Version = FHansaSimulationVersion::TryCreate(1);
		const THansaValueResult<FHansaSimulationClock> Clock = FHansaSimulationClock::TryCreate(Version.Value, MakeTick(0));
		FHansaSimulationInitialization Initialization;
		Initialization.Clock = Clock.Value;
		Initialization.CampaignSeed = 0x12345678;
		Initialization.Houses = {
			{ MakeEntityId<FHansaHouseId>(1), FHansaMoney::FromRaw(10'000) },
			{ MakeEntityId<FHansaHouseId>(2), FHansaMoney::FromRaw(20'000) }
		};
		const THansaValueResult<FHansaSimulationState> State = FHansaSimulationState::TryCreate(MoveTemp(Initialization));
		return RequireValue(State);
	}

	FHansaSimulationDefinitionContext MakeDefinitions()
	{
		const THansaValueResult<FHansaScenarioId> Scenario =
			FHansaScenarioId::TryParse(TEXT("Scenario.CommandGatewayTest"));
		const THansaValueResult<FHansaSimulationDefinitionContext> Definitions =
			FHansaSimulationDefinitionContext::TryCreate(Scenario.Value, 0x9911223344556677ULL);
		return RequireValue(Definitions);
	}

	FHansaCommandHeader MakeHeader(
		const uint64 CommandId,
		const uint64 Sequence,
		const int64 Tick,
		const uint64 HouseId = 1,
		const EHansaCommandOrigin Origin = EHansaCommandOrigin::PlayerInput)
	{
		FHansaCommandHeader Header;
		Header.CommandId = MakeEntityId<FHansaCommandId>(CommandId);
		Header.Authority.IssuingHouseId = MakeEntityId<FHansaHouseId>(HouseId);
		Header.Authority.PrincipalId = HouseId * 1000 + 7;
		Header.Authority.Origin = Origin;
		Header.RequestedExecutionTick = MakeTick(Tick);
		Header.GlobalSequence = Sequence;
		return Header;
	}

	FHansaGameplayCommand CreateEntity(
		const uint64 CommandId,
		const uint64 Sequence,
		const int64 Tick,
		const uint64 EntityId,
		const int64 Value,
		const uint64 HouseId = 1,
		const EHansaCommandOrigin Origin = EHansaCommandOrigin::PlayerInput)
	{
		return FHansaGameplayCommand::Create(
			MakeHeader(CommandId, Sequence, Tick, HouseId, Origin),
			FHansaCreateTestEntityCommand { MakeEntityId<FHansaTestEntityId>(EntityId), Value });
	}

	FHansaGameplayCommand CancelEntity(
		const uint64 CommandId,
		const uint64 Sequence,
		const int64 Tick,
		const uint64 EntityId,
		const uint64 HouseId = 1,
		const EHansaCommandOrigin Origin = EHansaCommandOrigin::PlayerInput)
	{
		return FHansaGameplayCommand::Create(
			MakeHeader(CommandId, Sequence, Tick, HouseId, Origin),
			FHansaCancelTestEntityCommand { MakeEntityId<FHansaTestEntityId>(EntityId) });
	}

	FHansaGameplayCommand NoOp(
		const uint64 CommandId,
		const uint64 Sequence,
		const int64 Tick,
		const int64 Value,
		const EHansaCommandOrigin Origin = EHansaCommandOrigin::PlayerInput)
	{
		return FHansaGameplayCommand::Create(
			MakeHeader(CommandId, Sequence, Tick, 1, Origin),
			FHansaNoOpTestCommand { Value });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaTypedCommandLifecycleTest,
	"Hansa.Simulation.Commands.TypedLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaTypedCommandLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Commands;

	FHansaSimulationState State = MakeState();
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	TArray<FHansaGameplayCommand> Commands = {
		CreateEntity(1, 1, 0, 20, 55, 1, EHansaCommandOrigin::PlayerInput),
		NoOp(2, 2, 0, 77, EHansaCommandOrigin::ArtificialIntelligence),
		CancelEntity(3, 3, 0, 20, 1, EHansaCommandOrigin::MultiplayerRpc)
	};

	const FHansaCommandGatewayResult Result =
		FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
	TestTrue(TEXT("A typed create/no-op/cancel batch succeeds"), Result.IsSuccess());
	TestEqual(TEXT("The transaction advances one tick"), Result.GetTickAfter().GetValue(), int64(1));
	TestEqual(TEXT("All commands use the same command accounting"),
		State.CreateReadOnlyAccess(Definitions).GetProcessedCommandCount(), uint64(3));
	TestEqual(TEXT("Create followed by cancel leaves no lifecycle record"),
		State.CreateReadOnlyAccess(Definitions).GetTestEntities().Num(), 0);
	TestEqual(TEXT("One immutable event is published per representative command"), Result.GetEvents().Num(), 3);
	if (Result.GetEvents().Num() == 3)
	{
		TestTrue(TEXT("Create event is first"), Result.GetEvents()[0].GetType() == EHansaDomainEventType::TestEntityCreated);
		TestTrue(TEXT("No-op event is second"), Result.GetEvents()[1].GetType() == EHansaDomainEventType::NoOpCommandAccepted);
		TestTrue(TEXT("Cancel event is third"), Result.GetEvents()[2].GetType() == EHansaDomainEventType::TestEntityCancelled);
		for (int32 Index = 0; Index < Result.GetEvents().Num(); ++Index)
		{
			TestEqual(FString::Printf(TEXT("Event %d has deterministic global order"), Index),
				Result.GetEvents()[Index].GetGlobalSequence(), static_cast<uint64>(Index + 1));
			TestEqual(FString::Printf(TEXT("Event %d correlates to its command"), Index),
				Result.GetEvents()[Index].GetSourceCommandId().GetValue(), static_cast<uint64>(Index + 1));
		}
	}
	TestEqual(TEXT("Published event sequence is authoritative"),
		State.CreateReadOnlyAccess(Definitions).GetPublishedDomainEventCount(), uint64(3));

	TArray<FHansaGameplayCommand> AutomationCommands = {
		CreateEntity(4, 4, 1, 10, 12, 1, EHansaCommandOrigin::ControlledAutomation)
	};
	const FHansaCommandGatewayResult AutomationResult =
		FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, AutomationCommands, Cache);
	TestTrue(TEXT("Controlled automation uses the same validated gateway"), AutomationResult.IsSuccess());
	const TConstArrayView<FHansaTestEntityState> TestEntities = State.CreateReadOnlyAccess(Definitions).GetTestEntities();
	TestEqual(TEXT("Controlled automation creates one lifecycle record"), TestEntities.Num(), 1);
	if (TestEntities.Num() == 1)
	{
		TestEqual(TEXT("Canonical insertion is independent of creation history"),
			TestEntities[0].Id.GetValue(), uint64(10));
	}

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCommandTransactionRollbackTest,
	"Hansa.Simulation.Commands.TransactionalRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCommandTransactionRollbackTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Commands;

	FHansaSimulationState State = MakeState();
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;
	const FHansaDeterminismFingerprint Before = State.CreateReadOnlyAccess(Definitions).GetFingerprint();
	TArray<FHansaGameplayCommand> Commands = {
		CreateEntity(1, 1, 0, 50, 99),
		CancelEntity(2, 2, 0, 50, 2)
	};

	const FHansaCommandGatewayResult Result =
		FHansaGameplayCommandGateway::ExecuteTick(State, Definitions, Commands, Cache);
	TestTrue(TEXT("A later unauthorized command rejects the complete batch"),
		Result.GetError() == EHansaCommandGatewayError::NotAuthorized);
	TestEqual(TEXT("The rejected command index is structured"), Result.GetFailedCommandIndex(), 1);
	TestEqual(TEXT("The rejected command identity is preserved"), Result.GetFailedCommandId().GetValue(), uint64(2));
	TestEqual(TEXT("Rejected transactions publish no events"), Result.GetEvents().Num(), 0);
	TestEqual(TEXT("Rejected transactions do not rebuild caches"), Cache.GetRebuildCount(), uint64(0));
	TestEqual(TEXT("Rejected transactions do not leave created records"),
		State.CreateReadOnlyAccess(Definitions).GetTestEntities().Num(), 0);
	TestTrue(TEXT("Rejected transactions preserve the complete authoritative fingerprint"),
		State.CreateReadOnlyAccess(Definitions).GetFingerprint() == Before);
	TestEqual(TEXT("Rejected transactions do not advance time"),
		State.CreateReadOnlyAccess(Definitions).GetClock().GetTick().GetValue(), int64(0));

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCommandValidationResultTest,
	"Hansa.Simulation.Commands.StructuredValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCommandValidationResultTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Commands;

	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache Cache;

	FHansaSimulationState SchemaState = MakeState();
	FHansaCommandHeader BadSchemaHeader = MakeHeader(1, 1, 0);
	BadSchemaHeader.SchemaVersion = FHansaCommandHeader::CurrentSchemaVersion + 1;
	TArray<FHansaGameplayCommand> BadSchema = {
		FHansaGameplayCommand::Create(BadSchemaHeader, FHansaNoOpTestCommand { 1 })
	};
	TestTrue(TEXT("Unsupported protocol schema has a stable cause"),
		FHansaGameplayCommandGateway::ExecuteTick(SchemaState, Definitions, BadSchema, Cache).GetError() ==
			EHansaCommandGatewayError::UnsupportedSchemaVersion);

	FHansaSimulationState AuthorityState = MakeState();
	FHansaCommandHeader BadAuthorityHeader = MakeHeader(1, 1, 0);
	BadAuthorityHeader.Authority.PrincipalId = 0;
	TArray<FHansaGameplayCommand> BadAuthority = {
		FHansaGameplayCommand::Create(BadAuthorityHeader, FHansaNoOpTestCommand { 1 })
	};
	TestTrue(TEXT("Invalid authority context has a stable cause"),
		FHansaGameplayCommandGateway::ExecuteTick(AuthorityState, Definitions, BadAuthority, Cache).GetError() ==
			EHansaCommandGatewayError::InvalidAuthorityContext);

	FHansaSimulationState OrderState = MakeState();
	TArray<FHansaGameplayCommand> BadIdentityOrder = {
		NoOp(2, 1, 0, 1),
		NoOp(1, 2, 0, 2)
	};
	TestTrue(TEXT("Command identities must be globally monotonic and unique"),
		FHansaGameplayCommandGateway::ExecuteTick(OrderState, Definitions, BadIdentityOrder, Cache).GetError() ==
			EHansaCommandGatewayError::CommandIdentityOrderInvalid);

	FHansaSimulationState PayloadState = MakeState();
	TArray<FHansaGameplayCommand> BadPayload = {
		CreateEntity(1, 1, 0, 10, -1)
	};
	TestTrue(TEXT("Invalid typed payload has a stable cause"),
		FHansaGameplayCommandGateway::ExecuteTick(PayloadState, Definitions, BadPayload, Cache).GetError() ==
			EHansaCommandGatewayError::InvalidPayload);
	TestEqual(TEXT("Stable causes have transport/localization keys"),
		FString(LexToString(EHansaCommandGatewayError::InvalidPayload)), FString(TEXT("InvalidPayload")));

	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FHansaCommandReplayTest,
	"Hansa.Simulation.Commands.ReplayAndEventOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHansaCommandReplayTest::RunTest(const FString& Parameters)
{
	using namespace Hansa::Simulation;
	using namespace Hansa::Tests::Commands;

	FHansaSimulationState First = MakeState();
	FHansaSimulationState Second = MakeState();
	FHansaSimulationState Different = MakeState();
	const FHansaSimulationDefinitionContext Definitions = MakeDefinitions();
	FHansaSimulationTransientCache FirstCache;
	FHansaSimulationTransientCache SecondCache;
	FHansaSimulationTransientCache DifferentCache;
	uint64 NextIdentity = 1;
	bool bReplayEqual = true;
	bool bEventOrderValid = true;
	bool bDifferentDiverged = false;

	for (int64 Tick = 0; Tick < 200; ++Tick)
	{
		TArray<FHansaGameplayCommand> Commands;
		TArray<FHansaGameplayCommand> DifferentCommands;
		const uint64 CommandId = NextIdentity++;
		Commands.Add(NoOp(CommandId, CommandId, Tick, Tick * 3));
		DifferentCommands.Add(NoOp(CommandId, CommandId, Tick, Tick == 100 ? Tick * 3 + 1 : Tick * 3));

		const FHansaCommandGatewayResult FirstResult =
			FHansaGameplayCommandGateway::ExecuteTick(First, Definitions, Commands, FirstCache);
		const FHansaCommandGatewayResult SecondResult =
			FHansaGameplayCommandGateway::ExecuteTick(Second, Definitions, Commands, SecondCache);
		const FHansaCommandGatewayResult DifferentResult =
			FHansaGameplayCommandGateway::ExecuteTick(Different, Definitions, DifferentCommands, DifferentCache);
		bReplayEqual &= FirstResult && SecondResult &&
			FirstResult.GetFingerprintAfter() == SecondResult.GetFingerprintAfter();
		bDifferentDiverged |= DifferentResult.GetFingerprintAfter() != FirstResult.GetFingerprintAfter();
		bEventOrderValid &= FirstResult.GetEvents().Num() == 1 &&
			FirstResult.GetEvents()[0].GetGlobalSequence() == static_cast<uint64>(Tick + 1) &&
			FirstResult.GetEvents()[0].GetTick().GetValue() == Tick;
	}

	TestTrue(TEXT("Equal typed command streams replay to equal fingerprints"), bReplayEqual);
	TestTrue(TEXT("A one-field payload difference causes replay divergence"), bDifferentDiverged);
	TestTrue(TEXT("Event order remains globally monotonic and tick-correlated"), bEventOrderValid);
	TestEqual(TEXT("Replay processes the expected command count"),
		First.CreateReadOnlyAccess(Definitions).GetProcessedCommandCount(), uint64(200));

	return !HasAnyErrors();
}

#endif
