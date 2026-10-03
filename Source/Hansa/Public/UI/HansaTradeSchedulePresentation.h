#pragma once
#include "CoreMinimal.h"
#include "HansaTradeSchedulePresentation.generated.h"
namespace Hansa::Simulation { class FHansaSimulationProjection; class FHansaEconomicRegistry; }
USTRUCT()
struct HANSA_API FHansaTradeScheduleRow {
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FString Group;
 UPROPERTY() int64 RouteId=0;
 UPROPERTY() int32 StopIndex=INDEX_NONE;
 UPROPERTY() int32 ActionIndex=INDEX_NONE;
 UPROPERTY() FName CityId;
 UPROPERTY() FName GoodId;
 UPROPERTY() FText Label;
 UPROPERTY() FText Detail;
 UPROPERTY() FText Evidence;
 UPROPERTY() FText GoodLabel;
 UPROPERTY() FText ActionLabel;
 UPROPERTY() FText ActionCity;
 UPROPERTY() FText Destination;
 UPROPERTY() FText TransferSummary;
 TOptional<int64> Carried, Prepared, TransferRequested;
 UPROPERTY() bool bCarriedKnown=false;
 UPROPERTY() bool bPreparedKnown=false;
 UPROPERTY() bool bTransferRequestedKnown=false;
 UPROPERTY() int64 CarriedValue=0;
 UPROPERTY() int64 PreparedValue=0;
 UPROPERTY() int64 TransferRequestedValue=0;
 UPROPERTY() int64 Requested=0;
 UPROPERTY() int64 Reserve=0;
 UPROPERTY() int64 Applied=0;
 UPROPERTY() int64 TransferTick=-1;
 UPROPERTY() bool bWarning=false;
};
USTRUCT()
struct HANSA_API FHansaTradeSchedulePresentation {
 GENERATED_BODY()
 UPROPERTY() int64 Tick=-1;
 UPROPERTY() int64 RouteId=0;
 UPROPERTY() int64 VehicleId=0;
 UPROPERTY() int64 InventoryId=0;
 UPROPERTY() int32 CurrentStop=INDEX_NONE;
 UPROPERTY() int32 NextStop=INDEX_NONE;
 TOptional<int64> ArrivalTick;
 TOptional<float> Progress;
 UPROPERTY() bool bArrivalKnown=false;
 UPROPERTY() bool bProgressKnown=false;
 UPROPERTY() int64 ArrivalValue=0;
 UPROPERTY() float ProgressValue=0;
 void PrepareForReplication();
 void RestoreReplicatedOptionals();
 UPROPERTY() FText Heading;
 UPROPERTY() FText Context;
 UPROPERTY() FText Evidence;
 UPROPERTY() FText Identity;
 UPROPERTY() FText DepartureCity;
 UPROPERTY() FText ArrivalCity;
 UPROPERTY() FText TravelStatus;
 UPROPERTY() FText DepartureDetail;
 UPROPERTY() FText ArrivalDetail;
 UPROPERTY() TArray<FHansaTradeScheduleRow> Rows;
};
namespace Hansa::UI {
 HANSA_API FHansaTradeSchedulePresentation BuildTradeSchedule(const Hansa::Simulation::FHansaSimulationProjection&, const Hansa::Simulation::FHansaEconomicRegistry&, int64 RouteId, int64 VehicleId, uint64 ViewerHouse);
}
