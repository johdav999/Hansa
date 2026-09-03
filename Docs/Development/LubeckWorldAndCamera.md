# Lübeck MVP world and strategy camera

## Runtime contract

S05-P01 establishes the buildable-world presentation boundary without adding authoritative placement state or final environment art. The checked-in World Partition level is `/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP`, and the default game mode is `AHansaGameMode`. On game initialization it guarantees exactly one `AHansaLubeckWorldFoundation`, one `AHansaPlacementProjectionManager` and one `AHansaLubeckAutomationStart` exist, so blank integration-test maps also remain directly playable.

The stable map identity is `Region.Lubeck.Mvp`; it is never derived from a package, level filename, display text, provider output, or Actor name. The deterministic start identity is `World.Lubeck.AutomationStart` at location `(-3200, -700, 150)` and yaw `35°`. Automation and future fixtures should resolve these IDs and must not depend on Actor iteration order.

The authored camera rectangle is `[-12000, -8000]` to `[12000, 8000]` Unreal units. Camera focus is clamped to it after every device-neutral intent update.

## Representative topology

`AHansaLubeckWorldFoundation` assembles fixed native Engine cube meshes at construction time. They are placeholders, not generated art and not authoritative simulation records. The composition provides:

- three overlapping land masses with space for housing, civic/logistics buildings, all four production chains, and later placement-grid coverage;
- three independently tagged shore segments for shoreline validation;
- a water plane and open eastern harbor basin;
- a quay and two piers for dock/fishery and sea-route presentation;
- two datum roads connecting the start area and quay.

Every component has one semantic surface tag: `Hansa.World.Surface.Land`, `.Shore`, `.Water`, `.Harbor`, or `.Road`. Collision is query/physics enabled on land, shore, harbor, and road surfaces; the underlying water plane does not intercept placement or selection traces. S05-P02 may consume the tags for preliminary terrain classification but must keep authoritative occupancy and placement in simulation records.

S05-P02 now provides the authoritative 60×40 deterministic grid and native world/grid conversion described in [Placement.md](Placement.md). The visual surface tags remain presentation aids; placement terrain, ownership, entitlements and occupancy come from the simulation grid contract.

S05-P03 adds the managed building/road Actor layer described in [WorldPlacementProjections.md](WorldPlacementProjections.md). Projected Actors attach to the foundation for transform-safe reloads, but remain disposable clients of simulation read models.

## Input and selection

`AHansaStrategyPlayerController` creates a fallback Enhanced Input mapping in C++ when no Blueprint assets are assigned. All bindings feed the same intent methods on `AHansaStrategyCameraPawn`, which also makes the controlled automation path explicit.

| Intent | Mouse/keyboard | Controller-ready mapping |
| --- | --- | --- |
| Pan | WASD, arrows, viewport-edge mouse pan | left stick |
| Fast pan | left Shift | right shoulder |
| Zoom | mouse wheel | left/right triggers |
| Rotate | Q/E | right-stick X |
| World select | left mouse button | face button bottom |

Zoom direction is positive toward the world. Pan is relative to camera yaw. Analog pan is normalized, rotation and pan are frame-rate independent, zoom and focus are clamped, and invalid non-finite intents cannot mutate state. The camera is presentation-only and is deliberately absent from simulation state/checksums.

World selection first traces the pointer on the Visibility channel. If no pointer hit is available, it traces from viewport center for controller use. `OnWorldSelectionChanged` publishes the selected Actor and full hit result; it does not mutate gameplay state.

## Blueprint/level composition

The checked-in level contains the C++ foundation and stable PlayerStart. Artists may extend its production composition without changing gameplay logic:

1. Keep one `AHansaLubeckWorldFoundation` at world origin. The game mode detects it and does not spawn a duplicate.
2. Optionally create `BP_LubeckWorldFoundation` derived from the C++ class and replace individual placeholder meshes while preserving component transforms, surface tags, native scale, camera bounds, map ID, and start ID.
3. Keep World Settings GameMode Override set to `AHansaGameMode`. Do not place economy or placement authority in the Level Blueprint.
4. Assign production content to World Partition Data Layers/HLODs without making streamed cells authoritative for simulation.
5. If content-authored Enhanced Input assets are desired, create `IMC_StrategyCamera` and the five actions shown above, then assign all six controller defaults together. Leaving them all empty selects the tested C++ fallback.

Data Layer population, HLOD generation and final shoreline/environment treatment remain content-production work. None is required for deterministic simulation, and no full-screen or raster environment mockup ships from this increment.

## Verification

`Hansa.UI.World.StrategyCameraIntents` covers pan orientation and normalization, fast pan, rotation, zoom limits, bounds and invalid input. `Hansa.Content.World.LubeckMapContract` locks the stable IDs, verifies the start is in bounds, and proves all required surface families exist on the foundation class default.
