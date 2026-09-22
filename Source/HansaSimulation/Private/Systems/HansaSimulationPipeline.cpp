#include "Systems/HansaSimulationPipeline.h"
#include "Trade/HansaWaterNavigation.h"
#include "Inventory/HansaSpoilageInternal.h"
#include "Population/HansaHeating.h"

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

		bool RoutePlanUsesReserveAutomation(const TConstArrayView<FHansaRouteStop> Stops)
		{
			for (const FHansaRouteStop& Stop : Stops)
				for (const FHansaRouteCargoAction& Action : Stop.Actions)
					if (Action.MinimumSourceReserve.GetRawValue() > 0) return true;
			return false;
		}

		bool MissingAuthoredRouteEffect(const FHansaEconomicRegistry& Registry,
			const TConstArrayView<FHansaHouseResearchState> Research, const FHansaHouseId HouseId,
			const EHansaResearchEffectKind Kind, const FString& RouteStableId)
		{
			return FHansaResearchEffectResolver::IsAuthoredForTarget(Registry.GetTechnologies(), Kind, RouteStableId) &&
				!FHansaResearchEffectResolver::IsEnabled(Research, HouseId, Kind, RouteStableId);
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
			const FHansaBuildingId BuildingId, const bool bRemoveResidenceCohort = false)
		{
			if (Productions.ContainsByPredicate([BuildingId](const FHansaProductionState& Production)
				{ return Production.BuildingId == BuildingId; }) ||
				(!bRemoveResidenceCohort && PopulationCohorts.ContainsByPredicate([BuildingId](const FHansaPopulationCohortState& Cohort)
				{ return Cohort.ResidenceBuildingId == BuildingId; })))
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
		uint64 StationOrderEvents = 0;
        for (const auto& Station : State.TradeStations) StationOrderEvents += static_cast<uint64>(Station.Orders.Num());
		const uint64 MaximumSystemEventCount = StationOrderEvents + static_cast<uint64>(State.Productions.Num()) +
			static_cast<uint64>(State.Research.Num()) +
			static_cast<uint64>(State.TradeStations.Num()) +
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
		if (const auto* Registry = Definitions.GetEconomicRegistry())
			FHansaHeating::RefreshProtection(Candidate.InventoryLedger, Candidate.Cities, Candidate.PopulationCohorts, *Registry, Candidate.Clock);
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
            case EHansaGameplayCommandType::SetProductionMode:
            case EHansaGameplayCommandType::UpgradeProduction:
            {
                const bool bUpgrade = Command.GetType() == EHansaGameplayCommandType::UpgradeProduction;
                const auto Id = bUpgrade ? Command.GetUpgradeProduction().ProductionId : Command.GetSetProductionMode().ProductionId;
                auto* Production = Candidate.Productions.FindByPredicate([&](const auto& P) { return P.Id == Id; });
                if (!Production || Production->Kind != EHansaProductionKind::BuildingRecipe)
                    return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
                auto* Building = Candidate.Buildings.FindByPredicate([&](const auto& B) { return B.Id == Production->BuildingId; });
                if (!Building || Building->OwnerId != Header.Authority.IssuingHouseId)
                    return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
                const auto* Registry = Definitions.GetEconomicRegistry();
                const auto* Source = Registry ? Registry->FindBuilding(Building->DefinitionId.ToString()) : nullptr;
                if (!Source || Building->ConstructionState != EHansaConstructionState::Completed || Production->PendingUpgradeBuildingId.IsValid())
                    return MakeFailure(EHansaCommandGatewayError::ConstructionStateInvalid, CommandIndex);
                if (!bUpgrade)
                {
                    const auto& Payload = Command.GetSetProductionMode();
                    if (!Payload.RecipeId.IsValid() || !Source->RecipeIds.Contains(Payload.RecipeId.ToString()) ||
                        (Payload.bFallbackToFresh && !Source->RecipeIds.Contains(TEXT("Recipe.CatchFish"))))
                        return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
                    Production->RequestedRecipeId = Payload.RecipeId;
                    Production->bFallbackToFresh = Payload.bFallbackToFresh;
                    Event.Type = EHansaDomainEventType::ProductionModeChanged;
                }
                else
                {
                    const auto* Target = Registry->FindBuilding(Source->UpgradeTargetBuildingId);
                    const auto TargetId = Target ? FHansaBuildingTypeId::TryParse(Target->StableId) : THansaValueResult<FHansaBuildingTypeId>::Failure(EHansaValueError::InvalidFormat);
                    if (!Target || !TargetId || Source->ResidenceCapacity || Target->ResidenceCapacity || Target->RecipeIds.IsEmpty() ||
                        Source->FootprintWidthCells != Target->FootprintWidthCells || Source->FootprintHeightCells != Target->FootprintHeightCells ||
                        !Target->RecipeIds.Contains(Production->RecipeId.ToString()))
                        return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
                    const auto* Placement = Candidate.Placement.FindPlacement(Building->Id);
                    if (!Placement) return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext, CommandIndex);
                    const auto Cost = FHansaConstructionExecutor::BuildCostProjection(Candidate.Houses, Candidate.InventoryLedger, *Registry,
                        Header.Authority.IssuingHouseId, Placement->Spec.CityId, TargetId.Value);
                    if (!Cost.IsAffordable() || !FHansaConstructionExecutor::TryPayCost(Candidate.Houses, Candidate.InventoryLedger, *Registry,
                        Header.Authority.IssuingHouseId, Placement->Spec.CityId, TargetId.Value, TickBefore))
                    {
                        auto Failure = MakeFailure(EHansaCommandGatewayError::ConstructionCostUnavailable, CommandIndex);
                        Failure.ConstructionCost = Cost; return Failure;
                    }
                    Production->PendingUpgradeBuildingId = TargetId.Value;
                    Event.Type = EHansaDomainEventType::ProductionUpgradeQueued;
                }
                Event.ProductionId = Production->Id; Event.BuildingId = Building->Id;
                break;
            }
            case EHansaGameplayCommandType::SetHouseholdAvailability:
            {
                const auto& Payload = Command.GetSetHouseholdAvailability();
                const auto* Market = SimulationPipelineFindBuilding(Candidate.Buildings, Payload.MarketBuildingId);
                const auto* Placement = Candidate.Placement.FindPlacement(Payload.MarketBuildingId);
                if (!Market || Market->OwnerId != Header.Authority.IssuingHouseId)
                    return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
                const auto* Registry = Definitions.GetEconomicRegistry();
                const auto* Definition = Registry ? Registry->FindBuilding(Market->DefinitionId.ToString()) : nullptr;
                if (!Placement || !Definition || !Definition->bProvidesMarketAccess || Payload.GoodId.ToString() != TEXT("Good.PreservedFish"))
                    return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
                for (auto& City : Candidate.Cities)
                    if (City.DefinitionId == Placement->Spec.CityId) City.bPreservedFishHouseholdAvailable = Payload.bAvailable;
                bool bChanged = false;
                for (const auto& Inventory : Candidate.InventoryLedger.CreateReadOnlyAccess().BuildProjection())
                    if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City && Inventory.CityId == Placement->Spec.CityId)
                        bChanged |= Candidate.InventoryLedger.SetHouseholdAvailable(Inventory.Id, Payload.GoodId, Payload.bAvailable);
                if (!bChanged) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
                Event.Type = EHansaDomainEventType::HouseholdAvailabilityChanged;
                Event.BuildingId = Market->Id; Event.Value = Payload.bAvailable ? 1 : 0;
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
				if (MissingAuthoredRouteEffect(*Registry, Candidate.Research, Header.Authority.IssuingHouseId,
					EHansaResearchEffectKind::UnlockStableId, Payload.Placement.BuildingDefinitionId.ToString()))
				{
					return MakeFailure(EHansaCommandGatewayError::ResearchEffectRequired, CommandIndex);
				}
				const FHansaPlacementValidationResult Validation = FHansaPlacementRules::Validate(
					Candidate.Placement, *Registry, Header.Authority.IssuingHouseId, Payload.Placement,
					Candidate.ForeignPresences, Candidate.LeasedPlots);
				if (!Validation)
				{
					FHansaCommandGatewayResult Failure = MakeFailure(
						EHansaCommandGatewayError::PlacementRejected, CommandIndex);
					Failure.PlacementValidation = Validation;
					return Failure;
				}
				FHansaInventoryId ConstructionFundingInventoryId;
				const bool bForeignConstruction = Validation.GetOccupiedCells().ContainsByPredicate([&](const FHansaGridCoordinate Cell)
				{
					const FHansaPlacementGridCell* GridCell = Candidate.Placement.FindCell(Payload.Placement.CityId, Cell);
					return GridCell != nullptr && GridCell->OwnerId != Header.Authority.IssuingHouseId;
				});
				if (bForeignConstruction)
				{
					const FHansaForeignPresenceState* Presence = Candidate.ForeignPresences.FindByPredicate([&](const auto& Value){ return Value.HouseId == Header.Authority.IssuingHouseId && Value.CityId == Payload.Placement.CityId; });
					const FHansaTradeStationState* Station = Presence ? Candidate.TradeStations.FindByPredicate([&](const auto& Value){ return Value.Id == Presence->StationId && Value.Status == EHansaTradeStationStatus::Active; }) : nullptr;
					if (Station == nullptr) return MakeFailure(EHansaCommandGatewayError::ConstructionCostUnavailable, CommandIndex);
					ConstructionFundingInventoryId = Station->InventoryId;
				}
				const FHansaConstructionCostProjection Cost = FHansaConstructionExecutor::BuildCostProjection(
					Candidate.Houses, Candidate.InventoryLedger, *Registry,
					Header.Authority.IssuingHouseId, Payload.Placement.CityId,
					Payload.Placement.BuildingDefinitionId, ConstructionFundingInventoryId);
				if (!Cost.IsAffordable() || !FHansaConstructionExecutor::TryPayCost(
					Candidate.Houses, Candidate.InventoryLedger, *Registry,
					Header.Authority.IssuingHouseId, Payload.Placement.CityId,
					Payload.Placement.BuildingDefinitionId, TickBefore, ConstructionFundingInventoryId))
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
				for (FHansaLeasedPlotState& Lease : Candidate.LeasedPlots)
				{
					if (Lease.OwnerId == Header.Authority.IssuingHouseId && Lease.CityId == Payload.Placement.CityId &&
						Validation.GetOccupiedCells().ContainsByPredicate([&](const FHansaGridCoordinate Cell){ return Cell.X >= Lease.BoundsMin.X && Cell.X <= Lease.BoundsMax.X && Cell.Y >= Lease.BoundsMin.Y && Cell.Y <= Lease.BoundsMax.Y; }))
					{
						Lease.OccupyingBuildingIds.AddUnique(Payload.BuildingId); Lease.OccupyingBuildingIds.Sort(); Lease.bOccupied = true; break;
					}
				}
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
				for (FHansaLeasedPlotState& Lease : Candidate.LeasedPlots) Lease.OccupyingBuildingIds.Remove(Payload.BuildingId);
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
                const auto* Registry=Definitions.GetEconomicRegistry();
                const auto* Definition=Registry?Registry->FindBuilding(Building.DefinitionId.ToString()):nullptr;
                const bool bRemoveResidenceCohort=Definition&&Definition->ResidenceCapacity>0&&!Definition->ResidentialCompoundId.IsEmpty();
                if (HasBuildingDependents(Candidate.Productions, Candidate.PopulationCohorts,
                    Candidate.InventoryLedger, Payload.BuildingId, bRemoveResidenceCohort))
				{
					return MakeFailure(EHansaCommandGatewayError::TargetHasDependents, CommandIndex);
				}
				if (!FHansaPlacementRules::Remove(Candidate.Placement, Payload.BuildingId))
				{
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				for (FHansaLeasedPlotState& Lease : Candidate.LeasedPlots) Lease.OccupyingBuildingIds.Remove(Payload.BuildingId);
				if(bRemoveResidenceCohort)
                {
                    for(const auto& Cohort:Candidate.PopulationCohorts)if(Cohort.ResidenceBuildingId==Payload.BuildingId)Event.Value+=Cohort.Residents;
                    Candidate.PopulationCohorts.RemoveAll([&](const auto& Cohort){return Cohort.ResidenceBuildingId==Payload.BuildingId;});
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
                const bool bCompoundDevelopment = Source != nullptr && Target != nullptr &&
                    !Source->ResidentialCompoundId.IsEmpty() && Source->ResidentialCompoundId == Target->ResidentialCompoundId &&
                    Source->CompoundDistrictId == Target->CompoundDistrictId && Source->CompoundStage > 0 &&
                    Target->CompoundStage == Source->CompoundStage + 1 &&
                    Source->ResidentPopulationTierId == Target->ResidentPopulationTierId &&
                    Source->FootprintWidthCells == Target->FootprintWidthCells &&
                    Source->FootprintHeightCells == Target->FootprintHeightCells;
				if (Cohort == nullptr || Source == nullptr || Target == nullptr || SourceTier == nullptr ||
					TargetTier == nullptr || !TargetBuildingId || !TargetTierId ||
					(!bCompoundDevelopment && TargetTier->PreviousTierId != SourceTier->StableId) ||
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
                if (Vehicle && Vehicle->Navigation.CityId.IsValid() && !Vehicle->Navigation.IsAtHome())
                    return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid,CommandIndex);
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
				const FString RouteStableId = Payload.RouteDefinitionId.ToString();
				if ((Payload.bActivate && MissingAuthoredRouteEffect(*Registry, Candidate.Research,
					Header.Authority.IssuingHouseId, EHansaResearchEffectKind::RouteScheduling, RouteStableId)) ||
					(RoutePlanUsesReserveAutomation(Payload.Stops) && MissingAuthoredRouteEffect(*Registry,
						Candidate.Research, Header.Authority.IssuingHouseId,
						EHansaResearchEffectKind::ReserveAutomation, RouteStableId)))
				{
					return MakeFailure(EHansaCommandGatewayError::ResearchEffectRequired, CommandIndex);
				}
				const EHansaRoutePlanError PlanError = FHansaTradeExecutor::ValidatePlan(
					*Vehicle, Payload.RouteDefinitionId, Payload.Stops, Candidate.Cities,
					Candidate.InventoryLedger, *Registry, Candidate.ForeignPresences, Candidate.TradeStations);
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
				const FString RouteStableId = Route.RouteDefinitionId.ToString();
				if (RoutePlanUsesReserveAutomation(Payload.Stops) && MissingAuthoredRouteEffect(*Registry,
					Candidate.Research, Header.Authority.IssuingHouseId,
					EHansaResearchEffectKind::ReserveAutomation, RouteStableId))
				{
					return MakeFailure(EHansaCommandGatewayError::ResearchEffectRequired, CommandIndex);
				}
				const EHansaRoutePlanError PlanError = FHansaTradeExecutor::ValidatePlan(
					*Vehicle, Route.RouteDefinitionId, Payload.Stops, Candidate.Cities,
					Candidate.InventoryLedger, *Registry, Candidate.ForeignPresences, Candidate.TradeStations);
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
            case EHansaGameplayCommandType::MoveShip:
            {
                const auto& Payload=Command.GetMoveShip();
                auto* V=FindVehicle(Candidate.Vehicles,Payload.VehicleId);
                if (!V) return MakeFailure(EHansaCommandGatewayError::TargetNotFound,CommandIndex);
                if (V->OwnerId!=Header.Authority.IssuingHouseId) return MakeFailure(EHansaCommandGatewayError::NotAuthorized,CommandIndex);
                auto& N=V->Navigation;
                if (V->Mode!=EHansaRouteMode::Sea || !N.CityId.IsValid() || V->CurrentCityId!=N.CityId)
                    return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid,CommandIndex);
                for (const auto& R:Candidate.Routes) if (R.VehicleId==V->Id &&
                    (R.Lifecycle==EHansaRouteLifecycleState::Traveling || R.Lifecycle==EHansaRouteLifecycleState::AtStop))
                    return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid,CommandIndex);
                const auto* Map=Candidate.Placement.GetMaps().FindByPredicate([&](const auto& M){return M.CityId==N.CityId;});
                TArray<FHansaGridCoordinate> Path;
                if (!Map || !FHansaWaterNavigation::FindPath(*Map,N.Cell,Payload.Target,Path))
                    return MakeFailure(EHansaCommandGatewayError::InvalidPayload,CommandIndex);
                N.Path=MoveTemp(Path);N.NextIndex=0;
                Event.Type=EHansaDomainEventType::ShipMoveOrdered;Event.VehicleId=V->Id;Event.CityId=N.CityId;
                Event.Value=Payload.Target.X;Event.RelatedValue=Payload.Target.Y;
                break;
            }
			case EHansaGameplayCommandType::SpotTrade:
			{
				const FHansaSpotTradeCommand& Payload = Command.GetSpotTrade();
				if (!Payload.VehicleId.IsValid() || !Payload.CityId.IsValid() || !Payload.GoodId.IsValid() ||
					Payload.Quantity.GetRawValue() <= 0 || Payload.ReviewedMarketUpdateTick < -1 ||
					Payload.ReviewedUnitPriceMilliMarks <= 0 || static_cast<uint8>(Payload.Side) > static_cast<uint8>(EHansaSpotTradeSide::SellToCity))
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				FHansaVehicleState* Vehicle = FindVehicle(Candidate.Vehicles, Payload.VehicleId);
				if (!Vehicle) return MakeFailure(EHansaCommandGatewayError::TargetNotFound, CommandIndex);
				if (Vehicle->OwnerId != Header.Authority.IssuingHouseId)
					return MakeFailure(EHansaCommandGatewayError::NotAuthorized, CommandIndex);
				const bool bTraveling = Candidate.Routes.ContainsByPredicate([&](const FHansaRouteState& Route)
					{ return Route.VehicleId == Vehicle->Id && Route.Lifecycle == EHansaRouteLifecycleState::Traveling; });
				if (Vehicle->Mode != EHansaRouteMode::Sea || Vehicle->CurrentCityId != Payload.CityId || bTraveling ||
					(Vehicle->Navigation.CityId.IsValid() && !Vehicle->Navigation.IsAtHome()))
					return MakeFailure(EHansaCommandGatewayError::SpotTradeNotBerthed, CommandIndex);
				const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
				const FHansaCompiledCityTradePolicyDefinition* Policy = Registry ? Registry->FindCityTradePolicyForCity(Payload.CityId.ToString()) : nullptr;
				const FHansaForeignPresenceState* Presence = Candidate.ForeignPresences.FindByPredicate([&](const FHansaForeignPresenceState& Value)
					{ return Value.HouseId == Header.Authority.IssuingHouseId && Value.CityId == Payload.CityId; });
				if (!Policy || !Policy->bPublicMarketAccess || !Presence || Presence->Status != EHansaForeignPresenceStatus::Active ||
					!Presence->GrantedCapabilityIds.Contains(TEXT("PresenceCapability.PublicMarketTrade")) ||
					Policy->DeniedCapabilityIds.Contains(TEXT("PresenceCapability.PublicMarketTrade")))
					return MakeFailure(EHansaCommandGatewayError::SpotTradeAccessUnavailable, CommandIndex);
				FHansaCityMarketState* Market = Candidate.Markets.FindByPredicate([&](const FHansaCityMarketState& Value)
					{ return Value.CityId == Payload.CityId && Value.GoodId == Payload.GoodId; });
				if (!Market || !Market->Report.bAvailable || Market->CurrentPriceMilliMarks <= 0)
					return MakeFailure(EHansaCommandGatewayError::SpotTradeAccessUnavailable, CommandIndex);
				const int32 Reduction = FHansaResearchEffectResolver::GetBasisPoints(Candidate.Research,
					Header.Authority.IssuingHouseId, EHansaResearchEffectKind::TransactionFrictionReductionBasisPoints, Payload.CityId.ToString());
				const int32 Friction = FMath::Max(0, 500 - Reduction);
				const int32 Multiplier = Payload.Side == EHansaSpotTradeSide::BuyFromCity ? 10000 + Friction : 10000 - Friction;
				const auto PriceResult = FHansaCheckedIntegerMath::TryMultiplyDivide(Market->CurrentPriceMilliMarks,
					Multiplier, 10000, EHansaRoundingMode::HalfAwayFromZero);
				if (!PriceResult || PriceResult.Value <= 0) return MakeFailure(EHansaCommandGatewayError::SpotTradeRejected, CommandIndex);
				const int64 UnitPrice = PriceResult.Value;
				if (Payload.ReviewedMarketUpdateTick != Market->LastUpdateTick || Payload.ReviewedUnitPriceMilliMarks != UnitPrice)
					return MakeFailure(EHansaCommandGatewayError::SpotTradeStaleReview, CommandIndex);
				FHansaHouseState* House = Candidate.Houses.FindByPredicate([&](const FHansaHouseState& Value){ return Value.Id == Header.Authority.IssuingHouseId; });
				if (!House) return MakeFailure(EHansaCommandGatewayError::UnknownIssuingHouse, CommandIndex);
				const FHansaInventoryReadOnlyAccess Opening = Candidate.InventoryLedger.CreateReadOnlyAccess();
				const TOptional<FHansaInventoryProjection> Cargo = Opening.QueryInventory(Vehicle->CargoInventoryId);
				if (!Cargo.IsSet()) return MakeFailure(EHansaCommandGatewayError::SpotTradeRejected, CommandIndex);
				int64 Remaining = Payload.Quantity.GetRawValue();
				EHansaSpotTradeBlocker Blocker = EHansaSpotTradeBlocker::None;
				if (Payload.Side == EHansaSpotTradeSide::BuyFromCity)
				{
					if (Cargo->FreeCapacity.GetRawValue() < Remaining) Blocker = EHansaSpotTradeBlocker::InsufficientShipCapacity;
					Remaining = FMath::Min(Remaining, Cargo->FreeCapacity.GetRawValue());
					int64 Available = 0;
					for (const FHansaInventoryId InventoryId : Market->InventoryIds)
						if (const TOptional<FHansaInventoryStockProjection> Stock = Opening.QueryStock(InventoryId, Payload.GoodId))
							Available = FMath::Min<int64>(MAX_int64, Available + FMath::Max<int64>(0, Stock->Available.GetRawValue() - Opening.QueryProtectedRaw(InventoryId, Payload.GoodId)));
					if (Available < Remaining) Blocker = EHansaSpotTradeBlocker::InsufficientMarketStock;
					Remaining = FMath::Min(Remaining, Available);
					const auto Affordable = FHansaCheckedIntegerMath::TryMultiplyDivide(FMath::Max<int64>(0, House->Money.GetRawValue()), 1000,
						UnitPrice, EHansaRoundingMode::TowardZero);
					if (!Affordable) return MakeFailure(EHansaCommandGatewayError::SpotTradeRejected, CommandIndex);
					if (Affordable.Value < Remaining) Blocker = EHansaSpotTradeBlocker::InsufficientFunds;
					Remaining = FMath::Min(Remaining, Affordable.Value);
				}
				else
				{
					const TOptional<FHansaInventoryStockProjection> Stock = Opening.QueryStock(Vehicle->CargoInventoryId, Payload.GoodId);
					const int64 Available = Stock.IsSet() ? Stock->Available.GetRawValue() : 0;
					if (Available < Remaining) Blocker = EHansaSpotTradeBlocker::InsufficientShipStock;
					Remaining = FMath::Min(Remaining, Available);
					int64 Free = 0;
					for (const FHansaInventoryId InventoryId : Market->InventoryIds)
						if (const TOptional<FHansaInventoryProjection> Inventory = Opening.QueryInventory(InventoryId)) Free = FMath::Min<int64>(MAX_int64, Free + Inventory->FreeCapacity.GetRawValue());
					if (Free < Remaining) Blocker = EHansaSpotTradeBlocker::InsufficientMarketCapacity;
					Remaining = FMath::Min(Remaining, Free);
				}
				const int64 AppliedTarget = FMath::Max<int64>(0, Remaining);
				int64 Applied = 0;
				if (Payload.Side == EHansaSpotTradeSide::BuyFromCity)
				{
					for (const FHansaInventoryId InventoryId : Market->InventoryIds)
					{
						if (Applied >= AppliedTarget) break;
						const auto Stock = Candidate.InventoryLedger.CreateReadOnlyAccess().QueryStock(InventoryId, Payload.GoodId);
						const int64 Quantity = Stock.IsSet() ? FMath::Min(AppliedTarget - Applied, FMath::Max<int64>(0, Stock->Available.GetRawValue() - Candidate.InventoryLedger.CreateReadOnlyAccess().QueryProtectedRaw(InventoryId, Payload.GoodId))) : 0;
						if (Quantity <= 0) continue;
						const auto Transfer = Candidate.InventoryLedger.TryTransfer(FHansaInventoryEndpoint::Inventory(InventoryId), FHansaInventoryEndpoint::Inventory(Vehicle->CargoInventoryId),
							Payload.GoodId, FHansaQuantity::FromRaw(Quantity), TickBefore, Candidate.InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
						if (!Transfer.IsSuccess()) return MakeFailure(EHansaCommandGatewayError::SpotTradeRejected, CommandIndex);
						Applied += Transfer.AppliedQuantity.GetRawValue();
					}
				}
				else
				{
					for (const FHansaInventoryId InventoryId : Market->InventoryIds)
					{
						if (Applied >= AppliedTarget) break;
						const auto Inventory = Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(InventoryId);
						const int64 Quantity = Inventory.IsSet() ? FMath::Min(AppliedTarget - Applied, Inventory->FreeCapacity.GetRawValue()) : 0;
						if (Quantity <= 0) continue;
						const auto Transfer = Candidate.InventoryLedger.TryTransfer(FHansaInventoryEndpoint::Inventory(Vehicle->CargoInventoryId), FHansaInventoryEndpoint::Inventory(InventoryId),
							Payload.GoodId, FHansaQuantity::FromRaw(Quantity), TickBefore, Candidate.InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
						if (!Transfer.IsSuccess()) return MakeFailure(EHansaCommandGatewayError::SpotTradeRejected, CommandIndex);
						Applied += Transfer.AppliedQuantity.GetRawValue();
					}
				}
				const auto Settlement = FHansaCheckedIntegerMath::TryMultiplyDivide(Applied, UnitPrice, 1000,
					Payload.Side == EHansaSpotTradeSide::BuyFromCity ? EHansaRoundingMode::Ceiling : EHansaRoundingMode::TowardZero);
				if (!Settlement) return MakeFailure(EHansaCommandGatewayError::SpotTradeRejected, CommandIndex);
				const int64 SignedSettlement = Payload.Side == EHansaSpotTradeSide::BuyFromCity ? -Settlement.Value : Settlement.Value;
				const auto MoneyAfter = FHansaCheckedIntegerMath::TryAdd(House->Money.GetRawValue(), SignedSettlement);
				if (!MoneyAfter || MoneyAfter.Value < 0) return MakeFailure(EHansaCommandGatewayError::SpotTradeRejected, CommandIndex);
				House->Money = FHansaMoney::FromRaw(MoneyAfter.Value);
				Vehicle->Cargo = Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Vehicle->CargoInventoryId)->UsedCapacity;
				FHansaSpotTradeRecord& Record = Vehicle->LastSpotTrade;
				Record.CommandId=Header.CommandId; Record.Tick=TickBefore; Record.HouseId=Header.Authority.IssuingHouseId; Record.VehicleId=Vehicle->Id;
				Record.CityId=Payload.CityId; Record.GoodId=Payload.GoodId; Record.Side=Payload.Side; Record.RequestedQuantity=Payload.Quantity;
				Record.AppliedQuantity=FHansaQuantity::FromRaw(Applied); Record.Blocker=Applied == Payload.Quantity.GetRawValue() ? EHansaSpotTradeBlocker::None : Blocker;
				Record.Outcome=Applied == Payload.Quantity.GetRawValue() ? EHansaSpotTradeOutcome::Completed : Applied > 0 ? EHansaSpotTradeOutcome::Partial : EHansaSpotTradeOutcome::Missed;
				Record.MarketUpdateTick=Market->LastUpdateTick; Record.UnitPriceMilliMarks=UnitPrice; Record.SettledMoneyRaw=SignedSettlement;
				Event.Type=Record.Outcome == EHansaSpotTradeOutcome::Completed ? EHansaDomainEventType::SpotTradeCompleted :
					Record.Outcome == EHansaSpotTradeOutcome::Partial ? EHansaDomainEventType::SpotTradePartial : EHansaDomainEventType::SpotTradeMissed;
				Event.VehicleId=Vehicle->Id; Event.CityId=Payload.CityId; Event.GoodId=Payload.GoodId; Event.Value=Applied; Event.RelatedValue=SignedSettlement;
				break;
			}
#include "Presence/HansaStationOrderCommand.inl"
#include "Presence/HansaPresenceProgressionCommand.inl"
			case EHansaGameplayCommandType::ProposeTradeStation:
			{
				const auto& Payload = Command.GetProposeTradeStation();
				if (!Payload.StationId.IsValid() || !Payload.FactorId.IsValid() || !Payload.LeasedPlotId.IsValid() || !Payload.InventoryId.IsValid() || !Payload.CityId.IsValid() || Payload.SiteId.IsEmpty())
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				if (Candidate.TradeStations.ContainsByPredicate([&](const auto& V){ return V.Id == Payload.StationId || V.InventoryId == Payload.InventoryId || V.FactorId == Payload.FactorId || V.LeasedPlotId == Payload.LeasedPlotId; }) ||
					Candidate.LeasedPlots.ContainsByPredicate([&](const auto& V){ return V.Id == Payload.LeasedPlotId || (V.CityId == Payload.CityId && V.SiteId == Payload.SiteId && V.bOccupied); }))
					return MakeFailure(EHansaCommandGatewayError::TargetAlreadyExists, CommandIndex);
				const auto* Registry = Definitions.GetEconomicRegistry();
				const auto* Policy = Registry ? Registry->FindCityTradePolicyForCity(Payload.CityId.ToString()) : nullptr;
				const auto* Site = Policy ? Policy->TradeStationSites.FindByPredicate([&](const auto& V){ return V.SiteId == Payload.SiteId; }) : nullptr;
				auto* Presence = Candidate.ForeignPresences.FindByPredicate([&](auto& V){ return V.HouseId == Header.Authority.IssuingHouseId && V.CityId == Payload.CityId; });
				const auto* CurrentStage = Presence && Registry ? Registry->FindPresenceStage(Presence->CurrentStageId) : nullptr;
				const auto* StationStage = Registry ? Registry->GetPresenceStages().FindByPredicate([&](const auto& V){ return V.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation")) && Registry->IsValidPresenceTransition(Payload.CityId.ToString(), Presence ? Presence->CurrentStageId : FString(), V.StableId); }) : nullptr;
				if (!Policy || !Site || Site->PlotCategory != TEXT("Commercial")) return MakeFailure(EHansaCommandGatewayError::TradeStationSiteUnavailable, CommandIndex);
				if (!Presence || Presence->Status != EHansaForeignPresenceStatus::Active || !CurrentStage || !StationStage || Presence->StationId.IsValid() ||
					Presence->Contributions.LawfulTradeVolumeMilliUnits < StationStage->RequiredLawfulTradeVolumeMilliUnits ||
					Presence->Contributions.CompletedDeliveryCount < StationStage->RequiredCompletedDeliveries ||
					(Presence->Contributions.InvestedPfennig > MAX_int64 - StationStage->UpgradeCostPfennig ? MAX_int64 : Presence->Contributions.InvestedPfennig + StationStage->UpgradeCostPfennig) < StationStage->RequiredInvestedPfennig ||
					Presence->Contributions.TransactionValuePfennig < StationStage->RequiredTransactionValuePfennig ||
					Presence->Contributions.FulfilledShortageMilliUnits < StationStage->RequiredFulfilledShortageMilliUnits ||
					Presence->Contributions.ReliableOperatingTicks < StationStage->RequiredReliableOperatingTicks ||
					Presence->Contributions.SolventOperatingTicks < StationStage->RequiredSolventOperatingTicks)
					return MakeFailure(EHansaCommandGatewayError::TradeStationAccessUnavailable, CommandIndex);
				FHansaInventoryInitialization Storage; Storage.Id = Payload.InventoryId; Storage.OwnerKind = EHansaInventoryOwnerKind::TradeStation;
				Storage.CityId = Payload.CityId; Storage.TradeStationId = Payload.StationId; Storage.Capacity = FHansaQuantity::FromRaw(Site->StorageCapacityMilliUnits);
				for (const auto& Good : Registry->GetGoods()) if (auto Id = FHansaGoodId::TryParse(Good.StableId); Id) Storage.AcceptedGoods.Add(Id.Value);
				if (!Candidate.InventoryLedger.TryAddEmptyInventory(MoveTemp(Storage))) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				FHansaTradeStationState Station; Station.Id=Payload.StationId; Station.OwnerId=Header.Authority.IssuingHouseId; Station.CityId=Payload.CityId; Station.SiteId=Payload.SiteId;
				Station.InventoryId=Payload.InventoryId; Station.FactorId=Payload.FactorId; Station.LeasedPlotId=Payload.LeasedPlotId; Station.ProposedTick=TickBefore; Station.UpkeepPfennigPerTick=Site->UpkeepPfennigPerTick;
				Candidate.TradeStations.Add(MoveTemp(Station)); Candidate.TradeStations.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
				FHansaLeasedPlotState Lease; Lease.Id=Payload.LeasedPlotId; Lease.StationId=Payload.StationId; Lease.OwnerId=Header.Authority.IssuingHouseId; Lease.CityId=Payload.CityId; Lease.SiteId=Payload.SiteId; Lease.PlotCategory=Site->PlotCategory; Lease.BoundsMin=Site->LeaseBoundsMin; Lease.BoundsMax=Site->LeaseBoundsMax; Lease.PermittedBuildingCategories=Site->PermittedBuildingCategories; Lease.bOccupied=true;
				Candidate.LeasedPlots.Add(MoveTemp(Lease)); Candidate.LeasedPlots.Sort([](const auto& A,const auto& B){return A.Id<B.Id;});
				Presence->StationId=Payload.StationId; Presence->LeasedPlotId=Payload.LeasedPlotId;
				Event.Type=EHansaDomainEventType::TradeStationProposed; Event.TradeStationId=Payload.StationId; Event.CityId=Payload.CityId;
				break;
			}
			case EHansaGameplayCommandType::FundTradeStation:
			{
				const auto& Payload=Command.GetFundTradeStation();
				auto* Station=Candidate.TradeStations.FindByPredicate([&](auto& V){return V.Id==Payload.StationId;});
				if (!Station) return MakeFailure(EHansaCommandGatewayError::TargetNotFound,CommandIndex);
				if (Station->OwnerId!=Header.Authority.IssuingHouseId) return MakeFailure(EHansaCommandGatewayError::NotAuthorized,CommandIndex);
				if (Station->OperationalState==EHansaTradeStationOperationalState::Underfunded)
				{
					auto* House=Candidate.Houses.FindByPredicate([&](auto& V){return V.Id==Station->OwnerId;});
					if(!Payload.FundingInventoryId.IsValid()||!House||Station->OutstandingUpkeepPfennig<=0||House->Money.GetRawValue()<Station->OutstandingUpkeepPfennig)return MakeFailure(EHansaCommandGatewayError::TradeStationCostUnavailable,CommandIndex);
					House->Money=FHansaMoney::FromRaw(House->Money.GetRawValue()-Station->OutstandingUpkeepPfennig);
					Event.Value=Station->OutstandingUpkeepPfennig;Station->OutstandingUpkeepPfennig=0;Station->Status=EHansaTradeStationStatus::Active;
					Station->OperationalState=EHansaTradeStationOperationalState::Active;Station->OperationalStateChangedTick=TickBefore;
					if(auto* Lease=Candidate.LeasedPlots.FindByPredicate([&](auto& V){return V.Id==Station->LeasedPlotId;}))Lease->bActive=true;
					Event.Type=EHansaDomainEventType::TradeStationFunded;Event.TradeStationId=Station->Id;Event.CityId=Station->CityId;break;
				}
				if (Station->Status!=EHansaTradeStationStatus::Proposed || !Payload.FundingInventoryId.IsValid()) return MakeFailure(EHansaCommandGatewayError::TradeStationStateInvalid,CommandIndex);
				const auto* Registry=Definitions.GetEconomicRegistry(); const auto* Policy=Registry?Registry->FindCityTradePolicyForCity(Station->CityId.ToString()):nullptr;
				const auto* Site=Policy?Policy->TradeStationSites.FindByPredicate([&](const auto& V){return V.SiteId==Station->SiteId;}):nullptr;
				auto* Presence=Candidate.ForeignPresences.FindByPredicate([&](auto& V){return V.HouseId==Station->OwnerId&&V.CityId==Station->CityId;});
				const auto* Stage=Registry&&Presence?Registry->GetPresenceStages().FindByPredicate([&](const auto& V){return V.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation"))&&Registry->IsValidPresenceTransition(Station->CityId.ToString(),Presence->CurrentStageId,V.StableId);}):nullptr;
				auto* House=Candidate.Houses.FindByPredicate([&](auto& V){return V.Id==Station->OwnerId;});
				const auto Funding=Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Payload.FundingInventoryId);
				bool bOwned=false;
				if(Funding.IsSet()&&Funding->OwnerKind==EHansaInventoryOwnerKind::Vehicle) if(const auto* Vehicle=Candidate.Vehicles.FindByPredicate([&](const auto& V){return V.Id==Funding->VehicleId;})) bOwned=Vehicle->OwnerId==Station->OwnerId;
				if(Funding.IsSet()&&(Funding->OwnerKind==EHansaInventoryOwnerKind::Building||Funding->OwnerKind==EHansaInventoryOwnerKind::Warehouse)) if(const auto* Building=Candidate.Buildings.FindByPredicate([&](const auto& V){return V.Id==Funding->BuildingId;})) bOwned=Building->OwnerId==Station->OwnerId;
				if(!Site||!Stage||!House||!Funding.IsSet()||!bOwned||House->Money.GetRawValue()<Stage->UpgradeCostPfennig) return MakeFailure(EHansaCommandGatewayError::TradeStationCostUnavailable,CommandIndex);
				for(const auto& Cost:Stage->UpgradeGoods){const auto Good=FHansaGoodId::TryParse(Cost.GoodId);const auto Stock=Good?Candidate.InventoryLedger.CreateReadOnlyAccess().QueryStock(Payload.FundingInventoryId,Good.Value):TOptional<FHansaInventoryStockProjection>();if(!Good||!Stock.IsSet()||Stock->Available.GetRawValue()<Cost.QuantityMilliUnits)return MakeFailure(EHansaCommandGatewayError::TradeStationCostUnavailable,CommandIndex);}
				House->Money=FHansaMoney::FromRaw(House->Money.GetRawValue()-Stage->UpgradeCostPfennig); Station->SpentMoneyRaw=Stage->UpgradeCostPfennig; Station->FundingInventoryId=Payload.FundingInventoryId;
				for(const auto& Cost:Stage->UpgradeGoods){const auto Good=FHansaGoodId::TryParse(Cost.GoodId).Value;const auto Tx=Candidate.InventoryLedger.TryTransfer(FHansaInventoryEndpoint::Inventory(Payload.FundingInventoryId),FHansaInventoryEndpoint::Sink(TEXT("TradeStationConstruction")),Good,FHansaQuantity::FromRaw(Cost.QuantityMilliUnits),TickBefore,Candidate.InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence()+1);if(!Tx.IsSuccess())return MakeFailure(EHansaCommandGatewayError::TradeStationCostUnavailable,CommandIndex);Station->SpentGoods.Add({Good,FHansaQuantity::FromRaw(Cost.QuantityMilliUnits)});}
				Station->SpentGoods.Sort([](const auto& A,const auto& B){return A.GoodId<B.GoodId;}); Station->Status=EHansaTradeStationStatus::UnderConstruction; Station->FundedTick=TickBefore;
				const auto Completion=FHansaSimulationTick::TryCreate(TickBefore.GetValue()+Site->ConstructionTicks); if(!Completion)return MakeFailure(EHansaCommandGatewayError::ClockOverflow,CommandIndex); Station->CompletionTick=Completion.Value;
				if(auto* Lease=Candidate.LeasedPlots.FindByPredicate([&](auto& V){return V.Id==Station->LeasedPlotId;})) Lease->bActive=true;
				Event.Type=EHansaDomainEventType::TradeStationFunded; Event.TradeStationId=Station->Id; Event.CityId=Station->CityId; Event.Value=Station->SpentMoneyRaw;
				break;
			}
			case EHansaGameplayCommandType::CloseTradeStation:
			{
				const auto& Payload=Command.GetCloseTradeStation(); auto* Station=Candidate.TradeStations.FindByPredicate([&](auto& V){return V.Id==Payload.StationId;});
				if(!Station)return MakeFailure(EHansaCommandGatewayError::TargetNotFound,CommandIndex); if(Station->OwnerId!=Header.Authority.IssuingHouseId)return MakeFailure(EHansaCommandGatewayError::NotAuthorized,CommandIndex);
				auto* ClosingLease=Candidate.LeasedPlots.FindByPredicate([&](auto& V){return V.Id==Station->LeasedPlotId;});
				if(!ClosingLease)return MakeFailure(EHansaCommandGatewayError::TradeStationStateInvalid,CommandIndex);
				const auto Storage=Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Station->InventoryId); if(!Storage.IsSet())return MakeFailure(EHansaCommandGatewayError::TradeStationStateInvalid,CommandIndex);
				const auto* Registry=Definitions.GetEconomicRegistry();const auto* Policy=Registry?Registry->FindCityTradePolicyForCity(Station->CityId.ToString()):nullptr;const auto* Site=Policy?Policy->TradeStationSites.FindByPredicate([&](const auto& V){return V.SiteId==Station->SiteId;}):nullptr;
				if(!Policy)return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext,CommandIndex);
				const auto Finalize=[&]()
				{
					ClosingLease->bOccupied=false;
					if(auto* Presence=Candidate.ForeignPresences.FindByPredicate([&](auto& V){return V.HouseId==Station->OwnerId&&V.CityId==Station->CityId;}))
						if(const auto* Initial=Registry?Registry->FindPresenceStage(Policy->InitialStageId):nullptr){Presence->CurrentStageId=Initial->StableId;Presence->GrantedCapabilityIds=Initial->GrantedCapabilityIds;Presence->GrantedCapabilityIds.RemoveAll([&](const FString& Id){return Policy->DeniedCapabilityIds.Contains(Id);});Presence->GrantedCapabilityIds.Sort();Presence->StationId=FHansaTradeStationId();Presence->LeasedPlotId=FHansaLeasedPlotId();Presence->LastUpgradeTick=TickBefore;}
				};
				if(Station->Status==EHansaTradeStationStatus::Closed)
				{
					if(Storage->UsedCapacity.GetRawValue()>0||Storage->Reserved.GetRawValue()>0||!ClosingLease->OccupyingBuildingIds.IsEmpty())return MakeFailure(EHansaCommandGatewayError::TradeStationHasCargo,CommandIndex);
					Finalize();Event.Type=EHansaDomainEventType::TradeStationClosed;Event.TradeStationId=Station->Id;Event.CityId=Station->CityId;break;
				}
				if(Station->Status==EHansaTradeStationStatus::UnderConstruction)
				{
					if(!Site)return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext,CommandIndex);
					auto* House=Candidate.Houses.FindByPredicate([&](auto& V){return V.Id==Station->OwnerId;}); if(!House)return MakeFailure(EHansaCommandGatewayError::UnknownIssuingHouse,CommandIndex);
					const int64 RefundMoney=Station->SpentMoneyRaw*Site->CancellationRefundBasisPoints/10000;const auto Money=FHansaCheckedIntegerMath::TryAdd(House->Money.GetRawValue(),RefundMoney);if(!Money)return MakeFailure(EHansaCommandGatewayError::TradeStationCostUnavailable,CommandIndex);House->Money=FHansaMoney::FromRaw(Money.Value);
					for(const auto& Cost:Station->SpentGoods){const int64 Raw=Cost.Quantity.GetRawValue()*Site->CancellationRefundBasisPoints/10000;if(Raw<=0)continue;const auto Tx=Candidate.InventoryLedger.TryTransfer(FHansaInventoryEndpoint::Source(TEXT("TradeStationCancellationRefund")),FHansaInventoryEndpoint::Inventory(Station->FundingInventoryId),Cost.GoodId,FHansaQuantity::FromRaw(Raw),TickBefore,Candidate.InventoryLedger.CreateReadOnlyAccess().GetLastMovementSequence()+1);if(!Tx.IsSuccess())return MakeFailure(EHansaCommandGatewayError::TradeStationCostUnavailable,CommandIndex);}
					Event.Value=RefundMoney;
				}
				for(auto& Order:Station->Orders)if(!Order.bCancelled)Order.bPaused=true;
				Station->Status=EHansaTradeStationStatus::Closed;Station->OperationalState=EHansaTradeStationOperationalState::VoluntarilyClosed;Station->OperationalStateChangedTick=TickBefore;Station->OutstandingUpkeepPfennig=0;ClosingLease->bActive=false;
				for(auto& Route:Candidate.Routes)
				{
					const bool bDepends=Route.OwnerId==Station->OwnerId&&Route.Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId==Station->CityId&&Stop.Actions.ContainsByPredicate([](const auto& Action){return IsStationTransfer(Action.Kind);});});
					if(bDepends&&(Route.Lifecycle==EHansaRouteLifecycleState::AtStop||Route.Lifecycle==EHansaRouteLifecycleState::Inactive))
					{
						if(Route.Stops.IsValidIndex(Route.CurrentStopIndex)&&Route.Stops[Route.CurrentStopIndex].CityId==Station->CityId)
							if(const auto* Action=Route.Stops[Route.CurrentStopIndex].Actions.FindByPredicate([](const auto& V){return IsStationTransfer(V.Kind);})){Route.LastTransfer.Tick=TickBefore;Route.LastTransfer.StopIndex=Route.CurrentStopIndex;Route.LastTransfer.Kind=Action->Kind;Route.LastTransfer.CityId=Station->CityId;Route.LastTransfer.GoodId=Action->GoodId;Route.LastTransfer.RequestedQuantity=Action->QuantityLimit;Route.LastTransfer.AppliedQuantity=FHansaQuantity();Route.LastTransfer.Outcome=EHansaRouteTransferOutcome::Missed;++Route.MissedCargoActionCount;}
						Route.Lifecycle=EHansaRouteLifecycleState::Inactive;Route.bPendingStopActions=false;
					}
				}
				if(Storage->UsedCapacity.GetRawValue()==0&&Storage->Reserved.GetRawValue()==0&&ClosingLease->OccupyingBuildingIds.IsEmpty())Finalize();
				Event.Type=EHansaDomainEventType::TradeStationClosed;Event.TradeStationId=Station->Id;Event.CityId=Station->CityId;break;
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
				if (Payload.bActive && Vehicle->Navigation.CityId.IsValid() && !Vehicle->Navigation.IsAtHome())
                    return MakeFailure(EHansaCommandGatewayError::RouteStateInvalid,CommandIndex);
				if (Payload.bActive)
				{
					const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry();
					if (Registry == nullptr) return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext, CommandIndex);
					if (MissingAuthoredRouteEffect(*Registry, Candidate.Research, Header.Authority.IssuingHouseId,
						EHansaResearchEffectKind::RouteScheduling, Route.RouteDefinitionId.ToString()))
						return MakeFailure(EHansaCommandGatewayError::ResearchEffectRequired, CommandIndex);
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
			case EHansaGameplayCommandType::SetHeatingReserve:
			{
				const auto& Payload = Command.GetSetHeatingReserve();
				const auto* Registry = Definitions.GetEconomicRegistry();
				const auto* Building = Candidate.Buildings.FindByPredicate([&](const auto& V) { return V.Id == Payload.MarketBuildingId; });
				const auto* Placement = Candidate.Placement.FindPlacement(Payload.MarketBuildingId);
				const auto* Definition = Building && Registry ? Registry->FindBuilding(Building->DefinitionId.ToString()) : nullptr;
				if (!Building || Building->OwnerId != Header.Authority.IssuingHouseId || !Placement || !Definition ||
					!Definition->bProvidesMarketAccess || Building->ConstructionState != EHansaConstructionState::Completed ||
					Payload.ReserveDays < 0 || Payload.ReserveDays > 90)
					return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				// A shared city policy requires uncontested civic ownership in this MVP.
				for (const auto& Other : Candidate.Buildings)
				{
					const auto* OtherPlacement = Candidate.Placement.FindPlacement(Other.Id);
					const auto* OtherDefinition = Registry->FindBuilding(Other.DefinitionId.ToString());
					if (OtherPlacement && OtherPlacement->Spec.CityId == Placement->Spec.CityId && OtherDefinition &&
						OtherDefinition->bProvidesMarketAccess && Other.OwnerId != Header.Authority.IssuingHouseId)
						return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				}
				auto* City = Candidate.Cities.FindByPredicate([&](const auto& V) { return V.DefinitionId == Placement->Spec.CityId; });
				if (!City) return MakeFailure(EHansaCommandGatewayError::InvalidPayload, CommandIndex);
				City->HeatingReserveDays = Payload.ReserveDays; City->bReleaseHeatingReserve = Payload.bReleaseProtection;
				FHansaHeating::RefreshProtection(Candidate.InventoryLedger, Candidate.Cities, Candidate.PopulationCohorts, *Registry, Candidate.Clock);
				Event.Type = EHansaDomainEventType::HeatingReserveChanged;
				Event.BuildingId = Payload.MarketBuildingId; Event.Value = Payload.ReserveDays;
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
			if (const auto* Registry = Definitions.GetEconomicRegistry())
				FHansaHeating::RefreshProtection(Candidate.InventoryLedger, Candidate.Cities, Candidate.PopulationCohorts, *Registry, ClockAfter.Value);
			if (Phase == EHansaSimulationPhase::CalendarAndWorldEvents)
			{
				Candidate.Clock = ClockAfter.Value;
				for(auto& Station:Candidate.TradeStations)
				{
					if(Station.Status==EHansaTradeStationStatus::Proposed||Station.Status==EHansaTradeStationStatus::UnderConstruction||Station.Status==EHansaTradeStationStatus::Closed)continue;
					auto* Presence=Candidate.ForeignPresences.FindByPredicate([&](auto& V){return V.HouseId==Station.OwnerId&&V.CityId==Station.CityId;});
					auto* Lease=Candidate.LeasedPlots.FindByPredicate([&](auto& V){return V.Id==Station.LeasedPlotId;});
					const auto SetState=[&](const EHansaTradeStationOperationalState Value){if(Station.OperationalState!=Value){Station.OperationalState=Value;Station.OperationalStateChangedTick=Candidate.Clock.GetTick();}};
					const auto PauseDependentRoutes=[&]()
					{
						for(auto& Route:Candidate.Routes)
						{
							const bool bDepends=Route.OwnerId==Station.OwnerId&&Route.Stops.ContainsByPredicate([&](const auto& Stop){return Stop.CityId==Station.CityId&&Stop.Actions.ContainsByPredicate([](const auto& Action){return IsStationTransfer(Action.Kind);});});
							if(bDepends&&(Route.Lifecycle==EHansaRouteLifecycleState::AtStop||Route.Lifecycle==EHansaRouteLifecycleState::Inactive))
							{
								if(Route.Stops.IsValidIndex(Route.CurrentStopIndex)&&Route.Stops[Route.CurrentStopIndex].CityId==Station.CityId)
									if(const auto* Action=Route.Stops[Route.CurrentStopIndex].Actions.FindByPredicate([](const auto& V){return IsStationTransfer(V.Kind);})){Route.LastTransfer.Tick=Candidate.Clock.GetTick();Route.LastTransfer.StopIndex=Route.CurrentStopIndex;Route.LastTransfer.Kind=Action->Kind;Route.LastTransfer.CityId=Station.CityId;Route.LastTransfer.GoodId=Action->GoodId;Route.LastTransfer.RequestedQuantity=Action->QuantityLimit;Route.LastTransfer.AppliedQuantity=FHansaQuantity();Route.LastTransfer.Outcome=EHansaRouteTransferOutcome::Missed;++Route.MissedCargoActionCount;}
								Route.Lifecycle=EHansaRouteLifecycleState::Inactive;Route.bPendingStopActions=false;
							}
						}
					};
					if(Station.Status==EHansaTradeStationStatus::Suspended&&Station.OperationalState==EHansaTradeStationOperationalState::Active)
					{
						SetState(EHansaTradeStationOperationalState::RightsSuspended);if(Lease)Lease->bActive=false;PauseDependentRoutes();continue;
					}
					if(!Presence||Presence->Status!=EHansaForeignPresenceStatus::Active)
					{
						Station.Status=EHansaTradeStationStatus::Suspended;SetState(Presence&&Presence->Status==EHansaForeignPresenceStatus::Revoked?EHansaTradeStationOperationalState::Revoked:EHansaTradeStationOperationalState::RightsSuspended);if(Lease)Lease->bActive=false;PauseDependentRoutes();continue;
					}
					if(Station.OperationalState==EHansaTradeStationOperationalState::Underfunded||Station.OperationalState==EHansaTradeStationOperationalState::RightsSuspended||Station.OperationalState==EHansaTradeStationOperationalState::Revoked)continue;
					Station.Status=EHansaTradeStationStatus::Active;if(Lease)Lease->bActive=true;
					auto* House=Candidate.Houses.FindByPredicate([&](auto& V){return V.Id==Station.OwnerId;});
					if(!House||House->Money.GetRawValue()<Station.UpkeepPfennigPerTick)
					{
						Station.OutstandingUpkeepPfennig=Station.UpkeepPfennigPerTick;Station.Status=EHansaTradeStationStatus::Suspended;SetState(EHansaTradeStationOperationalState::Underfunded);if(Lease)Lease->bActive=false;PauseDependentRoutes();continue;
					}
					House->Money=FHansaMoney::FromRaw(House->Money.GetRawValue()-Station.UpkeepPfennigPerTick);
					const auto Storage=Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Station.InventoryId);
					const bool bStorageBlocked=Storage&&Storage->FreeCapacity.GetRawValue()==0;
					const bool bHasLiveOrders=Station.Orders.ContainsByPredicate([](const auto& O){return !O.bCancelled;});
					const bool bAllOrdersPaused=bHasLiveOrders&&!Station.Orders.ContainsByPredicate([](const auto& O){return !O.bCancelled&&!O.bPaused;});
					SetState(bStorageBlocked?EHansaTradeStationOperationalState::StorageBlocked:bAllOrdersPaused?EHansaTradeStationOperationalState::OrderSuspended:EHansaTradeStationOperationalState::Active);
				}
			}
			else if (Phase == EHansaSimulationPhase::VehicleMovementAndTransfers)
            {
                for (auto& V:Candidate.Vehicles)
                {
                    auto& N=V.Navigation;
                    if (!N.IsMoving()) continue;
                    auto* House=Candidate.Houses.FindByPredicate([&](const auto& H){return H.Id==V.OwnerId;});
                    const auto Cost=FHansaMoney::FromRaw(V.UpkeepPfennigPerTravelTick);
                    if (!House) continue;
                    const auto Money=FHansaMoney::TrySubtract(House->Money,Cost);
                    if (!Money) continue;
                    House->Money=Money.Value;
                    V.AccruedUpkeepPfennig=V.AccruedUpkeepPfennig>MAX_int64-V.UpkeepPfennigPerTravelTick?MAX_int64:V.AccruedUpkeepPfennig+V.UpkeepPfennigPerTravelTick;
                    N.Cell=N.Path[N.NextIndex++];
                    if (!N.IsMoving())
                    {
                        N.Path.Reset();N.NextIndex=0;
                        FHansaDomainEvent E;E.Type=EHansaDomainEventType::ShipArrived;
                        E.VehicleId=V.Id;E.CityId=N.CityId;E.IssuingHouseId=V.OwnerId;
                        E.Tick=Candidate.Clock.GetTick();E.GlobalSequence=++Candidate.PublishedDomainEventCount;
                        E.Value=N.Cell.X;E.RelatedValue=N.Cell.Y;PendingEvents.Add(E);
                    }
                }
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
					FHansaTradeExecutor::AdvanceOneTick(
						Candidate.Routes, Candidate.Vehicles, Candidate.Houses,
						Candidate.InventoryLedger, Candidate.Placement, Candidate.Buildings, Candidate.Markets,
						Candidate.Research,
						*Registry, Candidate.Clock.GetTick(),
						Candidate.PublishedDomainEventCount, PendingEvents, Candidate.ForeignPresences, Candidate.TradeStations);
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
					Candidate.Research,
					Candidate.Clock.GetTick());
			}
            else if (Phase == EHansaSimulationPhase::WarehousesAndStorage)
            {
                if (const auto* Registry = Definitions.GetEconomicRegistry())
                    FHansaSpoilageExecutor::Advance(Candidate.InventoryLedger, Candidate.LocalLogisticsJobs,
                        Candidate.LocalLogisticsRequests, Candidate.Productions, *Registry, Candidate.Clock.GetTick(), Candidate.Clock.GetMinutesPerTick());
                for(auto& Vehicle:Candidate.Vehicles) if(Vehicle.CargoInventoryId.IsValid())
                    if(const auto Cargo=Candidate.InventoryLedger.CreateReadOnlyAccess().QueryInventory(Vehicle.CargoInventoryId);Cargo.IsSet()) Vehicle.Cargo=Cargo->UsedCapacity;
            }
			else if (Phase == EHansaSimulationPhase::ConstructionAndProduction)
			{
				if (const FHansaEconomicRegistry* Registry = Definitions.GetEconomicRegistry())
				{
#include "Presence/HansaPresenceProgressionTick.inl"
					for (FHansaTradeStationState& Station : Candidate.TradeStations)
					{
						if (Station.Status != EHansaTradeStationStatus::UnderConstruction || Candidate.Clock.GetTick().GetValue() < Station.CompletionTick.GetValue()) continue;
						auto* Presence = Candidate.ForeignPresences.FindByPredicate([&](auto& V){ return V.HouseId == Station.OwnerId && V.CityId == Station.CityId; });
						const auto* Policy = Registry->FindCityTradePolicyForCity(Station.CityId.ToString());
						const auto* Stage = Presence ? Registry->GetPresenceStages().FindByPredicate([&](const auto& V){ return V.GrantedCapabilityIds.Contains(TEXT("PresenceCapability.TradeStation")) && Registry->IsValidPresenceTransition(Station.CityId.ToString(), Presence->CurrentStageId, V.StableId); }) : nullptr;
						if (!Presence || !Policy || !Stage) return MakeFailure(EHansaCommandGatewayError::InvalidDefinitionContext);
						Station.Status=EHansaTradeStationStatus::Active; Station.CompletedTick=Candidate.Clock.GetTick();
						Presence->CurrentStageId=Stage->StableId; Presence->GrantedCapabilityIds=Stage->GrantedCapabilityIds;
						Presence->GrantedCapabilityIds.RemoveAll([&](const FString& Id){return Policy->DeniedCapabilityIds.Contains(Id);}); Presence->GrantedCapabilityIds.Sort(); Presence->LastUpgradeTick=Candidate.Clock.GetTick();
						FHansaDomainEvent Completion; Completion.Type=EHansaDomainEventType::TradeStationConstructionCompleted; Completion.Tick=Candidate.Clock.GetTick(); Completion.GlobalSequence=++Candidate.PublishedDomainEventCount;
						Completion.IssuingHouseId=Station.OwnerId; Completion.TradeStationId=Station.Id; Completion.CityId=Station.CityId; PendingEvents.Add(MoveTemp(Completion));
					}
                    // Paid upgrades wait for the current batch, preserving reservations and parcel identity.
                    for (auto& Production : Candidate.Productions)
                    {
                        if (!Production.PendingUpgradeBuildingId.IsValid() || Production.ProgressTicks != 0 || !Production.InputReservations.IsEmpty()) continue;
                        auto* Building = Candidate.Buildings.FindByPredicate([&](const auto& B) { return B.Id == Production.BuildingId; });
                        if (!Building) continue;
                        Building->DefinitionId = Production.PendingUpgradeBuildingId;
                        Building->ConstructionState = EHansaConstructionState::UnderConstruction;
                        Building->ConstructionProgress = FHansaRate();
                        Building->ConstructionElapsedTicks = 0;
                        Building->ConstructionStartedTick = Candidate.Clock.GetTick();
                        for (auto& Record : Candidate.Placement.Placements)
                            if (Record.BuildingId == Building->Id) Record.Spec.BuildingDefinitionId = Building->DefinitionId;
                        Production.PendingUpgradeBuildingId = FHansaBuildingTypeId();
                    }
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
					Candidate.Placement,
					Candidate.InventoryLedger,
					Definitions.GetEconomicRegistry(),
					Candidate.Research,
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
                const auto PreservedGood=FHansaGoodId::TryParse(TEXT("Good.PreservedFish")).Value;
                for (const auto& Inventory : Candidate.InventoryLedger.CreateReadOnlyAccess().BuildProjection())
                    if (Inventory.OwnerKind==EHansaInventoryOwnerKind::City && Inventory.AcceptedGoods.Contains(PreservedGood))
                        for (const auto& City : Candidate.Cities) if (City.DefinitionId==Inventory.CityId)
                            (void)Candidate.InventoryLedger.SetHouseholdAvailable(Inventory.Id,PreservedGood,City.bPreservedFishHouseholdAvailable);

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
#include "Presence/HansaStationOrderExecution.inl"
					FHansaMarketExecutor::AdvanceOneTick(Candidate.Markets, Candidate.MarketSettings,
						Candidate.InventoryLedger, Candidate.Productions,
						Candidate.PopulationCohorts, *Registry, Candidate.Clock.GetTick(), Candidate.Routes, Candidate.Vehicles,
						Candidate.RemoteIndustries,Candidate.RegionalShipments,Candidate.NextRegionalShipmentSequence);
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
        if (!Candidate.TradeStations.IsEmpty()) DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Houses);
		if (!Candidate.Routes.IsEmpty() || !Candidate.Vehicles.IsEmpty())
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Houses) |
				StateHashBit(EHansaStateHashSubsystem::Vehicles) |
				StateHashBit(EHansaStateHashSubsystem::Routes);
		}
		if (!Candidate.Buildings.IsEmpty())
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Buildings) | StateHashBit(EHansaStateHashSubsystem::Placement);
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
		// Completion clears ActiveTechnologyId. Include the pre-tick queue so the
		// final progress/effects update cannot keep the preceding research hash.
		if (State.Research.ContainsByPredicate([](const FHansaHouseResearchState& Research)
			{ return !Research.ActiveTechnologyId.IsEmpty(); }) ||
			Candidate.Research.ContainsByPredicate([](const FHansaHouseResearchState& Research)
			{ return !Research.ActiveTechnologyId.IsEmpty(); }))
		{
			DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Research);
		}
		for (const FHansaGameplayCommand& Command : Input.Commands)
		{
			switch (Command.GetType())
			{
			case EHansaGameplayCommandType::QueueResearch:
				// A one-tick technology can be queued and completed in this same tick.
				DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Research);
				break;
			case EHansaGameplayCommandType::SetHeatingReserve:
				DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Cities);
				break;
			case EHansaGameplayCommandType::CreateTestEntity:
			case EHansaGameplayCommandType::CancelTestEntity:
				DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::TestEntities);
				break;
			case EHansaGameplayCommandType::PlaceBuilding:
			case EHansaGameplayCommandType::CancelConstruction:
			case EHansaGameplayCommandType::RemoveBuilding:
                DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Population);
				DirtyHashSubsystems |= StateHashBit(EHansaStateHashSubsystem::Placement) |
					StateHashBit(EHansaStateHashSubsystem::Houses) |
					StateHashBit(EHansaStateHashSubsystem::Buildings) |
					StateHashBit(EHansaStateHashSubsystem::Inventories);
				break;
			case EHansaGameplayCommandType::UpgradeProduction:
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
