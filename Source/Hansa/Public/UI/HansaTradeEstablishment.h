#pragma once
#include "CoreMinimal.h"
#include "Model/HansaIds.h"
#include "HansaTradeEstablishment.generated.h"
namespace Hansa::Simulation { class FHansaSimulationProjection; class FHansaEconomicRegistry; }
USTRUCT()
struct HANSA_API FHansaStationMaterial {
 GENERATED_BODY()
 UPROPERTY() FString GoodId;
 UPROPERTY() FText Label;
 UPROPERTY() int64 RequiredMilliUnits=0;
 UPROPERTY() int64 AvailableMilliUnits=0;
 bool operator==(const FHansaStationMaterial& B) const {return GoodId==B.GoodId&&Label.EqualTo(B.Label)&&RequiredMilliUnits==B.RequiredMilliUnits&&AvailableMilliUnits==B.AvailableMilliUnits;}
};
USTRUCT()
struct HANSA_API FHansaStationRight {
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FText Label;
 UPROPERTY() bool bGranted=false;
 bool operator==(const FHansaStationRight& B) const {return Id==B.Id&&Label.EqualTo(B.Label)&&bGranted==B.bGranted;}
};
USTRUCT()
struct HANSA_API FHansaEstablishmentChoice {
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FText Label;
 UPROPERTY() FText Detail;
 UPROPERTY() FText Transfer;
 UPROPERTY() FText Action;
 UPROPERTY() FString ReviewKey;
 UPROPERTY() bool bEligible=false;
 UPROPERTY() uint8 DeliveryMode=0;
 UPROPERTY() FText PriorityTransfer;
 UPROPERTY() FText DeliveryStatus;
 UPROPERTY() bool bPriorityActive=false;
 UPROPERTY() bool bLocalStationSource=false;
 // Owner-scoped display data, serialized with the existing source choice.
 UPROPERTY() TArray<FHansaStationMaterial> Materials;
 UPROPERTY() FText UpgradeBenefit;
 UPROPERTY() FText UpgradeDuration;
 UPROPERTY() FText UpgradeCost;
 UPROPERTY() FText UpgradeTreasury;
 UPROPERTY() FText UpgradeRemainder;
};
USTRUCT()
struct HANSA_API FHansaEstablishmentRequirement {
 GENERATED_BODY()
 UPROPERTY() FString Id;
 UPROPERTY() FText Label;
 UPROPERTY() FText Value;
 UPROPERTY() FText Hint;
 UPROPERTY() float Progress=0;
 UPROPERTY() bool bMet=false;
};
USTRUCT()
struct HANSA_API FHansaTradeEstablishment {
 GENERATED_BODY()
 UPROPERTY() FName CityId;
 UPROPERTY() FText CityLabel;
 UPROPERTY() TArray<FHansaEstablishmentChoice> Sites;
 UPROPERTY() TArray<FHansaEstablishmentChoice> Sources;
 UPROPERTY() FString SiteId;
 UPROPERTY() FString SourceId;
 UPROPERTY() FString ReviewKey;
 UPROPERTY() FText Summary;
 UPROPERTY() FText SourceDetail;
 UPROPERTY() FText FundingTerms;
 UPROPERTY() FText Confirmation;
 UPROPERTY() FText Feedback;
 UPROPERTY() FText Action;
 UPROPERTY() FText OperationsSummary;
 UPROPERTY() FText State;
 // Completed owner-scoped capability, shared with the world model. A pending
 // upgrade must never make an ordinary station present itself as an office.
 UPROPERTY() bool bOfficeBuilt=false;
 UPROPERTY() FText OperationalLabel;
 UPROPERTY() FText ConstructionPaid;
 UPROPERTY() TArray<FHansaStationRight> Rights;
 UPROPERTY() int32 CancellationRefundBasisPoints=0;
 UPROPERTY() bool bPendingUpgrade=false;
 // Structured, owner-scoped presentation travels with the existing station review.
 // No UI parses prose or reads another house's private progress/treasury.
 UPROPERTY() TArray<FHansaEstablishmentRequirement> Requirements;
 UPROPERTY() FText SiteName;
 UPROPERTY() FText SiteStatus;
 UPROPERTY() FText Storage;
 UPROPERTY() FText BuildDuration;
 UPROPERTY() FText DailyUpkeep;
 UPROPERTY() FText Treasury;
 UPROPERTY() FText Cost;
 UPROPERTY() FText Remainder;
 UPROPERTY() FText Investment;
 UPROPERTY() FText Blocker;
 UPROPERTY() FText ConstructionRemaining;
 UPROPERTY() int32 UnmetRequirements=0;
 UPROPERTY() int32 MinutesPerTick=0;
 UPROPERTY() bool bInvestmentMet=false;
 UPROPERTY() bool bHasInvestment=false;
 UPROPERTY() bool bHasCost=false;
 UPROPERTY() bool bCanOpenMarket=false;
 UPROPERTY() bool bHasAccess=false;
 UPROPERTY() float ConstructionProgress=0;
 UPROPERTY() int64 StationId=0;
 UPROPERTY() bool bLocalDelivery=false;
 UPROPERTY() bool bPriorityReview=false;
 UPROPERTY() bool bCanPrioritizeDelivery=false;
 UPROPERTY() FIntPoint PlacementAnchor=FIntPoint::ZeroValue;
 UPROPERTY() int32 PlacementRotation=0;
 UPROPERTY() FIntPoint LeaseBoundsMin=FIntPoint::ZeroValue;
 UPROPERTY() FIntPoint LeaseBoundsMax=FIntPoint::ZeroValue;
 UPROPERTY() FString WorldPresentationClass;
 UPROPERTY() int64 Tick=0;
 UPROPERTY() bool bVisible=false;
 UPROPERTY() bool bAwaitingPickup=false;
 UPROPERTY() bool bProposed=false;
 UPROPERTY() bool bConstructing=false;
 UPROPERTY() bool bComplete=false;
 UPROPERTY() bool bArrears=false;
 UPROPERTY() bool bCanPropose=false;
 UPROPERTY() bool bCanFund=false;
 UPROPERTY() bool bReview=false;
 UPROPERTY() bool bPending=false;
 bool operator==(const FHansaTradeEstablishment& Other) const;
};
namespace Hansa::UI {
using ::FHansaEstablishmentChoice;
using ::FHansaTradeEstablishment;
// Campaign-calendar formatting shared by local and server-authored UI projections.
HANSA_API FText PresenceDuration(int64 Steps,int32 MinutesPerStep);
HANSA_API FText PresenceTimeProgress(int64 Current,int64 Required,int32 MinutesPerStep);
HANSA_API FText PresenceDailyUpkeep(int64 PerStep,int32 MinutesPerStep);
HANSA_API FHansaTradeEstablishment BuildTradeEstablishment(const Hansa::Simulation::FHansaSimulationProjection& Projection,
 const Hansa::Simulation::FHansaEconomicRegistry& Registry,Hansa::Simulation::FHansaHouseId Viewer,FName City,
 const FString& Site,const FString& Source);
HANSA_API TArray<FHansaEstablishmentChoice> BuildMerchantOfficeSources(const Hansa::Simulation::FHansaSimulationProjection& Projection,
 const Hansa::Simulation::FHansaEconomicRegistry& Registry,Hansa::Simulation::FHansaHouseId Viewer,FName City);
}
