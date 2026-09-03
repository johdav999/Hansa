# Deterministic grid placement

## Authority and ownership

S05-P02 adds placement as authoritative `HansaSimulation` state. Actors, collision traces, preview meshes and the Lübeck placeholder components never own occupancy. A placed building is represented by the existing `FHansaBuildingState` plus one `FHansaPlacedBuildingRecord`; both are created transactionally by `FHansaPlaceBuildingCommand` through `FHansaGameplayCommandGateway`.

Adding the closed `PlaceBuilding` payload advances the gameplay command schema to version 2. Older schema-1 commands continue to fail with the existing structured `UnsupportedSchemaVersion` result rather than being interpreted as placement-capable traffic.

The placement state is part of snapshots, read-only projections and the normalized determinism checksum (`DeterminismFingerprintVersion = 12`, `Placement` hash subsystem). Restore canonicalizes maps, cells, entitlements, placements and occupied cells, rejects duplicate or overlapping records, and cross-checks every placement against its building, city and owner.

## Lübeck grid

The representative map uses a fixed 60×40 grid at 400 Unreal units per cell over the S05-P01 camera rectangle. `Hansa::Game::LubeckPlacementGrid` owns the deterministic world/grid conversion and creates explicit land, shore and water cells from the same placeholder topology. `AHansaLubeckWorldFoundation::WorldToPlacementCell` and `PlacementCellToWorld` expose composition-safe Blueprint conversions and account for a transformed foundation Actor.

Scenario initialization supplies cell ownership and per-house building entitlements. The initial Lübeck factory grants the owning house every compiled MVP building; research/scenario work can replace those entitlements without changing building definitions or placement identity.

## Shared validation

Preview, player input, multiplayer, AI and controlled automation call `FHansaPlacementRules::Validate`. Confirmation submits the same typed build command and the gateway validates again against the transactional candidate state. The result returns the complete canonical footprint plus structured reasons with stable `Placement.Validation.*` and `Placement.Remedy.*` keys.

Stable causes cover:

- invalid request, unknown city or unknown compiled building;
- missing per-house prerequisite entitlement;
- bounds, missing grid cells and wrong cell owner;
- water, authored blockers and occupied cells;
- missing shoreline coverage;
- missing orthogonally adjacent road.

Rotations are quarter turns. East and west swap the authored footprint dimensions; all occupied cells are evaluated individually. Shoreline buildings must cover at least one shore cell. Road-dependent buildings must be orthogonally adjacent to an authoritative placement whose definition matches the map's stable road definition ID.

## Placement lifecycle and roads

`FHansaPlacementSession` is presentation-only state for selection, cursor anchor, rotation, road dragging, repeat and cancel. Its confirmation specs are converted to normal `FHansaPlaceBuildingCommand` payloads; it cannot mutate occupancy. A road drag expands inclusively along a stable horizontal-then-vertical Manhattan path and is submitted as one ordered gateway batch, so any invalid cell rolls the whole drag back.

After a successful confirmation, repeat mode keeps the selected tool but clears transient anchors. Non-repeat mode exits placement. Cancel clears every transient field and never submits a command.

## Editor and migration parity

No authored schema field changed in S05-P02. `UHansaBuildingDefinition` already exposes footprint width/height, road requirement and shoreline requirement through complete schema metadata, deterministic JSON Schema, validation, compiled-registry hashing, generic Authoring Studio editing and the reviewed 14-building asset set. Per-house entitlement is scenario/research state rather than a duplicated building-definition prerequisite list.

There is therefore no Data Asset migration for this increment. Snapshot/save implementations must preserve the new placement state and fingerprint version; future save-format work must add an explicit version migration rather than inferring occupancy from Actors. S05-P03 now projects these records through the disposable Actor layer described in [WorldPlacementProjections.md](WorldPlacementProjections.md). S05-P04 may add semantic preview/actions and screenshot evidence, but neither layer may bypass this validator or gateway.

## Verification

`Hansa.Simulation.Placement.*` covers footprint rotation, boundaries, terrain, blockers, shoreline, roads, entitlements, ownership, overlap rollback, command events, projection access, road-drag confirmation, cancel/repeat behavior and canonical restore order. `Hansa.Content.World.LubeckPlacementGrid` locks the production grid dimensions, topology families, entitlement construction and world/grid round trip.
