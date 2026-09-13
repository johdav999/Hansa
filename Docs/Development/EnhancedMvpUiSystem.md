# EMVP-P21 — Shared player UI system

## Design contract and inventory (before generation)

The visual source of truth is `Docs/UIDesignBrief.md`. Extend `FHansaUiStyle` and
`UHansaUiStyleLibrary`; do not introduce a competing theme. All generated images
in `Docs/Images/UI/DesignSystem/` are references, never shipping textures. All
functional geometry, labels, values, focus and state changes are native Slate.

The composed anchor targets a native 1536×1024 reference canvas. Each isolated
component reference targets 1024×1024 (1:1), with 48 px safe margins, opaque Linen
or Baltic Navy backing, orthographic front view and no perspective. These are
reference canvas sizes, not widget dimensions. If the generator returns another
native size, record that size without resampling. Runtime geometry uses the
brief's 8 px grid, 4 px micro-spacing, 16/24 px padding and 48 px focus targets.

| Component ID | Native implementation | Reference subject | States |
| --- | --- | --- | --- |
| ScreenShell | Safe-area overlay slots | World-dominant frame | normal, loading, modal |
| TopBar | Dark horizontal surface | Compact civic status strip | normal, paused, loading |
| BottomTray | Dark wrapping control host | Construction tray | normal, expanded, disabled |
| Tab | Selectable native button | Selected navigation tab | interactive matrix |
| CategoryButton | Labeled native button | Production category | interactive matrix |
| BuildingCard | Native content button | Building identity, costs, chain, reason | interactive matrix |
| ChainConnector | Native line/arrow | Directed production link | normal, blocked, stale |
| Panel | Native framed content slot | Linen working panel | normal, empty, loading, warning, error |
| TableRow | Virtualized table style | Selected ledger row | interactive matrix, stale |
| Tooltip | Native wrapped text/content | Cause and remedy tooltip | normal, focus, warning |
| Modal | Bounded native panel | Confirmation dossier | normal, warning, error, loading |
| Notification | Shape, text and native surface | Persistent warning | notice, warning, critical, success |
| Progress | Native labeled bar | Progress and reserve | normal, loading, warning, error |
| Chart | Native plotted geometry | Economic chart and legend | normal, empty, stale, estimated |
| Overlay | Native line/pattern | Invalid footprint indication | valid, warning, invalid |
| Cursor | Native pointer and status mark | Placement cursor | normal, drag, invalid, disabled |
| Focus | Two-tone native outline | Keyboard focus treatment | focused, high contrast |
| Icon | Scalable native glyph | Information symbol | normal, disabled, warning |
| Decoration | Non-hit-testable native ruling | Restrained brass header ornament | normal, reduced texture |
| Button | Native content button | Primary action | interactive matrix |

The interactive matrix is default, hover, pressed, selected, disabled,
keyboard/controller focus, loading, warning and error. Focus is independent of
selection and hover; disabled/loading actions cannot submit. Every status has a
label and shape. Native state overlays supply variants; no extra raster states
are necessary. Stateful content uses stable geometry and wrapped native FText,
with at least 30% localization expansion space. Semantic identity belongs to the
screen/presenter and must survive component reuse. Decorative layers never
intercept input.

## Shared material and accessibility rules

Dark floating controls use Baltic Navy/Harbor Slate with Chalk text. Working
surfaces use Linen/Parchment with Ink text. Oak is structural trim; Brass is
selection and ornament. Warning Amber is an accent with Ink text, never small
amber text on paper. Focus uses a dark under-ring so Brass is distinguishable on
both light and dark surfaces. High contrast strengthens outlines. Reduced motion
uses immediate state changes. Flat native centers and geometric borders avoid
painted nine-slice stretching entirely; future raster corners must be separate
1:1 assets with a tiled/procedural center.

## Acceptance tracking

P21 is implemented as a runtime native component foundation. The final selected
anchor is [the composed reference](../Images/UI/DesignSystem/design-system--anchor--default--1536x1024--v1.png).
It establishes hierarchy and material language; it is not a screenshot of the game
or an approval of generated sample data. Reference artwork is not imported into Content.

### API and assembly

`Source/Hansa/Public/UI/HansaUiComponents.h` exposes the shared components:

- `SHansaScreenShell`: safe-area content, top bar, centered bottom tray, inspector,
  notification and modal slots, using shared gutters. A presenter supplies bounded
  content and owns input mode, modal dismissal, focus restoration and semantic IDs.
- `SHansaSurface`: top bar, tray, panel, card, tooltip, modal and notification
  variants. `SetState` updates the shared accent, native glyph and localized status
  plus causal reason. A hidden status lane reserves space in the default state.
  Long reasons wrap; screen hosts must scroll bounded content.
- `SHansaAction`: primary/secondary/destructive/icon styles, used directly as buttons,
  tabs and categories, or with native content as building cards. `SetState` controls
  availability and the localized caption; focus remains independent of selection.
  Standard Slate mouse, Enter, Space and controller Accept behavior is retained.
  Disabled/loading actions reject activation. A paint-only content wrapper retains
  readable text without re-enabling input. Assign semantic metadata on the action.
- `GetLedgerRowStyle`: lifetime-owned by the consuming list/table, with alternating
  paper rows, selection, hover and independent focus. Use native `SListView`/
  `STableRow` virtualization and native aligned numeric text.
- `SHansaDiagram`: normalized, caller-supplied chart series, directed chain
  connectors, bounded progress and footprint geometry. It contains no economic
  formulas. Empty/loading/error chart states suppress data; incoming/estimated/stale
  series are dashed and reserve is dotted. Native summary/legend text accompanies
  geometry. Screen presenters supply authoritative axis/value descriptions and
  accessible data summaries, and allocate a bounded height with `SBox`.
- `SHansaGlyph`: information circle, warning triangle, error octagon, check,
  hourglass, arrow, cursor and decorative rule. Shapes retain a square silhouette
  in tall/wrapped layouts and never intercept input. Compose pointer + status glyph
  + tooltip for cursor variants; decorative ruling stays outside functional content.
- `FUiPreferences` selects high contrast, 125% text metrics and reduced motion.
  Panel entrances use the centralized 150 ms ease-out token with an active timer
  that stops when finished; reduced motion has no entrance animation. Hover/press
  feedback changes native fill/outline immediately, without moving the hit target.

The existing `UHansaUiStyleLibrary` remains the Blueprint-accessible token/style
facade; `FHansaUiStyle` remains the registered Slate style set. No second palette,
spacing scale or provider dependency was introduced. Rounded native geometry and
flat procedural centers implement the scalable material treatment; there are no
painted borders to stretch. Application UI scale changes native text/geometry only.
The scenario Close and Begin actions now consume `SHansaAction`, preserving their
existing handlers and automation semantics. P22–P29 assemble their own content from
this foundation; modal behavior, chart economics and build-card rules stay with
their existing screen/presentation contracts.

### Font resources and provenance

`Content/Hansa/UI/Fonts/` contains Source Serif 4 Semibold for civic headings and
Data, Atkinson Hyperlegible Regular for Body/Caption, and Noto Sans Symbols 2 as a
composite fallback for existing HUD symbols. Data uses verified equal-width digit
advances (520 font units). Swedish/German text, euro and digits were checked.
`provenance.json` records exact upstream URLs, SHA256 hashes, roles and glyph checks;
sibling OFL files travel with the fonts through UFS runtime dependencies. Fonts are
project-owned resources, not generated imagery. This is not a claim of complete
glyph coverage for every future localization language.

### Selected references and original-resolution inspection

All 21 selected images and sibling final prompts are in
`Docs/Images/UI/DesignSystem/`; `generation-records.json` retains generator source
provenance. Built-in ImageGen generated one composed anchor and one image per
distinct component. Anchor: 1536×1024. Top bar: 2172×724. The other 19 component
references: 1254×1254. Returned native sizes were preserved byte-for-byte; no
resampling, contact-sheet cropping or full-screen production bitmap was used.
Every original was displayed and inspected for subject, silhouette, dimensions,
aspect, margins, material/palette consistency and reference text. The ledger row
was regenerated as v2 to correct Grain; the rejected v1 is absent from the selected
folder. All other components use v1. Opaque backgrounds are intentional for these
references; none require production transparency.

The final prompt set is each image's sibling `.prompt.md`, including mode, intended
use, target and actual dimensions, inspection notes and image hash. Exact palette
tokens, native typography and native data override approximate generated colors,
ornament and sample values. Individual references illustrate shape/composition;
functional state variants are implemented natively. No generated production asset
or Unreal texture was promoted or imported by this task.

### Verification and scope

`python Scripts/ValidateUiSystem.py` checks all 21 selected originals, native PNG
dimensions, unique component coverage, sibling prompt hashes and font provenance.
`Scripts/RunAutomationTests.ps1 -Configuration DebugGame -TestFilter Hansa.UI.Style`
runs five style tests, including surface/state contrast, project fonts, input
rejection and keyboard/controller activation. DebugGame now selects the actual
DebugGame editor executable; `-debug` on the Development executable silently ran
older DLLs in this engine version.

`Scripts/CaptureUiSystem.ps1` runs `Hansa.UI.Style.RealViewport` in the actual Lübeck
game viewport. It validates native readback dimensions and captures a development
component gallery over the city, with default and accessibility preferences and
80–140% native UI scaling. These are real rendered viewport captures, not generated
artwork or an offscreen widget-only renderer. The gallery is compiled in the
non-Shipping test module and is never a player-facing menu. Native capture evidence
and exact test run paths are recorded in `Docs/Images/UI/DesignSystem/Native/evidence.json`.
The old scenario screen is visible behind the gallery; its remaining layout work
belongs to P28. Gallery evidence proves component rendering and does not approve
the later screens or the city's still-developing world art.

The full DebugGame editor build and the focused style/scenario suites pass.
Shipping packaging itself was not rerun for this presentation foundation; the
only added runtime staged files are the licensed fonts/OFL texts. Generated
references live under Docs, and the gallery stays in HansaTests. P22–P29 consume this API; this task does
not mark their screen journeys complete or claim the broader EMVP-UI release gate.
