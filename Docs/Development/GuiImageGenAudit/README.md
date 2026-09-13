# All game menus: ImageGen asset migration

Scope: title/frontend/settings, session/scenario/help, save/load, root HUD and
alerts, construction, city overview, market, research, trade map/route creator,
context/production/residence/market inspectors and shared feedback components.

Inventory: inventory.json records the source uses of glyphs, image paths and
symbol text. Existing generated portrait/goods masters have provenance but are
currently displayed through derived SVGs; shared icons are manually drawn Slate.

Component families: generated goods icons; generated system/status/navigation
icons; generated building/category icons; generated citizen/worker portraits.
Native shell, panels, lists, tables, charts, controls, dynamic text, tooltips,
focus/selection outlines, and progress geometry remain native per the brief.

States: default artwork; hover/pressed feedback on native control surfaces;
selected brass outline; disabled opacity plus reason; keyboard/controller focus
outline; loading/warning/error generated symbols plus localized explanation.
No state changes may stretch painted artwork. Existing UI palette and typography
remain authoritative. No new decorative background imagery is requested.

Sizing gate: existing icons use 20, 24, 28, 32, 40, 48 and 64 logical units and
UI scale 80–140%. Portraits use 56, 80 and 112 logical units. The brief forbids
resampling generated raster pixels. Before replacing the shared renderer, verify
that ImageGen can supply native small artwork suitable for those uses.

Status: audit in progress; no menu is yet certified ImageGen-only.


## Sizing test result

Two built-in ImageGen calls requested a native 32x32 icon or a tiny icon on a transparent canvas. Both returned 1254x1254 images with artwork larger than 32 pixels. See sizing-results.json and Docs/Images/UI/AllMenus/SizingTrials. Neither trial is imported or accepted as production art. The current no-resampling policy prevents using these outputs at the existing GUI icon sizes. A proportional downscaling exception would allow independently generated source masters to supply small GUI assets while preserving aspect ratio and reviewing legibility at actual display size. No scaling exception has been assumed.

## Approved migration

The user approved proportional resizing on 2026-09-10. Generate each icon separately near a 32px target; preserve native originals, crop unused transparent margins, create 20/24/28/32/40/48/64px variants and a 96px density master, and review the assembled UI at 80%, 100%, and 140%. Existing generated portraits will receive real raster display variants. Native text, panels, charts, controls, focus and selection surfaces remain native. The icon state matrix above applies to every asset.


## Completed migration — 2026-09-10

The earlier resizing blocker is resolved by explicit user authorization. See [final results and verification](RESULTS.md). 52 generated icons and two new transparent portraits are integrated as runtime PNG artwork.
