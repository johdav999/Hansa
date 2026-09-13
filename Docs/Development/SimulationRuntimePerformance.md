# Simulation runtime performance

## 2026-09-13 full-survey correction

The full Lubeck survey contains 1,014,049 placement cells. These cells describe immutable terrain,
ownership, bounds, and blocked-cell topology; they are compiled scenario data rather than campaign
state.

The former runtime stored that array directly in `FHansaPlacementState`. Every transactional tick
copied the complete state, and every determinism fingerprint traversed the complete cell array. At
accelerated time, the real-time host could then execute up to 16 overdue ticks in one rendered frame.
That combination formed a catch-up spiral and starved rendering.

The corrected ownership and scheduling contract is:

- `FHansaPlacementTopology` canonicalizes and validates map cells once, computes one topology hash,
  and is shared immutably by the definition context and all state snapshots.
- `FHansaPlacementState` contains only the shared topology handle plus sparse entitlements,
  placements, and occupied-cell records. Transactional copies no longer duplicate surveyed cells.
- State fingerprint contract version 20 incorporates the precomputed topology hash. Subsystem hashes
  are cached and invalidated through explicit mutation dependencies; unchanged subsystems are reused.
- Save-envelope format 7 records the topology identity and serializes sparse mutable placement data
  only. The explicit `Hansa.Save.6To7.MovePlacementTopologyToDefinitions` migration reads legacy map
  cells, verifies them against current compiled topology, and emits the new fingerprint contract.
- Real-time advancement executes at most one simulation tick per rendered frame. When overloaded it
  drops accumulated accelerated-time debt instead of processing a multi-tick catch-up burst.

Development automation evidence after the change:

- Full survey: 1,014,049 cells, 300 simulation ticks in 0.182 seconds (Development) and
  0.369 seconds (DebugGame).
- Full-survey format-7 save: 25,213 bytes.
- `Hansa.Simulation`: 80 tests passed.
- Placement, determinism diagnostics, save integration, runtime scheduling, and MVP envelope tests
  passed independently.

The benchmark uses Unreal command-line automation with NullRHI, so it proves simulation-side cost and
save size rather than a specific rendered-editor FPS. Render-thread/GPU profiling remains separate.
