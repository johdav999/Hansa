# Hansa — MVP Local Logistics

`S06-P02` adds an actor-independent local logistics layer between production inventories, city markets, warehouses and dock inventories. It runs in `VehicleMovementAndTransfers`, before construction/production, and uses the authoritative placement road graph rather than presentation actors.

## Authoritative model

- A request identifies source inventory, destination inventory, good, total quantity and priority. Requests are canonicalized by stable ID.
- Production recipes create high-priority input requests when a completed building lacks one cycle of inputs. Produced goods create normal-priority collection requests toward the first stable connected market, warehouse or dock inventory with capacity.
- A job reserves source stock at dispatch and is capped by `JobCapacity` plus the global concurrent-job limit.
- Stock stays in the source while a job waits for its pickup tick. Pickup records an explicit `LocalLogistics.Pickup` sink movement and transfers ownership to the job's `CargoQuantity`.
- Delivery records an explicit `LocalLogistics.Delivery` source movement only after the road-derived completion tick. A destination that becomes full retains the cargo in transit and reports `DestinationFull`; goods never silently disappear or teleport.
- Completed road placements are the only traversable cells. Building inventories connect through orthogonally adjacent road cells. A city-owned market inventory is the documented aggregate hand-off over that city's completed local road network.

The fixed policy contains per-job capacity, pickup delay, ticks per road cell and maximum concurrent jobs. Priority ordering is descending priority, then creation tick, then stable request ID. Causal blockers are `SourceInventoryMissing`, `DestinationInventoryMissing`, `DisconnectedRoad`, `SourceStockUnavailable`, `DestinationFull` and `FleetCapacity`.

## Read and automation parity

`FHansaSimulationReadOnlyAccess` exposes typed road-path, request and job queries plus owning request/job projections and snapshot state. `FHansaStateHasher` reports Logistics as authoritative subsystem 15 under determinism fingerprint version 13.

The controlled fixture adapter allowlists:

- `logistics.requests`
- `logistics.jobs`
- `logistics.path` with `sourceInventoryId` and `destinationInventoryId`

These return projections only; they do not expose mutable containers or presentation actors.

## Verification

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Logistics
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa
pwsh -File Scripts/RunHansaMcpTests.ps1
pwsh -File Scripts/VerifyRepositoryConventions.ps1
```

The focused suite proves production-created requests, capacity and pickup/delivery delay, cargo conservation, disconnected roads, full destinations, stable priority under competing demand, typed graph connectivity for market/warehouse/dock endpoints, and state-hash equality under reversed discovery order.
