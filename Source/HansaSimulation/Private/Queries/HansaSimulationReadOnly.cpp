#include "Queries/HansaSimulationReadOnly.h"

namespace Hansa::Simulation
{
	FString FHansaDeterminismFingerprint::ToDebugString() const
	{
		return FString::Printf(
			TEXT("StateFingerprint[version=%u;pipeline=%u;value=%016llX]"),
			Version,
			SystemPipelineVersion,
			static_cast<unsigned long long>(Value));
	}

	FHansaSimulationReadOnlyAccess FHansaSimulationState::CreateReadOnlyAccess(
		const FHansaSimulationDefinitionContext& Definitions) const
	{
		check(bInitialized);
		check(Definitions.IsValid());
		return FHansaSimulationReadOnlyAccess(*this, Definitions);
	}

	const FHansaSimulationClock& FHansaSimulationReadOnlyAccess::GetClock() const
	{
		return State->Clock;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetCampaignSeed() const
	{
		return State->CampaignSeed;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetProcessedCommandCount() const
	{
		return State->ProcessedCommandCount;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetLastProcessedCommandSequence() const
	{
		return State->LastProcessedCommandSequence;
	}

	FHansaCommandId FHansaSimulationReadOnlyAccess::GetLastProcessedCommandId() const
	{
		return State->LastProcessedCommandId;
	}

	uint64 FHansaSimulationReadOnlyAccess::GetPublishedDomainEventCount() const
	{
		return State->PublishedDomainEventCount;
	}

	FHansaDeterminismFingerprint FHansaSimulationReadOnlyAccess::GetFingerprint() const
	{
		FHansaDeterminismFingerprint Fingerprint;
		Fingerprint.Value = State->ComputeDeterminismFingerprint(*Definitions);
		return Fingerprint;
	}

	FHansaStateHashReport FHansaSimulationReadOnlyAccess::BuildStateHashReport() const
	{
		return FHansaStateHasher::Compute(*State, *Definitions);
	}

	TConstArrayView<FHansaHouseState> FHansaSimulationReadOnlyAccess::GetHouses() const
	{
		return State->Houses;
	}

	TConstArrayView<FHansaCityState> FHansaSimulationReadOnlyAccess::GetCities() const
	{
		return State->Cities;
	}

	TConstArrayView<FHansaBuildingState> FHansaSimulationReadOnlyAccess::GetBuildings() const
	{
		return State->Buildings;
	}

	TConstArrayView<FHansaVehicleState> FHansaSimulationReadOnlyAccess::GetVehicles() const
	{
		return State->Vehicles;
	}

	TConstArrayView<FHansaRouteState> FHansaSimulationReadOnlyAccess::GetRoutes() const
	{
		return State->Routes;
	}

	TConstArrayView<FHansaTestEntityState> FHansaSimulationReadOnlyAccess::GetTestEntities() const
	{
		return State->TestEntities;
	}

	FHansaSimulationSnapshot FHansaSimulationReadOnlyAccess::CaptureSnapshot() const
	{
		FHansaSimulationSnapshot Snapshot;
		Snapshot.Clock = State->Clock;
		Snapshot.CampaignSeed = State->CampaignSeed;
		Snapshot.ProcessedCommandCount = State->ProcessedCommandCount;
		Snapshot.LastProcessedCommandSequence = State->LastProcessedCommandSequence;
		Snapshot.LastProcessedCommandId = State->LastProcessedCommandId;
		Snapshot.CommandHistoryFingerprint = State->CommandHistoryFingerprint;
		Snapshot.PublishedDomainEventCount = State->PublishedDomainEventCount;
		Snapshot.Fingerprint = GetFingerprint();
		Snapshot.RandomStreams = State->RandomStreams;
		Snapshot.Houses = State->Houses;
		Snapshot.Cities = State->Cities;
		Snapshot.Buildings = State->Buildings;
		Snapshot.Vehicles = State->Vehicles;
		Snapshot.Routes = State->Routes;
		Snapshot.TestEntities = State->TestEntities;
		return Snapshot;
	}

	THansaValueResult<FHansaSimulationProjection> FHansaSimulationReadOnlyAccess::BuildProjection() const
	{
		const THansaValueResult<FHansaCalendarProjection> Calendar = State->Clock.TryProjectCalendar();
		if (!Calendar)
		{
			return THansaValueResult<FHansaSimulationProjection>::Failure(Calendar.Error);
		}

		FHansaSimulationProjection Projection;
		Projection.Clock = State->Clock;
		Projection.Calendar = Calendar.Value;
		Projection.Fingerprint = GetFingerprint();
		Projection.ProcessedCommandCount = State->ProcessedCommandCount;
		Projection.PublishedDomainEventCount = State->PublishedDomainEventCount;
		Projection.CityCount = State->Cities.Num();
		Projection.BuildingCount = State->Buildings.Num();
		Projection.VehicleCount = State->Vehicles.Num();
		Projection.RouteCount = State->Routes.Num();
		Projection.TestEntityCount = State->TestEntities.Num();
		Projection.Houses.Reserve(State->Houses.Num());
		for (const FHansaHouseState& House : State->Houses)
		{
			FHansaHouseProjection HouseProjection;
			HouseProjection.Id = House.Id;
			HouseProjection.Money = House.Money;
			Projection.Houses.Add(HouseProjection);
		}
		return THansaValueResult<FHansaSimulationProjection>::Success(Projection);
	}
}
