# Scenario objectives and victory resolution

S10-P03 adds one polished runtime scenario, `Scenario.LubeckGrainShortageV1`. The scenario is authored through generic primary data assets and compiled into `FHansaEconomicRegistry`; gameplay does not depend on editor classes.

## Authored endings

Every path must remain satisfied for five authoritative simulation ticks.

| Ending | Priority | Explicit objectives |
|---|---:|---|
| `Victory.ProsperityEconomic` | 10 | House money at least 90,000 pfennig; bread stock at least 24,000 milli-units; bread price at most 1,200 milli-marks; four player-owned productions active |
| `Victory.TradeNetwork` | 20 | Grain stock at least 20,000 milli-units; one active sea route; one active land route; at least two completed player trade legs |
| `Victory.ResearchCivic` | 30 | At least two completed technologies; Lübeck population at least 12; house money at least 90,000 pfennig |

Defeat requires twenty consecutive ticks at or below -5,000 pfennig with neither an active player route nor an active player production. Failure resolves first on an exact-tick victory tie. Simultaneous victories resolve by lowest authored priority, then by stable ID as a defensive deterministic fallback. Generic editor validation rejects equal enabled priorities, missing scenario/victory/objective/city/good references, duplicate endings, and impossible satisfaction thresholds.

## Runtime and UI contract

`FHansaScenarioEvaluator` observes only `FHansaSimulationReadOnlyAccess`. It cannot mutate gameplay and is evaluated after each authoritative runtime tick. The root HUD receives an event-refreshed `UHansaScenarioPresentationModel` and assembles `SHansaScenarioScreen` from native Slate components. The screen supports briefing, active progress, victory, failure, path selection, keyboard/controller focus, and semantic automation. Generated images in `Docs/Images/UI/Scenario` are reference-only; no full-screen raster is shipped.

The existing Objectives alert opens the scenario dossier. Outcome states force the dossier open, while closing restores the originating semantic focus target.

## Authored asset workflow

The normal `HansaEconomicDefinitionSeed` command creates missing assets only. To update the reviewed S10-P03 fields on existing scenario assets, run:

```powershell
UnrealEditor-Cmd.exe Hansa.uproject -run=HansaEconomicDefinitionSeed -MigrateScenarioS10P03 -unattended -nop4
```

The migration loads each exact stable-ID asset, copies only scenario/objective/victory fields from the reviewed seed set, refreshes its content hash, and saves exactly fifteen assets. The final 72-definition catalog hash is `D9476BB08CC54654`.

## Verification

- `Hansa.Simulation.Scenario`: all three authored victory paths plus failure/tie/ordering rules.
- `Hansa.UI.Scenario`: briefing, progress, success, failure, semantic state, and controller focus.
- `Hansa.Content.Definitions.ScenarioValidation`: impossible objectives, missing references, and ambiguous endings.
- `Hansa.Integration.Authoring.EconomicAssetReload`: all 72 assets reload and match the pinned registry hash.
- `Hansa.UI.HUD` and `Hansa.UI.RuntimeScenario.PlayableShortageProjection`: adjacent HUD and playable-scenario regressions.
