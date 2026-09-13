# Bakery inspector visual reference
Status: visual reference approved for implementation on 2026-09-09. Native implementation and evidence: ../../../Development/ProductionInspectorImplementation.md.
Generation mode: built-in ImageGen, new generation.
Composed reference: bakery--panel--baking--1024x1536--v1.png, native 1024 x 1536.
Component references: requested 1024 x 1024, returned native 1254 x 1254 (accepted without resampling), one independent generation per component.
Style anchor: composed panel v1, approved by the user for implementation on 2026-09-09.
File-based reference attachment was unavailable due to the Windows filesystem sandbox helper; component prompts repeat the anchor palette and material specification.

## Component inventory and implementation
- Shell/header/close navigation: native Slate geometry and text, building and close glyphs.
- Batch flow: native layout, arrows, circular progress, recipe text; flour and bread icons recreated as vector/SDF.
- Ledger row: reusable native label/value/divider for stock and production record.
- Workforce row: native people glyph, label, allocated/required counts and bar.
- Status card: native icon, heading, cause/remedy text and semantic border.
- Primary button: native pause/resume action.
- Secondary button: native related-view and pin/frame actions.
- Flour icon and bread icon: separate vector recreation references.
- Charts/overlays: batch progress ring and workforce bars; no historical chart.
- Decorative imagery: none beyond the goods illustrations.

## States specified before component generation
Shell: open/closed; navigation hover, pressed, keyboard/controller focus.
Batch flow: progressing, paused, loading, missing input, storage blocked, error.
Ledger rows: default, loading, unavailable, shortage warning.
Workforce: full, insufficient, unavailable. Numeric counts accompany color.
Status card: healthy check, warning triangle with cause/remedy, error icon with cause/remedy.
Buttons: default, hover, pressed, disabled with reason, keyboard/controller focus; selected for pin toggle.
Use centralized palette and native overlays for states; no raster scaling or separately painted hover states required.
Focus uses a clear brass outline; status never relies on color alone.

## Data and limits
All example values are illustrative, not a live bakery snapshot.
60% means 6 of 10 batch ticks, not productivity.
8 available flour excludes 2 reserved for this batch.
12 bread in output storage and 4 completed batches are plausible illustrative values.
Produced-so-far can be derived from completed batches with an unchanged recipe; recipe changes require accounting/history handling.
Use actual recipe duration and workforce requirements at runtime.
Tick labels are retained as verified simulation units; a player-facing time conversion requires the authoritative clock mapping.
No bakery upkeep, productivity bonuses, portraits, or simulated individual workers are implied.

## QA
Composed output visually inspected: readable flow, accurate sample text/numbers, complete margins, no clipped content, navy/linen/brass/teal family.
Generated typography approximates the requested fonts; shipping body text must use Atkinson Hyperlegible, not the serif body seen in parts of the reference.
Opaque reference images; alpha testing not applicable.
No resizing, cropping, or resampling. Composed reference is enlarged design art, not a shipping 1:1 inspector texture.
Shipping work must reconstruct the panel natively, verify contrast and supported resolutions, and use approved assets.
