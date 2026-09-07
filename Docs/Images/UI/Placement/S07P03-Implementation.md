# S07-P03 build menu and placement UI

## Component inventory

| Component | Native implementation | Required states |
| --- | --- | --- |
| Screen shell | `SHansaRootHud` bottom overlay | closed, open, keyboard/controller focus |
| Category navigation | seven `SButton` category tabs | default, selected, hover, pressed, focus |
| Building cards | dynamic `SGridPanel` cards | default, selected, locked, focus |
| Selection details | native text panel | empty, selected, favorite, compare result |
| Placement targets | click/A target buttons | enabled, disabled, focus |
| Preview and footprint | native bordered status surface | none, outlined-valid, striped-error |
| Validation | cause/remedy alert | valid, invalid, warning-ready |
| Overlays | grid and road toggle buttons | off, on, focus |
| Actions | rotate, repeat, confirm, cancel | enabled, disabled, selected, focus |
| Status and feedback | native icon, text, outline/pattern and color | success, invalid, focused action |

Loading remains an application-shell concern and is unchanged. Error and warning presentation uses a glyph, label and shape/pattern in addition to color. Locked cards remain visible and expose their reason but cannot be activated.

## Reference set and inspection

The existing selected placement-family references remain the style anchor; S07-P03 did not require a new raster variant. Each was inspected at its original resolution before implementation:

- `placement--build-mode-screen--invalid-road--1672x941--v1.png` — 1672×941 composed hierarchy reference.
- `placement--build-card--selected--1672x941--v1.png` — 1672×941 selected-card reference.
- `placement--action-controls--focus--1915x821--v1.png` — 1915×821 action/focus reference.
- `placement--validation-card--error--1881x836--v1.png` — 1881×836 cause/remedy reference.

All four were created previously with built-in ImageGen in new-generation mode, selected without resampling, and have sibling `.prompt.md` records containing their final prompts. Original-resolution inspection passed hierarchy, Baltic Navy/Linen/Parchment/Brass palette consistency, redundant invalid and focus cues, safe margins and intended component state. Reference text is not shipped.

## Shipping implementation

The shipping UI is reconstructed entirely from native Slate widgets and style tokens. Dynamic names, prices, workforce, upkeep, footprints, input/output summaries, shortcuts, state and validation text are native text. No generated raster is imported into `Content/`, and no reference image is used as an interactive screen.

`UHansaBuildMenuPresentationModel` owns the event-revisioned presentation state and translates player intents into the existing placement session and authoritative `FHansaGameplayCommandGateway`. Successful construction synchronizes the simulation projection back into the Lübeck world. The widget supplies direct click/A targets, keyboard shortcuts, an explicit controller focus order, and semantic activation/focus alternatives, so dragging is never required.

Every visible category, current card, target, overlay, card action, placement action, preview, footprint, cause, remedy and validation result has a stable semantic node. Card values expose stable ID, cost, workforce/upkeep, footprint, input/output and lock reason; validation exposes typed state plus cause and remedy.

## Evidence

`empty_lubeck_build_v1` now exercises the build-menu shell, Road and Warehouse cards, direct target actions, invalid road-connectivity feedback, cause/remedy text and authoritative confirms. Native-size evidence is emitted without post-capture resizing at:

- `Saved/TestEvidence/Automation/S07P03/contract-empty-lubeck-flow-720` — 1280×720.
- `Saved/TestEvidence/Automation/S07P03/contract-empty-lubeck-flow-1080` — 1920×1080.

The automated commandlet contract records exact dimensions, semantics, simulation tick, UI revision and structural assertions. The live named-pipe flow maps the same S07-P03 semantic IDs onto the real native Slate screen host for visual capture.

## Classification and limitations

- The four PNGs above are visual references only.
- `SHansaBuildMenu`, its presentation model, semantic tree and command-gateway integration are production code.
- The commandlet PNGs are test evidence, not production assets.
- The current direct-target buttons provide deterministic non-drag placement for the MVP test scenario. The shoreline action searches the authoritative Lübeck terrain and occupancy state for a footprint-valid target, so Dock and Fishery exercise the same validation rules as normal construction. Free map picking can replace these target adapters later without changing the build-card, validation, command or semantic contracts.
