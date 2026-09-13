# Visible local wagon transport

The local production economy now renders every active road delivery as a physical instance of the approved P19 wagon Blueprint. A grain pickup from `Building.GrainFarm` to `Building.Market`, a grain delivery from `Building.Market` to `Building.Mill`, and every other dispatched local logistics job use the same authoritative job identity, cargo good, quantity, endpoints, road route, timing, pause state, and delivery result as the simulation.

## Runtime contract

`AHansaCargoProjectionManager` reconciles one `AHansaCargoVehiclePresentation` actor for each authoritative local logistics job. It resolves `Vehicle.Wagon` through `DA_Vehicle_Wagon` to `/Game/Mesh/hansa-vehicles/BP_Wagon_Review`; a missing or invalid approved class produces an explicit presentation failure instead of a substitute mesh.

The displayed path starts at the source building anchor, follows the exact stored road cells, and ends at the destination building anchor. City-inventory endpoints resolve to the selected physical market. Transform, wheel rotation, cargo visibility, lane offsets, and fractional interpolation are read-only presentation state. They cannot change inventory, dispatch jobs, reserve stock, reconnect roads, or complete delivery.

Each observation exposes the delivery and request identities, source and destination building identities and definitions, good, quantity, road distance, elapsed and remaining travel ticks, logistics status, pause reason, progress, world position, visibility, and presentation failure. The inspector uses these fields to show the actual cargo and journey, such as `6.0 Grain aboard · GrainFarm to Market` or `4.0 Grain aboard · Market to Mill`.

Loaded wagons remain visible when an in-transit road becomes disconnected. Their progress and cargo freeze until the authoritative job resumes after reconnection. Awaiting-pickup, in-transit, paused, arrival, cancellation, and stale-job transitions retain or release the same semantic actor as appropriate. Save/load reconstructs the job identity, cargo, route progress, selection, and visual actor without duplicating inventory or delivery.

Concurrent deliveries receive deterministic presentation-only lane and convoy offsets so wagons with similar progress do not occupy the same pixels. Every authoritative job remains represented. Actors are pooled after completion or cancellation and reused on later jobs; the explicit local presentation safety limit is 128 active jobs. A six-job performance fixture averages 0.036 ms per synchronization across 250 samples against a 5 ms budget.

## Assets and visual scope

No new model, material, raster, or GUI design was generated. The production wagon remains the approved P19 asset:

- `Content/Mesh/hansa-vehicles/BP_Wagon_Review.uasset`
- `Content/Hansa/Core/Vehicles/DA_Vehicle_Wagon.uasset`
- P19 provenance: [Enhanced MVP vehicle family](EnhancedMvpVehicleFamily.md)

The existing native inspector and game HUD render the dynamic text, quantities, status, focus, and progress. Generation mode: **none**. The captures below are native game evidence rather than production textures.

## Verification

- `Hansa.Integration.CargoProjection.AllConcurrentLocalDeliveriesVisible` proves six simultaneous authoritative jobs create six distinct wagon actors and positions, then return them to the pool.
- `Hansa.Integration.CargoProjection.RoadBreakAndReconnect` proves a loaded wagon freezes with exact cargo, preserves actor identity, resumes after road repair, and never duplicates.
- `Hansa.Integration.CargoProjection.LocalDeliverySaveLoad` proves semantic identity, cargo, endpoints, progress, selection, and actor restoration.
- `Hansa.Integration.VisibleWagon.RealViewport` starts through the production frontend, creates real production-chain requests, selects the physical wagon, opens the native inspector, and captures Grain Farm to Market and Market to Mill while multiple deliveries are visible.
- Ten `Hansa.Simulation.Logistics`, eleven `Hansa.Integration.Save`, three `Hansa.Integration.RuntimeSimulationHost`, six `Hansa.UI.Inspector`, and seven focused cargo-projection tests pass.
- The two-player authority proof passes. Runtime wagon actors remain client-side read-only projections of the replicated authoritative snapshot.
- HansaEditor Development, Hansa Development, and the Shipping exclusion audit pass.

Evidence runs:

- `Saved/BuildArtifacts/20260911-122849159-automation-Hansa.Integration.CargoProjection`
- `Saved/BuildArtifacts/20260911-121522065-visible-wagons-1280-720`
- `Saved/BuildArtifacts/20260911-121723982-visible-wagons-1920-1080`
- `Saved/BuildArtifacts/20260911-121851085-automation-Hansa.Simulation.Logistics`
- `Saved/BuildArtifacts/20260911-121913981-automation-Hansa.Integration.Save`
- `Saved/BuildArtifacts/20260911-121935807-automation-Hansa.Integration.RuntimeSimulationHost`
- `Saved/BuildArtifacts/20260911-121957466-automation-Hansa.UI.Inspector`
- `Saved/BuildArtifacts/20260911-122244809-two-player-authority`
- `Saved/BuildArtifacts/20260911-122342789-build-Hansa-Win64-Development`
- `Saved/BuildArtifacts/20260911-122405571-shipping-exclusion-Win64/result.json`

Selected native captures and sibling typed evidence are preserved under `Docs/Images/World/VisibleWagons/` at 1280×720 and 1920×1080. Original-size review confirmed distinct wagon silhouettes, loaded cargo cues, road placement, readable selection, and exact inspector cargo and endpoints at both routes.

## Limits

The approved wagon has no horse, driver, animated harness, or suspension physics, so this implementation does not claim them. Lane separation is cosmetic and does not add road capacity, collision, overtaking, or traffic rules. The presentation renders local production deliveries within the loaded city; it does not replace the separate authored intercity trade-route presentation.