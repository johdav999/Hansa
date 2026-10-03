#pragma once
#include "CoreMinimal.h"
#include "UI/HansaTradeEstablishment.h"
#include "HansaTradeSpecialization.generated.h"

// Immutable owner-only comparison. Authored policy remains the gameplay contract.
USTRUCT()
struct HANSA_API FHansaTradeSpecializationOption {
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FText Name;
 UPROPERTY() TArray<FText> Dimensions;
 UPROPERTY() FText Recommendation;
 UPROPERTY() FText Blocker;
 UPROPERTY() TArray<FHansaEstablishmentChoice> Sources;
 UPROPERTY() bool bCurrent=false;
 UPROPERTY() bool bAvailable=false;
 UPROPERTY() bool bRespec=false;
};
USTRUCT()
struct HANSA_API FHansaTradeSpecialization {
 GENERATED_BODY()
 UPROPERTY() FName City;
 UPROPERTY() int64 Revision=0;
 UPROPERTY() TArray<FHansaTradeSpecializationOption> Options;
 FString Key() const;
};
namespace Hansa::UI {
HANSA_API FHansaTradeSpecialization BuildTradeSpecialization(const Hansa::Simulation::FHansaSimulationProjection& Projection,
 const Hansa::Simulation::FHansaEconomicRegistry& Registry,Hansa::Simulation::FHansaHouseId Viewer,FName City);
HANSA_API TArray<FText> SpecializationDimensionLabels();
}
