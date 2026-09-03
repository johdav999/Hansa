# Hansa — Determinism Diagnostics and Fixture Contract

- Implemented prompts: `S01-P04`, extended through `S06-P03`
- Contract date: 2026-09-03
- Owning module: `HansaSimulation`
- Hash format version: `1`
- Normalization version: `1`
- Determinism fingerprint version: `13`
- Evidence schema version: `1`
- Governing decision: [ADR-0006](../Architecture/Decisions/0006-normalized-state-hashes-and-diagnostics.md)

## Source ownership

| Concern | Public contract | Implementation |
| --- | --- | --- |
| Normalized state/subsystem hashes | `Source/HansaSimulation/Public/Diagnostics/HansaStateHash.h` | `Private/Diagnostics/HansaStateHash.cpp` |
| Per-tick traces and comparison | `Public/Diagnostics/HansaDeterminismTrace.h` | `Private/Diagnostics/HansaDeterminismTrace.cpp` |
| Machine-readable evidence | `Public/Diagnostics/HansaDeterminismEvidence.h` | `Private/Diagnostics/HansaDeterminismEvidence.cpp` |
| Named headless fixture harness | `Public/Fixtures/HansaDeterministicFixture.h` | `Private/Fixtures/HansaDeterministicFixture.cpp` |
| Projection diffs | `Public/Queries/HansaSimulationProjectionDiff.h` | `Private/Queries/HansaSimulationProjectionDiff.cpp` |
| Reviewed fixture descriptor | `Tests/Fixtures/foundation_determinism_v1.json` | Golden final checksum plus exact command schedule |
| Automation coverage | `Source/HansaTests/Private/Simulation/HansaDeterminismDiagnosticsTests.cpp` | Five `Hansa.Simulation.Diagnostics.*` tests |

## Hash normalization

The fifteen fixed-order subsystem hashes are:

1. `Contract` — fingerprint/pipeline versions, scenario ID and definition hash;
2. `SimulationMetadata` — clock, seed, command identity/order/history and published-event count;
3. `RandomStreams` — canonical stream name, algorithm, state and draw count;
4. `Houses`;
5. `Cities`;
6. `Buildings`;
7. `Vehicles`;
8. `Routes`;
9. `Inventories` — canonical inventory records, accepted goods, stock, reservations, retained movements and movement sequencing;
10. `Productions` — canonical production configuration, automatic city-workforce participation, progress, blockers, reservations, completed cycles and reservation sequencing;
11. `TestEntities` — the representative S01-P03 lifecycle records;
12. `Population` — canonical cohorts, operational residence and market-access status, requirements, satisfaction, workforce and growth state;
13. `Market` — cadence/settings plus canonical city-good price inputs, factors, accumulators, affordability, alert onset and bounded history;
14. `Placement` — canonical city grids, ownership, entitlements, placed footprints and road occupancy;
15. `Logistics` — local capacity/delay policy, canonical requests, causal bottlenecks, reserved pickups and in-transit delivery jobs.

Every component is domain-separated by format, normalization and subsystem ID and includes its normalized record count. State initialization already canonicalizes result-affecting arrays; hashes consume that stable order. The global `FHansaDeterminismFingerprint` is now the ordered aggregate of these component hashes, so there is no parallel checksum algorithm to drift.

Included fields are authoritative plain state and immutable definition identity. Excluded fields are transient cache/rebuild data, returned event batches, snapshots, projections/diffs, debug strings, logs, memory addresses, allocation capacity and every Actor/UObject/World/UI/editor/automation transport concern. The published-event count is included even though event payload batches are not retained.

## Projection diagnostics

`FHansaSimulationProjectionDiff::Compare` produces an owning stable-order list covering tick, processed commands, published events, entity counts, house money keyed by typed house identity, and correlation fingerprint. Equal projections return an empty diff. Compact summaries are bounded by a caller-provided maximum entry count.

Projection diffs are evidence only. They do not expose or mutate authoritative containers.

## Named fixture harness

`FHansaDeterministicFixtureDescriptor` validates lowercase snake-case `_vN` identity, schema version, owner, immutable definition context and canonical initial state. `FHansaDeterministicFixtureHarness::RunExactTicks` accepts a bounded exact tick count plus one globally ordered typed command schedule, initializes without a rendered world and sends every tick through `FHansaGameplayCommandGateway`.

The successful result contains the final read-only projection and `FHansaDeterminismTrace`: initial hashes plus one record for every processed tick. Invalid ranges/schedules and gateway/projection/trace failures return structured causes.

The reviewed `foundation_determinism_v1` descriptor runs six exact ticks with no-op commands at ticks two and four and locks final fingerprint `5428C9542869BCFE`. S06-P03 advances the contract to version 13 by including residence/market operational status and automatic city-workforce participation.

## First-divergence and evidence behavior

Trace comparison checks fixture contract, initial state, processed tick, pipeline order, event order and then ordered state subsystems. It stops at the first mismatch and returns a compact record with first processed tick, cause, relevant state subsystem where applicable and both hashes.

Tests write parseable evidence to:

```text
Saved/TestEvidence/foundation_determinism_v1/automation/determinism-run.json
Saved/TestEvidence/foundation_determinism_v1/automation/first-divergence.json
```

These ignored files carry version, fixture identity, seed, definition hash, tick records, pipeline/event hashes and component state hashes. They are evidence, not save or network formats.

## Verification

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Diagnostics
```

Coverage proves normalized component/global correlation, cache exclusion, compact summaries, projection diffs, exact named fixture execution, reviewed golden checksum, JSON evidence, one-field replay divergence at its first affected tick, authoritative house-subsystem divergence and intentional pipeline-order drift detection.

## Deliberate next boundary

Future simulation increments must preserve the versioned subsystem ordering or explicitly advance the fingerprint contract and refresh reviewed evidence.
