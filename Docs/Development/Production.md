# Hansa — Production and Causal Output Contract

- Implemented prompts: `S03-P03`, `S03-P04`; registry extended by `S04-P02`
- Contract date: 2026-09-02
- Owning module: `HansaSimulation`
- Determinism fingerprint version: `9`

## Runtime boundary

`FHansaProductionState` is canonical Actor-independent simulation state. Production records are keyed by `FHansaProductionId` and processed in typed ID order only during the existing `ConstructionAndProduction` fixed-step phase. They reference the immutable `FHansaEconomicRegistry` carried by `FHansaSimulationDefinitionContext`; the registry hash must equal the definition-context hash. No production value is copied into a second editor schema.

Building production references an existing building, recipe, input inventory and output inventory. City background supply references an existing city, output inventory, good, fixed quantity and fixed cycle. The latter models direct MVP supply such as Lüneburg salt without inventing a production building or undeclared recipe. Fish extraction uses the authored source recipe and normal building production path.

## Fixed-tick and atomic execution

An active, fully constructed production building must satisfy the larger authored workforce placeholder from its recipe and building definition. At cycle start, every recipe input is reserved in canonical good order using typed production-owned reservations. If any input cannot be reserved, the entire reservation attempt is discarded.

Progress advances by one authoritative tick. At completion, the system preflights all outputs and commits against a working inventory ledger:

1. consume every reserved input through `Sink.ProductionInput`;
2. create every recipe output through `Source.ProductionOutput`;
3. commit the ledger only when every operation succeeds.

When input and output share an inventory, the capacity preflight includes capacity released by the consumed inputs. Exact-capacity transformations therefore succeed. Missing input, arithmetic/transaction failure or output blockage never partially consumes or creates stock. A capacity-blocked completed cycle retains its reservations and elapsed progress so it can retry without duplicating work.

Background supply uses `Source.BackgroundSupply`, so all created salt or other direct supply is explicit in inventory movements and transaction accounting.

## Causal projections and events

`FHansaProductionProjection` reports:

- production/building/city/recipe and inventory identities;
- active state, progress, cycle duration and completed cycles;
- allocated versus required laborer and artisan workforce;
- nominal quantity per cycle and actual quantity produced on the last tick;
- a typed blocker plus blocking good, required quantity and available input/capacity.

Typed blockers distinguish inactive state, incomplete construction, missing definitions, each workforce tier, missing input, storage blockage and an internal inventory transaction failure. `FHansaSimulationReadOnlyAccess::QueryProduction` and `BuildProductionProjection` return owning read-only views; simulation snapshots own the canonical production records.

The normal tick result publishes stable-order `ProductionCycleCompleted`, `ProductionBlockerChanged`, and command-originated `ProductionActiveChanged` domain events. `SetProductionActive` uses the common authority, ordering, fingerprint, transactional candidate-state, and event-publication path. Player/AI/RPC callers may control only owned building production; controlled automation may additionally operate an allowlisted fixture background supply. Production progress, blockers, reservations, last-tick completion, cycle totals and reservation sequencing are included in the dedicated `Productions` state-hash subsystem.

## MVP coverage

The production tests execute:

- grain source → flour → bread;
- timber source → planks;
- iron stock → tools;
- grain → beer;
- fish source extraction;
- deterministic salt background supply.

They also cover inactive, construction, workforce, missing-input and storage blockers; multi-input all-or-nothing reservation; exact-capacity output; typed event order; snapshot/projection ownership; 1,000 identical ticks under reversed discovery order; non-negative inventory and reservation invariants; and dedicated/global state hashes.

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Production
```

## Headless fixture and evidence

`FHansaProductionFixture::TryCreate()` constructs the reviewed actor-free `mvp_production_chains_v1` fixture: the authored ten-good, eight-recipe, fourteen-building, six-need, two-tier and four-city-market registry; eight production building instances; one city inventory; and deterministic background salt supply. Fixture version `3` pins the authored registry hash `B0481C9F740D6C18`; an editor test fails if authored content and fixture version diverge. Its fingerprints track the current version-13 state-hash contract, including automatic city-workforce participation.

`FHansaProductionEvidenceWriter` emits machine-readable JSON containing fixture/version, registry hash, initial/final state hashes, tick count, ordered events and all final causal production projections. The checked-in oracle is `Tests/Golden/mvp_production_chains_v1.json`; complete generated evidence is written beneath ignored `Saved/TestEvidence/Production/`.

The adapter also exposes `lubeck_grain_shortage_v1`, causal `market.*` queries, `city.population`, shortage/reserve/price predicates, read-only assertions, and the controlled `production.set_active` and `residence.upgrade` commands. Every advance and recovery action goes through `FHansaGameplayCommandGateway`; there is no stock/price setter, UObject path, reflection query, script, console text, or generic predicate. See [GrainShortageFixture.md](GrainShortageFixture.md).

## Authoring validation

The economic compiler performs the same checks for Authoring Studio and the `HansaEconomicDefinitionValidate` commandlet. Stable diagnostics cover source/sink declaration mismatches, ambiguous same-good input/output conservation boundaries (`HSA-REGISTRY-008`), dependency cycles (`009`), and recipes unreachable from explicit source or externally supplied goods (`010`). The commandlet writes `Saved/TestEvidence/Production/economic-validation.json` by default and exits non-zero for invalid content:

```powershell
& UnrealEditor-Cmd.exe Hansa.uproject -run=HansaEconomicDefinitionValidate -unattended -NullRHI -NoSound
```
