#pragma once
#include "CoreMinimal.h"
#include "Presence/HansaForeignPresence.h"
#include "HansaTradeCityInspector.generated.h"
struct FHansaTradeMapCityPresentation;
namespace Hansa::Simulation { class FHansaSimulationProjection; class FHansaEconomicRegistry; }
UENUM()
enum class EHansaTradeCityState : uint8 { Unknown, Home, RenderedForeign, MarketOnly, UnsupportedSite, PolicyLocked };
/** Viewer-scoped UI summary; never grants authority or invents report values. */
USTRUCT()
struct HANSA_API FHansaTradeCityInspector
{
 GENERATED_BODY()
 UPROPERTY() FName CityId;
 UPROPERTY() EHansaTradeCityState State = EHansaTradeCityState::Unknown;
 UPROPERTY() FText Identity;
 UPROPERTY() FText Overview;
 UPROPERTY() FText MarketAccess;
 UPROPERTY() FText Presence;
 UPROPERTY() FText Routes;
 UPROPERTY() FText Expansion;
 UPROPERTY() FText Issue;
 UPROPERTY() FText PrimaryAction;
 UPROPERTY() FString PrimarySection = TEXT("Overview");
 UPROPERTY() bool bStationSupported = false;
 // Structured display facts, scoped to the viewer; not gameplay authority.
 UPROPERTY() int32 OwnedRouteCount = -1;
 UPROPERTY() int32 ApproachingShipCount = -1;
 UPROPERTY() int32 StationSiteCount = -1;
 UPROPERTY() bool bActiveLease = false;
 // -1 unknown, 0 not granted, 1 granted. Missing policy/presence is never permission.
 UPROPERTY() int32 ReportsAccess = -1;
 UPROPERTY() int32 PublicTradeAccess = -1;
 UPROPERTY() int32 RoutesAccess = -1;
 bool operator==(const FHansaTradeCityInspector& B) const;
};
namespace Hansa::UI {
 HANSA_API const Hansa::Simulation::FHansaTradeStationProjection* FindTradeCityStation(
  const Hansa::Simulation::FHansaSimulationProjection& Projection,FName City,Hansa::Simulation::FHansaHouseId Viewer);
 HANSA_API FHansaTradeCityInspector BuildTradeCityInspector(FName SelectedCity, const FHansaTradeMapCityPresentation* City,
  const Hansa::Simulation::FHansaSimulationProjection& Projection, const Hansa::Simulation::FHansaEconomicRegistry& Registry,
  Hansa::Simulation::FHansaHouseId Viewer);
}
