# Hansa — Integrated MVP Sprint and Prompt Plan

## 1. Purpose

This document converts [MVP.md](MVP.md) into an executable sprint backlog for the game, authoring editor, and hybrid MCP/semantic/screenshot testing stack. The three workstreams advance together. Editor support, deterministic fixtures, gameplay queries, semantic UI coverage, screenshots, tests, and Shipping exclusions are part of feature delivery rather than later cleanup.

The baseline assumes:

- fifteen sprints: Sprint 0 followed by fourteen two-week delivery sprints;
- a small focused team of roughly 1–3 developers using Codex for bounded implementation tasks;
- one prompt normally maps to one reviewable change set and one Codex task;
- prompts run in the listed order unless their dependencies are already proven;
- live paid provider calls are opt-in smoke tests, never an ordinary CI dependency;
- no sprint is accepted with failing build, test, deterministic-fixture, or release-boundary checks.

This is a dependency plan, not a promise of calendar duration. A larger team can overlap some editor, UI, and test prompts after their shared contracts land, while a solo developer should expect discovery and polish to extend the schedule.

## 2. How to use the prompts

Copy one prompt at a time into a Codex task opened on the Hansa repository. Complete and review that prompt before starting a dependent prompt. Do not paste an entire sprint as one request.

Every prompt in this plan inherits this execution contract:

```text
Read AGENTS.md completely, then read Docs/MVP.md, Docs/MVPSprintPlan.md, and every architecture or design document named by this prompt. Inspect the current repository and git status before changing anything. Preserve unrelated and pre-existing user changes. Implement the requested bounded vertical slice in Unreal Engine 5.8, primarily in C++ with Blueprint limited to presentation and designer-authored composition where the architecture permits it.

Keep runtime state authoritative and deterministic. Route state changes through the gameplay command gateway. Add or update editor coverage, schema metadata and validation, automation/query surfaces, deterministic fixtures, and tests whenever this task changes the corresponding feature. Do not expose mutable runtime state through automation, use arbitrary console execution, or add test-only branches to Shipping gameplay.

For UI or image work, read Docs/UIDesignBrief.md and Docs/UIAssetWorkflow.md completely and follow AGENTS.md. Use native UMG/Slate for interactive UI and dynamic text/data. If new visual design or raster imagery is required, use ImageGen through the component-first workflow, generate native-size assets separately, save prompt records, inspect at original resolution, and never resample or stretch a raster. If the approved references already specify the design, implement them without generating redundant artwork.

Add proportionate automated tests and run the narrowest relevant build/test/validation commands available. Never use live paid provider calls unless explicitly requested and credentials are present through the approved secure path. Do not commit, push, discard unrelated changes, or rewrite user work. Finish with a concise report listing files changed, validation run, evidence produced, known limitations, and the exact next prompt ID that is now unblocked.
```

If a prompt discovers that a prerequisite is absent or the architecture documents conflict with code, it should stop the conflicting implementation, record the evidence, and make only the smallest documentation/ADR correction needed for the user to decide. It must not silently invent a second architecture.

## 3. Sprint map

| Sprint | Outcome | Primary dependency |
| --- | --- | --- |
| 0 | Reproducible project, module skeleton, CI and release boundaries | Existing UE project |
| 1 | Deterministic simulation and command kernel | Sprint 0 |
| 2 | Editor, automation module, sidecar protocol and inspection foundations | Sprint 1 |
| 3 | Goods, recipes, inventories and production vertical slice | Sprint 2 |
| 4 | Population needs and deterministic local markets | Sprint 3 |
| 5 | Lübeck map, camera, grid, roads and placement | Sprint 2; definition registry |
| 6 | Construction, warehouses, local logistics and residences | Sprints 3–5 |
| 7 | Main HUD, build menu, inspector and alerts | Sprint 6 |
| 8 | City overview and signature market UI | Sprints 4 and 7 |
| 9 | Simulated cities, sea/land trade routes and route editor | Sprints 4, 6 and 8 |
| 10 | Research, merchant AI, scenario and victory | Sprint 9 |
| 11 | Save/load and two-client authoritative multiplayer proof | Sprint 10 |
| 12 | External generation worker and OpenAI proposal workflow | Sprints 3, 10 and 11 |
| 13 | Tripo prop and ElevenLabs SFX/dialogue proof pipelines | Sprint 12 |
| 14 | Integrated golden path, UAT, performance and Shipping hardening | All prior sprints |

## 4. Sprint 0 — Reproducible foundation

### Sprint goal

A clean checkout can generate project files, build the game and editor targets, run a smoke test, and prove that development-only modules will not enter Shipping.

### S00-P01 — Baseline audit and ADR seed

```text
Execute S00-P01 from Docs/MVPSprintPlan.md. Read Docs/TechnicalArchitecture.md in full. Audit the current UE 5.8 project, module/target files, config, tracked/untracked state, and build assumptions. Do not implement gameplay. Create a concise current-state report under Docs/Development, an ADR index, and the minimum first ADRs needed to lock module boundaries, stable ID policy, deterministic numeric/time policy, and Shipping exclusion policy. Reconcile documentation only where a demonstrable project fact differs. Add a dependency diagram and a verification checklist. Done when another developer can identify the intended modules, dependency direction, unresolved decisions, and the commands that will prove the baseline without guessing.
```

### S00-P02 — Unreal module and target skeleton

```text
Execute S00-P02 from Docs/MVPSprintPlan.md. Using the approved module boundaries in Docs/TechnicalArchitecture.md and the ADRs, create the minimal compilable module/target skeleton for Hansa runtime, HansaEditor as an Editor-only module, and HansaAutomation as a DeveloperTool module excluded from Shipping. Keep dependencies one-way and do not add provider SDKs. Add feature flags/config defaults that disable automation transport unless explicitly enabled in a non-Shipping build. Add module-level smoke automation tests for loadability and forbidden dependency checks. Done when Development Editor and game targets compile and a Shipping target cannot load or depend on HansaEditor/HansaAutomation.
```

### S00-P03 — Repeatable build and test entry points

```text
Execute S00-P03 from Docs/MVPSprintPlan.md. Add documented PowerShell entry points for generating project files where possible, building Development Editor, running headless Unreal automation tests, and performing a Shipping exclusion audit. Keep paths configurable and do not assume Unreal is installed on C:. Make failures actionable and preserve normal Unreal logs as artifacts under ignored directories. Add CI configuration only if the repository already has an intended CI platform; otherwise provide a platform-neutral CI checklist and local scripts. Done when the same commands can be used locally and later by CI, return meaningful exit codes, and do not track generated Unreal output.
```

### S00-P04 — Content, config and test conventions

```text
Execute S00-P04 from Docs/MVPSprintPlan.md. Establish the repository conventions needed by later prompts: C++ namespaces/folders, Content/Hansa paths, developer/staging exclusions, stable ID naming, log categories, test naming, fixture locations, Saved evidence locations, and config sections. Add lightweight examples without implementing domain features. Update README.md with the verified setup/build/test workflow and link the sprint plan. Add checks for accidental secrets, provider configuration, staging references, and ignored generated directories. Done when later features have one documented place for runtime code, editor code, automation code, fixtures, content, evidence and generated staging.
```

### Sprint 0 exit gate

- Development Editor and game targets build from the documented entry point.
- Module smoke tests pass.
- Shipping exclusion is executable, not merely documented.
- ADRs resolve the contracts required by Sprint 1.

## 5. Sprint 1 — Deterministic simulation kernel

### Sprint goal

Hansa has a headless, deterministic, serializable simulation kernel with stable identity, fixed time, ordered systems, commands, events and read-only projections.

### S01-P01 — Stable identities, quantities and clock

```text
Execute S01-P01 from Docs/MVPSprintPlan.md. Implement the foundational domain value types described by Docs/TechnicalArchitecture.md: stable definition/entity IDs, versioned simulation time, deterministic quantities/money/rates, seeded random streams, and explicit serialization helpers. Avoid float-dependent authoritative economy calculations. Add unit tests for comparison, overflow/range handling, ordering, serialization round trips and identical seeded sequences. Include debugging string formats that are stable enough for evidence but do not become save compatibility contracts. Done when these primitives compile without world Actors and can be tested headlessly.
```

### S01-P02 — Simulation state and ordered pipeline

```text
Execute S01-P02 from Docs/MVPSprintPlan.md. Implement the authoritative simulation state container and fixed-step ordered system pipeline, initially with no-op representative systems. Separate immutable definition data, mutable entity/state records, transient caches and UI projections. Define deterministic iteration/order rules and prevent direct UObject/Actor ownership of authoritative records. Add tests proving equal initial state plus equal commands and seed produces equal state hashes over many ticks. Expose a read-only snapshot/projection interface for later UI and automation.
```

### S01-P03 — Command gateway and domain events

```text
Execute S01-P03 from Docs/MVPSprintPlan.md. Implement the typed gameplay command gateway, validation/result model and ordered domain-event stream. Include representative create/cancel/no-op test commands without building city features. Commands must carry stable identity, authority context and deterministic ordering metadata; failures must return structured causes and never partially mutate state. Add transactional tests, replay tests and event-order tests. Ensure this same gateway can later serve player input, AI, multiplayer RPC handling and controlled automation actions without privileged shortcuts.
```

### S01-P04 — State hashes, projections and diagnostic evidence

```text
Execute S01-P04 from Docs/MVPSprintPlan.md. Add versioned state hashing, compact diagnostic summaries and projection-diff support around the simulation kernel. Define what is authoritative, excluded, normalized and versioned in hashes. Add a small deterministic fixture harness that can initialize state from a named descriptor and advance exact ticks without a rendered world. Produce machine-readable test evidence under Saved and tests that intentionally detect order drift. Done when a failed determinism test identifies the first divergent tick and relevant subsystem instead of returning only a final mismatch.
```

### Sprint 1 exit gate

- The kernel runs headlessly and has no presentation dependency.
- Deterministic replay/state-hash tests pass.
- Commands are the only supported mutation route.
- Read-only projections are ready for UI, editor previews and automation.

## 6. Sprint 2 — Editor and automation foundations

### Sprint goal

The project can author reflected definitions, export deterministic schemas, expose an opt-in semantic/query session, and capture native screenshots through development-only boundaries.

### S02-P01 — Definition base, schema registry and generic editor shell

```text
Execute S02-P01 from Docs/MVPSprintPlan.md. Read Docs/EditorArchitecture.md and Docs/EditorMVP.md in full. Implement UHansaDefinitionBase, schema/version metadata, registry discovery and deterministic JSON Schema export. Expand HansaEditor into an Editor-only Authoring Studio tab with a generic definition browser, search, details editing, validation results and undo/redo transactions. Start with one deliberately small sample definition used only to prove automatic property discovery. Add metadata-coverage and golden-schema tests. Done when adding an approved reflected field makes it appear in the generic editor and schema without a handwritten form, while missing required metadata fails validation.
```

### S02-P02 — HansaAutomation session and capability boundary

```text
Execute S02-P02 from Docs/MVPSprintPlan.md. Implement the HansaAutomation DeveloperTool module boundary described in Docs/TechnicalArchitecture.md. Add explicit non-Shipping enablement, session open/close, protocol version, capability discovery, permission levels, correlation IDs, timeouts and structured errors. Do not expose arbitrary UObject reflection, console commands, file access or mutable state pointers. Add tests for disabled-by-default behavior, incompatible versions, missing capabilities, rejected commands and Shipping unavailability.
```

### S02-P03 — MCP sidecar protocol scaffold

```text
Execute S02-P03 from Docs/MVPSprintPlan.md. Create Tools/HansaMcp as the external local sidecar scaffold. It must speak MCP over STDIO on the Codex side and a versioned framed named-pipe or loopback protocol to the running development game/editor. Implement only session, capabilities, ping, health and structured error forwarding in this prompt. Add a fake in-process endpoint for contract tests so CI needs no game process. Document startup, shutdown, reconnect and log redaction. The sidecar must never need Unreal headers, provider credentials or Shipping packaging.
```

### S02-P04 — Semantic registry, synchronization and screenshot base

```text
Execute S02-P04 from Docs/MVPSprintPlan.md. Add the first semantic UI inspection registry and native screenshot service in HansaAutomation. Define stable semantic IDs, roles, labels, states, bounds, enabled/visible/focus properties and child relationships independently of widget class names. Add wait_for synchronization based on observable predicates, not sleeps. Implement screenshot capture at explicitly requested 1280x720 and 1920x1080 output sizes, with metadata and no post-capture resizing. Use a minimal automation-only test screen to prove find/state/activate/focus and capture. Save evidence bundles under ignored Saved paths.
```

### Sprint 2 exit gate

- The generic editor proves reflected field and deterministic schema behavior.
- MCP sidecar and Unreal endpoint negotiate capabilities against mocks.
- Semantic inspection and native-size capture work on a minimal screen.
- Shipping still excludes editor, automation, sidecar and test assets.

## 7. Sprint 3 — Goods, recipes, inventory and production

### Sprint goal

The ten MVP goods and four representative production chains run deterministically in a headless city, and designers can author and validate their definitions.

### S03-P01 — Economic definitions and initial content

```text
Execute S03-P01 from Docs/MVPSprintPlan.md. Implement definition types for goods, recipes and buildings using the shared definition base and stable references. Author the ten goods and the MVP recipes/buildings enumerated in Docs/MVP.md, with units, cycle times, capacities, workforce placeholders and localization keys. Add deterministic registry compilation, reference validation and content hashes. Extend Authoring Studio with specialized high-value goods/recipe/building views only where the generic editor is insufficient. Add create/edit/save/reload/undo tests and invalid-reference fixtures.
```

### S03-P02 — Inventories and reservations

```text
Execute S03-P02 from Docs/MVPSprintPlan.md. Implement city/building/warehouse inventory records with deterministic capacity, accepted goods, transfers, reservations and transaction results. Model sources and sinks explicitly; prevent negative stock, duplication and partial transfer failure. Keep the system headless and Actor-independent. Add projections and typed read-only queries for stock, capacity, reserved amount and recent movements. Add property/invariant tests including competing reservations and deterministic ordering.
```

### S03-P03 — Production system and causal output

```text
Execute S03-P03 from Docs/MVPSprintPlan.md. Implement recipe execution across the four MVP production chains: grain-to-flour-to-bread, timber-to-planks, iron-to-tools and grain-to-beer, plus fish and salt extraction/supply behavior defined by the MVP. Production consumes inputs and produces outputs at fixed ticks, observes capacity/workforce placeholders and emits typed events. Add causal factor projections for actual versus nominal throughput, missing input, storage blockage and inactive state. Test long runs, boundary quantities and identical state hashes.
```

### S03-P04 — Production fixture, queries and editor validation

```text
Execute S03-P04 from Docs/MVPSprintPlan.md. Add a named headless production fixture with known stocks and buildings, then expose allowlisted automation queries and controlled run/step/run_until tools through HansaAutomation and the sidecar. Add recipe conservation, cycle/reachability and source/sink validators to Authoring Studio and commandlets. Produce a machine-readable golden evidence bundle containing fixture version, registry hash, initial/final state hashes, events and causal projections. Done when the production slice can be authored, compiled, run and diagnosed without opening a rendered map.
```

### Sprint 3 exit gate

- All ten goods and MVP recipes compile into a deterministic registry.
- The four production chains pass inventory/conservation tests.
- Authoring Studio can edit and validate the supported definitions.
- MCP can load, advance and query the production fixture.

## 8. Sprint 4 — Population needs and local markets

### Sprint goal

Two citizen tiers consume goods, influence migration/satisfaction, and drive explainable local supply/demand prices without nondeterministic behavior.

### S04-P01 — Population and needs definitions/system

```text
Execute S04-P01 from Docs/MVPSprintPlan.md. Implement two MVP population tiers, residence cohorts, workforce supply, need definitions and deterministic consumption. Separate access, affordability and reliability. Model growth/decline and satisfaction with bounded, explainable factors. Extend generic Authoring Studio coverage for population needs and add validation for impossible consumption, missing goods and invalid tier progression. Add projections and tests for consumption, reserve days, workforce and stable long-run behavior.
```

### S04-P02 — Local supply, demand and pricing

```text
Execute S04-P02 from Docs/MVPSprintPlan.md. Implement a deterministic local market per city using stock, desired reserve, citizen demand, industrial demand, expected incoming supply and bounded modifiers. Define exact update cadence, smoothing/rounding, min/max prices and stale-report behavior. Prevent oscillation caused solely by iteration order. Add typed price history and market projections plus unit/property tests for scarcity, surplus, no-demand, zero-stock and recovering-supply cases.
```

### S04-P03 — Causal market explanations and alerts

```text
Execute S04-P03 from Docs/MVPSprintPlan.md. Build a shared causal explanation model that converts market and population state into ordered human-readable factors without duplicating formulas in UI code. Add shortage/reserve/affordability alerts with severity, affected stable IDs, age, cause and suggested player actions. Expose typed queries for prices, history, supply/demand components, reserve days, consumer/producer lists and active alerts. Add localization-ready message templates and tests proving explanation totals match authoritative calculations.
```

### S04-P04 — Lübeck grain-shortage headless fixture

```text
Execute S04-P04 from Docs/MVPSprintPlan.md. Create the versioned lubeck_grain_shortage_v1 headless fixture with deterministic baseline, shortage onset and recoverable conditions. Add MCP tools/waits/assertions needed to run until the shortage, inspect causal factors, issue controlled stock/production commands through the normal gateway and verify price/reserve recovery. Add Authoring Studio fixture launch and before/after metric preview. Capture a golden structured evidence bundle and document fixture migration rules.
```

### Sprint 4 exit gate

- Population consumption drives market demand and prices.
- Prices remain bounded, stable and explainable.
- `lubeck_grain_shortage_v1` reproduces and recovers deterministically.
- Editor, queries and tests use the same definitions and causal model as runtime.

## 9. Sprint 5 — Lübeck world, camera and placement

### Sprint goal

The player can navigate a representative Lübeck map, select a building, place roads/buildings on a grid, and receive deterministic placement reasons with semantic test coverage.

### S05-P01 — Representative Lübeck map and camera

```text
Execute S05-P01 from Docs/MVPSprintPlan.md. Create the MVP Lübeck gameplay map with enough land, shore and harbor topology to support the full vertical slice, using placeholder or licensed existing assets where necessary. Implement strategy camera pan/zoom/rotate, world selection trace, map bounds and Enhanced Input actions for mouse/keyboard plus controller-ready intents. Do not spend this prompt on final environment art. Add a deterministic map identifier and automation spawn/start location. Test input intent handling where possible and document required Blueprint composition.
```

### S05-P02 — Grid, footprints and placement validation

```text
Execute S05-P02 from Docs/MVPSprintPlan.md. Implement grid coordinates, building footprints, rotation, occupancy and deterministic placement validation for terrain, collision, shore, roads, prerequisites and ownership. Placement preview must consume structured validation results and confirmation must submit the normal build command. Add road drag placement and cancel/repeat behavior. Keep authoritative placement in simulation records; Actors are projections. Add unit tests for rotations, boundaries, overlap and order independence.
```

### S05-P03 — Building/road world projections

```text
Execute S05-P03 from Docs/MVPSprintPlan.md. Implement pooled or managed world presentation for placed buildings, construction placeholders, roads, selection outlines and status markers. Bind visuals to simulation projections/events rather than owning economy state. Establish stable mapping between entity IDs and Actors and safe teardown/rebuild behavior. Use simple MVP meshes/materials and record any missing art without blocking mechanics. Add tests for projection creation/update/removal and map reload reconstruction.
```

### S05-P04 — Semantic placement test and screenshots

```text
Execute S05-P04 from Docs/MVPSprintPlan.md. Instrument camera, build selection, placement preview, validation reason, confirm/cancel and resulting entity with stable semantic IDs and typed state. Add controlled automation actions only through normal input intents/commands. Create empty_lubeck_build_v1 and an automated flow that loads it, selects a road and building, verifies invalid then valid placement, confirms, waits for authoritative state and captures native 1280x720 and 1920x1080 evidence. Do not resize captures. Add screenshot metadata and structural assertions so the test is not pixel-only.
```

### Sprint 5 exit gate

- The map, camera, selection and placement loop are playable.
- Roads/buildings are authoritative simulation entities with projected Actors.
- Placement failures explain the precise cause.
- `empty_lubeck_build_v1` passes semantic and screenshot checks at both native sizes.

## 10. Sprint 6 — Construction, logistics and residences

### Sprint goal

Placed buildings construct, warehouses move inputs/outputs over roads, residences host citizens, and the complete Lübeck production/population loop runs in the world.

### S06-P01 — Construction lifecycle and costs

```text
Execute S06-P01 from Docs/MVPSprintPlan.md. Implement construction states, deterministic resource/currency costs, build time, completion, cancellation and safe removal through typed commands. Integrate all MVP building definitions. Emit ordered events and projections for progress, missing cost and completion. Add validation against negative refunds/duplication and tests for cancel boundaries, save-ready serialization and command rejection. Update editor fields/schema/validation and automation queries/actions in the same change.
```

### S06-P02 — Warehouse and road logistics

```text
Execute S06-P02 from Docs/MVPSprintPlan.md. Implement the MVP local logistics abstraction connecting production buildings, markets, warehouses and docks over the road graph. Use deterministic requests, capacity, pickup/delivery delay and prioritization; visual haulers may be simplified projections. Prevent teleporting goods except where the documented abstraction explicitly models a completed delivery. Add bottleneck causal factors, typed queries and invariant tests for disconnected roads, full destinations, competing requests and state-hash determinism.
```

### S06-P03 — Residences, growth and city loop

```text
Execute S06-P03 from Docs/MVPSprintPlan.md. Connect constructed residences to the two population tiers, housing capacity, migration/growth, workforce assignment, market access and needs satisfaction. Implement the MVP rules for residence progression without adding post-MVP civic depth. Add city-level projections for population trend, workforce, satisfaction and staple reserve. Extend editor validation and tests for unsatisfied needs, no market access, workforce shortage and recovery.
```

### S06-P04 — Integrated Lübeck gameplay fixture

```text
Execute S06-P04 from Docs/MVPSprintPlan.md. Upgrade empty_lubeck_build_v1 or add a narrowly named fixture to exercise construction, road connection, warehouse delivery, production, residence growth and bread consumption in one deterministic world. Add MCP waits/assertions around building completion, inventory movement, production and population state. Capture structured evidence and minimal world screenshots. Run long enough to reveal leaks, deadlocks and nondeterministic ordering. Done when the world slice and the headless economic slice produce matching projections for equivalent state.
```

### Sprint 6 exit gate

- Construction, logistics, production and population operate together in Lübeck.
- All MVP building types have valid definitions and at least placeholder presentation.
- Bottlenecks and failures are queryable and explainable.
- The integrated city fixture passes repeatedly with equal final hashes.

## 11. Sprint 7 — Main HUD, build menu, inspector and alerts

### Sprint goal

The first complete playable UI shell makes construction and problem diagnosis usable with mouse/keyboard and the controller golden path.

### S07-P01 — HUD component specification and style system

```text
Execute S07-P01 from Docs/MVPSprintPlan.md. Read Docs/UIDesignBrief.md and Docs/UIAssetWorkflow.md completely. Inventory the MVP HUD components and state matrix, classify each as UMG, Slate, vector/SDF, material or raster, and map every component to stable semantic IDs. Implement centralized color, typography, spacing, focus, severity and motion tokens plus reusable native panel/button/tab/tooltip styles. Use the approved main HUD reference as direction. Generate new component references with ImageGen only if an unresolved visual component genuinely requires them, following native-size separate-component rules and prompt records. Add contrast/token tests where feasible.
```

### S07-P02 — Root HUD and presentation models

```text
Execute S07-P02 from Docs/MVPSprintPlan.md. Implement the root HUD shell: top status bar, selected-city breadcrumb, time/speed controls, collapsible alert stack, bottom build/selection area, right inspector host, notification layer and focus layer. Drive it through event-updated C++ presentation models with no Blueprint tick or raw per-frame bindings. Keep the central city view visible. Implement responsive anchors and safe areas for 1280x720 and 1920x1080. Add semantic roles/state and automated open/close/focus tests.
```

### S07-P03 — Build menu and placement UI

```text
Execute S07-P03 from Docs/MVPSprintPlan.md. Implement the MVP build categories/cards, locked/invalid reasons, costs, workforce, footprint, input-output summary, rotation/repeat/cancel and placement feedback. Connect all actions to input intents and the command gateway. Status must use text/icon/shape in addition to color. Add keyboard shortcuts, controller focus order and non-drag alternatives. Instrument every visible action and validation reason semantically. Extend empty_lubeck_build_v1 with a full build-menu flow and native-size screenshots.
```

### S07-P04 — Inspector, alerts and causal navigation

```text
Execute S07-P04 from Docs/MVPSprintPlan.md. Implement the reusable contextual inspector for buildings and residences in the stable order defined by Docs/UIDesignBrief.md. Show identity/state, result, inputs/outputs or needs, current problem/cause, actions and history. Implement alert grouping, severity, age, affected object, click-to-frame/open-cause, snooze and pinned tracking at MVP depth. Use the shared causal model; do not recalculate simulation formulas in widgets. Add semantic navigation, tooltip, focus restoration, localization expansion and screenshot tests.
```

### Sprint 7 exit gate

- Core construction and diagnosis require no debug UI.
- All visible actions and status have stable semantic coverage.
- Mouse/keyboard flow is complete and controller golden path works.
- HUD screenshots pass structural checks at both native resolutions.

## 12. Sprint 8 — City overview and market UI

### Sprint goal

The player can diagnose a grain shortage, understand price movement and identify an effective response from the city and market screens within the MVP usability target.

### S08-P01 — City overview screen

```text
Execute S08-P01 from Docs/MVPSprintPlan.md. Implement the MVP City Overview with Population, Production and Market tabs; Administration may be a clearly non-interactive future placeholder only if needed for layout. Add header summaries for population trend, treasury contribution, satisfaction, workforce, staple reserve and alerts. Use virtualized/event-driven presentation models and causal links to supplying chains/buildings. Implement stable semantic IDs, keyboard/controller tab/focus behavior, empty/loading/error states and long-label layout tests.
```

### S08-P02 — Market table and selection model

```text
Execute S08-P02 from Docs/MVPSprintPlan.md. Implement the signature market goods table for the ten MVP goods with search, sort and filters; stock, reserve, demand, price, trend, incoming and status columns; and stable selection across refreshes. Use native text/data and a virtualized list. Add stale/estimated treatment even if the local Lübeck report is current. Connect table actions to typed view models, not mutable state. Instrument rows/cells/actions semantically and test sorting, filtering, selection and screen-reader-equivalent labels.
```

### S08-P03 — Selected-good causal panel and chart

```text
Execute S08-P03 from Docs/MVPSprintPlan.md. Implement the selected-good panel with local price, recent-average difference, stock versus desired reserve, citizen and industrial demand, incoming supply, consumers/producers and a bounded price-history chart. Use a custom Slate chart only if profiling or accessibility makes UMG composition unsuitable. The explanation must come from authoritative causal projections. Add direct actions for pinning and beginning a route workflow, with unavailable actions explaining why. Add semantic data points/summaries and deterministic history tests.
```

### S08-P04 — Shortage-diagnosis UAT automation

```text
Execute S08-P04 from Docs/MVPSprintPlan.md. Build an automated semantic UAT flow on lubeck_grain_shortage_v1: detect the alert, open the affected city/market, select grain, verify the causal explanation, navigate to an undersupplied bakery or source chain, and identify a valid corrective action. Capture native screenshots at key waits in 1280x720 and 1920x1080 and bundle semantic tree, query snapshots, logs, fixture/hash metadata and assertions. Add accessibility checks for color redundancy, focus order, high contrast, large text and reduced motion. Do not accept a pixel-only pass.
```

### Sprint 8 exit gate

- A player can find why bread/grain is unavailable in the target flow.
- Market numbers and explanations match simulation projections.
- Tables remain responsive and accessible at both reference resolutions.
- The semantic shortage-diagnosis UAT produces a complete evidence bundle.

## 13. Sprint 9 — Intercity trade and route editor

### Sprint goal

Lübeck trades with Hamburg, Lüneburg and Rostock through one cog and one wagon route, with deterministic delivery and an understandable route editor.

### S09-P01 — Simulated city markets and information age

```text
Execute S09-P01 from Docs/MVPSprintPlan.md. Add simulated market-only city definitions/state for Hamburg, Lüneburg and Rostock using the same goods, demand and pricing contracts as Lübeck. Model report timestamps, current/recent/stale/estimated/unknown states and deterministic remote market evolution at MVP depth. Extend Authoring Studio generic coverage, validation and initial content. Add typed queries for known prices, report age, supply/demand and opportunity comparison, plus tests that never represent unknown information as zero.
```

### S09-P02 — Route, vehicle and delivery simulation

```text
Execute S09-P02 from Docs/MVPSprintPlan.md. Implement route and vehicle definitions/state for one cog sea route and one wagon land route. Support ordered stops, load/unload quantities, minimum reserve, capacity, travel time, cost and deterministic arrival/delivery. Keep advanced conditional trading out of scope except schema-safe future extension points. All route edits use typed commands and validate ownership, stock protection and reachable stops. Add projections/events and invariant tests for capacity, reserve, cancellation, missed cargo and identical replay.
```

### S09-P03 — European trade map and route editor UI

```text
Execute S09-P03 from Docs/MVPSprintPlan.md. Implement the MVP European trade-map shell focused on the four cities and two route modes, plus the Simple route editor defined in Docs/UIDesignBrief.md. Show stops, cargo actions, minimum reserve, round-trip time, capacity, upkeep, expected profit range and reserve risk. Reuse centralized style/components and native route geometry; do not bake dynamic map labels into rasters. Add stable semantic IDs, controller alternatives to drag, uncertainty treatment and responsive native screenshots.
```

### S09-P04 — Route delivery fixture and MCP flow

```text
Execute S09-P04 from Docs/MVPSprintPlan.md. Create route_delivery_v1 with known Lübeck shortage, remote supply, cog/wagon availability and expected delivery timing. Add semantic/MCP tools to create or edit the route through ordinary commands, wait for departure/arrival/delivery, query cargo and market effects, and assert the Lübeck reserve/price response. Capture route-editor and market evidence with synchronized state hashes, events and screenshots. Test cancellation, reserve protection and stale-report branches as focused cases.
```

### Sprint 9 exit gate

- Both sea and land routes deliver goods deterministically.
- Four local markets retain separate prices and information age.
- The simple route workflow is usable without debug controls.
- `route_delivery_v1` proves an observable market response end to end.

## 14. Sprint 10 — Research, merchant AI, scenario and victory

### Sprint goal

The slice has strategic progression, one capable rival and an explicit scenario with several MVP-compatible win paths and a deterministic completion state.

### S10-P01 — Research definitions, system, graph editor and UI

```text
Execute S10-P01 from Docs/MVPSprintPlan.md. Implement the roughly nine MVP technologies across Commerce, Production and Logistics, including prerequisites, costs, progress, unlock/effect application and a small queue. Add research graph validation for missing nodes, cycles and unreachable content; implement the specialized Authoring Studio research graph and the runtime research screen. Effects must reference stable IDs and apply deterministically. Add semantic coverage and tests for unlocks, prerequisites, serialization readiness and UI focus navigation.
```

### S10-P02 — Merchant AI through player contracts

```text
Execute S10-P02 from Docs/MVPSprintPlan.md. Implement one merchant AI rival using read-only observations, utility/goal evaluation and the same typed commands available to a player. MVP behavior must manage a simple economy, react to shortages/opportunities, research and operate routes without perfect hidden information. Use deterministic decision cadence and seeded tie-breaking. Add AI tuning definitions with generic editor coverage, traceable decision diagnostics and scenario tests proving it can recover from one shortage and complete one trade objective without privileged mutation.
```

### S10-P03 — Scenario, objectives and victory paths

```text
Execute S10-P03 from Docs/MVPSprintPlan.md. Implement scenario/objective/victory definitions and runtime evaluation for lubeck_grain_shortage_v1. Provide several bounded MVP win possibilities consistent with Docs/GameConcept.md, such as prosperity/economic, trade-network and research/civic milestones, while using a single polished scenario and explicit thresholds. Add scenario start, progress, success/failure and victory UI. Extend generic editor coverage and validation for impossible objectives, missing references and ambiguous endings. Add deterministic tests for each path and tie/ordering rules.
```

### S10-P04 — Strategic vertical-slice golden flow

```text
Execute S10-P04 from Docs/MVPSprintPlan.md. Extend the automated vertical slice from building and shortage diagnosis through route recovery, research unlock, AI turn progression and one scenario victory. Use only stable semantic UI operations, typed gameplay commands, observable waits and typed queries. Add assertion checkpoints and screenshots without making this the final full-MVP golden test. Bundle AI decisions, research events, objective state, state hashes and logs so failures are causal. Run multiple seeds allowed by the fixture contract and prove deterministic results per seed.
```

### Sprint 10 exit gate

- Research unlocks change gameplay through stable effect contracts.
- The merchant AI acts through the normal command gateway.
- The scenario exposes multiple clear victory approaches.
- A strategic end-to-end flow reaches a valid deterministic victory.

## 15. Sprint 11 — Save/load and multiplayer proof

### Sprint goal

The complete vertical slice survives save/load and runs as a server-authoritative two-client technical multiplayer proof with queryable evidence.

### S11-P01 — Versioned save envelope and migrations

```text
Execute S11-P01 from Docs/MVPSprintPlan.md. Implement the versioned save envelope described in Docs/TechnicalArchitecture.md, including content/registry hash, simulation version, scenario/seed/time, authoritative state, player ownership, pending commands where appropriate and migration metadata. Use stable IDs rather than UObject paths for domain identity. Add explicit migrations from at least one synthetic prior schema, corruption/incompatibility errors and deterministic round-trip hashes. Keep UI/transient caches out of authoritative saves.
```

### S11-P02 — Save/load UI and round-trip fixture

```text
Execute S11-P02 from Docs/MVPSprintPlan.md. Implement MVP save/load UI with slots, timestamp/scenario/version/content compatibility, confirmation and actionable errors. Create save_roundtrip_v1 that reaches nontrivial construction, inventories, prices, route cargo, research, AI and objective state; saves; reloads; and proves equivalent authoritative hash and projections before continuing deterministically. Instrument the UI semantically and add MCP save/load/wait/assert coverage without granting arbitrary filesystem access.
```

### S11-P03 — Server authority and replicated projections

```text
Execute S11-P03 from Docs/MVPSprintPlan.md. Implement the two-client multiplayer technical proof: server owns the simulation and validates commands; clients send intents and receive relevancy-aware projections/events rather than mutable full-state access. Cover placement, market state, route operation, research and victory progress at MVP depth. Add ownership and rejection feedback. Do not attempt production matchmaking, diplomacy or latency compensation beyond the proof. Add tests for unauthorized commands, ordering, late projection refresh and client/server hash diagnostics.
```

### S11-P04 — Two-player authority fixture and reconnect proof

```text
Execute S11-P04 from Docs/MVPSprintPlan.md. Create two_player_authority_v1 and an automated two-client test that starts server/client processes, verifies session readiness, issues allowed and rejected commands from each owner, observes replicated projections, disconnects/reconnects one client and validates resynchronization. Capture per-process logs, correlations, server authoritative hash, client projection digests and native screenshots. Make timeouts and process cleanup robust. Document the limitations separating this proof from production multiplayer.
```

### Sprint 11 exit gate

- `save_roundtrip_v1` resumes with equivalent authoritative state and deterministic continuation.
- The server rejects unauthorized client mutation.
- Both clients observe consistent relevant projections and recover after reconnect.
- Evidence distinguishes authoritative hash from partial client projections.

## 16. Sprint 12 — Generation worker and OpenAI authoring

### Sprint goal

Authoring Studio can submit resumable external jobs and safely review/apply an OpenAI structured proposal without credentials or model output entering runtime authority.

### S12-P01 — Worker protocol, job store and mock providers

```text
Execute S12-P01 from Docs/MVPSprintPlan.md. Implement Tools/HansaGenerationWorker with the versioned local protocol, persistent job state machine, immutable manifests, input/output hashes, cancellation, retry, resume and structured progress/errors defined in Docs/EditorArchitecture.md. Add a provider-neutral capability/request/result contract and deterministic mock provider. Store transient jobs under ignored Saved paths and redact secrets/URLs/private inputs. Add contract tests for success, malformed output, timeout, cancellation, retry, worker restart and idempotent resume.
```

### S12-P02 — Authoring Studio job bridge and security controls

```text
Execute S12-P02 from Docs/MVPSprintPlan.md. Add the HansaEditor-to-worker bridge and Authoring Studio job queue UI. Include provider capability display, exact upload preview, rights acknowledgement, estimated budget/spend confirmation, progress, cancellation, retry, result preview and provenance. Credentials must come only from the approved OS credential store or worker environment and never enter Unreal assets/config/logs/manifests. Add mock-backed editor automation tests and prove worker/editor absence from Shipping.
```

### S12-P03 — OpenAI schema-valid proposal adapter

```text
Execute S12-P03 from Docs/MVPSprintPlan.md. Implement the OpenAI Responses API adapter in the external worker for bounded definition proposals/patches using the deterministic exported JSON Schema and structured output. Support only allowlisted AI-writable fields and stable references; include base revision/content hash and provenance. In Authoring Studio, show a field-level diff with selective acceptance, validation, temporary-registry balance preview, explicit apply and undo. Reject prose masquerading as data, stale revisions, unknown fields/references and schema mismatches. Use mock/recorded responses in tests; a live smoke test is optional and explicitly user-triggered.
```

### S12-P04 — OpenAI authoring acceptance flow

```text
Execute S12-P04 from Docs/MVPSprintPlan.md. Add automated editor tests and a manual demo flow that asks for one bounded recipe or technology proposal, rejects one field, accepts another, validates it, previews lubeck_grain_shortage_v1 metrics, applies in one transaction, undoes/redoes and recompiles the deterministic registry. Cover stale-content, invalid-reference, forbidden-field, malformed-response, cancellation and worker-restart failures with mocks. Document secure setup, model/version pinning, budget policy and recovery without placing an API key in the repo.
```

### Sprint 12 exit gate

- Worker jobs survive restart and remain provider-neutral.
- OpenAI output is schema constrained, diffed, validated and human approved.
- Ordinary CI uses mocks and spends no provider credits.
- Runtime and Shipping have no worker, credentials or direct OpenAI dependency.

## 17. Sprint 13 — Tripo and ElevenLabs media proof

### Sprint goal

The editor can generate, validate, review and promote one static harbor prop, one SFX and one spoken line through provider-neutral, auditable pipelines.

### S13-P01 — Shared staging, provenance and promotion

```text
Execute S13-P01 from Docs/MVPSprintPlan.md. Implement the common staged asset workflow for provider outputs: immutable source download/hash, allowlisted formats, isolated staging import, asset metadata/provenance, rights approval, reviewer identity, explicit destination, validation, preview and atomic promotion. Prevent silent overwrite and any production reference to Content/Hansa/Generated/Staging. Add golden artifact tests, referencer checks, rollback behavior and a Shipping cook audit. Keep provider IDs/URLs out of gameplay identities.
```

### S13-P02 — Tripo static harbor prop adapter

```text
Execute S13-P02 from Docs/MVPSprintPlan.md. Implement the live Tripo adapter in the external worker for one text/image-to-static-mesh harbor prop workflow, following Docs/EditorMVP.md. Add spend confirmation, polling/retry/cancel, bounded download, GLB/FBX validation and manifest provenance. In Unreal staging, validate centimeters, axes, pivot, triangle/material/texture limits and basic collision; preview in a deterministic scene and promote only after human approval. Add mock/recorded provider tests. Do not implement skeletal meshes, rigging, animations, automated retopology or live TRELLIS.
```

### S13-P03 — ElevenLabs SFX and speech adapters

```text
Execute S13-P03 from Docs/MVPSprintPlan.md. Implement worker adapters for one short non-looping UI/harbor SFX and one short single-speaker English line, with at most two variants each. Require rights/voice acknowledgement and spend confirmation. Validate decoding, duration, channels, sample rate, clipping and leading/trailing silence; attach stable SFX/dialogue line IDs and subtitle text; preview and promote approved takes with provider/model/settings/output-hash provenance. Use mocks/recordings in CI and do not add voice cloning, multi-speaker dialogue or localization batches.
```

### S13-P04 — Media pipeline acceptance and recovery

```text
Execute S13-P04 from Docs/MVPSprintPlan.md. Build editor automation and a clean manual demo for mock-first Tripo, ElevenLabs SFX and ElevenLabs speech jobs: submit, observe progress, cancel/retry one case, restart/resume, validate, preview, approve, promote and verify final references. Add optional explicitly enabled live smoke instructions without embedding credentials or making acceptance depend on provider availability. Capture provenance/evidence and prove that deleting transient Saved job state does not invalidate promoted asset provenance stored under the approved source/content paths.
```

### Sprint 13 exit gate

- One static prop, one SFX and one spoken line complete staged promotion.
- Normal CI proves the same workflows with mocks/golden artifacts.
- Production references contain stable Hansa IDs/assets, not provider identities.
- Shipping excludes providers, credentials, worker, staging and developer preview content.

## 18. Sprint 14 — Integrated hardening and MVP release candidate

### Sprint goal

The complete MVP passes the clean-checkout authoring/play/automation demo, accessibility and performance gates, deterministic and multiplayer tests, and a Shipping exclusion audit.

### S14-P01 — Full MCP golden end-to-end test

```text
Execute S14-P01 from Docs/MVPSprintPlan.md. Implement the final golden MCP test required by Docs/MVP.md: start a development session, negotiate capabilities, load/reset the fixture, build the Lübeck slice, diagnose the grain shortage through semantic UI, establish a delivering route, unlock research, observe merchant AI, save/load, reach a valid victory and capture synchronized evidence. Use observable waits rather than sleeps, normal commands rather than shortcuts, and structural/query assertions alongside native screenshots. Bundle protocol versions, fixture/content hashes, seed, state hashes, semantic snapshots, events, logs and assertions for actionable failures.
```

### S14-P02 — UI UAT, accessibility and resolution pass

```text
Execute S14-P02 from Docs/MVPSprintPlan.md. Run evidence-led UAT on the MVP golden flows and fix verified priority defects in HUD, build mode, inspector, city overview, market, route editor, research, scenario/victory and save/load UI. Validate mouse/keyboard, complete controller golden path, focus restoration, localization expansion, high contrast, large text, reduced motion, color redundancy and safe areas. Capture true native 1280x720 and 1920x1080 screenshots; never resize raster assets or captures. Keep changes component-based and update semantic coverage with every UI fix.
```

### S14-P03 — Determinism, performance, networking and security hardening

```text
Execute S14-P03 from Docs/MVPSprintPlan.md. Profile and harden the MVP against the budgets in Docs/TechnicalArchitecture.md. Run long deterministic simulations, repeated fixture hashes, inventory conservation, market bounds, AI cadence, list/widget update costs, Actor/projection counts, save size/time, two-client replication and reconnect. Fuzz or boundary-test command/protocol/schema inputs. Audit secret redaction, upload confirmation, file/path limits and disabled automation. Fix measured MVP blockers and record deferred non-blockers with evidence; do not broaden feature scope.
```

### S14-P04 — Clean-checkout release gate and handoff

```text
Execute S14-P04 from Docs/MVPSprintPlan.md. From a clean-checkout-equivalent state, run the documented build, automated tests, editor authoring demo with mocks, integrated gameplay demo, MCP golden test, save/multiplayer fixtures and Shipping package audit. Verify no Shipping reference or package contains HansaEditor, HansaAutomation, Tools workers, provider credentials/config, Generated/Staging, Developer preview content or test-only fixtures. Update README and developer/provider/test documentation to match verified commands. Produce an MVP acceptance report mapping every Docs/MVP.md gate to evidence, with unresolved items explicitly blocking release rather than being silently waived.
```

### Sprint 14 exit gate

- Every acceptance item in [MVP.md](MVP.md) maps to passing evidence.
- The clean authoring, play and automated demo succeeds without live provider calls.
- Native UI, accessibility, deterministic, save and multiplayer gates pass.
- Shipping exclusion is proven by inspecting the built artifact.

## 19. Parallelization guidance

Safe overlap is limited by shared contracts:

- S03 economic content and S05 world/placement can overlap after S02, but S06 requires both.
- Within S07–S10, UI composition can follow one prompt behind its C++ presentation model and semantic contract; do not let UI invent duplicate formulas.
- S12 worker infrastructure can begin after S02-P03 if staffing permits, but OpenAI proposal acceptance depends on stable schemas and real definitions from S03/S10.
- Provider adapters in S13 may be developed in parallel after S12-P01 and S13-P01 contracts are fixed.
- S14 tasks may run in parallel for discovery, but fixes must be revalidated by the single final clean-checkout gate.

Avoid parallel edits to the same definition base, command gateway, protocol schema, style tokens or save envelope without an explicitly assigned owner and merge order.

## 20. Backlog and scope-change rule

Anything outside [MVP.md](MVP.md)—including live TRELLIS, skeletal generation/rigging/animation, more than one fully buildable city, advanced route conditions, production multiplayer services, politics/diplomacy depth and full localization—goes into the post-MVP backlog unless it is required to repair an accepted MVP invariant.

When a new gameplay model or field is approved during implementation, add its runtime, editor, schema/migration, validation, query/semantic, fixture/test and documentation work to the same sprint or move the whole feature. Never ship only the runtime half and label the missing parity work as polish.
