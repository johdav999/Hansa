#include "Trade/HansaTradeInternal.h"

#include "Logistics/HansaLocalLogistics.h"
#include "Math/NumericLimits.h"

namespace Hansa::Simulation
{
	namespace
	{
		FHansaVehicleState* TradeInternalFindVehicle(TArray<FHansaVehicleState>& Vehicles, const FHansaVehicleId Id)
		{
			return Vehicles.FindByPredicate([Id](const FHansaVehicleState& Vehicle) { return Vehicle.Id == Id; });
		}

		FHansaHouseState* TradeInternalFindHouse(TArray<FHansaHouseState>& Houses, const FHansaHouseId Id)
		{
			return Houses.FindByPredicate([Id](const FHansaHouseState& House) { return House.Id == Id; });
		}

		bool TradeInternalHasPlacementMap(const FHansaPlacementState& Placement, const FHansaCityDefinitionId CityId)
		{
			for (const FHansaPlacementMapInitialization& Map : Placement.GetMaps())
			{
				if (Map.CityId == CityId) return true;
			}
			return false;
		}

		bool CanUseCityInventory(const FHansaInventoryProjection& Inventory,
			const EHansaRouteMode Mode, const FHansaInventoryLedger& Inventories,
			const FHansaPlacementState& Placement, const TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaEconomicRegistry& Registry)
		{
			if (!TradeInternalHasPlacementMap(Placement, Inventory.CityId)) return true;
			if (!FHansaLocalLogisticsQueries::QueryRoadPath(
				Inventory.Id, Inventory.Id, Inventories.CreateReadOnlyAccess(), Placement, Buildings, &Registry).bMarketEligible)
			{
				return false;
			}
			if (Mode != EHansaRouteMode::Sea) return true;
			for (const FHansaPlacedBuildingRecord& Record : Placement.GetPlacements())
			{
				if (Record.Spec.CityId == Inventory.CityId &&
					Record.Spec.BuildingDefinitionId.ToString() == TEXT("Building.Dock") &&
					FHansaLocalLogisticsQueries::QueryBuildingMarketAccess(
						Record.BuildingId, Inventory.Id, Inventories.CreateReadOnlyAccess(),
						Placement, Buildings, &Registry).bMarketEligible)
				{
					return true;
				}
			}
			return false;
		}

		TArray<FHansaInventoryProjection> CityInventories(const FHansaInventoryLedger& Inventories,
			const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId, const EHansaRouteMode Mode,
			const FHansaPlacementState& Placement, const TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaEconomicRegistry& Registry)
		{
			TArray<FHansaInventoryProjection> Result;
			for (const FHansaInventoryProjection& Inventory : Inventories.CreateReadOnlyAccess().BuildProjection())
			{
				if (Inventory.OwnerKind == EHansaInventoryOwnerKind::City && Inventory.CityId == CityId &&
					Inventory.AcceptedGoods.Contains(GoodId) &&
					CanUseCityInventory(Inventory, Mode, Inventories, Placement, Buildings, Registry))
				{
					Result.Add(Inventory);
				}
			}
			return Result;
		}

		int64 TradeInternalAddClamped(const int64 Left, const int64 Right)
		{
			return Right > 0 && Left > TNumericLimits<int64>::Max() - Right
				? TNumericLimits<int64>::Max() : Left + Right;
		}

				const FHansaForeignPresenceState* FindPresence(FHansaHouseId Owner,FHansaCityDefinitionId City,TConstArrayView<FHansaForeignPresenceState> Presences)
		{
			for(const auto& Presence:Presences)if(Presence.HouseId==Owner&&Presence.CityId==City)return &Presence;return nullptr;
		}
const FHansaTradeStationState* FindAccessibleStation(FHansaHouseId Owner, FHansaCityDefinitionId City, EHansaRouteCargoActionKind ActionKind,
			TConstArrayView<FHansaForeignPresenceState> Presences, TConstArrayView<FHansaTradeStationState> Stations,
			const FHansaEconomicRegistry& Registry)
		{
			const auto* Policy = Registry.FindCityTradePolicyForCity(City.ToString());
			if (!Policy) return nullptr;
			for (const auto& Presence : Presences)
			{
				if (Presence.HouseId != Owner || Presence.CityId != City) continue;
				for (const auto& Station : Stations)
					if (Station.Id == Presence.StationId && Station.OwnerId == Owner && Station.CityId == City)
					{
						// Outbound recovery is deliberately available after suspension, revocation or voluntary closure.
						if(ActionKind==EHansaRouteCargoActionKind::StationLoad&&Station.Status!=EHansaTradeStationStatus::Proposed&&Station.Status!=EHansaTradeStationStatus::UnderConstruction&&
							(Station.Status==EHansaTradeStationStatus::Active||Station.OperationalState!=EHansaTradeStationOperationalState::Active))return &Station;
						if(Presence.Status!=EHansaForeignPresenceStatus::Active||Station.Status!=EHansaTradeStationStatus::Active||Station.OperationalState==EHansaTradeStationOperationalState::StorageBlocked)return nullptr;
						for (const TCHAR* Capability : {TEXT("PresenceCapability.RouteAccess"), TEXT("PresenceCapability.LocalStorage")})
							if (!Presence.GrantedCapabilityIds.Contains(Capability) || Policy->DeniedCapabilityIds.Contains(Capability)) return nullptr;
						return &Station;
					}
			}
			return nullptr;
		}

		FHansaRouteTransferRecord ExecuteAction(FHansaRouteState& Route, FHansaVehicleState& Vehicle,
			const FHansaRouteCargoAction& Action, const int32 ActionIndex, FHansaInventoryLedger& Inventories,
			const FHansaPlacementState& Placement, const TConstArrayView<FHansaBuildingState> Buildings,
			const FHansaEconomicRegistry& Registry, const FHansaSimulationTick Tick,
            FHansaHouseState* House, TConstArrayView<FHansaCityMarketState> Markets,
			TConstArrayView<FHansaHouseResearchState> Research,
			TConstArrayView<FHansaForeignPresenceState> Presences, TConstArrayView<FHansaTradeStationState> Stations)
		{
			FHansaRouteTransferRecord Record;
			Record.Tick = Tick;
			Record.StopIndex = Route.CurrentStopIndex;
			Record.ActionIndex = ActionIndex;
			Record.Kind = Action.Kind;
			Record.CityId = Route.Stops[Route.CurrentStopIndex].CityId;
			Record.GoodId = Action.GoodId;
			Record.RequestedQuantity = Action.QuantityLimit;

			int64 Remaining = Action.QuantityLimit.GetRawValue();
            if (IsStationTransfer(Action.Kind))
            {
                int64 HandlingCap=50000;const auto* Policy=Registry.FindCityTradePolicyForCity(Record.CityId.ToString());
                const auto* Presence=FindPresence(Route.OwnerId,Record.CityId,Presences);
                if(Policy&&Presence)for(const auto& Branch:Policy->Specializations)if(Presence->ActiveSpecializationIds.Contains(Branch.SpecializationId))HandlingCap=TradeInternalAddClamped(HandlingCap,Branch.StationTransferCapBonusMilliUnits);
                Remaining=FMath::Min(Remaining,HandlingCap);
            }
            const auto* City = Registry.FindCityMarket(Record.CityId.ToString());
            const bool StationAction = IsStationTransfer(Action.Kind);
            const bool Foreign = City && City->bMarketOnly && (Action.Kind == EHansaRouteCargoActionKind::Load || Action.Kind == EHansaRouteCargoActionKind::Unload);
            const auto* Station = StationAction ? FindAccessibleStation(Route.OwnerId, Record.CityId, Action.Kind, Presences, Stations, Registry) : nullptr;
            TArray<FHansaInventoryProjection> Endpoints;
            if (StationAction)
            {
                const auto Storage = Station ? Inventories.CreateReadOnlyAccess().QueryInventory(Station->InventoryId) : TOptional<FHansaInventoryProjection>();
                if (Storage && Storage->OwnerKind == EHansaInventoryOwnerKind::TradeStation && Storage->TradeStationId == Station->Id && Storage->CityId == Record.CityId && Storage->AcceptedGoods.Contains(Action.GoodId)) Endpoints.Add(*Storage);
            }
            else if (Action.Kind == EHansaRouteCargoActionKind::Load || Action.Kind == EHansaRouteCargoActionKind::Unload || (City && !City->bMarketOnly)) Endpoints = CityInventories(Inventories, Record.CityId, Action.GoodId, Vehicle.Mode, Placement, Buildings, Registry);
            if (Vehicle.OwnerId != Route.OwnerId || Vehicle.CurrentCityId != Record.CityId || (StationAction && Vehicle.Mode != EHansaRouteMode::Sea)) Endpoints.Reset();
            int64 Reserve = Action.MinimumSourceReserve.GetRawValue();
            if (Station) for (const auto& Order : Station->Orders)
                if (!Order.bCancelled && Order.Terms.Side == EHansaStationOrderSide::Release && Order.Terms.GoodId == Action.GoodId) Reserve = FMath::Max(Reserve, Order.Terms.TargetOrReserveMilliUnits);
            const FHansaCityMarketState* Market = nullptr;
            for (const auto& M : Markets) if (M.CityId == Record.CityId && M.GoodId == Action.GoodId) { Market = &M; break; }
            if (Foreign)
            {
                if (!House || !Market || Market->CurrentPriceMilliMarks <= 0) Remaining = 0;
                else
                {
					const int32 Reduction = FHansaResearchEffectResolver::GetBasisPoints(
						Research, Route.OwnerId, EHansaResearchEffectKind::TransactionFrictionReductionBasisPoints,
						Record.CityId.ToString());
					const int32 Friction = FMath::Max(0, 500 - Reduction);
					const int32 Multiplier = IsRouteLoad(Action.Kind) ? 10000 + Friction : 10000 - Friction;
					const auto EffectivePrice = FHansaCheckedIntegerMath::TryMultiplyDivide(
						Market->CurrentPriceMilliMarks, Multiplier, 10000, EHansaRoundingMode::HalfAwayFromZero);
					Record.UnitPriceMilliMarks = EffectivePrice ? EffectivePrice.Value : Market->CurrentPriceMilliMarks;
                    const int64 Cash = House->Money.GetRawValue();
                    const int64 Budget = IsRouteLoad(Action.Kind) ? FMath::Max<int64>(0,Cash) : Cash >= 0 ? MAX_int64-Cash : MAX_int64;
                    const auto Affordable = FHansaCheckedIntegerMath::TryMultiplyDivide(Budget,1000,Record.UnitPriceMilliMarks,EHansaRoundingMode::TowardZero);
                    if (Affordable) Remaining=FMath::Min(Remaining,Affordable.Value);
                    // A quotient overflow means the budget covers every legal quantity.
                    const auto Quote=FHansaCheckedIntegerMath::TryMultiplyDivide(Remaining,Record.UnitPriceMilliMarks,1000,EHansaRoundingMode::Ceiling);
                    if (!Quote) Remaining=0;
                }
            }
            int64 Applied = 0;
			if (IsRouteLoad(Action.Kind))
			{
				const TOptional<FHansaInventoryProjection> Cargo =
					Inventories.CreateReadOnlyAccess().QueryInventory(Vehicle.CargoInventoryId);
				if (Cargo.IsSet())
				{
					Remaining = FMath::Min(Remaining, Cargo->FreeCapacity.GetRawValue());
					const TArray<FHansaInventoryProjection> Sources =
						Endpoints;
					int64 TotalStock = 0;
					int64 TotalAvailable = 0;
					for (const FHansaInventoryProjection& Source : Sources)
					{
						const TOptional<FHansaInventoryStockProjection> Stock =
							Inventories.CreateReadOnlyAccess().QueryStock(Source.Id, Action.GoodId);
						if (!Stock.IsSet()) continue;
						TotalStock = TradeInternalAddClamped(TotalStock, Stock->Stock.GetRawValue());
						TotalAvailable = TradeInternalAddClamped(TotalAvailable, FMath::Max<int64>(0, Stock->Available.GetRawValue() - Inventories.CreateReadOnlyAccess().QueryProtectedRaw(Source.Id, Action.GoodId)));
					}
					Remaining = FMath::Min(Remaining, FMath::Min(TotalAvailable,
						FMath::Max<int64>(0, TotalStock - Reserve)));
					for (const FHansaInventoryProjection& Source : Sources)
					{
						if (Remaining <= 0) break;
						const TOptional<FHansaInventoryStockProjection> Stock =
							Inventories.CreateReadOnlyAccess().QueryStock(Source.Id, Action.GoodId);
						if (!Stock.IsSet()) continue;
						const int64 Transfer = FMath::Min(Remaining, FMath::Max<int64>(0, Stock->Available.GetRawValue() - Inventories.CreateReadOnlyAccess().QueryProtectedRaw(Source.Id, Action.GoodId)));
						if (Transfer <= 0) continue;
						const auto Result = Inventories.TryTransfer(FHansaInventoryEndpoint::Inventory(Source.Id),
							FHansaInventoryEndpoint::Inventory(Vehicle.CargoInventoryId), Action.GoodId,
							FHansaQuantity::FromRaw(Transfer), Tick,
							Inventories.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
						if (!Result.IsSuccess()) continue;
						Applied = TradeInternalAddClamped(Applied, Result.AppliedQuantity.GetRawValue());
						Remaining -= Result.AppliedQuantity.GetRawValue();
					}
				}
			}
			else
			{
				const TOptional<FHansaInventoryStockProjection> CargoStock =
					Inventories.CreateReadOnlyAccess().QueryStock(Vehicle.CargoInventoryId, Action.GoodId);
				Remaining = CargoStock.IsSet() ? FMath::Min(Remaining, CargoStock->Available.GetRawValue()) : 0;
                if (Action.Kind == EHansaRouteCargoActionKind::StationUnload || Action.Kind == EHansaRouteCargoActionKind::OwnedCityUnload)
                    Remaining = CargoStock ? FMath::Min(Remaining, FMath::Max<int64>(0, CargoStock->Available.GetRawValue() - FMath::Max(Action.MinimumSourceReserve.GetRawValue(), Inventories.CreateReadOnlyAccess().QueryProtectedRaw(Vehicle.CargoInventoryId, Action.GoodId)))) : 0;
				for (const FHansaInventoryProjection& Destination : Endpoints)
				{
					if (Remaining <= 0) break;
					const TOptional<FHansaInventoryProjection> Current =
						Inventories.CreateReadOnlyAccess().QueryInventory(Destination.Id);
					if (!Current.IsSet()) continue;
					const int64 Transfer = FMath::Min(Remaining, Current->FreeCapacity.GetRawValue());
					if (Transfer <= 0) continue;
					const auto Result = Inventories.TryTransfer(
						FHansaInventoryEndpoint::Inventory(Vehicle.CargoInventoryId),
						FHansaInventoryEndpoint::Inventory(Destination.Id), Action.GoodId,
						FHansaQuantity::FromRaw(Transfer), Tick,
						Inventories.CreateReadOnlyAccess().GetLastMovementSequence() + 1);
					if (!Result.IsSuccess()) continue;
					Applied = TradeInternalAddClamped(Applied, Result.AppliedQuantity.GetRawValue());
					Remaining -= Result.AppliedQuantity.GetRawValue();
				}
			}

			if (Foreign && Applied > 0 && House)
            {
                const bool Buy = IsRouteLoad(Action.Kind);
                const auto Quote = FHansaCheckedIntegerMath::TryMultiplyDivide(Applied,Record.UnitPriceMilliMarks,1000,Buy?EHansaRoundingMode::Ceiling:EHansaRoundingMode::TowardZero);
                check(Quote.IsSuccess()); // Quantity was bounded and quoted before any physical transfer.
                Record.SettledMoneyRaw=Buy?-Quote.Value:Quote.Value;
                House->Money=FHansaMoney::FromRaw(House->Money.GetRawValue()+Record.SettledMoneyRaw);
            }
            Record.AppliedQuantity = FHansaQuantity::FromRaw(Applied);
			Record.Outcome = Applied == Action.QuantityLimit.GetRawValue()
				? EHansaRouteTransferOutcome::Completed
				: Applied > 0 ? EHansaRouteTransferOutcome::Partial : EHansaRouteTransferOutcome::Missed;
			const TOptional<FHansaInventoryProjection> Cargo =
				Inventories.CreateReadOnlyAccess().QueryInventory(Vehicle.CargoInventoryId);
			Vehicle.Cargo = Cargo.IsSet() ? Cargo->UsedCapacity : FHansaQuantity();
			return Record;
		}
	}

	void FHansaTradeExecutor::PublishRouteEvent(FHansaRouteState& Route, FHansaVehicleState& Vehicle,
		const EHansaDomainEventType Type, const FHansaSimulationTick Tick,
		const FHansaCityDefinitionId CityId, const FHansaGoodId GoodId,
		const EHansaRouteCargoActionKind Kind, const int64 Value, const int64 RelatedValue,
		uint64& InOutPublishedEventCount, TArray<FHansaDomainEvent>& OutEvents)
	{
		FHansaDomainEvent Event;
		Event.GlobalSequence = ++InOutPublishedEventCount;
		Event.Tick = Tick;
		Event.Type = Type;
		Event.IssuingHouseId = Route.OwnerId;
		Event.RouteId = Route.Id;
		Event.VehicleId = Vehicle.Id;
		Event.CityId = CityId;
		Event.GoodId = GoodId;
		Event.RouteCargoActionKind = Kind;
		Event.Value = Value;
		Event.RelatedValue = RelatedValue;
		OutEvents.Add(MoveTemp(Event));
	}

	int32 FHansaTradeExecutor::FindTravelTicks(const FHansaCompiledRouteDefinition& Definition,
		const FHansaCityDefinitionId Source, const FHansaCityDefinitionId Destination)
	{
		for (const FHansaCompiledRouteConnection& Connection : Definition.Connections)
		{
			if ((Connection.SourceCityId == Source.ToString() && Connection.DestinationCityId == Destination.ToString()) ||
				(Connection.SourceCityId == Destination.ToString() && Connection.DestinationCityId == Source.ToString()))
			{
				return Connection.TravelTicks;
			}
		}
		return 0;
	}

	EHansaRoutePlanError FHansaTradeExecutor::ValidatePlan(const FHansaVehicleState& Vehicle,
		const FHansaRouteDefinitionId RouteDefinitionId, const TConstArrayView<FHansaRouteStop> Stops,
		const TConstArrayView<FHansaCityState> Cities, const FHansaInventoryLedger& Inventories,
		const FHansaEconomicRegistry& Registry,
		TConstArrayView<FHansaForeignPresenceState> Presences, TConstArrayView<FHansaTradeStationState> Stations)
	{
		if (!Vehicle.Id.IsValid() || !RouteDefinitionId.IsValid()) return EHansaRoutePlanError::InvalidIdentity;
		const FHansaCompiledVehicleDefinition* VehicleDefinition = Registry.FindVehicle(Vehicle.DefinitionId.ToString());
		const FHansaCompiledRouteDefinition* RouteDefinition = Registry.FindRoute(RouteDefinitionId.ToString());
		if (VehicleDefinition == nullptr || RouteDefinition == nullptr) return EHansaRoutePlanError::DefinitionNotFound;
		if (VehicleDefinition->Mode != RouteDefinition->Mode || Vehicle.Mode != RouteDefinition->Mode)
			return EHansaRoutePlanError::ModeMismatch;
		const TOptional<FHansaInventoryProjection> Cargo =
			Inventories.CreateReadOnlyAccess().QueryInventory(Vehicle.CargoInventoryId);
		if (!Cargo.IsSet() || Cargo->OwnerKind != EHansaInventoryOwnerKind::Vehicle ||
			Cargo->VehicleId != Vehicle.Id || Cargo->Capacity.GetRawValue() != VehicleDefinition->CargoCapacityMilliUnits)
			return EHansaRoutePlanError::InvalidCargoInventory;
		if (Stops.Num() < 2 || Stops.Num() > 16) return EHansaRoutePlanError::InvalidStops;
		int32 ActionCount = 0;
		for (int32 StopIndex = 0; StopIndex < Stops.Num(); ++StopIndex)
		{
			const FHansaRouteStop& Stop = Stops[StopIndex];
			if (!Stop.CityId.IsValid() || !Cities.ContainsByPredicate([&Stop](const FHansaCityState& City)
			{
				return City.DefinitionId == Stop.CityId;
			})) return EHansaRoutePlanError::InvalidStops;
			const FHansaRouteStop& Next = Stops[(StopIndex + 1) % Stops.Num()];
			if (Stop.CityId == Next.CityId || FindTravelTicks(*RouteDefinition, Stop.CityId, Next.CityId) <= 0)
				return EHansaRoutePlanError::UnreachableLeg;
			for (const FHansaRouteCargoAction& Action : Stop.Actions)
			{
				++ActionCount;
                if (static_cast<uint8>(Action.Kind) > static_cast<uint8>(EHansaRouteCargoActionKind::OwnedCityUnload)) return EHansaRoutePlanError::InvalidAction;
                if (IsStationTransfer(Action.Kind))
                {
                    const auto* Station = FindAccessibleStation(Vehicle.OwnerId, Stop.CityId, Action.Kind, Presences, Stations, Registry);
                    const auto Storage = Station ? Inventories.CreateReadOnlyAccess().QueryInventory(Station->InventoryId) : TOptional<FHansaInventoryProjection>();
                    if (Vehicle.Mode != EHansaRouteMode::Sea || !Storage || Storage->OwnerKind != EHansaInventoryOwnerKind::TradeStation || Storage->TradeStationId != Station->Id || Storage->CityId != Stop.CityId || !Storage->AcceptedGoods.Contains(Action.GoodId)) return EHansaRoutePlanError::InvalidAction;
                    int64 HandlingCap=50000;const auto* Policy=Registry.FindCityTradePolicyForCity(Stop.CityId.ToString());const auto* Presence=FindPresence(Vehicle.OwnerId,Stop.CityId,Presences);
                    if(Policy&&Presence)for(const auto& Branch:Policy->Specializations)if(Presence->ActiveSpecializationIds.Contains(Branch.SpecializationId))HandlingCap=TradeInternalAddClamped(HandlingCap,Branch.StationTransferCapBonusMilliUnits);
                    if(Action.QuantityLimit.GetRawValue()>HandlingCap)return EHansaRoutePlanError::InvalidAction;
                }
                else if (Action.Kind == EHansaRouteCargoActionKind::OwnedCityLoad || Action.Kind == EHansaRouteCargoActionKind::OwnedCityUnload)
                {
                    const auto* City = Registry.FindCityMarket(Stop.CityId.ToString());
                    if (!City || City->bMarketOnly) return EHansaRoutePlanError::InvalidAction;
                }
				if (!Action.GoodId.IsValid() || Registry.FindGood(Action.GoodId.ToString()) == nullptr ||
					Action.Condition != EHansaRouteCargoCondition::Always ||
					Action.QuantityLimit.GetRawValue() <= 0 ||
					Action.QuantityLimit.GetRawValue() > VehicleDefinition->CargoCapacityMilliUnits ||
					Action.MinimumSourceReserve.GetRawValue() < 0)
					return EHansaRoutePlanError::InvalidAction;
			}
		}
		return ActionCount > 0 ? EHansaRoutePlanError::None : EHansaRoutePlanError::InvalidStops;
	}

	void FHansaTradeExecutor::AdvanceOneTick(TArray<FHansaRouteState>& Routes,
		TArray<FHansaVehicleState>& Vehicles, TArray<FHansaHouseState>& Houses,
		FHansaInventoryLedger& Inventories, const FHansaPlacementState& Placement,
		const TConstArrayView<FHansaBuildingState> Buildings, const TConstArrayView<FHansaCityMarketState> Markets,
		const TConstArrayView<FHansaHouseResearchState> Research, const FHansaEconomicRegistry& Registry,
		const FHansaSimulationTick Tick, uint64& InOutPublishedEventCount,
		TArray<FHansaDomainEvent>& OutEvents,
		TConstArrayView<FHansaForeignPresenceState> Presences, TConstArrayView<FHansaTradeStationState> Stations)
	{
		for (FHansaVehicleState& Vehicle : Vehicles)
		{
			const FHansaCompiledVehicleDefinition* Definition = Registry.FindVehicle(Vehicle.DefinitionId.ToString());
			if (Definition == nullptr || Definition->CargoCapacityMilliUnits <= 0) continue;
			const int32 Bonus = FHansaResearchEffectResolver::GetBasisPoints(
				Research, Vehicle.OwnerId, EHansaResearchEffectKind::VehicleCapacityBasisPoints, Vehicle.DefinitionId.ToString());
			const auto Effective = FHansaCheckedIntegerMath::TryMultiplyDivide(
				Definition->CargoCapacityMilliUnits, 10000 + Bonus, 10000, EHansaRoundingMode::TowardZero);
			if (Effective)
			{
				Vehicle.Capacity = FHansaQuantity::FromRaw(Effective.Value);
				ensure(Inventories.TrySetCapacity(Vehicle.CargoInventoryId, Vehicle.Capacity));
			}
		}
		for (FHansaRouteState& Route : Routes)
		{
			if (Route.Lifecycle == EHansaRouteLifecycleState::Inactive ||
				Route.Lifecycle == EHansaRouteLifecycleState::Cancelled) continue;
			FHansaVehicleState* Vehicle = TradeInternalFindVehicle(Vehicles, Route.VehicleId);
			const FHansaCompiledRouteDefinition* Definition = Registry.FindRoute(Route.RouteDefinitionId.ToString());
			if (Vehicle == nullptr || Definition == nullptr || Route.Stops.Num() < 2) continue;

			if (Route.Lifecycle == EHansaRouteLifecycleState::Traveling)
			{
				if (FHansaHouseState* House = TradeInternalFindHouse(Houses, Route.OwnerId))
				{
					const auto Money = FHansaMoney::TrySubtract(House->Money,
						FHansaMoney::FromRaw(Vehicle->UpkeepPfennigPerTravelTick));
					if (Money) House->Money = Money.Value;
				}
				Vehicle->AccruedUpkeepPfennig = TradeInternalAddClamped(
					Vehicle->AccruedUpkeepPfennig, Vehicle->UpkeepPfennigPerTravelTick);
				--Route.RemainingTravelTicks;
				const int64 Elapsed = Route.TotalTravelTicks - Route.RemainingTravelTicks;
				Route.Progress = FHansaRate::TryRatio(Elapsed, Route.TotalTravelTicks).Value;
				if (Route.RemainingTravelTicks == 0)
				{
					Route.CurrentStopIndex = Route.NextStopIndex;
					Route.NextStopIndex = (Route.CurrentStopIndex + 1) % Route.Stops.Num();
					Route.Lifecycle = EHansaRouteLifecycleState::AtStop;
					Route.bPendingStopActions = true;
					Route.Progress = FHansaRate::FromPartsPerMillion(FHansaRate::Scale);
					++Route.CompletedLegCount;
					Vehicle->CurrentCityId = Route.Stops[Route.CurrentStopIndex].CityId;
					PublishRouteEvent(Route, *Vehicle, EHansaDomainEventType::RouteArrived, Tick,
						Vehicle->CurrentCityId, FHansaGoodId(), EHansaRouteCargoActionKind::Load,
						Route.CompletedLegCount, 0, InOutPublishedEventCount, OutEvents);
				}
				continue;
			}

			if (Route.bPendingStopActions)
			{
				const FHansaRouteStop& Stop = Route.Stops[Route.CurrentStopIndex];
				for (int32 ActionIndex = 0; ActionIndex < Stop.Actions.Num(); ++ActionIndex)
				{
					Route.LastTransfer = ExecuteAction(Route, *Vehicle, Stop.Actions[ActionIndex],
						ActionIndex, Inventories, Placement, Buildings, Registry, Tick,
						TradeInternalFindHouse(Houses, Route.OwnerId), Markets, Research, Presences, Stations);
                    if (Route.LastTransfer.SettledMoneyRaw != 0)
                        PublishRouteEvent(Route,*Vehicle,EHansaDomainEventType::RouteTradeSettled,Tick,Stop.CityId,Route.LastTransfer.GoodId,Route.LastTransfer.Kind,Route.LastTransfer.SettledMoneyRaw,Route.LastTransfer.UnitPriceMilliMarks,InOutPublishedEventCount,OutEvents);
					if (Route.LastTransfer.Outcome != EHansaRouteTransferOutcome::Completed)
						++Route.MissedCargoActionCount;
					PublishRouteEvent(Route, *Vehicle,
						Route.LastTransfer.Outcome == EHansaRouteTransferOutcome::Completed
							? EHansaDomainEventType::RouteCargoTransferred : EHansaDomainEventType::RouteCargoMissed,
						Tick, Stop.CityId, Route.LastTransfer.GoodId, Route.LastTransfer.Kind,
						Route.LastTransfer.AppliedQuantity.GetRawValue(),
						Route.LastTransfer.RequestedQuantity.GetRawValue(), InOutPublishedEventCount, OutEvents);
                    if (IsStationTransfer(Route.LastTransfer.Kind))
                    {
                        for (const auto& Presence : Presences)
                            if (Presence.HouseId == Route.OwnerId && Presence.CityId == Stop.CityId)
                                OutEvents.Last().TradeStationId = Presence.StationId;
                    }
				}
				Route.bPendingStopActions = false;
			}

			const FHansaRouteStop& Source = Route.Stops[Route.CurrentStopIndex];
			const FHansaRouteStop& Destination = Route.Stops[Route.NextStopIndex];
			const int32 TravelTicks = FindTravelTicks(*Definition, Source.CityId, Destination.CityId);
			if (TravelTicks <= 0) continue;
			Route.TotalTravelTicks = TravelTicks;
			Route.RemainingTravelTicks = TravelTicks;
			Route.Progress = FHansaRate();
			Route.Lifecycle = EHansaRouteLifecycleState::Traveling;
			PublishRouteEvent(Route, *Vehicle, EHansaDomainEventType::RouteDeparted, Tick,
				Source.CityId, FHansaGoodId(), EHansaRouteCargoActionKind::Load, TravelTicks, 0,
				InOutPublishedEventCount, OutEvents);
		}
	}
}
