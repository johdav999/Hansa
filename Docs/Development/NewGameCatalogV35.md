# New Game catalog v35 repair

## Root cause

The accepted runtime pin remained at catalog v34 (205 authored definitions, registry
`D18AC831ED9C7710`) after the trade-presence specialization work had legitimately
promoted three capability assets and updated the six presence stages and four city
policies that reference them. Disk discovery therefore produced 208 definitions and
registry `2A9D09E1C63AA6E1`. The strict production initializer rejected the catalog
before creating a simulation state, which made the New Game action fail.

The guard was correct and was not weakened. Catalog v35 records the complete reviewed
208-definition snapshot in `Tests/Golden/economic_catalog_v35.json`, advances the
runtime count/hash together, and names v34 as the compatible predecessor through
`Hansa.Content.34To35.AddPresenceSpecializations`. Existing v33 and v32 migration
paths remain explicit.

## Reviewed delta

Added definitions:

- `PresenceCapability.HarborSpecialization`
- `PresenceCapability.MarketSpecialization`
- `PresenceCapability.WarehouseSpecialization`

Changed definitions are limited to the six presence stages and the Hamburg, Lübeck,
Lüneburg, and Rostock city trade policies. No v34 definition is removed. The
authoritative reload test compares this exact delta and reports future fingerprint
drift per definition.

## Acceptance

The blocking flow is: production profile initializes; New Game creates an empty Lübeck
session; the session advances one tick without recreating starter buildings; repeating
New Game yields the same fingerprint. The focused frontend/runtime automation test is
the regression gate. The catalog reload test proves the 208 disk assets, v35 golden,
reverse-order determinism, exact reviewed delta, and predecessor identity.

Verified evidence:

- Editor build: `Saved/BuildArtifacts/20260922-132504968-build-HansaEditor-Win64-Development`
- Catalog reload and v34 lineage: `Saved/BuildArtifacts/20260922-132524475-automation-Hansa.Integration.Authoring.EconomicAssetReload`
- Headless production New Game/reset/save flow: `Saved/BuildArtifacts/20260922-132553795-automation-Hansa.Integration.Save.FrontendNewGameAndRejectedRestore`
- Native 1280 × 720 client journey (25 frontend stages): `Saved/BuildArtifacts/20260922-133036823-gui-repair-1280-720`

The older `Hansa.UI.RuntimeScenario.PlayableShortageProjection` test initializes v35
successfully but retains stale pre-regional-catalog quantity assertions (for example,
14 goods versus the current 27). That independent expectation drift is not a New Game
initialization failure and is left outside this repair.
