# Hansa — MVP Local Logistics

`S06-P02` adds an actor-independent local logistics layer between production inventories, city markets, warehouses and dock inventories. It runs in `VehicleMovementAndTransfers`, before construction/production, and uses the authoritative placement road graph rather than presentation actors.

## Authoritative model

- A request identifies source inventory, destination inventory, good, total quantity and priority. Requests are canonicalized by stable ID.
- Production recipes create high-priority input requests when a completed building lacks one cycle of inputs. Produced goods create normal-priority collection requests toward the first stable connected market, warehouse or dock inventory with capacity.
- A job reserves source stock at dispatch and is capped by `JobCapacity` plus the global concurrent-job limit.
- Stock stays in the source while a job waits for its pickup tick. Pickup records an explicit `LocalLogistics.Pickup` sink movement and transfers ownership to the job's `CargoQuantity`.
- Delivery records an explicit `LocalLogistics.Delivery` source movement only after the road-derived completion tick. A destination that becomes full retains the cargo in transit and reports `DestinationFull`; goods never silently disappear or teleport.
- Every dispatched job stores its selected market, ordered road cells, elapsed travel ticks, remaining travel ticks, and any topology pause cause. This route is authoritative for both simulation and cargo presentation.
- Completed road placements are the only traversable cells. Building inventories connect through orthogonally adjacent road cells; diagonal contact never grants access.
- A city-owned inventory is anchored to a completed building whose authored `bProvidesMarketAccess` capability is true. The reviewed MVP provider is `Building.Market`. It can use only the road cells orthogonally adjacent to the selected provider, rather than every road in the city.
- `FHansaInventoryInitialization::BuildingId` stores that market anchor for buildable cities. Several city inventories may coexist only when their `(CityId, BuildingId)` pairs differ. Legacy inventories without an anchor remain in place and resolve to the lowest completed market ID; loading never copies or redistributes their stock.
- A local delivery is eligible only when both endpoints share a completed road component that also reaches a completed market. An isolated road component, unfinished road, unfinished market, warehouse, or harbor cannot independently grant city-economy access.
- When several markets qualify, the query selects the shortest delivery path and then the lowest stable market building ID. Connectivity is derived directly from authoritative construction and placement state on every query, so completion and removal take effect without presentation state or a stale graph cache.

The fixed policy contains per-job capacity, pickup delay, ticks per road cell and maximum concurrent jobs. Priority ordering is descending priority, then creation tick, then stable request ID. Causal blockers are `SourceInventoryMissing`, `DestinationInventoryMissing`, `DisconnectedRoad`, `SourceStockUnavailable`, `DestinationFull` and `FleetCapacity`.

Production recipes use their building inventory as the physical buffer. A disconnected producer can finish work from inputs already stored there and retain its outputs until the buffer fills, but logistics creates no replacement request and raw producers never write directly into city stock. Reconnection makes the existing deterministic request eligible again without duplicating demand.

Resident goods consumption uses the same building-to-market query from the completed residence to its assigned market inventory. Market projections may aggregate several spatial inventories for citywide totals, but consumption, reservations, production deliveries and route actions operate on the actual inventory ID. A buildable city sea route can load or unload only through a completed `Building.Dock` whose road component reaches that inventory market. Market-only cities with no placement map retain their documented abstract background and intercity supply model.

## Topology changes during delivery

Active jobs revalidate the physical market path on every logistics tick, before pickup, movement, and delivery. An elapsed `DeliveryTick` is an estimate and history value; it never authorizes transfer without a currently valid route.

- Before pickup, a disconnected job enters `PausedAwaitingPickup`, releases its source reservation immediately, and leaves stock at the source. Reconnection makes the same job reacquire stock deterministically and wait the configured pickup delay. This release-and-reacquire policy prevents disconnected reservations from locking inventory indefinitely.
- After pickup, a disconnected job enters `PausedInTransit`. Its exact cargo, ordered route, elapsed travel, and remaining travel stay authoritative and do not advance while blocked.
- If another valid shortest route exists, the job adopts it deterministically. The new remaining duration subtracts already completed travel and retains at least one connected travel tick, so a reroute neither restarts the journey nor delivers immediately.
- Paused jobs remain obligations but do not consume `MaximumConcurrentJobs`; eligible moving deliveries can use the fleet capacity while a route is blocked.
- Removing a source, destination, or selected market while a job is active is rejected as `TargetHasCargoObligations`. Roads remain removable so topology can change normally.
- Cargo actors consume the job's saved `RouteCells` and travel counters. Presentation never rebuilds a competing route from current actors or road meshes.

## Read and automation parity

`FHansaSimulationReadOnlyAccess` exposes typed road-path, request and job queries plus owning request/job projections and snapshot state. Road-path results include market eligibility, selected market building ID, route distance, a typed causal failure, and stable message/remedy keys. `FHansaStateHasher` reports Logistics as an authoritative subsystem.

The controlled fixture adapter allowlists:

- `building.list`
- `building.market_access` with `buildingId`
- `logistics.requests`
- `logistics.jobs`
- `logistics.path` with `sourceInventoryId` and `destinationInventoryId`

Both production and strategic/playable fixtures return these projections. They do not expose mutable containers or presentation actors. `building.market_access` is populated for every completed road-dependent building even when it has no current request, and reports the selected market ID, road distance, stable failure/message/remedy keys, and any paused delivery count.

The production inspector consumes the same building-world projection. Its native market-access row and semantic node distinguish no adjacent road, no operational market, a market without road access, a severed market network, and a delivery paused by a topology change. Missing recipe input and full output storage retain separate causes and remedies. Reconnecting the road resumes an existing paused job instead of creating a presentation-only recovery.

## Verification

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Logistics
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa
pwsh -File Scripts/RunHansaMcpTests.ps1
pwsh -File Scripts/VerifyRepositoryConventions.ps1
```

The focused suite proves production-created requests, capacity and pickup/delivery delay, cargo conservation, disconnected roads, full destinations, stable priority under competing demand, physical market anchoring, four-way adjacency, incomplete construction handling, deterministic multiple-market selection, city boundaries, state-hash equality under reversed discovery order, road removal before pickup and immediately before delivery, reservation release, alternate routing, paused fleet capacity, demolition guards, reconnection, and save/load while cargo is blocked.
