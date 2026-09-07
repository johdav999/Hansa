# S10-P03 Scenario and Victory UI Specification

## Scope and visual direction

The scenario surface is an important-decision dossier layered over the live Lübeck view. It follows the shared Baltic counting-house language: Baltic Navy scrim, Linen and Parchment working surfaces, Oak framing, Brass focus/progress, Hanseatic Brick identity accents, Prosperity Teal success and Oxblood failure. Dynamic text, thresholds, values and progress are rendered natively.

The existing main HUD reference at `Docs/Images/UI/hansa-ui-main-city-hud.png` is the family style anchor. The new composed scenario reference confirms hierarchy only and is not shipped.

## Component inventory

| Component | Class | Native size/reference | Implementation | Purpose |
| --- | --- | --- | --- | --- |
| Scenario screen shell | Native widget + reference-only raster | 1536 × 1024 reference | Slate | Bounded centered dossier, dimmed world backdrop, close/focus layers |
| Scenario header | Native widget | Responsive | Slate | Scenario identity, state label, elapsed/sustain timing |
| Victory-path selector | Native widget + reference-only raster | Responsive / 1536 × 1024 reference | Slate | Prosperity, trade-network and research/civic path cards |
| Objective progress row | Native widget + reference-only raster | Responsive / 1536 × 1024 reference | Slate | Label, current/target value, progress bar, met/sustained/failed state |
| Outcome banner | Native widget + reference-only raster | Responsive / 1536 × 1024 reference | Slate | Explicit success, failure or victory result and explanation |
| Progress and threshold bars | Native shape | Responsive | Slate | Deterministic ratio and sustained-tick progress |
| Status and focus marks | Native shape/text | Responsive | Slate | Icon/shape + text redundancy and Brass controller focus ring |
| Open/close controls | Native control | Minimum 48 × 48 focus target | Slate | HUD entry and predictable focus restoration |

No production raster asset is required. Ornament, borders, bars, icons and status marks use shared code-native Slate tokens and geometry.

## State matrix

| Component | Default | Hover/pressed | Selected/focus | Loading | Warning/error | Success/victory | Disabled |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Screen shell | Linen dossier | N/A | Brass outer focus cue | Stable skeleton rows | Oxblood edge + reason/remedy | Teal/brass formal frame | N/A |
| Path card | Parchment, path label | Native tint, no layout shift | Brass 3 px ring, selected text | Stable dimensions | Oxblood icon + failed reason | Teal check/seal + completed text | Muted Ink + explanation |
| Objective row | Ink label and data | Native row highlight | Brass outline independent of color | Reserved bar geometry | Amber/Oxblood icon + label | Teal check + `Met`/`Sustained` | Muted Ink + unavailable reason |
| Outcome banner | Hidden while active | N/A | Focus on primary action | Stable reserved region | Oxblood octagon + `Scenario failed` | Teal check/seal + explicit victory path | N/A |
| Buttons | Shared secondary/primary style | Shared native hover/pressed | 48 px target + Brass ring | Disabled with loading label | Error remains contextual | Primary continue/close action | Reduced emphasis plus tooltip |

## Interaction and accessibility

- Stable semantic namespace: `Scenario.*`.
- The screen opens from the HUD objective area and closes with Escape, controller Back, or `Scenario.Close`.
- Controller focus order is close, path cards, objective rows, and the outcome action.
- Every status pairs color with a shape/icon and explicit text.
- Progress exposes raw current value, target value, unit, ratio, and required sustained ticks.
- Layout supports localization expansion and UI scale 80–140% without raster scaling.
- Reference resolutions are 1280 × 720 and 1920 × 1080; the dossier is width-bounded and keeps the central world context visible around it.

## Reference deliverables

- `scenario--composed--active-progress--1536x1024--v1.png`
- `scenario--objective-row--warning-focus--1536x1024--v1.png`
- `scenario--victory-path-card--selected--1536x1024--v1.png`
- `scenario--outcome-banner--victory--1536x1024--v1.png`

All four images are reference-only, generated at native 1536 × 1024, inspected at original resolution, and never imported into `Content/`.
