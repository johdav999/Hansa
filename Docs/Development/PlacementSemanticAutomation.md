# Lübeck placement semantic automation

S05-P04 adds a controlled, screenshot-capable placement proof over the authoritative S05-P02 placement model and S05-P03 world projections. `empty_lubeck_build_v1` owns a real 60×40 Lübeck placement grid, compiled Road and 2×3 road-dependent Warehouse definitions, one house entitlement set, and initially empty simulation state.

## Authority boundary

Slate clicks and semantic `activate` actions call the same device-neutral fixture intents. Those intents update `FHansaPlacementSession`, preview through `FHansaPlacementRules`, and confirm only by submitting typed `FHansaPlaceBuildingCommand` batches to `FHansaGameplayCommandGateway`. The host observes the live `AHansaStrategyCameraPawn` and synchronizes the fixture's read-only projection into `AHansaPlacementProjectionManager`, so accepted road/building records also create the normal disposable world Actors. The semantic registry has no occupancy setter, building setter, raw UObject path, reflection surface, or arbitrary command payload.

## Component inventory

| Component | Native implementation | Semantic root |
| --- | --- | --- |
| Screen shell | Slate border/scale box | `BuildMode.Screen` |
| Camera status | Native text with typed camera state | `BuildMode.Camera` |
| Map and target cells | Slate map panel and normal button intents | `BuildMode.Map` |
| Placement preview | Native status/overlay reference | `BuildMode.Placement.Preview` |
| Validation cause/remedy | Native alert plus two text nodes | `BuildMode.Placement.Validation` |
| Build selection | Road and Warehouse buttons | `BuildMode.Tool.*` |
| Controls | Rotate, Repeat, Confirm and Cancel buttons | `BuildMode.Action.*` |
| Authoritative result | Stable status node with typed entity value | `BuildMode.Result.Building` |

The native screen uses the project palette and compact bottom build strip. Generated images under `Docs/Images/UI/Placement/` are visual references only; no raster was imported, resized, or shipped.

## State contract

Every node retains the existing boolean state (`visible`, `enabled`, `focused`, `selected`, `loading`, `warning`, `error`) and may add `valueType` plus `value`. Camera, grid coordinate, building definition, rotation, repeat flag, placement preview, placement validation and building entity identity therefore remain machine-readable without parsing labels.

The implemented flow exercises these states:

1. Load `empty_lubeck_build_v1` and assert zero placements.
2. Select `BuildMode.Tool.Road`, target `BuildMode.Map.RoadTarget`, validate and confirm.
3. Select `BuildMode.Tool.Warehouse` and target `BuildMode.Map.InvalidTarget`.
4. Assert `BuildMode.Placement.Validation` is `error=true`, `valueType=placement-validation`, `value=RoadRequired`, with cause “Road required” and remedy “Build next to a road”.
5. Target `BuildMode.Map.ValidTarget`, assert `selected=true` and `error=false`, then confirm.
6. Wait for `BuildMode.Result.Building.selected=true` and assert `valueType=building-entity-id`, `value=Building:2:0`.
7. Capture native 1280×720 and 1920×1080 evidence without post-capture resizing.

`Tools/HansaMcp/scripts/placement-flow.js` performs the real named-pipe flow. The fake endpoint mirrors it for fast sidecar contract coverage.

## Evidence

Placement captures are written beneath `Saved/TestEvidence/Automation/S05P04/<bundle>/`. Metadata records the exact fixture, flow ID, native dimensions, `postCaptureResized=false`, simulation tick, UI revision, semantic snapshot, content hash, structural assertions and `structuralAssertionsPassed`. The structural evidence checks fixture load, authoritative placement count, camera/validation/result semantic existence and the selected result entity, so the flow cannot pass from pixels alone.

## Verification

```powershell
pwsh -NoProfile -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Architecture.Automation.EmptyLubeckPlacementFlow -SkipBuild
pwsh -NoProfile -File Scripts/RunHansaMcpTests.ps1
npm --prefix Tools/HansaMcp run smoke:placement
```

The last command requires an explicitly enabled Development game with `FixtureControl` permission and the matching short-lived pipe/token environment.
