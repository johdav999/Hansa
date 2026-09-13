# Product construction tray

Native component inventory: bottom navy category shell (existing), end-product selector button (icon and caption), compact recipe popover (heading, stage nodes and native arrows), native placement help/status and existing action controls. No tables or charts. Existing vector goods/building glyphs supply imagery; no production raster is required.

States: default navy/linen; hover HarborSlate; pressed inset; selected brass border plus label; disabled reason tooltip; keyboard/controller independent focus border; loading stable geometry; warning/error icon and explanatory text. Shared Source Serif/Atkinson fonts and Hansa tokens apply. Minimum 48px targets; 8px spacing; localization wraps. Popover sits above product row. Product click expands the authored chain; stage click selects building. Mouse move previews; press commits; held movement stamps valid nonoverlapping footprints; release ends stroke; Esc cancels. No invented costs.

ImageGen: composed reference followed by separate product-selector and stage-node component references. Reference-only native generator dimensions retained; no resampling. Runtime uses native Slate/vector assets.


## Final navigation revision

Category text is replaced by native Road, Building, Production, Storage and Harbor
glyphs. Names remain localized tooltips and semantic/controller labels. Existing
Civic/Decoration categories also have native glyphs when authored in the catalog.
Product selectors retain small end-good captions for recognition. Building tiles
use Farm/Mill/Bakery native glyphs and names. No new raster is shipped.

## Asset records

All references use built-in ImageGen and retain native dimensions. The composed
tray is 1536x1024; selector, stage, popover and final icon-navigation references are each
1254x1254. Exact final prompts and inspection notes are sibling `.prompt.md` files
in `Docs/Images/UI/ConstructionProducts/`. Navigation reference supersedes the
category captions in the earlier composed reference. Original-resolution review
accepted composition, subject, margins and palette. Raster alpha is not required
for opaque documentation references; no raster resampling or texture import occurs.

## Implementation boundaries

The existing authored construction-chain output/stage metadata drives selection
and ordering; no economic definition, editor schema, save format or provider
integration changes. Native UI consumes the same command gateway as gameplay.
The pointer stroke validates each traversed cell; occupancy rejects overlapping
footprints and visited anchors reject duplicates. Releasing ends the stroke but
keeps the building selected. UI crossing breaks the line segment, preventing a
bridge of accidental construction through the tray. Esc/category changes/closing
construction cancel the stroke. Existing roads retain connected path drawing.


## Verification (2026-09-10)

- Development and DebugGame editor builds passed: `20260910-151710135` and `20260910-151715660` under `Saved/BuildArtifacts`.
- All 10 `Hansa.UI.BuildMenu` tests passed in `20260910-151754670-automation-Hansa.UI.BuildMenu`.
- Actual native mouse journey passed in `20260910-151752934-construction-products-1920-1080`: passive ghost, click constructs exactly one, held movement constructs more, release stops construction. Fixture prepares a road through normal commands before placing farms.
- Native layout captures passed at 1280x720 (`151252166`), 1920x1080 (`151752934`), 2560x1440 (`151448774`) and 3440x1440 (`151612296`). Capture stages cover Bread, Fish, Planks, unavailable cards, placement feedback, controller focus, large text/high contrast/reduced motion, and long tooltip content.
- Original-resolution visual inspection confirmed legible compact tiles, category icons, chain direction and focus outlines at the four reference resolutions; no overlap or clipping in reviewed default/accessible views.
- Selected screenshots and semantic TSVs are archived in `Docs/Images/UI/ConstructionProducts/Native/`, with SHA-256 hashes in `manifest.json`. Full stage captures remain in `Saved/P22`.

References are documentation artwork only. Shipping visuals are native Slate glyphs,
buttons, borders and text; there are no new imported production raster assets.
Dedicated native mouse testing ran at 1080p; the other resolutions received layout
and keyboard/controller capture coverage. Custom economic recipe definitions and
non-MVP chain art were not added.


## Released-button preview repair — 2026-09-10

The preview previously read PlayerController.GetMousePosition, backed by
FSceneViewport.CachedCursorPos. Unreal clears that cache on mouse leave. A cleared
cache hid the preview even when Slate knew the live desktop cursor. A separate
screen-position early return also ignored stationary-pointer camera/focus changes.
The original native test selected a farm directly and forcibly focused the viewport,
so it did not exercise clicking and releasing a real residence card.

Placement now converts the live Slate cursor through the owning game viewport
geometry into viewport pixels, including viewport offset and DPI scaling. It checks
the exact owning viewport hit target so UI controls and other editor viewports do
not become placement surfaces. Each tick projects the cursor; unchanged grid cells
avoid repeated placement validation and ghost rebuilds. Selection survives release;
only a world press starts construction. No gameplay/editor schema or artwork changed.

The native regression now clicks/releases the actual laborer residence card without
forcing viewport focus, checks the cleared-cache condition, verifies a visible ghost
without construction, confirms one house on the next click, verifies held movement
and stops on release. The cleared-cache check fails before the fix
(20260910-163450134) and passes after it. Final Development build:
20260910-163844262. Final 1080p native construction suite:
20260910-163850165-construction-products-1920-1080.

The 720p regression initially chose sites beneath the expanded toolbar. It now
frames valid terrain above the toolbar and checks the real hit path before choosing
a site. UI protection remains active: only exposed game viewport space can place.
Selecting a card transfers input focus to the scene without capturing the cursor.

Final native suites pass at 1280x720 (20260910-165833981) and 1920x1080
(20260910-165928222), covering click/release selection, cleared cursor cache,
passive ghost, first-click construction, held stroke and release. All 10
Hansa.UI.BuildMenu tests pass (20260910-165942118). Final Development build:
20260910-165829039. Native after-release screenshot:
Docs/Images/UI/ConstructionProducts/Native/released-pointer-house-1920x1080.png.
