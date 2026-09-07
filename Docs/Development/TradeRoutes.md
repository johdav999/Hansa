# S09-P02 Trade Routes

S09-P02 adds the MVP's first deterministic intercity transport layer. It is intentionally small: one sea vehicle family and route network, one land vehicle family and route network, ordered stops, unconditional load/unload actions, minimum source reserves, finite cargo capacity, authored travel time, and per-travel-tick upkeep.

## Authored definitions

- `Vehicle.Cog`: sea mode, 60,000 milli-units capacity, 12 pfennig upkeep per travel tick.
- `Vehicle.Wagon`: land mode, 20,000 milli-units capacity, 5 pfennig upkeep per travel tick.
- `Route.BalticSea`: bidirectional Lübeck–Hamburg (8 ticks) and Lübeck–Rostock (10 ticks) connections.
- `Route.SaltRoad`: bidirectional Lübeck–Lüneburg connection (6 ticks).

The four production assets live under `Content/Hansa/Core/Vehicles` and `Content/Hansa/Core/Routes`. They are scanned and always cooked as runtime primary assets. The Authoring Studio, generic schema registry, validation compiler, content hashing, disk-reload tests, and Shipping runtime loader all recognize these definition classes. Route connection references fail closed when their `City.*` identity is absent from the authored market-city catalog.

## Authoritative state and commands

Each vehicle owns a ledger-backed cargo inventory. Capacity is enforced by that ledger and vehicle cargo is derived from the inventory's used capacity. A route owns an ordered cyclic stop list; each stop owns ordered cargo actions. The only MVP condition is `Always`. Its explicit schema enum and the route definition's fixed cargo-rule schema version are safe extension points for later conditional trading without accepting an unversioned rule language now.

Create, edit, activate/deactivate, and cancel are closed typed gameplay commands at command schema version 5. The shared gateway validates command authority, route and vehicle identities, single-route vehicle assignment, owner equality, compatible sea/land mode, cargo-inventory capacity, current vehicle location, known goods, positive quantities, ordered stop bounds, and every cyclic leg's reachability. Edits require an inactive route at its first stop with empty cargo. Failed commands leave the authoritative tick, state, command history, and event sequence unchanged.

The integrated Lübeck shortage scenario and version-4 production fixtures start with one cog route to Rostock and one wagon route to Lüneburg. Both are inactive, so scenario economics remain unchanged until an explicit activation command is accepted.

## Deterministic execution

On activation, stop actions execute in declared order and the vehicle departs. Loads are bounded by the requested quantity, unreserved source stock above the action's minimum reserve, and remaining vehicle capacity. Unloads are bounded by available vehicle stock and destination capacity. Partial and zero transfers are retained as explicit partial/missed outcomes instead of silently disappearing.

Travel advances by one tick in the fixed `VehicleMovementAndTransfers` phase. Authored upkeep is charged only for traveling ticks. Arrival occurs after exactly the authored number of ticks, updates the vehicle city, records a completed leg, and schedules the destination actions for the following vehicle phase. Cancellation is immediate and deterministic: travel stops, pending actions are cleared, and carried cargo remains in the vehicle inventory.

Route creation/edit/activation/cancellation, departure/arrival, successful transfer, and missed/partial cargo produce ordered domain events. Read-only vehicle and route projections expose current city, cargo/free capacity, upkeep, lifecycle, progress, stop plan, leg count, missed-action count, and last transfer without exposing mutable containers.

## Determinism and verification

Determinism fingerprint version 15 includes every vehicle field, ordered route stop/action, lifecycle/travel counter, and last-transfer record. `Hansa.Simulation.Trade.CapacityReserveArrivalReplay` and `Hansa.Simulation.Trade.ValidationCancellationMissedCargo` cover capacity, reserve protection, authored arrival/delivery timing, upkeep, ownership, typed rejection causes, cancellation with cargo preservation, missed cargo, projections, and identical replay fingerprints.

Advanced price-triggered, conditional, opportunistic, or multi-vehicle trading remains outside S09-P02.
