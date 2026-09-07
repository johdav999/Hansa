# S09-P03 European trade map and Simple route editor

## User flow

Open European trade from the HUD or `Begin route` on a selected market good, compare the two MVP route modes across Lübeck, Hamburg, Lüneburg, and Rostock, select a route, edit its ordered stops and unconditional cargo rule, protect a minimum source reserve, review time/capacity/upkeep/profit uncertainty, then save and start the route through typed gameplay commands.

## Component inventory

| Component | Deliverable | Shipping implementation | Purpose |
| --- | --- | --- | --- |
| `TradeMap.Root` | approved composed reference + native screen | existing reference-only raster + Slate | bounded European trade shell |
| `TradeMap.Mode.Filter` | native control | Slate | all/sea/land filtering |
| `TradeMap.Route.*` | native list row | Slate | route mode, lifecycle, stops, and selection |
| `TradeMap.Canvas` | native geometry | custom Slate paint | sea solid line, land line, city rings, labels |
| `TradeMap.City.*` | native map marker and semantic status | Slate | four city positions plus current/stale/unknown report treatment |
| `TradeMap.Stop.*` | native editor row | Slate | ordered stop, load/unload, quantity, protected reserve |
| `TradeMap.Editor.*` | native controls | Slate | controller-safe alternatives to drag and command commit |
| `TradeMap.Editor.ReserveRisk` | native alert | Slate | icon, text, and semantic warning independent of color |
| bottom summary | native responsive panel | Slate | route legend and uncertainty explanation; collapses at compact height |

No new raster asset is shipped. `Docs/Images/UI/hansa-ui-trade-map.png` is the approved style anchor; dynamic labels, route lines, markers, quantities, prices, states, and warnings are all native.

## State matrix

| Component | Default | Hover/pressed | Selected/focus | Disabled | Loading | Warning/error |
| --- | --- | --- | --- | --- | --- | --- |
| Route row | mode pattern + lifecycle text | centralized button feedback | brass selected style and independent focus | excluded if unavailable | stable list shell | amber icon/text reserve warning |
| City marker | ring + label + report age | no layout movement | semantic selection remains route-driven | n/a | stable position | stale dashed/amber wording; unknown says `No recent report` |
| Stop row | city/action/quantity/reserve | centralized button feedback | selected stop and controller focus | invalid movement/action is rejected | stable editor | explicit reserve cause and remedy |
| Save/start | normal action | native pressed state | 48 px controller target | no dirty draft or rejected command | stable geometry | gateway rejection text |

## Input, accessibility, and responsive contract

- Stable semantic IDs cover the root, mode filter, four cities, routes, stops, cargo action, quantity, reserve, save, and active-state controls.
- Quantity and reserve steppers, action cycling, route selection, and ordered-stop controls are available without drag.
- Sea and land routes differ by pattern and label; stale, unknown, and reserve-risk states include glyph/text redundancy.
- The 1280×720 composition hides only the nonessential bottom legend; the full editor and metrics remain reachable. At 1920×1080 the full legend/schedule summary is restored.
- All changing/localized text is native and scrollable where expansion is likely.

## Visual-reference decision

The repository already contained the approved composed trade-map reference required by the design brief. The ImageGen workflow was applied to classify it as the style anchor; an additional generation pass would duplicate the approved direction, so S09-P03 adds no new generated component raster. Shipping output is entirely native Slate and uses centralized Hansa tokens.
