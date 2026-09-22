#include "Presence/HansaForeignPresenceInitialization.h"

namespace Hansa::Simulation
{
	bool FHansaForeignPresenceInitialization::SeedAuthoredInitialPresence(
		FHansaSimulationInitialization& Initialization, const FHansaEconomicRegistry& Registry)
	{
		for (const FHansaHouseState& House : Initialization.Houses)
		{
			for (const FHansaCityState& City : Initialization.Cities)
			{
				if (Initialization.ForeignPresences.ContainsByPredicate([&](const auto& Value){return Value.HouseId==House.Id&&Value.CityId==City.DefinitionId;})) continue;
				const auto* Policy=Registry.FindCityTradePolicyForCity(City.DefinitionId.ToString());
				if (!Policy || Policy->InitialStageId.IsEmpty()) continue;
				const auto* Stage=Registry.FindPresenceStage(Policy->InitialStageId);
				if (!Stage || !Policy->AllowedStageIds.Contains(Stage->StableId)) return false;
				FHansaForeignPresenceState Presence;
				Presence.HouseId=House.Id; Presence.CityId=City.DefinitionId; Presence.CurrentStageId=Stage->StableId;
				Presence.GrantedCapabilityIds=Stage->GrantedCapabilityIds;
				Presence.GrantedCapabilityIds.RemoveAll([&](const FString& CapabilityId){return Policy->DeniedCapabilityIds.Contains(CapabilityId);});
				Presence.GrantedCapabilityIds.Sort(); Presence.EstablishedTick=Initialization.Clock.GetTick(); Presence.LastUpgradeTick=Initialization.Clock.GetTick();
				Initialization.ForeignPresences.Add(MoveTemp(Presence));
			}
		}
		return true;
	}
}
