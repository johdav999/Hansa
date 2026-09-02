# Hansa — Integrated MVP Scope

## 1. Definition

The Hansa MVP is one playable, authorable, and automatically testable vertical slice. It has three inseparable workstreams:

1. **Game:** a small but complete city-building and regional-trade scenario.
2. **Editor:** the tools required to create, validate, generate, and update that scenario's data and selected assets.
3. **Hybrid testing:** MCP orchestration, semantic UI inspection, native screenshots, deterministic fixtures, gameplay queries, controlled actions, waits, and evidence bundles.

The MVP is not complete if only the game is playable, only the editor can author data, or only headless tests pass. The same feature change must deliver all applicable sides of the contract.

## 2. MVP outcome

A player starts in Lübeck, builds a small settlement and production economy, supplies citizen needs, observes local prices respond to supply and demand, and creates sea and land trade routes with Hamburg, Lüneburg, and Rostock. One AI merchant competes in the same markets. A compact research tree provides meaningful improvements. The scenario ends when Lübeck recovers from a grain shortage and reaches a defined prosperity target.

A designer can author the relevant goods, recipes, buildings, needs, cities, technologies, and scenario values inside Hansa Authoring Studio. OpenAI can propose schema-constrained changes that are reviewed and simulation-tested before application. One generated 3D prop and a small audio pair prove the staged media pipeline.

Codex or another approved MCP client can load the same scenario deterministically, inspect and operate its UI semantically, query the underlying simulation, capture correlated native-resolution screenshots, wait for conditions, assert results, and produce a reproducible evidence bundle.

## 3. Scope guardrail

The MVP is a proof of the complete Hansa loop, not a miniature version of every final system.

| Area | MVP depth |
| --- | --- |
| Geography | Four connected cities; Lübeck is fully buildable, three are simulated trade markets |
| Economy | Ten goods and four representative production chains |
| Population | Two citizen tiers with needs, workforce, residence upgrade and demand |
| Building | One compact city region, roads, logistics, housing, civic/logistics and production buildings |
| Trade | One sea route pattern and one land route pattern with automation rules |
| Market | Local stock, reserve, citizen/industrial demand, incoming supply, price factors and history |
| AI | One server-authoritative merchant rival using normal market/route commands |
| Research | Three branches and approximately nine meaningful technologies |
| Events | Season/winter modifier plus one scripted shortage scenario |
| Victory | One scenario victory with transparent progress |
| Multiplayer | Technical two-client authority/join proof; no production lobby or diplomacy UX |
| Editor | Schema-driven authoring for all MVP definitions plus specialized economic/research views |
| AI generation | OpenAI data proposals, one Tripo static prop, one ElevenLabs SFX and one speech line |
| Testing | Full hybrid MCP/semantic/screenshot workflow for the golden shortage scenario |

If a proposed feature does not directly improve this loop or reduce a top architectural risk, it belongs after the MVP.

## 4. Game MVP

### 4.1 World and cities

- **Lübeck:** one fully buildable World Partition region with harbor, roads, building plots, resource locations, camera bounds and placement grid.
- **Hamburg:** simulated port market with fixed background production/demand and a sea-route connection.
- **Lüneburg:** simulated inland salt market with a land-route connection.
- **Rostock:** simulated Baltic port market with a sea-route connection.
- A compact regional map, not the full European campaign map.
- Route graph supports sea and land edges, distance, capacity, time and seasonal modifier.
- River-specific vehicle/navigation rules and the full Europe map are deferred.

The three non-buildable cities still run the same market records and economic rules. Their fixed background production replaces local construction only for this milestone.

### 4.2 Goods and production chains

MVP goods:

1. grain;
2. flour;
3. bread;
4. fish;
5. salt;
6. timber/logs;
7. planks;
8. iron;
9. tools;
10. beer.

Representative chains:

```text
Grain → Mill → Flour → Bakery → Bread
Timber → Sawmill → Planks
Iron → Smithy → Tools
Grain + water/fuel abstraction → Brewery → Beer
Fish and salt remain direct market/need/trade goods in the MVP
```

The data model must support multi-input recipes even when some MVP recipes remain simple. Quality tiers, detailed spoilage, barrels/packaging, substitutions and advanced by-products are deferred.

### 4.3 Buildings and city building

Required buildable types:

- road;
- laborer residence;
- artisan residence/upgrade state;
- market;
- warehouse;
- dock/harbor connection;
- grain farm;
- mill;
- bakery;
- fishery;
- lumber camp;
- sawmill;
- smithy/tool workshop;
- brewery.

Required mechanics:

- grid/shore/road placement validation;
- construction cost and construction completion;
- road connection and simple service/logistics reachability;
- warehouse inventory and reservations;
- aggregated cart/delivery jobs rather than authoritative individual citizens;
- workforce allocation by tier;
- building inputs, output, utilization and explicit blocker cause;
- residence needs, satisfaction and manual upgrade;
- demolition with confirmation; relocation and blueprints are deferred.

### 4.4 Population

Two tiers:

- **Laborers:** consume bread, fish, beer and basic services; provide basic workforce.
- **Artisans:** add tools and stronger beer/bread/service expectations; provide skilled workforce.

Population is represented as cohorts/households. The UI exposes population, workforce, consumption, reserve days, access, affordability and satisfaction. Merchants, patricians, clergy/institutions, disease and detailed migration are deferred.

### 4.5 Local market simulation

Each city tracks per good:

- current stock and warehouse availability;
- desired reserve;
- recent local production;
- citizen demand;
- industrial demand;
- confirmed incoming supply;
- unmet demand;
- current price, recent average and bounded history;
- season and city modifiers;
- causal price factors used by UI and tests.

MVP price behavior:

- fixed-point deterministic calculation;
- bounded movement per simulation step;
- scarcity versus reserve;
- citizen and industrial demand pressure;
- expected incoming supply influence;
- winter/shortage scenario modifiers;
- large purchases raise price and large sales lower it over time.

Background trade is deterministic and limited. Credit, insurance, tariffs, privileges, embargoes, order books and speculation are deferred.

### 4.6 Trade and routes

- One player cog with cargo capacity, speed, upkeep and route state.
- One wagon route abstraction with capacity, travel time and upkeep.
- Route stops with load, unload, quantity cap and minimum reserve.
- Simple route schedule, arrival, loading/unloading and cargo transfer.
- Expected travel time, capacity use and approximate profit preview.
- Winter can delay a sea route through a deterministic modifier.
- No piracy, combat, convoy, insurance, conditional price rules or multimodal transshipment.

### 4.7 AI rival

One merchant AI:

- observes only allowed market knowledge;
- evaluates shortages and price margins;
- chooses from a bounded set of trade opportunities;
- submits normal buy/sell/route commands;
- owns money, inventory and one abstract/visible trade vehicle;
- has no free goods and cannot write market state directly;
- records its selected goal, considered options and reason for automation queries.

AI city construction, diplomacy, politics and multiple personalities are deferred.

### 4.8 Research

Approximately nine technologies across three branches:

- **Commerce:** better market reports, lower transaction friction, reserve automation.
- **Production:** improved milling, sawmilling and smithing efficiency.
- **Logistics:** warehouse handling, cart capacity and route scheduling.

Each node has stable ID, prerequisite, cost/time, effect and visible unlock explanation. Research uses a queue of one and server-authoritative progress. Mutually exclusive branches and the full six-branch tree are deferred.

### 4.9 Scenario and victory

Golden scenario: `lubeck_grain_shortage_v1`.

Starting state:

- Lübeck has low grain/bread reserve and rising prices;
- local production cannot recover quickly enough without player action;
- Lüneburg and/or Rostock has exportable supply or enables a supporting route decision;
- the player has enough resources to build a small chain and establish trade;
- the AI may compete for part of the available supply.

Victory requires all of the following for a sustained number of ticks:

- bread reserve above the defined safety threshold;
- staple affordability below the defined maximum;
- target population and artisan count;
- positive operating cash flow;
- one active sea route and one active land route;
- no unresolved critical shortage.

Defeat occurs after sustained insolvency with no assets or viable production. Multiple victory types and recovery finance are deferred.

### 4.10 Save/load and multiplayer proof

- Manual save and one rotating autosave slot.
- Versioned authoritative snapshot with seed, tick, RNG and definition hash.
- Save/load round trip preserves the determinism checksum.
- Single-player uses the same validated command path as multiplayer.
- One host/dedicated-server test plus two clients can join the scenario, issue authorized commands and observe scoped projections.
- The proof covers private/public data, invalid ownership rejection and late join snapshot/delta catch-up.
- Session browser, invites, reconnect UX, teams, chat and platform integration are deferred.

## 5. Game UI MVP

### 5.1 Screens and components

| Screen/component | MVP responsibility |
| --- | --- |
| Main HUD | Money/trend, population/workforce, selected city, date/season, pause/speed, research summary, alerts |
| Build menu | Categories, building cards, cost, workforce, prerequisites and placement state |
| Context inspector | Identity, production/needs, blocker cause, inventory and applicable actions |
| City overview | Population, production and market summary tabs |
| Market | Goods table, stock/reserve/demand/price/trend, causal selected-good panel |
| Route editor | Stops, load/unload, quantity, minimum reserve, time/capacity/profit preview |
| Research | Three branches, prerequisites, effects, queue and progress |
| Scenario/victory | Objectives, progress, success and defeat result |
| Save/load | Named manual save, autosave state, validation error |

Implementation uses native UMG/Slate components, C++ view models and event-driven updates. It follows `Docs/UIDesignBrief.md` and `Docs/UIAssetWorkflow.md`. Full-screen generated mockups never become interactive shipping UI, and raster assets are never resampled.

### 5.2 MVP usability acceptance

- A new tester can discover why bread price is rising within 30 seconds.
- A tester can place the bread production chain without opening external documentation.
- A tester can create the shortage-relief route using the simple route editor.
- Every invalid placement or command provides cause and remedy.
- Mouse/keyboard is complete; controller navigation is implemented for the main golden path but may omit secondary editor/debug surfaces.
- Critical information is not conveyed by color alone.
- MVP layout is verified at native 1280×720 and 1920×1080 captures without raster resampling.

## 6. Editor MVP workstream

The detailed editor scope and acceptance tests are defined in [EditorMVP.md](EditorMVP.md). Within the integrated MVP, the editor must cover every game definition required by the playable slice.

### 6.1 Required authoring coverage

Specialized Authoring Studio support:

- goods;
- recipes;
- buildings;
- technologies and editable research graph.

Generic schema-driven details support:

- needs/population tier;
- city/market background profile;
- route/vehicle basics;
- scenario and victory thresholds;
- AI merchant tuning;
- presentation asset references.

All types share stable IDs, schema version, authoring metadata, validation, deterministic JSON Schema export, diff/patch paths and migration classification.

### 6.2 AI-assisted authoring included

- OpenAI structured proposal/diff/apply for a bounded definition or related set.
- Temporary registry compilation and targeted fixture run before Apply.
- One Tripo-generated static harbor prop through staging and promotion.
- One ElevenLabs SFX and one single-speaker line through staging and promotion.
- Provider-neutral job state, provenance, cost confirmation, rights acknowledgement and mocks.
- TRELLIS live integration, skeletal generation, animation and multi-speaker dialogue remain post-MVP.

## 7. Hybrid MCP and UI-testing MVP

### 7.1 Components

- Separate `HansaAutomation` `DeveloperTool` Unreal module.
- External local `Tools/HansaMcp` sidecar using MCP over STDIO for Codex.
- Versioned named-pipe or explicitly enabled loopback transport between sidecar and game.
- `UHansaAutomationSubsystem` coordinating fixtures, semantic UI, queries, actions, waits and evidence.
- Automation disabled by default and absent from Shipping.

### 7.2 Minimum MCP tool surface

| Category | MVP tools |
| --- | --- |
| Session | `capabilities_get`, `session_get`, `session_start`, `session_stop` |
| Fixtures | `fixture_list`, `fixture_load`, `fixture_reset` |
| Simulation | `simulation_step`, `simulation_run_until`, `gameplay_query` |
| Semantic UI | `ui_find`, `ui_state`, `ui_activate`, `ui_set_value`, `ui_scroll` |
| Evidence | `capture_screenshot`, `logs_get`, `evidence_bundle_create` |
| Synchronization | `wait_for`, `assert_state`, `test_run` |

`ui_tree` may exist for diagnostics, but production tests use stable semantic IDs rather than tree position, localized text or pixel coordinates.

### 7.3 Semantic UI coverage

Every interactive or information-bearing control in the golden path exposes:

- stable namespaced semantic ID;
- role and localized label key;
- visible, enabled, focused, selected, loading, warning and error state as applicable;
- pixel bounds and clipping state;
- current semantic value and unit where applicable;
- allowed semantic actions;
- parent/child or label relationship;
- frame, UI revision and correlated simulation tick.

Required namespaces:

```text
HUD.*
BuildMenu.*
Placement.*
Inspector.*
CityOverview.*
Market.*
TradeRoute.Editor.*
Research.*
Scenario.*
SaveLoad.*
```

Coordinate clicks are diagnostic only and do not satisfy MVP test coverage.

### 7.4 Gameplay query coverage

- session, scenario, seed, tick, checksum, pause and speed;
- player/house money, authorized inventory and public/private visibility;
- buildings, placement result, construction, production, inputs, output and blockers;
- population, workforce, needs, consumption, satisfaction and reserve days;
- city market stock, reserve, demand, incoming supply, price/history and causal factors;
- vehicle, cargo, stops, route state, ETA and transfer results;
- AI current goal, known facts, chosen opportunity and accepted command;
- research prerequisites, queue, progress and applied effects;
- scenario objectives, victory progress and defeat state;
- save identity, format version, tick and round-trip checksum.

Queries are typed and read-only. They return stable IDs, raw values and units. They do not expose mutable containers.

### 7.5 Controlled actions

Automation may:

- open/close/select UI through normal presenter intent;
- place a building through placement intent and the normal command gateway;
- create/edit/activate a route through the normal command gateway;
- queue research;
- change pause/speed under scenario policy;
- save/load through the normal save subsystem;
- advance explicit simulation ticks in a fixture/test session.

It may not directly set money, stock, price, ownership, research completion or victory. Fixture setup may establish initial state only before play begins.

### 7.6 Native screenshot evidence

- Capture full viewport or semantic region by stable ID.
- MVP resolutions: native 1280×720 and 1920×1080.
- Never resize or resample a screenshot to satisfy another target.
- Record fixture, tick, frame, UI revision, viewport, UI scale, map, screen and screenshot hash.
- Correlate screenshots with semantic and gameplay snapshots.
- Use structured assertions for deterministic facts; screenshots judge layout, clipping, readability, focus, styling and visual regressions.

### 7.7 Deterministic fixtures

| Fixture | Purpose |
| --- | --- |
| `empty_lubeck_build_v1` | Placement, construction, road connection and build UI |
| `lubeck_grain_shortage_v1` | Complete market/production/trade/research/victory golden path |
| `route_delivery_v1` | Sea/land loading, travel, unloading, inventory and price effect |
| `save_roundtrip_v1` | Save/load checksum and UI restoration |
| `two_player_authority_v1` | Ownership, private data, rejection and late join |

Every fixture records schema version, content hash, seed, initial tick, expected checkpoints and final checksum.

### 7.8 Golden end-to-end MCP test

```text
Start explicitly automation-enabled Development game
  → load lubeck_grain_shortage_v1
  → inspect Market.Grain semantically
  → query grain stock/demand/price causes
  → capture native market screenshot
  → build or confirm local bread chain
  → create import route using semantic actions
  → advance/run until delivery condition
  → assert cargo, warehouse, reserve and price movement
  → queue one relevant technology
  → run until objective thresholds remain satisfied
  → assert victory
  → save, reload and compare checksum
  → write evidence bundle
```

This test is the integrated MVP release gate, not a demonstration script maintained separately from CI.

## 8. Cross-workstream parity contract

Every implemented MVP feature must satisfy the applicable columns:

| Feature | Game | Editor | Automation | Evidence/test |
| --- | --- | --- | --- | --- |
| Good/recipe | Runtime definition and simulation | Typed editing and validation | Query by stable ID | Conservation/ratio fixture |
| Building | Placement/production | Definition and asset reference editing | Semantic placement + state query | Valid/invalid placement screenshots |
| Need/population | Cohort consumption/satisfaction | Generic typed editing | Need/workforce queries | Shortage and recovery assertions |
| Market | Supply/demand/price history | City/good tuning | Market causal query + semantic screen | Correlated values and screenshots |
| Route | Vehicle/cargo/transfers | Route/vehicle basics | Semantic editor + controlled commands | Delivery fixture and evidence |
| Research | Prerequisites/effects | Editable graph | Query/queue/wait | Reachability and applied-effect test |
| AI rival | Legitimate query + commands | Tuning fields | Goal/decision query | Deterministic opportunity test |
| Victory | Objective evaluation | Threshold editing | Progress query | Golden scenario completion |
| Save/load | Versioned snapshot | Migration classification | Controlled save/load | Checksum round trip |
| UI screen | Native UMG/Slate | Applicable preview/reference picker | Stable semantics | Native screenshots and focus states |

A feature cannot be marked complete while one required column is deferred.

## 9. Integrated demo and acceptance path

### 9.1 Authoring phase

1. Open Hansa Authoring Studio.
2. Edit `Good.Grain`, a bread recipe and one research node.
3. Request a bounded OpenAI proposal, reject one field and accept another.
4. Compile a temporary registry and run affected validation/fixture checks.
5. Apply through an undoable transaction and compile the accepted registry.
6. Review the promoted Tripo harbor prop and ElevenLabs audio provenance.

### 9.2 Play phase

1. Start the Lübeck scenario.
2. Place roads, residences, warehouse/market/dock and a bread chain.
3. Observe workforce, inputs, citizen consumption and price causes.
4. Create one sea and one land route.
5. Choose one research improvement.
6. React to AI competition and winter delay.
7. Satisfy victory thresholds.
8. Save, reload and verify restored state.

### 9.3 Automated phase

1. Repeat the golden scenario through MCP and semantic actions.
2. Assert the same simulation facts through gameplay queries.
3. Capture correlated native screenshots at required states.
4. Produce an evidence bundle containing commands, queries, semantics, screenshots, logs and checksums.
5. Run the two-client authority fixture.
6. Package Shipping and prove all editor, MCP, automation, worker, credential and staging components are absent.

## 10. MVP acceptance gates

### 10.1 Gameplay

- The scenario can be completed from a clean start without cheats or direct state mutation.
- The same seed and command stream produce the same checksum.
- Goods and money conservation invariants pass.
- Local prices respond in the expected direction and remain bounded.
- The AI uses normal commands and resources.
- Save/load preserves the authoritative checksum.
- Two clients do not diverge or access unauthorized private data in the technical proof.

### 10.2 Editor

- All MVP definitions can be authored without editing C++ values or raw asset serialization.
- A new reflected field receives generic editing/schema support automatically and missing metadata fails CI.
- OpenAI output cannot bypass schema, revision, reference, range or domain validation.
- Generated media cannot bypass staging, provenance, QA and approval.
- Normal CI uses provider mocks and spends no credits.

### 10.3 UI and automation

- Every golden-path action has a stable semantic target.
- Displayed authoritative values agree with correlated gameplay queries.
- Tests wait on state/tick conditions, not arbitrary sleeps.
- Native 1280×720 and 1920×1080 captures have no critical clipping or overlap.
- A failed end-to-end test produces sufficient evidence to reproduce the failure.
- Automation-disabled Development play opens no endpoint and incurs no per-frame automation work.

### 10.4 Release boundary

- Shipping contains no `HansaEditor`, `HansaAutomation`, `HansaMcp`, `HansaGenerationWorker`, provider SDK/configuration/credentials, staging asset or QA-only fixture.
- Shipping ignores/rejects development enable flags and opens no editor/automation endpoint.
- The packaged vertical slice launches and completes without any development tool installed.

## 11. Implementation increments

The executable two-week sprint sequence and copy-ready Codex task prompts are maintained in [MVPSprintPlan.md](MVPSprintPlan.md). The increments below remain milestone groupings; the sprint plan is the delivery breakdown.

### Increment 0 — Foundations

- Module/target skeleton: `HansaSimulation`, `Hansa`, `HansaEditor`, `HansaAutomation`, `HansaTests`.
- Stable IDs, fixed-point types, deterministic clock/RNG, definition base/registry and command pipeline.
- Minimal Authoring Studio schema discovery.
- Minimal automation session/fixture/query handshake and external MCP sidecar.
- Shipping exclusion checks.

Exit: an empty deterministic simulation can be authored, launched, queried through MCP and packaged without development tooling.

### Increment 1 — Headless economy and data authoring

- Ten goods, recipes, buildings, needs, four city-market records and shortage fixture.
- Production, population, inventory, market and price systems.
- Authoring/validation for the corresponding definitions.
- Gameplay queries, fixture control, stepping, invariants and checksum evidence.

Exit: the shortage and recovery can run deterministically headless from editor-authored data.

### Increment 2 — Buildable Lübeck and core UI

- Map/region, camera, placement, roads, housing, production, logistics and presentation actors.
- HUD, build menu, inspector, city overview and market UI.
- Semantic IDs/actions, UI waits and native screenshots for each implemented state.

Exit: a player and MCP test can build the bread chain and diagnose the shortage.

### Increment 3 — Trade, research, AI and victory

- Cog/wagon routes, route editor, three-branch research, one AI merchant and scenario victory.
- Editor graph/tuning coverage and cross-definition validation.
- Route/research/AI/victory queries, actions, fixtures and evidence.

Exit: the complete golden scenario passes manually and through MCP before save/multiplayer/provider work.

### Increment 4 — Save, multiplayer proof and AI authoring/media

- Save/load round trip and two-client authority fixture.
- OpenAI proposal workflow and balance check.
- Tripo static prop and ElevenLabs SFX/speech staging/promotion.
- Worker recovery, provenance, cost and rights checks.

Exit: accepted editor changes/media appear in the game, save/multiplayer checks pass, and live-provider work remains replaceable and non-shipping.

### Increment 5 — Integrated hardening

- Performance/soak, accessibility, controller golden path and both screenshot resolutions.
- Complete evidence bundles, clean-checkout demo and failure recovery.
- Cook/package and Shipping exclusion audit.
- Documentation for build, editor, credentials, live smoke tests and test execution.

Exit: every acceptance gate in this document passes from a clean checkout.

## 12. Explicitly out of scope

### Game

- full Europe map or more than one fully buildable city;
- more than ten goods, two population tiers or three research branches;
- politics, Hanseatic assembly, diplomacy, contracts, loans, insurance and bankruptcy recovery;
- combat, piracy, convoys, detailed weather, fire, disease or crime;
- quality tiers, detailed spoilage, packaging/barrels and advanced conditional orders;
- full AI city construction or more than one AI rival;
- production multiplayer lobby, matchmaking, invites, reconnect UI, teams, chat or platform services;
- multiple victory paths, campaign meta-progression, tutorial campaign or mod support.

### Editor and generation

- live TRELLIS generation;
- skeletal generation, auto-rigging, animation generation or retargeting;
- multi-speaker dialogue, voice cloning and localization batches;
- full DCC mesh/audio editing;
- unattended AI changes or automatic balance optimization;
- multi-user merge tooling, cloud worker farm or runtime/player generation.

### Testing

- unrestricted console, reflection, filesystem or arbitrary code execution;
- pixel-coordinate-first gameplay tests;
- exhaustive visual baselines for every resolution, language and accessibility mode;
- live billable provider calls in ordinary CI;
- load/performance targets for the full 30–50-city campaign.

## 13. Post-MVP priorities

1. Make Hamburg buildable and extend the regional map.
2. Add live TRELLIS as the second 3D provider.
3. Add canonical human skeleton, rigging/retargeting and dockworker animation slice.
4. Expand production chains, population tiers and specialized editor views.
5. Add full multiplayer sessions/reconnect and additional AI houses.
6. Add multi-speaker dialogue/localization and broader audio production.
7. Add contracts, credit, privileges, politics and additional victory paths.
8. Expand toward the early-access city/goods/map target only after profiling.

## 14. MVP definition of done

The Hansa MVP is complete only when:

1. the integrated authoring, play and automated demo succeeds from a clean checkout;
2. the golden scenario is winnable manually and through the controlled automation surface;
3. deterministic, save/load and two-client authority checks pass;
4. all implemented features satisfy the game/editor/automation parity contract;
5. native screenshot and semantic evidence confirms the critical UI states;
6. generated data/media is traceable, validated and explicitly approved;
7. normal CI performs no billable provider calls;
8. the Shipping build is playable and contains none of the development-only systems.
