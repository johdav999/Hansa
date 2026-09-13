#include "Systems/HansaSimulationPipeline.h"

#include "Construction/HansaConstructionInternal.h"
#include "Logistics/HansaLocalLogisticsInternal.h"
#include "Math/NumericLimits.h"
#include "Market/HansaMarketInternal.h"
#include "Production/HansaProductionInternal.h"
#include "Population/HansaPopulationInternal.h"
#include "Trade/HansaTradeInternal.h"

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

		constexpr uint64 SimulationPipelineFnvPrime = 1099511628211ULL;

		constexpr uint32 StateHashBit(const EHansaStateHashSubsystem Subsystem)
		{
			return 1U << static_cast<uint8>(Subsystem);
		}

		void AddHistoryByte(uint64& Hash, const uint8 Value)
		{
			Hash ^= Value;
			Hash *= SimulationPipelineFnvPrime;
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

		bool SimulationPipelineContainsHouse(const TArray<FHansaHouseState>& Houses, const FHansaHouseId HouseId)
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

		int32 FindProductionIndex(const TArray<FHansaProductionState>& Productions, const FHansaProductionId ProductionId)
		{
			for (int32 Index = 0; Index < Productions.Num(); ++Index)
			{
				if (Productions[Index].Id == ProductionId)
				{
					return Index;
				}
				if (ProductionId < Productions[Index].Id)
				{
					break;
				}
			}
			return INDEX_NONE;
		}

		int32 FindRouteIndex(const TArray<FHansaRouteState>& Routes, const FHansaRouteId RouteId)
		{
			for (int32 Index = 0; Index < Routes.Num(); ++Index)
			{
				if (Routes[Index].Id == RouteId) return Index;
				if (RouteId < Routes[Index].Id) break;
			}
			return INDEX_NONE;
		}

		int32 FindRouteInsertionIndex(const TArray<FHansaRouteState>& Routes, const FHansaRouteId RouteId)
		{
			int32 Index = 0;
			while (Index < Routes.Num() && Routes[Index].Id < RouteId) ++Index;
			return Index;
		}

		FHansaVehicleState* FindVehicle(TArray<FHansaVehicleState>& Vehicles, const FHansaVehicleId VehicleId)
		{
			return Vehicles.FindByPredicate([VehicleId](const FHansaVehicleState& Vehicle)
			{
				return Vehicle.Id == VehicleId;
			});
		}

		const FHansaBuildingState* SimulationPipelineFindBuilding(const TArray<FHansaBuildingState>& Buildings, const FHansaBuildingId BuildingId)
		{
			return Buildings.FindByPredicate([BuildingId](const FHansaBuildingState& Building)
			{
				return Building.Id == BuildingId;
			});
		}

		int32 FindBuildingInsertionIndex(const TArray<FHansaBuildingState>& Buildings, const FHansaBuildingId BuildingId)
		{
			int32 Index = 0;
			while (Index < Buildings.Num() && Buildings[Index].Id < BuildingId)
			{
				++Index;
			}
			return Index;
		}

		int32 FindBuildingIndex(const TArray<FHansaBuildingState>& Buildings, const FHansaBuildingId BuildingId)
		{
			for (int32 Index = 0; Index < Buildings.Num(); ++Index)
			{
				if (Buildings[Index].Id == BuildingId)
				{
					return Index;
				}
				if (BuildingId < Buildings[Index].Id)
				{
					break;
				}
			}
			return INDEX_NONE;
		}

		bool HasBuildingDependents(
			const TArray<FHansaProductionState>& Productions,
			const TArray<FHansaPopulationCohortState>& PopulationCohorts,
			const FHansaInventoryLedger& InventoryLedger,
			const FHansaBuildingId BuildingId)
		{
			if (Productions.ContainsByPredicate([BuildingId](const FHansaProductionState& Production)
				{ return Production.BuildingId == BuildingId; }) ||
				PopulationCohorts.ContainsByPredicate([BuildingId](const FHansaPopulationCohortState& Cohort)
				{ return Cohort.ResidenceBuildingId == BuildingId; }))
			{
				return true;
			}
			for (const FHansaInventoryProjection& Inventory : InventoryLedger.CreateReadOnlyAccess().BuildProjection())
			{
				if (Inventory.BuildingId == BuildingId)
				{
					return true;
				}
			}
			return false;
		}

		bool HasCargoObligations(
			const TArray<FHansaLogisticsJobState>& Jobs,
			const FHansaInventoryLedger& InventoryLedger,
			const FHansaBuildingId BuildingId)
		{
			const FHansaInventoryReadOnlyAccess Inventories = InventoryLedger.CreateReadOnlyAccess();
			for (const FHansaLogisticsJobState& Job : Jobs)
			{
				if (Job.Status == EHansaLogisticsJobStatus::Completed)
				{
					continue;
				}
				if (Job.SelectedMarketBuildingId == BuildingId)
				{
					return true;
				}
				const TOptional<FHansaInventoryProjection> Source =
					Inventories.QueryInventory(Job.SourceInventoryId);
				const TOptional<FHansaInventoryProjection> Destination =
					Inventories.QueryInventory(Job.DestinationInventoryId);
				if ((Source.IsSet() && Source->BuildingId == BuildingId) ||
					(Destination.IsSet() && Destination->BuildingId == BuildingId))
				{
					return true;
				}
			}
			return false;
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
		const uint64 MaximumSystemEventCount = static_cast<uint64>(State.Productions.Num()) +
			static_cast<uint64>(State.Research.Num()) +
			static_cast<uint64>(State.Buildings.Num() + Input.Commands.Num()) * 2ULL +
			static_cast<uint64>(State.Routes.Num() + Input.Commands.Num()) * 64ULL;
		if (CommandCount > TNumericLimits<uint64>::Max() - MaximumSystemEventCount ||
			CommandCount + MaximumSystemEventCount > TNumericLimits<uint64>::Max() - State.PublishedDomainEventCount)
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
			if (!SimulationPipelineContainsHouse(State.Houses, Header.Authority.IssuingHouseId))
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
			case EHansaGameplayCommandType::SetProductionActive:
			{
				const FHansaSetProductionActiveCommand& Payload = Command.GetSetProductionActive();
				if (!Payload.ProductionId.IsValid())
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				const int32 ProductionIndex = FindProductionIndex(Candidate.Productions, Payload.ProductionId);
				if (ProductionIndex == INDEX_NONE)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
				}
				FHansaProductionState& Production = Candidate.Productions[ProductionIndex];
				if (Production.Kind == EHansaProductionKind::BackgroundSupply)
				{
					if (Header.Authority.Origin != EHansaCommandOrigin::ControlledAutomation)
					{
						return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
					}
				}
				else
				{
					const FHansaBuildingState* Building = SimulationPipelineFindBuilding(Candidate.Buildings, Production.BuildingId);
					if (Building == nullptr || Building->OwnerId != Header.Authority.IssuingHouseId)
					{
						return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
					}
				}
				Production.bActive = Payload.bActive;
				Event.Type = EHansaDomainEventType::ProductionActiveChanged;
				Event.ProductionId = Production.Id;
				Event.BuildingId = Production.BuildingId;
				Event.RecipeId = Production.RecipeId;
				Event.Value = Payload.bActive ? 1 : 0;
				break;
			}
			case EHansaGameplayCommandType::PlaceBuilding:
			{
				const FHansaPlaceBuildingCommand& Payload = Command.GetPlaceBuilding();
				if (!Payload.BuildingId.IsValid() || !Payload.Placement.CityId.IsValid() ||
					!Payload.Placement.BuildingDefinitionId.IsValid())
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				if (SimulationPipelineFindBuilding(Candidate.Buildings, Payload.BuildingId) != nullptr ||
					Candidate.Placement.FindPlacement(Payload.BuildingId) != nullptr)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetAlreadyExists, CommandIndex);
				}
				const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
				if (Registry == nullptr)
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext, CommandIndex);
				}
				const FHansaPlacementValidationResult Validation = FHansaPlacementRules::Validate(
					Candidate.Placement, *Registry, Header.Authority.IssuingHouseId, Payload.Placement);
				if (!Validation)
				{
					FHansaCommandGatewayResult Failure = MakeFailure(
						EHansaCommandGatewayError::PlacementRejected, CommandIndex);
					Failure.PlacementValidation = Validation;
					return Failure;
				}
				const FHansaConstructionCostProjection Cost = FHansaConstructionExecutor::BuildCostProjection(
					Candidate.Houses, Candidate.InventoryLedger, *Registry,
					Header.Authority.IssuingHouseId, Payload.Placement.CityId,
					Payload.Placement.BuildingDefinitionId);
				if (!Cost.IsAffordable() || !FHansaConstructionExecutor::TryPayCost(
					Candidate.Houses, Candidate.InventoryLedger, *Registry,
					Header.Authority.IssuingHouseId, Payload.Placement.CityId,
					Payload.Placement.BuildingDefinitionId, TickBefore))
				{
					FHansaCommandGatewayResult Failure = MakeFailure(
						EHansaCommandGatewayError::ConstructionCostUnavailable, CommandIndex);
					Failure.ConstructionCost = Cost;
					return Failure;
				}

				FHansaBuildingState Building;
				Building.Id = Payload.BuildingId;
				Building.DefinitionId = Payload.Placement.BuildingDefinitionId;
				Building.OwnerId = Header.Authority.IssuingHouseId;
				Building.ConstructionProgress = FHansaRate();
				Building.ConstructionState = EHansaConstructionState::UnderConstruction;
				Building.ConstructionStartedTick = ClockAfter.Value.GetTick();
				Building.ConstructionElapsedTicks = 0;
				Candidate.Buildings.Insert(
					Building,
					FindBuildingInsertionIndex(Candidate.Buildings, Payload.BuildingId));
				FHansaPlacementRules::ApplyValidated(
					Candidate.Placement,
					Payload.BuildingId,
					Header.Authority.IssuingHouseId,
					Payload.Placement,
					Validation.GetOccupiedCells());
				Event.Type = EHansaDomainEventType::BuildingPlaced;
				Event.BuildingId = Payload.BuildingId;
				Event.Placement = Payload.Placement;
				break;
			}
			case EHansaGameplayCommandType::CancelConstruction:
			{
				const FHansaCancelConstructionCommand& Payload = Command.GetCancelConstruction();
				if (!Payload.BuildingId.IsValid())
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				const int32 BuildingIndex = FindBuildingIndex(Candidate.Buildings, Payload.BuildingId);
				if (BuildingIndex == INDEX_NONE)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
				}
				const FHansaBuildingState& Building = Candidate.Buildings[BuildingIndex];
				if (Building.OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				if (Building.ConstructionState != EHansaConstructionState::UnderConstruction)
				{
					return MakeFailure(EHansaCommandGatewayError::ConstructionStateInvalid, CommandIndex);
				}
				if (HasBuildingDependents(Candidate.Productions, Candidate.PopulationCohorts,
					Candidate.InventoryLedger, Payload.BuildingId))
				{
					return MakeFailure(EHansaCommandGatewayError::TargetHasDependents, CommandIndex);
				}
				const FHansaPlacedBuildingRecord* Placement = Candidate.Placement.FindPlacement(Payload.BuildingId);
				const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
				if (Placement == nullptr || Registry == nullptr)
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext, CommandIndex);
				}
				FHansaMoney CurrencyRefund;
				if (!FHansaConstructionExecutor::TryRefundCancellation(
					Candidate.Houses, Candidate.InventoryLedger, *Registry, Building,
					Placement->Spec.CityId, TickBefore, CurrencyRefund))
				{
					return MakeFailure(EHansaCommandGatewayError::ConstructionRefundUnavailable, CommandIndex);
				}
				FHansaPlacementRules::Remove(Candidate.Placement, Payload.BuildingId);
				Candidate.Buildings.RemoveAt(BuildingIndex);
				Event.Type = EHansaDomainEventType::ConstructionCancelled;
				Event.BuildingId = Payload.BuildingId;
				Event.Value = CurrencyRefund.GetRawValue();
				break;
			}
			case EHansaGameplayCommandType::RemoveBuilding:
			{
				const FHansaRemoveBuildingCommand& Payload = Command.GetRemoveBuilding();
				if (!Payload.BuildingId.IsValid())
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				const int32 BuildingIndex = FindBuildingIndex(Candidate.Buildings, Payload.BuildingId);
				if (BuildingIndex == INDEX_NONE)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
				}
				const FHansaBuildingState& Building = Candidate.Buildings[BuildingIndex];
				if (Building.OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				if (Building.ConstructionState != EHansaConstructionState::Completed)
				{
					return MakeFailure(EHansaCommandGatewayError::ConstructionStateInvalid, CommandIndex);
				}
				if (HasCargoObligations(Candidate.LocalLogisticsJobs, Candidate.InventoryLedger,
					Payload.BuildingId))
				{
					return MakeFailure(EHansaCommandGatewayError::TargetHasCargoObligations, CommandIndex);
				}
				if (HasBuildingDependents(Candidate.Productions, Candidate.PopulationCohorts,
					Candidate.InventoryLedger, Payload.BuildingId))
				{
					return MakeFailure(EHansaCommandGatewayError::TargetHasDependents, CommandIndex);
				}
				if (!FHansaPlacementRules::Remove(Candidate.Placement, Payload.BuildingId))
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				Candidate.Buildings.RemoveAt(BuildingIndex);
				Event.Type = EHansaDomainEventType::BuildingRemoved;
				Event.BuildingId = Payload.BuildingId;
				break;
			}
			case EHansaGameplayCommandType::UpgradeResidence:
			{
				const FHansaUpgradeResidenceCommand& Payload = Command.GetUpgradeResidence();
				if (!Payload.BuildingId.IsValid())
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				const int32 BuildingIndex = FindBuildingIndex(Candidate.Buildings, Payload.BuildingId);
				if (BuildingIndex == INDEX_NONE)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
				}
				FHansaBuildingState& Building = Candidate.Buildings[BuildingIndex];
				if (Building.OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				if (Building.ConstructionState != EHansaConstructionState::Completed)
				{
					return MakeFailure(EHansaCommandGatewayError::ConstructionStateInvalid, CommandIndex);
				}
				FHansaPopulationCohortState* Cohort = Candidate.PopulationCohorts.FindByPredicate(
					[&Payload](const FHansaPopulationCohortState& Value)
					{ return Value.ResidenceBuildingId == Payload.BuildingId; });
				const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
				const FHansaCompiledBuildingDefinition* Source = Registry != nullptr
					? Registry->FindBuilding(Building.DefinitionId.ToString()) : nullptr;
				const FHansaCompiledBuildingDefinition* Target = Source != nullptr && !Source->UpgradeTargetBuildingId.IsEmpty()
					? Registry->FindBuilding(Source->UpgradeTargetBuildingId) : nullptr;
				const FHansaCompiledPopulationTierDefinition* SourceTier = Source != nullptr
					? Registry->FindPopulationTier(Source->ResidentPopulationTierId) : nullptr;
				const FHansaCompiledPopulationTierDefinition* TargetTier = Target != nullptr
					? Registry->FindPopulationTier(Target->ResidentPopulationTierId) : nullptr;
				const auto TargetBuildingId = Target != nullptr
					? FHansaBuildingTypeId::TryParse(Target->StableId)
					: THansaValueResult<FHansaBuildingTypeId>::Failure(EHansaValueError::InvalidFormat);
				const auto TargetTierId = TargetTier != nullptr
					? FHansaPopulationTierId::TryParse(TargetTier->StableId)
					: THansaValueResult<FHansaPopulationTierId>::Failure(EHansaValueError::InvalidFormat);
				if (Cohort == nullptr || Source == nullptr || Target == nullptr || SourceTier == nullptr ||
					TargetTier == nullptr || !TargetBuildingId || !TargetTierId ||
					TargetTier->PreviousTierId != SourceTier->StableId ||
					Cohort->SatisfactionBasisPoints < SourceTier->GrowthSatisfactionBasisPoints ||
					Cohort->Residents > Target->ResidenceCapacity)
				{
					return MakeFailure(EHansaCommandGatewayError::ResidenceProgressionUnavailable, CommandIndex);
				}
				const FHansaPlacedBuildingRecord* Placement = Candidate.Placement.FindPlacement(Payload.BuildingId);
				if (Placement == nullptr)
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext, CommandIndex);
				}
				const FHansaConstructionCostProjection Cost = FHansaConstructionExecutor::BuildCostProjection(
					Candidate.Houses, Candidate.InventoryLedger, *Registry,
					Header.Authority.IssuingHouseId, Placement->Spec.CityId, TargetBuildingId.Value);
				if (!Cost.IsAffordable() || !FHansaConstructionExecutor::TryPayCost(
					Candidate.Houses, Candidate.InventoryLedger, *Registry,
					Header.Authority.IssuingHouseId, Placement->Spec.CityId, TargetBuildingId.Value, TickBefore))
				{
					FHansaCommandGatewayResult Failure = MakeFailure(
						EHansaCommandGatewayError::ConstructionCostUnavailable, CommandIndex);
					Failure.ConstructionCost = Cost;
					return Failure;
				}
				Building.DefinitionId = TargetBuildingId.Value;
				Cohort->TierId = TargetTierId.Value;
				Cohort->ResidenceCapacity = Target->ResidenceCapacity;
				Cohort->ConsecutiveGrowthTicks = 0;
				Cohort->ConsecutiveDeclineTicks = 0;
				for (FHansaPlacedBuildingRecord& PlacementRecord : Candidate.Placement.Placements)
				{
					if (PlacementRecord.BuildingId == Payload.BuildingId)
					{
						PlacementRecord.Spec.BuildingDefinitionId = TargetBuildingId.Value;
						break;
					}
				}
				Event.Type = EHansaDomainEventType::ResidenceUpgraded;
				Event.BuildingId = Payload.BuildingId;
				Event.Value = Cohort->Residents;
				break;
			}
			case EHansaGameplayCommandType::CreateRoute:
			{
				const FHansaCreateRouteCommand& Payload = Command.GetCreateRoute();
				if (!Payload.RouteId.IsValid() || !Payload.VehicleId.IsValid() || !Payload.RouteDefinitionId.IsValid())
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				if (FindRouteIndex(Candidate.Routes, Payload.RouteId) != INDEX_NONE)
				{
					return MakeFailure(EHansaCommandGatewayError::TargetAlreadyExists, CommandIndex);
				}
				FHansaVehicleState* Vehicle = FindVehicle(Candidate.Vehicles, Payload.VehicleId);
				if (Vehicle == nullptr)
				{
					FHansaCommandGatewayResult Failure = MakeFailure(EHansaCommandGatewayError::RouteRejected, CommandIndex);
					Failure.RoutePlanError = EHansaRoutePlanError::VehicleNotFound;
					return Failure;
				}
				if (Vehicle->OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				if (Candidate.Routes.ContainsByPredicate([&Payload](const FHansaRouteState& Route)
				{
					return Route.VehicleId == Payload.VehicleId && Route.Lifecycle != EHansaRouteLifecycleState::Cancelled;
				}))
				{
					return MakeFailure(EHansaCommandGatewayError::VehicleAlreadyAssigned, CommandIndex);
				}
				const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
				if (Registry == nullptr)
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext, CommandIndex);
				}
				const EHansaRoutePlanError PlanError = FHansaTradeExecutor::ValidatePlan(
					*Vehicle, Payload.RouteDefinitionId, Payload.Stops, Candidate.Cities,
					Candidate.InventoryLedger, *Registry);
				if (PlanError != EHansaRoutePlanError::None || Payload.Stops.IsEmpty() ||
					Vehicle->CurrentCityId != Payload.Stops[0].CityId)
				{
					FHansaCommandGatewayResult Failure = MakeFailure(EHansaCommandGatewayError::RouteRejected, CommandIndex);
					Failure.RoutePlanError = PlanError != EHansaRoutePlanError::None
						? PlanError : EHansaRoutePlanError::VehicleLocationMismatch;
					return Failure;
				}
				FHansaRouteState Route;
				Route.Id = Payload.RouteId;
				Route.OwnerId = Header.Authority.IssuingHouseId;
				Route.VehicleId = Vehicle->Id;
				Route.RouteDefinitionId = Payload.RouteDefinitionId;
				Route.Mode = Vehicle->Mode;
				Route.Stops = Payload.Stops;
				Route.Lifecycle = Payload.bActivate ? EHansaRouteLifecycleState::AtStop : EHansaRouteLifecycleState::Inactive;
				Route.bPendingStopActions = Payload.bActivate;
				Candidate.Routes.Insert(Route, FindRouteInsertionIndex(Candidate.Routes, Route.Id));
				Event.Type = EHansaDomainEventType::RouteCreated;
				Event.RouteId = Route.Id;
				Event.VehicleId = Route.VehicleId;
				Event.CityId = Vehicle->CurrentCityId;
				Event.Value = Payload.bActivate ? 1 : 0;
				break;
			}
			case EHansaGameplayCommandType::EditRoute:
			{
				const FHansaEditRouteCommand& Payload = Command.GetEditRoute();
				const int32 RouteIndex = Payload.RouteId.IsValid()
					? FindRouteIndex(Candidate.Routes, Payload.RouteId) : INDEX_NONE;
				if (RouteIndex == INDEX_NONE)
				{
					return MakeFailure(Payload.RouteId.IsValid() ? EHansaCommandGatewayError::TargetNotFound
						: EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				FHansaRouteState& Route = Candidate.Routes[RouteIndex];
				if (Route.OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				FHansaVehicleState* Vehicle = FindVehicle(Candidate.Vehicles, Route.VehicleId);
				if (Route.Lifecycle != EHansaRouteLifecycleState::Inactive || Vehicle == nullptr ||
					Vehicle->Cargo.GetRawValue() != 0)
				{
					return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid, CommandIndex);
				}
				const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
				if (Registry == nullptr) return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext, CommandIndex);
				const EHansaRoutePlanError PlanError = FHansaTradeExecutor::ValidatePlan(
					*Vehicle, Route.RouteDefinitionId, Payload.Stops, Candidate.Cities,
					Candidate.InventoryLedger, *Registry);
				if (PlanError != EHansaRoutePlanError::None || Payload.Stops.IsEmpty() ||
					Vehicle->CurrentCityId != Payload.Stops[0].CityId)
				{
					FHansaCommandGatewayResult Failure = MakeFailure(EHansaCommandGatewayError::RouteRejected, CommandIndex);
					Failure.RoutePlanError = PlanError != EHansaRoutePlanError::None
						? PlanError : EHansaRoutePlanError::VehicleLocationMismatch;
					return Failure;
				}
				Route.Stops = Payload.Stops;
				Route.CurrentStopIndex = 0;
				Route.NextStopIndex = 1;
				Route.Progress = FHansaRate();
				Route.RemainingTravelTicks = 0;
				Route.TotalTravelTicks = 0;
				Route.bPendingStopActions = false;
				Event.Type = EHansaDomainEventType::RouteEdited;
				Event.RouteId = Route.Id;
				Event.VehicleId = Route.VehicleId;
				Event.CityId = Vehicle->CurrentCityId;
				break;
			}
			case EHansaGameplayCommandType::SetRouteActive:
			{
				const FHansaSetRouteActiveCommand& Payload = Command.GetSetRouteActive();
				const int32 RouteIndex = Payload.RouteId.IsValid()
					? FindRouteIndex(Candidate.Routes, Payload.RouteId) : INDEX_NONE;
				if (RouteIndex == INDEX_NONE)
				{
					return MakeFailure(Payload.RouteId.IsValid() ? EHansaCommandGatewayError::TargetNotFound
						: EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				FHansaRouteState& Route = Candidate.Routes[RouteIndex];
				if (Route.OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				if (Route.Lifecycle == EHansaRouteLifecycleState::Cancelled ||
					Route.Lifecycle == EHansaRouteLifecycleState::Traveling)
				{
					return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid, CommandIndex);
				}
				FHansaVehicleState* Vehicle = FindVehicle(Candidate.Vehicles, Route.VehicleId);
				if (Vehicle == nullptr || Route.Stops.IsEmpty() ||
					Vehicle->CurrentCityId != Route.Stops[Route.CurrentStopIndex].CityId)
				{
					return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid, CommandIndex);
				}
				Route.Lifecycle = Payload.bActive ? EHansaRouteLifecycleState::AtStop : EHansaRouteLifecycleState::Inactive;
				Route.bPendingStopActions = Payload.bActive;
				Event.Type = EHansaDomainEventType::RouteActivationChanged;
				Event.RouteId = Route.Id;
				Event.VehicleId = Route.VehicleId;
				Event.CityId = Vehicle->CurrentCityId;
				Event.Value = Payload.bActive ? 1 : 0;
				break;
			}
			case EHansaGameplayCommandType::CancelRoute:
			{
				const FHansaCancelRouteCommand& Payload = Command.GetCancelRoute();
				const int32 RouteIndex = Payload.RouteId.IsValid()
					? FindRouteIndex(Candidate.Routes, Payload.RouteId) : INDEX_NONE;
				if (RouteIndex == INDEX_NONE)
				{
					return MakeFailure(Payload.RouteId.IsValid() ? EHansaCommandGatewayError::TargetNotFound
						: EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				FHansaRouteState& Route = Candidate.Routes[RouteIndex];
				if (Route.OwnerId != Header.Authority.IssuingHouseId)
				{
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				}
				if (Route.Lifecycle == EHansaRouteLifecycleState::Cancelled)
				{
					return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid, CommandIndex);
				}
				Route.Lifecycle = EHansaRouteLifecycleState::Cancelled;
				Route.Progress = FHansaRate();
				Route.RemainingTravelTicks = 0;
				Route.TotalTravelTicks = 0;
				Route.bPendingStopActions = false;
				Event.Type = EHansaDomainEventType::RouteCancelled;
				Event.RouteId = Route.Id;
				Event.VehicleId = Route.VehicleId;
				Event.Value = Route.LastTransfer.AppliedQuantity.GetRawValue();
				break;
			}
			case EHansaGameplayCommandType::QueueResearch:
			{
				const FHansaQueueResearchCommand& Payload = Command.GetQueueResearch();
				const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
				if (Payload.TechnologyId.IsEmpty() || Registry == nullptr)
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				const FHansaResearchQueueResult Queue = FHansaResearchExecutor::TryQueue(
					Candidate.Research, Header.Authority.IssuingHouseId, Payload.TechnologyId, Registry->GetTechnologies());
				if (!Queue)
				{
					return MakeFailure(EHansaCommandGatewayError::ResearchRejected, CommandIndex);
				}
				Event.Type = EHansaDomainEventType::ResearchQueued;
				Event.TechnologyId = Payload.TechnologyId;
				Event.Value = Registry->FindTechnology(Payload.TechnologyId)->DurationTicks;
				break;
			}
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
			Candidate.Buildings.Num() + Candidate.Vehicles.Num() + Candidate.Routes.Num() + Candidate.TestEntities.Num() +
			Candidate.InventoryLedger.CreateReadOnlyAccess().GetInventoryCount() + Candidate.Productions.Num() +
			Candidate.PopulationCohorts.Num() + Candidate.Markets.Num() + Candidate.Placement.GetMaps().Num() +
			Candidate.Placement.GetPlacements().Num() + Candidate.LocalLogisticsRequests.Num() +
			Candidate.LocalLogisticsJobs.Num();
		// Research records are authoritative entities even while their one-item queue is empty.
		const int64 AuthoritativeEntityCount = EntityCount + Candidate.Research.Num();
		TransientCache.BeginStep(TickBefore, AuthoritativeEntityCount);
		for (const EHansaSimulationPhase Phase : OrderedPhases)
		{
			TransientCache.RecordPhase(Phase);
			if (Phase == EHansaSimulationPhase::CalendarAndWorldEvents)
			{
				Candidate.Clock = ClockAfter.Value;
			}
			else if (Phase == EHansaSimulationPhase::VehicleMovementAndTransfers)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					FHansaTradeExecutor::AdvanceOneTick(
						Candidate.Routes, Candidate.Vehicles, Candidate.Houses,
						Candidate.InventoryLedger, Candidate.Placement, Candidate.Buildings,
						*Registry, Candidate.Clock.GetTick(),
						Candidate.PublishedDomainEventCount, PendingEvents);
					FHansaLocalLogisticsExecutor::SynchronizeProductionRequests(
						Candidate.LocalLogisticsRequests,
						Candidate.Productions,
						Candidate.Markets,
						*Registry,
						Candidate.InventoryLedger,
						Candidate.Placement,
						Candidate.Buildings,
						Candidate.Clock.GetTick());
				}
				FHansaLocalLogisticsExecutor::AdvanceOneTick(
					Candidate.LocalLogisticsRequests,
					Candidate.LocalLogisticsJobs,
					Candidate.NextLogisticsJobValue,
					Candidate.NextLogisticsReservationValue,
					Candidate.LocalLogisticsSettings,
					Candidate.InventoryLedger,
					Candidate.Placement,
					Candidate.Buildings,
					Definitions.GetEconomicRegistry(),
					Candidate.Clock.GetTick());
			}
			else if (Phase == EHansaSimulationPhase::ConstructionAndProduction)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					FHansaConstructionExecutor::AdvanceOneTick(
						Candidate.Buildings, *Registry, Candidate.Clock.GetTick(),
						Candidate.PublishedDomainEventCount, PendingEvents);
					if (!FHansaProductionExecutor::SynchronizeCompletedBuildings(
						Candidate.Productions, Candidate.Buildings, Candidate.Placement,
						Candidate.InventoryLedger, *Registry))
						return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext);
					FHansaPopulationExecutor::SynchronizeResidencesAndAssignWorkforce(
						Candidate.PopulationCohorts, Candidate.Productions, Candidate.Buildings,
						Candidate.Placement, Candidate.InventoryLedger, *Registry);
				}
				TArray<FHansaProductionStepEvent> ProductionEvents;
				FHansaProductionExecutor::AdvanceOneTick(
					Candidate.Productions,
					Candidate.NextProductionReservationValue,
					Candidate.Buildings,
					Candidate.InventoryLedger,
					Definitions.GetEconomicRegistry(),
					Candidate.Clock.GetTick(),
					ProductionEvents);
				for (const FHansaProductionStepEvent& ProductionEvent : ProductionEvents)
				{
					FHansaDomainEvent Event;
					Event.GlobalSequence = Candidate.PublishedDomainEventCount + 1;
					Event.Tick = Candidate.Clock.GetTick();
					Event.Type = ProductionEvent.Kind == EHansaProductionStepEventKind::CycleCompleted
						? EHansaDomainEventType::ProductionCycleCompleted
						: EHansaDomainEventType::ProductionBlockerChanged;
					Event.ProductionId = ProductionEvent.ProductionId;
					Event.BuildingId = ProductionEvent.BuildingId;
					Event.RecipeId = ProductionEvent.RecipeId;
					Event.ProductionBlocker = ProductionEvent.Blocker;
					Event.Value = static_cast<int64>(ProductionEvent.CompletedCycles);
					if (ProductionEvent.BuildingId.IsValid())
					{
						const FHansaBuildingState* Building = Candidate.Buildings.FindByPredicate(
							[&ProductionEvent](const FHansaBuildingState& Value)
							{
								return Value.Id == ProductionEvent.BuildingId;
							});
						if (Building != nullptr)
						{
							Event.IssuingHouseId = Building->OwnerId;
						}
					}
					++Candidate.PublishedDomainEventCount;
					PendingEvents.Add(MoveTemp(Event));
				}
			}
			else if (Phase == EHansaSimulationPhase::WorkforceAndNeeds)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					FHansaPopulationExecutor::AdvanceOneTick(Candidate.PopulationCohorts,
						Candidate.InventoryLedger, Candidate.Markets, Candidate.Buildings,
						Candidate.Placement, *Registry,
						Candidate.Clock.GetTick(), Candidate.Clock.GetMinutesPerTick());
					if (!Candidate.ConsumptionHistory.Record(Candidate.PopulationCohorts, Candidate.Clock))
						return MakeFailure(EHansaCommandGatewayError::InvalidPayload);
                    for (auto& Cohort : Candidate.PopulationCohorts)
                        if (!Cohort.ConsumptionHistory.Record(MakeArrayView(&Cohort, 1), Candidate.Clock))
                            return MakeFailure(EHansaCommandGatewayError::InvalidPayload);
				}
			}
			else if (Phase == EHansaSimulationPhase::MarketClearing)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					FHansaMarketExecutor::AdvanceOneTick(Candidate.Markets, Candidate.MarketSettings,
						Candidate.InventoryLedger, Candidate.Productions,
						Candidate.PopulationCohorts, *Registry, Candidate.Clock.GetTick(), Candidate.Routes, Candidate.Vehicles);
				}
			}
			else if (Phase == EHansaSimulationPhase::ResearchPoliticsAndVictory)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					TArray<FHansaResearchCompletion> Completions;
					FHansaResearchExecutor::AdvanceOneTick(Candidate.Research, Registry->GetTechnologies(), Completions);
					for (const FHansaResearchCompletion& Completion : Completions)
					{
						FHansaDomainEvent CompletionEvent;
						CompletionEvent.GlobalSequence = Candidate.PublishedDomainEventCount + 1;
						CompletionEvent.Tick = Candidate.Clock.GetTick();
						CompletionEvent.IssuingHouseId = Completion.HouseId;
						CompletionEvent.Type = EHansaDomainEventType::ResearchCompleted;
						CompletionEvent.TechnologyId = Completion.TechnologyId;
						CompletionEvent.Value = Completion.AppliedEffects.Num();
						++Candidate.PublishedDomainEventCount;
						PendingEvents.Add(MoveTemp(CompletionEvent));
					}
				}
			}
			else if (Phase != EHansaSimulationPhase::ApplyCommands)
			{
				ExecuteRepresentativeNoOpSystem(Phase);
			}
		}

		uint32 DirtyHashSubsystems = StateHashBit(EHansaStateHashSubsystem::SimulationMetadata);
		if (!Candidate.Routes.IsEmpty())
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Houses) |
				StateHashBit(EHansaStateHashSubsystem::Vehicles) |
				StateHashBit(EHansaStateHashSubsystem::Routes);
		}
		if (!Candidate.Buildings.IsEmpty())
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Buildings);
		}
		if (!Candidate.Productions.IsEmpty())
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Productions);
		}
		// The aggregate consumption history records a sample even when no cohort exists.
		if (Definitions.GetEconomicRegistry() != nullptr)
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Population);
		}
		if (!Candidate.Markets.IsEmpty())
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Market);
		}
		if (!Candidate.LocalLogisticsRequests.IsEmpty() || !Candidate.LocalLogisticsJobs.IsEmpty() ||
			!Candidate.Productions.IsEmpty())
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Logistics);
		}
		if (Candidate.InventoryLedger.CreateReadOnlyAccess().GetInventoryCount() > 0)
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Inventories);
		}
		if (Candidate.Research.ContainsByPredicate([](const FHansaHouseResearchState& Research)
			{ return !Research.ActiveTechnologyId.IsEmpty(); }))
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Research);
		}
		for (const FHansaGameplayCommand& Command : Input.Commands)
		{
			switch (Command.GetType())
			{
			case EHansaGameplayCommandType::CreateTestEntity:
			case EHansaGameplayCommandType::CancelTestEntity:
				DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::TestEntities);
				break;
			case EHansaGameplayCommandType::PlaceBuilding:
			case EHansaGameplayCommandType::CancelConstruction:
			case EHansaGameplayCommandType::RemoveBuilding:
				DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Placement) |
					StateHashBit(EHansaStateHashSubsystem::Houses) |
					StateHashBit(EHansaStateHashSubsystem::Buildings) |
					StateHashBit(EHansaStateHashSubsystem::Inventories);
				break;
			case EHansaGameplayCommandType::UpgradeResidence:
				DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Placement) |
					StateHashBit(EHansaStateHashSubsystem::Houses) |
					StateHashBit(EHansaStateHashSubsystem::Buildings) |
					StateHashBit(EHansaStateHashSubsystem::Inventories) |
					StateHashBit(EHansaStateHashSubsystem::Population);
				break;
			default:
				break;
			}
		}
		Candidate.InvalidateStateHashCache(DirtyHashSubsystems);
		State = MoveTemp(Candidate);
		FHansaCommandGatewayResult Result;
		Result.TickBefore = TickBefore;
		Result.TickAfter = State.Clock.GetTick();
		Result.FingerprintAfter.Value = State.ComputeDeterminismFingerprint(Definitions);
		Result.Events = MoveTemp(PendingEvents);
		return Result;
	}
}
