# Residence rolling product fulfillment

The residence inspector displays each product's actual consumed / required
quantities and fulfillment percentage over its own last 30 game days. It reuses
the market's bounded integer consumption-history implementation, stored on the
authoritative residence cohort rather than borrowing city totals. Empty homes
have no prospective product demand. Historical demand remains after residents
leave; history also survives a residence upgrade. Removing a residence removes
its own inspection history, while the city market retains its historical totals.

The recorded window begins when the cohort is first evaluated, including empty
steps. During startup or migration the panel shows the actual recorded days,
hours and minutes. A complete window shows “Last 30 days.” No history is pending;
zero required quantity shows “No demand in this period” and a dash. Positive
demand with zero consumption shows 0%.

Basic services retain their current service metric and have a visible “Current
service level” label. Services have no consumed/required product units and are
not included in product consumption history. Product tooltips explain the
residence scope and period; current access and affordability remain causal
details rather than substitutes for actual historical consumption.

## Components and artwork

Reuse the approved portrait, navy header, linen shell, Close/Details controls,
product icons, need labels, bars, tooltips and focusable native rows. Add native
quantity captions to rows and the recorded-period label in the state area.
The default residence host is 320x600 Slate units, bounded by available viewport
height; scrolling and focus reveal retain all content in compact windows.

States: pending, no demand, measured zero/partial/full supply, empty/occupied
residence, current service, Details expanded/collapsed and controller focus.
No new raster/reference image, generation prompt, palette or typography is
introduced. The existing approved artwork is reused without resizing.

## Persistence, validation and authoring parity

Save format 5 adds each cohort's history; fingerprint 18 includes its samples
in the Population subsystem. Format 4 / fingerprint 17 is checked against its
original checksum before migration. Its city history is retained and residence
histories start empty: historical city totals cannot be reliably allocated to
individual residences. Formats 1–3 / fingerprint 16 retain their prior migration
chain and also start empty residence histories. No historical demand is invented.

The real format-4 automation fixture is retained as
Tests/Fixtures/residence_consumption_prior_v4.hansa (copied from the earlier
market implementation's automation session, not a player save). It starts at
tick zero and has no consumption yet. Tests verify explicit migration and only
one recorded tick after advancing. Current-format tests cover nonzero history,
canonical byte round trips, upgrade continuity and deterministic continuation.

The new field is derived runtime reporting state, not an authored definition,
economic parameter or AI-editable field. Existing need/clock schemas, metadata,
validation, imports and generation contracts are unchanged. Save validation
checks sample order, bounds, quantities, sums, city ownership and good identity.
No new editor/provider dependency, generated content or staging promotion occurs.

## Read models and automation

`FHansaPopulationCohortProjection::Consumption` owns the residence's totals and
coverage. The existing population-cohort automation query exposes
`consumption30Days` with coveredMinutes, fullWindow, known and per-good
requiredMilliUnits, consumedMilliUnits and percent when demand is positive.

Existing `Inspector.Residence.Need.*` semantic IDs include the raw quantities,
percentage and residence/current-service scope. The new
`Inspector.Residence.ConsumptionPeriod` ID exposes coverage and the period label.
The market inspector continues to use the city aggregate.

## Verification plan

- Residence UI: per-home values, 70% example, current services, pending/no-demand,
  empty homes, partial/full periods, and sums across multiple homes matching the
  city's totals.
- Population: actual demand/consumption after a tick, empty-home recovery,
  upgrade history retention and existing bounded-window tests.
- Save: current histories, prior format migration and deterministic continuation.
- Real viewport: empty, evaluated, Details, accessibility/focus, tooltip,
  occupied and full-window residence captures at supported resolutions.
- Market regression: city totals and existing product-row behavior unchanged.

Final logs and visual inspection results are recorded after validation.

## Validation results — 2026-09-11

- HansaEditor Win64 Development final build and residence UI test passed:
  Saved/BuildArtifacts/20260911-085053430-automation-Hansa.UI.ResidenceInspector.AuthoritativeOccupancyAndNeeds.
- Population suite: 9 tests passed, including actual per-residence quantities,
  upgrade retention, empty-home history and bounded-window coverage:
  Saved/BuildArtifacts/20260911-084557437-automation-Hansa.Simulation.Population.
- Save suite: 11 tests passed, including a real format-4 migration and nonzero
  residence history round trips/continuation:
  Saved/BuildArtifacts/20260911-084436545-automation-Hansa.Integration.Save.
- The residence UI test proves that per-home totals partition the city totals;
  changing an individual product to 7,000 consumed / 10,000 required produces
  70%, and an empty measured home shows a dash rather than 0% or 100%.
- The first 720p real-viewport run passed all seven stages. Inspection revealed
  that switching homes retained the old scroll offset; the final build resets
  scrolling for a different residence so its recorded period is visible first.
- Fingerprint 18 updates the foundation checksum to 1CA93F3AE33C8CB1 from the
  fingerprint-17 value E39DB36B589D8616. The fixture has no population or economy;
  this is the explicit versioned hashing contract change.

No new raster asset, native master dimension or prompt set applies: existing
approved portrait/product artwork is reused. Native text and layout are the
production outputs. The verification is scoped to this feature; no full Shipping
cook/package or complete MVP playthrough was performed.

- Final diagnostics: 5 passed, including the fingerprint-18 golden:
  Saved/BuildArtifacts/20260911-085152985-automation-Hansa.Simulation.Diagnostics.
- Market product-row regression: 1 passed:
  Saved/BuildArtifacts/20260911-085156396-automation-Hansa.UI.MarketInspector.DemandAndSelection.
- Final 1920x1080 real-viewport capture: all seven stages passed:
  Saved/BuildArtifacts/20260911-085154875-residence-inspector-1920-1080.
  Native pixel inspection confirms the period, every product's quantities and
  percentages, services label and controls fit the default 320x600 panel.
- Final 1280x720 real-viewport capture: all seven stages passed:
  Saved/BuildArtifacts/20260911-085319156-residence-inspector-1280-720.
  The available-height clamp requires scrolling. The full-window period is
  visible at the top; keyboard/controller focus reveals the last product and
  its quantity caption in the large-text/high-contrast state.

PNG and correlated semantic TSV evidence is retained in Saved/ResidenceInspector
with names residence-<width>x<height>-<stage>. Stages: initial, evaluated, details,
accessible, hover, occupied and 30-days. These are actual viewport captures,
not generated references or shipping textures. Tool inspection uses native
panel crops only, with no resampling.


Final verification totals: 27 targeted non-rendering tests passed. All seven
real-viewport stages passed at each of 1280x720, 1920x1080, 2560x1440 and
3440x1440. Additional final capture logs:

- Saved/BuildArtifacts/20260911-085434639-residence-inspector-2560-1440.
- Saved/BuildArtifacts/20260911-085532257-residence-inspector-3440-1440.

The 1080p full-window panel was visually inspected at native pixel size. Other
resolution checks include automated viewport capture and correlated semantic
bounds/state assertions; they do not imply a full manual gameplay review.
