# S08-P01 city overview component specification

## User flow

Open Lübeck's city overview from the HUD, scan the six authoritative header summaries, move between Population, Production, and Market with mouse, keyboard, or controller, and follow a causal link from a need or bottleneck to the supplying chain/building. Close the screen and restore focus to the control that opened it.

Administration is represented only by a disabled `Future` tab so the MVP does not imply an implemented policy system.

## Component inventory

| Component ID | Deliverable class | Implementation | Content and states |
| --- | --- | --- | --- |
| `CityOverview.ScreenReference` | Screen reference/style anchor | Reference-only raster | full 16:9 management screen over a dimmed city; Population selected; warning visible |
| `CityOverview.Root` | Native widget | Slate | closed/open, normal, loading, empty, error, long-localization |
| `CityOverview.Header` | Component reference + native widget | Reference-only raster + Slate | city identity and six summaries: population trend, treasury contribution, satisfaction, workforce, staple reserve, alerts |
| `CityOverview.TabBar` | Component reference + native widget | Reference-only raster + Slate | Population, Production, Market; disabled Administration `Future` placeholder |
| `CityOverview.List` | Native virtualized list | `SListView` | stable rows for the active tab; loading/empty/error rows without geometry jumps |
| `CityOverview.Population.Row.*` | Component reference + native row | Reference-only raster + Slate | tier/residence, residents, homes, workforce, need access/affordability/reliability/reserve, selected/focus/warning |
| `CityOverview.Production.Row.*` | Component reference + native row | Reference-only raster + Slate | chain/output, actual/nominal throughput, utilization, workforce, blocker, selected/focus/warning |
| `CityOverview.Market.Row.*` | Component reference + native row | Reference-only raster + Slate | good, stock/reserve, demand, incoming, price/trend/status, selected/focus/warning |
| `CityOverview.CausalLink.*` | Component reference + native button | Reference-only raster + Slate | reveal supplying chain/building, hover, pressed, focus, disabled with reason |
| `CityOverview.Status` | Component reference + native panel | Reference-only raster + Slate | stable loading, empty, warning, and actionable error messages |
| `CityOverview.Close` | Native button | Slate | default, hover, pressed, keyboard/controller focus |
| `CityOverview.FocusLayer` | Native geometry/text | Slate | brass focus ring/announcement independent of hover and color |

No production raster is required. Borders, status marks, focus rings, charts, data, icons, and all localized/dynamic text are native Slate or scalable glyphs. Generated PNGs are visual references only.

## State matrix

| Surface | Default | Hover/pressed | Selected | Disabled | Keyboard/controller focus | Loading | Warning | Error |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Tab | Harbor Slate + text | Brass edge/tactile inset | Baltic Navy + Brass rule + selected state | Muted Ink + `Future` explanation | 3 px Brass ring, 48 px target | stable tab strip | n/a | n/a |
| Header summary | Linen ledger card | no layout movement | n/a | n/a | link card receives focus ring where actionable | value placeholder with `Loading` text | Amber triangle + label | Oxblood octagon + remedy |
| Data row | Parchment/linen rule | subtle native fill | Brass left rule + selected state | muted plus cause | Brass outline independent of selection | stable row skeleton/text | icon + `Warning` + Amber edge | icon + `Error` + Oxblood edge |
| Causal link | secondary native button | native hover/pressed tint | preserves row selection | muted with reason tooltip | Brass focus ring | unavailable with reason | label explains affected supply | actionable cause and remedy |
| Whole screen | working panel | n/a | active tab and row | Administration stays non-interactive | deterministic focus order | stable header/tab geometry | alert count and affected rows | retry/remedy status panel |

Status is never conveyed by color alone. Every target is at least 40×40 px; controller focus targets are 48 px. Labels wrap or ellipsize with tooltips and reserve at least 35% localization expansion.

## Focus and input contract

Focus order is `Close → Population → Production → Market → first active row → its causal link → next row`. Left/Right or controller shoulders change among implemented tabs. Up/Down moves through the virtualized active list. Enter/Space or controller A activates. Escape/controller B closes and restores focus to `HUD.TopStatus.CityOverview`.

## Reference generation plan

1. Composed 16:9 city-overview screen, Population selected and one shortage warning, using the approved main HUD as style anchor.
2. Isolated six-card summary header.
3. Isolated four-tab strip including disabled Administration.
4. Isolated population row in warning/focus state.
5. Isolated production row in warning/focus state.
6. Isolated market row in warning/focus state.
7. Isolated causal-link plus loading/empty/error status family.

Each selected reference is stored at generator-native dimensions without resizing, receives a sibling prompt record, and is inspected at original resolution. The shipping UI is reconstructed with native Slate.
