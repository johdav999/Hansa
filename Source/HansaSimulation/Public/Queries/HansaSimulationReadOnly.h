#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Definitions/HansaSimulationDefinitionContext.h"
#include "Diagnostics/HansaStateHash.h"
#include "Model/HansaSimulationState.h"

namespace Hansa::Simulation
{
	struct FHansaDeterminismFingerprint
	{
		uint32 Version = FHansaSimulationState::DeterminismFingerprintVersion;
		uint32 SystemPipelineVersion = FHansaSimulationState::CurrentSystemPipelineVersion;
		uint64 Value = 0;

		[[nodiscard]] FString ToDebugString() const;

		friend bool operator==(const FHansaDeterminismFingerprint& Left, const FHansaDeterminismFingerprint& Right)
		{
			return Left.Version == Right.Version &&
				Left.SystemPipelineVersion == Right.SystemPipelineVersion &&
				Left.Value == Right.Value;
		}
	};

	/** Owning immutable copy suitable for asynchronous save, UI, networking or automation work. */
	class HANSASIMULATION_API FHansaSimulationSnapshot final
	{
	public:
		[[nodiscard]] const FHansaSimulationClock& GetClock() const { return Clock; }
		[[nodiscard]] uint64 GetCampaignSeed() const { return CampaignSeed; }
		[[nodiscard]] uint64 GetProcessedCommandCount() const { return ProcessedCommandCount; }
		[[nodiscard]] uint64 GetLastProcessedCommandSequence() const { return LastProcessedCommandSequence; }
		[[nodiscard]] FHansaCommandId GetLastProcessedCommandId() const { return LastProcessedCommandId; }
		[[nodiscard]] uint64 GetCommandHistoryFingerprint() const { return CommandHistoryFingerprint; }
		[[nodiscard]] uint64 GetPublishedDomainEventCount() const { return PublishedDomainEventCount; }
		[[nodiscard]] const FHansaDeterminismFingerprint& GetFingerprint() const { return Fingerprint; }
		[[nodiscard]] TConstArrayView<FHansaRandomStream> GetRandomStreams() const { return RandomStreams; }
		[[nodiscard]] TConstArrayView<FHansaHouseState> GetHouses() const { return Houses; }
		[[nodiscard]] TConstArrayView<FHansaCityState> GetCities() const { return Cities; }
		[[nodiscard]] TConstArrayView<FHansaBuildingState> GetBuildings() const { return Buildings; }
		[[nodiscard]] TConstArrayView<FHansaVehicleState> GetVehicles() const { return Vehicles; }
		[[nodiscard]] TConstArrayView<FHansaRouteState> GetRoutes() const { return Routes; }
		[[nodiscard]] TConstArrayView<FHansaTestEntityState> GetTestEntities() const { return TestEntities; }

	private:
		friend class FHansaSimulationReadOnlyAccess;

		FHansaSimulationClock Clock;
		uint64 CampaignSeed = 0;
		uint64 ProcessedCommandCount = 0;
		uint64 LastProcessedCommandSequence = 0;
		FHansaCommandId LastProcessedCommandId;
		uint64 CommandHistoryFingerprint = FHansaSimulationState::EmptyCommandHistoryFingerprint;
		uint64 PublishedDomainEventCount = 0;
		FHansaDeterminismFingerprint Fingerprint;
		TArray<FHansaRandomStream> RandomStreams;
		TArray<FHansaHouseState> Houses;
		TArray<FHansaCityState> Cities;
		TArray<FHansaBuildingState> Buildings;
		TArray<FHansaVehicleState> Vehicles;
		TArray<FHansaRouteState> Routes;
		TArray<FHansaTestEntityState> TestEntities;
	};

	struct FHansaHouseProjection
	{
		FHansaHouseId Id;
		FHansaMoney Money;
	};

	/** Purpose-built copy for UI and automation; it never exposes authoritative containers. */
	class HANSASIMULATION_API FHansaSimulationProjection final
	{
	public:
		[[nodiscard]] const FHansaSimulationClock& GetClock() const { return Clock; }
		[[nodiscard]] const FHansaCalendarProjection& GetCalendar() const { return Calendar; }
		[[nodiscard]] const FHansaDeterminismFingerprint& GetFingerprint() const { return Fingerprint; }
		[[nodiscard]] uint64 GetProcessedCommandCount() const { return ProcessedCommandCount; }
		[[nodiscard]] uint64 GetPublishedDomainEventCount() const { return PublishedDomainEventCount; }
		[[nodiscard]] int32 GetCityCount() const { return CityCount; }
		[[nodiscard]] int32 GetBuildingCount() const { return BuildingCount; }
		[[nodiscard]] int32 GetVehicleCount() const { return VehicleCount; }
		[[nodiscard]] int32 GetRouteCount() const { return RouteCount; }
		[[nodiscard]] int32 GetTestEntityCount() const { return TestEntityCount; }
		[[nodiscard]] TConstArrayView<FHansaHouseProjection> GetHouses() const { return Houses; }

	private:
		friend class FHansaSimulationReadOnlyAccess;

		FHansaSimulationClock Clock;
		FHansaCalendarProjection Calendar;
		FHansaDeterminismFingerprint Fingerprint;
		uint64 ProcessedCommandCount = 0;
		uint64 PublishedDomainEventCount = 0;
		int32 CityCount = 0;
		int32 BuildingCount = 0;
		int32 VehicleCount = 0;
		int32 RouteCount = 0;
		int32 TestEntityCount = 0;
		TArray<FHansaHouseProjection> Houses;
	};

	/** Borrowed const-only view over the live state plus its immutable definition context. */
	class HANSASIMULATION_API FHansaSimulationReadOnlyAccess final
	{
	public:
		[[nodiscard]] const FHansaSimulationClock& GetClock() const;
		[[nodiscard]] uint64 GetCampaignSeed() const;
		[[nodiscard]] uint64 GetProcessedCommandCount() const;
		[[nodiscard]] uint64 GetLastProcessedCommandSequence() const;
		[[nodiscard]] FHansaCommandId GetLastProcessedCommandId() const;
		[[nodiscard]] uint64 GetPublishedDomainEventCount() const;
		[[nodiscard]] FHansaDeterminismFingerprint GetFingerprint() const;
		[[nodiscard]] FHansaStateHashReport BuildStateHashReport() const;
		[[nodiscard]] TConstArrayView<FHansaHouseState> GetHouses() const;
		[[nodiscard]] TConstArrayView<FHansaCityState> GetCities() const;
		[[nodiscard]] TConstArrayView<FHansaBuildingState> GetBuildings() const;
		[[nodiscard]] TConstArrayView<FHansaVehicleState> GetVehicles() const;
		[[nodiscard]] TConstArrayView<FHansaRouteState> GetRoutes() const;
		[[nodiscard]] TConstArrayView<FHansaTestEntityState> GetTestEntities() const;

		[[nodiscard]] FHansaSimulationSnapshot CaptureSnapshot() const;
		[[nodiscard]] THansaValueResult<FHansaSimulationProjection> BuildProjection() const;

	private:
		friend class FHansaSimulationState;

		FHansaSimulationReadOnlyAccess(
			const FHansaSimulationState& InState,
			const FHansaSimulationDefinitionContext& InDefinitions)
			: State(&InState)
			, Definitions(&InDefinitions)
		{
		}

		const FHansaSimulationState* State = nullptr;
		const FHansaSimulationDefinitionContext* Definitions = nullptr;
	};
}
