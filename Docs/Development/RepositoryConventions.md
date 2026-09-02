# Hansa — Repository Conventions

## 1. Purpose

This document is the canonical directory, naming, configuration, fixture, evidence, and generated-content contract for Hansa. Later feature prompts extend these locations rather than inventing parallel ones. Architectural ownership remains governed by the accepted [ADRs](../Architecture/Decisions/README.md).

Run the executable policy gate after changing repository structure, configuration, tests, fixtures, or content paths:

```powershell
pwsh -File Scripts/VerifyRepositoryConventions.ps1
```

The complete local/CI sequence runs this gate automatically.

## 2. C++ ownership, folders, and namespaces

| Concern | Owning location | Rule |
| --- | --- | --- |
| Deterministic domain primitives and systems | `Source/HansaSimulation/` | May depend only on `Core` where practical; no Actors, UI, editor, automation, or provider code |
| Unreal gameplay and presentation integration | `Source/Hansa/` | Depends inward on `HansaSimulation`; owns Framework, Assets, Input, Network, Presentation, Subsystems, UI, and World integration |
| Authoring and validation | `Source/HansaEditor/` | Editor-only; owns Studio, Schema, Validators, Migrations, GraphEditors, Import, and Generation clients |
| Development automation | `Source/HansaAutomation/` | DeveloperTool-only; owns Protocol, SemanticUI, Endpoint, Fixtures, GameplayQueries, Screenshots, and Waits |
| Tests and C++ fixture builders | `Source/HansaTests/Private/` | DeveloperTool-only; organize under `Tests`, `Simulation`, `Integration`, `UI`, `Multiplayer`, `EndToEnd`, and `Fixtures` as those layers appear |
| External development processes | `Tools/HansaMcp/`, `Tools/HansaGenerationWorker/` | No Unreal runtime dependency and no Shipping inclusion |

Each Unreal module keeps externally consumed headers in `Public/<Feature>/` and implementation-only headers/source in `Private/<Feature>/`. Do not make a header public only for convenience. New features use a vertical feature folder consistently across their owning modules—for example `Definitions/`, `Commands/`, or `Market/`—rather than a generic `Helpers/`, `Common/`, or `Managers/` dumping ground.

Non-reflected helpers use nested namespaces rooted at `Hansa`, such as `Hansa::Simulation`, `Hansa::Game`, `Hansa::Editor`, `Hansa::Automation`, and `Hansa::Tests`. Unreal-reflected `U`, `A`, `F`, and `E` types retain normal globally visible Unreal naming because UHT namespace support must not be assumed. File names match their primary type, and includes use module-root-relative paths.

## 3. Content and source-art paths

All project-owned Unreal content lives below `/Game/Hansa` (`Content/Hansa/`):

```text
Content/Hansa/
├── Core/
│   ├── Goods/
│   ├── Recipes/
│   ├── Buildings/
│   ├── Needs/
│   ├── Technologies/
│   └── Scenarios/
├── World/
│   ├── Europe/
│   ├── Cities/
│   ├── Buildings/
│   ├── Vehicles/
│   └── DataLayers/
├── UI/
├── Audio/
├── VFX/
├── Developer/
│   ├── Fixtures/
│   └── GenerationPreview/
└── Generated/
    └── Staging/
```

Rules:

- Production assets live in the appropriate `Core`, `World`, `UI`, `Audio`, or `VFX` feature folder.
- `Content/Hansa/Developer/` may contain reviewed test maps, QA fixture assets, and preview helpers. It may be tracked but is always excluded from cook.
- `Content/Hansa/Generated/Staging/` contains transient Unreal imports awaiting review. It is ignored by Git and excluded from cook. Nothing in production may reference it.
- Selected immutable generation sources and provenance are promoted to `SourceArt/Generated/<Domain>/<JobId>/`; resumable transient job state stays in ignored `Saved/GenerationJobs/<JobId>/`.
- Production assets must never reference `/Game/Hansa/Developer`, `/Game/Hansa/Generated/Staging`, transient job paths, external service URLs, or development-only classes.
- Project-owned production content does not live directly under `Content/` or beside engine/plugin content.

Use normal Unreal prefixes (`DA_`, `DT_`, `BP_`, `WBP_`, `T_`, `M_`, `MI_`, `SM_`, `SK_`, `S_`, `NS_`) followed by the Hansa feature and purpose. UI-specific production naming remains governed by [UIAssetWorkflow.md](../UIAssetWorkflow.md).

Raster references and source media use Git LFS. Generated UI masters and prompts retain the exact paths and native-size naming rules in `AGENTS.md`.

## 4. Stable identity namespaces

Canonical definition IDs follow [ADR-0002](../Architecture/Decisions/0002-stable-identifiers.md): ASCII `Domain.Name[.Variant]`, with singular PascalCase segments. Initial registered domain prefixes are:

```text
Good Recipe Building Need PopulationTier Vehicle City Region
Technology Event Victory Scenario
```

Examples: `Good.Grain`, `Recipe.Bread`, `Building.Bakery.Small`, `City.Lubeck`. IDs are explicit, immutable after publication, globally unique, and never derived from package paths, localized labels, provider identifiers, or filenames. Adding a new domain prefix requires updating the central registry introduced with the definition system and its validation tests.

Other identity spaces remain distinct:

- deterministic fixture IDs use lowercase snake case plus a version suffix: `lubeck_grain_shortage_v1`;
- semantic UI IDs use namespaced PascalCase paths: `Market.GoodsTable.Row[Good=Grain].LocalPrice`;
- localization keys use `UI.<Feature>.<Purpose>` or `Game.<Feature>.<Purpose>`;
- protocol correlation/job IDs are transport identity and never gameplay identity.

## 5. Configuration sections and secrets

Checked-in configuration contains safe, machine-independent defaults only:

- `[Hansa.Project]` owns repository/schema convention values;
- `[Hansa.Automation]` owns development automation defaults and must keep transport disabled by default;
- future plain project sections use `[Hansa.<Area>]`;
- reflected settings use Unreal's generated `[/Script/<Module>.<SettingsClass>]` section;
- local overrides belong in ignored generated/local configuration, command-line arguments, environment variables, or the OS credential store as appropriate.

Do not store API keys, passwords, bearer/access tokens, signed URLs, private keys, provider endpoints, or machine-specific install paths in `.ini`, assets, source, fixture descriptors, manifests, or logs. External generation configuration and credentials belong to the future worker boundary, never runtime modules or Shipping content. Example values in documentation must be visibly nonfunctional placeholders.

## 6. Logging

Log categories use `LogHansa<Domain>` and are declared in the module or narrow public feature header that owns them. Existing foundation examples are `LogHansa`, `LogHansaSimulation`, `LogHansaEditor`, and `LogHansaAutomation`. Add domain categories—such as `LogHansaMarket`, `LogHansaLogistics`, `LogHansaNetwork`, `LogHansaAI`, `LogHansaSave`, or `LogHansaContent`—only when the domain implementation lands.

Logs contain stable IDs, tick/correlation IDs, structured reason codes, and bounded diagnostics. They never contain credentials, authorization material, raw private inputs, signed URLs, or unbounded payloads. Routine state is `Verbose`; actionable lifecycle results are `Log`/`Display`; recoverable invalid external input is `Warning`; broken invariants are `Error` or checked assertions as appropriate.

## 7. Tests and fixtures

Automation test names use:

```text
Hansa.<Layer>.<Feature>.<Behavior>
```

Allowed top-level layers are `Architecture`, `Simulation`, `Integration`, `Content`, `UI`, `Multiplayer`, and `EndToEnd`. Names are stable API for filters and CI; do not encode sprint IDs, developer names, dates, or transient bug numbers.

Fixture ownership is split deliberately:

| Artifact | Location |
| --- | --- |
| C++ fixture builders | `Source/HansaTests/Private/Fixtures/` |
| Reviewed descriptor/data fixtures | `Tests/Fixtures/<fixture_id>/` or a single focused JSON file while small |
| Golden saves/protocol payloads | `Tests/Golden/<Area>/` |
| Unreal fixture maps/assets | `Content/Hansa/Developer/Fixtures/` |
| Generated run evidence | `Saved/TestEvidence/<run-id>/` |

Every runnable deterministic fixture records its stable fixture ID, schema version, owner, seed, definition/content hash, initial tick, expected checkpoints, and final checksum when applicable. The checked-in `Tests/Fixtures/fixture.example.json` is documentation-only and must be copied to a stable fixture-specific path before becoming runnable.

## 8. Evidence and transient output

Generated evidence is never tracked:

| Output | Location |
| --- | --- |
| Build/test/policy command artifacts | `Saved/BuildArtifacts/<timestamp>-<operation>/` |
| Feature and deterministic evidence bundles | `Saved/TestEvidence/<test-or-fixture>/<run-id>/` |
| Automation session logs/screenshots | `Saved/Automation/<session-id>/` |
| External generation job state | `Saved/GenerationJobs/<job-id>/` |

An evidence bundle records the operation/test, build/version, fixture/seed where applicable, commands and structured results, timestamps, relevant hashes, and paths to normal Unreal logs. Native screenshots remain at their requested pixel dimensions and are never resized to satisfy another target.

## 9. Git and enforcement

`Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, IDE workspaces, generated solutions, and generated staging remain ignored. Unreal packages, source art, audio/video, and raster images use Git LFS. Do not force-add ignored output.

`Scripts/VerifyRepositoryConventions.ps1` fails for:

- high-confidence credential material in repository text;
- provider configuration inside runtime source, project config, or the project descriptor;
- production references to Developer/generated staging paths;
- a nonempty Android file-server security token;
- missing cook exclusions or an enabled-by-default automation transport;
- tracked generated/staging output or missing ignore rules;
- missing LFS routing for representative binary/media assets;
- nonconforming Unreal automation test names or fixture template identity.

The guard complements, but does not replace, Unreal Data Validation, asset referencer checks, or the packaged Shipping inspection required later. Binary production assets receive a full referencer/cook audit when real content and packaging land.
