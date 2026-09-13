# Integrated GUI repair

Status (2026-09-09): cross-screen engineering repair implemented and tested. Artistic production approval and human UAT remain open. This is not a declaration of AAA release readiness.

No `design.md` was found in the repository. Its location/identity is pending user clarification; the work follows the repository-authoritative `Docs/UIDesignBrief.md` and `Docs/UIAssetWorkflow.md`. No compliance with an unseen document is claimed.
Sources read: AGENTS, UIDesignBrief, UIAssetWorkflow, MVP and GUI root-cause review.
Existing HUD, Construction, City Overview, Market, Trade, Research, Scenario and Save/load references were inspected at original resolution; the comparison gallery identifies the exact files.
HUD style anchor: Docs/Images/UI/hansa-ui-main-city-hud.png.
It establishes a slim status strip, grouped notices, contextual linen inspector,
compact ledger rows, selective primary emphasis and a city-visible construction tray.
Its illustrative world, extra goods and mechanics are not implementation requirements.

## Screen and component inventory

| Family | Existing native surfaces | Components / references | Required states |
| --- | --- | --- | --- |
| Gameplay shell | Root HUD, inspector, alerts, trackers | slim navy strip; notice row; linen inspector; action row; existing native glyphs | resting, selected, warning, error, loading, no selection |
| Construction | categories, cards, chain expansion, placement | P22 card/chain references, native thumbnails and overlays | hover, drag, focus, valid, invalid, locked |
| City / Market | Population, Production, remote summaries, ten-good ledger | P24/P25 references; ledger, selected detail, native charts | selected, filtered, unknown, stale, empty, error |
| Trade | existing map and route editor | map canvas, cities, route list, stop editor | selected, invalid, stale, focus |
| Research | three branches and queue | branch nodes, detail, progress | locked, available, queued, completed |
| Scenario | briefing, objectives, outcomes | dossier header, progress, primary start/close | initial, active, victory, failure |
| Save/load | slots, details, confirmation | slot row, metadata, primary/secondary actions | empty, compatible, incompatible, overwrite, load failure |

All typography, controls, state feedback and changing data remain native. Default,
hover, pressed, selected, disabled, keyboard/controller focus and relevant loading/
warning/error states apply across the shared components. No new shipping raster was
introduced for functional surfaces. Selected reference images require sibling prompts.

## Review flows and gates

1. Ordinary HUD → bakery → output/input summary → optional cause → action.
2. Construction category → chain/card → placement and cancellation.
3. City population → production cause → Market → selected good/world relationship.
4. Trade map → existing route/stop workflow.
5. Research → inspect prerequisites/effects → queue.
6. Scenario objectives and save/load/confirmation/recovery.

Track functional, visual-conformance, accessibility/usability and performance
acceptance separately. Reference-to-native comparisons must assess geometry,
hierarchy, density and action emphasis, beyond palette and outer bounds.
Required matrix: 1280×720, 1920×1080, 2560×1440, ultrawide, UI scales 80–140%,
large text, high contrast and reduced motion. Human UAT is a separate open gate.

## Issue ledger

| ID | Severity | Finding / owner | Acceptance | Status |
| --- | --- | --- | --- | --- |
| GUI-RC-01 | P2 | Pixel tokens passed as points / shared fonts | Explicit unit conversion and measured native size | Fixed and regression-tested |
| GUI-RC-02 | P2 | Oversized controls and reserved status captions / shared actions | 40/48-unit total targets, consistent states, no redundant caption | Fixed and regression-tested |
| GUI-RC-03 | P2 | Healthy inspector dominated by diagnostic prose | Summary first, causes on demand, player-oriented copy | Fixed and regression-tested |
| GUI-RC-04 | P2 | Integrated hierarchy diverges from original reference | Screen comparison and real-flow inspection | Improved; final visual acceptance open |
| GUI-RC-05 | P2 | Inadequate visual acceptance / validation | Internal layout, density and reference review gates | Internal-geometry tests and comparison evidence added; human UAT open |
| GUI-RC-06 | P2 | Research lacks scrolling and stable refresh focus | Scrollable branches/detail, selected/focused node retained | Fixed; native focus checked |
| GUI-RC-07 | P2 | Save/load exposes implementation hashes | Player metadata first, diagnostic data excluded from default | Fixed; metadata/confirmation regression tests pass |
| GUI-RC-08 | P2 | Secondary screen preference/layout inconsistency | Shared fonts/actions/preferences and supported matrix | Shared preferences and native matrix implemented; exhaustive accessibility review open |
| GUI-RC-09 | P2 | Live HUD still displays demo treasury/population/workforce/objective | Use the same runtime projection as city reports; explicit unknown values | Fixed; new live-status regression |
| GUI-RC-10 | P2 | Unreal resolution DPI curve combines with explicit UI scale | One owner of UI scale, true native 720p geometry | Fixed; flat engine curve plus root scaler |
| GUI-RC-11 | P2 | Market sort label identity and limited content space | Column-keyed controls, readable row labels and scrolling | Fixed; final market keyboard/controller/world flow passes |

No screen is production-approved by this inventory. Earlier completion reports
describe implemented functionality; their visual readiness remains under review.

## What changed

- Shared typography converts brief pixels to Slate points (72/96). Button target dimensions include padding; state changes no longer add a caption row or move content. Minimum targets remain 48 physical pixels at 80%, with larger targets at larger scales.
- The HUD uses a bounded wrapping status strip, grouped notices and runtime treasury, population, workers, clock and objective data. Unreported income is not fabricated. Existing gameplay commands remain authoritative.
- The inspector has a navy identity header, a result-first production summary, distinct last-tick/capacity values, optional healthy diagnostics, one primary action, grouped neutral actions and quiet demolition. Stable widget identity preserves focus.
- Construction inherits corrected typography/control sizing and the existing category, chain, drag/placement and cancellation behavior.
- City Overview has compact summaries and paired fields. Market navigation shares the tab row; empty action results collapse. Market controls scroll independently, row metrics carry labels, sort labels bind to their column rather than array position, and selected-good details retain focus and reveal world buildings.
- Trade uses an opaque native shell, readable route/stop controls, human labels and explicit schematic-map identification. Native city/route data is preserved; the reference's extra cities and geographic features are not invented.
- Research has shared header/actions, independently scrollable branches/detail, readable effect labels and stable focus. Scenario uses three victory-path cards, native themed progress and a scrolling body with a persistent main action.
- Save/load presents player metadata, hides implementation hashes, keeps actions visible above preferences and isolates confirmation focus. Settings support 80–140% scale, large text, high contrast and reduced motion, persisted through the normal settings path.
- Root semantic focus now resolves the actual secondary-screen widgets. Changing preferences preserves the active screen and Market mode. Native captures assert internal usable areas, not merely screenshot existence or outer bounds.

## Generated references and asset integrity

All four new images use **built-in ImageGen, generate**. They are non-shipping visual references. No generated full-screen image is used as the interactive GUI; no new generated texture was imported or promoted to production content. Screens, text, controls, glyphs, charts and progress remain native Slate. Existing style-family references were reused for unchanged visual components.

| Component | Selected reference under `Docs/Images/UI/GuiRepair/` | Native dimensions | Final prompt / inspection |
| --- | --- | --- | --- |
| Composed healthy HUD | [guirepair--screen--healthy--1536x1024--v1.png](../Images/UI/GuiRepair/guirepair--screen--healthy--1536x1024--v1.png) | 1536 × 1024 | [Prompt](../Images/UI/GuiRepair/guirepair--screen--healthy--1536x1024--v1.prompt.md) |
| Action hierarchy | [guirepair--actions--default--1536x1024--v1.png](../Images/UI/GuiRepair/guirepair--actions--default--1536x1024--v1.png) | 1536 × 1024 | [Prompt](../Images/UI/GuiRepair/guirepair--actions--default--1536x1024--v1.prompt.md) |
| Healthy inspector | [guirepair--inspector--healthy--1024x1536--v1.png](../Images/UI/GuiRepair/guirepair--inspector--healthy--1024x1536--v1.png) | 1024 × 1536 | [Prompt](../Images/UI/GuiRepair/guirepair--inspector--healthy--1024x1536--v1.prompt.md) |
| Preferences | [guirepair--preferences--default--1536x1024--v1.png](../Images/UI/GuiRepair/guirepair--preferences--default--1536x1024--v1.png) | 1536 × 1024 | [Prompt](../Images/UI/GuiRepair/guirepair--preferences--default--1536x1024--v1.prompt.md) |

Every output was inspected at original resolution. The generator produced an unexpected portrait inspector, enlarged some controls and included inconsistent fonts/sample data. Those discrepancies are recorded beside each reference; they were not copied into the native implementation. Native dimensions and aspect ratios are preserved. Alpha is not required for these opaque, non-shipping references. No resizing, crops, mockup slicing or raster scale transforms were used to manufacture assets.

## Validation and reproducible evidence

Final Development build: `Saved/BuildArtifacts/20260909-102920931-build-HansaEditor-Win64-Development`.

- `Hansa.UI.`: **54 tests passed**, final run `20260909-102949662-automation-Hansa.UI.`. Includes new runtime-data and target-geometry regressions, plus existing input, presentation, accessibility, save/load, research, scenario and trade coverage.
- Real viewport suite: **nine screen families × two preference profiles × twelve resolution/scale combinations = 216 native captures**. Each profile opens the real Lübeck game screen; default and combined high-contrast/large-text/reduced-motion profiles run at each matrix point below. Native size, actual focus target and selected internal viewport geometry are asserted.
- Final Market and City Overview native interaction suites pass. Earlier HUD/inspector and construction native suites also pass. They exercise mouse/keyboard/controller routes, selection, loading/error/retry and world navigation as applicable. A test that incorrectly closed preserved Market mode after a preference change was corrected, then rerun successfully.
- Market refresh-budget test passed: 1,000 identical projections caused zero list refreshes; 200 sorts remained within the expected refresh count. Recorded times were 168.557 ms and 17.152 ms respectively in that local run. This is a structural microbenchmark, not a Shipping frame-time guarantee.
- [Original-pixel comparison gallery](GuiRepairComparison.html) contains nine screen/reference pairs with two selectable native profiles. [Capture manifest](../Images/UI/GuiRepair/Native/manifest.json) records dimensions, SHA-256 hashes, TSV companions and final test-log paths. Eighteen curated native captures are retained in Docs; all 216 are under `Saved/GuiRepair/Native/`.

| Resolution | Explicit UI scales tested | Profiles at every scale |
| --- | --- | --- |
| 1280 × 720 | 80%, 100%, 140% | Default; high contrast + large text + reduced motion |
| 1920 × 1080 | 80%, 100%, 140% | Same two profiles |
| 2560 × 1440 | 80%, 100%, 140% | Same two profiles |
| 3440 × 1440 | 80%, 100%, 140% | Same two profiles |

This is a sampled scale matrix, not every possible combination. Captures are real Unreal viewport readbacks using RenderOffscreen, not fabricated buffers or rendered mockups. Semantic TSVs supplement visual inspection; they do not prove every word is readable or every interaction is usable. Engine startup also reports unrelated experimental Toolsets/GameFeatures configuration errors; the GUI results are not represented as a clean whole-project release build.

## Remaining production acceptance gates

The root causes in the original screenshot were not just missing textures: incorrect unit conversion, double padding, diagnostic-first content and inadequate acceptance criteria drove the result. This repair addresses those engineering problems, but **AAA production acceptance remains open**:

1. Reference conformance: City needs presentation still uses detailed text fields; construction uses line glyphs rather than building portraits; research lacks the reference's prerequisite connectors; save slots lack screenshot thumbnails. These visible differences require deliberate design acceptance or additional implementation. Trade is explicitly a four-city schematic; it is not the illustrated geographic map in the concept.
2. Smallest-screen usability: at 720p/140% with large text, dense ledgers and settings require scrolling. Market filter/sort controls have a separate scroll region. The native areas and navigation pass, but this high-density state needs player validation. Automated area thresholds alone cannot certify production usability.
3. Exhaustive accessibility/localization: shared target, contrast and focus tests plus sampled high-contrast/large-text/controller flows pass. They do not certify every tooltip, glyph, localized string, screen reader path or input state on every screen. The supplied brief's full acceptance requirements still apply.
4. Motion/audio and performance: shared reduced-motion behavior and state tests pass; no final sound-direction review, all-screen motion review, or Shipping hardware frame-time/memory qualification was performed here.
5. Human acceptance: complete the MVP's 30–60 minute unscripted player session, including build → production issue → market/trade → research → save/load recovery. Artistic and usability approval must be based on the running GUI and this evidence, not the number of passing tests.
6. `design.md` remains unavailable. Reconcile it if it is a separate required specification. World geometry, terrain, lighting and building art visible behind the GUI are separate workstreams.

No release-readiness checkbox is closed by this report. Existing P21–P25 functionality reports are retained as implementation records, not retrospective artistic approval.

## See and test the result

From the repository root, run:

```powershell
.\Scripts\LaunchGuiPreview.ps1
```

This launches the built **Development** game preview on Lübeck. An already-running DebugGame session retains the old loaded DLL; a fresh Development launch is required to see these changes. The script does not close existing user sessions. Use `-Build` to rebuild first or `-Width 1280 -Height 720` for the small viewport.

Open **Save / load → Interface** for UI scale, high contrast, large text and reduced motion. Inspect a bakery, open Construction, use the city breadcrumb for Population/Production/Market, and open Trade map and Research from the top bar. Scenario is available through the existing scenario flow. Test save/load in a disposable slot and verify its confirmation and recovery behavior.

Reproduce engineering checks with:

```powershell
.\Scripts\RunAutomationTests.ps1 -TestFilter Hansa.UI. -SkipBuild
.\Scripts\CaptureGuiRepair.ps1 -Width 1920 -Height 1080
.\Scripts\CaptureGuiRepair.ps1 -Width 1280 -Height 720 -UiScale 1.4
.\Scripts\CaptureGuiRepair.ps1 -TestFilter Hansa.UI.MarketPolish.RealViewport
```
