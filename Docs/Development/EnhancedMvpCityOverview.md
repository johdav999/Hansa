# EMVP-P24 — City Overview

## Component inventory before implementation

All interactive output is native Slate. Generated images are visual references only.

| Component | Implementation / reference | States |
| --- | --- | --- |
| Management shell | Shared P21 surface, composed P24 reference | open, closed, responsive |
| City title and report context | Native text and shared glyph | current, stale, estimated, unavailable |
| Population / Production / Market tabs and Close | Existing P21 action reference | default, hover, pressed, selected, disabled, focus |
| Six summary cards | Native text; P24 summary reference | normal, warning, unknown |
| Cohort / production / good row | Native virtualized card; P24 row reference | selected, warning, unavailable, focus |
| Detail fields and supplying-chain action | Native wrapping text and shared action | ready, disabled with reason, focus |
| State panel and Retry | P21 state surface/action reference | loading, empty, error, retry |
| Scroll, focus, tooltips | Existing P21 native components | pointer, keyboard, controller, large text, reduced motion |
| Charts / overlays / decoration | No new chart or raster imagery required | explicit numeric trend and native state symbols |

Style: UIDesignBrief palette and P21 fonts, restrained linen/ink/brass, 8 px grid.
New references: composed screen at supported native landscape size, summary card and
row references at supported native landscape size. Never resample. P21 Panel, Action,
Focus, Tooltip and status glyph references remain the reusable component anchors.

## Implementation and verification

Implemented 2026-09-09. The native overview now fills the available height within
its bounded 1600-unit management width. Opaque shared surfaces, P21 fonts, shared
actions, native focus outlines, a linen list background, and wrapping field rows
replace the dense narrow-column layout. Only Population, Production and Market
appear as tabs; the old Administration placeholder is hidden.

Population presents tier totals, satisfaction, available workforce and employment by
tier, every configured need, access/affordability/reliability, consumption and reserves,
and growth/decline context. Service needs omit irrelevant goods quantities. Production
shows actual/nominal output, utilization, allocated/required workers and blocker state.
Market opens a readable ten-good overview with stock/reserve, demand, incoming supply,
price and supplying-production navigation. An explicit **Open full market** action
preserves access to the existing detailed market; its full redesign is P25.

Need links select supplying production, scroll it into view, and allow the native
Reveal buildings action to open the contextual inspector. Equally satisfied needs
prefer a good over a service and then the lower reserve, so a service with no supplying
chain does not mask an actionable good. Selection survives same-city projection
refreshes. Virtualized row objects survive value-only updates; field identity changes
rebuild the appropriate row. Mouse focus and keyboard/controller focus update the
same semantic model. Offscreen focus requests transfer to the materialized row.

The city switcher inspects Lübeck or Rostock reports. It does not initiate world travel.
Rostock civic statistics and production records are explicitly unavailable. Market
fields use the existing known-price and known-supply queries rather than current
unrestricted market values. Current/recent, stale, estimated and unknown report states
are labeled; missing values remain Unavailable. Stale values are labeled historical,
and estimated values indicative. Report age accompanies each good. Remote building
and full-local-market actions are disabled or absent. Modal semantic activation and
focus cannot reach construction behind the overview.

Loading, error and empty states retain city/tab navigation and recovery guidance.
Changing tabs cannot bypass an outstanding error. Retry requests a real refreshed
projection. Close returns to the HUD. High contrast, large text and reduced motion
use the shared preferences; transitions are immediate and no raster scales at runtime.

This changes presentation and adds two typed read-only host query forwards. It does
not change gameplay definitions, simulation behavior, economic tuning, save schemas,
editor schemas or provider integrations. Existing authoring remains applicable; the
new screen/component references are checked into the project for review.

## Verification and reproduction

UE 5.8 Win64 DebugGame build and focused automated checks passed. Suites cover:

- City Overview: 5 projection, selection, remote-knowledge, hidden-focus, tab and state tests.
- HUD: 5 regression tests, including child-screen integration.
- Market: 4 regression tests, including the explicit full-market entry path.
- Native viewport: one complete flow at each of 1280×720 and 1920×1080.

The native flow verifies mouse switching to Rostock, keyboard cohort selection,
controller tab navigation and building reveal, need-to-production navigation, real
retry recovery, and negative construction activation/focus. It captures 13 states
per resolution: population, focus, production, market, Rostock population, Rostock
production, Rostock market, loading, error, accessibility, long localization,
supplying chain and building cause. Default population/production/market data comes
from the real playable map. Loading/error are explicit presentation fixtures. The
Rostock screenshot records an actual stale report; the no-known-report case is also
covered by the projection test.

```powershell
Scripts/Build.ps1 -Configuration DebugGame
Scripts/RunAutomationTests.ps1 -Configuration DebugGame -TestFilter Hansa.UI.CityOverview -SkipBuild
Scripts/CaptureCityOverview.ps1 -Width 1280 -Height 720
Scripts/CaptureCityOverview.ps1 -Width 1920 -Height 1080
python Scripts/ValidateCityOverview.py
```

The validator checks exact PNG dimensions, semantic viewport bounds, modal construction
isolation, unavailable civic summaries, and loading/error visibility. Native mouse
events route through an explicit real window-to-widget path because offscreen windows
are excluded from platform hit testing. This verifies Slate input handling, not OS
hit testing or a physical controller session. A Shipping cook and the broader enhanced
MVP release gates were not run for this presentation change.

## Reference assets and visual inspection

Three built-in ImageGen references are stored in `Docs/Images/UI/CityOverview/`:

| Reference | Native dimensions | Prompt record |
| --- | --- | --- |
| cityoverview--screen--population--1536x1024--v1.png | 1536×1024 | same basename .prompt.md |
| cityoverview--summary--default--1536x1024--v1.png | 1536×1024 | same basename .prompt.md |
| cityoverview--row--default--1536x1024--v1.png | 1536×1024 | same basename .prompt.md |

All are opaque visual references, inspected at original resolution. Palette, hierarchy,
focus/safe margins and edges were accepted. Generated illustrative text, numbers,
charts and invented need categories are explicitly not domain requirements and are
not imported. Shipping widgets use authoritative native text and geometry. No new
production raster, texture import or SourceArt master is needed. P21 shared Panel,
Action, Tooltip, Focus and status references remain the component anchors.

`Docs/Images/UI/CityOverview/Native/` contains 26 unresampled viewport PNGs, paired
semantic TSVs and a SHA-256 manifest. Original-resolution inspections checked the
720p population/needs and large-text layout, 1080p remote-report and market layouts,
focus rings, opaque text surfaces, scrolling and state recovery. The final plan keeps
all operational data native; the three generated references are never shipping GUI.


## Final local evidence locations

Under `Saved/BuildArtifacts/`:

- `20260909-074648001-automation-Hansa.UI.CityOverview` — 5 passed.
- `20260909-074707230-automation-Hansa.UI.HUD` — 5 passed.
- `20260909-074303306-automation-Hansa.UI.Market` — 4 passed.
- `20260909-074721951-p24-native-1280-720` — native flow passed.
- `20260909-074750281-p24-native-1920-1080` — native flow passed.

The archived PNG/TSV manifest and validation script preserve reviewable evidence
without relying on ignored local build logs. `git diff --check` passed.

## P24 follow-up — recovery readability, 2026-09-09

Revalidated the existing P24 implementation after the cross-screen GUI repair. The
Population, Production, Market and report-aware Rostock flows remain implemented;
this follow-up fixes an uncovered recovery-state defect rather than rebuilding the
screen or changing its visual direction.

| Issue | Severity | Finding / acceptance | Result |
| --- | --- | --- | --- |
| P24-UAT-01 | P2 | Error text used Ink on the critical navy surface; cause and remedy were nearly invisible. Use readable text on the actual surface in default/high-contrast modes and reset it for loading. | Verified with native captures and actual-widget contrast regression. |
| P24-UAT-02 | P2 | Retry used an older button without shared preference-aware targets/focus. Use the shared action, preserve the minimum physical target at 80%, and recover through normal keyboard activation. | Verified with target regression and native Enter-to-retry flow. |
| P24-UAT-03 | P3 | Failure copy mentioned an authoritative projection and incorrectly suggested keeping Lübeck selected even for a Rostock failure. | Replaced with city-report wording and a city-independent retry instruction. |

Error summary labels and values also switch to Chalk on dark critical cards. The
state message uses a bounded native wrap width. Existing ImageGen composition,
summary and row references above were reused; the composition was re-inspected at
1536×1024. No new design, generated image, production texture or asset promotion was
needed for fixes that preserve the established design. The existing prompt records,
native geometry and asset-integrity rules remain applicable.

Final validation: Development build `20260909-121609059` passed; **six**
`Hansa.UI.CityOverview` regression tests passed; both real-viewport flows passed,
producing **26 native captures** at 1280×720 and 1920×1080. The capture flow now uses
Enter on the focused Retry control, then verifies restoration of the real report.
The evidence validator checks the refreshed archived PNG/TSV set. Native error
screens were inspected for text contrast, readable cause/remedy, focus and target
placement. No generated mockups or resized screenshots are used as test output.

The current capture runs and SHA-256 hashes are in
`Docs/Images/UI/CityOverview/Native/manifest.json`. Prior error images are retained
under `Docs/Images/UI/CityOverview/Review-20260909/` for comparison. The earlier
DebugGame evidence above is historical; reproduce current work with:

```powershell
.\Scripts\RunAutomationTests.ps1 -TestFilter Hansa.UI.CityOverview -SkipBuild
.\Scripts\CaptureGuiRepair.ps1 -TestFilter Hansa.UI.CityOverviewPolish.RealViewport -Width 1280 -Height 720
.\Scripts\CaptureGuiRepair.ps1 -TestFilter Hansa.UI.CityOverviewPolish.RealViewport -Width 1920 -Height 1080
python Scripts/ValidateCityOverview.py
.\Scripts\LaunchGuiPreview.ps1
```

Open the city breadcrumb in the fresh Development preview; select Population,
Production or Market, or switch to Rostock for the read-only report summary.
The broader artistic, performance, accessibility and human UAT gates documented in
[GuiRepairSession.md](GuiRepairSession.md) remain separate from P24 functionality.
