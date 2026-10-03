#pragma once
#include "Network/HansaMultiplayerTypes.h"
class UHansaRuntimeSimulationHost;
namespace Hansa::Simulation { class FHansaSimulationProjection; }
namespace Hansa::UI {
HANSA_API FString TradeRoutePlanKey(int64 RouteId,TConstArrayView<FHansaClientRouteStopIntent> Stops);
// Public map metadata and owner-only operational details; never a client simulation copy.
HANSA_API FHansaReplicatedTradeWorkspace BuildRemoteTradeWorkspace(
    UHansaRuntimeSimulationHost& Host, const Hansa::Simulation::FHansaSimulationProjection& Source,
    const FHansaClientProjectionSnapshot& Authorized);
}
