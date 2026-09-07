# S07-P04 implementation record

## Runtime architecture

`UHansaInspectorPresentationModel` owns the reusable production/residence presentation snapshot and a shared `FHansaCausalPresentation`. Production and residence causal builders consume authoritative simulation projections. `SHansaContextInspector` only renders the supplied values; it does not reproduce production, workforce, market, or population formulas.

`SHansaRootHud` integrates the inspector with grouped alerts. Alerts expose severity, age, affected object, cause/evidence/remedy, frame/open-cause/snooze/pin actions, and pinned tracking. Opening from an alert stores the originating semantic action; closing restores focus to that action or falls back to the alert-stack toggle.

The building world projection carries the authoritative production blocker to selection, and the strategy camera exposes a bounded focus intent used by alert framing.

## Native component contract

The shipping UI is native Slate and follows the stable order: identity/state, most important result, inputs/outputs or needs, problem/cause, actions/automation, history. Warning and critical states use glyph, text, and edge treatment in addition to color. Text wraps inside the approved responsive inspector width, and semantic nodes expose all sections, causal details, actions, tooltips, alerts, and pinned trackers.

## Reference assets

All PNGs in this directory are visual references only. They remain at their generator-native pixel sizes and are not imported into `Content/Hansa/UI`:

- `inspector--causal-navigation-screen--warning--1672x941--v1.png` — composed hierarchy/style anchor.
- `inspector--production-building--warning--864x1821--v2.png` — reusable inspector reference.
- `alert--production-group--warning--1122x1402--v1.png` — grouped alert reference.
- `tooltip--open-cause--focus--1740x904--v1.png` — causal tooltip/focus reference.

Each selected image has a sibling prompt record. All four were inspected at original resolution; no resampling or production raster import was performed.

## Verification contract

Automation covers production and residence data projection, stable semantic section order, alert grouping and causal fields, open-cause, pin, snooze, tooltip semantics, focus restoration, long localization strings, and native 1280×720 / 1920×1080 screenshot evidence with structural assertions.
