# S07-P02 root HUD implementation

## Component inventory and states

The shipping shell is assembled from native Slate widgets over a hit-test-transparent city-view center. It contains:

- a safe-area-anchored top status bar with house, money/trend, population, workforce, selected-city breadcrumb, date/season, speed, research, connection and menu regions;
- a top-left alert stack with expanded and collapsed states plus notice/warning shape-and-label redundancy;
- a bottom-center build/selection host with open and closed semantic state;
- a conditional right inspector host with identity, summary and close control;
- a bottom-right notification layer;
- a separate, non-color keyboard/controller focus announcement layer and brass focus rings around speed controls.

Interactive controls expose default, hover, pressed, selected, disabled and focused behavior through the shared S07-P01 styles. Open, closed, selected, warning and focused states are also represented in the semantic snapshot rather than inferred from pixels.

## Presentation and integration

`UHansaHudPresentationModel` owns complete presentation snapshots and publishes a revisioned C++ multicast event only when state changes. `SHansaRootHud` subscribes once and updates stored native widgets in response. It has no tick, Blueprint tick, raw per-frame text binding or visibility lambda. Player world-selection events update the selection/inspector presentation through `AHansaRootHud`; speed and panel actions return through presentation-model intent methods.

`AHansaGameMode` installs `AHansaRootHud` as its production HUD class. The root uses `SelfHitTestInvisible`, so empty HUD space does not prevent selection of the central city view while child controls remain interactive. Viewport resize notifications update the native presentation surface without polling.

## Responsive layout

The clusters use top, bottom, left and right Slate alignment anchors plus safe margins. Native-size metrics are defined for:

| Viewport | Safe margin | Top bar | Alerts | Bottom host | Inspector | Notification |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1280×720 | 16 px | 64 px | 288×184 px | 720×96 px | 360 px | 320 px |
| 1920×1080 | 24 px | 72 px | 320×224 px | 896×112 px | 400 px | 360 px |

The default shell's structural occlusion is below 30% at both targets, leaving at least 70% of the city view unobstructed. The conditional inspector is excluded from the default occlusion calculation because it opens only for a selection.

## Semantic and test coverage

The runtime widget emits widget-class-neutral nodes with stable IDs, roles, labels, typed values, visibility, enabled, selected, focused and warning state, bounds, and supported activate/focus actions. Automated tests cover event coalescing, both responsive target layouts, alert close/reopen, selection-driven inspector open/close, focus announcement, and speed activation.

## Visual assets and inspection

- Style anchor: `Docs/Images/UI/hansa-ui-main-city-hud.png` (reference only).
- Generation mode: no new generation. The approved anchor and S07-P01 component specification fully resolved the shell, and its scalable surfaces are native Slate geometry.
- New raster assets, source masters, prompt records and imported production textures: none.
- Production-ready outputs: the native runtime HUD, presentation model, responsive metrics and semantic/test surface.
- Inspection: the existing style anchor was inspected at original resolution; palette, hierarchy and edge-cluster treatment were carried into native widgets. No raster was resized, resampled, or shipped as interactive UI.
- Remaining limitation: the current repository still uses the approved fallback engine fonts until the project adds licensed font files; downstream S07 work will populate the build menu and full inspector content inside these hosts.
