# Hansa — Trade, foreign presence, and city-control implementation prompts

Prepared 2026-09-20. This is an executable prompt sequence derived from [trade.md](trade.md). It is not a claim that the described post-MVP systems already exist.

Run the prompts in order. Each prompt implements one bounded vertical slice, verifies it, and records evidence before the next prompt begins. Do not execute several prompts in one task unless the user explicitly requests that scope.

## Outcome and scope

Deliver a trade system in which:

- Lübeck remains the fully controlled starting city;
- existing foreign cities remain autonomous by default;
- a player may begin with limited public-market trade;
- a permanent trade station adds local storage, a resident factor, reliable route buffering, and a leased foothold;
- continued lawful commerce and investment advance foreign presence;
- higher presence unlocks merchant offices, specializations, bounded construction rights, merchant quarters, and later privileges;
- full city control is rare and scenario- or charter-gated rather than the automatic result of trading;
- every good, coin, reservation, ship, station, plot, and market effect remains authoritative, conserved, deterministic, inspectable, and saved.

The current MVP route system remains a valid foundation. Do not silently replace its physical load/unload behavior or rewrite old MVP evidence. Existing route cargo actions currently transfer inventory and do not by themselves perform market purchases or sales. Explicit commerce added by these prompts must use distinct typed commands and ledger effects.

## How to use this file

Start a task with the following launcher, replacing the prompt ID:

```text
Read trade.md and trade-prompts.md. Execute TR-01 in full, including the shared execution contract and acceptance gate. Implement and verify the work; do not stop at a plan. Preserve unrelated working-tree changes. Report exact evidence and remaining blockers. Do not execute later prompts in this turn.
```

Continue with TR-02, TR-03, and so on. A prompt is complete only when its acceptance gate passes. If a prerequisite is blocked, record the blocker and complete safe independent work without representing mocks, skipped checks, invented values, or disconnected screens as production behavior.

## Shared execution contract — applies to every prompt

1. Read `AGENTS.md`, `trade.md`, `Docs/MVP.md`, `Docs/EditorArchitecture.md`, and the relevant current implementation notes completely. Inspect the current code, assets, catalog, saves, tests, and working-tree changes before editing. Current code and promoted catalog data take precedence over obsolete baseline assumptions.
2. Preserve the integrated game/editor/automation parity contract. Every gameplay-model change must include reflected definition/state coverage, stable IDs, authoring metadata, validation, migration classification, schema export, impact analysis, deterministic compilation, save/hash coverage, queries/projections, tests, and documentation in the same prompt.
3. Keep `HansaEditor` Editor-only. Runtime modules must never depend on editor or provider integration. No provider SDK, credential, generation job, staging asset, test helper, automation endpoint, or development-only manifest may enter Shipping.
4. Use the existing authoritative command gateway, inventories, transaction ledger, market reports, vehicle cargo, route state, city definitions, and deterministic tick phases. Do not create a second economy, a parallel route authority, a UI-owned inventory, or a hidden background settlement path.
5. Conserve goods and money exactly. Every transfer identifies source inventory, destination inventory, good, quantity, price where applicable, fees where implemented, reservations, command/event identity, and authoritative tick. Partial and missed outcomes remain explicit. Do not mint goods, teleport cargo, spend negative money, or calculate outcomes only in UI code.
6. Maintain the distinction between:
   - inventory transfer;
   - direct market purchase or sale;
   - station purchase or sale order;
   - ship-to-station transfer;
   - station-to-building/local-logistics transfer.
   These operations may share ledger primitives but must not become one ambiguous command with hidden ownership or price effects.
7. Preserve the current MVP. Lübeck remains buildable; Rostock remains prebuilt, inspectable, and non-buildable under the historical MVP profile. New progression may initialize a compatible pre-established access state for that profile or live in a deliberately versioned successor scenario. Changing an MVP acceptance requirement requires an explicit `Docs/MVP.md` revision, not an incidental implementation choice.
8. Every player action uses ordinary mouse/keyboard/controller intent and stable semantic IDs. Automation calls the same validated commands and cannot bypass money, ownership, access, travel, capacity, construction, or progression rules. Errors include cause and remedy.
9. For any GUI design or material visual change, first read `design.md`, `Docs/UIDesignBrief.md`, and `Docs/UIAssetWorkflow.md` completely and use the `imagegen` skill. Inventory shell, navigation, panels, tables/lists, controls, feedback, map overlays/charts, icons, and decoration. Define applicable default, hover, pressed, selected, disabled, focus, loading, unavailable, warning, and error states. Generate and inspect required references/assets, store prompts/provenance, and implement dynamic text/data/layout natively in UMG/Slate. Never ship a generated full-screen mockup.
10. Reuse approved Hansa UI and world assets where valid. New GUI imagery uses ImageGen under the repository workflow. New Hansa building or station models use the applicable Hansa model workflow. Never substitute Engine primitives, handmade placeholder icons, or unverified staging assets in a player-facing or Shipping path.
11. Use real authoritative values. Unknown is not zero; stale reports retain age; estimated values are labeled ranges; reference-image numbers never become balance values. Do not display profit until an implemented transaction model can support it.
12. All asynchronous lists and panels preserve widget identity and focus as values update. Support localization expansion, large text, 80–140% UI scale, high contrast, reduced motion, keyboard/controller focus, and non-drag alternatives.
13. Tests must cover success, partial completion, rejection, rollback, integer boundaries, deterministic replay, save/load, prior-save migration, reversed discovery order where relevant, authority/ownership, semantic input, and real assembled viewport behavior. Do not loosen invariants, hide failures, or repin hashes solely to make tests pass.
14. Normal CI uses no live OpenAI, image, 3D, audio, or other provider call. Live media work occurs only when the prompt explicitly requires a production asset and follows the applicable skill, budget, staging, provenance, validation, and explicit promotion flow.
15. Record each completed prompt at `Docs/Development/TradePresence/TR-XX.md`. Maintain `Docs/Development/TradePresence/Acceptance.md` as a requirement-to-evidence matrix with Implemented, Verified, Blocked, and Deferred states. Include exact commands, build/content/save/fingerprint versions, logs, hashes, screenshots, and known limitations.
16. Preserve unrelated user changes. Do not use destructive Git or filesystem operations. A prompt is not complete until its vertical acceptance gate passes or a concrete external blocker is recorded honestly.

## Prompt sequence

| Stage | Prompts | Exit condition |
| --- | --- | --- |
| Contract and data foundation | TR-01–TR-02 | Current behavior is reconciled and foreign presence is authorable, deterministic, queryable, and inert until used |
| Basic commerce | TR-03–TR-04 | Visiting trade and trade-station establishment work end to end |
| Station logistics | TR-05–TR-06 | Station orders and physical Cog routes interact without hidden settlement |
| Presence and construction | TR-07–TR-09 | Commerce advances presence and unlocks bounded foreign development |
| Authority and competition | TR-10–TR-11 | Privileges, exceptional control, and AI participation obey the same rules |
| Scale and release | TR-12–TR-14 | Multi-city performance, complete UAT, compatibility, and Shipping gates pass |

---

## TR-01 — Baseline audit, economic contract, and ADRs

### Objective

Reconcile the design in `trade.md` with the current implementation and record the exact authority, ownership, settlement, save, UI, and scenario contracts before adding new state.

### Implement

- Inspect route definitions, vehicle/cargo state, market inventories, money ledgers, route creation, market actions, city ownership/buildability, known-information reports, AI commands, saves, schema registry, semantic UI, tests, and Shipping exclusions.
- Trace one current Lübeck–Rostock route from command through load, departure, travel, arrival, unload, market report, and UI projection. Record every inventory and money effect.
- Resolve any documentation disagreement about whether foreign-market settlement already exists. Treat executable code and passing tests as evidence; do not infer behavior from names or old prompts.
- Create ADRs or an equivalent decision record for:
  - inventory transfer versus purchase/sale;
  - ownership of station inventory;
  - house-plus-city foreign-presence identity;
  - how the historical MVP profile receives minimum Rostock access without invalidating its acceptance journey;
  - stable capability and permission representation;
  - leased-plot ownership versus city ownership;
  - transaction price timing and rounding;
  - report age and information access;
  - save and fingerprint version strategy.
- Create `Docs/Development/TradePresence/Acceptance.md` with rows covering every later prompt.
- Add characterization tests where important current behavior lacks a reliable test. Do not change player-visible economics in this prompt.

### Explicitly out of scope

- new station/presence gameplay;
- new UI layout or imagery;
- catalog promotion;
- changing current route settlement merely to match prose.

### Acceptance gate

- A report identifies the authoritative types, commands, ledgers, tick phases, projections, editor coverage, saves, and tests that later prompts extend.
- A deterministic characterization test proves whether current route load/unload changes money.
- Existing trade, market, save, UI, and Shipping tests still pass.
- Every unresolved conflict has a named decision owner or blocker; none is hidden behind a vague future note.

---

## TR-02 — Foreign-presence definitions, compiled policy, and state foundation

### Objective

Add the smallest stable, authorable, deterministic domain foundation for city-specific commercial presence without yet granting functional economic power.

### Implement

- Add stable reflected definitions, using repository naming conventions, for:
  - foreign-presence stages;
  - city trade/access policy;
  - typed capabilities or permissions;
  - stage prerequisites and upgrade costs;
  - permitted plot/building categories where future stages require them.
- Compile definitions into immutable runtime forms. Avoid a fragile collection of unrelated booleans; use a validated typed capability set with explicit semantics and versioning.
- Add authoritative state keyed by stable house/player identity plus `City.*`. At minimum retain current stage, granted capabilities, contribution/progress counters, status, establishment/upgrade tick, and references to owned station/plot identities when present.
- Add read-only projections and queries for a house's presence in one city, all known presences, available next stages, unmet requirements, and capability reasons.
- Seed an authored stage ladder matching `trade.md`: visiting/contact, trade station, merchant office, merchant quarter, privileged presence, and exceptional charter/governance. Exact costs may remain conservative authored values but cannot be invented in UI code.
- Add city policies for current scoped cities. Preserve the historical MVP by initializing or migrating its Rostock access according to TR-01's approved ADR.
- Add schema metadata, reference pickers, validation, impact analysis, deterministic JSON export/schema, migration classification, dry-run migration, catalog compilation, and generic Authoring Studio coverage.
- Reject duplicate house/city records, invalid stage transitions, unknown capabilities, unsupported plot/building rights, cyclic prerequisites, negative costs, and policy/stage contradictions.

### Explicitly out of scope

- paying for or constructing a station;
- market transactions;
- foreign construction;
- specialized player UI beyond diagnostic/query exposure.

### Acceptance gate

- A designer can create/edit stages and city policies through generic Authoring Studio support without raw serialization edits.
- Stable compilation and hashes are identical under reversed asset discovery order.
- Existing saves migrate deterministically and preserve the old MVP route journey.
- Runtime queries explain current stage, capabilities, next stage, and unmet requirements without mutating state.
- No capability changes cargo, money, construction, or market behavior yet.

---

## TR-03 — Visiting-merchant spot trade

### Objective

Implement explicit public-market buying and selling for a visiting player ship, with physical goods, money settlement, limited access, and no permanent station.

### Implement

- Add closed typed commands for buying from and selling to a foreign city market while an eligible player vehicle is physically berthed at that city.
- Validate ownership, location, public access, good visibility/access, quantity, market stock/capacity, ship capacity/cargo, player money, city policy, current authoritative price, and any already implemented transaction friction.
- Settle money and goods atomically through the normal ledgers. Define exact price sampling, integer rounding, partial-fill policy, event order, and rollback behavior in code and documentation.
- Record typed success, partial, missed, and rejected outcomes. Repeated command IDs must be idempotent.
- Update the market and ship projections so the next report reflects actual changed stock; do not mutate price directly outside the market system.
- Add a compact native trade action from the foreign-city/market inspector. Show current known price/report age, available stock, quantity, resulting cargo/cash estimate, and explicit uncertainty. Revalidate on confirmation.
- Disable trade when the ship is absent or access is unavailable, with cause and remedy. Support keyboard/controller quantity changes and confirmation.
- Add semantic actions/queries and ordinary-input automation coverage.

### Explicitly out of scope

- unattended orders;
- local player storage;
- a resident factor;
- station construction;
- advanced price triggers, tariffs, contracts, credit, or speculation.

### Acceptance gate

- A player sails an owned Cog to Rostock, buys a real good, and later sells a carried good through ordinary controls.
- Goods and money conserve exactly across full, partial, insufficient-funds, insufficient-stock, insufficient-capacity, stale-review, duplicate-command, and rejection cases.
- Save/reload while berthed and after settlement preserves identical authoritative state and allows deterministic continuation.
- The UI never claims a price or quantity is guaranteed before confirmation.
- Existing transfer-only routes remain unchanged.

---

## TR-04 — Establish a trade station

### Objective

Let the player establish the first permanent commercial foothold in a foreign city and receive a physical station inventory, factor, and bounded leased site.

### Implement

- Add typed commands and state transitions for proposing, validating, funding, constructing, completing, inspecting, and voluntarily closing a trade station.
- Require the authored city policy, prior presence stage, money/material costs, allowed site, and any relationship or trade prerequisites from TR-02.
- Allocate a stable station identity, physical inventory with finite capacity, factor/office identity, and leased-plot identity. Do not use display names, actor paths, filenames, or provider IDs as identity.
- Consume establishment costs transactionally. Construction failure or cancellation follows an authored refund policy and cannot duplicate materials.
- Add a production-quality station presentation in the foreign city using an approved existing asset or the required Hansa model workflow. Missing approved presentation blocks player-facing completion; no Engine primitive fallback.
- Add authoritative world selection and a compact station inspector showing identity, stage, construction/operating state, storage, factor, upkeep, capabilities, blockers, and next step.
- Integrate the trade-map city marker and presence inspector with station available, unaffordable, under construction, active, suspended, and error states.
- Add editor preview/validation for station presentation and site policy, plus save/migration, impact analysis, semantic controls, and real-viewport evidence.

### Explicitly out of scope

- recurring market orders;
- route loading through the station;
- merchant-office upgrades;
- construction outside the single station site.

### Acceptance gate

- The player establishes one station through ordinary controls, watches its real construction/presentation complete, selects it, and sees its authoritative empty inventory.
- Costs, identity, site allocation, capability state, and upkeep survive save/load and deterministic replay.
- Unauthorized, duplicate, unaffordable, invalid-site, and capacity-invalid establishments reject without mutation.
- Rostock remains an autonomous non-buildable city outside the granted station site.

---

## TR-05 — Resident factor and station market orders

### Objective

Make the station economically useful by allowing a resident factor to buy and sell over time into the station's physical inventory.

### Implement

- Add typed station orders owned by one house and city station. Initial order forms are intentionally bounded:
  - acquire a good until station stock reaches a target quantity;
  - release/sell a good down to a protected station reserve;
  - per-update quantity cap;
  - total or periodic spending limit.
- If price thresholds are included, gate them behind an authored capability and define exact inclusive/exclusive comparisons, report/settlement price, rounding, and behavior when the market changes. Otherwise defer thresholds to TR-08.
- Execute orders at a deterministic market phase in stable order. Use real player money, city market inventory, station capacity, reservations, and transaction ledger entries.
- Partial, blocked, skipped, insufficient-funds, insufficient-stock, insufficient-demand/capacity, suspended, and completed states remain observable.
- Prevent simultaneous orders from overspending or overcommitting the same station stock. Define deterministic priority and reservation behavior.
- Add order projections, history, last outcome, next eligible update, and causal blocker.
- Add a native station Orders section with create/edit/pause/cancel actions, current market report, target/reserve, cap, budget, actual recent execution, and cause/remedy feedback.
- Add schema/editor support for order policy limits and city capability, but keep live campaign orders in save state rather than definition identity.

### Explicitly out of scope

- ship route integration;
- multiple offices in one city;
- contracts, credit, short selling, speculative order books, or perfect future-price knowledge.

### Acceptance gate

- A station gradually acquires goods while the player's ship is absent, spending exactly the correct money and reducing real city stock.
- A station gradually sells imported stock and receives exact proceeds without flooding more than its cap.
- Competing orders, low money, full station, empty market, save boundaries, and reversed order discovery produce deterministic, conserved results.
- UI outcomes agree with ledger events and market reports.

---

## TR-06 — Route integration and station buffering

### Objective

Connect the existing physical Cog route system to station inventory without hiding market settlement inside load/unload actions.

### Implement

- Add explicit route action targets/types for:
  - owned-city inventory transfer;
  - foreign station load;
  - foreign station unload;
  - direct market transaction only if TR-03's contract explicitly supports route automation and the capability permits it.
- Preserve existing MVP transfer semantics and route schema migration. Old saves and routes must retain their behavior.
- Validate route owner, station owner, presence capability, ship mode/location, good, quantity, source reserve, station reservation, capacity, destination capacity, and complete cyclic reachability.
- Ensure the factor order and ship transfer phases cannot sell goods that have already been reserved for loading or load goods already committed to an order.
- Extend route preview with station stock, protected reserve, prepared cargo, report age, capacity use, travel time, upkeep, and expected transaction effects only where they are supported by explicit orders.
- Update the route creator, city marker, station inspector, Cog inspector, cargo manifest, journey timeline, and relevant alerts using native components and approved assets.
- Show the complete physical chain: city market ↔ station inventory ↔ Cog cargo ↔ destination inventory/market.
- Add typed events and projections correlating route action, station transfer, Cog cargo, city market effect, and visible vehicle state.

### Explicitly out of scope

- convoys;
- piracy, insurance, contracts, or multimodal transshipment;
- merchant-office specializations;
- foreign production buildings.

### Acceptance gate

- A factor accumulates cargo in Rostock, a Cog loads it from station stock, visibly sails, and unloads at Lübeck with exact correlated inventories.
- The reverse flow can unload goods into the station and sell them gradually without the Cog waiting at berth.
- Reservation races, full holds, full station, changed runtime state, partial load/unload, cancellation with cargo, pause/resume, and save/reload conserve state.
- The route UI distinguishes transfer, purchase, and sale and never labels travel-only inventory movement as profit.

---

## TR-07 — Presence contributions and merchant-office advancement

### Objective

Turn reliable commerce into explicit, inspectable foreign-presence progression and implement the first upgrade beyond a station.

### Implement

- Add authoritative contribution tracking for authored requirement types such as lawful delivered volume, transaction value, fulfilled city shortage, reliable operating duration, solvency/upkeep, local employment where available, and explicit investment.
- Count contributions from accepted domain events. Do not let UI, telemetry, or repeated replay messages increment progress.
- Define exact attribution, time windows, decay if any, caps, anti-farming rules, and treatment of returned/self-cancelled goods.
- Add typed commands to request and fund an upgrade. Revalidate all requirements and costs atomically at confirmation.
- Implement the Merchant office stage with concrete authored capabilities, such as larger station storage, more order slots/goods, better lawful report access, and increased route/handling allowance. Do not grant every later capability at once.
- Add the presence inspector's progress view, next-stage requirements, upgrade review, construction/progress state, completion, and history.
- Add alerts for eligible upgrade, blocked requirement, unaffordable investment, and suspended progress without spamming.
- Add generic and specialized Authoring Studio coverage for stage requirements and capability diffs, including impact analysis showing which cities/scenarios change.

### Explicitly out of scope

- generic political reputation detached from implemented events;
- merchant-quarter construction;
- civic privileges or full governance;
- percentage-only upgrade trees with no capability effect.

### Acceptance gate

- The player can explain every contribution and unmet requirement from UI and query data.
- Replayed events, route loops that return the same goods without eligible value, rejected trades, and save/reload cannot duplicate progress.
- Upgrading changes only the authored capabilities and preserves station inventory, orders, routes, site, identity, and history.
- A valid alternative commercial play pattern can reach the stage without one prescribed exact route sequence.

---

## TR-08 — Merchant-office specializations and advanced commerce

### Objective

Introduce meaningful branch choices that improve how the foreign operation works rather than merely increasing generic percentages.

### Implement

- Add authored mutually compatible or exclusive specialization choices for at least:
  - Warehouse: capacity, reservation, and throughput;
  - Market: reports, order slots, and price-limited orders;
  - Harbor: berth/handling and route operations.
- Each branch must grant typed capabilities consumed by real systems. Avoid UI-only bonuses and unbounded modifier stacking.
- If price thresholds were deferred in TR-05, implement them now with exact price sampling, inclusive comparisons, report uncertainty, settlement behavior, and deterministic tests.
- Add investment costs, prerequisites, respec/reversal policy, and impact preview. Applying a branch uses one atomic validated command.
- Update station/office presentation only where the branch has an approved visible change. Generate or build each required asset through the repository workflow; otherwise represent the capability through native operational UI rather than placeholder world art.
- Add a native comparison surface for 2–3 candidate branches, showing concrete unlocked actions, costs, capacity, and constraints.
- Add Authoring Studio branch graph/relationship visibility, validation, migration, and scenario impact analysis.

### Explicitly out of scope

- permitted production workshops;
- civic politics;
- insurance, loans, or convoys;
- automatic best-branch recommendations that play the game for the user.

### Acceptance gate

- Each specialization produces a demonstrably different operational capability using the same underlying station state.
- Exclusive choices, stale review, insufficient resources, duplicate submission, save migration, and respec rules are deterministic and transactional.
- Branch UI explains opportunity cost before confirmation and remains usable at supported scales and inputs.

---

## TR-09 — Leased plots, bounded foreign construction, and merchant quarter

### Objective

Allow higher presence to unlock carefully bounded construction in an autonomous foreign city while preserving local ownership and simulation.

### Implement

- Add authoritative leased-plot allocations with stable city, house, plot, bounds, stage, permitted categories, occupancy, and active/suspended state.
- Extend construction validation so foreign placement requires both normal building validity and an active permission covering the entire footprint and building category.
- Add approved commercial/logistics building choices appropriate to the current stage: office expansion, warehouse, quay/handling facility, and selected local logistics. Reuse current definitions/assets where semantics are valid; otherwise deliver complete definition/editor/presentation parity.
- Advance to a Merchant quarter only through authored requirements and investment. It may grant several leased plots or a bounded district, never blanket city ownership.
- If permitted workshops enter scope, they use real local workforce, inputs, outputs, inventories, roads, and market/logistics rules. Decorative workshops must never create simulation production.
- Add world overlays for owned, leased-empty, future-available, prohibited, warning, and invalid land. Use shape/pattern/text in addition to color.
- Add a city-presence construction browser filtered by actual permissions, with locked categories and precise reasons.
- Add save/migration, plot impact analysis, authoring previews, construction semantic controls, ordinary input tests, and real-world captures.

### Explicitly out of scope

- editing or demolishing city-owned buildings;
- building anywhere outside granted plots;
- claiming city market stock, roads, workforce, or taxes as player property;
- automatic conversion of Rostock into a buildable player city.

### Acceptance gate

- A player constructs an authorized building wholly inside a leased plot through the ordinary build flow.
- The same building rejects outside the plot, across a boundary, under the wrong stage/category, or while rights are suspended without mutating resources.
- Local workers, inputs, outputs, roads, and inventories remain physically and economically correct.
- The rest of the city remains selectable, simulated, and non-buildable by the player.

---

## TR-10 — City privileges, projects, and exceptional control transition

### Objective

Implement the upper boundary of foreign influence without making ordinary trade automatically confer sovereignty.

### Implement

- Add authored city privileges as scoped capabilities with issuer city, recipient house, requirements, cost/contribution, duration or permanence, suspension/revocation rules, and explicit effects.
- Begin with privileges backed by existing or deliberately implemented systems, such as additional plots, a permitted building category, berth priority, or a bounded city project. Do not expose fake tariff, council, tax, or policy controls before those authoritative systems exist.
- Add one physical/economic city project funded by the player that produces an observable shared effect and consumes real resources over time.
- Define the exceptional transition to broad/full authority. It must require a scenario rule, founding/charter outcome, or explicit authored governance event. Record what assets, plots, policies, routes, debts, station inventory, and city-owned property transfer or remain autonomous.
- Keep ordinary historical cities autonomous when their policy disallows governance transfer.
- Add privilege/charter review UI with effects, ownership, costs, reversibility, and affected parties before confirmation.
- Add editor definitions, dependency/impact analysis, migrations, authority commands, save state, semantic input, and negative authorization tests.

### Explicitly out of scope

- a complete Hanseatic assembly or diplomacy game unless separately authorized;
- conquest, combat, occupation, or hidden annexation;
- flavor-only privileges with no implemented effect.

### Acceptance gate

- At least one scoped privilege and one funded city project function through real authoritative systems.
- A city that disallows broader authority cannot be converted through trade volume or forged commands.
- The scenario-gated authority transition, if implemented, has a complete transactional ownership/migration contract and preserves conservation.
- UI wording clearly distinguishes commercial presence, privilege, partnership, and governance.

---

## TR-11 — AI merchant parity and competition

### Objective

Allow AI merchant houses to use the same public trade, stations, orders, routes, progression, and permissions without free goods, free money, private player information, or direct state mutation.

### Implement

- Extend AI goals and bounded decision inputs to consider only authorized known market reports, current money, ships, cargo, station inventories, capabilities, progression requirements, and city policies.
- AI actions submit the same typed commands as a human house. Do not add AI-only settlement, progression, construction, or privilege mutation.
- Define deterministic option ordering, budget/risk limits, cooldowns, and reasons for selecting or rejecting direct trade, station establishment, orders, routes, upgrades, and branches.
- Make competition for limited market stock, station sites, or privileges transactional and deterministic.
- Expose an allowlisted AI explanation projection for diagnostics/automation without leaking hidden market or rival information to ordinary players.
- Add authored tuning metadata and generic editor support; do not embed balance constants in controller code.
- Add UI feedback only where the player lawfully observes rival presence or market effects.

### Explicitly out of scope

- AI city construction beyond granted merchant plots;
- omniscient planning;
- diplomacy personalities unrelated to implemented rules;
- multiple new AI houses solely to inflate test scope.

### Acceptance gate

- One AI can establish and operate a station, execute lawful trades, run a route, and progress using normal money/inventory/capability rules.
- Concurrent human/AI attempts resolve once and conserve goods, money, sites, and privileges.
- AI replay with the same seed/commands produces the same choices and fingerprint.
- Player-facing projections do not leak the AI's private cargo, orders, or reports beyond authorized visibility.

---

## TR-12 — Multi-city rollout and regional-economy integration

### Objective

Scale the proven system beyond Rostock without turning every remote city into a rendered/buildable clone or bypassing the existing regional production economy.

### Implement

- Author city access policies, stage availability, station sites where rendered, goods access, and specialization restrictions for a reviewed subset of cities from the existing regional catalog.
- Market-only cities may support abstract-but-authoritative commercial presence and inventory only if the player experience clearly identifies their non-rendered status. Do not fabricate visitable world scenes or construction plots for them.
- Integrate station demand and transactions with actual remote-city production, regional transfers, reserves, incoming supply, and price reports.
- Ensure Lübeck never enters automatic regional exchange merely because the player has foreign stations.
- Add scalable queries, virtualized lists, filters, search, selected-good mode, presence mode, route mode, report-age handling, and bounded map markers.
- Profile deterministic tick cost, save size, projection size, UI list performance, and route/order throughput at the intended early multi-city scale.
- Add catalog validation proving every station-capable city has compatible market, policy, route access, and presentation/abstract classification.

### Explicitly out of scope

- claiming the full 30–50-city campaign is release-ready without complete content and evidence;
- automatic global goods exchange;
- player construction in non-rendered cities;
- hidden fallback supply.

### Acceptance gate

- A reviewed multi-city fixture runs stations, orders, routes, regional production, and regional transfers without duplication, starvation caused by ordering bugs, or unauthorized automatic Lübeck supply.
- The same simulation produces identical fingerprints under reversed definition discovery.
- Trade-map interaction remains responsive and accessible with the tested city/route/order counts.
- Every city accurately advertises rendered/visitable/buildable/market-only capabilities.

---

## TR-13 — Suspension, revocation, closure, recovery, and compatibility hardening

### Objective

Make the system recoverable under failure and change, and prove migrations across every released intermediate version.

### Implement

- Implement explicit active, underfunded, storage-blocked, order-suspended, rights-suspended, voluntarily closed, and revoked states supported by current game rules.
- Define which actions continue, which pause, which upkeep remains, and how cargo, orders, routes, plots, buildings, workers, reservations, and station inventory can be recovered.
- Closing or losing a station must never delete cargo silently. Use a deterministic liquidation, transfer, stranded-inventory, or recovery contract appropriate to the available systems.
- Update routes that depend on a suspended/closed station with safe pause/missed-action behavior and actionable alerts.
- Add migrations from every save/fingerprint/catalog version introduced by TR-02 onward, plus the prior trade-route formats. Test dry-run, applied, repeated/idempotent, and failure rollback.
- Add corrupted, missing-definition, removed-city-policy, changed-plot, and incompatible-capability recovery diagnostics.
- Audit stable IDs, integer limits, duplicate commands, tick boundaries, simultaneous events, and transaction rollback.

### Explicitly out of scope

- bankruptcy loans, insurance payouts, war seizure, or political compensation unless separately implemented;
- destructive silent cleanup to make saves load.

### Acceptance gate

- Every supported interruption state explains what happened, what is preserved, what is costing money, and how to recover.
- Cargo, money, plots, buildings, orders, routes, and presence survive or transition exactly according to documented rules.
- All historical and intermediate save fixtures migrate or fail safely with an actionable reason.
- Determinism, conservation, and command-history integrity pass across suspension/closure boundaries.

---

## TR-14 — Full UAT, accessibility, clean-build release, and handoff

### Objective

Verify the complete trade-to-presence journey through ordinary packaged gameplay and produce a release-quality evidence and maintenance handoff.

### Implement and verify

- Run at least these complete player journeys:
  1. visit and perform spot trade;
  2. establish a station;
  3. create factor orders;
  4. buffer and execute a Cog route;
  5. advance to Merchant office;
  6. choose different specializations in separate runs;
  7. construct inside a leased foreign plot and recover from an invalid attempt;
  8. reach a Merchant quarter or reviewed upper-stage fixture;
  9. encounter AI competition;
  10. suspend, recover, close, save, and reload.
- Use different viable trade goods/routes and at least one poor but recoverable decision. Do not certify only one scripted golden ordering.
- Verify every screen/state at native 1280×720 and 1920×1080, plus the supported wide/large-text/high-contrast/reduced-motion profiles required by the UI brief.
- Test mouse, keyboard, controller, focus restoration, localization expansion, semantic automation, stale/unknown information, loading, empty, partial, warning, error, and unavailable states.
- Correlate real viewport captures with authoritative commands, ledger events, inventories, market reports, route/Cog projections, presence state, plots, construction, and saves.
- Run performance/soak coverage with the supported city, route, order, station, ship, and AI counts. Record actual hardware and percentiles.
- From a clean-checkout-equivalent state, build game/editor/tests, compile content, run migrations and deterministic suites, cook/package Shipping, launch the packaged game, complete the reviewed journey, and audit forbidden content.
- Finalize `Docs/Development/TradePresence/Acceptance.md` and produce a handoff covering authoring, tuning, migrations, debugging, test commands, known limits, and future extensions.

### Acceptance gate

- Every mandatory acceptance row has dated structured, semantic/input, and real-viewport evidence as applicable.
- Goods and money conserve across the complete journey and AI competition.
- No critical clipping, inaccessible action, placeholder asset, stale fake value, automation-only workaround, or disconnected UI remains.
- The clean Shipping package contains no editor, automation, worker, provider, credential, staging, developer, or test-only surface.
- If any mandatory row is blocked, release status remains blocked with a concrete remaining-work list; the gate is not weakened or reworded to claim success.

## Final sequencing notes

- TR-01–TR-06 establish the essential commercial loop and should be completed before broad privilege or construction work.
- TR-07–TR-09 deliver the user's defining progression from commerce to bounded foreign-city control.
- TR-10 is intentionally scenario- and policy-gated so ordinary commerce does not become automatic annexation.
- TR-11 must use the same commands and information rules as human play.
- TR-12 expands content only after the two-city loop is proven.
- TR-13 is not permission to postpone save, migration, and rollback work from earlier prompts; it audits and hardens the accumulated system.
- TR-14 verifies the assembled product. Generated references, headless tests, or source inspection alone cannot satisfy player-facing acceptance.
