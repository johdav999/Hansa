# Construction lifecycle and costs

`S06-P01` extends authoritative placement without creating a second building path. `FHansaPlaceBuildingCommand` still validates the footprint and entitlement first, then atomically charges the issuing house and the city's canonical inventory before it creates the building and occupancy records. A rejection leaves money, inventory, occupancy, command counters, events, tick and state hash unchanged.

## Definition contract

Every one of the 14 MVP `UHansaBuildingDefinition` assets owns:

- positive resource rows in `ConstructionCosts` where the building consumes goods;
- a non-negative `ConstructionCostPfennig` currency charge;
- a positive deterministic `BuildTicks` duration;
- `CancellationRefundBasisPoints` constrained to `0..10000`.

These reflected fields carry full Authoring Studio metadata, participate in deterministic content/registry hashing and JSON Schema export, and are checked both on the definition and during cross-definition compilation. The explicit one-time content migration is:

```powershell
UnrealEditor-Cmd.exe Hansa.uproject -run=HansaEconomicDefinitionSeed -MigrateConstructionS06P01 -unattended -nop4
```

It changes only the two S06 construction policy fields on the exact 14 stable building assets. It does not replace packages or silently run during startup/CI.

## Authoritative lifecycle

```text
placement/cost preflight
  -> PlaceBuilding (charge and create atomically)
  -> UnderConstruction (fixed elapsed ticks and normalized progress)
  -> Completed
  -> RemoveBuilding (only when dependency-free)

UnderConstruction
  -> CancelConstruction (bounded refund and occupancy release atomically)
```

The placement tick never grants work. The first following simulation tick advances `ConstructionElapsedTicks`; completion occurs exactly at the authored `BuildTicks` boundary. Existing restored records at one-million parts-per-million are normalized as completed for compatibility.

Cancellation is allowed only before completion and refunds the authored share of both currency and resources, rounded toward zero. The candidate-state transaction prevents partial refunds, negative results, overflow and duplicate cancellation. A refund that cannot fit in the city inventory rejects without mutation. Completed demolition gives no implicit refund. Safe removal rejects buildings referenced by production, population or building-owned inventory records rather than leaving dangling state.

Ordered events are `BuildingPlaced`, `ConstructionProgressed`, `ConstructionCompleted`, `ConstructionCancelled` and `BuildingRemoved`. World Actors consume the updated projection and these events only; they never advance construction themselves.

## Read models and automation

`FHansaSimulationReadOnlyAccess` provides:

- `QueryConstructionCost` for required, available and missing currency/resources;
- `QueryConstruction` for one building;
- `BuildConstructionProjection` and `FHansaSimulationProjection::GetConstructions` for owning lists.

With `empty_lubeck_build_v1` loaded, the allowlisted automation surface adds:

- queries: `construction.list`, `construction.get`, `construction.cost`;
- actions: `construction.cancel`, `building.remove`;
- fixed stepping through the existing `simulation_step`/`simulation_run` operations.

The placement fixture exposes `BuildMode.Construction.Status` and `BuildMode.Construction.Cost` semantic nodes. Controlled actions construct normal schema-version-3 gameplay commands and cannot set progress, money, stock or lifecycle state directly.

## Verification

`Hansa.Simulation.Construction.*` covers atomic cost rejection, exact progress/completion, event ordering, cancellation boundaries, bounded refunds, duplicate rejection, completed removal and snapshot ownership. Authoring tests cover metadata discovery, all 14 seeded policies, negative currency/over-refund validation and reloaded production assets.
