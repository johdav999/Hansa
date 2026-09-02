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
		case EHansaStateHashSubsystem::TestEntities: return TEXT("TestEntities");
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
		Report.Subsystems.Reserve(9);

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
