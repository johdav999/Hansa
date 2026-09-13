# Road-disconnection world indicator

Completed buildings that require roads show a rotating 3D broken-road symbol only when their authoritative world projection reports no orthogonally adjacent completed road. Road adjacency and market connectivity are separate checks: a building connected to a local road does not show this marker merely because the road component lacks a completed Market.

## Runtime behavior

`AHansaBuildingWorldProjectionActor` creates a `RoadDisconnectedMarker` component using `/Game/Mesh/hansa-road-disconnected-symbol/SM_HansaRoadDisconnectedSymbol`. The component is visible only when all three conditions are true:

1. Construction is complete.
2. The building type requires road access.
3. The authoritative projection reports no adjacent completed road.

The marker is positioned 250 cm above the authored building visual top, displayed at scale 2.25, and rotated 45 degrees per second around local +Z. It has no collision, overlap events, shadows, or navigation effect. Actor tick is enabled only while the marker is visible. Road reconnection hides the component and disables tick during the same projection update.

The feature does not mutate simulation state. It visualizes `FHansaBuildingWorldProjection::bRequiresRoad` and `bHasRoadAccess`. The independent `bHasMarketAccess` result still drives production, population, inspector cause/remedy feedback, and local-market logistics.

Verbose `[RoadConnectivity]` diagnostics report the building ID/type/city, anchor and footprint, adjacent road cells, road-only result, market result/failure, selected Market, road distance, marker decision, and projection-refresh event triggers. Enable `LogHansa` and `LogHansaSimulation` at `Verbose` when reproducing a connectivity problem.

## Assets

- Blender source and evidence: `SourceArt/Generated/Props/HansaRoadDisconnectedSymbol_20260911/`
- Unreal mesh and materials: `Content/Mesh/hansa-road-disconnected-symbol/`
- Native gameplay captures: `Docs/Images/World/RoadDisconnectedIndicator/`

## Acceptance evidence

- HansaEditor Development build: `Saved/BuildArtifacts/20260911-130401465-build-HansaEditor-Win64-Development`
- Focused marker test: `Saved/BuildArtifacts/20260911-130418546-automation-Hansa.Integration.RoadConnectionIndicator.VisibilityAndRotation`
- Runtime simulation host: `Saved/BuildArtifacts/20260911-130638878-automation-Hansa.Integration.RuntimeSimulationHost`
- Wagon/cargo projection: `Saved/BuildArtifacts/20260911-130748013-automation-Hansa.World.CargoProjection`
- Native 1280 x 720 capture: `Saved/BuildArtifacts/20260911-130443778-production-inspector-1280-720`
- Native 1920 x 1080 capture: `Saved/BuildArtifacts/20260911-131236960-production-inspector-1920-1080`
- Two-player authority proof: `Saved/BuildArtifacts/20260911-130824116-two-player-authority`
- Hansa Win64 Shipping build: `Saved/BuildArtifacts/20260911-130954179-build-Hansa-Win64-Shipping`
- Shipping-exclusion audit: `Saved/BuildArtifacts/20260911-131023613-shipping-exclusion-Win64/result.json`

The aggregate `Hansa.World.Road` run at `Saved/BuildArtifacts/20260911-130710625-automation-Hansa.World.Road` confirmed its topology, production projection, and command projection tests pass. Five unrelated `NativeReviewCapture` cases fail because they require an isolated authored review scene; they do not exercise this indicator.

The repository convention audit at `Saved/BuildArtifacts/20260911-131038420-repository-conventions/result.json` remains red due existing Windmill transient-path metadata, existing staging references, and existing test names. It did not flag the new indicator files or test name.
