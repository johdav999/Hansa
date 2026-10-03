#pragma once
#include "CoreMinimal.h"
#include "Trade/HansaTrade.h"
#include "Inventory/HansaInventory.h"

struct FHansaCargoProductChoice
{
 Hansa::Simulation::FHansaGoodId GoodId;
 FText Name, Detail;
 bool bEnabled = false;
 int64 Planned = 0, Current = 0, Reported = -1, PricePf = -1, ReportAge = -1;
 int32 AssignedSlot = INDEX_NONE;
};
struct FHansaCargoCellEditor
{
 bool bOpen = false, bLoad = false;
 int64 Ship = 0, Route = 0;
 int32 Stop = INDEX_NONE, Slot = INDEX_NONE;
 Hansa::Simulation::FHansaCityDefinitionId City;
 Hansa::Simulation::FHansaRouteCargoAction Draft;
 TArray<FHansaCargoProductChoice> Products;
 FText Error;
 FText SourceLabel;
 bool bTownMarketOnly = true;
};
