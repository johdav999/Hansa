# Residence inspector

Current data contract: see [ResidenceRollingFulfillment.md](ResidenceRollingFulfillment.md).
Products now show this residence's rolling 30-day consumed / required quantities
and percentage. Services retain an explicitly labeled current metric. Native
quantity captions, a recorded-period label and a 320x600 bounded/scrollable host
extend the existing approved component family. Save format 5 and fingerprint 18
preserve the new residence histories; old saves start gathering new history.

The following record describes the original visual implementation and its
original acceptance evidence, before the rolling-consumption revision.


Component inventory: compact right inspector shell (native linen/navy/brass); original citizen portrait in an arched frame (ImageGen source, scalable ink reconstruction); native identity/close header; occupancy count and bar; reusable need row with good/service icon, name, fulfillment percent and bar; explanatory tooltip; expandable cause and existing residence actions. No tabs, tables, charts beyond bars, or unrelated decoration.

States: live/default, hovered explanatory tooltip, keyboard/controller focus outline, pressed controls, disabled upgrade with cause, empty house, pending need evaluation (dash, not invented zero), partial/fulfilled/shortage with numeric values, unavailable residence, loading/error fallback. Existing style tokens and fonts, 8px spacing, 16px body margins and 48px actions. Layout target 320x520 Slate units at 1080p with bounded height and scroll; vector content at other scales, never resampled rasters.

Data: authoritative cohort Residents/ResidenceCapacity and individual SatisfactionBasisPoints already exist. Show fulfillment separately from aggregate satisfaction. Tooltips expose access, affordability and reliability. Definition-only needs before first tick remain explicitly pending. No simulation/editor/save schema additions required.


## Data and behavior

The existing `FHansaPopulationCohortProjection` supplies `Residents`,
`ResidenceCapacity`, `WorkforceSupply` and `Needs`. The panel copies each need's
`SatisfactionBasisPoints` to the displayed fulfillment percentage; it does not
substitute city-average satisfaction or inventory stock. Access, affordability
and reliability remain separate tooltip values. Before evaluation, authored
need IDs are retained with an explicit unknown/pending state. Empty houses show
zero residents and their actual capacity. Under-construction buildings retain
the existing construction inspector until a residence cohort is available.

This is a presentation change: no authored population schema, save version,
editor validator or economic behavior changes. Existing upgrade and other actions
continue through the normal command gateway under Details.

## Assets and generation

Built-in ImageGen created the composed reference, citizen portrait source,
occupancy reference and need-row/tooltip reference. The panel reference is
1024x1536; the other masters are 1254x1254. The final panel revision moved the
portrait above the title as requested. Its example Firewood/Market rows are
illustrative only; runtime rows use actual tier definitions.

- Reference images and exact sibling prompts: `Docs/Images/UI/Residence/`.
- Citizen source and prompt: `SourceArt/UI/Residence/residence--citizen--default--1254x1254--v1.png` and `.prompt.md`.
- Runtime scalable portrait: `Content/Hansa/UI/Residence/citizen.svg`.
- Reproducible vector conversion: `Scripts/TraceResidencePortrait.py`.
- Source checksum and conversion provenance: `SourceArt/UI/Residence/provenance.json`.

Original-resolution inspection accepted the portrait silhouette, margins and ink
contrast, and reference hierarchy, palette and labels. Source PNGs are never
resampled or shipped as interactive UI. The portrait uses transparent native
vector paths derived from the inspected ink source; all labels, bars, glyphs and
controls are native Slate. The portrait is shared between population tiers.


## Verification — 2026-09-10

Development and DebugGame editor builds pass. The residence automation test
passes authoritative occupancy, individual need fulfillment, partial fulfillment,
empty/pending needs, detail toggling and keyboard focus on need rows. Existing
inspector regression suite also passes (6 tests).

Final native game viewport captures pass at 1280x720, 1920x1080, 2560x1440 and
3440x1440. Each run checks live resident/capacity/need values against the simulation
and panel bounds, and captures initial, evaluated, details, accessibility and
hover states. Native screenshots and semantic bounds are archived in
`Docs/Images/UI/Residence/Native/` with SHA-256 checksums. Original-resolution
inspection of the final 1080p evaluated and 720p large-text/high-contrast views
confirms readable hierarchy and bounded scrolling. Need tooltip was also
inspected at native resolution. Need rows are keyboard/controller focusable.

Final runs:
- Development: `20260910-155259202-build-HansaEditor-Win64-Development`
- DebugGame: `20260910-155808802-build-HansaEditor-Win64-DebugGame`
- Residence unit: `20260910-155736216-automation-Hansa.UI.ResidenceInspector`
- 720p: `20260910-155754521-residence-inspector-1280-720`
- 1080p: `20260910-155819405-residence-inspector-1920-1080`
- 1440p: `20260910-155844783-residence-inspector-2560-1440`
- Ultrawide: `20260910-155911141-residence-inspector-3440-1440`

Limitations: one shared citizen portrait currently represents all residence
tiers. Packaging is configured for the runtime SVG, but a full Shipping cook
was not run for this presentation change. Arbitrary UI scale combinations beyond
the captured preferences/resolutions were not exhaustively tested.


## Selectable starter home — 2026-09-10

The default new Lübeck game now adds completed laborer residence 13 at grid
(15,16), beside the starting road near the mill. It has a real residence cohort
with 0 initial residents and capacity 12; it adds no initial workforce. The empty
construction fixture remains empty, and existing saves are not changed.
The native residence-inspector test now selects this exact actor and confirms
its cohort counts/needs match the simulation. Native capture passes:
20260910-170340256-residence-inspector-1920-1080. Development and DebugGame
builds pass (20260910-170335350 and 20260910-170413247).


## Empty-house values clarified — 2026-09-10

Verified that the GUI copies per-cohort residents, capacity and each evaluated
need's SatisfactionBasisPoints from the authoritative population projection.
For goods, population evaluation uses city stock and actual required/consumed
quantities. Empty cohorts consume zero and evaluate supply for one prospective
resident. Their 100% values therefore mean prospective supply, not actual citizen
consumption. The empty-house heading now says "Supply for new residents" and
hover text explicitly explains the estimate and zero consumption. Occupied houses
retain "Citizen needs" and fulfillment tooltips. No percentages are fabricated
or overwritten by UI logic.

Model limits: affordability uses cohort purchasing-power basis points, and basic
services use cohort service-access/reliability inputs. These are simulation inputs,
not a new per-building spatial service calculation implemented by this panel.
