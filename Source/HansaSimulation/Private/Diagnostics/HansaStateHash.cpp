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
		FHansaSubsystemStateHash BuildRawSubsystem(
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
		case EHansaStateHashSubsystem::Research: return TEXT("Research");
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
        return ComputeVersion(State, Definitions, FHansaSimulationState::DeterminismFingerprintVersion);
    }

    FHansaStateHashReport FHansaStateHasher::ComputeLegacyV16(const FHansaSimulationState& State,
        const FHansaSimulationDefinitionContext& Definitions)
    {
        return ComputeVersion(State, Definitions, 16);
    }

    FHansaStateHashReport FHansaStateHasher::ComputeLegacyV17(const FHansaSimulationState& State,
        const FHansaSimulationDefinitionContext& Definitions)
    {
        return ComputeVersion(State, Definitions, 17);
    }

    FHansaStateHashReport FHansaStateHasher::ComputeLegacyV18(const FHansaSimulationState& State,
        const FHansaSimulationDefinitionContext& Definitions)
    {
        return ComputeVersion(State, Definitions, 18);
    }

	FHansaStateHashReport FHansaStateHasher::ComputeLegacyV19(
		const FHansaSimulationState& State,
		const FHansaSimulationDefinitionContext& Definitions)
	{
		return ComputeVersion(State, Definitions, 19);
	}

    FHansaStateHashReport FHansaStateHasher::ComputeSavedVersion(const FHansaSimulationState& State,
        const FHansaSimulationDefinitionContext& Definitions, uint32 Version)
    { return ComputeVersion(State,Definitions,Version); }
    FHansaStateHashReport FHansaStateHasher::ComputeVersion(const FHansaSimulationState& State,
        const FHansaSimulationDefinitionContext& Definitions, const uint32 FingerprintVersion)
    {
        check(State.bInitialized);
		check(Definitions.IsValid());

		FHansaStateHashReport Report;
		Report.SystemPipelineVersion = FHansaSimulationState::CurrentSystemPipelineVersion;
		Report.Tick = State.Clock.GetTick();
		Report.Subsystems.Reserve(16);

		const bool bUseCache = FingerprintVersion == FHansaSimulationState::DeterminismFingerprintVersion;
		const uint64 TopologyHash = State.Placement.GetTopologyHash();
		if (bUseCache &&
			(State.CachedHashScenarioId != Definitions.GetScenarioId() ||
				State.CachedHashDefinitionHash != Definitions.GetDefinitionHash() ||
				State.CachedHashTopologyHash != TopologyHash))
		{
			State.CachedStateHashValidMask &= ~(
				(1U << static_cast<uint8>(EHansaStateHashSubsystem::Contract)) |
				(1U << static_cast<uint8>(EHansaStateHashSubsystem::Placement)));
			State.CachedHashScenarioId = Definitions.GetScenarioId();
			State.CachedHashDefinitionHash = Definitions.GetDefinitionHash();
			State.CachedHashTopologyHash = TopologyHash;
		}
		const auto BuildSubsystem = [&State, &Report, bUseCache](
			const EHansaStateHashSubsystem Subsystem,
			const uint32 RecordCount,
			auto&& Populate)
		{
			const uint32 Index = static_cast<uint32>(Subsystem);
			const uint32 Bit = 1U << Index;
			if (bUseCache && (State.CachedStateHashValidMask & Bit) != 0)
			{
				return FHansaSubsystemStateHash {
					Subsystem,
					State.CachedStateHashValues[Index],
					State.CachedStateHashRecordCounts[Index]
				};
			}
			FHansaSubsystemStateHash Result = BuildRawSubsystem(Subsystem, RecordCount, Populate);
			++Report.RecomputedSubsystemCount;
			if (bUseCache)
			{
				State.CachedStateHashValues[Index] = Result.Value;
				State.CachedStateHashRecordCounts[Index] = Result.RecordCount;
				State.CachedStateHashValidMask |= Bit;
			}
			return Result;
		};

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Contract, 1,
			[&Definitions, &State, FingerprintVersion](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(FingerprintVersion);
				Builder.AddUInt32(FHansaSimulationState::CurrentSystemPipelineVersion);
				Builder.AddAsciiString(Definitions.GetScenarioId().ToString());
				Builder.AddUInt64(Definitions.GetDefinitionHash());
				if (FingerprintVersion >= 20)
				{
					Builder.AddUInt64(State.Placement.GetTopologyHash());
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::SimulationMetadata, 1,
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
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
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaRandomStream& Stream : State.RandomStreams)
				{
					Builder.AddAsciiString(Stream.GetName());
					Builder.AddUInt8(static_cast<uint8>(Stream.GetAlgorithm()));
					Builder.AddUInt64(Stream.GetState());
					Builder.AddUInt64(Stream.GetDrawCount());
				}
			}));

		const uint32 HouseRecordCount = State.Houses.Num() + (FingerprintVersion >= 25 ? State.ForeignPresences.Num() : 0) +
			(FingerprintVersion >= 27 ? State.TradeStations.Num() + State.LeasedPlots.Num() : 0);
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Houses, HouseRecordCount,
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaHouseState& House : State.Houses)
				{
					Builder.AddUInt64(House.Id.GetValue()); Builder.AddUInt32(House.Id.GetGeneration()); Builder.AddInt64(House.Money.GetRawValue());
				}
				if (FingerprintVersion >= 25) for (const FHansaForeignPresenceState& Presence : State.ForeignPresences)
				{
					Builder.AddUInt64(Presence.HouseId.GetValue()); Builder.AddUInt32(Presence.HouseId.GetGeneration());
					Builder.AddAsciiString(Presence.CityId.ToString()); Builder.AddAsciiString(Presence.CurrentStageId);
					Builder.AddUInt32(static_cast<uint32>(Presence.GrantedCapabilityIds.Num()));
					for (const FString& CapabilityId : Presence.GrantedCapabilityIds) Builder.AddAsciiString(CapabilityId);
					Builder.AddInt64(Presence.Contributions.LawfulTradeVolumeMilliUnits); Builder.AddInt64(Presence.Contributions.CompletedDeliveryCount); Builder.AddInt64(Presence.Contributions.InvestedPfennig);
					if(FingerprintVersion>=30){Builder.AddUInt32(Presence.ActiveSpecializationIds.Num());for(const FString& SpecializationId:Presence.ActiveSpecializationIds)Builder.AddAsciiString(SpecializationId);Builder.AddInt64(Presence.SpecializationRevision);}if(FingerprintVersion>=29){Builder.AddInt64(Presence.Contributions.TransactionValuePfennig);Builder.AddInt64(Presence.Contributions.FulfilledShortageMilliUnits);Builder.AddInt64(Presence.Contributions.ReliableOperatingTicks);Builder.AddInt64(Presence.Contributions.SolventOperatingTicks);Builder.AddUInt64(Presence.LastAcceptedContributionEventSequence);Builder.AddUInt8(static_cast<uint8>(Presence.Upgrade.Status));Builder.AddAsciiString(Presence.Upgrade.TargetStageId);Builder.AddUInt64(Presence.Upgrade.FundingInventoryId.GetValue());Builder.AddUInt32(Presence.Upgrade.FundingInventoryId.GetGeneration());Builder.AddInt64(Presence.Upgrade.RequestedTick.GetValue());Builder.AddInt64(Presence.Upgrade.FundedTick.GetValue());Builder.AddInt64(Presence.Upgrade.CompletionTick.GetValue());Builder.AddInt64(Presence.Upgrade.SpentMoneyPfennig);Builder.AddUInt32(Presence.History.Num());for(const auto& Entry:Presence.History){Builder.AddUInt8(static_cast<uint8>(Entry.Kind));Builder.AddInt64(Entry.Tick.GetValue());Builder.AddUInt64(Entry.SourceEventSequence);Builder.AddAsciiString(Entry.StageId);Builder.AddInt64(Entry.QuantityMilliUnits);Builder.AddInt64(Entry.MoneyPfennig);}}
					if(FingerprintVersion>=32){Builder.AddUInt32(Presence.Privileges.Num());for(const auto& V:Presence.Privileges){Builder.AddAsciiString(V.PrivilegeId);Builder.AddUInt64(V.GrantedLeaseId.GetValue());Builder.AddUInt8((uint8)V.Status);Builder.AddInt64(V.GrantedTick.GetValue());Builder.AddInt64(V.ExpiryTick.GetValue());Builder.AddInt64(V.SpentMoneyPfennig);}Builder.AddUInt32(Presence.CityProjects.Num());for(const auto& V:Presence.CityProjects){Builder.AddAsciiString(V.ProjectId);Builder.AddUInt8((uint8)V.Status);Builder.AddInt64(V.FundedTick.GetValue());Builder.AddInt64(V.CompletionTick.GetValue());Builder.AddInt64(V.SpentMoneyPfennig);Builder.AddUInt8(V.bSharedEffectApplied?1:0);}Builder.AddUInt8(Presence.bGovernanceAuthority?1:0);Builder.AddAsciiString(Presence.GovernanceCharterId);Builder.AddInt64(Presence.GovernanceGrantedTick.GetValue());Builder.AddInt64(Presence.AuthorityRevision);}
					Builder.AddUInt8(static_cast<uint8>(Presence.Status)); Builder.AddInt64(Presence.EstablishedTick.GetValue()); Builder.AddInt64(Presence.LastUpgradeTick.GetValue());
					Builder.AddUInt64(Presence.StationId.GetValue()); Builder.AddUInt32(Presence.StationId.GetGeneration()); Builder.AddUInt64(Presence.LeasedPlotId.GetValue()); Builder.AddUInt32(Presence.LeasedPlotId.GetGeneration());
				}
				if (FingerprintVersion >= 27) for (const FHansaTradeStationState& Station : State.TradeStations)
				{
					Builder.AddUInt64(Station.Id.GetValue()); Builder.AddUInt32(Station.Id.GetGeneration()); Builder.AddUInt64(Station.OwnerId.GetValue()); Builder.AddUInt32(Station.OwnerId.GetGeneration());
					Builder.AddAsciiString(Station.CityId.ToString()); Builder.AddAsciiString(Station.SiteId); Builder.AddUInt64(Station.InventoryId.GetValue()); Builder.AddUInt32(Station.InventoryId.GetGeneration());
					Builder.AddUInt64(Station.FactorId.GetValue()); Builder.AddUInt32(Station.FactorId.GetGeneration()); Builder.AddUInt64(Station.LeasedPlotId.GetValue()); Builder.AddUInt32(Station.LeasedPlotId.GetGeneration());
					Builder.AddUInt8(static_cast<uint8>(Station.Status)); Builder.AddInt64(Station.ProposedTick.GetValue()); Builder.AddInt64(Station.FundedTick.GetValue()); Builder.AddInt64(Station.CompletionTick.GetValue()); Builder.AddInt64(Station.CompletedTick.GetValue());
					Builder.AddInt64(Station.UpkeepPfennigPerTick); Builder.AddInt64(Station.SpentMoneyRaw); Builder.AddUInt64(Station.FundingInventoryId.GetValue()); Builder.AddUInt32(Station.FundingInventoryId.GetGeneration());
					Builder.AddUInt32(Station.SpentGoods.Num()); for (const auto& Cost : Station.SpentGoods) { Builder.AddAsciiString(Cost.GoodId.ToString()); Builder.AddInt64(Cost.Quantity.GetRawValue()); }
                    if (FingerprintVersion >= 28) {
                        Builder.AddUInt32(Station.Orders.Num());
                        for (const auto& O : Station.Orders) {
                            Builder.AddUInt64(O.Id); Builder.AddUInt64(O.LastCommandId.GetValue()); Builder.AddUInt32(O.LastCommandId.GetGeneration());
                            Builder.AddAsciiString(O.Terms.GoodId.ToString()); Builder.AddUInt8(static_cast<uint8>(O.Terms.Side));
                            Builder.AddInt64(O.Terms.TargetOrReserveMilliUnits); Builder.AddInt64(O.Terms.CapMilliUnits); Builder.AddInt64(O.Terms.TotalBudgetPfennig);
                            if(FingerprintVersion>=30){Builder.AddInt64(O.Terms.LimitUnitPriceMilliMarks);Builder.AddInt64(O.Terms.ReviewedMarketUpdateTick);Builder.AddInt64(O.Terms.ReviewedUnitPriceMilliMarks);}
                            Builder.AddUInt8(O.bPaused); Builder.AddUInt8(O.bCancelled); Builder.AddInt64(O.SpentPfennig); Builder.AddInt64(O.NextUpdateTick);
                            Builder.AddUInt32(O.History.Num());
                            for (const auto& E : O.History) {
                                Builder.AddInt64(E.Tick); Builder.AddInt64(E.MarketUpdateTick); Builder.AddInt64(E.RequestedMilliUnits); Builder.AddInt64(E.AppliedMilliUnits);
                                Builder.AddInt64(E.UnitPriceMilliMarks); Builder.AddInt64(E.MoneyDelta); Builder.AddUInt64(E.FirstMovementSequence); Builder.AddUInt64(E.LastMovementSequence);
                                Builder.AddUInt8(static_cast<uint8>(E.Outcome)); Builder.AddUInt8(static_cast<uint8>(E.Blocker));
                            }
                        }
                    }
					if (FingerprintVersion >= 33) { Builder.AddUInt8(static_cast<uint8>(Station.OperationalState)); Builder.AddInt64(Station.OperationalStateChangedTick.GetValue()); Builder.AddInt64(Station.OutstandingUpkeepPfennig); }
				}
				if (FingerprintVersion >= 27) for (const FHansaLeasedPlotState& Lease : State.LeasedPlots)
				{
					Builder.AddUInt64(Lease.Id.GetValue()); Builder.AddUInt32(Lease.Id.GetGeneration()); Builder.AddUInt64(Lease.StationId.GetValue()); Builder.AddUInt32(Lease.StationId.GetGeneration());
					Builder.AddUInt64(Lease.OwnerId.GetValue()); Builder.AddUInt32(Lease.OwnerId.GetGeneration()); Builder.AddAsciiString(Lease.CityId.ToString()); Builder.AddAsciiString(Lease.SiteId); Builder.AddAsciiString(Lease.PlotCategory);
					if (FingerprintVersion >= 31) { Builder.AddInt64(Lease.BoundsMin.X); Builder.AddInt64(Lease.BoundsMin.Y); Builder.AddInt64(Lease.BoundsMax.X); Builder.AddInt64(Lease.BoundsMax.Y); Builder.AddUInt32(Lease.PermittedBuildingCategories.Num()); for (const FString& Category : Lease.PermittedBuildingCategories) Builder.AddAsciiString(Category); Builder.AddUInt32(Lease.OccupyingBuildingIds.Num()); for (const FHansaBuildingId BuildingId : Lease.OccupyingBuildingIds) { Builder.AddUInt64(BuildingId.GetValue()); Builder.AddUInt32(BuildingId.GetGeneration()); } }
					Builder.AddUInt8(Lease.bActive ? 1 : 0); Builder.AddUInt8(Lease.bOccupied ? 1 : 0);
				}
			}));
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Cities, State.Cities.Num(),
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaCityState& City : State.Cities)
				{
					Builder.AddAsciiString(City.DefinitionId.ToString());
					Builder.AddInt64(City.AggregateStock.GetRawValue());
                    if (FingerprintVersion >= 22) Builder.AddUInt8(City.bPreservedFishHouseholdAvailable ? 1 : 0);
					if (City.HeatingReserveDays != -1 || City.bReleaseHeatingReserve)
					{
						Builder.AddInt64(City.HeatingReserveDays);
						Builder.AddInt64(City.bReleaseHeatingReserve ? 1 : 0);
					}
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
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
			{
				for (const FHansaVehicleState& Vehicle : State.Vehicles)
				{
					Builder.AddUInt64(Vehicle.Id.GetValue());
					Builder.AddUInt32(Vehicle.Id.GetGeneration());
					Builder.AddAsciiString(Vehicle.DefinitionId.ToString());
					Builder.AddUInt64(Vehicle.OwnerId.GetValue());
					Builder.AddUInt32(Vehicle.OwnerId.GetGeneration());
					Builder.AddInt64(Vehicle.Cargo.GetRawValue());
					Builder.AddUInt64(Vehicle.CargoInventoryId.GetValue());
					Builder.AddUInt32(Vehicle.CargoInventoryId.GetGeneration());
					Builder.AddUInt8(static_cast<uint8>(Vehicle.Mode));
					Builder.AddInt64(Vehicle.Capacity.GetRawValue());
					Builder.AddAsciiString(Vehicle.CurrentCityId.ToString());
					Builder.AddInt64(Vehicle.UpkeepPfennigPerTravelTick);
					Builder.AddInt64(Vehicle.AccruedUpkeepPfennig);
					if (FingerprintVersion >= 26)
					{
						const FHansaSpotTradeRecord& T=Vehicle.LastSpotTrade;
						Builder.AddUInt64(T.CommandId.GetValue()); Builder.AddUInt32(T.CommandId.GetGeneration()); Builder.AddInt64(T.Tick.GetValue());
						Builder.AddUInt64(T.HouseId.GetValue()); Builder.AddUInt32(T.HouseId.GetGeneration()); Builder.AddUInt64(T.VehicleId.GetValue()); Builder.AddUInt32(T.VehicleId.GetGeneration());
						Builder.AddAsciiString(T.CityId.ToString()); Builder.AddAsciiString(T.GoodId.ToString()); Builder.AddUInt8(static_cast<uint8>(T.Side));
						Builder.AddInt64(T.RequestedQuantity.GetRawValue()); Builder.AddInt64(T.AppliedQuantity.GetRawValue()); Builder.AddUInt8(static_cast<uint8>(T.Outcome)); Builder.AddUInt8(static_cast<uint8>(T.Blocker));
						Builder.AddInt64(T.MarketUpdateTick); Builder.AddInt64(T.UnitPriceMilliMarks); Builder.AddInt64(T.SettledMoneyRaw);
					}                    if (FingerprintVersion >= 23)
                    {
                        const auto& N=Vehicle.Navigation;
                        Builder.AddAsciiString(N.CityId.ToString());
                        Builder.AddInt32(N.Home.X);Builder.AddInt32(N.Home.Y);
                        Builder.AddInt32(N.Cell.X);Builder.AddInt32(N.Cell.Y);
                        Builder.AddInt32(N.NextIndex);Builder.AddInt32(N.Path.Num());
                        for (auto C:N.Path) { Builder.AddInt32(C.X);Builder.AddInt32(C.Y); }
                    }
				}
			}));

		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Routes, State.Routes.Num(),
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
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
					Builder.AddAsciiString(Route.RouteDefinitionId.ToString());
					Builder.AddUInt8(static_cast<uint8>(Route.Mode));
					Builder.AddUInt32(static_cast<uint32>(Route.Stops.Num()));
					for (const FHansaRouteStop& Stop : Route.Stops)
					{
						Builder.AddAsciiString(Stop.CityId.ToString());
						Builder.AddUInt32(static_cast<uint32>(Stop.Actions.Num()));
						for (const FHansaRouteCargoAction& Action : Stop.Actions)
						{
							Builder.AddUInt8(static_cast<uint8>(Action.Kind));
							Builder.AddUInt8(static_cast<uint8>(Action.Condition));
							Builder.AddAsciiString(Action.GoodId.ToString());
							Builder.AddInt64(Action.QuantityLimit.GetRawValue());
							Builder.AddInt64(Action.MinimumSourceReserve.GetRawValue());
						}
					}
					Builder.AddUInt8(static_cast<uint8>(Route.Lifecycle));
					Builder.AddInt32(Route.CurrentStopIndex);
					Builder.AddInt32(Route.NextStopIndex);
					Builder.AddInt32(Route.RemainingTravelTicks);
					Builder.AddInt32(Route.TotalTravelTicks);
					Builder.AddUInt8(Route.bPendingStopActions ? 1 : 0);
					Builder.AddInt64(Route.CompletedLegCount);
					Builder.AddInt64(Route.MissedCargoActionCount);
					Builder.AddInt64(Route.LastTransfer.Tick.GetValue());
					Builder.AddInt32(Route.LastTransfer.StopIndex);
					Builder.AddInt32(Route.LastTransfer.ActionIndex);
					Builder.AddUInt8(static_cast<uint8>(Route.LastTransfer.Kind));
					Builder.AddAsciiString(Route.LastTransfer.CityId.ToString());
					Builder.AddAsciiString(Route.LastTransfer.GoodId.ToString());
					Builder.AddInt64(Route.LastTransfer.RequestedQuantity.GetRawValue());
					Builder.AddInt64(Route.LastTransfer.AppliedQuantity.GetRawValue());
					Builder.AddUInt8(static_cast<uint8>(Route.LastTransfer.Outcome));
                    if (FingerprintVersion >= 22) { Builder.AddInt64(Route.LastTransfer.UnitPriceMilliMarks); Builder.AddInt64(Route.LastTransfer.SettledMoneyRaw); }
				}
			}));

		const uint32 InventoryRecordCount = static_cast<uint32>(
			State.InventoryLedger.Inventories.Num() +
			State.InventoryLedger.Reservations.Num() +
			State.InventoryLedger.RecentMovements.Num());
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Inventories, InventoryRecordCount,
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(static_cast<uint32>(State.InventoryLedger.MovementCapacity));
				Builder.AddUInt64(State.InventoryLedger.LastMovementSequence);
                if (FingerprintVersion >= 22)
                {
                    Builder.AddUInt32(State.InventoryLedger.Spoilage.Num());
                    for (const auto& Loss : State.InventoryLedger.Spoilage)
                    { Builder.AddAsciiString(Loss.GoodId.ToString()); Builder.AddInt64(Loss.RemainderNumerator); Builder.AddInt64(Loss.DestroyedMilliUnits); }
                }
				Builder.AddUInt32(static_cast<uint32>(State.InventoryLedger.Inventories.Num()));
				for (const FHansaInventoryRecord& Inventory : State.InventoryLedger.Inventories)
				{
					Builder.AddUInt64(Inventory.Id.GetValue());
					Builder.AddUInt32(Inventory.Id.GetGeneration());
					Builder.AddUInt8(static_cast<uint8>(Inventory.OwnerKind));
					Builder.AddAsciiString(Inventory.CityId.ToString());
					Builder.AddUInt64(Inventory.BuildingId.GetValue());
					Builder.AddUInt32(Inventory.BuildingId.GetGeneration());
					Builder.AddUInt64(Inventory.VehicleId.GetValue());
					Builder.AddUInt32(Inventory.VehicleId.GetGeneration());
					if (FingerprintVersion >= 27) { Builder.AddUInt64(Inventory.TradeStationId.GetValue()); Builder.AddUInt32(Inventory.TradeStationId.GetGeneration()); }
					Builder.AddInt64(Inventory.Capacity.GetRawValue());
					Builder.AddUInt32(static_cast<uint32>(Inventory.AcceptedGoods.Num()));
					for (const FHansaGoodId& GoodId : Inventory.AcceptedGoods)
					{
						Builder.AddAsciiString(GoodId.ToString());
					}
					if (FingerprintVersion >= 22)
                    {
                        Builder.AddUInt32(Inventory.HouseholdExcludedGoods.Num());
                        for (const auto Good : Inventory.HouseholdExcludedGoods) Builder.AddAsciiString(Good.ToString());
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
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
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
					if (FingerprintVersion >= 22)
                    {
                        Builder.AddAsciiString(Production.RequestedRecipeId.ToString());
                        Builder.AddUInt8(Production.bFallbackToFresh ? 1 : 0);
                        Builder.AddAsciiString(Production.PendingUpgradeBuildingId.ToString());
                        Builder.AddUInt32(Production.OutputTotals.Num());
                        for(const auto& Total:Production.OutputTotals){Builder.AddAsciiString(Total.GoodId.ToString());Builder.AddInt64(Total.QuantityMilliUnits);}
                    }
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
            if (FingerprintVersion >= 18)
                for (const auto& Sample : Cohort.ConsumptionHistory.Samples) PopulationRecordCount += 1 + Sample.Goods.Num();
		}
        if (FingerprintVersion >= 17)
            for (const auto& Sample : State.ConsumptionHistory.Samples) PopulationRecordCount += 1 + Sample.Goods.Num();
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Population, PopulationRecordCount,
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
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
                        if (FingerprintVersion >= 22)
                        {
                            Builder.AddUInt32(Need.SuppliedGoods.Num());
                            for (const auto& Supply : Need.SuppliedGoods)
                            {
                                Builder.AddAsciiString(Supply.GoodId.ToString());
                                Builder.AddInt64(Supply.QuantityMilliUnits);
                                Builder.AddInt64(Supply.FulfillmentMilliUnits);
                            }
                        }
					}
                    if (FingerprintVersion >= 18)
                    {
                        Builder.AddUInt32(Cohort.ConsumptionHistory.Samples.Num());
                        for (const auto& Sample : Cohort.ConsumptionHistory.Samples)
                        {
                            Builder.AddInt64(Sample.EndTick);
                            Builder.AddUInt32(Sample.Goods.Num());
                            for (const auto& Good : Sample.Goods)
                            {
                                Builder.AddAsciiString(Good.CityId.ToString());
                                Builder.AddAsciiString(Good.GoodId.ToString());
                                Builder.AddInt64(Good.Required);
                                Builder.AddInt64(Good.Consumed);
                                if (FingerprintVersion >= 22)
                                {
                                    Builder.AddUInt32(Good.SuppliedGoods.Num());
                                    for (const auto& G:Good.SuppliedGoods)
                                    {Builder.AddAsciiString(G.GoodId.ToString());Builder.AddInt64(G.QuantityMilliUnits);Builder.AddInt64(G.FulfillmentMilliUnits);}
                                }
                            }
                        }
                    }
				}
                if (FingerprintVersion >= 17)
                {
                    Builder.AddUInt32(State.ConsumptionHistory.Samples.Num());
                    for (const auto& Sample : State.ConsumptionHistory.Samples)
                    {
                        Builder.AddInt64(Sample.EndTick);
                        Builder.AddUInt32(Sample.Goods.Num());
                        for (const auto& Good : Sample.Goods)
                        {
                            Builder.AddAsciiString(Good.CityId.ToString());
                            Builder.AddAsciiString(Good.GoodId.ToString());
                            Builder.AddInt64(Good.Required);
                            Builder.AddInt64(Good.Consumed);
                                if (FingerprintVersion >= 22)
                                {
                                    Builder.AddUInt32(Good.SuppliedGoods.Num());
                                    for (const auto& G:Good.SuppliedGoods)
                                    {Builder.AddAsciiString(G.GoodId.ToString());Builder.AddInt64(G.QuantityMilliUnits);Builder.AddInt64(G.FulfillmentMilliUnits);}
                                }
                        }
                    }
                }
			}));

		uint32 MarketRecordCount = static_cast<uint32>(State.Markets.Num());
		for (const FHansaCityMarketState& Market : State.Markets)
		{
			MarketRecordCount += static_cast<uint32>(Market.PriceHistory.Num() + Market.Report.PriceHistory.Num());
		}
		if(FingerprintVersion>=24) MarketRecordCount+=static_cast<uint32>(State.RemoteIndustries.Num()+State.RegionalShipments.Num());
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Market, MarketRecordCount,
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
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
					Builder.AddUInt8(Market.bMarketOnly ? 1 : 0);
					Builder.AddInt64(Market.BackgroundProductionPerUpdate.GetRawValue());
					Builder.AddInt64(Market.BackgroundCitizenDemandPerUpdate.GetRawValue());
					Builder.AddInt64(Market.BackgroundIndustrialDemandPerUpdate.GetRawValue());
					Builder.AddInt32(Market.ReportPolicy.ReportCadenceTicks);
					Builder.AddInt32(Market.ReportPolicy.CurrentMaxAgeTicks);
					Builder.AddInt32(Market.ReportPolicy.RecentMaxAgeTicks);
					Builder.AddInt32(Market.ReportPolicy.StaleMaxAgeTicks);
					Builder.AddInt32(Market.ReportPolicy.EstimatedMaxAgeTicks);
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
					Builder.AddUInt8(Market.Report.bAvailable ? 1 : 0);
					Builder.AddInt64(Market.Report.ReportTick);
					Builder.AddInt64(Market.Report.MarketUpdateTick);
					Builder.AddInt64(Market.Report.Stock.GetRawValue());
					Builder.AddInt64(Market.Report.DesiredReserve.GetRawValue());
					Builder.AddInt64(Market.Report.CitizenDemand.GetRawValue());
					Builder.AddInt64(Market.Report.IndustrialDemand.GetRawValue());
					Builder.AddInt64(Market.Report.RecentLocalProduction.GetRawValue());
					Builder.AddInt64(Market.Report.ExpectedIncomingSupply.GetRawValue());
					Builder.AddInt64(Market.Report.UnmetDemand.GetRawValue());
					Builder.AddInt64(Market.Report.PriceMilliMarks);
					Builder.AddInt32(Market.Report.Factors.ScarcityBasisPoints);
					Builder.AddInt32(Market.Report.Factors.CitizenDemandBasisPoints);
					Builder.AddInt32(Market.Report.Factors.IndustrialDemandBasisPoints);
					Builder.AddInt32(Market.Report.Factors.IncomingSupplyBasisPoints);
					Builder.AddInt32(Market.Report.Factors.UnmetDemandBasisPoints);
					Builder.AddInt32(Market.Report.Factors.SeasonModifierBasisPoints);
					Builder.AddInt32(Market.Report.Factors.CityModifierBasisPoints);
					Builder.AddInt32(Market.Report.Factors.TargetMultiplierBasisPoints);
					Builder.AddUInt32(static_cast<uint32>(Market.Report.PriceHistory.Num()));
					for (const FHansaMarketPriceHistoryEntry& Entry : Market.Report.PriceHistory)
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
				if(FingerprintVersion>=24)
				{
					Builder.AddUInt64(State.NextRegionalShipmentSequence);
					Builder.AddUInt32(static_cast<uint32>(State.RemoteIndustries.Num()));
					for(const auto& Industry:State.RemoteIndustries)
					{
						Builder.AddAsciiString(Industry.CityId.ToString());Builder.AddAsciiString(Industry.ProductionChainId);Builder.AddAsciiString(Industry.StageKey);Builder.AddAsciiString(Industry.RecipeId);Builder.AddInt64(Industry.CompletedCycles);Builder.AddUInt8(static_cast<uint8>(Industry.Blocker));Builder.AddAsciiString(Industry.BlockingGoodId.ToString());Builder.AddInt64(Industry.BlockingRequired.GetRawValue());Builder.AddInt64(Industry.BlockingAvailable.GetRawValue());Builder.AddInt64(Industry.LastProduced.GetRawValue());Builder.AddInt64(Industry.LastUpdateTick.GetValue());
					}
					Builder.AddUInt32(static_cast<uint32>(State.RegionalShipments.Num()));
					for(const auto& Shipment:State.RegionalShipments)
					{
						Builder.AddUInt64(Shipment.Sequence);Builder.AddAsciiString(Shipment.RegionId);Builder.AddAsciiString(Shipment.SourceCityId.ToString());Builder.AddAsciiString(Shipment.DestinationCityId.ToString());Builder.AddAsciiString(Shipment.GoodId.ToString());Builder.AddInt64(Shipment.CommittedQuantity.GetRawValue());Builder.AddInt64(Shipment.DeliverableQuantity.GetRawValue());Builder.AddInt64(Shipment.DispatchTick.GetValue());Builder.AddInt64(Shipment.DeliveryTick.GetValue());Builder.AddInt64(Shipment.TransportCostMilliMarks);
					}
				}
			}));

		uint32 PlacementRecordCount = State.Placement.GetTopologyRecordCount() +
			static_cast<uint32>(State.Placement.Entitlements.Num() + State.Placement.Placements.Num());
		for (const FHansaPlacedBuildingRecord& Placement : State.Placement.Placements)
		{
			PlacementRecordCount += static_cast<uint32>(Placement.OccupiedCells.Num());
		}
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Placement, PlacementRecordCount,
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
			{
				if (FingerprintVersion >= 20)
				{
					Builder.AddUInt64(State.Placement.GetTopologyHash());
					Builder.AddUInt32(State.Placement.GetTopologyRecordCount());
				}
				else
				{
					Builder.AddUInt32(static_cast<uint32>(State.Placement.GetMaps().Num()));
					for (const FHansaPlacementMapInitialization& Map : State.Placement.GetMaps())
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
			[&State, FingerprintVersion](FNormalizedHashBuilder& Builder)
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
					if (FingerprintVersion >= 19)
					{
						Builder.AddUInt64(Job.SelectedMarketBuildingId.GetValue());
						Builder.AddUInt32(Job.SelectedMarketBuildingId.GetGeneration());
						Builder.AddUInt32(static_cast<uint32>(Job.RouteCells.Num()));
						for (const FHansaGridCoordinate Cell : Job.RouteCells)
						{
							Builder.AddInt32(Cell.X);
							Builder.AddInt32(Cell.Y);
						}
						Builder.AddInt32(Job.ElapsedTravelTicks);
						Builder.AddInt32(Job.RemainingTravelTicks);
						Builder.AddUInt8(static_cast<uint8>(Job.PauseReason));
					}
				}
			}));

		uint32 ResearchRecordCount = static_cast<uint32>(State.Research.Num());
		for (const FHansaHouseResearchState& Research : State.Research)
		{
			ResearchRecordCount += static_cast<uint32>(Research.CompletedTechnologyIds.Num() + Research.AppliedEffects.Num());
		}
		Report.Subsystems.Add(BuildSubsystem(EHansaStateHashSubsystem::Research, ResearchRecordCount,
			[&State](FNormalizedHashBuilder& Builder)
			{
				Builder.AddUInt32(static_cast<uint32>(State.Research.Num()));
				for (const FHansaHouseResearchState& Research : State.Research)
				{
					Builder.AddUInt64(Research.HouseId.GetValue());
					Builder.AddUInt32(Research.HouseId.GetGeneration());
					Builder.AddInt32(Research.AvailableResearchPoints);
					Builder.AddAsciiString(Research.ActiveTechnologyId);
					Builder.AddInt32(Research.ProgressTicks);
					Builder.AddUInt32(static_cast<uint32>(Research.CompletedTechnologyIds.Num()));
					for (const FString& TechnologyId : Research.CompletedTechnologyIds) Builder.AddAsciiString(TechnologyId);
					Builder.AddUInt32(static_cast<uint32>(Research.AppliedEffects.Num()));
					for (const FHansaAppliedResearchEffect& Effect : Research.AppliedEffects)
					{
						Builder.AddAsciiString(Effect.SourceTechnologyId);
						Builder.AddUInt8(static_cast<uint8>(Effect.Kind));
						Builder.AddAsciiString(Effect.TargetStableId);
						Builder.AddInt32(Effect.Magnitude);
					}
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
