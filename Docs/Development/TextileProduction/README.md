# Textile production review candidate

Status: promoted with explicit user approval on 2026-09-19. Accepted catalog v30: `3472D1CD822731FA` (181 definitions). See [promotion record](PROMOTION.md) for production paths, verification and save compatibility. The following balance and original review evidence describe the preserved V2 snapshot (`386F8F6145FE5135`, 136 definitions).

This feature adds the Craftsmen production chains for linen clothing, candles and rope. `Flax` and `Hemp` are gameplay abstractions for prepared textile fibres that are already retted, broken, scutched and hackled as appropriate, and are suitable for delivery to a Weaver or Ropewalk. Flax, hemp and beeswax initially enter the economy through the existing market/trade model. No farms, retting works or apiaries are added.

## Authored balance

Quantities are ordinary displayed good units; the authoritative definitions store milliunits. Times are simulation ticks. All values remain editable in `HansaTextileProductionDraft.cpp` and are surfaced through the existing editor reflection and property-diff pipeline.

| Workshop | Stable recipe | Input per batch | Output per batch | Time | Craftsmen | Nominal output / 1,000 ticks |
|---|---|---:|---:|---:|---:|---:|
| Weaver | `Recipe.WeaveLinen` | 2 flax | 1 linen cloth | 120 | 3 | 8.33 cloth |
| Tailor | `Recipe.SewLinenClothing` | 1.5 linen cloth | 1 linen clothing | 100 | 2 | 10 clothing |
| Chandler | `Recipe.DipCandles` | 2 beeswax | 1 candles | 120 | 2 | 8.33 candles |
| Ropewalk | `Recipe.LayHempRope` | 2 hemp | 1 rope | 140 | 3 | 7.14 rope |
| Ropewalk | `Recipe.LayFlaxRope` | 2.5 flax | 1 rope | 160 | 3 | 6.25 rope |

The linen chain is input-balanced at approximately 5.56 clothing per 1,000 ticks per Weaver when downstream Tailor capacity is available. Chandler wick cord is an abstracted workshop supply: the current commodity model has no general-purpose thread good, and adding one solely for candle wicks would add UI and logistics noise without an independent gameplay role. This decision is recorded in the Chandler purpose text.

Craftsmen consume linen clothing and candles at one milliunit per resident per tick, with importance values 750 and 500 basis points respectively. Rope is an industrial/trade good with external market demand. The current catalog exposes no supported shipbuilding or maintenance production recipe to extend, so rope is not attached to an invented subsystem.

## Construction and storage

| Building | Stable ID | Tier/category | Footprint | Money | Materials | Build time | Storage |
|---|---|---|---:|---:|---|---:|---:|
| Weaver | `Building.Weaver` | Craftsmen / Production | 3×3 | 2,400 pf | 7 planks, 4 timber, 0.5 tools | 120 | 80 |
| Tailor | `Building.Tailor` | Craftsmen / Production | 3×2 | 2,000 pf | 5 planks, 3 timber, 0.5 tools | 120 | 80 |
| Chandler | `Building.Chandler` | Craftsmen / Production | 3×2 | 2,100 pf | 5 planks, 3.5 timber, 0.5 tools | 120 | 80 |
| Ropewalk | `Building.Ropewalk` | Craftsmen / Production | 8×2 | 3,200 pf | 10 planks, 6 timber, 1 tool | 160 | 120 |

All four cards have explicit `Craftsmen` construction ownership. Ingredient tier and helper workforce are never used to infer browsing ownership. The tray exposes three end-product selectors: Linen clothing expands to Weaver then Tailor; Candles expands to Chandler; Rope expands to Ropewalk. The Ropewalk card formats hemp and flax as alternatives.

## UI assets and states

Component inventory: native construction shell, category and tier navigation, native end-product chain selectors, four illustrated card images, seven individual good icons, native cost/workforce/footprint/flow rows, native inspector recipe actions, progress, shortage/warning text and shared focus treatment. Dynamic text, quantities, prices, status and prompts remain native Slate.

Required states reuse `SHansaAction` and the existing navy/linen/brass system: default, hover, pressed, selected, disabled, keyboard/controller focus, loading/unavailable, warning and error. Selected and active recipes are shown separately while a batch-boundary switch is pending. Missing inputs are reported per selected recipe; hemp and flax are never shown as simultaneous requirements.

ImageGen source masters and sibling prompt records are in `SourceArt/UI/TextileProduction/`. All eleven masters are 1,254×1,254 RGBA images generated with built-in ImageGen. Documented 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112 and 160 pixel display variants are in `Content/Hansa/UI/TextileProduction/`; they use transparent-margin crop, square padding and premultiplied-alpha proportional resampling under the approved GUI exception. The source/variant hashes and exact crop records are in `display-variants.json`. The review sheet is `Docs/Images/UI/TextileProduction/display-size-review.png`.

## Models

Editable source: `SourceArt/Generated/Buildings/TextileProductionV1/exports/HansaTextileWorkshops.blend`. Per-workshop FBX and GLB exports, 1,024×1,024 PBR maps, export manifest, clean Blender reimport checks and Unreal evidence are in the same asset family.

Observed evidence supports timber/brick/stone workshop construction, clay-tile roof language, a long narrow rope walk, open drying/working bays, twisting heads, tensioning trolleys, rails/rakes and fibre handling. The Weaver's street-facing loom bay, Tailor's window-lit bench and sign, and Chandler's hearth/dipping-rack arrangement are historically plausible inferred layouts rather than direct reconstructions of a surviving Lübeck interior. The Ropewalk deliberately adapts much later surviving machinery evidence to a hand-powered late-medieval visual language and omits steam engines, line shafts and modern machinery.

| Mesh | Imported bounds | LOD0 / LOD1 / LOD2 triangles | Collision |
|---|---|---:|---|
| `SM_Weaver` | 11.11×6.84×9.37 m | 65,120 / 32,560 / 13,024 | four convex hulls, max 16 vertices |
| `SM_Tailor` | 8.31×6.52×9.09 m | 54,176 / 27,088 / 10,836 | four convex hulls, max 16 vertices |
| `SM_Chandler` | 9.66×6.52×9.62 m | 55,380 / 27,690 / 11,076 | four convex hulls, max 16 vertices |
| `SM_Ropewalk` | 31.94×7.20×4.38 m | 6,860 / 3,430 / 1,372 | four convex hulls, max 16 vertices |

The authoritative export origin is a ground-plane pivot at `(0,0,0)`, metres are converted to Unreal centimetres, and the OpenGL +Y normal maps have their green channel flipped on Unreal import. Staged assets live under `/Game/Hansa/Generated/Staging/TextileProductionModelsV1`; the approved copies now live under `/Game/Mesh/hansa-textile-production/`.

## Persistence, parity and validation

The Ropewalk reuses the existing authoritative `SetProductionMode` command and `RequestedRecipeId` save field. Recipe changes occur at batch boundaries. The save format already carries this field, so no format migration is required; old saves retain their accepted catalog pin and do not silently acquire candidate content. The development-only `-TextileProductionCandidate` loader preserves the accepted v28 catalog and registry-hash checks.

The authored definition set, reflected metadata, deterministic compiler validation, per-property impact report, stable localized identities, disk-reload verification and model binding are handled by `HansaTextileProductionCommandlet`. `-ReviseV1 -Stage -BindModels -TextileProductionCandidate` creates V2 from the preserved V1 snapshot; `-VerifyStage -TextileProductionCandidate` reloads it from disk and writes `candidate.json` and `reloaded.json`. Promotion remains a separate explicit decision.

Automated coverage is in `HansaTextileProductionTests.cpp`: exact batch consumption/output, missing-input and workforce blockers, output storage, market sourcing, household needs, construction ownership, chain selectors, Ropewalk alternative wording, recipe switching and save/load persistence. The actual-player UAT session and screenshots are tracked in `UAT/SESSION.md`.

## Review and release gates

The V2 candidate is staged with verified workshop bindings, freshly reloaded and pinned. Five mechanics tests and four final rendered viewport runs pass. The detailed issue ledger, exact evidence locations and distinction between rendered controls and simulated economic assertions are in `UAT/SESSION.md`.

Promotion approval was granted and applied; see `PROMOTION.md`. Remaining release gates: hardware-controller and manual staffed-economy playthrough; clean cooked-package media/reference audit; existing road spline-material warning; broader soak monitoring of one earlier unexpected process exit. Shipping binary/receipt checks do not replace a cooked asset audit. The reviewed assets and definitions are promoted; original staging assets and historical fixtures remain preserved.

## V2 continuation and review command

Run `Scripts/PreviewTextileProduction.ps1` to build and open the accepted catalog review on the surveyed Lübeck map. Add `-Verify` for the scripted placement/inspector review; `-SkipBuild` reuses the current linked build. Example compact check: `-Verify -SkipBuild -Width 1280 -Height 720 -UiScale 1.4 -LargeText`.

V2 preserves the original staging snapshot and accepted catalog. Candidate discovery reads its own staging root instead of enumerating newer Core definitions and looking for nonexistent equivalents. The exact definition count and registry hash are still enforced. Tailor and Chandler occupy 3×2 four-metre cells so their imported 8.31 m and 9.66 m widths fit without scaling. Binding now rejects a mesh exceeding its authored footprint. Workshop-purpose texts receive stable namespace/key identities. `candidate-v1.json` and `reloaded-v1.json` preserve the previous reports.

Recipe actions display authored localized names, preserve their native widget identity during refresh, and retain separate selected and active recipes. The compact tray has native vertical scrolling with focus reveal; compact inspectors use the available vertical space. Reused ImageGen icons/illustrations and model sources are unchanged. No additional generation or production promotion occurred in this continuation.

Changed implementation files in this continuation:
- `Source/Hansa/Private/World/HansaLubeckScenarioInitializer.cpp` and its public header: isolated candidate discovery and V2 pin.
- `Source/Hansa/Private/Definitions/HansaDefinitionBase.cpp`: resolve V2 review definitions.
- `Source/Hansa/Hansa.Build.cs`: non-Shipping candidate discovery dependency and review PNG staging.
- `Source/Hansa/Private/UI/HansaInspectorPresentationModel.cpp`: authored recipe labels.
- `Source/Hansa/Private/UI/SHansaProductionInspector.cpp`: recipe widget identity retention.
- `Source/Hansa/Private/UI/SHansaBuildMenu.cpp` and `SHansaRootHud.cpp`: compact containment and inspector height.
- `Source/HansaEditor/Private/Definitions/HansaTextileProductionDraft.cpp` and `HansaTextileProductionCommandlet.cpp`: corrected footprints, stable purpose keys, isolated revision and footprint validation.
- `Source/HansaEditor/Private/Tests/HansaTextileProductionTests.cpp`: exact alternative wording independent of recipe order, physical delivery/disconnection, actual household consumption and pending-batch save/load.
- `Source/HansaTests/Private/UI/HansaTextileProductionCaptureTests.cpp`: actual selection, readable label, focus/identity, save/load and configurable viewport evidence.
- `Scripts/PreviewTextileProduction.ps1`: reproducible interactive/automated review.
- `Config/DefaultGame.ini`: required empty GameFeatureData scan rule, NeverCook, preventing commandlet startup errors.
- `Content/Hansa/Generated/Staging/TextileProductionV2/`: isolated revised definitions.
- `design.md`, `Docs/UIDesignBrief.md`, and this report: remove duplicate textile sections and record current verification.

The five mechanics tests cover exact inputs/outputs, workforce and capacity blockers, Craftsmen-only cards, imported stock, both recipe modes, selection before and during a batch, save/load, cloth physically in transit and disconnected-road rejection, and actual clothing/candle consumption. The rendered review places all four workshops through the normal placement presenter, checks their imported mesh binding, exercises native recipe actions and semantic focus, and round-trips the candidate save. It is not a complete manual economic playthrough: those captures start from an empty city without established workforce, so running production/household assertions are supplied by the deterministic integration tests. Hardware-controller play and a clean cooked package remain release gates.