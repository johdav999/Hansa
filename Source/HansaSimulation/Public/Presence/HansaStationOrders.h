#pragma once

#include "Model/HansaIds.h"
#include "Containers/Array.h"

namespace Hansa::Simulation
{
 enum class EHansaStationOrderSide : uint8 { Acquire = 0, Release };
 enum class EHansaStationOrderAction : uint8 { Create = 0, Edit, Pause, Resume, Cancel };
 enum class EHansaStationOrderOutcome : uint8 { Pending = 0, Filled, Partial, Blocked, Completed, Paused, Suspended, Cancelled };
 enum class EHansaStationOrderBlocker : uint8 { None = 0, Funds, Budget, MarketStock, StationCapacity, MarketCapacity, ReservedStock, Access, MarketUnavailable, PriceLimit, Arithmetic };

 struct HANSASIMULATION_API FHansaStationOrderTerms final
 {
  FHansaGoodId GoodId;
  EHansaStationOrderSide Side = EHansaStationOrderSide::Acquire;
  int64 TargetOrReserveMilliUnits = 0;
  int64 CapMilliUnits = 1000;
  int64 TotalBudgetPfennig = 0;
  /** Zero disables the limit. Acquire executes at or below; release at or above the friction-adjusted settlement price. */
  int64 LimitUnitPriceMilliMarks = 0;
  int64 ReviewedMarketUpdateTick = -1;
  int64 ReviewedUnitPriceMilliMarks = 0;
 };

 struct HANSASIMULATION_API FHansaStationOrderExecution final
 {
  int64 Tick = -1;
  int64 MarketUpdateTick = -1;
  int64 RequestedMilliUnits = 0;
  int64 AppliedMilliUnits = 0;
  int64 UnitPriceMilliMarks = 0;
  int64 MoneyDelta = 0;
  uint64 FirstMovementSequence = 0;
  uint64 LastMovementSequence = 0;
  EHansaStationOrderOutcome Outcome = EHansaStationOrderOutcome::Pending;
  EHansaStationOrderBlocker Blocker = EHansaStationOrderBlocker::None;
 };

 /** Identity is (station ID, order ID); owner and city are inherited from the station. */
 struct HANSASIMULATION_API FHansaStationOrderState final
 {
  uint64 Id = 0;
  FHansaCommandId LastCommandId;
  FHansaStationOrderTerms Terms;
  bool bPaused = false;
  bool bCancelled = false;
  int64 SpentPfennig = 0;
  int64 NextUpdateTick = 0;
  TArray<FHansaStationOrderExecution> History;
 };

 struct FHansaManageStationOrderCommand final
 {
  FHansaTradeStationId StationId;
  uint64 OrderId = 0;
  EHansaStationOrderAction Action = EHansaStationOrderAction::Create;
  FHansaStationOrderTerms Terms;
 };

 HANSASIMULATION_API const TCHAR* LexToString(EHansaStationOrderOutcome Value);
 HANSASIMULATION_API const TCHAR* LexToString(EHansaStationOrderBlocker Value);
 HANSASIMULATION_API bool ValidateStationOrder(const FHansaStationOrderState& Order, int64 CurrentTick);
}
