# S08-P02 Market table component specification

## User flow

Open the City Overview, choose Market, scan all ten MVP goods, narrow the table with search and filter controls, sort any economic column, and select a good without losing that selection when the authoritative report refreshes. Current, stale, estimated, unknown, shortage, and opportunity states remain distinguishable without relying on color.

## Component inventory

| Component ID | Deliverable | Shipping implementation | Purpose |
| --- | --- | --- | --- |
| `Market.Root` | Composed reference + native widget | Reference-only raster + Slate | bounded market table surface inside City Overview |
| `Market.Toolbar` | Component reference + native controls | Reference-only raster + Slate | search, category, trend, and quick-filter controls plus visible count |
| `Market.Header` | Component reference + native controls | Reference-only raster + Slate | sticky sortable Good, Stock, Reserve, Demand, Price, Trend, Incoming, Status headers |
| `Market.List` | Native virtualized list | Slate | filtered/sorted immutable row source |
| `Market.Good.*` | Current-row component reference + native row | Reference-only raster + Slate | icon/glyph plus label and the eight economic cells |
| `Market.Good.*.Stale` | Stale/estimated component reference + native row state | Reference-only raster + Slate | dashed/desaturated report treatment with age and explicit text |
| `Market.State.FilterEmpty` | Component reference + native state panel | Reference-only raster + Slate | no-match explanation and clear-filters action |
| `Market.Focus` | Native outline | Slate/style tokens | keyboard/controller focus independent of selection and hover |

All dynamic labels, quantities, prices, trends, report ages, sort glyphs, search text, and status text are native. Generated images are visual references only.

## State matrix

| Component | Default | Hover | Pressed | Selected | Disabled | Focus | Loading | Warning/error |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Search | empty or query text | native border tint | inset feedback | caret/query retained | muted with reason | brass ring | stable width | invalid query never required |
| Filter | active value label | native tint | inset feedback | brass marker + text | muted | brass ring | stable width | no-match count + remedy |
| Sort header | neutral arrows | native tint | inset feedback | direction arrow + label | n/a | brass ring | stable geometry | n/a |
| Market row | linen ledger row | subtle slate tint | immediate inset | brass outline + fill | n/a | high-contrast double ring | placeholder values | amber/oxblood plus glyph and text |
| Stale row | dashed edge, reduced saturation, age label | unchanged geometry | unchanged geometry | selection remains dominant | n/a | brass ring | n/a | hourglass glyph + `Stale · estimated` |
| Filter-empty state | explanation | n/a | n/a | n/a | n/a | clear action focused | stable table shell | cause and clear-filter remedy |

## Data and interaction contract

- Canonical rows are the ten MVP goods: grain, flour, bread, fish, salt, timber, planks, iron, tools, and beer.
- The presentation model owns numeric formatting, price-difference calculation, trend classification, sparkline text, filtering, sorting, and stable tie-breaking.
- The widget never reads mutable simulation state and never recalculates market formulas.
- Selection is keyed by the canonical `Good.*` ID and survives report refreshes, search/filter changes, and re-sorting.
- Search is case-insensitive across localized display label and stable ID.
- Filters cover category, price trend, shortage, owned stock, incoming route/supply, and opportunity.
- Sort direction toggles on repeated header activation. Equal values fall back to canonical good ID.
- Mouse clicks, Enter/Space/controller A, directional focus, and non-drag actions reach every control and visible row.

## Semantic contract

- Controls: `Market.Search`, `Market.Filter.Category`, `Market.Filter.Trend`, `Market.Filter.Quick`, `Market.Filter.Clear`.
- Sort headers: `Market.Header.Good|Stock|Reserve|Demand|Price|Trend|Incoming|Status`.
- Rows: `Market.Row.<GoodStableIdSuffix>`.
- Cells: `Market.Row.<GoodStableIdSuffix>.<Column>`.
- Each row exposes a screen-reader-equivalent value containing good label, stock, reserve, total demand, local price, trend change, incoming supply, status, report age, and current/stale/estimated/unknown confidence.

## Generation plan

Built-in ImageGen, `ui-mockup`, reference-only:

1. Composed City Overview Market tab with the ten-good table.
2. Isolated search/filter toolbar.
3. Isolated sticky sortable header.
4. Isolated current selected grain row.
5. Isolated stale/estimated salt row.
6. Isolated no-filter-results state.

Style anchors: `Docs/Images/UI/hansa-ui-city-market-v2.png` for dense market hierarchy and `Docs/Images/UI/CityOverview/city-overview--composed--population-warning--1672x941--v1.png` for the containing City Overview shell. Palette and materials remain the exact Hansa navy/slate/linen/parchment/oak/brass/brick/oxblood/teal/blue/amber system.
