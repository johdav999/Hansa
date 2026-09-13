# Compact production inspector — 2026-09-10

User direction supersedes the earlier tall bakery inspector: worker portrait,
production title, input(s) → animated batch ring → output(s), coin and labor footer.
Keep Hansa navy, linen, brass, ink and teal; original imagery and native controls.

## Component inventory / pre-generation specification

- Shell: native 320-wide, up to 480-high compact bottom-right inspector at 1080p;
  responsive bounded height and scrolling at smaller screens. Transparent portrait
  surround; linen operational surface; 16/8/4 spacing, 48-pixel controls.
- Portrait: original Hanseatic worker engraving, waist/shoulder bust in a native
  brass/linen arch above the header. Square 1024 reference, intended scalable ink
  recreation (not a resized raster). Same shared worker for initial bread chain.
- Header/navigation: navy native production name, close; information/Details toggle.
- Flow: existing engraved grain/flour/bread vectors, native quantities/names,
  existing animated brass circle. Sources explicitly show no input. All ports kept.
- Controls: compact pause/resume and Details. Storage, chain, pin/frame and demolition
  available in expanded details. No unrelated extra dashboard by default.
- Footer: native coin and worker glyphs with dynamic values, two readable labeled
  cells; no currency operating-cost field exists, so the cost value is blank as explicitly requested.
- Tables/lists: previous stock, reservations, workforce breakdown and production
  record retained in expandable Details, with causal feedback and tooltips.
- Status: readable producing/paused/blocker text, symbol and cause; tooltip exposes
  current tick timing without claiming productivity or an invented currency charge.

States: open/closed, default/hover/pressed/focus/disabled/selected controls, producing,
paused, blocked input/workforce/storage, unavailable cost, expanded/collapsed,
large text, high contrast and reduced motion. Loading/error keep legacy inspector.

New composed reference, portrait reference, shell reference and footer reference
are generated separately using built-in ImageGen. Existing good/flow components
retain their prior selected references. All dynamic text and charts are native.


## Hover popup addition

Native tooltip on the ring and its percentage content: Batch process title,
Batch time (1x), and % completed. Duration is mm:ss from CycleTicks because the
runtime normal-speed adapter advances one tick per real second. Percentage is
refreshed from authoritative batch progress, including while the popup is open.
Popup artwork is a separate reference; all content is native and event-driven.


## Final implementation and assets

Native `SHansaProductionInspector` replaces the default tall presentation for
production units. The shared root host uses 320x480, bounded by available height,
with bottom-right anchoring and a transparent portrait surround. Other inspector
types retain their existing layout. All typography, palette and interaction
styles remain centralized. Large text uses a smaller 80x64 decorative portrait
frame with 56-unit vector art to leave room for the full process flow.

Selected built-in ImageGen references:
- `Docs/Images/UI/Production/Compact/production--compact-panel--default--992x1586--v1.png`
- `Docs/Images/UI/Production/Compact/production--compact-shell--default--1254x1254--v1.png`
- `Docs/Images/UI/Production/Compact/production--compact-footer--default--1254x1254--v1.png`
- `Docs/Images/UI/Production/Compact/production--compact-tooltip--default--1254x1254--v1.png`
- `SourceArt/UI/Production/production--compact-worker--default--1254x1254--v1.png`

Every selected image has its full final prompt, generation mode, dimensions and
revision notes in a sibling `.prompt.md`. Requested 1024 sizes were returned at
the native sizes listed above and retained without resampling. The illustrations
were inspected at original resolution for subject, silhouette, safe margins,
text and palette. The worker is an original engraving, not a copied character.
The initial composed/footer references illustrate a dash; the user's subsequent
instruction is authoritative and the implemented cost amount is blank.

These PNGs are references/source masters, not interactive shipping textures.
The engraved worker is recreated as scalable ink geometry by the existing
`Scripts/TraceProductionGoodIcons.py`, preserving native ink samples as SVG
paths. Source hashes and vector provenance are in
`SourceArt/UI/Production/provenance.json`. Runtime assets are
`Content/Hansa/UI/Production/worker.svg`, `cost.svg`, and `labor.svg`; existing
`grain.svg`, `flour.svg`, and `bread.svg` are reused. SVGs are staged by the
existing runtime dependency declaration. No raster was resized or imported as a
full-panel texture. Header, arch, circle, popup, controls and all values are native.

No economy/save/definition schema changed. The existing registry remains the
source of recipe and labor requirements. There is no per-batch operating-cost
field, so no construction cost or ingredient valuation is repurposed. This is a
UI presentation change, requiring no migration or new editor data schema. The
shared worker portrait is decorative, not a simulated individual employee.

## Verification coverage

The native viewport test now checks compact bounds, blank cost, real production
selection for farm/mill/bakery, moving arc, controller activation in expanded
Details, fully visible focused controls, and the complete large-text process
flow above fixed controls. It moves the pointer onto the actual ring, waits for
the real Slate tooltip to open, validates its percentage, advances a simulation
tick, and verifies the popup updates without re-hovering. A separate native
capture records the actual hovered tooltip window at its own pixel size.
The cursor is restored on completion or early exit.

Presenter tests verify every recipe port, joined stock/reservations, explicit
multi-input/output fixtures, blank cost, mm:ss batch duration, Details expand/
collapse, normal-speed interpolation, pause and real batch completion. Existing
inspector regressions also run. No full Shipping cook/package is claimed.


## Final results

- Development build: `20260910-143857354-build-HansaEditor-Win64-Development` — passed.
- DebugGame build: `20260910-144132483-build-HansaEditor-Win64-DebugGame` — passed.
- Two production inspector tests: `20260910-143952565-automation-Hansa.UI.ProductionInspector` — passed.
- Six existing inspector tests: `20260910-143953831-automation-Hansa.UI.Inspector` — passed.
- 1280x720 real viewport: `20260910-143938595-production-inspector-1280-720` — passed.
- 1920x1080 real viewport: `20260910-144007017-production-inspector-1920-1080` — passed.
- 2560x1440 real viewport: `20260910-144037894-production-inspector-2560-1440` — passed.
- 3440x1440 real viewport: `20260910-144111912-production-inspector-3440-1440` — passed.

Logs are under `Saved/BuildArtifacts/`. Native PNGs, semantic TSV evidence and
hashes are archived under `Docs/Images/UI/Production/Compact/Native/`.
The actual hovered popup is 280x123 pixels; it is saved separately from the
native game viewport, not composited into or scaled over a screenshot.

At 1080p the default inspector bounding box is 320x480 versus the earlier
400x844, a 54.5% area reduction. The cost amount remains blank; bakery labor
requirement is 6 (4 laborers, 2 artisans). Hover duration is 00:45 at 1x for the
current bakery recipe; completion percent updates with authoritative ticks.

[Live bakery](../Images/UI/Production/Compact/Native/production-1920x1080-batch-a.png),
[hover popup](../Images/UI/Production/Compact/Native/production-1920x1080-tooltip-popup.png),
[large-text 720p](../Images/UI/Production/Compact/Native/production-1280x720-accessible.png).


Final polish: new production selections start with Details collapsed, including
paused or blocked units. User expansion is retained while the same building
refreshes. Legacy residence/alert expansion behavior is unchanged. Large-text
portrait corner radii fit its smaller frame. Native captures temporarily park the
cursor outside HUD controls before the explicit batch-hover stage, then restore
its original position, preventing unrelated tooltips in evidence.


Final original-resolution visual QA: 1080p bakery shows the worker portrait,
compact recipe flow, all primary controls and footer with a blank cost amount.
The actual hovered navy popup has readable duration and live completion values.
The corrected 720p large-text view shows the full circular process display,
wrapped output label and focused Pause control without overlap. No raster
resampling was used. Both editor build configurations include this final revision.

The batch tick counter is removed from the player-facing panel (2026-09-10). The ring retains percent progress; remaining time uses minutes:seconds and the hover popup retains batch duration. Raw tick fields remain internal automation data.
