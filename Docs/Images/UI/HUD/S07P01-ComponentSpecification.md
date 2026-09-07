# S07-P01 — HUD component specification and style system

## Status and visual anchor

- Milestone: S07-P01
- Approved style anchor: [`../hansa-ui-main-city-hud.png`](../hansa-ui-main-city-hud.png)
- Anchor status: composed visual-direction reference only; it is not a shipping screen or a source for cropped production controls.
- Generation decision: no new ImageGen output. The approved anchor resolves the shared panel, button, tab, tooltip, focus, and severity language needed by this increment. All deliverables below are native or scalable.
- Runtime implementation: `Source/Hansa/Public/UI/HansaUiStyle.h`, `Source/Hansa/Private/UI/HansaUiStyle.cpp`
- Stable semantic contract: `Source/Hansa/Public/UI/HansaHudSemanticIds.h`, `Source/Hansa/Private/UI/HansaHudSemanticIds.cpp`

The style remains a restrained merchant counting-house treatment: navy/slate floating surfaces, linen working surfaces, oak/brass rules, and modern native controls. Dynamic text, values, focus, status, charts, and interaction states are never baked into raster images.

## Component inventory

| Family | Component | Implementation | Semantic ID or instance prefix | Notes |
| --- | --- | --- | --- | --- |
| Shell | Root HUD shell | UMG responsive layout | `HUD.Root` | Safe-area owner; center city view remains unobstructed. |
| Shell | Top status bar | UMG | `HUD.TopStatus` | Floating dark panel; event-updated children. |
| Status | House crest | vector/SDF plus native label | `HUD.TopStatus.HouseCrest` | Player color also pairs with emblem/name. |
| Status | Money and trend | UMG native text/icon | `HUD.TopStatus.Money`, `HUD.TopStatus.MoneyTrend` | Tabular data; trend uses arrow and signed text as well as color. |
| Status | Population and workforce | UMG native text/icon | `HUD.TopStatus.Population`, `HUD.TopStatus.Workforce` | Summary values only. |
| Navigation | Selected-city breadcrumb | UMG | `HUD.TopStatus.CityBreadcrumb` | Localized labels and stable city identity remain separate. |
| Status | Season/date | UMG native text/icon | `HUD.TopStatus.DateSeason` | Seasonal glyph is vector/SDF. |
| Controls | Time/speed group | UMG buttons | `HUD.TopStatus.Speed` | Parent for pause and discrete speed controls. |
| Controls | Pause/normal/fast/fastest | UMG buttons plus vector/SDF glyphs | `HUD.TopStatus.Speed.Pause`, `.Normal`, `.Fast`, `.Fastest` | Selected state is persistent and distinct from hover. |
| Status | Research summary | UMG | `HUD.TopStatus.Research` | Native progress geometry and text. |
| Status | Connection state | UMG native text/icon | `HUD.TopStatus.Connection` | Never color-only. |
| Controls | Main menu | UMG icon button | `HUD.TopStatus.Menu` | Minimum 44 px pointer and 48 px focus target. |
| Alerts | Objectives and alert stack | UMG virtualized/collapsible list | `HUD.AlertStack` | Maximum three expanded alerts. |
| Controls | Alert stack toggle | UMG button | `HUD.AlertStack.Toggle` | Announces expanded state. |
| Alerts | Alert group | UMG list row | `HUD.AlertStack.Group.<CategoryStableId>` | `<CategoryStableId>` is stable data identity, not localized text. |
| Alerts | Alert row | UMG list row plus vector/SDF severity shape | `HUD.AlertStack.Alert.<AlertStableId>` | Shows severity label/shape, subject, cause, and age. |
| Shell | Bottom build/selection area | UMG responsive container | `HUD.BottomArea` | Hosts build menu and contextual actions. |
| Shell | Inspector host | UMG drawer container | `HUD.InspectorHost` | 360–440 px target width at 1080p. |
| Feedback | Notification layer and row | UMG pooled list | `HUD.NotificationLayer`, `HUD.NotificationLayer.Notification.<NotificationStableId>` | Ambient/notice events aggregate. |
| Feedback | Tooltip layer | Slate/UMG native tooltip | `HUD.TooltipLayer` | Native text, bounded width, cause/remedy expansion. |
| Feedback | Controller focus layer | Slate/UMG native rounded outline | `HUD.FocusLayer` | Brass 3 px; Chalk 4 px in high-contrast mode. |
| Build menu | Root and category list | UMG | `BuildMenu.Root`, `BuildMenu.Categories` | Native responsive layout. |
| Build menu | Category tab | UMG toggle style plus vector/SDF icon | `BuildMenu.Category.<CategoryStableId>` | Uses the shared tab style. |
| Build menu | Card list | UMG virtualized/tile list | `BuildMenu.Cards` | No generated full-screen content. |
| Build menu | Building card | UMG native card plus production illustration/icon | `BuildMenu.Card.<BuildingStableId>` | Raster building art may be added later only at defined native sizes. |
| Build menu | Recent/favorites | UMG lists | `BuildMenu.Recent`, `BuildMenu.Favorites` | Reuses card semantics and styles. |
| Placement | Placement surface | native world/UMG overlay | `Placement.Root` | Does not own simulation rules. |
| Placement | Preview and footprint | material/native geometry | `Placement.Preview`, `Placement.Footprint` | Teal valid, amber warning, oxblood striped invalid; label/shape accompanies color. |
| Placement | Validation/cause/remedy | UMG tooltip/card | `Placement.Validation`, `.Cause`, `.Remedy` | Exact cause and remedy are event-updated text. |
| Placement | Overlay toggle | UMG toggle plus native/material overlay | `Placement.Overlay.<OverlayStableId>` | Road, shore, service, fertility, workforce, logistics. |
| Placement | Rotate/repeat/confirm/cancel | UMG buttons | `Placement.Action.Rotate`, `.Repeat`, `.Confirm`, `.Cancel` | Non-drag actions with keyboard/controller support. |
| Inspector | Root drawer | UMG | `Inspector.Root` | Working linen surface in stable section order. |
| Inspector | Identity and state | UMG native content | `Inspector.Identity` | Stable object ID is semantic value, never display copy. |
| Inspector | Primary result | UMG native data | `Inspector.Result` | Most important outcome first. |
| Inspector | Inputs/outputs/needs | UMG or Slate virtualized rows | `Inspector.Flows` | Data and causal factors come from presentation models. |
| Inspector | Problem and cause | UMG native alert/factor stack | `Inspector.Problem`, `Inspector.Problem.Cause` | Status has label/shape/icon redundancy. |
| Inspector | Action list and action | UMG buttons | `Inspector.Actions`, `Inspector.Action.<ActionStableId>` | Stable intent identity; localized copy is metadata. |
| Inspector | History | Slate virtualized list when needed | `Inspector.History` | Bounded and event-driven. |
| Inspector | Close | UMG icon button | `Inspector.Close` | Escape/right-click/back parity and focus restoration. |
| Shared | Working/floating/decision/critical panels | native Slate rounded-box brush | n/a; used by semantic owners above | 2–4 px corners; no raster scaling. |
| Shared | Primary/secondary/destructive/icon buttons | native Slate `FButtonStyle` | n/a; semantic ID belongs to the control | Default, hover, pressed, disabled brushes. |
| Shared | Tabs | native Slate toggle `FCheckBoxStyle` | n/a; semantic ID belongs to the tab | Selected state has a persistent brass outline. |
| Shared | Tooltip | native panel brush plus `FTextBlockStyle` | `HUD.TooltipLayer` | Short hover may expand into cause/formula content. |
| Shared | Status and system icons | vector/SDF | IDs belong to their containing semantic node | Strong 20–24 px silhouette; no color-only distinction. |
| Decorative | Subtle surface variation | optional UI material | n/a | Reduced-texture mode disables it; never behind small text without flat backing. |

The checked-in C++ catalog contains the static IDs and the instance prefixes from this table. Runtime instance suffixes must use canonical stable identifiers normalized to semantic-safe segments (for example `Building_Bakery`), not package paths, translated labels, indexes, or provider IDs.

## State matrix

| State | Panels | Buttons/tabs | Lists/cards | Alerts/status | Accessibility and semantic state |
| --- | --- | --- | --- | --- | --- |
| Default | Stable native surface and padding | Readable resting brush | Stable geometry | Shape/icon plus label | `visible=true`, normal role/value. |
| Hover | No layout movement | Brass outline within 80 ms | Row highlight without reflow | Expanded hint may appear | Hover never substitutes for focus. |
| Pressed | No scaling/distortion | Native pressed brush and 1 px content offset | Immediate tactile response | Acknowledgement only after intent succeeds | Semantic action remains the normal command path. |
| Selected | Brass outline/light treatment | Persistent selected tab/control | Selected row remains distinct from hover | Tracker/pin state is explicit | `selected=true`. |
| Disabled | Structure remains visible where useful | Muted treatment plus explanation tooltip | Reason stays discoverable | Never masquerades as missing data | `enabled=false`; cause/remedy exposed. |
| Keyboard/controller focus | Shared outer focus layer | 3 px Brass ring; 4 px Chalk high contrast | Full row or control target outlined | Focus does not change severity | `focused=true`; minimum 48×48 target. |
| Loading | Stable dimensions | Input blocked without moving copy | Skeleton/progress occupies final geometry | Loading label/icon | `loading=true`; no per-frame raw binding. |
| Warning | Amber native edge | Action remains readable | Warning row includes triangle/clock and text | Warning triangle plus localized label | `warning=true`; color is redundant. |
| Error/critical | Oxblood 3 px edge on dark or flat-backed surface | Failed action returns precise reason | Critical row includes octagon/exclamation and text | Critical octagon plus localized label | `error=true`; cause and remedy included. |

## Central token contract

### Color

The code exposes the fifteen exact palette tokens from `UIDesignBrief.md`: Baltic Navy `#152A35`, Harbor Slate `#29424D`, Ink `#202628`, Muted Ink `#596160`, Linen `#F2E9D8`, Parchment `#DFCFAF`, Oak `#795137`, Brass `#C19A52`, Hanseatic Brick `#A44C3F`, Oxblood `#762F32`, Prosperity Teal `#35766F`, Baltic Blue `#397FA3`, Warning Amber `#D09132`, Frost Blue `#9CC3CF`, and Chalk `#FAF7EF`. Opacity variants are permitted for native overlays; they are not new palette hues.

### Typography

| Token | Size at 1080p | Current native face | Intended role |
| --- | ---: | --- | --- |
| Display | 36 | engine default bold placeholder | Rare major title/victory moment |
| Heading 1 | 26 | engine default bold placeholder | Panel/screen title |
| Heading 2 | 20 | engine default bold placeholder | Section title |
| Body | 16 | engine default regular placeholder | Labels/descriptions/tooltips |
| Data | 15 | engine default regular placeholder | Dense numeric/economic data |
| Caption | 13 | engine default regular placeholder | Non-essential metadata |

The font-family decision remains deliberately reversible until Source Serif 4 and an accessible sans-serif have completed license, language coverage, and tabular-numeral evaluation. Widgets consume roles now; changing the registered native font later will not require screen-local edits.

### Spacing, focus, and targets

- Micro 4, base unit 8, compact 12, panel 16, spacious 24, gutter 32 Slate units.
- Minimum pointer target 40, preferred pointer target 44, controller focus target 48.
- Normal focus: Brass, 3 px, 2 px outer separation.
- High-contrast focus: Chalk, 4 px, 2 px outer separation.

### Severity

| Severity | Color | Required non-color shape | Persistence | Label key |
| --- | --- | --- | --- | --- |
| Ambient | Muted Ink | dot | timeline/non-persistent | `UI.Severity.Ambient` |
| Notice | Baltic Blue | information circle | timed/non-persistent | `UI.Severity.Notice` |
| Warning | Warning Amber | warning triangle | persistent | `UI.Severity.Warning` |
| Critical | Oxblood | critical octagon | persistent | `UI.Severity.Critical` |
| Decision | Brass | decision seal | persistent/blocking as policy requires | `UI.Severity.Decision` |

### Motion

| Token | Duration | Curve | Reduced motion |
| --- | ---: | --- | --- |
| Hover feedback | 80 ms | ease out | immediate |
| Panel transition | 150 ms | ease out | immediate |
| Numeric pulse | 180 ms | ease in/out | immediate |
| Major scale transition | 300 ms | ease in/out | immediate |

Continuous animation is not a token. It is allowed only when state or direction is changing.

## Reusable native styles

The registered `HansaUi` Slate style set supplies:

- panels: world overlay, floating, working, decision, critical, tooltip;
- buttons: primary, secondary, destructive, icon;
- a toggle-button tab style;
- display, heading, body, data, and caption text styles for light and dark surfaces;
- tooltip body text.

`UHansaUiStyleLibrary` exposes token values and style copies to Blueprint/UMG. `FHansaUiStyle` exposes the registered style set to C++/Slate. Focus is an outer native overlay so it remains independent of pointer hover and can surround any interactive widget.

## Contrast and validation

Automated tests verify the exact palette, type/spacing/motion bounds, semantic catalog uniqueness and parents, native brush/style registration, severity shape redundancy, and the following WCAG AA body pairs:

- Ink on Linen;
- Ink on Parchment;
- Muted Ink on Linen;
- Chalk on Baltic Navy;
- Chalk on Harbor Slate;
- Chalk on Hanseatic Brick;
- Chalk on Oxblood;
- Ink on Warning Amber.

Baltic Blue with Chalk is reserved for large text or essential icons because it meets the 3:1 target but not the 4.5:1 body target. Body copy therefore stays on the approved dark/light surface pairs above.

## Asset and implementation classification

- Visual reference: `hansa-ui-main-city-hud.png`; composed reference only.
- New visual references: none.
- New production raster assets: none.
- New generated masters or prompt records: none, because no visual component was unresolved.
- Production-ready in this increment: native token API, Slate styles, semantic ID catalog, and their automated contracts.
- Deferred to S07-P02–P04: assembled root HUD, event-updated presentation models, build cards/placement behavior, inspector/alert behavior, controller traversal, and native screenshots at 1280×720 and 1920×1080.
