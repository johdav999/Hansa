# Managed building and road world projections

## Authority and lifecycle

S05-P03 projects authoritative placement records into disposable world Actors. `FHansaBuildingWorldProjection` is an immutable join of building identity, placement, footprint, construction progress and production blocker state. It is built by `FHansaSimulationReadOnlyAccess::BuildProjection`; it never exposes mutable simulation containers.

`AHansaPlacementProjectionManager` consumes only this read model and relevant domain-event notifications. `BuildingPlaced`, production-blocker and production-active events request a refresh, while the accompanying projection remains the source of truth. The manager cannot change occupancy, construction, inventory, production or ownership.

The manager maintains one `AHansaBuildingWorldProjectionActor` per full entity identity (numeric value plus generation). Reconciliation is transactional and canonical:

- new IDs create Actors;
- changed records update the same Actor;
- absent IDs destroy their managed Actors;
- duplicate or malformed projection records are rejected without partially changing the visible mapping;
- an externally destroyed Actor is recreated on the next synchronization;
- map/foundation changes tear down the old set and reconstruct entirely from the latest projection.

`AHansaGameMode` guarantees that blank runtime maps receive one projection manager alongside the Lübeck foundation. The future scenario/runtime host supplies projections and committed event batches to it; no Level Blueprint owns placement state.

## Component inventory

All S05-P03 visuals are native Engine geometry and dynamic material colors. There are no generated, imported or resampled raster assets.

| Component | Implementation | Purpose |
| --- | --- | --- |
| Projection manager | Managed C++ Actor | Stable entity-to-Actor mapping, refresh, teardown and rebuild |
| Building body | Native cube mesh | Simple ready-state building mass fitted to the authored footprint |
| Road segment | Native cube mesh | Low-profile one-cell road presentation |
| Construction placeholder | Native cube mesh | Low amber foundation pad while progress is incomplete |
| Selection outline | Native cube mesh | Brass footprint plate extending beyond the body silhouette |
| Status marker | Native cone or sphere | Construction or blocked state with shape as well as color |
| Identity/status tags | Actor/component tags | Stable entity, definition, road, selectable and status inspection |

The palette uses the existing design tokens: Oak for roads, Hanseatic Brick for ready building masses, Warning Amber for construction, Brass for selection, and Oxblood for blocked state. These are MVP presentation primitives, not final building art.

## State matrix

| State | Body | Placeholder | Marker | Selection |
| --- | --- | --- | --- | --- |
| Under construction | Hidden | Amber pad visible | Amber cone visible | Optional brass footprint plate |
| Ready | Building or road mesh visible | Hidden | Hidden | Optional brass footprint plate |
| Blocked | Building or road mesh visible | Hidden | Oxblood sphere visible | Optional brass footprint plate |
| Selected | Status-dependent | Status-dependent | Status-dependent | Brass footprint plate visible |
| Deselected | Status-dependent | Status-dependent | Status-dependent | Hidden |
| Removed/map teardown | Actor destroyed | Actor destroyed | Actor destroyed | Selection identity cleared |

Color never carries the status alone: construction changes the body/placeholder silhouette and uses a cone; blocked uses a sphere; the status is also exposed as `Hansa.Status.*` and through `GetStatusName()`.

Hover, pressed, keyboard/controller focus, warning and error treatments belong to the build-mode input/semantic layer in S05-P04. S05-P03 exposes the selected state needed by that layer without inventing a second selection authority.

## Transform and map contract

Actors derive their center from canonical occupied cells, their unrotated dimensions from the compiled building definition, and their yaw from the authoritative quarter-turn rotation. They attach to `AHansaLubeckWorldFoundation`, so a transformed or reloaded foundation preserves the same local grid relationship. Roads and buildings use the 400-unit Lübeck cell size without raster scaling.

The managed Actors are local presentation (`bReplicates = false`, no tick). Multiplayer clients reconstruct them from their authorized simulation projections rather than replicating presentation Actors as economy state.

## Missing production art

The following are deliberately recorded rather than blocking mechanics:

- definition-specific building meshes and material instances;
- connected road corners, junctions and end caps;
- scaffold geometry and construction-stage variants;
- authored selection-outline material and accessibility thickness settings;
- definition-specific blocker icons.

Replacing these primitives must preserve stable identity tags, footprint transforms, status redundancy, collision/selectability behavior and the projection-only authority boundary.

## Verification

`Hansa.UI.World.PlacementProjectionRegistry` covers canonical creation, no-op refresh, in-place update, removal, duplicate rejection and reload reconstruction. `Hansa.UI.World.PlacementProjectionActorLifecycle` creates a transient world and verifies managed Actor creation, stable mapping, construction/ready visual swaps, road classification, selection outline, removal, new-foundation reconstruction and explicit teardown.
