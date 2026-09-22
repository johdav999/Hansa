# Cog destination-change jump — 2026-09-17

## Root cause

`AHansaCargoProjectionManager::Synchronize` discarded its previous interpolation state, rebuilt the manual-navigation segment from the authoritative four-metre grid cell, and called `Sample(0)` whenever a command published a projection. The movement command executes a full simulation step immediately, advancing the grid cell even while paused. A ship therefore snapped to that newly advanced cell. The next frame reused the global tick fraction on the replacement segment, causing another discontinuity. A first order issued partway through a tick could likewise jump ahead.

## Correction

Retain previous presentation entries while rebuilding the projection. The movement command explicitly requests presentation continuity when publishing its new tick. Within the same simulation tick, also preserve interpolation for an unchanged segment. When a manual order changes its current or next cell, rebase the segment on the previous displayed location and normalize interpolation over the remaining fraction of that tick. Stop orders settle onto their authoritative cell by the next tick. Repeated orders, unrelated projection refreshes and paused orders preserve presentation continuity. New simulation ticks rebuild from authoritative navigation as before.

This changes only transient presentation state. Commands, pathfinding, save format, gameplay identity and authoring schemas are unchanged. Concurrent heading-easing changes in the workspace are preserved.

## Regression coverage

`Hansa.ShipNavigation.OrderContinuity` exercises the real runtime command gateway and ship presentation actor: a first order at fraction 0.4, movement, paused reversal, resampling, repeated projection refresh/order, invalid target, resumed travel, tick-boundary continuity and stopping between cells. Existing navigation and cargo-projection tests are also run.

The initial failing regression reproduced a 400 cm jump on departure and a 640 cm displacement on reversal. Verification results for the corrected command-driven tick handling are recorded after execution. The open main editor is preserved; isolated verification binaries do not update that running process.

## Executed results

- UE 5.8 Development Editor build succeeded in `Saved/ShipNavigationVerify`.
- `Hansa.ShipNavigation`: 3/3 passed, including `OrderContinuity`, deterministic save/restore and water-only navigation. Departure and reversal positions are identical before and after the command (zero measured displacement).
- Earlier broader cargo run: ProductionRoles, RealRoadDelivery and SpeedClock passed. RouteLifecycle failed its `Sea fleet bounded` assertion, which counts all cargo actors, including wagons, against a limit of eight. This unrelated assertion was not changed. The broader cargo suite is not claimed green.
- Compact evidence: `ShipNavigationEvidence/JumpFix/`. Full verification logs remain under `Saved/ShipNavigationVerify/Saved/JumpFix*`.
- No new viewport capture was made for this correction; continuity was measured directly on the real ship presentation actor through normal runtime commands. The running main editor still needs a rebuild/restart to load the source changes.

A final source comparison found a concurrent addition calling `RebaseHeadingClock` during the new command-continuity handoff. Its declaration and implementation are present and were preserved. The position-interpolation and runtime-command changes match the passing test build; that subsequent heading-clock addition belongs to the concurrent ship-turning work and was not included in this run.
