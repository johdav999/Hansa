#pragma once
#include "CoreMinimal.h"
#include "HansaVisitingTrade.generated.h"

/** Owner-only quay quote; quantities are milli-units, money is pfennig. Never a command authority. */
USTRUCT()
struct HANSA_API FHansaVisitingTradeOffer
{
 GENERATED_BODY()
 UPROPERTY() FString City;
 UPROPERTY() FString Good;
 UPROPERTY() int64 Vehicle=0;
 UPROPERTY() bool bBuy=true;
 UPROPERTY() bool bCanSubmit=false;
 UPROPERTY() FString Cause;
 UPROPERTY() FString Remedy;
 UPROPERTY() int64 Bound=0;
 UPROPERTY() int64 Price=0;
 UPROPERTY() int64 ReportPrice=0;
 UPROPERTY() int64 ReportTick=-1;
 UPROPERTY() int64 ReportAge=-1;
 UPROPERTY() int64 ReportStock=-1;
 UPROPERTY() int64 FreeCapacity=0;
 UPROPERTY() int64 Cargo=0;
 UPROPERTY() int64 Money=0;
 UPROPERTY() FString Manifest;
 UPROPERTY() FString Receipt;
 UPROPERTY() int64 ReceiptId=0;
};
