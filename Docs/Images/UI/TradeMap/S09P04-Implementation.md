# S09-P04 route-delivery evidence surface

## Component inventory

- Screen shell: `RouteDelivery.Root` at a native 1280×720 or 1920×1080 capture size.
- Navigation: `RouteDelivery.Tab.Route` and `RouteDelivery.Tab.Market`.
- Route panel: `RouteDelivery.RouteEditor`, the Lübeck/Rostock stops, unload/load actions, 30,000-unit source reserve, round-trip time, capacity, upkeep, and expected arrival/delivery ticks.
- Controls: `RouteEditor.Action.Save`, `RouteEditor.Action.Start`, and `RouteEditor.Action.Cancel`.
- Status/feedback: separate departed, arrived, delivered, cargo, lifecycle, simulation tick, state hash, and event-count values.
- Market panel: `RouteDelivery.Market` and `RouteDelivery.Market.State`, including stock/reserve, price, report age, and explicit current/stale text.
- Route geometry and icons: inherited from the native S09-P03 trade-map implementation; no new raster is introduced.
- Decorative imagery: none. The surface uses the centralized Baltic navy, harbor slate, linen, parchment, brass, oxblood, prosperity teal, ink, and chalk tokens.

Interactive controls provide default, hover, pressed, disabled, and focus behavior through native Slate buttons. Tab selection and delivery milestones use semantic selected state plus text; stale state is written explicitly and never communicated by color alone. Error and warning values remain typed semantic state supplied by the automation registry.

## Evidence contract

The route and market views are reconstructed with native Slate widgets. The route fixture owns all changing text and values; nothing dynamic is baked into an image. Native screenshots are written at exactly 1280×720 or 1920×1080 with `postCaptureResized=false` under `Saved/TestEvidence/Automation/S09P04/<bundle>/`.

Every bundle contains:

- the PNG screenshot;
- metadata with fixture, flow, screen, native size, simulation tick, UI revision, and structural assertions;
- the full semantic snapshot;
- a query snapshot with the authoritative state hash, ordered route events, and event count.

The checked-in automation test produces separate `route-editor` and `market-stale` contract bundles. The real named-pipe smoke flow produces `route-editor-delivered` and `market-after-delivery` captures from the live native surface.

## Asset and generation report

There are no new or revised raster assets, production imports, source masters, or prompt files. S09-P04 is a pure implementation/evidence extension of the approved S09-P03 visual design, so no ImageGen pass or resampling was used. The only screenshot outputs are ignored QA evidence, not production assets.

Inspection verifies native pixel dimensions, no post-capture resize, semantic route/market structure, state-hash and event synchronization, explicit stale-report text, reserve-safe partial cargo handling, and palette consistency. A live capture still requires an explicitly enabled Development process with Slate and the local automation pipe; the game-free fake endpoint covers the same contract in CI.
