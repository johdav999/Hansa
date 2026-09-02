# Hansa — Authoring Editor MVP Scope

This document is the editor/generation workstream of the integrated product MVP in [MVP.md](MVP.md). `MVP.md` controls the overall playable scope and cross-workstream release gate; this document supplies the detailed editor acceptance criteria.

## 1. MVP objective

Deliver the smallest usable Hansa Authoring Studio that proves the editor will evolve safely with the game and that AI-generated work can move through a controlled draft → validate → review → promote workflow.

The MVP is successful when a designer can:

1. create and edit the economic vertical-slice definitions inside Unreal Editor;
2. visualize and validate a small research dependency graph;
3. request a schema-constrained OpenAI proposal and selectively apply it through an undoable diff;
4. generate one static 3D prop through Tripo, import it into staging, validate it, and promote it;
5. generate one sound effect and one spoken line through ElevenLabs, review them, and promote them;
6. compile the accepted definitions into the same deterministic registry used by the game;
7. verify that none of the editor, worker, credentials, or staging content enters a Shipping build.

The MVP proves extensibility, safety, and the complete workflow. It is not intended to replace a full DCC, dialogue studio, balance suite, or production asset-management system.

## 2. Product slice

The MVP supports the four-city economic vertical slice defined in `Docs/GameConcept.md`:

- Lübeck, Hamburg, Lüneburg, and Rostock;
- 10 initial goods;
- initial recipes and production buildings;
- a small technology subset sufficient to test prerequisites and unlock effects;
- one deterministic Lübeck grain-shortage fixture;
- one static harbor prop generated for use in a preview/test scene;
- one ship-bell or market-confirmation SFX;
- one short merchant or dockworker voice line with matching subtitle text.

The editor does not need to author the entire final campaign before it is considered an MVP.

## 3. MVP architecture

```text
Unreal Editor
└── HansaEditor module
    ├── Authoring Studio shell
    ├── schema registry
    ├── generic data browser/details
    ├── research graph
    ├── validation and compiled preview
    ├── proposal/asset review
    └── generation job client
             │ versioned local protocol
             ▼
Tools/HansaGenerationWorker
    ├── persistent job store
    ├── OpenAI adapter
    ├── Tripo adapter
    ├── ElevenLabs adapter
    ├── Mock provider
    └── staging/provenance writer
```

`HansaEditor` is an Editor-only Unreal C++ module. `HansaGenerationWorker` is an external local development process. Runtime modules do not depend on either component.

## 4. In-scope definition types

### 4.1 Common definition contract

Implement `UHansaDefinitionBase` with:

- stable ID;
- schema version;
- authored revision;
- display/localization identity;
- category/tags;
- deprecation/replacement metadata;
- content hash;
- validation hook.

### 4.2 Editable types

| Definition | MVP editing support | Key validation |
| --- | --- | --- |
| `UHansaGoodDefinition` | Create, duplicate, search, edit, delete/redirect | Stable ID, value/range, unit, elasticity, spoilage, icon/reference |
| `UHansaRecipeDefinition` | Create, edit inputs/outputs, cycle time and workforce | Valid goods, positive output, quantities, declared sources/sinks |
| `UHansaBuildingDefinition` | Edit costs, recipes, workforce and static mesh reference | Valid recipe, footprint basics, presentation reference, positive costs |
| `UHansaTechnologyDefinition` | Create/edit nodes, prerequisites, costs and unlock references | Reachability, no forbidden cycles, valid unlocks, stable references |

City, need, population, vehicle, AI tuning, victory, and scenario definitions receive the common schema-driven browser/details fallback required by the integrated MVP. Events and dialogue may continue through ordinary Unreal asset editing until their game systems enter scope. Specialized Authoring Studio views for these types remain post-MVP.

## 5. Schema-driven adaptation requirement

This is the central MVP capability, not optional infrastructure.

`FHansaEditorSchemaRegistry` must discover the four supported definition classes and reflected properties. From one authoritative C++ property definition it produces:

- generic Details-panel fields;
- browser/table metadata;
- stable diff/patch paths;
- JSON Schema for AI proposals;
- metadata-coverage diagnostics;
- validation and serialization coverage checks.

MVP metadata:

```text
DisplayName / tooltip / category
Units and numeric range
Required or optional
Stable-reference type
Bulk-editable yes/no
AI access: Never / Read / Suggest / Generate
Migration classification
```

Acceptance test: add a test-only reflected property to a supported definition. It appears in the generic inspector and exported JSON Schema without a handwritten form. CI fails until its editor metadata, validation classification, serialization coverage, migration classification, and AI-access policy are provided.

## 6. Authoring Studio MVP UI

### 6.1 Component inventory

| Component | Implementation | Purpose |
| --- | --- | --- |
| Studio shell | Native Slate dock tab | Hosts the complete MVP workspace |
| Domain/definition browser | Slate tree/list | Switch type, search, filter, select definitions |
| Definition table | Virtualized Slate table | Compare and bulk-select definitions |
| Details inspector | Unreal Details framework | Edit reflected typed properties |
| Research graph | Slate graph editor | View/edit prerequisites and select a node |
| Validation panel | Slate list with severity/status icons | Explain errors, warnings and remedies |
| Impact panel | Slate list/tree | Show direct definition/fixture referencers |
| AI proposal diff | Native field-diff table | Accept/reject individual generated fields |
| Generation jobs panel | Virtualized Slate list | Status, provider, progress, cost and actions |
| Artifact review | Static-mesh/audio preview integration | Compare staged output and metadata |
| Promotion dialog | Native modal | Show checks, destination and final approval |

The MVP uses standard Unreal Editor styling with Hansa semantic status colors and accessible labels. Historical ornament and custom generated editor artwork are out of scope. No raster UI asset is required for the MVP.

### 6.2 Required states

Every asynchronous or interactive component supports:

- default, hover, pressed, selected, disabled, and keyboard focus;
- empty, loading, success, warning, error, cancelled, and stale states where applicable;
- explicit text/icon feedback in addition to color;
- non-blocking progress and cancellation for remote jobs;
- keyboard navigation for the primary workflow.

### 6.3 Main workflow

```text
Select definition
  → edit or request AI proposal
  → inspect field diff
  → validate temporary registry
  → run targeted fixture
  → accept selected fields
  → undo/redo if needed
  → save
  → compile registry
```

## 7. Validation and impact analysis MVP

### 7.1 Included validation

- required fields, types, ranges and units;
- duplicate/empty stable IDs;
- missing or wrong-type stable references;
- recipe has at least one positive output;
- no negative recipe quantity, duration, workforce or cost;
- building references valid recipe and presentation asset;
- technology prerequisites and unlocks exist;
- forbidden research cycles and unreachable nodes;
- deterministic registry compile and content hash;
- production-chain inputs are available in the selected vertical-slice content set;
- save-breaking ID change requires an explicit redirect.

### 7.2 Included impact analysis

- direct Asset Registry referencers;
- definition-graph references;
- recipes/buildings/technologies affected by the selected definition;
- vertical-slice fixtures whose content hash changes;
- multiplayer definition hash changes;
- warnings for renamed/deleted stable IDs.

Transitive balancing consequences across the entire future campaign are deferred.

## 8. OpenAI data-authoring MVP

### 8.1 Included use cases

- draft a new good, recipe, building, or technology definition;
- propose changes to selected `Suggest` fields;
- produce a short description/tooltip draft;
- explain validation failures and propose a corrected patch;
- propose research prerequisites using only allowlisted stable IDs.

The OpenAI Responses API supports structured JSON outputs and typed custom tools, which allows the MVP to constrain proposals to the generated Hansa schema rather than parsing free-form text. See the [official OpenAI Responses API reference](https://developers.openai.com/api/reference/cli/resources/responses/methods/create).

### 8.2 Request restrictions

- one definition or one tightly bounded related set per request;
- only selected existing definitions supplied as context;
- generated JSON Schema and allowed stable references included;
- no automatic web research in the MVP;
- no generated code, Blueprint, command, class name, path, or provider URL;
- fixed per-request output and cost limits;
- explicit user submission and spend confirmation;
- API key held by the worker, never Unreal or project content.

### 8.3 Review behavior

- reject schema/version/base-revision mismatches;
- show old value, proposed value, unit and rationale per field;
- allow individual field acceptance;
- validate a temporary copy before enabling Apply;
- apply as one `FScopedTransaction`;
- support undo/redo before save;
- store prompt, model/version, usage, response hash and decision in the job manifest;
- never interpret model prose as a passing balance result.

### 8.4 Balance check

For recipes and economic values, the MVP runs:

- type/range/reference validation;
- recipe conservation/source/sink checks;
- nominal production-ratio calculation;
- `lubeck_grain_shortage_v1` when affected fields participate in that scenario;
- before/after metrics and checksum comparison.

Full automated balance optimization is out of scope.

## 9. Generation worker MVP

### 9.1 Technology scope

The worker needs only:

- versioned request/response envelope;
- authenticated local connection;
- small persistent job database;
- provider capability listing;
- submit, poll, cancel, retry and download;
- isolated per-job temporary directory;
- content hashing and immutable manifest;
- cost estimate/actual usage fields;
- structured redacted logs;
- clean recovery after worker or editor restart.

Use a mock provider in automated tests. Normal CI never performs billable calls.

### 9.2 Provider scope

| Provider | MVP status | Capability |
| --- | --- | --- |
| OpenAI | Live adapter | Structured definition draft/patch |
| Tripo | Live adapter | One text/image-to-static-mesh workflow |
| ElevenLabs | Live adapter | One SFX workflow and one speech-line workflow |
| TRELLIS | Contract only | Capability descriptor and mock adapter; live local inference deferred |

The provider-neutral job model must not contain a Tripo-only assumption. TRELLIS is the first post-MVP adapter and must fit without changing Unreal production asset identities or the job state machine.

## 10. Static 3D asset MVP

### 10.1 Asset target

Generate one non-skeletal harbor prop, preferably a barrel stack, cargo crate set, mooring bollard, or simple market cart prop. Do not begin with a hero building or character.

### 10.2 Included pipeline

```text
Prompt/reference
  → spend confirmation
  → Tripo asynchronous job
  → download and hash
  → GLB/FBX validation
  → Unreal staging import
  → scale/axis/pivot/material inspection
  → triangle/material/texture budget check
  → simple collision check
  → deterministic preview scene
  → human approval
  → promotion to final path
```

Included validation:

- allowlisted input/output formats and maximum file size;
- centimeters/world-scale normalization;
- documented forward/up axes and pivot policy;
- configurable triangle and material-slot limit;
- textures present, decodable and within size limits;
- no provider URL/task ID used as Unreal asset identity;
- basic collision exists or a deliberate “visual-only” classification is recorded;
- complete prompt, provider/model, input rights, output hash and reviewer provenance;
- no production reference to the staging path.

Automated retopology, segmentation, custom LOD production, Nanite policy automation, skeletal meshes and animations are out of scope.

## 11. Audio MVP

### 11.1 Assets

- one short non-looping UI/harbor SFX;
- one short single-speaker English line with stable line ID and subtitle;
- two generated variants per asset maximum for the proof workflow.

### 11.2 Included pipeline

```text
Text/prompt + rights confirmation
  → spend confirmation
  → ElevenLabs job
  → download and hash
  → technical audio checks
  → Unreal staging import
  → preview variants
  → human approval
  → promotion and stable Hansa reference
```

Included checks:

- decodable output, duration, channels and sample rate;
- clipping and leading/trailing silence thresholds;
- subtitle/source text attached to the spoken line;
- speaker/voice permission acknowledgement;
- provider/model/settings and output hash provenance;
- final asset naming, compression category and destination;
- no provider voice ID used as gameplay identity.

Multi-speaker dialogue, voice cloning, pronunciation dictionaries, localization batches, forced alignment, looping ambience, automated loudness mastering and dialogue graphs are out of scope.

## 12. Staging and promotion MVP

Use these boundaries:

```text
Saved/GenerationJobs/<JobId>/
SourceArt/Generated/<Domain>/<JobId>/
Content/Hansa/Generated/Staging/<JobId>/
Content/Hansa/<Domain>/<Feature>/
```

Promotion requires:

1. completed immutable job manifest;
2. successful applicable validation;
3. provenance and input-rights acknowledgement;
4. visible staged preview;
5. explicit final destination;
6. named human approval;
7. atomic asset creation/reimport and reference update;
8. targeted reference validation after promotion.

Promotion cannot overwrite a production asset silently. Replacing an existing asset is a separate deliberate action showing referencers.

## 13. Security and release constraints

- `HansaEditor` is `Editor` host type only.
- Worker tools and provider SDKs remain under `Tools/` and are never staged with the game.
- Credentials come from the OS credential store or worker environment.
- No credentials, bearer headers, signed download URLs or raw private inputs appear in logs/manifests.
- Remote upload shows the exact selected input files before submission.
- Live calls require explicit enablement and per-job spend confirmation.
- Provider requests have time, size, retry and download limits.
- Staging and Developer content are excluded from Shipping cooks.
- A Shipping gate proves the editor module, worker, provider configuration, credentials, staging assets and development manifests are absent.

## 14. Tests and MVP acceptance criteria

### 14.1 Automated acceptance

- `HansaEditor` builds and loads without runtime modules depending on it.
- All four definition types are discoverable and editable.
- A test property automatically appears through schema reflection.
- Missing required metadata fails the schema-coverage test.
- JSON Schema export is deterministic and golden-tested.
- Create/edit/save/reload and undo/redo pass for each type.
- Recipe and research validation catches representative invalid content.
- Temporary registry compile never changes saved assets.
- AI patches reject invalid schema, stale revision, unknown field/reference and forbidden access.
- Mock worker covers success, timeout, cancellation, retry, malformed result and restart/resume.
- Golden mesh and audio artifacts pass/fail the correct import validators.
- Production content cannot reference `/Generated/Staging/`.
- Shipping package contains none of the forbidden editor/generation artifacts.

### 14.2 Manual demo acceptance

1. Open Hansa Authoring Studio.
2. Search for `Good.Grain`, edit a valid property, validate, undo and redo.
3. Open the research graph and create a valid prerequisite relationship.
4. Ask OpenAI for a bounded technology or recipe proposal.
5. Reject one field, accept another, run validation and apply the transaction.
6. Run the Lübeck shortage fixture and inspect before/after metrics.
7. Generate and review the static harbor prop through Tripo.
8. Promote it and confirm a production definition references only the promoted asset.
9. Generate and review the SFX and spoken line through ElevenLabs.
10. Compile the runtime registry and launch the vertical slice with the accepted content.
11. Package Shipping and run the exclusion audit.

### 14.3 MVP exit gate

The MVP is complete only when the full manual demo succeeds from a clean checkout and all automated acceptance tests pass without live-provider calls in normal CI.

## 15. Explicitly out of scope

- live TRELLIS generation;
- skeletal mesh generation, auto-rigging and animation generation/retargeting;
- custom production-chain graph editing beyond read-only relationship visualization;
- specialized editors for cities, needs, population, vehicles, routes, events, dialogue, scenarios, politics, AI and victory;
- multi-speaker dialogue and localization batch production;
- voice cloning;
- image generation or a custom visual-art editor;
- full DCC mesh editing, sculpting, UV editing or animation-curve editing;
- automatic balancing or unattended AI application;
- multi-user concurrent editing and custom merge tools;
- cloud-hosted worker farm, team queue or webhook infrastructure;
- arbitrary provider plugins installed at runtime;
- runtime/player-facing generation;
- live API calls in ordinary CI;
- custom ornamental Editor UI artwork.

## 16. Post-MVP order

1. Add live TRELLIS as the second 3D provider and prove provider interchangeability.
2. Add production-chain graph editing and specialized city/needs views.
3. Add canonical human skeleton, Tripo rigging, retargeting and dockworker animation slice.
4. Add narrative/dialogue editor, multi-speaker generation, pronunciation and localization.
5. Expand 3D QA with retopology, LOD/Nanite, texture packing, collision generation and modular-building profiles.
6. Add source-control workflow improvements, transitive impact analysis, batch jobs and team approvals.

## 17. Deliverable checklist

- [ ] `HansaEditor` module shell and Authoring Studio tab
- [ ] common definition base and four supported definition types
- [ ] schema registry, metadata policy and deterministic JSON Schema export
- [ ] definition browser/table/details and research graph
- [ ] validation, direct impact analysis and registry preview
- [ ] OpenAI structured proposal/diff/apply workflow
- [ ] external worker, protocol, job store, manifests and mock provider
- [ ] live Tripo static-mesh adapter and staged import/promotion
- [ ] live ElevenLabs SFX/speech adapter and staged import/promotion
- [ ] generation provenance, rights and spend confirmation
- [ ] editor/workflow automation tests and golden artifacts
- [ ] Shipping exclusion audit
- [ ] setup, credential, provider smoke-test and recovery documentation
