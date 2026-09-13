# Selected market demand inspector

## Component inventory and state contract

| Component | Implementation | States and behavior |
| --- | --- | --- |
| Shell, navy identity header, linen panel | Existing native inspector, compact 360 x 520 Slate-unit host | selected, closed; bounded height with scrolling |
| Navigation and close | Existing native controls | default, hover, pressed, disabled and keyboard/controller focus |
| Citizen-demand list | Native rows grouped by authored good ID | measured full, partial, zero; pending; no demand; city data unavailable |
| Product glyph and label | Existing native Hansa glyphs and text | Bread, Fish, Beer, Tools, labeled fallback for future goods |
| Rounded supply bar | Native rounded-box brushes, 8-unit height | 0–100% measured ratio; no animation; blank track and explicit dash for unknown/no demand |
| Quantities and percentage | Native localized text | supplied / required in units, 3 decimal precision; percent up to 1 decimal |
| Details, causes, history and actions | Existing inspector native controls | collapsed default, expanded, disabled reasons, focus |
| Charts/overlays | Only the fulfillment bar; no added world overlay | not applicable |
| Decorative imagery | None added to production | generated images are references only |

New demand rows are read-only and focusable. Hover exposes the same supply
explanation available through visible labels and Details. Pressed/selected/
disabled states do not apply to these information rows. Keyboard/controller focus
uses a 2-unit ink outline. Warnings include numeric shortages, never color alone.
Existing controls retain shared Hansa state styles.

## Data contract

BuildMarketDemandFlows presents authoritative rolling city/good consumption
history. Required and consumed quantities are summed over 30 game days before
division. Services and prospective demand are excluded. Earlier residents'
consumption stays in the window after removal or tier changes. Pending history,
partial coverage, no demand and measured shortage have explicit labels. Rows
remain alive through value updates to preserve focus.

See [RollingCitizenFulfillment.md](RollingCitizenFulfillment.md) for the runtime
history, save-format 4 migration, fingerprint 17, editor impact and automation
contract. This revision reuses existing approved artwork and native components.

Semantic IDs reuse Inspector.Flows.Item.Good_<Name> and expose known state,
requiredMilliUnits, suppliedMilliUnits and formatted percentage. Normal world
selection, projection refresh, Details, close and controller navigation are used.

## Visual references and prompts

- [Panel, native 1024 x 1536](../Images/UI/MarketInspector/market--panel--partial--1024x1536--v1.png)
- [Panel prompt](../Images/UI/MarketInspector/market--panel--partial--1024x1536--v1.prompt.md)
- [Demand row, native 1536 x 1024](../Images/UI/MarketInspector/market--demand-row--partial--1536x1024--v1.png)
- [Row prompt](../Images/UI/MarketInspector/market--demand-row--partial--1536x1024--v1.prompt.md)

Built-in ImageGen generate mode; panel serves as row style anchor. Both outputs
were inspected at native resolution. No production textures are imported or
referenced; interactive UI is reconstructed natively with existing glyphs.
No raster has been resized.

## Original visual implementation validation (2026-09-10)

- HansaEditor Win64 Development build: passed.
- Hansa.UI.MarketInspector.DemandAndSelection: passed (1 test).
  Validates weighted quantities, city isolation, pending cohorts, zero demand,
  zero supply, empty homes, actual market construction/selection, stable rows,
  and Details toggling.
- Hansa.UI.Inspector: passed (6 existing regression tests).
- Real game captures: passed at 1280 x 720, 1920 x 1080,
  2560 x 1440 and 3440 x 1440. Demand, Details and accessibility
  states are recorded in Saved/MarketInspector as PNG and semantic TSV pairs.
- Final 720p capture additionally asserts the expanded Frame action is revealed
  inside the inspector after keyboard/controller focus.
- Inspected representative demand, Details, large-text/high-contrast and focus
  captures. The focused-row white-overlay defect found during QA was corrected
  by passing the rounded brush's transparent tint into the native draw call.
- Contrast: Ink/Linen 12.72:1; Ink/Parchment 9.99:1;
  Teal/Parchment 3.44:1; Chalk/Navy 13.87:1.

Evidence logs:
- Saved/BuildArtifacts/20260910-181126563-build-HansaEditor-Win64-Development/
- Saved/BuildArtifacts/20260910-180840147-automation-Hansa.UI.MarketInspector.DemandAndSelection/
- Saved/BuildArtifacts/20260910-180633843-automation-Hansa.UI.Inspector/
- Saved/BuildArtifacts/20260910-181209643-market-inspector-1280-720/
- Saved/BuildArtifacts/20260910-180621012-market-inspector-1920-1080/
- Saved/BuildArtifacts/20260910-180827151-market-inspector-2560-1440/
- Saved/BuildArtifacts/20260910-180917027-market-inspector-3440-1440/

## Scope and remaining limits

This is a native UI implementation with reference art, not a new production
raster import or a full MVP release audit. No Shipping cook/package was run.
Large text, high contrast and reduced motion were exercised together; an
exhaustive sweep of every localization and UI-scale setting was not performed.
Real starting-city captures show measured shortages; partial fulfillment is
covered by the quantity test and visual references. The background market-only
city model has no per-residence consumption report to display in this view.
Already-running editor/game processes must reload the rebuilt module (normally
by restarting) to use the new C++ UI.

