#include "Systems/HansaSimulationPipeline.h"

#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	namespace
	{
		constexpr EHansaSimulationPhase OrderedPhases[] = {
			EHansaSimulationPhase::ApplyCommands,
			EHansaSimulationPhase::CalendarAndWorldEvents,
			EHansaSimulationPhase::VehicleMovementAndTransfers,
			EHansaSimulationPhase::WarehousesAndStorage,
			EHansaSimulationPhase::ConstructionAndProduction,
			EHansaSimulationPhase::WorkforceAndNeeds,
			EHansaSimulationPhase::MarketClearing,
			EHansaSimulationPhase::PricesAndHistory,
			EHansaSimulationPhase::FinanceAndContracts,
			EHansaSimulationPhase::ResearchPoliticsAndVictory,
			EHansaSimulationPhase::PublishAndChecksum
		};

		constexpr uint64 FnvPrime = 1099511628211ULL;

		void AddHistoryByte(uint64& Hash, const uint8 Value)
		{
			Hash ^= Value;
			Hash *= FnvPrime;
		}

		void AddHistoryUInt64(uint64& Hash, const uint64 Value)
		{
			for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
			{
				AddHistoryByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		bool IsKnownOrigin(const EHansaCommandOrigin Origin)
		{
			switch (Origin)
			{
			case EHansaCommandOrigin::PlayerInput:
			case EHansaCommandOrigin::ArtificialIntelligence:
			case EHansaCommandOrigin::MultiplayerRpc:
			case EHansaCommandOrigin::ControlledAutomation:
				return true;
			default:
				return false;
			}
		}

		bool ContainsHouse(const TArray<FHansaHouseState>& Houses, const FHansaHouseId HouseId)
		{
			for (const FHansaHouseState& House : Houses)
			{
				if (House.Id == HouseId)
				{
					return true;
				}
			}
			return false;
		}

		int32 FindTestEntityIndex(const TArray<FHansaTestEntityState>& Entities, const FHansaTestEntityId EntityId)
		{
			for (int32 Index = 0; Index < Entities.Num(); ++Index)
			{
				if (Entities[Index].Id == EntityId)
				{
					return Index;
				}
				if (EntityId < Entities[Index].Id)
				{
					break;
				}
			}
			return INDEX_NONE;
		}

		int32 FindTestEntityInsertionIndex(const TArray<FHansaTestEntityState>& Entities, const FHansaTestEntityId EntityId)
		{
			int32 Index = 0;
			while (Index < Entities.Num() && Entities[Index].Id < EntityId)
			{
				++Index;
			}
			return Index;
		}

		void ExecuteRepresentativeNoOpSystem(const EHansaSimulationPhase Phase)
		{
			switch (Phase)
			{
			case EHansaSimulationPhase::VehicleMovementAndTransfers:
			case EHansaSimulationPhase::WarehousesAndStorage:
			case EHansaSimulationPhase::ConstructionAndProduction:
			case EHansaSimulationPhase::WorkforceAndNeeds:
			case EHansaSimulationPhase::MarketClearing:
			case EHansaSimulationPhase::PricesAndHistory:
			case EHansaSimulationPhase::FinanceAndContracts:
			case EHansaSimulationPhase::ResearchPoliticsAndVictory:
			case EHansaSimulationPhase::PublishAndChecksum:
			case EHansaSimulationPhase::ApplyCommands:
			case EHansaSimulationPhase::CalendarAndWorldEvents:
			default:
				return;
			}
		}
	}

	const TCHAR* LexToString(const EHansaSimulationPhase Phase)
	{
		switch (Phase)
		{
		case EHansaSimulationPhase::ApplyCommands: return TEXT("ApplyCommands");
		case EHansaSimulationPhase::CalendarAndWorldEvents: return TEXT("CalendarAndWorldEvents");
		case EHansaSimulationPhase::VehicleMovementAndTransfers: return TEXT("VehicleMovementAndTransfers");
		case EHansaSimulationPhase::WarehousesAndStorage: return TEXT("WarehousesAndStorage");
		case EHansaSimulationPhase::ConstructionAndProduction: return TEXT("ConstructionAndProduction");
		case EHansaSimulationPhase::WorkforceAndNeeds: return TEXT("WorkforceAndNeeds");
		case EHansaSimulationPhase::MarketClearing: return TEXT("MarketClearing");
		case EHansaSimulationPhase::PricesAndHistory: return TEXT("PricesAndHistory");
		case EHansaSimulationPhase::FinanceAndContracts: return TEXT("FinanceAndContracts");
		case EHansaSimulationPhase::ResearchPoliticsAndVictory: return TEXT("ResearchPoliticsAndVictory");
		case EHansaSimulationPhase::PublishAndChecksum: return TEXT("PublishAndChecksum");
		default: return TEXT("UnknownPhase");
		}
	}

	void FHansaSimulationTransientCache::Discard()
	{
		PreparedForTick = FHansaSimulationTick();
		CachedEntityCount = 0;
		LastPhaseOrder.Reset();
	}

	void FHansaSimulationTransientCache::BeginStep(const FHansaSimulationTick Tick, const int64 EntityCount)
	{
		PreparedForTick = Tick;
		CachedEntityCount = EntityCount;
		LastPhaseOrder.Reset(UE_ARRAY_COUNT(OrderedPhases));
		++RebuildCount;
	}

	void FHansaSimulationTransientCache::RecordPhase(const EHansaSimulationPhase Phase)
	{
		LastPhaseOrder.Add(Phase);
	}

	TConstArrayView<EHansaSimulationPhase> FHansaSimulationPipeline::GetOrderedPhases()
	{
		return MakeArrayView(OrderedPhases);
	}

	FHansaCommandGatewayResult FHansaSimulationPipeline::AdvanceOneTick(
		FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions,
		const FHansaSimulationStepInput& Input,
		FHansaSimulationTransientCache& TransientCache)
	{
		const auto MakeFailure = [&State, &Definitions, &Input](
			const EHansaCommandGatewayError Error,
			const int32 FailedCommandIndex = INDEX_NONE)
		{
			FHansaCommandGatewayResult Result;
			Result.Error = Error;
			Result.FailedCommandIndex = FailedCommandIndex;
			if (Input.Commands.IsValidIndex(FailedCommandIndex))
			{
				Result.FailedCommandId = Input.Commands[FailedCommandIndex].GetHeader().CommandId;
			}
			if (State.bInitialized)
			{
				Result.TickBefore = State.Clock.GetTick();
				Result.TickAfter = State.Clock.GetTick();
				if (Definitions.IsValid())
				{
					Result.FingerprintAfter.Value = State.ComputeDeterminismFingerprint(Definitions);
				}
			}
			return Result;
		};

		if (!State.bInitialized)
		{
			return MakeFailure(EHansaCommandGatewayError::UninitializedState);
		}
		if (!Definitions.IsValid())
		{
			return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext);
		}

		const FHansaSimulationTick TickBefore = State.Clock.GetTick();
		const THansaValueResult<FHansaSimulationDuration> OneTick = FHansaSimulationDuration::TryCreate(1);
		const THansaValueResult<FHansaSimulationClock> ClockAfter = State.Clock.TryAdvance(OneTick.Value);
		if (!ClockAfter)
		{
			return MakeFailure(EHansaCommandGatewayError::ClockOverflow);
		}

		const uint64 CommandCount = static_cast<uint64>(Input.Commands.Num());
		if (CommandCount > TNumericLimits<uint64>::Max() - State.ProcessedCommandCount)
		{
			return MakeFailure(EHansaCommandGatewayError::CommandCountOverflow);
		}
		if (CommandCount > TNumericLimits<uint64>::Max() - State.PublishedDomainEventCount)
		{
			return MakeFailure(EHansaCommandGatewayError::EventCountOverflow);
		}

		uint64 PreviousSequence = State.LastProcessedCommandSequence;
		FHansaCommandId PreviousCommandId = State.LastProcessedCommandId;
		for (int32 CommandIndex = 0; CommandIndex < Input.Commands.Num(); ++CommandIndex)
		{
			const FHansaCommandHeader& Header = Input.Commands[CommandIndex].GetHeader();
			if (Header.SchemaVersion != FHansaCommandHeader::CurrentSchemaVersion)
			{
				return MakeFailure(EHansaCommandGatewayError::UnsupportedSchemaVersion, CommandIndex);
			}
			if (!Header.CommandId.IsValid())
			{
				return MakeFailure(EHansaCommandGatewayError::InvalidCommandIdentity, CommandIndex);
			}
			if (!Header.Authority.IssuingHouseId.IsValid() || Header.Authority.PrincipalId == 0 ||
				!IsKnownOrigin(Header.Authority.Origin))
			{
				return MakeFailure(EHansaCommandGatewayError::InvalidAuthorityContext, CommandIndex);
			}
			if (!ContainsHouse(State.Houses, Header.Authority.IssuingHouseId))
			{
				return MakeFailure(EHansaCommandGatewayError::UnknownIssuingHouse, CommandIndex);
			}
			if (Header.RequestedExecutionTick != TickBefore)
			{
				return MakeFailure(EHansaCommandGatewayError::ExecutionTickMismatch, CommandIndex);
			}
			if (Header.GlobalSequence == 0 || Header.GlobalSequence <= PreviousSequence)
			{
				return MakeFailure(EHansaCommandGatewayError::CommandOrderInvalid, CommandIndex);
			}
			if (PreviousCommandId.IsValid() && !(PreviousCommandId < Header.CommandId))
			{
				return MakeFailure(EHansaCommandGatewayError::CommandIdentityOrderInvalid, CommandIndex);
			}
			PreviousSequence = Header.GlobalSequence;
			PreviousCommandId = Header.CommandId;
		}

		FHansaSimulationState Candidate = State;
		TArray<FHansaDomainEvent> PendingEvents;
		PendingEvents.Reserve(Input.Commands.Num());
		for (int32 CommandIndex = 0; CommandIndex < Input.Commands.Num(); ++CommandIndex)
		{
			const FHansaGameplayCommand& Command = Input.Commands[CommandIndex];
			const FHansaCommandHeader& Header = Command.GetHeader();
			FHansaDomainEvent Event;
			Event.GlobalSequence = Candidate.PublishedDomainEventCount + 1;
			Event.Tick = TickBefore;
			Event.SourceCommandId = Header.CommandId;
			Event.IssuingHouseId = Header.Authority.IssuingHouseId;

			switch (Command.GetType())
			{
			case EHansaGameplayCommandType::CreateTestEntity:
			{
				const FHansaCreateTestEntityCommand& Payload = Command.GetCreateTestEntity();
				if (!Payload.EntityId.IsValid() || Payload.InitialValue < 0)
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				if (FindTestEntityIndex(Candidate.TestEntities, Payload.EntityId) != INDEX_NONE)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetAlreadyExists, CommandIndex);
				}
				FHansaTestEntityState Entity;
				Entity.Id = Payload.EntityId;
				Entity.OwnerId = Header.Authority.IssuingHouseId;
				Entity.Value = Payload.InitialValue;
				Candidate.TestEntities.Insert(Entity, FindTestEntityInsertionIndex(Candidate.TestEntities, Payload.EntityId));
				Event.Type = EHansaDomainEventType::TestEntityCreated;
				Event.TestEntityId = Payload.EntityId;
				Event.Value = Payload.InitialValue;
				break;
			}
			case EHansaGameplayCommandType::CancelTestEntity:
			{
				const FHansaCancelTestEntityCommand& Payload = Command.GetCancelTestEntity();
				if (!Payload.EntityId.IsValid())
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				const int32 EntityIndex = FindTestEntityIndex(Candidate.TestEntities, Payload.EntityId);
				if (EntityIndex == INDEX_NONE)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
				}
				const FHansaTestEntityState& Entity = Candidate.TestEntities[EntityIndex];
				if (Entity.OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				Event.Type = EHansaDomainEventType::TestEntityCancelled;
				Event.TestEntityId = Payload.EntityId;
				Event.Value = Entity.Value;
				Candidate.TestEntities.RemoveAt(EntityIndex);
				break;
			}
			case EHansaGameplayCommandType::NoOpTest:
				Event.Type = EHansaDomainEventType::NoOpCommandAccepted;
				Event.Value = Command.GetNoOpTest().CorrelationValue;
				break;
			default:
				return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
			}

			AddHistoryUInt64(Candidate.CommandHistoryFingerprint, static_cast<uint64>(Header.RequestedExecutionTick.GetValue()));
			AddHistoryUInt64(Candidate.CommandHistoryFingerprint, Header.GlobalSequence);
			AddHistoryUInt64(Candidate.CommandHistoryFingerprint, Command.ComputeStableFingerprint());
			++Candidate.ProcessedCommandCount;
			Candidate.LastProcessedCommandSequence = Header.GlobalSequence;
			Candidate.LastProcessedCommandId = Header.CommandId;
			++Candidate.PublishedDomainEventCount;
			PendingEvents.Add(Event);
		}

		const int64 EntityCount = static_cast<int64>(Candidate.Houses.Num()) + Candidate.Cities.Num() +
			Candidate.Buildings.Num() + Candidate.Vehicles.Num() + Candidate.Routes.Num() + Candidate.TestEntities.Num();
		TransientCache.BeginStep(TickBefore, EntityCount);
		for (const EHansaSimulationPhase Phase : OrderedPhases)
		{
			TransientCache.RecordPhase(Phase);
			if (Phase == EHansaSimulationPhase::CalendarAndWorldEvents)
			{
				Candidate.Clock = ClockAfter.Value;
			}
			else if (Phase != EHansaSimulationPhase::ApplyCommands)
			{
				ExecuteRepresentativeNoOpSystem(Phase);
			}
		}

		State = MoveTemp(Candidate);
		FHansaCommandGatewayResult Result;
		Result.TickBefore = TickBefore;
		Result.TickAfter = State.Clock.GetTick();
		Result.FingerprintAfter.Value = State.ComputeDeterminismFingerprint(Definitions);
		Result.Events = MoveTemp(PendingEvents);
		return Result;
	}
}
