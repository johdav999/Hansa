# Hansa Authoring Studio

`S02-P01` establishes the first schema-driven Hansa authoring path. It is deliberately small: one editor-only sample proves that reflection drives both the native editor surface and deterministic JSON Schema without a handwritten form.

`S03-P01` extends that same contract with production `UHansaGoodDefinition`, `UHansaRecipeDefinition`, and `UHansaBuildingDefinition` assets. The generic Details inspector remains authoritative for ordinary fields. The only specialized economic presentation added in this slice is a compiled-registry status/diagnostic row because cross-asset reference validity and the registry hash cannot be inferred from one object in isolation.

## Runtime definition contract

`UHansaDefinitionBase` is a runtime `UPrimaryDataAsset`. It owns stable definition ID, schema version, authored revision, localized display identity, category/tags/content set, deprecation replacement and a derived FNV-1a content hash. Runtime validation reports structured severity, code, property path, cause and remedy.

Concrete classes require class metadata `HansaSchemaId` and `HansaSchemaVersion`. Every reflected field requires:

- `DisplayName`, `ToolTip`, `Category`
- `HansaRequired`, `HansaReference`, `HansaBulkEditable`
- `HansaAIAccess`, `HansaMigration`, `HansaSerialization`, `HansaValidation`
- numeric fields additionally require `HansaUnit`, `HansaMin`, `HansaMax`

Missing metadata produces `HSA-SCHEMA-003` or `HSA-SCHEMA-004` and invalidates the class schema. The abstract negative-test definition demonstrates this fail-closed behavior without entering normal registry discovery.

## Registry and export

`FHansaEditorSchemaRegistry` lives in `HansaEditor` and therefore cannot enter Shipping runtime code. It discovers non-abstract `UHansaDefinitionBase` subclasses, walks inherited reflected properties, sorts classes and properties ordinally, validates metadata, and derives Draft 2020-12 JSON Schema.

The exporter writes stable `x-hansa-*` annotations for category, reference type, AI access, bulk editing, migration, serialization, validation and unit. Required fields and numeric bounds come from reviewed metadata. The Authoring Studio toolbar exports valid schemas to `Saved/SchemaExport/`; automation writes comparison evidence to `Saved/TestEvidence/authoring_schema_v1/`.

## Native Editor workflow

Open **Window → Hansa Authoring Studio**. The Editor-only nomad tab contains:

- toolbar actions for Validate, Export schema, Run shortage fixture, Undo and Redo;
- a searchable, virtualized definition browser populated from the Asset Registry plus the transient proof sample;
- the standard Property Editor Details view, so new approved reflected fields appear automatically;
- a validation summary and virtualized issue rows with severity text/icon, code, field, cause and remedy.

S04-P04 adds a compact Fixture preview in the validation column. **Run shortage fixture** executes `lubeck_grain_shortage_v1` headlessly, uses the normal command gateway for recovery production, and shows localized baseline, shortage and recovered tick/stock/price metrics plus explicit pass/fail text. It does not mutate definition assets or bypass inventory transactions. See [GrainShortageFixture.md](GrainShortageFixture.md).

Details changes use Unreal transactions. Validation refreshes after property changes, undo and redo. The transient `UHansaFoundationSampleDefinition` exists only to prove the foundation; it is not production gameplay content.

## Economic definition catalogue

Accepted MVP content is stored as Primary Data Assets below:

- `Content/Hansa/Core/Goods/`: 10 goods with explicit kilogram, item, or litre units, fixed-point base values, elasticity, spoilage policy, and localization keys;
- `Content/Hansa/Core/Recipes/`: 8 source/transformation recipes with stable good references, milli-unit quantities, deterministic cycle ticks, and workforce placeholders;
- `Content/Hansa/Core/Buildings/`: 14 required building types with costs, recipe and upgrade references, footprints, build times, storage/residence capacities, placement requirements, workforce placeholders, localization keys, and an explicit reviewed placeholder mesh.

`DefaultGame.ini` registers all three Primary Asset types and production directories for runtime discovery and cook. Values are authored in assets; JSON remains interchange/test data and is not a parallel source of truth.

`S03-P03` consumes the compiled recipe cycle, input/output and workforce fields plus each building's recipe/workforce relationship directly through the immutable economic registry. It adds no reflected authoring field or duplicate schema. Runtime causal projections expose the measured result; `S03-P04` adds the corresponding conservation, source/sink, reachability and fixture validation surfaces to Authoring Studio and commandlets.

`FHansaEconomicDefinitionCompiler` accepts a set of definition assets without modifying them, validates local contracts and cross-definition stable references, sorts all identities and semantic collections ordinally, and produces an immutable `HansaSimulation` registry. Registry queries are stable-ID based. The registry hash is FNV-1a over canonical class, stable-ID, and per-definition content hashes; localized display text contributes its culture-independent source identity rather than the active localized display string.

The explicit `HansaEconomicDefinitionSeed` Editor commandlet creates only missing reviewed MVP assets. Existing assets are skipped unless a deliberate replacement workflow is added; the current `-Replace` path fails closed rather than overwriting a possibly loaded designer asset. It is not a startup, build, or CI hook, so it cannot silently change subsequent designer edits.

Population needs and tiers use the same generic browser, reflected Details panel, transactions, validation list and registry compiler as goods, recipes and buildings. See [Population.md](Population.md) for the S04-P01 contracts.

S04-P02 adds four `UHansaCityMarketProfileDefinition` assets through that same generic path. Each profile owns per-good reserve, incoming-supply, modifiers and price bounds plus the shared report cadence/history policy. Cross-asset compilation rejects missing goods and the Studio status reports the compiled city-market count. See [Market.md](Market.md).

## Acceptance evidence

Run:

```powershell
pwsh -NoProfile -File Scripts\Build.ps1 -Target HansaEditor -Platform Win64 -Configuration Development
pwsh -NoProfile -File Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Architecture.Authoring -SkipBuild
```

The focused filter covers discovery, required-metadata rejection, deterministic golden schema and undo/redo. The reviewed fixture is `Tests/Golden/Editor/foundation_sample.schema.json`.

S03-P01 adds these focused filters:

```powershell
pwsh -NoProfile -File Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Content.Definitions -SkipBuild
pwsh -NoProfile -File Scripts\RunAutomationTests.ps1 -TestFilter Hansa.Integration.Authoring -SkipBuild
```

They cover catalogue size and stable-ID lookup, deterministic compilation under reordered discovery, non-mutating temporary compiles, invalid good/recipe/upgrade references, production asset reload, and undo/redo for every economic definition type. Invalid-reference cases are reviewed in `Tests/Fixtures/economic_invalid_references_v1.json`.

UE 5.8 headless startup can otherwise abort Win64 automation while validating unrelated LinuxArm64/VisionOS SDK descriptors. `RunAutomationTests.ps1` scopes the engine-supported `UE_SKIP_UBT_SDK_SETUP=1` environment variable to the Editor child process and restores the caller afterward; target compilation still performs normal Win64 toolchain validation.

## Visual references

The component/state specification and five built-in ImageGen references are under `Docs/Images/UI/AuthoringStudio/`. They are native-size design references only. The shipping interface is reconstructed from Slate/Property Editor controls; no generated raster is imported into `Content/`.
