#include "Systems/HansaSimulationPipeline.h"

#include "Construction/HansaConstructionInternal.h"
#include "Logistics/HansaLocalLogisticsInternal.h"
#include "Math/NumericLimits.h"
#include "Market/HansaMarketInternal.h"
#include "Production/HansaProductionInternal.h"
#include "Population/HansaPopulationInternal.h"

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

		const FHansaBuildingState* FindBuilding(const TArray<FHansaBuildingState>& Buildings, const FHansaBuildingId BuildingId)
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
			static_cast<uint64>(State.Buildings.Num() + Input.Commands.Num()) * 2ULL;
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
					const FHansaBuildingState* Building = FindBuilding(Candidate.Buildings, Production.BuildingId);
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
				if (FindBuilding(Candidate.Buildings, Payload.BuildingId) != nullptr ||
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
				Building.DefinitionId = TargetBuildingId.Value;
				Cohort->TierId = TargetTierId.Value;
				Cohort->ResidenceCapacity = Target->ResidenceCapacity;
				Cohort->ConsecutiveGrowthTicks = 0;
				Cohort->ConsecutiveDeclineTicks = 0;
				for (FHansaPlacedBuildingRecord& Placement : Candidate.Placement.Placements)
				{
					if (Placement.BuildingId == Payload.BuildingId)
					{
						Placement.Spec.BuildingDefinitionId = TargetBuildingId.Value;
						break;
					}
				}
				Event.Type = EHansaDomainEventType::ResidenceUpgraded;
				Event.BuildingId = Payload.BuildingId;
				Event.Value = Cohort->Residents;
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
		TransientCache.BeginStep(TickBefore, EntityCount);
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
					Candidate.Clock.GetTick());
			}
			else if (Phase == EHansaSimulationPhase::ConstructionAndProduction)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					FHansaConstructionExecutor::AdvanceOneTick(
						Candidate.Buildings, *Registry, Candidate.Clock.GetTick(),
						Candidate.PublishedDomainEventCount, PendingEvents);
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
						Candidate.InventoryLedger, Candidate.Markets, Candidate.Buildings, *Registry,
						Candidate.Clock.GetTick(), Candidate.Clock.GetMinutesPerTick());
				}
			}
			else if (Phase == EHansaSimulationPhase::MarketClearing)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					FHansaMarketExecutor::AdvanceOneTick(Candidate.Markets, Candidate.MarketSettings,
						Candidate.InventoryLedger, Candidate.Productions,
						Candidate.PopulationCohorts, *Registry, Candidate.Clock.GetTick());
				}
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
