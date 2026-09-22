#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Model/HansaIds.h"
#include "Multiplayer/HansaAdmission.h"
#include "Network/HansaMultiplayerTypes.h"

class UHansaRuntimeSimulationHost;

namespace Hansa::Multiplayer
{
	/**
	 * Transport-neutral server authority boundary shared by Unreal RPCs and
	 * deterministic multiplayer tests. It never returns mutable simulation state.
	 */
	class HANSA_API FHansaMultiplayerAuthority final
	{
	public:
		bool Initialize(UHansaRuntimeSimulationHost& InHost);
		bool RegisterAdmittedClient(const Hansa::Simulation::FHansaAdmissionGrant& Admission,
			const FHansaClientInterest& Interest, FString& OutError);
		void UnregisterClient(uint64 PrincipalId);
		bool SetClientInterest(uint64 PrincipalId, const FHansaClientInterest& Interest, FString& OutError);
		/** Server-policy hook for team/report visibility. Client RPCs cannot call this. */
		bool SetAuthorizedReportHouses(uint64 PrincipalId,
			TConstArrayView<Hansa::Simulation::FHansaHouseId> HouseIds, FString& OutError);
		[[nodiscard]] bool IsRegistered(uint64 PrincipalId) const;
		[[nodiscard]] bool IsHouseRegistered(Hansa::Simulation::FHansaHouseId HouseId) const;
		[[nodiscard]] int32 GetRegisteredClientCount() const { return Clients.Num(); }
		[[nodiscard]] uint64 GetExpectedClientSequence(uint64 PrincipalId) const;

		FHansaClientCommandFeedback SubmitIntent(uint64 PrincipalId, const FHansaClientCommandIntent& Intent);
		bool BuildProjection(uint64 PrincipalId, int64 ClientKnownRevision, bool bForceFullRefresh,
			FHansaClientProjectionSnapshot& OutProjection, FString& OutError);

	private:
		struct FClientState
		{
			Hansa::Simulation::FHansaParticipantId ParticipantId;
			Hansa::Simulation::FHansaHouseId HouseId;
			FHansaClientInterest Interest;
			uint64 ExpectedClientSequence = 1;
			TSet<uint64> SeenNonces;
			int64 LastProjectionRevision = 0;
			uint64 LastDeliveredEventSequence = 0;
			TSet<Hansa::Simulation::FHansaHouseId> AuthorizedReportHouses;
			FHansaClientProjectionSnapshot LastFullProjection;
			bool bHasLastFullProjection = false;
		};

		bool ValidateInterest(const FHansaClientInterest& Interest, FString& OutError) const;
		bool IsInterestedInCity(const FClientState& Client, const FString& CityId) const;
		bool CanReadHousePrivate(const FClientState& Client, Hansa::Simulation::FHansaHouseId HouseId) const;

		TWeakObjectPtr<UHansaRuntimeSimulationHost> Host;
		TMap<uint64, FClientState> Clients;
	};
}
