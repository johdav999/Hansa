#pragma once
#include "CoreMinimal.h"
#include "HansaTradeViewTypes.generated.h"

UENUM(BlueprintType)
enum class EHansaTradeMapModeFilter : uint8 { All, Sea, Land };

UENUM(BlueprintType)
enum class EHansaTradeMapCityFilter : uint8 { All, Presence, Routes };

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapCityPresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName StableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FVector2D NormalizedPosition = FVector2D::ZeroVector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Information;
	UPROPERTY() FText GoodReport;
	// Viewer-authorized quote components. Negative means unavailable, never zero stock.
	UPROPERTY() int64 ReportedPriceMilliMarks = -1;
	UPROPERTY() int64 ReportedStockMilliUnits = -1;
	UPROPERTY() FText PresenceReport;
	UPROPERTY() FText MapAlert;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bOwned = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bStale = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bUnknown = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bRendered = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bVisitable = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bBuildable = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bMarketOnly = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bHasPresence = false;
    UPROPERTY() bool bPresenceAttention = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bHasRoute = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 ReportAgeTicks = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText CapabilitySummary;
	friend HANSA_API bool operator==(const FHansaTradeMapCityPresentation&, const FHansaTradeMapCityPresentation&);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapStopPresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int32 Index = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName CityStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText CityLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName GoodStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ActionLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Quantity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText MinimumReserve;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText AccessibleLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bReserveRisk = false;
	friend bool operator==(const FHansaTradeMapStopPresentation&, const FHansaTradeMapStopPresentation&);
};

USTRUCT(BlueprintType)
struct HANSA_API FHansaTradeMapRoutePresentation final
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 RouteValue = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") int64 VehicleValue = 0;
	UPROPERTY() bool bCancelled=false;
	UPROPERTY() TArray<FName> CityIds;
	UPROPERTY() int64 CapacityMilliUnits = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FName DefinitionStableId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Label;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Mode;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText State;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Ownership;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StateHeading;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StateDetail;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ToggleActionLabel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ToggleActionHint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText StopSummary;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Capacity;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Upkeep;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText RoundTripTime;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText ExpectedProfitRange;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") FText Uncertainty;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bSea = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bActive = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bOwnedByPlayer = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCanToggleActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bCanCancel = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bTraveling = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bReserveRisk = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hansa|UI|Trade") bool bProfitKnown = false;
	friend bool operator==(const FHansaTradeMapRoutePresentation&, const FHansaTradeMapRoutePresentation&);
};

// Read-only directory projection. Private values are removed before rows are built.
USTRUCT()
struct HANSA_API FHansaTradeDirectoryEntry
{
 GENERATED_BODY()
 UPROPERTY() FString SemanticId;
 UPROPERTY() int64 RouteValue=0;
 UPROPERTY() int64 VehicleValue=0;
 UPROPERTY() FName CityId;
 UPROPERTY() FText Label;
 UPROPERTY() FText Detail;
 UPROPERTY() FText Alert;
 UPROPERTY() bool bSea=false;
 UPROPERTY() bool bOwned=false;
 UPROPERTY() bool bActive=false;
 UPROPERTY() bool bAvailable=false;
 UPROPERTY() bool bAttention=false;
 UPROPERTY() bool bPresence=false;
 UPROPERTY() TArray<FName> Goods;
 bool operator==(const FHansaTradeDirectoryEntry& B) const {
  return SemanticId==B.SemanticId&&RouteValue==B.RouteValue&&VehicleValue==B.VehicleValue&&CityId==B.CityId&&Label.EqualTo(B.Label)&&Detail.EqualTo(B.Detail)&&Alert.EqualTo(B.Alert)&&bSea==B.bSea&&bOwned==B.bOwned&&bActive==B.bActive&&bAvailable==B.bAvailable&&bAttention==B.bAttention&&bPresence==B.bPresence&&Goods==B.Goods;
 }
};

