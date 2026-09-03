#include "Model/HansaSimulationState.h"

#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Diagnostics/HansaStateHash.h"

namespace Hansa::Simulation
{
	namespace
	{
		template <typename TValue, typename TKeySelector>
		bool HasDuplicateKey(const TArray<TValue>& Values, TKeySelector SelectKey)
		{
			for (int32 Index = 1; Index < Values.Num(); ++Index)
			{
				if (SelectKey(Values[Index - 1]) == SelectKey(Values[Index]))
				{
					return true;
				}
			}
			return false;
		}

		bool ContainsHouse(const TArray<FHansaHouseState>& Houses, const FHansaHouseId Id)
		{
			for (const FHansaHouseState& House : Houses)
			{
				if (House.Id == Id)
				{
					return true;
				}
			}
			return false;
		}

		bool ContainsVehicle(const TArray<FHansaVehicleState>& Vehicles, const FHansaVehicleId Id)
		{
			for (const FHansaVehicleState& Vehicle : Vehicles)
			{
				if (Vehicle.Id == Id)
				{
					return true;
				}
			}
			return false;
		}

		bool ContainsCity(const TArray<FHansaCityState>& Cities, const FHansaCityDefinitionId Id)
		{
			return Cities.ContainsByPredicate([Id](const FHansaCityState& City) { return City.DefinitionId == Id; });
		}

		bool ContainsBuilding(const TArray<FHansaBuildingState>& Buildings, const FHansaBuildingId Id)
		{
			return Buildings.ContainsByPredicate([Id](const FHansaBuildingState& Building) { return Building.Id == Id; });
		}

		bool IsKnownProductionKind(const EHansaProductionKind Kind)
		{
			return Kind == EHansaProductionKind::BuildingRecipe || Kind == EHansaProductionKind::BackgroundSupply;
		}
	}

	THansaValueResult<FHansaSimulationState> FHansaSimulationState::TryCreate(
		FHansaSimulationInitialization Initialization)
	{
		const THansaValueResult<FHansaSimulationClock> ValidClock = FHansaSimulationClock::TryCreate(
			Initialization.Clock.GetVersion(),
			Initialization.Clock.GetTick(),
			Initialization.Clock.GetMinutesPerTick());
		if (!ValidClock)
		{
			return THansaValueResult<FHansaSimulationState>::Failure(ValidClock.Error);
		}
		if ((Initialization.ProcessedCommandCount == 0 &&
			(Initialization.LastProcessedCommandSequence != 0 || Initialization.LastProcessedCommandId.IsValid())) ||
			(Initialization.ProcessedCommandCount != 0 &&
			(Initialization.LastProcessedCommandSequence == 0 || !Initialization.LastProcessedCommandId.IsValid())))
		{
			return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
		}
		THansaValueResult<FHansaInventoryLedger> ValidInventoryLedger = FHansaInventoryLedger::TryCreate(
			MoveTemp(Initialization.Inventories),
			Initialization.InventoryMovementHistoryCapacity);
		if (!ValidInventoryLedger)
		{
			return THansaValueResult<FHansaSimulationState>::Failure(ValidInventoryLedger.Error);
		}
		THansaValueResult<FHansaPlacementState> ValidPlacement = FHansaPlacementState::TryCreate(
			MoveTemp(Initialization.Placement));
		if (!ValidPlacement)
		{
			return THansaValueResult<FHansaSimulationState>::Failure(ValidPlacement.Error);
		}

		Initialization.RandomStreams.Sort([](const FHansaRandomStream& Left, const FHansaRandomStream& Right)
		{
			return Left.GetName().Compare(Right.GetName(), ESearchCase::CaseSensitive) < 0;
		});
		Initialization.Houses.Sort([](const FHansaHouseState& Left, const FHansaHouseState& Right) { return Left.Id < Right.Id; });
		Initialization.Cities.Sort([](const FHansaCityState& Left, const FHansaCityState& Right) { return Left.DefinitionId < Right.DefinitionId; });
		Initialization.Buildings.Sort([](const FHansaBuildingState& Left, const FHansaBuildingState& Right) { return Left.Id < Right.Id; });
		Initialization.Vehicles.Sort([](const FHansaVehicleState& Left, const FHansaVehicleState& Right) { return Left.Id < Right.Id; });
		Initialization.Routes.Sort([](const FHansaRouteState& Left, const FHansaRouteState& Right) { return Left.Id < Right.Id; });
		Initialization.TestEntities.Sort([](const FHansaTestEntityState& Left, const FHansaTestEntityState& Right) { return Left.Id < Right.Id; });
		Initialization.Productions.Sort([](const FHansaProductionInitialization& Left, const FHansaProductionInitialization& Right)
		{
			return Left.Id < Right.Id;
		});
		Initialization.PopulationCohorts.Sort([](const FHansaPopulationCohortInitialization& Left, const FHansaPopulationCohortInitialization& Right)
		{
			return Left.Id < Right.Id;
		});
		Initialization.LocalLogisticsRequests.Sort([](
			const FHansaLogisticsRequestInitialization& Left,
			const FHansaLogisticsRequestInitialization& Right)
		{
			return Left.Id < Right.Id;
		});
		for (FHansaCityMarketInitialization& Market : Initialization.Markets)
		{
			Market.InventoryIds.Sort();
		}
		Initialization.Markets.Sort([](const FHansaCityMarketInitialization& Left, const FHansaCityMarketInitialization& Right)
		{
			return Left.CityId != Right.CityId ? Left.CityId < Right.CityId : Left.GoodId < Right.GoodId;
		});

		if (HasDuplicateKey(Initialization.RandomStreams, [](const FHansaRandomStream& Stream) { return Stream.GetName(); }) ||
			HasDuplicateKey(Initialization.Houses, [](const FHansaHouseState& House) { return House.Id; }) ||
			HasDuplicateKey(Initialization.Cities, [](const FHansaCityState& City) { return City.DefinitionId; }) ||
			HasDuplicateKey(Initialization.Buildings, [](const FHansaBuildingState& Building) { return Building.Id; }) ||
			HasDuplicateKey(Initialization.Vehicles, [](const FHansaVehicleState& Vehicle) { return Vehicle.Id; }) ||
			HasDuplicateKey(Initialization.Routes, [](const FHansaRouteState& Route) { return Route.Id; }) ||
			HasDuplicateKey(Initialization.TestEntities, [](const FHansaTestEntityState& Entity) { return Entity.Id; }) ||
			HasDuplicateKey(Initialization.Productions, [](const FHansaProductionInitialization& Production) { return Production.Id; }) ||
			HasDuplicateKey(Initialization.PopulationCohorts, [](const FHansaPopulationCohortInitialization& Cohort) { return Cohort.Id; }) ||
			HasDuplicateKey(Initialization.LocalLogisticsRequests, [](const FHansaLogisticsRequestInitialization& Request) { return Request.Id; }) ||
			HasDuplicateKey(Initialization.Markets, [](const FHansaCityMarketInitialization& Market)
			{
				return Market.CityId.ToString() + TEXT("|") + Market.GoodId.ToString();
			}))
		{
			return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
		}

		for (const FHansaRandomStream& Stream : Initialization.RandomStreams)
		{
			if (Stream.GetName().IsEmpty())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
		}
		for (const FHansaHouseState& House : Initialization.Houses)
		{
			if (!House.Id.IsValid())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidZero);
			}
		}
		for (const FHansaCityState& City : Initialization.Cities)
		{
			if (!City.DefinitionId.IsValid())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (City.AggregateStock.GetRawValue() < 0)
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
			}
		}
		for (FHansaBuildingState& Building : Initialization.Buildings)
		{
			if (!Building.Id.IsValid() || !Building.OwnerId.IsValid() || !Building.DefinitionId.IsValid())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!ContainsHouse(Initialization.Houses, Building.OwnerId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!Building.ConstructionProgress.IsNormalized() || Building.ConstructionElapsedTicks < 0 ||
				(Building.ConstructionState != EHansaConstructionState::UnderConstruction &&
					Building.ConstructionState != EHansaConstructionState::Completed))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
			}
			if (Building.ConstructionProgress.GetPartsPerMillion() == FHansaRate::Scale)
			{
				Building.ConstructionState = EHansaConstructionState::Completed;
			}
			else if (Building.ConstructionState == EHansaConstructionState::Completed)
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
		}
		for (const FHansaPlacementMapInitialization& Map : ValidPlacement.Value.GetMaps())
		{
			if (!ContainsCity(Initialization.Cities, Map.CityId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
		}
		for (const FHansaPlacementEntitlement& Entitlement : ValidPlacement.Value.GetEntitlements())
		{
			if (!ContainsHouse(Initialization.Houses, Entitlement.HouseId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
		}
		for (const FHansaPlacedBuildingRecord& Placement : ValidPlacement.Value.GetPlacements())
		{
			const FHansaBuildingState* Building = Initialization.Buildings.FindByPredicate(
				[&Placement](const FHansaBuildingState& Value) { return Value.Id == Placement.BuildingId; });
			if (Building == nullptr || Building->OwnerId != Placement.OwnerId ||
				Building->DefinitionId != Placement.Spec.BuildingDefinitionId ||
				!ContainsCity(Initialization.Cities, Placement.Spec.CityId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
		}
		for (const FHansaVehicleState& Vehicle : Initialization.Vehicles)
		{
			if (!Vehicle.Id.IsValid() || !Vehicle.OwnerId.IsValid() || !Vehicle.DefinitionId.IsValid())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!ContainsHouse(Initialization.Houses, Vehicle.OwnerId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (Vehicle.Cargo.GetRawValue() < 0)
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
			}
		}
		for (const FHansaRouteState& Route : Initialization.Routes)
		{
			if (!Route.Id.IsValid() || !Route.OwnerId.IsValid() || !Route.VehicleId.IsValid())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!ContainsHouse(Initialization.Houses, Route.OwnerId) ||
				!ContainsVehicle(Initialization.Vehicles, Route.VehicleId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!Route.Progress.IsNormalized())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
			}
		}
		for (const FHansaTestEntityState& Entity : Initialization.TestEntities)
		{
			if (!Entity.Id.IsValid() || !Entity.OwnerId.IsValid())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!ContainsHouse(Initialization.Houses, Entity.OwnerId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (Entity.Value < 0)
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
			}
		}
		for (const FHansaInventoryProjection& Inventory : ValidInventoryLedger.Value.CreateReadOnlyAccess().BuildProjection())
		{
			if ((Inventory.OwnerKind == EHansaInventoryOwnerKind::City &&
				!ContainsCity(Initialization.Cities, Inventory.CityId)) ||
				(Inventory.OwnerKind != EHansaInventoryOwnerKind::City &&
				!ContainsBuilding(Initialization.Buildings, Inventory.BuildingId)))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
		}
		TArray<FHansaProductionState> ValidProductions;
		ValidProductions.Reserve(Initialization.Productions.Num());
		for (int32 ProductionIndex = 0; ProductionIndex < Initialization.Productions.Num(); ++ProductionIndex)
		{
			const FHansaProductionInitialization& Production = Initialization.Productions[ProductionIndex];
			if (!Production.Id.IsValid() || !IsKnownProductionKind(Production.Kind) ||
				!Production.OutputInventoryId.IsValid() ||
				Production.AllocatedLaborerWorkforce < 0 || Production.AllocatedArtisanWorkforce < 0)
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!ValidInventoryLedger.Value.CreateReadOnlyAccess().QueryInventory(Production.OutputInventoryId).IsSet())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (Production.Kind == EHansaProductionKind::BuildingRecipe)
			{
				if (!Production.BuildingId.IsValid() || !Production.RecipeId.IsValid() ||
					!Production.InputInventoryId.IsValid() || Production.CityId.IsValid() ||
					Production.SupplyGoodId.IsValid() || Production.SupplyQuantityPerCycle.GetRawValue() != 0 ||
					Production.SupplyCycleTicks != 0 || !ContainsBuilding(Initialization.Buildings, Production.BuildingId) ||
					!ValidInventoryLedger.Value.CreateReadOnlyAccess().QueryInventory(Production.InputInventoryId).IsSet())
				{
					return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
				}
				for (int32 ExistingIndex = 0; ExistingIndex < ProductionIndex; ++ExistingIndex)
				{
					const FHansaProductionInitialization& Existing = Initialization.Productions[ExistingIndex];
					if (Existing.Kind == EHansaProductionKind::BuildingRecipe && Existing.BuildingId == Production.BuildingId)
					{
						return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
					}
				}
			}
			else
			{
				const TOptional<FHansaInventoryProjection> OutputInventory =
					ValidInventoryLedger.Value.CreateReadOnlyAccess().QueryInventory(Production.OutputInventoryId);
				if (!Production.CityId.IsValid() || !Production.SupplyGoodId.IsValid() ||
					Production.SupplyQuantityPerCycle.GetRawValue() <= 0 || Production.SupplyCycleTicks <= 0 ||
					Production.BuildingId.IsValid() || Production.RecipeId.IsValid() ||
					Production.InputInventoryId.IsValid() || Production.AllocatedLaborerWorkforce != 0 ||
					Production.AllocatedArtisanWorkforce != 0 || Production.bUsesCityWorkforce ||
					!ContainsCity(Initialization.Cities, Production.CityId) ||
					!OutputInventory.IsSet() || OutputInventory->OwnerKind != EHansaInventoryOwnerKind::City ||
					OutputInventory->CityId != Production.CityId)
				{
					return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
				}
			}

			FHansaProductionState State;
			State.Id = Production.Id;
			State.Kind = Production.Kind;
			State.BuildingId = Production.BuildingId;
			State.CityId = Production.CityId;
			State.RecipeId = Production.RecipeId;
			State.SupplyGoodId = Production.SupplyGoodId;
			State.SupplyQuantityPerCycle = Production.SupplyQuantityPerCycle;
			State.SupplyCycleTicks = Production.SupplyCycleTicks;
			State.InputInventoryId = Production.InputInventoryId;
			State.OutputInventoryId = Production.OutputInventoryId;
			State.AllocatedLaborerWorkforce = Production.AllocatedLaborerWorkforce;
			State.AllocatedArtisanWorkforce = Production.AllocatedArtisanWorkforce;
			State.bUsesCityWorkforce = Production.bUsesCityWorkforce;
			State.bActive = Production.bActive;
			ValidProductions.Add(MoveTemp(State));
		}
		TArray<FHansaPopulationCohortState> ValidPopulationCohorts;
		ValidPopulationCohorts.Reserve(Initialization.PopulationCohorts.Num());
		for (const FHansaPopulationCohortInitialization& Cohort : Initialization.PopulationCohorts)
		{
			if (!Cohort.Id.IsValid() || !Cohort.ResidenceBuildingId.IsValid() || !Cohort.CityId.IsValid() ||
				!Cohort.ConsumptionInventoryId.IsValid() || !Cohort.TierId.IsValid() ||
				Cohort.Residents < 0 || Cohort.ResidenceCapacity <= 0 || Cohort.Residents > Cohort.ResidenceCapacity ||
				Cohort.PurchasingPowerBasisPoints < 0 || Cohort.PurchasingPowerBasisPoints > 10000 ||
				Cohort.ServiceAccessBasisPoints < 0 || Cohort.ServiceAccessBasisPoints > 10000 ||
				Cohort.ServiceReliabilityBasisPoints < 0 || Cohort.ServiceReliabilityBasisPoints > 10000 ||
				!ContainsBuilding(Initialization.Buildings, Cohort.ResidenceBuildingId) ||
				!ContainsCity(Initialization.Cities, Cohort.CityId) ||
				!ValidInventoryLedger.Value.CreateReadOnlyAccess().QueryInventory(Cohort.ConsumptionInventoryId).IsSet())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			FHansaPopulationCohortState State;
			State.Id = Cohort.Id;
			State.ResidenceBuildingId = Cohort.ResidenceBuildingId;
			State.CityId = Cohort.CityId;
			State.ConsumptionInventoryId = Cohort.ConsumptionInventoryId;
			State.TierId = Cohort.TierId;
			State.Residents = Cohort.Residents;
			State.ResidenceCapacity = Cohort.ResidenceCapacity;
			State.PurchasingPowerBasisPoints = Cohort.PurchasingPowerBasisPoints;
			State.ServiceAccessBasisPoints = Cohort.ServiceAccessBasisPoints;
			State.ServiceReliabilityBasisPoints = Cohort.ServiceReliabilityBasisPoints;
			ValidPopulationCohorts.Add(MoveTemp(State));
		}
		if (Initialization.MarketSettings.UpdateCadenceTicks <= 0 ||
			Initialization.MarketSettings.PriceHistoryCapacity <= 0 ||
			Initialization.MarketSettings.PriceHistoryCapacity > 4096 ||
			Initialization.MarketSettings.TargetSmoothingBasisPoints <= 0 ||
			Initialization.MarketSettings.TargetSmoothingBasisPoints > 10000 ||
			Initialization.MarketSettings.MaximumMovementBasisPointsPerUpdate <= 0 ||
			Initialization.MarketSettings.MaximumMovementBasisPointsPerUpdate > 10000 ||
			Initialization.MarketSettings.StaleAfterTicks < Initialization.MarketSettings.UpdateCadenceTicks)
		{
			return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
		}
		TArray<FHansaCityMarketState> ValidMarkets;
		ValidMarkets.Reserve(Initialization.Markets.Num());
		for (const FHansaCityMarketInitialization& Market : Initialization.Markets)
		{
			if (!Market.CityId.IsValid() || !Market.GoodId.IsValid() || !ContainsCity(Initialization.Cities, Market.CityId) ||
				Market.InventoryIds.IsEmpty() || Market.DesiredReserve.GetRawValue() < 0 ||
				Market.ConfirmedIncomingSupplyPerUpdate.GetRawValue() < 0 ||
				Market.SeasonModifierBasisPoints < -5000 || Market.SeasonModifierBasisPoints > 5000 ||
				Market.CityModifierBasisPoints < -5000 || Market.CityModifierBasisPoints > 5000 ||
				Market.MinimumPriceMilliMarks <= 0 || Market.MaximumPriceMilliMarks < Market.MinimumPriceMilliMarks ||
				Market.MaximumPriceMilliMarks > 1'000'000'000'000'000LL ||
				Market.InitialPriceMilliMarks < Market.MinimumPriceMilliMarks ||
				Market.InitialPriceMilliMarks > Market.MaximumPriceMilliMarks ||
				Market.InitialLastUpdateTick < -1 || Market.InitialLastUpdateTick > Initialization.Clock.GetTick().GetValue() ||
				HasDuplicateKey(Market.InventoryIds, [](const FHansaInventoryId Id) { return Id; }))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			for (const FHansaInventoryId InventoryId : Market.InventoryIds)
			{
				if (!ValidInventoryLedger.Value.CreateReadOnlyAccess().QueryStock(InventoryId, Market.GoodId).IsSet())
				{
					return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
				}
			}
			FHansaCityMarketState State;
			State.CityId = Market.CityId;
			State.GoodId = Market.GoodId;
			State.InventoryIds = Market.InventoryIds;
			State.DesiredReserve = Market.DesiredReserve;
			State.ConfirmedIncomingSupplyPerUpdate = Market.ConfirmedIncomingSupplyPerUpdate;
			State.SeasonModifierBasisPoints = Market.SeasonModifierBasisPoints;
			State.CityModifierBasisPoints = Market.CityModifierBasisPoints;
			State.MinimumPriceMilliMarks = Market.MinimumPriceMilliMarks;
			State.MaximumPriceMilliMarks = Market.MaximumPriceMilliMarks;
			State.CurrentPriceMilliMarks = Market.InitialPriceMilliMarks;
			State.LastUpdateTick = Market.InitialLastUpdateTick;
			ValidMarkets.Add(MoveTemp(State));
		}
		if (Initialization.LocalLogisticsSettings.JobCapacity.GetRawValue() <= 0 ||
			Initialization.LocalLogisticsSettings.PickupDelayTicks < 0 ||
			Initialization.LocalLogisticsSettings.TicksPerRoadCell <= 0 ||
			Initialization.LocalLogisticsSettings.MaximumConcurrentJobs <= 0 ||
			Initialization.LocalLogisticsSettings.MaximumConcurrentJobs > 4096)
		{
			return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
		}
		TArray<FHansaLogisticsRequestState> ValidLogisticsRequests;
		ValidLogisticsRequests.Reserve(Initialization.LocalLogisticsRequests.Num());
		const FHansaInventoryReadOnlyAccess InventoryView = ValidInventoryLedger.Value.CreateReadOnlyAccess();
		for (const FHansaLogisticsRequestInitialization& Request : Initialization.LocalLogisticsRequests)
		{
			if (!Request.Id.IsValid() || !Request.SourceInventoryId.IsValid() ||
				!Request.DestinationInventoryId.IsValid() ||
				Request.SourceInventoryId == Request.DestinationInventoryId || !Request.GoodId.IsValid() ||
				Request.Quantity.GetRawValue() <= 0 ||
				Request.Priority > EHansaLogisticsPriority::Critical ||
				!InventoryView.QueryInventory(Request.SourceInventoryId).IsSet() ||
				!InventoryView.QueryStock(Request.SourceInventoryId, Request.GoodId).IsSet() ||
				!InventoryView.QueryInventory(Request.DestinationInventoryId).IsSet() ||
				!InventoryView.QueryStock(Request.DestinationInventoryId, Request.GoodId).IsSet())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			FHansaLogisticsRequestState State;
			State.Id = Request.Id;
			State.SourceInventoryId = Request.SourceInventoryId;
			State.DestinationInventoryId = Request.DestinationInventoryId;
			State.GoodId = Request.GoodId;
			State.RequestedQuantity = Request.Quantity;
			State.RemainingQuantity = Request.Quantity;
			State.Priority = Request.Priority;
			State.CreatedTick = Initialization.Clock.GetTick();
			ValidLogisticsRequests.Add(MoveTemp(State));
		}

		FHansaSimulationState State;
		State.bInitialized = true;
		State.Clock = ValidClock.Value;
		State.CampaignSeed = Initialization.CampaignSeed;
		State.ProcessedCommandCount = Initialization.ProcessedCommandCount;
		State.LastProcessedCommandSequence = Initialization.LastProcessedCommandSequence;
		State.LastProcessedCommandId = Initialization.LastProcessedCommandId;
		State.CommandHistoryFingerprint = Initialization.CommandHistoryFingerprint;
		State.PublishedDomainEventCount = Initialization.PublishedDomainEventCount;
		State.RandomStreams = MoveTemp(Initialization.RandomStreams);
		State.Houses = MoveTemp(Initialization.Houses);
		State.Cities = MoveTemp(Initialization.Cities);
		State.Buildings = MoveTemp(Initialization.Buildings);
		State.Vehicles = MoveTemp(Initialization.Vehicles);
		State.Routes = MoveTemp(Initialization.Routes);
		State.TestEntities = MoveTemp(Initialization.TestEntities);
		State.Placement = MoveTemp(ValidPlacement.Value);
		State.InventoryLedger = MoveTemp(ValidInventoryLedger.Value);
		State.Productions = MoveTemp(ValidProductions);
		State.PopulationCohorts = MoveTemp(ValidPopulationCohorts);
		State.MarketSettings = Initialization.MarketSettings;
		State.Markets = MoveTemp(ValidMarkets);
		State.LocalLogisticsSettings = Initialization.LocalLogisticsSettings;
		State.LocalLogisticsRequests = MoveTemp(ValidLogisticsRequests);
		return THansaValueResult<FHansaSimulationState>::Success(State);
	}

	uint64 FHansaSimulationState::ComputeDeterminismFingerprint(
		const FHansaSimulationDefinitionContext& Definitions) const
	{
		return FHansaStateHasher::Compute(*this, Definitions).GetOverallHash();
	}
}
