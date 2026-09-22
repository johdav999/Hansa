# Reference HUD implementation — 2026-09-17

Implements `ReferenceHudUpdatePrompt.md` against `UIDesignBrief.md` and the supplied approved reference. All dynamic text, layouts, interactions and data are native Slate. Generated screenshots are non-shipping design references.

## Components and states

| Component | Implementation and states |
|---|---|
| Screen shell / top navigation | Continuous near-black navy bar; icon-first money, citizens, artisans, laborers; city, season/campaign year, speed, influence, research. Two-row compact layout at narrow logical widths and with large text. |
| Alert list | Separate framed tablets; icon plus short text; detail activation and expandable Frame/Snooze/Pin controls. Warning/critical glyphs and text; stable widget identity during live updates. |
| Minimap overlay | Live orthographic unlit scene capture (independent of cinematic exposure/bloom); camera footprint; click to move; wheel/buttons to zoom; arrow/controller navigation; visible focus ring. |
| Construction navigation / cards | Category labels and generated icons above larger building illustrations; scrollable chains/cards; reserves the open inspector width at small sizes; existing selected, locked, disabled, focus, hover and pressed behavior retained. |
| Details panel | Right-aligned framed native inspector; existing loading, unavailable, error, production, residence and causal detail views retained. |
| Frames / ornament | Native double brass rules and separate generated corner ornaments, dark or linen surfaces. |
| Icons / decorative imagery | Existing ImageGen icon family reused; new calendar/season emblem and engraved corner artwork. No dynamic text baked into shipping art. |

Interaction states use the existing shared SHansaAction style: default, hover, pressed, selected, disabled and keyboard/controller focus. Alerts retain textual severity and contextual remedies. Runtime data remains authoritative: influence displays an unavailable dash because no influence model is exposed; calendar uses the simulation season and campaign year, not an invented historical epoch. Construction categories represent the real catalog; no unsupported Civic category or fabricated buildings are added.

## Assets and provenance

- Approved style anchor: `Docs/Images/UI/ReferenceHud/approved-reference.png` (1672 × 941).
- Composed ImageGen reference: `Docs/Images/UI/ReferenceHud/referencehud--screen--default--1672x941--v1.png`.
- Separate component references in that folder: top-bar 2076 × 757; alert-tablet 2172 × 724; minimap 1254 × 1254; construction-tray 2172 × 724; inspector 1086 × 1448.
- Generated source masters: `SourceArt/UI/ReferenceHud/referencehud--corner--default--1254x1254--v1.png` and `referencehud--season--default--1254x1254--v1.png`.
- Runtime corner variants: `Content/Hansa/UI/ReferenceHud/Corner{0..3}--{28,40,56}.png`.
- Runtime season variants: `Content/Hansa/UI/Icons/Season--{16,20,24,28,32,40,48,56,64,80,96,112,160}.png`.
- Each generated master/reference has a sibling `.prompt.md` containing the final prompt, generation mode, intended use, requested/native dimensions and revision notes. Variant JSON records preserve crop and output sizes.

Generation mode: built-in ImageGen, reference-guided. Masters remain unchanged. GUI-only proportional Lanczos variants follow the repository's approved resizing exception. Corners are flipped into four orientations without stretching. Artwork was inspected at original resolution; the small season variant was checked on the intended dark background. Runtime PNGs are loaded as Slate brushes and included as UFS runtime dependencies; these are not imported Unreal Texture2D assets.

The supplementary component generations drifted in some details (calendar motif reused as decoration; construction/inspector surface balance). They are exploratory references only. The user's approved reference remains authoritative; those deviations were not copied into the implementation. No generated screen image ships as the GUI.

## Verification

The full HansaEditor Development build and final Hansa module rebuild succeeded. Intermittent MSVC C1001 internal errors were avoided on the successful final build by disabling UBA execution and limiting compilation to four concurrent actions.

`Hansa.UI.ReferenceHud.RealViewport` passed at 1280×720, 1920×1080, 2560×1440 and 2834×901. Each resolution captured six states: default shell, open construction/details, minimap keyboard navigation, large text/high contrast, 80% scale and 140% scale. All 24 captures pass native viewport bounds, square minimap geometry and construction/inspector separation checks. The minimap keyboard test verifies movement of the actual camera. Visual inspection checked dark/light surfaces, ornament placement, icon legibility, scrollable compact layouts and the live map.

Evidence: `Docs/Images/UI/ReferenceHud/verification-results.json`, `runtime-assets.json`, and `referencehud--ingame--*.png`. The real captures show the empty-start scenario's objective inspector because it contains no placed warehouse. They do not substitute fictional inventory values or a fabricated city. Detailed native screenshots and semantic bounds are also available under `Saved/ReferenceHud/`.

The HUD and inspector regression groups passed. The broader run currently reports 23 passing tests and four failing construction tests:

- `Hansa.UI.BuildMenu.CatalogAndLockedReasons`: old catalog/chain count and cost expectations differ from the current authored content (including Firewood).
- `Hansa.UI.BuildMenu.BakeryFitsBesideRoad`: completed bakery reports Blocked rather than the expected Ready.
- `Hansa.UI.BuildMenu.ShorelineBuildingsUseAuthoritativeTarget`: fixture cannot find the second shoreline footprint.
- `Hansa.UI.BuildMenu.SemanticsShortcutsAndFocus`: expects legacy placement action widgets in the focus order; those widgets are also absent in the HEAD version of the tray.

These failures are reported rather than hidden or weakened. No catalog, balance, shoreline-placement or production-state changes were made for this HUD redesign. The one include-order adjustment in HansaSimulationPipeline.cpp fixes its build requirement; the rest of that already-modified file is outside this task.

## Limits

The live minimap is an unlit top-down rendering of the actual map, not a fabricated illustrated city. World Partition cells outside the loaded terrain appear dark; this is not a baked whole-map atlas. Its camera footprint intersects the ground plane at Z=0 and may differ over elevated terrain. The existing contextual inspector retains its actual game data and available actions; the reference's warehouse capacities, reserve days and route actions are not invented when no backing model exists.
