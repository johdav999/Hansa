# Hansa — Simulation Kernel Contract

- Implemented prompts: `S01-P02`, extended through `S04-P03`
- Contract date: 2026-09-02
- Owning module: `HansaSimulation`
- System pipeline version: `1`
- Determinism fingerprint version: `8`

## Purpose

This note records the concrete authoritative-state, fixed-step, transient-cache and read-only-query boundaries introduced by `S01-P02`. It builds on [DeterministicPrimitives.md](DeterministicPrimitives.md), ADR-0002 and ADR-0003 without introducing an Actor, UObject, World, UI, editor, automation or provider dependency.

## Source ownership

| Concern | Public contract | Implementation |
| --- | --- | --- |
| Immutable definition identity/context | `Source/HansaSimulation/Public/Definitions/HansaSimulationDefinitionContext.h` | `Private/Definitions/HansaSimulationDefinitionContext.cpp` |
| Authoritative plain records and state | `Public/Model/HansaSimulationState.h` | `Private/Model/HansaSimulationState.cpp` |
| Owning snapshots and UI/automation projections | `Public/Queries/HansaSimulationReadOnly.h` | `Private/Queries/HansaSimulationReadOnly.cpp` |
| Fixed-step pipeline and transient cache | `Public/Systems/HansaSimulationPipeline.h` | `Private/Systems/HansaSimulationPipeline.cpp` |
| Inventory ledger and typed queries | `Public/Inventory/HansaInventory.h` | `Private/Inventory/HansaInventory.cpp` |
| Production state and causal output | `Public/Production/HansaProduction.h` | `Private/Production/HansaProduction.cpp` |
| Local market state and reports | `Public/Market/HansaMarket.h` | `Private/Market/HansaMarket.cpp` |
| Headless tests | `Source/HansaTests/Private/Simulation/` | Kernel, inventory, diagnostics and integration automation tests |

## State boundaries

`FHansaSimulationDefinitionContext` is immutable after validated construction. It currently identifies the scenario and compiled-definition hash; the later definition registry can extend this boundary without becoming mutable campaign state.

`FHansaSimulationState` owns authoritative mutable data:

- clock and campaign seed;
- named deterministic RNG state;
- processed-command count, last global sequence and rolling command-history fingerprint;
- house, city, building, vehicle and route records using typed IDs and fixed-point values;
- the canonical city/building/warehouse inventory ledger, reservations and bounded recent movements;
- canonical production progress, blockers, production-owned input reservations and completed-cycle state;
- canonical per-city/good market configuration, prices, causal inputs and bounded report history.

The state fields and arrays are private. Initialization/restore uses `FHansaSimulationInitialization`; `TryCreate` copies and canonicalizes discovery input, rejects duplicate keys, invalid ranges and missing owner/vehicle references, then stores all result-affecting arrays in typed stable order. Systems never depend on discovery order, `TMap`/`TSet` order, pointer value or Actor tick.

Only `FHansaSimulationPipeline` mutates a live state in this increment. Public consumers receive one of three read-only forms:

- `FHansaSimulationReadOnlyAccess`: a borrowed const-only view over live state;
- `FHansaSimulationSnapshot`: an owning immutable copy for asynchronous save/network/query work;
- `FHansaSimulationProjection`: a purpose-built copy containing clock/calendar, correlation fingerprint, entity counts, canonical house summaries, inventory projections and causal production projections.

Snapshots and projections expose `TConstArrayView` rather than mutable containers. A captured snapshot does not change when the live state advances.

## Fixed-step pipeline

Pipeline version `1` executes exactly once in this order:

| Order | Phase |
| --- | --- |
| 1 | `ApplyCommands` |
| 2 | `CalendarAndWorldEvents` |
| 3 | `VehicleMovementAndTransfers` |
| 4 | `WarehousesAndStorage` |
| 5 | `ConstructionAndProduction` |
| 6 | `WorkforceAndNeeds` |
| 7 | `MarketClearing` |
| 8 | `PricesAndHistory` |
| 9 | `FinanceAndContracts` |
| 10 | `ResearchPoliticsAndVictory` |
| 11 | `PublishAndChecksum` |

The representative domain phases are stateless no-op services until their feature prompts land. A successful step advances exactly one simulation tick. Phase names, meanings or order cannot change silently; such a change requires a pipeline/simulation-version decision and updated replay/migration evidence.

As of `S01-P03`, the command phase accepts typed `FHansaGameplayCommand` payloads only through `FHansaGameplayCommandGateway::ExecuteTick`; the pipeline executor is private to that gateway. Validation and sequential application occur against a working state copy, and only a fully successful tick commits. The complete header/payload fingerprint replaces caller-supplied accepted-command stamps. See [CommandGateway.md](CommandGateway.md).

Clock overflow and every input preflight failure leave authoritative state and its fingerprint unchanged.

## Transient and fingerprint contracts

`FHansaSimulationTransientCache` contains only rebuildable phase trace and derived entity-count data. It can be discarded at any time and is excluded from state, snapshots, projections and fingerprints.

As of `S01-P04`, the global deterministic fingerprint is assembled from the versioned normalized subsystem report documented in [DeterminismDiagnostics.md](DeterminismDiagnostics.md). Its fixed component order contains:

- fingerprint and pipeline versions;
- scenario ID and compiled-definition hash;
- clock, seed, command/event sequencing history and all RNG state;
- every canonical authoritative record and fixed-point field.

As of `S03-P02`, [Inventory.md](Inventory.md) defines the atomic inventory transaction, reservation, projection and query boundary. Inventory state is included as its own ordered hash subsystem, requiring determinism fingerprint version `4`.

As of `S03-P03`, [Production.md](Production.md) binds the immutable compiled economic registry to the definition context and executes canonical production records during `ConstructionAndProduction`. Production state is included as its own ordered hash subsystem, requiring determinism fingerprint version `5`.

As of `S04-P01`, [Population.md](Population.md) executes cohort needs in `WorkforceAndNeeds` and adds the `Population` hash subsystem, requiring fingerprint version `6`. As of `S04-P02`, [Market.md](Market.md) executes deterministic city-good reports in `MarketClearing` and adds the `Market` hash subsystem, requiring fingerprint version `7`. S04-P03 adds authoritative affordability and alert-onset fields to that subsystem for exact causal alert age, requiring fingerprint version `8`.

Component and global debug strings are correlation evidence, not save formats. Incompatible inclusion, byte normalization, component order or algorithm changes require a hash/normalization/fingerprint-version change and updated fixture evidence.

## Verification

Run the focused filter with:

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Kernel
```

Coverage includes canonicalization independent of discovery order, duplicate/reference/range rejection, exact phase order, accepted-command history, transactional errors, cache exclusion, immutable snapshot behavior, projection correlation, inventory invariants and per-tick equality across a 1,000-tick replay with the same initial state, seed and command stream.

## Deliberate next boundary

`S03-P04 — Production fixture, queries and editor validation` is the next integrated boundary.
