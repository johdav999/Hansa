# EMVP-P25 — Full Market

## Component inventory before implementation

| Component | Implementation and reference | States |
| --- | --- | --- |
| Screen shell/navigation | Existing P24 shell, P21 actions | open, close, focus |
| Search and filters | Native search plus shared actions | default, active, hover, pressed, focus, empty |
| Sorting and good rows | Native virtualized ledger, new row reference | ascending/descending, selected, shortage, stale, unknown |
| Selected-good panel | Native scrollable detail, composed reference | none, selected, loading, error |
| Price chart | Native shared palette lines, new chart reference | current, stale, single sample, empty |
| Production/consumption and relationships | Native metrics and shared reveal actions | current, missing, disabled, focus |
| Pin and route actions | P21 action reference | selected, unavailable, focus |
| Status/recovery/tooltips | P21 state surfaces and actions | loading, empty, error, retry, warning |

New reference-only images: composed screen, one reusable ledger row, one price chart.
Target supported native landscape dimensions 1536×1024; never resample. Existing P21
Panel/Action/Focus/Tooltip references resolve remaining components. No shipping raster.
Palette and typography follow UIDesignBrief; native text remains authoritative.

## Implementation and evidence

Implemented 2026-09-09. The full Market opens from City Overview's Market summary.
The shell hides civic summary cards while the ledger is open, leaving more room for
goods. Five proportional main columns and a wrapped secondary line replace eight
fixed-width columns. Search, all eight sort keys, category/trend/quick filters,
selection and pin/route actions use the existing typed presentation intents.

The selected-good panel exposes recent local production, actual fulfilled
consumption, citizen/industrial demand, reserve coverage, incoming supply, surplus
or shortage and a qualified route opportunity. Values come from authoritative
simulation projections; unknowns remain unavailable and sort after known numbers
in either direction. Price text keeps milli-mark precision. Producers and consumers
carry stable building identities; Reveal opens the real inspector and frames that
building. Background sources without buildings have an explicit disabled reason.
Route creation continues through the existing trade-map entry point; P26 owns the
complete route-creation redesign.

Native price geometry uses actual ticks, includes the reference average in its
scale, supports empty/single-sample history, and distinguishes stale price history
from the average with different dash patterns. Price strokes use the P21 Ink
outline plus Brass color. Text summaries expose tick range, price range, current
price, average and report confidence. No dynamic information is rasterized.

Shared actions, focus rings, proportional layout, wrapping and independent native
scrolling support normal and high-contrast/large-text modes. There is no chart
animation. Selection reuses row instances; identical projections do not rebuild
the ledger. Live tooltips bind current values and relationship rebuilds preserve
focused actions. Deferred focus scrolling accounts for newly wrapped detail rows.
City Overview supplies loading/error/retry surfaces and modal input protection.
Market semantic summaries include report age, production, consumption, causal
balance, chart samples and building IDs, with real widget bounds where materialized.

This is presentation-only work. No simulation definition, save format, editor
schema, provider integration or Shipping dependency changed.

## Generated references and inspection

All three selected references are in `Docs/Images/UI/MarketP25/`:

| File | Native dimensions | Mode |
| --- | --- | --- |
| market--screen--selected--1536x1024--v1.png | 1536×1024 | Built-in ImageGen, composed reference |
| market--row--selected--1536x1024--v1.png | 1536×1024 | Built-in ImageGen, component reference |
| market--chart--selected--1536x1024--v1.png | 1536×1024 | Built-in ImageGen, component reference |

Each has a sibling `.prompt.md` containing its final prompt, use, mode and revision
notes. Original-resolution inspection accepted palette, hierarchy, safe margins and
component relationships. Illustrative generated units, dates, numbers, labels and
ornament are not domain requirements. These opaque images are visual references
only; none is a production raster or imported Unreal texture. Shipping content is
native Slate. Existing P21 Panel, Action, Focus, Tooltip and status references cover
the reused components. No raster was resampled.

## Verification

- DebugGame editor build passed.
- 18 focused tests passed: MarketTable (5), SelectedGood (2), CityOverview (5),
  HUD (5), and MarketListRefreshBudget (1).
- Real Lübeck viewport tests passed at 1280×720 and 1920×1080. Native mouse selects
  Bread, Enter sorts prices, filtering preserves selection and rejects hidden
  targets, shared accessibility preferences rebuild the screen, loading/error/retry
  recover, and a visibly focused controller action opens the exact world building.
- 20 native PNGs and paired semantic TSVs cover ledger, selection, sort focus,
  filtered empty, accessible, loading, error, retry, scrolled relationships and
  world navigation. They are archived under `Docs/Images/UI/MarketP25/Native/`.
  A SHA-256 manifest records every archived file.
- `python Scripts/ValidateMarket.py` passed: dimensions, semantic bounds, metric
  presence, selection, state visibility, world targets and evidence hashes.
- Original-resolution inspection checked both reference resolutions, large text,
  contrast, chart lines, scrolling and focused producer actions. It found and
  corrected inherited dark search/header text, clipped columns, missing trend
  glyphs and focus scrolling before wrapped layout settled.
- The touched implementation files pass `git diff --check`.

Capture reproduction: `Scripts/CaptureMarket.ps1 -Width 1280 -Height 720` or
`-Width 1920 -Height 1080`, after a DebugGame build. The tests use the actual
Lübeck game viewport with native input and readback, not a synthetic table image.

Limits: interactive OS screen-reader behavior was not separately exercised.
Unavailable civic/remote reports remain governed by P24. Profit and destination
stock are not invented by this Market panel.


## P25 follow-up — native repeat input and building identity, 2026-09-09

Revalidated the existing ten-good Market after the shared GUI repair. This follow-up
keeps the established ImageGen design and fixes two concrete gaps in its input and
player-facing relationship presentation.

| Issue | Severity | Evidence and acceptance | Result |
| --- | --- | --- | --- |
| P25-UAT-01 | P2 | Enter on an already-selected good or already-clear filters returned Unhandled. A real 720p regression reproduced Slate's `Reply.IsEventHandled` assertion. Repeat activation must be consumed without changing selection/revision. | Fixed and verified at 720p and 1080p with repeated mouse clicks and native Enter. |
| P25-UAT-02 | P2 | Links displayed internal names such as Production 1 and Industry 2. A player must see the actual building while navigation retains its exact stable identity. | Labels now resolve the projected building definition; missing identity is explicitly unavailable. Producer identity regression and native world reveal pass. |

The button adapters consume no-change input; presentation-model mutation return
values remain unchanged. Row handlers reject goods outside the current visible
result set. Repeated clicks do not force a projection revision or list refresh.
Producer/consumer labels now use the real building name; resident consumers are
identified as residents of their projected building. Background supply stays
explicit. Relationship buttons wrap long labels using the shared action component.
No simulation, schema, save format or stable gameplay identity changed.

The new native regression first failed in
`20260909-122150201-gui-repair-1280-720` with the Slate keyboard assertion. The final
Development build `20260909-122720629` and all focused checks passed:

- Market presentation: 5 tests.
- Selected-good causal data and actions: 2 tests.
- Market refresh budget: 1 test.
- Real viewport/input flow: 10 states at each of 1280×720 and 1920×1080, including
  repeated no-change activations, sort/search, stable filtered selection,
  accessibility, loading/error/retry, focused producer scrolling and exact
  controller-to-world-building navigation.

The refreshed 20 PNGs and paired TSVs are under `Docs/Images/UI/MarketP25/Native/`;
`manifest.json` records SHA-256 hashes. Build/test provenance is in
`Docs/Images/UI/MarketP25/verification-20260909.json`. `ValidateMarket.py` passed on
the current archive. Native relationship and selected-good screenshots were
inspected at original resolution; no screenshot or raster asset was resized.

The existing three 1536×1024 built-in ImageGen references and sibling prompt records
listed above are retained. This is an implementation/copy repair preserving their
visual design, so no redundant generation or production import was performed.
The component inventory, state matrix and native-text/chart requirements remain
applicable. Broader artistic, human usability and Shipping acceptance remain
separate, as recorded in [GuiRepairSession.md](GuiRepairSession.md).

Current reproduction (the older DebugGame results above are historical):

```powershell
.\Scripts\RunAutomationTests.ps1 -TestFilter Hansa.UI.Market -SkipBuild
.\Scripts\RunAutomationTests.ps1 -TestFilter Hansa.UI.SelectedGood -SkipBuild
.\Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Integration.Performance.MarketListRefreshBudget -SkipBuild
.\Scripts\CaptureGuiRepair.ps1 -TestFilter Hansa.UI.MarketPolish.RealViewport -Width 1280 -Height 720
.\Scripts\CaptureGuiRepair.ps1 -TestFilter Hansa.UI.MarketPolish.RealViewport -Width 1920 -Height 1080
python Scripts/ValidateMarket.py
```

To play, run `.\Scripts\LaunchGuiPreview.ps1`, then open the city breadcrumb →
Market → Open full market. The fresh Development process contains these changes;
an already-running game retains its previously loaded DLL.
