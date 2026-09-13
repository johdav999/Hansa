# Residence migration diagnostics — 2026-09-12

## Root cause and player rule

A completed laborer residence is housing capacity, not an immediate workforce grant. An empty residence attracts one resident after each authored 60-tick evaluation interval while its weighted need satisfaction remains at least 80%. Product needs require a physical road path to a completed Market and available Market stock; basic services alone account for only 20% of laborer satisfaction.

The compact inspector previously showed `Empty house` and rolling historical consumption. An empty house correctly has no historical demand, so bread, fish, and beer displayed dashes even when their current access prevented migration. Basic services remained visibly 100%. This made a disconnected or undersupplied house look healthy and hid why the farm stayed at `0 / 8` laborers.

The authored laborer tier supplies 60% workforce. A full 12-resident laborer house therefore supplies seven workers after deterministic integer rounding, while a Grain Farm requires eight. The minimum staffing for one farm is fourteen laborer residents across at least two houses; for example, twelve residents in one house and two in another supply seven plus one workers.

## Native component inventory

- Existing compact residence shell, portrait, navy identity header, linen body and close control: reused unchanged.
- Occupancy value and bar: existing native Slate text and progress bar, unchanged.
- State/recorded-period area: existing native text now includes an always-visible migration status for empty homes.
- Need rows: existing approved ImageGen icons plus native labels, rolling quantities, bars and tooltips, unchanged. Empty product rows remain historical `No demand`, as required by the residence rolling-consumption contract.
- Causal Details card: existing native text/action surface now distinguishes missing physical Market access from low individual need satisfaction.
- Semantic status node: existing automation/accessibility node now exposes the visible status and stable cause code alongside the rolling-period fields.

No new raster, icon, material, layout family, palette, typography or spacing token was introduced. The approved residence inspector remains the style anchor.

## States and interaction

- Empty and eligible: `Empty house · accepting residents`.
- Empty and disconnected: `Empty house · No Market access`, with a completed-Market/shared-completed-road remedy under Details.
- Empty and undersupplied: the existing weakest-need cause is shown in the state area and expanded under Details.
- Occupied: the existing occupancy and need state remains unchanged.
- Pending history and zero historical demand: remain explicit and never invent prospective consumption percentages.
- Hover, controller/keyboard focus, Details collapsed/expanded, large text, high contrast, warning and critical semantics reuse the existing component behavior.

## Verification

- `Hansa.UI.ResidenceInspector`: two tests pass, including the no-Market cause/remedy and always-visible semantic status.
- `Hansa.Integration.RuntimeSimulationHost.EmptyNewGameResidenceAttractsLaborers`: a normal New Game places a connected road, residence, Market and Grain Farm through the command gateway; prospective needs pass and at least two residents arrive after exactly two migration intervals.

This is a native causal-feedback repair. It changes no economic definition, simulation formula, save schema, generated asset or provider workflow.
