#include "Diagnostics/HansaStateHash.h"

#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	namespace
	{
		class FNormalizedHashBuilder final
		{
		public:
			void AddUInt8(const uint8 Value)
			{
				Hash ^= Value;
				Hash *= FnvPrime;
			}

			void AddUInt32(const uint32 Value)
			{
				for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
				{
					AddUInt8(static_cast<uint8>(Value >> (ByteIndex * 8)));
				}
			}

			void AddUInt64(const uint64 Value)
			{
				for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
				{
					AddUInt8(static_cast<uint8>(Value >> (ByteIndex * 8)));
				}
			}

			void AddInt64(const int64 Value)
			{
				AddUInt64(static_cast<uint64>(Value));
			}

			void AddInt32(const int32 Value)
			{
				AddUInt32(static_cast<uint32>(Value));
			}

			void AddAsciiString(const FString& Value)
			{
				AddUInt32(static_cast<uint32>(Value.Len()));
				for (const TCHAR Character : Value)
				{
					AddUInt8(static_cast<uint8>(Character));
				}
			}

			[[nodiscard]] uint64 Get() const { return Hash; }

		private:
			static constexpr uint64 FnvPrime = 1099511628211ULL;
			uint64 Hash = FHansaSimulationState::EmptyCommandHistoryFingerprint;
		};

		template <typename TPopulate>
		FHansaSubsystemStateHash BuildSubsystem(
			const EHansaStateHashSubsystem Subsystem,
			const uint32 RecordCount,
			TPopulate Populate)
		{
			FNormalizedHashBuilder Builder;
			Builder.AddUInt32(FHansaStateHashReport::CurrentHashFormatVersion);
			Builder.AddUInt32(FHansaStateHashReport::CurrentNormalizationVersion);
			Builder.AddUInt8(static_cast<uint8>(Subsystem));
			Builder.AddUInt32(RecordCount);
			Populate(Builder);
			return { Subsystem, Builder.Get(), RecordCount };
		}
	}

	const TCHAR* LexToString(const EHansaStateHashSubsystem Subsystem)
	{
		switch (Subsystem)
		{
		case EHansaStateHashSubsystem::Contract: return TEXT("Contract");
		case EHansaStateHashSubsystem::SimulationMetadata: return TEXT("SimulationMetadata");
		case EHansaStateHashSubsystem::RandomStreams: return TEXT("RandomStreams");
		case EHansaStateHashSubsystem::Houses: return TEXT("Houses");
		case EHansaStateHashSubsystem::Cities: return TEXT("Cities");
		case EHansaStateHashSubsystem::Buildings: return TEXT("Buildings");
		case EHansaStateHashSubsystem::Vehicles: return TEXT("Vehicles");
		case EHansaStateHashSubsystem::Routes: return TEXT("Routes");
		case EHansaStateHashSubsystem::Inventories: return TEXT("Inventories");
		case EHansaStateHashSubsystem::Productions: return TEXT("Productions");
		case EHansaStateHashSubsystem::TestEntities: return TEXT("TestEntities");
		case EHansaStateHashSubsystem::Population: return TEXT("Population");
		case EHansaStateHashSubsystem::Market: return TEXT("Market");
		case EHansaStateHashSubsystem::Placement: return TEXT("Placement");
		case EHansaStateHashSubsystem::Logistics: return TEXT("Logistics");
		case EHansaStateHashSubsystem::NotApplicable: return TEXT("NotApplicable");
		default: return TEXT("UnknownStateHashSubsystem");
		}
	}

	const FHansaSubsystemStateHash* FHansaStateHashReport::Find(const EHansaStateHashSubsystem Subsystem) const
	{
		for (const FHansaSubsystemStateHash& Hash : Subsystems)
		{
			if (Hash.Subsystem == Subsystem)
			{
				return &Hash;
			}
		}
		return nullptr;
	}

	FString FHansaStateHashReport::ToCompactDebugString() const
	{
		FString Result = FString::Printf(
			TEXT("StateHash[v=%u;n=%u;p=%u;t=%lld;all=%016llX"),
			HashFormatVersion,
			NormalizationVersion,
			SystemPipelineVersion,
			static_cast<long long>(Tick.GetValue()),
			static_cast<unsigned long long>(OverallHash));
		for (const FHansaSubsystemStateHash& Hash : Subsystems)
		{
			Result += FString::Printf(
				TEXT(";%s=%016llX/%u"),
				LexToString(Hash.Subsystem),
				static_cast<unsigned long long>(Hash.Value),
				Hash.RecordCount);
		}
		Result += TEXT("]");
		return Result;
	}

	FHansaStateHashReport FHansaStateHasher::Compute(
		const FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions)
	{
		check(State.bInitialized);
		check(Definitions.IsValid());

		FHansaStateHashReport Report;
		Report.SystemPipelineVersion = FHansaSimulationState::CurrentSystemPipelineVersion;
		Report.Tick = State.Clock.GetTick();
		Report.Subsystems.Reserve(15);

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Contract, 1,
			[&Definitions](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(FHansaSimulationState::DeterminismFingerprintVersion);
				Builder.AddUInt32(FHansaSimulationState::CurrentSystemPipelineVersion);
				Builder.AddAsciiString(Definitions.GetScenarioId().ToString());
				Builder.AddUInt64(Definitions.GetDefinitionHash());
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::SimulationMetadata, 1,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(State.Clock.GetVersion().GetValue());
				Builder.AddInt64(State.Clock.GetTick().GetValue());
				Builder.AddUInt32(State.Clock.GetMinutesPerTick());
				Builder.AddUInt64(State.CampaignSeed);
				Builder.AddUInt64(State.ProcessedCommandCount);
				Builder.AddUInt64(State.LastProcessedCommandSequence);
				Builder.AddUInt64(State.LastProcessedCommandId.GetValue());
				Builder.AddUInt32(State.LastProcessedCommandId.GetGeneration());
				Builder.AddUInt64(State.CommandHistoryFingerprint);
				Builder.AddUInt64(State.PublishedDomainEventCount);
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::RandomStreams, State.RandomStreams.Num(),
			[&State](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaRandomStream& Stream : State.RandomStreams)
				{
					Builder.AddAsciiString(Stream.GetName());
					Builder.AddUInt8(static_cast<uint8>(Stream.GetAlgorithm()));
					Builder.AddUInt64(Stream.GetState());
					Builder.AddUInt64(Stream.GetDrawCount());
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Houses, State.Houses.Num(),
			[&State](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaHouseState& House : State.Houses)
				{
					Builder.AddUInt64(House.Id.GetValue());
					Builder.AddUInt32(House.Id.GetGeneration());
					Builder.AddInt64(House.Money.GetRawValue());
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Cities, State.Cities.Num(),
			[&State](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaCityState& City : State.Cities)
				{
					Builder.AddAsciiString(City.DefinitionId.ToString());
					Builder.AddInt64(City.AggregateStock.GetRawValue());
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Buildings, State.Buildings.Num(),
			[&State](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaBuildingState& Building : State.Buildings)
				{
					Builder.AddUInt64(Building.Id.GetValue());
					Builder.AddUInt32(Building.Id.GetGeneration());
					Builder.AddAsciiString(Building.DefinitionId.ToString());
					Builder.AddUInt64(Building.OwnerId.GetValue());
					Builder.AddUInt32(Building.OwnerId.GetGeneration());
					Builder.AddInt64(Building.ConstructionProgress.GetPartsPerMillion());
					Builder.AddUInt8(static_cast<uint8>(Building.ConstructionState));
					Builder.AddInt64(Building.ConstructionStartedTick.GetValue());
					Builder.AddInt32(Building.ConstructionElapsedTicks);
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Vehicles, State.Vehicles.Num(),
			[&State](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaVehicleState& Vehicle : State.Vehicles)
				{
					Builder.AddUInt64(Vehicle.Id.GetValue());
					Builder.AddUInt32(Vehicle.Id.GetGeneration());
					Builder.AddAsciiString(Vehicle.DefinitionId.ToString());
					Builder.AddUInt64(Vehicle.OwnerId.GetValue());
					Builder.AddUInt32(Vehicle.OwnerId.GetGeneration());
					Builder.AddInt64(Vehicle.Cargo.GetRawValue());
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Routes, State.Routes.Num(),
			[&State](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaRouteState& Route : State.Routes)
				{
					Builder.AddUInt64(Route.Id.GetValue());
					Builder.AddUInt32(Route.Id.GetGeneration());
					Builder.AddUInt64(Route.OwnerId.GetValue());
					Builder.AddUInt32(Route.OwnerId.GetGeneration());
					Builder.AddUInt64(Route.VehicleId.GetValue());
					Builder.AddUInt32(Route.VehicleId.GetGeneration());
					Builder.AddInt64(Route.Progress.GetPartsPerMillion());
				}
			}));

		const uint32 InventoryRecordCount = static_cast<uint32>(
			State.InventoryLedger.Inventories.Num() +
			State.InventoryLedger.Reservations.Num() +
			State.InventoryLedger.RecentMovements.Num());
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Inventories, InventoryRecordCount,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(static_cast<uint32>(State.InventoryLedger.MovementCapacity));
				Builder.AddUInt64(State.InventoryLedger.LastMovementSequence);
				Builder.AddUInt32(static_cast<uint32>(State.InventoryLedger.Inventories.Num()));
				for (const FHansaInventoryRecord& Inventory : State.InventoryLedger.Inventories)
				{
					Builder.AddUInt64(Inventory.Id.GetValue());
					Builder.AddUInt32(Inventory.Id.GetGeneration());
					Builder.AddUInt8(static_cast<uint8>(Inventory.OwnerKind));
					Builder.AddAsciiString(Inventory.CityId.ToString());
					Builder.AddUInt64(Inventory.BuildingId.GetValue());
					Builder.AddUInt32(Inventory.BuildingId.GetGeneration());
					Builder.AddInt64(Inventory.Capacity.GetRawValue());
					Builder.AddUInt32(static_cast<uint32>(Inventory.AcceptedGoods.Num()));
					for (const FHansaGoodId& GoodId : Inventory.AcceptedGoods)
					{
						Builder.AddAsciiString(GoodId.ToString());
					}
					Builder.AddUInt32(static_cast<uint32>(Inventory.Stocks.Num()));
					for (const FHansaInventoryStockRecord& Stock : Inventory.Stocks)
					{
						Builder.AddAsciiString(Stock.GoodId.ToString());
						Builder.AddInt64(Stock.Quantity.GetRawValue());
						Builder.AddInt64(Stock.Reserved.GetRawValue());
					}
				}
				Builder.AddUInt32(static_cast<uint32>(State.InventoryLedger.Reservations.Num()));
				for (const FHansaInventoryReservation& Reservation : State.InventoryLedger.Reservations)
				{
					Builder.AddUInt64(Reservation.Id.GetValue());
					Builder.AddUInt32(Reservation.Id.GetGeneration());
					Builder.AddUInt64(Reservation.InventoryId.GetValue());
					Builder.AddUInt32(Reservation.InventoryId.GetGeneration());
					Builder.AddAsciiString(Reservation.GoodId.ToString());
					Builder.AddInt64(Reservation.Quantity.GetRawValue());
				}
				Builder.AddUInt32(static_cast<uint32>(State.InventoryLedger.RecentMovements.Num()));
				for (const FHansaInventoryMovement& Movement : State.InventoryLedger.RecentMovements)
				{
					Builder.AddUInt64(Movement.Sequence);
					Builder.AddInt64(Movement.Tick.GetValue());
					Builder.AddUInt8(static_cast<uint8>(Movement.Kind));
					Builder.AddUInt64(Movement.InventoryId.GetValue());
					Builder.AddUInt32(Movement.InventoryId.GetGeneration());
					Builder.AddUInt64(Movement.CounterpartyInventoryId.GetValue());
					Builder.AddUInt32(Movement.CounterpartyInventoryId.GetGeneration());
					Builder.AddAsciiString(Movement.ExternalEndpointId.ToString());
					Builder.AddAsciiString(Movement.GoodId.ToString());
					Builder.AddInt64(Movement.Quantity.GetRawValue());
					Builder.AddUInt64(Movement.ReservationId.GetValue());
					Builder.AddUInt32(Movement.ReservationId.GetGeneration());
				}
			}));

		uint32 ProductionRecordCount = static_cast<uint32>(State.Productions.Num());
		for (const FHansaProductionState& Production : State.Productions)
		{
			ProductionRecordCount += static_cast<uint32>(Production.InputReservations.Num());
		}
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Productions, ProductionRecordCount,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt64(State.NextProductionReservationValue);
				Builder.AddUInt32(static_cast<uint32>(State.Productions.Num()));
				for (const FHansaProductionState& Production : State.Productions)
				{
					Builder.AddUInt64(Production.Id.GetValue());
					Builder.AddUInt32(Production.Id.GetGeneration());
					Builder.AddUInt8(static_cast<uint8>(Production.Kind));
					Builder.AddUInt64(Production.BuildingId.GetValue());
					Builder.AddUInt32(Production.BuildingId.GetGeneration());
					Builder.AddAsciiString(Production.CityId.ToString());
					Builder.AddAsciiString(Production.RecipeId.ToString());
					Builder.AddAsciiString(Production.SupplyGoodId.ToString());
					Builder.AddInt64(Production.SupplyQuantityPerCycle.GetRawValue());
					Builder.AddInt32(Production.SupplyCycleTicks);
					Builder.AddUInt64(Production.InputInventoryId.GetValue());
					Builder.AddUInt32(Production.InputInventoryId.GetGeneration());
					Builder.AddUInt64(Production.OutputInventoryId.GetValue());
					Builder.AddUInt32(Production.OutputInventoryId.GetGeneration());
					Builder.AddInt32(Production.AllocatedLaborerWorkforce);
					Builder.AddInt32(Production.AllocatedArtisanWorkforce);
					Builder.AddUInt8(Production.bUsesCityWorkforce ? 1 : 0);
					Builder.AddUInt8(Production.bActive ? 1 : 0);
					Builder.AddInt32(Production.ProgressTicks);
					Builder.AddUInt64(Production.CompletedCycles);
					Builder.AddUInt8(Production.bCompletedCycleLastTick ? 1 : 0);
					Builder.AddUInt8(static_cast<uint8>(Production.Blocker));
					Builder.AddAsciiString(Production.BlockingGoodId.ToString());
					Builder.AddInt64(Production.BlockingRequiredQuantity.GetRawValue());
					Builder.AddInt64(Production.BlockingAvailableQuantity.GetRawValue());
					Builder.AddUInt32(static_cast<uint32>(Production.InputReservations.Num()));
					for (const FHansaProductionInputReservation& Reservation : Production.InputReservations)
					{
						Builder.AddAsciiString(Reservation.GoodId.ToString());
						Builder.AddUInt64(Reservation.ReservationId.GetValue());
						Builder.AddUInt32(Reservation.ReservationId.GetGeneration());
						Builder.AddInt64(Reservation.Quantity.GetRawValue());
					}
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::TestEntities, State.TestEntities.Num(),
			[&State](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaTestEntityState& Entity : State.TestEntities)
				{
					Builder.AddUInt64(Entity.Id.GetValue());
					Builder.AddUInt32(Entity.Id.GetGeneration());
					Builder.AddUInt64(Entity.OwnerId.GetValue());
					Builder.AddUInt32(Entity.OwnerId.GetGeneration());
					Builder.AddInt64(Entity.Value);
				}
			}));

		uint32 PopulationRecordCount = static_cast<uint32>(State.PopulationCohorts.Num());
		for (const FHansaPopulationCohortState& Cohort : State.PopulationCohorts)
		{
			PopulationRecordCount += static_cast<uint32>(Cohort.Needs.Num());
		}
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Population, PopulationRecordCount,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(static_cast<uint32>(State.PopulationCohorts.Num()));
				for (const FHansaPopulationCohortState& Cohort : State.PopulationCohorts)
				{
					Builder.AddUInt64(Cohort.Id.GetValue());
					Builder.AddUInt32(Cohort.Id.GetGeneration());
					Builder.AddUInt64(Cohort.ResidenceBuildingId.GetValue());
					Builder.AddUInt32(Cohort.ResidenceBuildingId.GetGeneration());
					Builder.AddAsciiString(Cohort.CityId.ToString());
					Builder.AddUInt64(Cohort.ConsumptionInventoryId.GetValue());
					Builder.AddUInt32(Cohort.ConsumptionInventoryId.GetGeneration());
					Builder.AddAsciiString(Cohort.TierId.ToString());
					Builder.AddInt32(Cohort.Residents);
					Builder.AddInt32(Cohort.ResidenceCapacity);
					Builder.AddInt32(Cohort.PurchasingPowerBasisPoints);
					Builder.AddInt32(Cohort.ServiceAccessBasisPoints);
					Builder.AddInt32(Cohort.ServiceReliabilityBasisPoints);
					Builder.AddUInt8(Cohort.bResidenceOperational ? 1 : 0);
					Builder.AddUInt8(Cohort.bHasMarketAccess ? 1 : 0);
					Builder.AddInt32(Cohort.AccessBasisPoints);
					Builder.AddInt32(Cohort.AffordabilityBasisPoints);
					Builder.AddInt32(Cohort.ReliabilityBasisPoints);
					Builder.AddInt32(Cohort.SatisfactionBasisPoints);
					Builder.AddInt32(Cohort.WorkforceSupply);
					Builder.AddInt32(Cohort.ConsecutiveGrowthTicks);
					Builder.AddInt32(Cohort.ConsecutiveDeclineTicks);
					Builder.AddInt32(Cohort.ResidentChangeLastTick);
					Builder.AddUInt32(static_cast<uint32>(Cohort.Needs.Num()));
					for (const FHansaPopulationNeedState& Need : Cohort.Needs)
					{
						Builder.AddAsciiString(Need.NeedId.ToString());
						Builder.AddAsciiString(Need.GoodId.ToString());
						Builder.AddInt64(Need.RequiredLastTick.GetRawValue());
						Builder.AddInt64(Need.ConsumedLastTick.GetRawValue());
						Builder.AddInt32(Need.AccessBasisPoints);
						Builder.AddInt32(Need.AffordabilityBasisPoints);
						Builder.AddInt32(Need.ReliabilityBasisPoints);
						Builder.AddInt32(Need.SatisfactionBasisPoints);
						Builder.AddInt64(Need.ReserveMilliDays);
					}
				}
			}));

		uint32 MarketRecordCount = static_cast<uint32>(State.Markets.Num());
		for (const FHansaCityMarketState& Market : State.Markets)
		{
			MarketRecordCount += static_cast<uint32>(Market.PriceHistory.Num());
		}
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Market, MarketRecordCount,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddInt32(State.MarketSettings.UpdateCadenceTicks);
				Builder.AddInt32(State.MarketSettings.PriceHistoryCapacity);
				Builder.AddInt32(State.MarketSettings.TargetSmoothingBasisPoints);
				Builder.AddInt32(State.MarketSettings.MaximumMovementBasisPointsPerUpdate);
				Builder.AddInt32(State.MarketSettings.StaleAfterTicks);
				Builder.AddUInt32(static_cast<uint32>(State.Markets.Num()));
				for (const FHansaCityMarketState& Market : State.Markets)
				{
					Builder.AddAsciiString(Market.CityId.ToString());
					Builder.AddAsciiString(Market.GoodId.ToString());
					Builder.AddUInt32(static_cast<uint32>(Market.InventoryIds.Num()));
					for (const FHansaInventoryId InventoryId : Market.InventoryIds)
					{
						Builder.AddUInt64(InventoryId.GetValue());
						Builder.AddUInt32(InventoryId.GetGeneration());
					}
					Builder.AddInt64(Market.DesiredReserve.GetRawValue());
					Builder.AddInt64(Market.ConfirmedIncomingSupplyPerUpdate.GetRawValue());
					Builder.AddInt32(Market.SeasonModifierBasisPoints);
					Builder.AddInt32(Market.CityModifierBasisPoints);
					Builder.AddInt64(Market.MinimumPriceMilliMarks);
					Builder.AddInt64(Market.MaximumPriceMilliMarks);
					Builder.AddInt64(Market.CurrentPriceMilliMarks);
					Builder.AddInt64(Market.LastUpdateTick);
					Builder.AddInt64(Market.CurrentStock.GetRawValue());
					Builder.AddInt64(Market.CitizenDemand.GetRawValue());
					Builder.AddInt64(Market.IndustrialDemand.GetRawValue());
					Builder.AddInt64(Market.RecentLocalProduction.GetRawValue());
					Builder.AddInt64(Market.AccumulatedLocalProductionSinceUpdate.GetRawValue());
					Builder.AddInt64(Market.ExpectedIncomingSupply.GetRawValue());
					Builder.AddInt64(Market.UnmetDemand.GetRawValue());
					Builder.AddInt32(Market.MinimumConsumerAffordabilityBasisPoints);
					Builder.AddInt64(Market.ShortageSinceTick);
					Builder.AddInt64(Market.LowReserveSinceTick);
					Builder.AddInt64(Market.AffordabilitySinceTick);
					Builder.AddInt32(Market.Factors.ScarcityBasisPoints);
					Builder.AddInt32(Market.Factors.CitizenDemandBasisPoints);
					Builder.AddInt32(Market.Factors.IndustrialDemandBasisPoints);
					Builder.AddInt32(Market.Factors.IncomingSupplyBasisPoints);
					Builder.AddInt32(Market.Factors.UnmetDemandBasisPoints);
					Builder.AddInt32(Market.Factors.SeasonModifierBasisPoints);
					Builder.AddInt32(Market.Factors.CityModifierBasisPoints);
					Builder.AddInt32(Market.Factors.TargetMultiplierBasisPoints);
					Builder.AddUInt32(static_cast<uint32>(Market.PriceHistory.Num()));
					for (const FHansaMarketPriceHistoryEntry& Entry : Market.PriceHistory)
					{
						Builder.AddInt64(Entry.Tick.GetValue());
						Builder.AddInt64(Entry.Stock.GetRawValue());
						Builder.AddInt64(Entry.CitizenDemand.GetRawValue());
						Builder.AddInt64(Entry.IndustrialDemand.GetRawValue());
						Builder.AddInt64(Entry.LocalProduction.GetRawValue());
						Builder.AddInt64(Entry.ExpectedIncomingSupply.GetRawValue());
						Builder.AddInt64(Entry.UnmetDemand.GetRawValue());
						Builder.AddInt32(Entry.MinimumConsumerAffordabilityBasisPoints);
						Builder.AddInt64(Entry.PriceMilliMarks);
					}
				}
			}));

		uint32 PlacementRecordCount = static_cast<uint32>(
			State.Placement.Maps.Num() + State.Placement.Entitlements.Num() + State.Placement.Placements.Num());
		for (const FHansaPlacementMapInitialization& Map : State.Placement.Maps)
		{
			PlacementRecordCount += static_cast<uint32>(Map.Cells.Num());
		}
		for (const FHansaPlacedBuildingRecord& Placement : State.Placement.Placements)
		{
			PlacementRecordCount += static_cast<uint32>(Placement.OccupiedCells.Num());
		}
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Placement, PlacementRecordCount,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(static_cast<uint32>(State.Placement.Maps.Num()));
				for (const FHansaPlacementMapInitialization& Map : State.Placement.Maps)
				{
					Builder.AddAsciiString(Map.CityId.ToString());
					Builder.AddInt32(Map.BoundsMin.X);
					Builder.AddInt32(Map.BoundsMin.Y);
					Builder.AddInt32(Map.BoundsMax.X);
					Builder.AddInt32(Map.BoundsMax.Y);
					Builder.AddAsciiString(Map.RoadBuildingDefinitionId.ToString());
					Builder.AddUInt32(static_cast<uint32>(Map.Cells.Num()));
					for (const FHansaPlacementGridCell& Cell : Map.Cells)
					{
						Builder.AddInt32(Cell.Coordinate.X);
						Builder.AddInt32(Cell.Coordinate.Y);
						Builder.AddUInt8(static_cast<uint8>(Cell.Terrain));
						Builder.AddUInt64(Cell.OwnerId.GetValue());
						Builder.AddUInt32(Cell.OwnerId.GetGeneration());
						Builder.AddUInt8(Cell.bBlocked ? 1 : 0);
					}
				}
				Builder.AddUInt32(static_cast<uint32>(State.Placement.Entitlements.Num()));
				for (const FHansaPlacementEntitlement& Entitlement : State.Placement.Entitlements)
				{
					Builder.AddUInt64(Entitlement.HouseId.GetValue());
					Builder.AddUInt32(Entitlement.HouseId.GetGeneration());
					Builder.AddAsciiString(Entitlement.BuildingDefinitionId.ToString());
				}
				Builder.AddUInt32(static_cast<uint32>(State.Placement.Placements.Num()));
				for (const FHansaPlacedBuildingRecord& Placement : State.Placement.Placements)
				{
					Builder.AddUInt64(Placement.BuildingId.GetValue());
					Builder.AddUInt32(Placement.BuildingId.GetGeneration());
					Builder.AddUInt64(Placement.OwnerId.GetValue());
					Builder.AddUInt32(Placement.OwnerId.GetGeneration());
					Builder.AddAsciiString(Placement.Spec.CityId.ToString());
					Builder.AddAsciiString(Placement.Spec.BuildingDefinitionId.ToString());
					Builder.AddInt32(Placement.Spec.Anchor.X);
					Builder.AddInt32(Placement.Spec.Anchor.Y);
					Builder.AddUInt8(static_cast<uint8>(Placement.Spec.Rotation));
					Builder.AddUInt32(static_cast<uint32>(Placement.OccupiedCells.Num()));
					for (const FHansaGridCoordinate Cell : Placement.OccupiedCells)
					{
						Builder.AddInt32(Cell.X);
						Builder.AddInt32(Cell.Y);
					}
				}
			}));

		const uint32 LogisticsRecordCount = static_cast<uint32>(
			1 + State.LocalLogisticsRequests.Num() + State.LocalLogisticsJobs.Num());
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Logistics, LogisticsRecordCount,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddInt64(State.LocalLogisticsSettings.JobCapacity.GetRawValue());
				Builder.AddInt32(State.LocalLogisticsSettings.PickupDelayTicks);
				Builder.AddInt32(State.LocalLogisticsSettings.TicksPerRoadCell);
				Builder.AddInt32(State.LocalLogisticsSettings.MaximumConcurrentJobs);
				Builder.AddUInt64(State.NextLogisticsJobValue);
				Builder.AddUInt64(State.NextLogisticsReservationValue);
				Builder.AddUInt32(static_cast<uint32>(State.LocalLogisticsRequests.Num()));
				for (const FHansaLogisticsRequestState& Request : State.LocalLogisticsRequests)
				{
					Builder.AddUInt64(Request.Id.GetValue());
					Builder.AddUInt32(Request.Id.GetGeneration());
					Builder.AddUInt64(Request.SourceInventoryId.GetValue());
					Builder.AddUInt32(Request.SourceInventoryId.GetGeneration());
					Builder.AddUInt64(Request.DestinationInventoryId.GetValue());
					Builder.AddUInt32(Request.DestinationInventoryId.GetGeneration());
					Builder.AddAsciiString(Request.GoodId.ToString());
					Builder.AddInt64(Request.RequestedQuantity.GetRawValue());
					Builder.AddInt64(Request.RemainingQuantity.GetRawValue());
					Builder.AddInt64(Request.InFlightQuantity.GetRawValue());
					Builder.AddUInt8(static_cast<uint8>(Request.Priority));
					Builder.AddUInt8(static_cast<uint8>(Request.Status));
					Builder.AddUInt8(static_cast<uint8>(Request.Bottleneck));
					Builder.AddInt64(Request.CreatedTick.GetValue());
				}
				Builder.AddUInt32(static_cast<uint32>(State.LocalLogisticsJobs.Num()));
				for (const FHansaLogisticsJobState& Job : State.LocalLogisticsJobs)
				{
					Builder.AddUInt64(Job.Id.GetValue());
					Builder.AddUInt32(Job.Id.GetGeneration());
					Builder.AddUInt64(Job.RequestId.GetValue());
					Builder.AddUInt32(Job.RequestId.GetGeneration());
					Builder.AddUInt64(Job.SourceReservationId.GetValue());
					Builder.AddUInt32(Job.SourceReservationId.GetGeneration());
					Builder.AddUInt64(Job.SourceInventoryId.GetValue());
					Builder.AddUInt32(Job.SourceInventoryId.GetGeneration());
					Builder.AddUInt64(Job.DestinationInventoryId.GetValue());
					Builder.AddUInt32(Job.DestinationInventoryId.GetGeneration());
					Builder.AddAsciiString(Job.GoodId.ToString());
					Builder.AddInt64(Job.Quantity.GetRawValue());
					Builder.AddInt64(Job.CargoQuantity.GetRawValue());
					Builder.AddInt64(Job.DispatchTick.GetValue());
					Builder.AddInt64(Job.PickupTick.GetValue());
					Builder.AddInt64(Job.DeliveryTick.GetValue());
					Builder.AddInt32(Job.RoadDistanceCells);
					Builder.AddUInt8(static_cast<uint8>(Job.Status));
				}
			}));

		FNormalizedHashBuilder Overall;
		Overall.AddUInt32(Report.HashFormatVersion);
		Overall.AddUInt32(Report.NormalizationVersion);
		Overall.AddUInt32(Report.SystemPipelineVersion);
		Overall.AddUInt32(static_cast<uint32>(Report.Subsystems.Num()));
		for (const FHansaSubsystemStateHash& Hash : Report.Subsystems)
		{
			Overall.AddUInt8(static_cast<uint8>(Hash.Subsystem));
			Overall.AddUInt64(Hash.Value);
			Overall.AddUInt32(Hash.RecordCount);
		}
		Report.OverallHash = Overall.Get();
		return Report;
	}
}
