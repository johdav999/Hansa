#pragma once
#include "CoreMinimal.h"
#include "Presence/HansaForeignPresence.h"
#include "HansaTradeConstruction.generated.h"

namespace Hansa::Simulation { class FHansaSimulationProjection; class FHansaEconomicRegistry; }

/** Immutable, owner-scoped construction report; never grants placement authority. */
USTRUCT()
struct HANSA_API FHansaTradeConstructionOption
{
 GENERATED_BODY()
 UPROPERTY() FName Id;
 UPROPERTY() FText Name;
 UPROPERTY() FText Detail;
 UPROPERTY() FText Reason;
 UPROPERTY() bool bPermitted=false;
 UPROPERTY() bool bAffordable=false;
};
USTRUCT()
struct HANSA_API FHansaTradeConstructionPlot
{
 GENERATED_BODY()
 UPROPERTY() uint64 Id=0;
 UPROPERTY() uint64 StationId=0;
 UPROPERTY() uint64 OwnerId=0;
 UPROPERTY() FIntPoint BoundsMin=FIntPoint::ZeroValue;
 UPROPERTY() FIntPoint BoundsMax=FIntPoint::ZeroValue;
 UPROPERTY() TArray<FString> PermittedCategories;
 UPROPERTY() TArray<FIntPoint> OccupiedCells;
 UPROPERTY() FText Summary;
 UPROPERTY() bool bActive=false;
};
USTRUCT()
struct HANSA_API FHansaTradeConstruction
{
 GENERATED_BODY()
 UPROPERTY() FName City;
 UPROPERTY() TArray<FHansaTradeConstructionPlot> Plots;
 UPROPERTY() TArray<FHansaTradeConstructionOption> Options;
 UPROPERTY() uint64 SelectedLease=0;
 UPROPERTY() FName SelectedBuilding;
 UPROPERTY() FText Status;
 UPROPERTY() FText LockedCategories;
 FString Key() const;
};
namespace Hansa::UI
{
 HANSA_API FHansaTradeConstruction BuildTradeConstruction(
  const Hansa::Simulation::FHansaSimulationProjection& Projection,
  const Hansa::Simulation::FHansaEconomicRegistry& Registry,
  Hansa::Simulation::FHansaHouseId Viewer,FName City,uint64 SelectedLease,FName SelectedBuilding);
}
