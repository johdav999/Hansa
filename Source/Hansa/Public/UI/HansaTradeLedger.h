#pragma once
#include "CoreMinimal.h"
#include "Model/HansaIds.h"
#include "HansaTradeLedger.generated.h"
namespace Hansa::Simulation { class FHansaSimulationProjection; class FHansaEconomicRegistry; }
/** Scoped presentation only. Unknown attribution is never represented as zero. */
USTRUCT()
struct HANSA_API FHansaTradeLedgerRow {
 GENERATED_BODY()
 UPROPERTY() FName Good;
 UPROPERTY() FText Label;
 UPROPERTY() int64 Physical=0;
 UPROPERTY() int64 Reserved=0;
 UPROPERTY() int64 Available=0;
 UPROPERTY() int64 DesiredReserve=0;
 UPROPERTY() int64 AcquireTarget=0;
 UPROPERTY() bool bHasReserve=false;
 UPROPERTY() bool bHasTarget=false;
 UPROPERTY() bool bRouteCargo=false;
 UPROPERTY() bool bOrderCargo=false;
 UPROPERTY() int64 RouteId=0;
 UPROPERTY() int64 OrderId=0;
 UPROPERTY() FText CapacityShare;
 UPROPERTY() FText LastResult;
 UPROPERTY() FText Detail;
 bool Matches(int32 Filter) const;
};
USTRUCT()
struct HANSA_API FHansaTradeLedger {
 GENERATED_BODY()
 UPROPERTY() FName City;
 UPROPERTY() int64 StationId=0;
 UPROPERTY() int64 InventoryId=0;
 UPROPERTY() int64 Tick=0;
 UPROPERTY() int64 Capacity=0;
 UPROPERTY() int64 Used=0;
 UPROPERTY() int64 Free=0;
 UPROPERTY() int64 Reserved=0;
 UPROPERTY() bool bAvailable=false;
 UPROPERTY() FText Identity;
 UPROPERTY() FText Status;
 UPROPERTY() FText Upkeep;
 UPROPERTY() FText Factor;
 UPROPERTY() FText Handling;
 UPROPERTY() FText Blocker;
 UPROPERTY() FText Preservation;
 UPROPERTY() TArray<FHansaTradeLedgerRow> Rows;
 FString Key() const;
};
namespace Hansa::UI {
HANSA_API FHansaTradeLedger BuildTradeLedger(const Hansa::Simulation::FHansaSimulationProjection&,
 const Hansa::Simulation::FHansaEconomicRegistry&,Hansa::Simulation::FHansaHouseId,FName);
}
