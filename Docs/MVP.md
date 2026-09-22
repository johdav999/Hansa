# Hansa — Enhanced Playable MVP Scope

## 1. Definition

The Hansa MVP is one playable, authorable, and automatically testable 30–60 minute vertical slice. It has three inseparable workstreams:

1. **Game:** a small but complete city-building and regional-trade scenario.
2. **Editor:** the tools required to create, validate, generate, and update that scenario's data and selected assets.
3. **Hybrid testing:** MCP orchestration, semantic UI inspection, native screenshots, deterministic fixtures, gameplay queries, controlled actions, waits, and evidence bundles.

The MVP is not complete if only the game is playable, only the editor can author data, or only headless tests pass. The same feature change must deliver all applicable sides of the contract. Visual or interactive requirements require evidence from the real assembled game at native resolution; headless state, synthetic test surfaces, source-art renders, and UI mockups cannot by themselves satisfy those requirements.

## 2. MVP outcome

A player starts through the production frontend in Lübeck and plays a non-prescriptive 30–60 minute session. Lübeck is fully buildable. Rostock is a second fully rendered coastal Hanseatic city that is prebuilt, inspectable, and tradeable, but not player-buildable. The player builds the bread, fish, planks, beer, and firewood production chains, supplies citizen needs, observes all fifteen MVP goods respond to supply and demand, and establishes a Lübeck–Rostock sea route. The route's Cog and local cargo movement are visible in the world and correspond to authoritative simulated cargo.

Construction is a tactile player flow: building selection previews under the mouse and click/held-stroke input constructs in the world, placement uses the authored 3D model as its ghost, roads are drawn directly across the terrain, and every valid or invalid state explains itself without relying on color alone. Every visible golden-path building, vehicle, road piece, recurring prop, and player-facing screen uses approved production presentation rather than Engine primitives, test controls, or unverified staging content.

One AI merchant may compete in the same markets and a compact research tree provides meaningful improvements. The session offers several viable emphases across local production, population growth, and trade, with recovery from poor choices and no required build order. Completion is expressed through transparent prosperity milestones, not a prescribed automation script.

A designer can author the relevant goods, recipes, buildings, needs, cities, technologies, and scenario values inside Hansa Authoring Studio. OpenAI can propose schema-constrained changes that are reviewed and simulation-tested before application. One generated 3D prop and a small audio pair prove the staged media pipeline.

Codex or another approved MCP client can load the same scenario deterministically, inspect and operate its UI semantically, query the underlying simulation, capture correlated native-resolution screenshots, wait for conditions, assert results, and produce a reproducible evidence bundle.

## 3. Scope guardrail

The MVP is a proof of the complete Hansa loop, not a miniature version of every final system.

| Area | MVP depth |
| --- | --- |
| Geography | Two rendered coastal cities: buildable Lübeck and prebuilt, inspectable, tradeable Rostock |
| Economy | Fifteen active market goods; bread, fish, planks, beer, and firewood are the locally buildable production chains |
| Population | Two citizen tiers with needs, workforce, residence upgrade and demand |
| Building | Click-select and held-stroke building placement with authored 3D ghosts, player-drawn roads, local logistics, housing, civic/logistics and three chain families |
| Trade | One complete Lübeck–Rostock sea-route journey with a visible Cog carrying simulated cargo |
| Market | Local stock, reserve, citizen/industrial demand, incoming supply, price factors and history |
| AI | One server-authoritative merchant rival using normal market/route commands |
| Research | Three branches and approximately nine meaningful technologies |
| Events | Season/winter modifier plus one scripted shortage scenario |
| Session | A recoverable 30–60 minute session with several viable approaches and transparent prosperity milestones |
| Multiplayer | Technical two-client authority/join proof; no production lobby or diplomacy UX |
| Editor | Schema-driven authoring for all MVP definitions plus specialized economic/research views |
| AI generation | OpenAI data proposals, one Tripo static prop, one ElevenLabs SFX and one speech line |
| Testing | Full hybrid state, semantic, input, real-viewport, playthrough and package evidence for the enhanced golden session |

The separate post-MVP 2–8-player release is defined by [MultiplayerImplementationPrompts.md](MultiplayerImplementationPrompts.md) and [Development/Multiplayer/ReleaseContract.md](Development/Multiplayer/ReleaseContract.md), with independent evidence in [Development/Multiplayer/Acceptance.md](Development/Multiplayer/Acceptance.md). It does not change or retroactively satisfy the historical MVP gates below.

If a proposed feature does not directly improve this loop or reduce a top architectural risk, it belongs after the MVP.

The implementation state inspected when this enhanced scope was adopted is recorded in [Development/EnhancedMvpBaseline.md](Development/EnhancedMvpBaseline.md). That baseline is descriptive; the requirement-to-evidence matrix in §10.1 is normative.

## 4. Game MVP

### 4.1 World and cities

- **Lübeck:** one fully buildable World Partition region with harbor, roads, building plots, resource locations, camera bounds and placement grid.
- **Rostock:** one fully rendered, prebuilt coastal city presentation with a recognizable waterfront, market/warehouse/dock context, camera transition, selection and inspection. Rostock runs the same fifteen-good market contracts as Lübeck, supports route stops and visible cargo arrival, and exposes no construction controls.
- A compact Baltic map and route connection between Lübeck and Rostock, not the full European campaign map.
- The route graph retains provider-neutral sea/land extension contracts, but only the Lübeck–Rostock sea journey is required for playable-MVP acceptance.
- Existing Hamburg and Lüneburg market records may remain for compatibility and non-gating tests, but they are not required player-facing cities in this slice. Additional cities, a player-facing land-route journey, river navigation, and the full Europe map are post-MVP.

### 4.2 Goods and production chains

MVP goods:

1. grain;
2. flour;
3. hops;
4. malt;
5. bread;
6. fresh fish;
7. salt;
8. timber/logs;
9. planks;
10. iron;
11. tools;
12. barrels;
13. beer;
14. firewood;
15. preserved fish.

Locally buildable chains:

```text
Grain Farm → Grain → Mill → Flour → Bakery → Bread
Fishery → Fresh fish
Fishery + Salting Shed + imported salt + cooperage barrels → Preserved fish
Lumber Camp → Timber/Logs → Sawmill → Planks
Grain Farm → Grain → Malt House → Malt ┐
Hop Farm → Hops ───────────────────────┼→ Brewery → Beer
Lumber Camp → Timber → Cooperage → Barrels ┘
Lumber Camp → Timber → Woodcutter’s Yard → Firewood → Heating / Bakery / Malt House / Brewery
```

All fifteen goods remain active in market stock, reserve, production/consumption history, demand, price, incoming cargo, needs, or industrial use. Salt, iron, and tools use explicit bounded starting stock, Rostock/background supply, imports, or consumption rules; they do not require player-buildable local chains in this milestone. Beer is locally produced from malt, hops, and empty barrels. Generic grain is shared by bread and malt production, while timber is shared by planks, barrel production and firewood. Firewood supplies seasonal household heating and year-round process heat in bakeries, malt houses and breweries. The accepted preservation catalog v25 contains fourteen goods; firewood adds the fifteenth when its separately approved promotion completes. This combined scope statement is not evidence of completed release gates. The existing smithy definition remains supported data but is not offered as a Lübeck construction choice in the enhanced golden session.

The data model must continue to support multi-input recipes even when some MVP recipes remain simple. Preserved fish extends the existing fish chain through the optional Salting Shed upgrade. Need.Fish accepts fresh fish first and preserved fish second at equal edible value. Deterministic aggregate spoilage applies to these two goods; detailed ages, quality tiers, barrel returns, additional chain families and advanced by-products remain deferred.

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
- hop farm;
- malt house;
- cooperage;
- brewery;

Required mechanics:

- data-driven building cards grouped by category and chain;
- mouse drag from a building card into the world, plus click, keyboard, and controller alternatives;
- the authored production 3D model as the placement ghost, aligned to grid, rotation, footprint, terrain and shore rules;
- valid, warning, and invalid placement feedback through material/outline, footprint shape, cursor and concise cause/remedy text;
- grid/shore/road placement validation;
- construction cost and construction completion;
- direct world road drawing with live connected preview, total cost, per-cell validity, cancellation and predictable continuation;
- road connection and simple service/logistics reachability;
- warehouse inventory and reservations;
- deterministic aggregated cart/delivery jobs with visible read-only cart/wagon projections where cargo movement is shown;
- workforce allocation by tier;
- building inputs, output, utilization and explicit blocker cause;
- residence needs, satisfaction and manual upgrade;
- demolition with confirmation; relocation and blueprints are deferred.

Every golden-path building and road piece must have an approved production presentation at authored world scale. Missing presentation is a release error; Engine basic-shape fallback is permitted only on explicit developer/test surfaces that cannot enter the playable or Shipping path.

### 4.4 Population

Two tiers:

- **Laborers:** consume bread, fish, beer, seasonal firewood heating and basic services; provide basic workforce.
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

- One player Cog with cargo capacity, speed, upkeep and route state.
- Route stops with load, unload, quantity cap and minimum reserve.
- Simple route schedule, arrival, loading/unloading and cargo transfer.
- Expected travel time, capacity use and approximate profit preview.
- Winter can delay a sea route through a deterministic modifier.
- The player can create, name, validate, activate, pause, edit and cancel the Lübeck–Rostock route through the normal production UI without debug commands.
- A world Cog projection shows departure, travel, berth, loading and unloading from the authoritative route projection. Visible cargo cues and inventory/market events must agree.
- Local cargo carts/wagons are visible where the playable presentation shows local delivery; Actor transforms and animation remain non-authoritative projections.
- No piracy, combat, convoy, insurance, conditional price rules or multimodal transshipment.
- A player-facing intercity land-route workflow is deferred, although the generic simulation contract may retain land-mode coverage.

### 4.7 AI rival

One merchant AI:

- observes only allowed Lübeck/Rostock market knowledge;
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

Enhanced golden session: the versioned playable profile of `lubeck_grain_shortage_v1`, or a deliberately versioned successor when balance/content changes require it.

Starting state:

- Lübeck has low grain/bread reserve and rising prices;
- local production cannot recover quickly enough without player action;
- Rostock has exportable supply and supports a meaningful sea-route decision;
- the player has enough resources to build a small chain and establish trade;
- the AI may compete for part of the available supply.
- the map has enough viable space and capital for different openings and recoverable mistakes.

The session exposes transparent prosperity milestones and at least three viable strategic emphases across local production, population growth, and trade. It must not require one exact build order. The enhanced golden acceptance journey demonstrates all of the following, while scenario completion may support more than one ordering or weighting:

- bread reserve above the defined safety threshold;
- staple affordability below the defined maximum;
- target population and artisan count;
- positive operating cash flow;
- one functioning Lübeck–Rostock sea route with an observed delivery;
- no unresolved critical shortage.

Defeat occurs after sustained insolvency with no assets or viable production. Poor but viable choices provide causal feedback and a recovery path. A tutorial checklist, mandatory exact sequence, multiple campaign victory modes, and recovery finance are deferred.

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
| Frontend/system shell | Production title/start, New Game, Continue, save/load, settings, confirmation, loading, results and credits/legal placeholder |
| Main HUD | Money/trend, population/workforce, selected city, date/season, pause/speed, research summary, alerts and contextual inspector host while keeping the city dominant |
| Build menu | Data-driven categories and chain expansion, draggable building cards, cost, workforce, prerequisites, lock/availability and placement state |
| World placement | Authored 3D ghost, footprint cells, cursor/reason feedback, rotation, repeat, cancellation and non-drag alternatives |
| Road tool | Press/click start, live Manhattan path preview, total cost, per-cell validity, release-to-submit and accessible alternatives |
| Context inspector | Identity, production/needs, blocker cause, inventory and applicable actions |
| City overview | Population, production and market summary tabs for Lübeck plus inspectable non-buildable Rostock state |
| Market | Goods table, stock/reserve/demand/price/trend, causal selected-good panel |
| Trade map/route creator | Create and edit the Lübeck–Rostock route; stops, Cog, load/unload, quantity, minimum reserve, time/capacity/upkeep/profit/risk preview |
| Research | Three branches, prerequisites, effects, queue and progress |
| Scenario/onboarding/results | Concise context, dismissible contextual teaching, prosperity progress, recovery, success and defeat result without a prescribed checklist |
| Save/load | Named manual save, autosave state, validation error |

Implementation uses native UMG/Slate components, C++ view models and event-driven updates. It follows `Docs/UIDesignBrief.md` and `Docs/UIAssetWorkflow.md`. Full-screen generated mockups never become interactive shipping UI, and raster assets are never resampled.

### 5.2 MVP usability acceptance

- A new tester can discover why bread price is rising within 30 seconds.
- A first-time player can launch the packaged build, start the slice and recover from ordinary mistakes without developer instructions.
- A tester can place and operate the bread, fish, planks, beer, and firewood chains without opening external documentation.
- A tester can drag a building card into the real world, understand every warning/invalid state, rotate/cancel/repeat, and complete the same action without drag.
- A tester can draw a road from the world viewport, understand invalid spans and cost, and connect production to storage without debug controls.
- A tester can create a delivering Lübeck–Rostock route using the production route creator and observe the same simulated cargo on the visible Cog and in both city inventories.
- Every invalid placement or command provides cause and remedy.
- Mouse/keyboard and controller are complete for the enhanced golden path; drag actions have click/select alternatives.
- Critical information is not conveyed by color alone.
- MVP layout is verified at native 1280×720 and 1920×1080 captures without raster resampling.
- Player-facing screens share the approved Hansa component system and include default, focus, disabled, loading, warning, error and empty states where applicable.
- Development fixtures, test-helper controls and automation scenarios are absent from the normal player journey.

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
| playable `lubeck_grain_shortage_v1` profile or versioned successor | Complete non-prescriptive market/production/population/trade/research/prosperity path in the real Lübeck and Rostock presentation |
| `route_delivery_v1` or versioned successor | Lübeck–Rostock Cog loading, travel, visible arrival/unloading, inventory and price effect |
| `save_roundtrip_v1` | Save/load checksum and UI restoration |
| `two_player_authority_v1` | Ownership, private data, rejection and late join |

Every fixture records schema version, content hash, seed, initial tick, expected checkpoints and final checksum.

### 7.8 Golden end-to-end MCP test

```text
Start explicitly automation-enabled Development game through the production frontend
  → load the enhanced lubeck_grain_shortage playable profile
  → use ordinary build-card, world-placement and road-tool intents
  → construct representative bread, fish, planks, beer and firewood chain buildings in valid and invalid states
  → inspect the fifteen-good market and population causes semantically
  → travel to and inspect rendered Rostock
  → create and activate a Lübeck–Rostock Cog route through the production route creator
  → wait for visible departure, travel, arrival and unloading synchronized to authoritative cargo events
  → assert both markets, needs, warehouse, reserve and price movement
  → queue one relevant technology and reach prosperity through an allowed strategy/order
  → save, reload and compare authoritative and visible state
  → capture real native viewport evidence at required checkpoints
  → write a synchronized evidence bundle
```

This test is one integrated release gate, not a demonstration script maintained separately from CI. Because an automated path can accidentally become prescriptive, it is complemented by real player-flow playthroughs that use different valid openings and recovery decisions.

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

1. Start the enhanced slice through the production frontend with no developer instructions.
2. Drag authored building cards into Lübeck, recover from an invalid placement, and draw connecting roads directly in the world.
3. Build and operate the bread, fish, planks, beer, and firewood chains in a player-chosen order.
4. Observe workforce, needs, growth/decline, inputs, citizen consumption and price causes across all fifteen goods.
5. Inspect rendered Rostock and create a Lübeck–Rostock Cog route through the normal route creator.
6. Watch authoritative cargo depart, travel, arrive and unload; verify both markets respond.
7. Choose a research improvement, react to AI competition/winter, recover from one poor decision, and reach prosperity.
8. Save, reload and verify restored visible and authoritative state.

### 9.3 Automated phase

1. Repeat an enhanced golden journey through ordinary semantic/input actions and the normal command gateway.
2. Assert the same simulation facts through gameplay queries and correlate visible vehicles/buildings with their authoritative entities and cargo.
3. Capture real assembled game-viewport screenshots/video at native 1280×720 and 1920×1080; synthetic flat-color or isolated test surfaces do not qualify for visual rows.
4. Produce an evidence bundle containing input events, commands, queries, semantics, real viewport captures, logs, events and checksums.
5. Run alternative-order and recovery playthroughs plus the two-client authority fixture.
6. Package Shipping and prove all editor, MCP, automation, worker, credential, test-helper, staging and fallback-placeholder components are absent.

## 10. MVP acceptance gates

### 10.1 Enhanced requirement-to-evidence matrix

Every row is release-blocking. `Structured` means typed gameplay projections, commands/events, deterministic hashes, or package inspection. `Semantic/input` means stable UI semantics plus the ordinary mouse/keyboard/controller intent path. `Visual` means original-resolution capture from the real assembled game viewport. A row that requires Visual or Semantic/input evidence cannot pass on headless state, a protocol-faithful fake endpoint, an isolated Slate proof surface, source art, or a generated mockup alone.

| ID | Requirement | Required evidence | Evidence that is insufficient by itself |
| --- | --- | --- | --- |
| EMVP-START | Launch a packaged build, enter the slice through the production frontend, and receive no developer instructions or test affordances | Packaged-build launch log; real-viewport capture/video from frontend to Lübeck; semantic/input trace; package audit | Headless fixture start; editor PIE only; frontend mockup |
| EMVP-WORLD-LUBECK | Lübeck is fully buildable and remains readable before and after construction | Structured placements/occupancy; ordinary input trace; representative real-viewport captures at gameplay zooms; save/load reconstruction | Placement state or Actor-count assertion without rendered inspection |
| EMVP-WORLD-ROSTOCK | Rostock is rendered, prebuilt, inspectable and tradeable, with no construction controls | Real-viewport city/travel/selection captures; semantic inspector state; typed market/route-stop projection; negative construction-control assertion | A `City.Rostock` market record or trade-map marker alone |
| EMVP-CHAINS | Bread, fish, planks and beer are the locally buildable production chains and all can be placed and operated | Data-driven catalog query; ordinary card/placement/road actions; production/inventory events; real-viewport chain-state captures | Recipe simulations or definition assets alone |
| EMVP-MARKET | All fifteen goods participate through explicit stock, supply, demand, consumption, production, reserve, incoming-cargo or price rules | Registry/market validation; deterministic simulations; typed fifteen-good projections; native Market/City Overview captures with unknown/stale treatment | Fifteen definition IDs without active rules; unknown represented as zero |
| EMVP-PLACE | A building card drags into the world using the authored 3D model as its ghost and exposes valid, warning and invalid feedback, rotation, cancellation, repeat and accessible alternatives | Pointer and non-drag input traces; semantic state/reason; accepted/rejected commands; real-viewport captures/video of each critical state | Direct placement command, fixed target button, or flat-color preview alone |
| EMVP-ROAD | The player draws connected roads directly in the world with live path, cost and per-cell validity feedback | Pointer and controller/keyboard traces; preview semantics; authoritative road batch/events; real-viewport valid/invalid/intersection evidence | Headless Manhattan-path tests or debug target buttons alone |
| EMVP-LOGISTICS | Visible Cog and local cargo vehicles project real simulated cargo movement | Vehicle/cargo/route projections and events correlated by stable entity ID and tick; real-viewport departure/travel/arrival/unload and local-delivery captures | Route-state records, UI rows, or cosmetic animation without correlation |
| EMVP-POPULATION | Needs, satisfaction, workforce, growth, decline and laborer-to-artisan progression are understandable and responsive | Deterministic causal projections/events; ordinary inspector/overview/upgrade actions; real-viewport before/after building and UI evidence; save/load | Population totals or headless upgrade success alone |
| EMVP-UI | All player-facing screens use the approved production component system with complete interaction, accessibility, localization and responsive states | Component/state inventory; semantic mouse/keyboard/controller tests; inspected real-viewport 1280×720 and 1920×1080 captures; high-contrast/large-text/reduced-motion checks | Source mockups, isolated widgets, or synthetic screenshot buffers alone |
| EMVP-SESSION | The slice supports a recoverable 30–60 minute session with several viable strategic emphases and no exact required build order | Multiple timed real playthrough records using different openings; recovery playthrough; deterministic accelerated balance evidence; completion telemetry and defect notes | One scripted automation sequence or accelerated headless duration alone |
| EMVP-SAVE | Save/load restores authoritative and visible city, vehicle, route, market, population and UI state | Pre/post checksum and projection equality; ordinary save/load input trace; matched real-viewport checkpoints | Envelope round trip without visible reconstruction |
| EMVP-RELEASE | The clean Shipping package contains the playable content and excludes editor, automation, worker, provider, staging, placeholder and test-only surfaces | Clean-checkout build/cook/package; expanded package audit; packaged launch/completion; real-viewport evidence | Target receipt/executable token scan without a successful cook/package |

Evidence locations and verdicts are tracked in dated release reports. A previously passing foundation test remains useful evidence but does not satisfy a broader enhanced row unless it includes every required evidence class above.

### 10.2 Gameplay

- The scenario can be completed from a clean start without cheats or direct state mutation.
- The same seed and command stream produce the same checksum.
- Goods and money conservation invariants pass.
- Local prices respond in the expected direction and remain bounded.
- The AI uses normal commands and resources.
- Save/load preserves the authoritative checksum.
- Two clients do not diverge or access unauthorized private data in the technical proof.

### 10.3 Editor

- All MVP definitions can be authored without editing C++ values or raw asset serialization.
- A new reflected field receives generic editing/schema support automatically and missing metadata fails CI.
- OpenAI output cannot bypass schema, revision, reference, range or domain validation.
- Generated media cannot bypass staging, provenance, QA and approval.
- Normal CI uses provider mocks and spends no credits.

### 10.4 UI and automation

- Every golden-path action has a stable semantic target.
- Displayed authoritative values agree with correlated gameplay queries.
- Tests wait on state/tick conditions, not arbitrary sleeps.
- Real assembled game-viewport captures at native 1280×720 and 1920×1080 have no critical clipping or overlap.
- A failed end-to-end test produces sufficient evidence to reproduce the failure.
- Automation-disabled Development play opens no endpoint and incurs no per-frame automation work.

### 10.5 Release boundary

- Shipping contains no `HansaEditor`, `HansaAutomation`, `HansaMcp`, `HansaGenerationWorker`, provider SDK/configuration/credentials, staging asset, Engine basic-shape fallback, test-helper UI or QA-only fixture.
- Shipping ignores/rejects development enable flags and opens no editor/automation endpoint.
- The packaged vertical slice launches and completes without any development tool installed.

## 11. Implementation increments

The original foundation sprint sequence and completed prompt record are maintained in [MVPSprintPlan.md](MVPSprintPlan.md). The remaining enhanced playable-slice delivery plan is [MVP-enhanced.md](MVP-enhanced.md). The increments below are architectural milestone groupings, not permission to accept a headless or placeholder presentation as the current MVP.

### Increment 0 — Foundations

- Module/target skeleton: `HansaSimulation`, `Hansa`, `HansaEditor`, `HansaAutomation`, `HansaTests`.
- Stable IDs, fixed-point types, deterministic clock/RNG, definition base/registry and command pipeline.
- Minimal Authoring Studio schema discovery.
- Minimal automation session/fixture/query handshake and external MCP sidecar.
- Shipping exclusion checks.

Exit: an empty deterministic simulation can be authored, launched, queried through MCP and packaged without development tooling.

### Increment 1 — Headless economy and data authoring

- Fifteen goods, recipes, buildings, needs, the required Lübeck/Rostock market records and shortage fixture; compatible extra market records may remain non-gating.
- Production, population, inventory, market and price systems.
- Authoring/validation for the corresponding definitions.
- Gameplay queries, fixture control, stepping, invariants and checksum evidence.

Exit: the shortage and recovery can run deterministically headless from editor-authored data.

### Increment 2 — Buildable Lübeck and core UI

- Lübeck map/region, camera, card-to-world placement, direct road drawing, housing, the three local chain families, logistics and production presentation actors.
- Authored 3D ghosts and production presentations with no golden-path Engine primitive fallback.
- Production HUD, build menu, inspector, city overview, market and frontend UI.
- Semantic IDs/actions, ordinary input, UI waits and real native viewport evidence for each implemented state.

Exit: a player and MCP test can build all three local chains, diagnose the shortage and recover from placement/road errors without debug controls.

### Increment 3 — Trade, research, AI and victory

- Lübeck–Rostock Cog route creation and visible delivery, rendered Rostock, three-branch research, one AI merchant and prosperity progression.
- Editor graph/tuning coverage and cross-definition validation.
- Route/research/AI/victory queries, actions, fixtures and evidence.

Exit: the enhanced two-city golden journey passes manually and through MCP with correlated real-viewport and authoritative cargo evidence before save/multiplayer/provider work.

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

- a second fully buildable city, including buildable Rostock;
- additional required rendered/player-facing cities beyond Lübeck and Rostock, including Hamburg or Lüneburg, the full Europe map, and a player-facing intercity land-route journey;
- locally buildable production chains beyond bread, fish, planks, beer, and firewood, including smithy/tools;
- more than fifteen goods, two population tiers or three research branches;
- politics, Hanseatic assembly, diplomacy, contracts, loans, insurance and bankruptcy recovery;
- combat, piracy, convoys, detailed weather, fire, disease or crime;
- quality tiers, detailed spoilage, packaging beyond the empty-barrel good and advanced conditional orders;
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

1. Make Rostock buildable, then add rendered Hamburg/Lüneburg and extend the regional map.
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
2. a new player completes a recoverable 30–60 minute session without developer instructions or a prescribed build order;
3. deterministic, save/load and two-client authority checks pass;
4. all implemented features satisfy the game/editor/automation parity contract;
5. Lübeck is buildable and Rostock is rendered, prebuilt, inspectable and tradeable without construction controls;
6. ordinary card drag, accessible placement alternatives and direct road drawing operate the three local chain families with authored production presentations;
7. the full fifteen-good population/market loop and a visible, cargo-correlated Lübeck–Rostock Cog delivery are proven;
8. real assembled game-viewport, semantic, input and structured evidence satisfies every row in §10.1 at the required native resolutions;
9. generated data/media is traceable, validated and explicitly approved;
10. normal CI performs no billable provider calls;
11. the playable Shipping package contains none of the development-only systems, staging references, test helpers, or golden-path Engine primitive fallbacks.


## New-game empty-city revision — 2026-09-11

The user's current direction supersedes the prebuilt Lübeck opening for normal New Game: Lübeck starts with no buildings, roads, residences or building production units. Retain the house's starting money, city construction/material reserves, markets, technologies and trading opportunities. Rostock remains the prebuilt trading destination. Explicit shortage automation fixtures may retain their original seeded city. Existing saves preserve their buildings; choose New Game to start from scratch. The existing in-game Menu/Escape surface provides Save / load and confirmed Return to title.

### Survey waterfront opening — 2026-09-12

On the currently configured surveyed Lübeck map, normal New Game begins at a
fishery-compatible Trave bank and construction covers the surveyed land rather than
the prototype grid. Starting planks are four times the base opening stock (112 rather
than 28), plus a 1,000-plank construction-testing grant, for 1,112 total. Starting
timber receives a 100-unit construction-testing grant, for 134 total. Starting
tools receive a 1,000-unit construction-testing grant, for 1,018 total. Other opening
goods and existing saves remain unchanged. This is a gameplay
placement revision, not a promotion or historical reapproval of the staged terrain.
See [Development/SurveyWaterfrontOpening.md](Development/SurveyWaterfrontOpening.md).


### Starting Cog and local water exploration — 2026-09-17

The user explicitly adds a starting selectable Cog and manual sailing across connected navigable water on the complete current map, including rivers. This supersedes the earlier river-navigation exclusion for this local exploration feature. Reuse the approved cargo inspector and ship assets. Right-click sets a course; right-drag retains camera panning. See [ship navigation contract](Development/ShipNavigation.md) for authoritative commands, terrain validation, save migration, semantic controls and evidence.


### Craftsmen production scope extension — 2026-09-19

The user's explicit request extends normal gameplay with local Tools and Shoes production and a Day Laborers charcoal supply chain. This supersedes the earlier exclusion of buildable smithies/tools and the fifteen-good ceiling for this feature. Catalog v28 contains twenty goods, including charcoal, raw hides, tanning bark, leather and shoes. Iron bars + charcoal produce Tools at the Smithy; raw hides + tanning bark produce Leather at the Tannery; Leather produces Shoes at the Shoemaker. Artisan households consume Shoes. Inputs use the existing import/market system. See Development/ArtisanProduction.md.

Linen, candles and rope are now an isolated development review candidate rather than accepted content. The candidate adds prepared flax/hemp and beeswax imports, Weaver, Tailor, Chandler and Ropewalk workshops, Craftsmen needs and separate Ropewalk recipes without changing the accepted v28 catalog. It must pass disk-reload hash verification, playable UAT and explicit promotion approval before normal New Game uses it. See [Development/TextileProduction/README.md](Development/TextileProduction/README.md).

### Regional production economy — 2026-09-19

Remote-city production is represented by reusable `ProductionChain.*` definitions, five `Region.*` portfolios, and per-city stage subsets for thirty cities. Market-only industries consume inputs and create outputs through recipe semantics and physical city inventories. Bounded same-region exchange commits goods at source and delivers them after at least one market update; Lübeck never participates automatically. Legacy background-production fields remain migration-readable but are zero for regional cities. Raw hides and tanning bark are accessible from Rostock through normal market trade, while their source activities remain remote-only rather than player buildings. Save format 11 / determinism fingerprint 24 require a new game. See [Development/RegionalProductionEconomy.md](Development/RegionalProductionEconomy.md).
