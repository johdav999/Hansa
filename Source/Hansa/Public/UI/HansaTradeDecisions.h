#pragma once
#include "CoreMinimal.h"
#include "UI/HansaTradeEstablishment.h"
#include "HansaTradeDecisions.generated.h"

USTRUCT()
struct HANSA_API FHansaTradeDecisionOption
{
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FString DefinitionId;
 // 0 acquire privilege, 1 revoke privilege, 2 fund project, 3 charter.
 UPROPERTY() uint8 Kind=0;
 UPROPERTY() FText Title;
 UPROPERTY() FText Terms;
 UPROPERTY() FText Commitment;
 UPROPERTY() FText Status;
 UPROPERTY() bool bAvailable=false;
 UPROPERTY() TArray<FHansaEstablishmentChoice> Sources;
};
USTRUCT()
struct HANSA_API FHansaTradeDecisions
{
 GENERATED_BODY()
 UPROPERTY() FName City;
 UPROPERTY() int64 Revision=0;
 UPROPERTY() FText Context;
 UPROPERTY() TArray<FHansaTradeDecisionOption> Options;
 FString Key() const;
};
namespace Hansa::UI {
HANSA_API FHansaTradeDecisions BuildTradeDecisions(const Hansa::Simulation::FHansaSimulationProjection& P,
 const Hansa::Simulation::FHansaEconomicRegistry& R,Hansa::Simulation::FHansaHouseId Viewer,FName City,const FString& Scenario);
}
