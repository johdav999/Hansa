#pragma once
#include "CoreMinimal.h"
#include "HansaTradeRecovery.generated.h"
USTRUCT()
struct HANSA_API FHansaRecoveryCargoAction {
 GENERATED_BODY()
 UPROPERTY() FString Good;
 UPROPERTY() uint8 Kind=0;
 UPROPERTY() int64 Quantity=0;
 UPROPERTY() int64 Reserve=0;
};
USTRUCT()
struct HANSA_API FHansaRecoveryStop {
 GENERATED_BODY()
 UPROPERTY() FString City;
 UPROPERTY() TArray<FHansaRecoveryCargoAction> Actions;
};
USTRUCT()
struct HANSA_API FHansaRecoveryItem {
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FString Label;
 UPROPERTY() FString Detail;
 UPROPERTY() FString Target;
 UPROPERTY() int64 Entity=0;
 UPROPERTY() int64 Vehicle=0;
 UPROPERTY() FString Lifecycle;
 UPROPERTY() TArray<FHansaRecoveryStop> Stops;
 UPROPERTY() bool bCanToggle=false;
 UPROPERTY() bool bCanCancel=false;
};
USTRUCT()
struct HANSA_API FHansaTradeRecovery {
 GENERATED_BODY()
 UPROPERTY() FName City;
 UPROPERTY() int64 Station=0;
 UPROPERTY() FString Status;
 UPROPERTY() FString Cause;
 UPROPERTY() FString Terms;
 UPROPERTY() FString ReviewKey;
 UPROPERTY() bool bCanClose=false;
 UPROPERTY() bool bFinalizing=false;
 UPROPERTY() bool bAttention=false;
 UPROPERTY() TArray<FHansaRecoveryItem> Items;
};
