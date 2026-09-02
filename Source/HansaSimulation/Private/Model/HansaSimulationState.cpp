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

		if (HasDuplicateKey(Initialization.RandomStreams, [](const FHansaRandomStream& Stream) { return Stream.GetName(); }) ||
			HasDuplicateKey(Initialization.Houses, [](const FHansaHouseState& House) { return House.Id; }) ||
			HasDuplicateKey(Initialization.Cities, [](const FHansaCityState& City) { return City.DefinitionId; }) ||
			HasDuplicateKey(Initialization.Buildings, [](const FHansaBuildingState& Building) { return Building.Id; }) ||
			HasDuplicateKey(Initialization.Vehicles, [](const FHansaVehicleState& Vehicle) { return Vehicle.Id; }) ||
			HasDuplicateKey(Initialization.Routes, [](const FHansaRouteState& Route) { return Route.Id; }) ||
			HasDuplicateKey(Initialization.TestEntities, [](const FHansaTestEntityState& Entity) { return Entity.Id; }))
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
		for (const FHansaBuildingState& Building : Initialization.Buildings)
		{
			if (!Building.Id.IsValid() || !Building.OwnerId.IsValid() || !Building.DefinitionId.IsValid())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!ContainsHouse(Initialization.Houses, Building.OwnerId))
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::InvalidFormat);
			}
			if (!Building.ConstructionProgress.IsNormalized())
			{
				return THansaValueResult<FHansaSimulationState>::Failure(EHansaValueError::OutOfRange);
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
		return THansaValueResult<FHansaSimulationState>::Success(State);
	}

	uint64 FHansaSimulationState::ComputeDeterminismFingerprint(
		const FHansaSimulationDefinitionContext& Definitions) const
	{
		return FHansaStateHasher::Compute(*this, Definitions).GetOverallHash();
	}
}
