# Top menu revision

Requested layout: treasury, signed monthly cash change and current-city population
on the left; three leading locally produced goods above laborers and wealthy
citizens in the center; icon speed controls at top right. All values follow icons.

## Components and states

| Component | Implementation | States |
| --- | --- | --- |
| Top shell with three anchored groups | Native Slate, navy/brass shared tokens | Default; compact viewport; large text |
| Metric chip (money, delta, population, good, citizen tier) | Reusable native icon/value with tooltip | Default; hover explanation; unavailable dash; signed gain/loss |
| Speed control | Existing SHansaAction with native glyph | Default, hover, pressed, selected, disabled, keyboard/controller focus |
| Secondary city/date/navigation row | Existing native controls | Default, focus, remote-city return |
| Tooltip | Native explanatory text | Hover; unavailable-data explanation |

No tables, charts, overlays or decorative raster imagery are needed. References
are non-shipping 1536x1024 images; shipping geometry and text remain native.
Palette: Baltic Navy #152A35, Harbor Slate #29424D, Brass #C19A52,
Chalk #FAF7EF. Existing Source Serif 4 data and Atkinson body fonts.

Production ranking uses recent actual local production from the city market
projection, descending quantity with stable good-ID tie breaking. No stock values
or forecast output are substituted. Wealthy citizens is the requested HUD label
for the existing artisan resident tier, explained in its tooltip.

Monthly money history is presentation-only, observed over 30 game days. Until a
complete window exists it is unavailable; history restarts when the player changes
or simulation time rewinds. It is not persisted by save/load.


## Implementation and assets

The native implementation is in `Source/Hansa/Private/UI/SHansaRootHud.cpp` and
`HansaHudPresentationModel.cpp`, using `SHansaGlyph` and `SHansaAction`. No new
raster is imported to Content. New reflected fields are presentation-only; no
simulation definition schema, authoring provider or save format changes.

Reference-only built-in ImageGen outputs, all native 1536x1024:

- `Docs/Images/UI/TopMenu/topmenu--screen--default--1536x1024--v1.png`
- `Docs/Images/UI/TopMenu/topmenu--metric--hover--1536x1024--v1.png`
- `Docs/Images/UI/TopMenu/topmenu--speed--selected--1536x1024--v1.png`

Each has a sibling `.prompt.md` with the complete final prompt, mode and QA record.
The composed reference anchors the metric and speed family; all references were
inspected. Shipping output consists of native Slate geometry, dynamic text,
semantic nodes, tooltips and existing focus/selection behavior. Status groups have
bounded widths on ultrawide screens.

## Verification

- HansaEditor Win64 DebugGame build passes.
- `Hansa.UI.HUD`: six tests pass, including authoritative city scope, production
  ranking and quantities, monthly money difference, rewind and load reset.
- `Hansa.UI.TopMenu.RealViewport`: real assembled game captures at 1280x720,
  1920x1080, 2560x1440 and 3440x1440, at 100%, 80%, and 140% UI scale. The 140%
  case enables large text, high contrast and reduced motion. Tests assert viewport
  bounds, center alignment, tier placement, tooltips and native keyboard speed input.
- The older HudPolish capture also exercised native mouse speed control successfully,
  but its overall verdict fails two inspector controller assertions (OpenCause
  navigation and Pin activation), outside this top-menu change.
- No raster resampling. Viewport PNGs and semantic TSV evidence are in
  `Saved/TopMenu/`; selected captures are copied into `Docs/Images/UI/TopMenu/Native/`.

## Limits

Monthly cash history is session-only and requires 30 observed game days after
start/load. Initial/unreported production uses a dash; goods are not invented
from warehouse stock. Wealthy citizens maps to the existing artisan tier. Full
Shipping package qualification and a broad localized-language playthrough were
not part of this top-menu verification.


Final evidence: build `Saved/BuildArtifacts/20260910-182545851-build-HansaEditor-Win64-DebugGame/`;
HUD tests `Saved/BuildArtifacts/20260910-182444796-automation-Hansa.UI.HUD/`;
final real-viewport gates `20260910-182632170-topmenu-native-1280-720`,
`20260910-182631953-topmenu-native-1920-1080`,
`20260910-182631753-topmenu-native-2560-1440`, and
`20260910-182637490-topmenu-native-3440-1440` under `Saved/BuildArtifacts/`.
All four gates pass three scale cases each. Edited-file whitespace check passes.


## Three-panel layout revision

Split the full-width top surface into three independent native navy/brass panels,
with transparent world-visible gaps. Keep all existing data, actions and tooltips.
Left: treasury, month delta, population, city overview and remote-city return.
Center: three product metrics, laborer/wealthy counts, date/time.
Right: speeds and connection state above research, save/load, trade map and menu.
Panels use the same existing border/surface tokens; navigation wraps at compact
widths and large text. All existing hover, pressed, selected, disabled and
keyboard/controller focus behavior is preserved. The shell is one reusable native
panel component with three content instances. No new shipping images or icons are
introduced by this layout-only revision. ImageGen-only artwork migration remains
pending the previously requested raster-scaling decision.

Reference plan: composed three-panel layout and one reusable panel-shell reference,
1536x1024 native canvases, non-shipping built-in ImageGen outputs.


## Bread balance revision (2026-09-11)

The center product row always tracks Bread, including when no bakery produces.
Empty slots are collapsed and omitted from semantic snapshots; remaining product
content is centered at its desired width. Existing ImageGen bread artwork, citizen
artwork, navy/brass shell, typography and component states are reused. This is a
native content/layout correction; no raster or new reference is generated.

The value is local production minus total citizen and industrial demand, normalized
to units per game day using the authoritative clock minutes per tick. It averages
complete reports from the most recent day (or the available startup reports; at
least one full report). Production is per report. Local demand is per tick, whereas
market-only background city demand is per report. Read-only projection metadata
copies these existing units and cadence so UI never guesses them from city names.
Stock and expected imports are excluded; unmet demand remains part of demand.
Positive numbers include +; deficits include -; balanced flow is 0. Up to two
decimals preserve small changes. Tooltip explains both rates and observed hours.
Missing reports show Bread with a dash and a pending explanation. Stale reports
are identified in the tooltip. No authoritative state, editor definition, save
format, provider integration or asset reference is changed; no migration required.

Native component inventory: existing panel shell, one centered Bread metric
(existing artwork plus native text), explanatory tooltip; existing citizen/date
rows. Surplus/deficit/zero/pending/stale have readable text, never color alone.
No new navigation, controls, charts, or decorative assets.

Verification: Development build and both Hansa.UI.HUD.TopMenu tests passed in
Saved/BuildArtifacts/20260911-072549853-automation-Hansa.UI.HUD.TopMenu.
Coverage includes positive/negative/zero flow, industrial demand, batch smoothing,
expired reports, background-city units, clock scaling, stock/import exclusion,
pending data and city switching. Native viewport checks passed at 1280x720
(20260911-072626056) and 1920x1080 (20260911-072638706), each at 80%, 100% and
140% UI scales, including large text/high contrast and ordinary keyboard input.
They assert Bread-only semantics, no empty slots, centering and viewport bounds.
Native screenshots and correlated TSVs are in
Docs/Images/UI/TopMenu/BreadBalance-20260911/. Original-size inspection confirmed
centered readable Bread -28.8, preserved citizen/date rows and no hammer placeholders.
The active DebugGame editor still needs closing before its DLLs can be rebuilt.
