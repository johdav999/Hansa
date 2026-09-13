# Rolling citizen demand fulfillment

The residence extension now uses save format 5 / fingerprint 18 and preserves
this market history. See [ResidenceRollingFulfillment.md](ResidenceRollingFulfillment.md).
The format-4/fingerprint-17 results below record the original market increment.

The selected market's product rows show `sum(consumed) / sum(required)` over the
last 30 game days. Amounts are stored as integer milli-units; rounding happens
only for display. This is actual citizen consumption, not production, inventory,
incoming cargo, satisfaction, or an average of tick/residence percentages.

## Runtime contract

`FHansaConsumptionHistory` records one canonical city/good aggregate after each
`WorkforceAndNeeds` step. The current tick's `RequiredLastTick` and
`ConsumedLastTick` values already account for residents, access, affordability,
and successful inventory transfers. Services are excluded. Historical records
survive residence removal and tier changes and expire by simulation time.

The clock defaults to 60 minutes per tick, giving 720 retained ticks. The window
uses the configured minutes per tick, never wall-clock time, market-report
cadence, UI refreshes, or playback speed. Alternate clock cadences retain whole
ticks fitting within 43,200 minutes (a non-divisor cadence covers up to one tick
less than 30 days). Empty population steps still record coverage. Duplicate or
nonconsecutive ticks, invalid quantities and overflowing sums are rejected.

`FHansaSimulationProjection::GetCitizenConsumption()` owns the same totals and
coverage used by the UI. The automation `market.components` query includes
`citizenFulfillment30Days`: coveredMinutes, fullWindow, known,
requiredMilliUnits, consumedMilliUnits, and percent when demand is positive.
Remote background market-only demand is not relabeled as measured citizen
consumption; this metric concerns the simulated residence cohorts.

## Presentation and component inventory

Reuse the approved MarketInspector shell, header, Close/Details controls,
product icons, focusable product rows and rounded bars. All updates are native
text/data changes; no image generation, new raster masters, imports or palette
changes are required. Existing reference images remain visual style anchors.

- Each product displays its rolling percentage and consumed / required units.
- The panel and row tooltip identify the recorded period.
- A complete window reads “Last 30 days”; startup/old saves show the actual
  first recorded days, hours and minutes.
- No history shows a dash and pending wording; zero demand shows a dash and
  “No demand in this period”; zero consumption against positive demand is 0%.
- Product semantic IDs are preserved; their values include period metadata.
- Existing hover, focus, Details, scrolling and accessibility states are reused.

## Saves, editor parity and impact

Save format 4 adds history; fingerprint contract 17 hashes all samples in the
Population subsystem. Formats 1–3 with fingerprint 16 are checked using their
original checksum before migration, then receive empty history and explicit
`Hansa.Save.3To4.StartConsumptionHistory` provenance. No past consumption is
invented. Current saves preserve history and deterministic continuation.

This adds runtime reporting state, not authored gameplay tuning or definitions.
The 30-day reporting policy has no editable/AI-generatable property. Existing
need amounts and clock configuration keep their schemas, metadata, validation,
import/generation workflow and economic meaning. No content/registry migration
or new provider/editor dependency is introduced. Save validation shares the
history invariants and checks city identities; unknown definition hashes remain
incompatible. No generated content or staging asset is promoted.

## Verification

Targeted suites: `Hansa.Simulation.Population`, `Hansa.Integration.Save`,
`Hansa.Simulation.Diagnostics`, `Hansa.UI.MarketInspector.DemandAndSelection`.
The rolling-history test exercises weighted demand, 70% fulfillment, city
separation, service exclusion, expiry, multiple clock cadences, removed homes,
empty histories, old-save startup, duplicate ticks and overflow. Save tests
compare retained quantities and deterministic continuation.

`Scripts/CaptureMarketInspector.ps1` captures the actual game viewport and
checks every displayed quantity against the rolling projection. Capture results
and final validation are recorded after running the tests.

## Verified results — 2026-09-11

- HansaEditor Win64 Development rebuild passed.
- Population: 9 tests passed, including RollingConsumption.
- Save integration: 10 tests passed after final saved-history validation changes.
- Market inspector product values/selection: 1 test passed.
- Determinism diagnostics: 5 tests passed.
- Actual game viewport at 1280x720: demand, Details, accessibility/focus and full
  30-day stages passed. Structured assertions match every displayed quantity to
  the authoritative rolling projection. Native pixels inspected; no clipping
  in the product rows, percentages or period label.
- 720p example after a full window: Bread 80.2 / 864 units, 9.3%; Beer
  61 / 345.6, 17.7%; Fish 28 / 518.4, 5.4%. These are fixture observations,
  not hardcoded UI values.

Logs under Saved/BuildArtifacts:

- 20260911-074754314-automation-Hansa.Simulation.Population
- 20260911-075343499-automation-Hansa.Integration.Save
- 20260911-074923665-automation-Hansa.UI.MarketInspector.DemandAndSelection
- 20260911-075451220-automation-Hansa.Simulation.Diagnostics
- 20260911-075453043-market-inspector-1280-720

The foundation fixture's expected checksum is explicitly updated for fingerprint
17 to E39DB36B589D8616. Its previous C827BB245C79D327 golden already had documented
baseline drift (see EnhancedMvpEconomy.md); the new value establishes the current
contract rather than implying that historical drift was caused by this feature.
The fixture has no economic registry, so this change records the fingerprint
contract and empty history representation, not altered economic behavior.

The feature introduces no art assets: generation mode, native master dimensions
and new prompt set are not applicable. Existing approved product art and native
market components are reused. This was a targeted feature verification, not a
full MVP playthrough or Shipping cook/package audit.

- Final 1920x1080 partial, Details, accessible and full-window captures passed:
  Saved/BuildArtifacts/20260911-075538508-market-inspector-1920-1080.
- Final 2560x1440 captures passed:
  Saved/BuildArtifacts/20260911-075649567-market-inspector-2560-1440.
- The final high-contrast/large-text 1080p panel was inspected at native pixels:
  labels, quantities, percentages and focus outline remain readable.
- Full-size PNG evidence and correlated semantic TSV files are in
  Saved/MarketInspector/market-<width>x<height>-<stage>.*; stages are demand,
  details, accessible and 30-days. These are real-game evidence captures,
  not generated references or production texture assets.

- Final 3440x1440 captures passed:
  Saved/BuildArtifacts/20260911-075749026-market-inspector-3440-1440.
- Native 1440p and ultrawide full-window panel crops were inspected without
  resampling. Product rows and the period label remain readable and unclipped.
- All four supported capture resolutions passed all four stages; 25 targeted
  non-rendering regression tests passed. No new generated image was needed.
