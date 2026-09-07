# S08-P03 Selected-good panel implementation

## Outcome

The Market tab now uses a bounded 60/40 native Slate composition: the existing virtualized ten-good table remains on the left and a scrollable selected-good causal inspector occupies the right. Selection remains keyed by canonical good ID through sorting, filtering, and authoritative projection refreshes.

## Component inventory

- Screen shell: existing `SHansaCityOverview` Market tab.
- Navigation: existing City Overview tabs and market search/filter toolbar.
- Table/list: compact native `SListView` market table with sticky sortable header.
- Selected-good panel: native header, confidence status, metric grid, causal prose, factor list, consumer list, producer list, and action bar.
- Chart/overlay: bounded, non-animated `SLeafWidget` line chart. It draws the authoritative price-history samples and recent-average rule without per-frame work. A native semantic summary and one semantic node per source point provide the accessible equivalent.
- Controls: `Pin price & stock` is an enabled typed intent for a reported good; `Begin route` is retained visibly but disabled until the S09 route editor exists.
- Status/feedback: current/stale/unknown confidence, signed direction text, blocker text, disabled-action reason, and pin result.
- Icons/decorative imagery: native glyphs and Slate styling only; no generated raster is shipped.

## Authoritative data contract

`FHansaSimulationProjection` now owns immutable copies of market reserve, explanation, consumer, and producer projections alongside its existing market and alert projections. The presentation model formats these query results and never recomputes price causes. Price-history points retain the source simulation tick and milli-mark value in deterministic order and are bounded to the 64-entry market history contract.

Pin state is presentation-only and keyed by `GoodStableId`. Route creation remains unavailable with the explicit reason `Route editor not available yet.` No runtime gameplay identity, definition schema, editor schema, provider integration, or Shipping dependency changed.

## Reference assets

All generated images are design references, inspected at native resolution, and are not imported production textures:

- `selected-good--composed--grain-shortage--1672x941--v1.png` — 1672 x 941
- `selected-good--metrics--current--1402x1122--v1.png` — 1402 x 1122
- `selected-good--chart--current--1692x929--v1.png` — 1692 x 929
- `selected-good--factors--shortage--1086x1448--v1.png` — 1086 x 1448
- `selected-good--relationships--shortage--1024x1536--v1.png` — 1024 x 1536
- `selected-good--actions--route-disabled--2043x770--v1.png` — 2043 x 770

Generation mode was built-in ImageGen. Each reference has a same-basename `.prompt.md` record containing intended use, native dimensions, final prompt, revision notes, and inspection outcome. No raster was stretched, resized, cropped into production components, or imported under `Content/Hansa/UI`.

## Verification

- `HansaEditor Win64 Development`: passed.
- `Hansa.UI.SelectedGood`: 2/2 passed.
- `Hansa.UI`: 29/29 passed.
- `Hansa.Simulation.Market`: 6/6 passed.
- Repository convention audit: S08-P03 additions passed; the command remains red only for two pre-existing unrelated test names in `HansaRuntimeSimulationHostTests.cpp` (`PauseAndSpeed` and `ConstructionProjectionCompletes`).

## Remaining limitation

`Begin route` intentionally cannot transition yet because the route editor is assigned to S09. The action stays visible, semantic, disabled, and paired with its prerequisite reason. The normal build-only runtime registry still presents unknown-report detail until a city market report is available; no placeholder market values are fabricated.
