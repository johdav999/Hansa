# S08-P03 selected-good causal panel component specification

## User flow

Open City Overview → Market, select one of the ten stable `Good.*` rows, understand the local price and its authoritative causes, inspect bounded history and affected consumers/producers, then pin the good or begin route planning. Selection and pin state survive authoritative refreshes. An unavailable action stays visible and explains the exact prerequisite.

## Screen composition

At supported desktop resolutions the Market surface uses a bounded 60/40 split: the S08-P02 virtualized table remains on the left and the selected-good causal panel occupies the right. At the narrow 1280 × 720 target, labels wrap inside fixed sections rather than shrinking text or resampling imagery.

## Component inventory

| Component ID | Deliverable | Shipping implementation | Purpose |
| --- | --- | --- | --- |
| `Market.Detail` | composed/component reference + native panel | reference-only raster + Slate | selected-good identity and complete causal summary |
| `Market.Detail.Metrics` | native text/cards | Slate | base/local/average difference, stock/reserve, reserve days, citizen/industrial demand, incoming, confidence |
| `Market.Detail.Chart` | component reference + native chart | reference-only raster + custom Slate | bounded price-history line, current/average labels, stale style |
| `Market.Detail.Factors` | component reference + native list | reference-only raster + Slate | authoritative ordered price-factor contributions |
| `Market.Detail.Consumers` | component reference + native rows | reference-only raster + Slate | citizen/industry demand, fulfillment, affordability/blocker |
| `Market.Detail.Producers` | same relationship-list family | reference-only raster + Slate | recipe/background supply, actual/nominal output, blocker |
| `Market.Detail.Action.Pin` | component reference + native button | reference-only raster + Slate | pin/unpin selected price and stock watch |
| `Market.Detail.Action.BeginRoute` | component reference + native button | reference-only raster + Slate | begin route workflow or explain why unavailable |
| `Market.Detail.Empty` | native state | Slate | select-a-good instruction or explicit no-report state |

All dynamic text, quantities, prices, causal prose, chart points, report ages, action labels, and disabled reasons are native. Generated images are visual references only.

## State matrix

| Component | Default | Hover | Pressed | Selected | Disabled | Focus | Loading | Warning/error |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Detail panel | selected good | unchanged geometry | n/a | follows stable row ID | select-a-good message | first action reachable | stable section skeleton | stale/unknown confidence text |
| Chart | solid brass price line | point tooltip | n/a | selected good history | no-history message | semantic point focus | stable bounds | stale dashed/desaturated line |
| Factor row | signed contribution | native tint | n/a | n/a | n/a | semantic focus when actionable later | stable row | arrow + sign + label, never color only |
| Consumer/producer row | identity + metrics | native tint | n/a | n/a | n/a | semantic reading order | stable row | blocker/under-supply glyph + text |
| Pin action | “Pin price & stock” | native tint | inset | “Pinned” | no report, reason tooltip | brass ring | stable width | result text |
| Begin route | “Begin route” | native tint | inset | n/a | visible with route-editor prerequisite | brass ring | stable width | result/reason text |

## Authoritative data contract

- The aggregate immutable simulation projection carries existing read-only `FHansaMarketExplanationProjection`, reserve-days, consumer, and producer projections for every city/good market.
- The UI presentation model formats those projections but never derives causal factors from widget state.
- Base value comes from the immutable economic registry. Current/recent-average prices, stock/reserve, demand, incoming, report confidence, and history come from the selected city-market projection.
- The prose explanation names the selected good, price difference, reserve coverage, unmet demand/incoming supply, and the strongest authoritative factor or alert cause.
- History is deterministic, retains source ticks/raw prices, and is bounded to the latest 64 points.
- Consumers and producers retain stable cohort/production/building/recipe IDs in semantic values.

## Semantic contract

- Panel/summary: `Market.Detail`, `Market.Detail.Summary`, `Market.Detail.Metric.*`.
- Chart: `Market.Detail.Chart`, `Market.Detail.Chart.Point.<Tick>`.
- Causal factors: `Market.Detail.Factors`, `Market.Detail.Factor.<Factor>`.
- Relationships: `Market.Detail.Consumers`, `Market.Detail.Consumer.<StableSuffix>`, `Market.Detail.Producers`, `Market.Detail.Producer.<StableSuffix>`.
- Actions: `Market.Detail.Action.Pin`, `Market.Detail.Action.BeginRoute`, `Market.Detail.Action.Result`.
- Every chart point exposes tick and raw milli-mark price. The chart root exposes count, minimum, maximum, first, last, current, average, and confidence as a screen-reader-equivalent summary.

## Custom chart decision

The project’s runtime UI is native Slate and has no UMG composition layer. A small custom `SLeafWidget` is appropriate here because it draws one bounded polyline without per-point child widgets or tick work. Accessibility is preserved through explicit chart-summary and point semantics generated by the parent panel; tests validate deterministic source order and values.

## Image generation plan

Built-in ImageGen, `ui-mockup`, reference-only, one call per distinct visual:

1. Composed Market 60/40 table + selected Grain causal panel.
2. Isolated selected-good metrics/header panel.
3. Isolated bounded price-history chart.
4. Isolated authoritative factor stack.
5. Isolated consumers/producers relationship section.
6. Isolated action bar with pinned and unavailable-route states.

Style anchor: `Docs/Images/UI/MarketTable/market-table--composed--grain-selected--1672x941--v1.png`. Palette, typography direction, restrained linen/slate/brass materials, 8 px rhythm, focus language, and non-color status redundancy remain unchanged.
