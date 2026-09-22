# Regional production economy — implementation prompt 1

Implement the first data-and-simulation slice of Hansa's regional production economy. The purpose is to replace undifferentiated per-city background goods creation with a small reusable library of production chains, a larger production portfolio permitted by each region, and a smaller subset of active production stages assigned to each city.

This prompt is authoritative for this implementation. It covers economic definitions, compiled registry data, deterministic runtime simulation, save/hash behavior, authoring parity, initial historical-region data, tests, and documentation. It does **not** authorize new player-facing GUI designs, icons, raster artwork, workshop meshes, city maps, environment art, animations, audio, or generated media.

## Required reading and repository constraints

Before implementation, read `AGENTS.md`, `Docs/MVP.md`, and `Docs/EditorArchitecture.md` completely. Inspect the current catalog and implementations rather than assuming the older baseline still matches promoted content. Preserve unrelated working-tree changes.

Follow the editor/game/automation parity contract. Runtime modules must not depend on `HansaEditor`. Use reflected Unreal definitions as the accepted authoring truth, compile them into immutable deterministic runtime structs, and update metadata, validation, migration classification, schema export, impact analysis, tests, and documentation in the same implementation.

Do not make a live provider call. No ImageGen, 3D provider, OpenAI data-generation call, or other billable generation is needed. Do not create placeholder artwork or substitute Engine primitives. Existing presentation assets and screens must remain untouched except for unavoidable compile-safe bindings to existing data.

## Product decision

Use this hierarchy:

```text
global production-chain definitions
    -> regional permitted production portfolio and resource endowments
        -> city-selected subset of production stages and capacities
            -> actual inventory consumption/output, market supply/demand and prices
```

A region owns a **superset** of historically and geographically plausible chains. Each city selects only part of that set. Do not duplicate recipes or production graphs per city. A city binds existing recipe stages and capacities; it must not claim direct output goods independently of those recipes.

All goods remain technically tradeable everywhere. Regional or city production controls native supply, not whether a delivered good can exist in a market. A city may receive, store, consume, resell, or export goods it does not produce.

Keep the distinction between:

- population/workforce tiers: `PopulationTier.Laborer` and `PopulationTier.Artisan`;
- authored construction tiers: `DayLaborers` and `Craftsmen`.

Do not silently treat those identities as interchangeable. Chain metadata may state its intended construction tier, while workforce requirements remain properties of the referenced recipes.

## Scope

### In scope

- reusable production-chain definitions referencing existing recipes;
- five economic-region definitions with five or six member cities each;
- resource endowments and permitted chain/stage portfolios per region;
- city industry bindings that select a subset of their region's stages;
- aggregated but physically conserved remote-city production using the ordinary recipe inputs and outputs;
- bounded deterministic exchange of intermediate goods between market-only cities in the same region;
- market production/demand and price integration;
- structured queries/projections sufficient to inspect region, city industry, production, blockers, and regional transfers;
- full generic Authoring Studio/schema support with validation and impact analysis;
- deterministic compilation, save/hash coverage, fixtures, tests, catalog evidence, and documentation;
- initial data for the currently enabled Day Laborer and Craftsmen production families.

### Explicitly out of scope

- new player-facing screens, panels, maps, buttons, icons, tooltips, or visual redesign;
- image generation or modification;
- 3D workshop, city, vehicle, prop, or terrain creation;
- rendering unbuilt remote-city workshops as Actors;
- making any additional city buildable or visitable;
- adding player-facing routes to every new city;
- new goods such as flax, hemp, beeswax, linen, clothing, candles, rope, furs, amber, copper, wool, cloth, wine, pitch, or tar;
- activating the proposed textile/candle/rope content if it is still unpromoted;
- politics, tariffs, privileges, contracts, credit, insurance, embargoes, piracy, or detailed order books;
- replacing explicit inter-region player/AI routes with automatic global trade;
- separate city-specific copies of an existing recipe.

## Authoritative definitions

Choose names consistent with repository conventions, but implement these concepts as first-class reflected definitions.

### Production chain

Add a stable production-chain definition, preferably `UHansaProductionChainDefinition`, compiled into a provider-neutral `FHansaCompiledProductionChainDefinition`. If a new stable-ID domain is required, add and document `ProductionChain.*` rather than encoding identity in display text or package paths.

Required fields:

- stable chain ID and localized display identity;
- intended construction tier: Day Laborers or Craftsmen for this slice;
- ordered stage records;
- each stage has a stable stage key and one existing `Recipe.*` reference;
- optional prerequisite stage keys for graph presentation/validation;
- stage role: resource source, primary processing, intermediate processing, or finished manufacture;
- optional notes/tags for economic classification, never runtime identity;
- schema version, revision, deterministic content hash, migration classification and complete authoring metadata.

Outputs, inputs, workforce and cycle quantities are derived from the referenced recipes. Do not duplicate them in the chain definition.

### Region economic profile

Add `UHansaRegionEconomicProfileDefinition` with `Region.*` identity and a compiled counterpart.

Required fields:

- member `City.*` references;
- permitted chain profiles;
- for each permitted chain, the allowed stage-key subset;
- resource endowments for declared-source stages;
- endowment level: `Absent`, `ImportOnly`, `Available`, `Abundant`, or `Signature`;
- maximum source yield/capacity per market update for applicable declared-source stages;
- bounded regional exchange capacity per update;
- regional transfer delay in market updates, at least one update;
- deterministic transport-cost or loss policy if used; quantities and money must remain auditable;
- schema/hash/metadata/validation parity.

`ImportOnly` permits processing stages but never creates the raw material. `Absent` permits neither local extraction nor an implicit fallback source. A region profile is permission and capacity, not a shared inventory.

### City industry bindings

Extend the city market/economy definition, or add a separate city-industry definition keyed by `City.*`, with:

- exactly one `Region.*` reference;
- enabled chain/stage bindings, all of which must be allowed by the region;
- per-stage capacity expressed as maximum recipe cycles per market update or another exact fixed-point deterministic unit;
- efficiency basis points if needed, defaulting to 10000;
- input reserve and output/export reserve policies;
- enabled/disabled state;
- optional city signature rank used for authoring/reporting, not as an alternative production formula.

Do not add a direct list of produced goods. The compiler derives each city's native outputs and required inputs from its enabled recipe stages.

### Compatibility fields

Retain the current `FHansaMarketGoodProfile.BackgroundProductionMilliUnitsPerUpdate` fields for old fixtures and explicit compatibility content in this first slice. New region/city data must not use background production for a good also produced by an enabled regional industry stage. Validation must reject double supply.

Document a later removal path, but do not perform a destructive field removal in this prompt.

## Runtime behavior

### Remote industries

Market-only cities must run aggregated remote industries through actual recipe semantics:

1. Resolve the city industry binding and referenced recipe.
2. Limit work by authored stage capacity, available input stock, output capacity, endowment/source limits, and deterministic fixed-point efficiency.
3. Consume recipe inputs from the city's real inventory through the inventory transaction ledger.
4. Create outputs only through declared source transactions or the ordinary recipe transformation contract.
5. Record recent local production and industrial demand in the market system.
6. Expose a causal blocker when a stage cannot run: missing input, depleted/absent endowment, input protected by reserve, output capacity, disabled binding, or invalid definition.
7. Use stable city + chain + stage identity. Do not create fake workshop/building entities or provider/file identities.

Reuse the existing production/inventory math where practical, but do not weaken invariants by assigning an invalid building ID to a normal `FHansaProductionState`. A dedicated compact remote-industry state is preferable if ordinary production state requires a physical building.

Declared source recipes such as farming, fishing and timber felling remain legitimate sources, but remote execution is bounded by the region endowment and city stage capacity. No source may fill stock without a ceiling.

### Regional exchange

Implement bounded exchange of goods between **market-only cities in the same region**:

- a source may offer only stock above its output/export reserve and protected inventory;
- a destination requests inputs needed by an enabled industry, then goods below its desired market reserve;
- match deterministically by stable region, good, destination priority, source priority and stable city ID;
- total movement is capped by the region's exchange capacity;
- schedule a transfer and deliver it no earlier than the next market update;
- pending transfers count as expected incoming supply only after quantity has been physically committed at the source;
- save and hash pending transfers;
- publish typed transfer events and structured projections;
- conserve goods exactly except for an explicitly authored and ledger-recorded transport-loss policy;
- do not automatically move goods into or out of a player-controlled/buildable city.

Lübeck must therefore still use explicit player or AI trade routes for inter-city imports and exports. Regional exchange may support the remote suppliers behind Rostock but must not silently supply Lübeck.

Do not use one shared regional inventory. Origin, source city, destination city, quantity, good, dispatch tick and arrival tick must remain observable.

### Markets and trade routes

- Continue to create a market row for every registered good in every instantiated city.
- A city without native production may still receive and trade that good.
- Loading from a market remains constrained by physical stock, protected quantity, route minimum reserve, vehicle capacity and buyer cash.
- Foreign-market purchases and sales retain current price settlement and transaction-friction behavior.
- Existing market price factors continue to use actual stock, reserve, citizen demand, industrial demand, committed incoming cargo/transfers, unmet demand, season and city modifiers.
- Replace background-production contributions with remote-industry output for migrated rows.
- Route validation must not add a hard export/import whitelist.

Correct any existing review text or projection that contradicts authoritative foreign-market money settlement, but do not redesign the trade UI.

## Initial chain definitions

Create production-chain assets/data for the currently promoted goods and recipes. Preserve the exact accepted recipe quantities, workforce, cycle times, firewood process inputs, spoilage behavior, and stable IDs from the current catalog. Do not reconstruct older seed values over newer promoted content.

### Day Laborers

At minimum:

1. `ProductionChain.Bread`
   - `Recipe.GrowGrain`
   - `Recipe.MillFlour`
   - `Recipe.BakeBread`
2. `ProductionChain.FreshFish`
   - `Recipe.CatchFish`
3. `ProductionChain.PreservedFish`
   - the current catch/salting contract including `Recipe.SaltedCatch`
4. `ProductionChain.Planks`
   - `Recipe.FellTimber`
   - `Recipe.SawPlanks`
5. `ProductionChain.Beer`
   - `Recipe.GrowGrain`
   - `Recipe.GrowHops`
   - `Recipe.MaltGrain`
   - `Recipe.MakeBarrels`
   - `Recipe.BrewBeer`
6. `ProductionChain.Firewood`
   - `Recipe.FellTimber`
   - `Recipe.SplitFirewood`
7. `ProductionChain.Charcoal`
   - `Recipe.FellTimber`
   - `Recipe.BurnCharcoal`

Shared stages may be referenced by multiple chains. They remain one global recipe definition and one production outcome; chain membership is classification and graph topology, not duplicate execution.

### Craftsmen

At minimum:

1. `ProductionChain.Tools`
   - imported or regionally supplied `Good.Iron`
   - `Recipe.BurnCharcoal` as the local supporting stage where enabled
   - the current promoted `Recipe.SmithTools`, including charcoal if present in catalog v28
2. `ProductionChain.Shoes`
   - regional/source supply of `Good.RawHides`
   - regional/source supply of `Good.TanningBark`
   - `Recipe.TanLeather`
   - `Recipe.MakeShoes`

Raw hides, tanning bark and iron need explicit source/endowment contracts for remote regions even where the current player city imports them. Do not invent a player-buildable cattle farm, slaughterhouse, mine, bark-stripper or new workshop in this prompt.

## Initial regional and city data

Create five region definitions and five or six city records per region. New cities are economic definitions only: no world map, city scene, construction menu or travel UI is required. Use ASCII stable IDs and localized display names. If campaign dating later requires a roster adjustment, keep the data easy to revise; do not encode the historical claim in C++ branching.

### `Region.WendishLowerElbe`

Cities: Lübeck, Hamburg, Lüneburg, Rostock, Wismar and Stralsund.

Permitted portfolio: all initial Day Laborer chains plus Tools and Shoes. Regional strengths include salt distribution/extraction at Lüneburg, beer and finished crafts at the western hubs, coastal fish and preservation, Mecklenburg grain, timber, hides, bark and tanning.

Required city differentiation:

- Lübeck: processing, brewing, preservation, tools and shoes; player production remains building-driven and receives no automatic regional exchange.
- Hamburg: fish, bread, beer, tools and shoes; substantial consumption/export-hub demand.
- Lüneburg: signature salt supply, limited grain/beer processing, no native fishery or hide source.
- Rostock: grain, fish, timber, raw hides, tanning bark and leather; it is the first accessible remote supplier for the current Lübeck–Rostock slice.
- Wismar: fish, beer, timber, bark and limited leather.
- Stralsund: fish/preservation, grain, bark and limited tanning.

### `Region.PrussianPomeranian`

Cities: Danzig, Elbing, Königsberg, Thorn, Stettin and Greifswald.

Permitted portfolio: grain/bread, fish/preservation, timber/planks, barrels, beer, firewood, charcoal, raw hides/leather/shoes and limited tools. Emphasize grain, timber and raw-material exports. Distribute stages so Danzig and Elbing are export/processing hubs while inland cities supply grain, timber or hides. Do not give every city the complete grain-to-bread or hides-to-shoes chain.

### `Region.LivonianRus`

Cities: Riga, Reval, Dorpat, Narva, Pskov and Novgorod.

Permitted portfolio: grain, fish, timber/planks, firewood, charcoal, raw hides, tanning bark, leather and limited shoes/tools. Emphasize raw hides, bark and timber in the hinterland; Riga and Reval perform more processing and export. Do not add furs, wax, flax, hemp or tar until those goods receive a separate approved implementation.

### `Region.Scandinavian`

Cities: Bergen, Oslo, Stockholm, Visby, Kalmar and Malmö.

Permitted portfolio: fish/preservation, timber/planks, firewood, raw hides/leather, grain/beer in the south, and iron/tools around Stockholm. Bergen is a signature fish supplier but must depend on imported grain and salt for chains that need them. Do not invent copper, butter or stockfish as new goods; use current fish/preserved-fish semantics only.

### `Region.WesternNorthSea`

Cities: Bruges, Antwerp, London, Boston, King's Lynn and Kampen.

Permitted portfolio: grain/bread, beer, fish/preservation, tools, leather and shoes, with strong finished-goods consumption and redistribution. This region should import much of its timber, bark and raw hides unless a particular city binding explicitly supplies a limited amount. Do not add cloth, wool, wine or luxury goods in this slice.

### Portfolio limits

- Each ordinary city should enable roughly three to six production families.
- Each city should have no more than two signature stages.
- Each region must collectively have a reachable path for every chain it claims can complete locally.
- It is valid and desirable for a region to support only part of a chain and depend on inter-region imports.
- At least one stage of every multi-stage regional chain must be assigned to a different city than another stage; do not make every city self-sufficient.
- Rostock must produce hides and tanning bark through explicit source bindings and must be able to tan leather after regional/local inputs exist.
- Lübeck must not receive free hides, bark, leather or shoes from regional exchange.

Record the final city-by-stage matrix as a checked-in machine-readable fixture plus a concise Markdown table. Treat historical assignments as reviewed game data with source notes, not hard-coded conditionals.

## Compilation and validation

The definition compiler must:

- compile regions, chains and city bindings in stable-ID order;
- include all new definitions and relationships in the registry hash;
- reject missing/duplicate region, city, chain, stage and recipe references;
- reject a city with no region or membership disagreement between city and region;
- reject city stages outside the regional allowed set;
- reject duplicate city-stage bindings;
- reject resource-source capacity where the region endowment is `Absent` or `ImportOnly`;
- reject background production for a good also output by an enabled remote industry;
- reject a claimed locally completable chain with no reachable source/input path;
- distinguish a valid import-dependent stage from an accidentally unreachable stage;
- detect dependency cycles that are not legitimate shared-input graph structure;
- enforce deterministic results regardless of asset discovery order;
- expose reverse references and impact analysis for region, chain, recipe, city and good changes.

Do not calculate reachability from display names, array order, package names or filenames.

## Authoring and editor parity

- Add complete `UPROPERTY` metadata, units, ranges, tooltips, reference domains, AI-access classification, serialization and migration policy.
- Generic Authoring Studio details/table support is required.
- Extend the existing production graph data model so regions and city subsets can be inspected through structured graph data, but do not build or redesign a new editor UI in this prompt.
- JSON Schema export and strict interchange must include nested region/chain/city structures.
- AI proposals remain drafts and cannot write derived hashes, compiled outputs or runtime state.
- Add impact descriptions such as “changing this region removes an allowed stage from three cities” and “changing this recipe affects two chains and eight city industries.”
- Update schema-coverage and round-trip tests.

## Runtime state, save and migration

Add remote-industry state and pending regional transfers to:

- authoritative simulation state;
- read-only projections;
- deterministic state hash;
- save envelope serialization and validation;
- multiplayer snapshot/delta coverage where the existing architecture requires it;
- query allowlists and automation fixtures.

This changes normal-game economics. Promote it as the next deliberate economic catalog version after v28 and require **New Game** for the new catalog unless a complete, explicitly tested migration is implemented. Existing v28 saves must never be silently loaded under changed economics; preserve the ordinary catalog/hash incompatibility explanation.

Legacy fixtures that intentionally use authored background supply may remain on their matching catalog/fixture contract.

## Structured observability

Provide deterministic queries/projections for:

- region profile and member cities;
- region-permitted chains and stages;
- city-enabled industry stages;
- derived native inputs and outputs;
- stage capacity, utilization and blocker;
- recent input consumption and output production;
- queued/in-transit regional transfers with origin and arrival;
- native, regional and route-based expected incoming supply as separate causes;
- city/region chain reachability diagnostics.

Do not add a new player-facing GUI. Existing market and city views may continue showing their current aggregate values, which must agree with authoritative production and transfer state.

## Determinism and economic invariants

- Equal definitions, seed and commands must produce equal state hashes and event order.
- Use fixed-point integer math only in authoritative simulation.
- Production transformations conserve recipe inputs/outputs exactly.
- Regional transfers neither create nor destroy goods except an explicit recorded loss.
- A committed transfer is removed/reserved at its source before it contributes to incoming supply.
- Capacity, reserves, storage, source endowments and insufficient inputs produce partial or blocked work rather than negative inventory.
- Stable ordering resolves equal-priority matches; do not use UObject iteration or `TMap` iteration order as authority.
- Prices must react to the resulting real stock, demand and committed incoming supply; do not set prices directly.

## Required tests

At minimum, add focused tests for:

1. chain schema, stable IDs, stage uniqueness and recipe-derived inputs/outputs;
2. five region profiles, membership and city-subset validation;
3. every city binding is a subset of its region portfolio;
4. no city-specific recipe duplication;
5. remote source capacity and endowment enforcement;
6. remote processing consumes inputs and creates the exact recipe output;
7. missing input, protected reserve, storage and disabled-stage blockers;
8. background-production double-supply rejection;
9. deterministic regional matching independent of authored array order;
10. one-update-or-greater transfer delay and committed incoming supply;
11. regional exchange conservation, capacity and reserve protection;
12. no automatic regional transfer into or out of Lübeck;
13. Rostock hides + bark -> leather operation;
14. a distributed chain whose source and processing stages are in different cities;
15. an import-dependent stage that blocks, then recovers after a real route/regional delivery;
16. market price direction after production, shortage and incoming transfer;
17. foreign trade purchase/sale settlement remains correct;
18. save/load equality with active industries and pending transfers;
19. state-hash and event-order determinism;
20. registry compilation and reverse-order hash equality;
21. JSON Schema/generic authoring coverage, import/export round trip and impact analysis;
22. catalog v28 incompatibility is explicit and the new catalog starts correctly;
23. Shipping exclusion remains clean.

Run the relevant existing production, inventory, logistics, market, trade, save, multiplayer, definition compiler, schema and authoring regressions. Do not claim completion from new focused tests alone.

## Documentation and evidence

Update `Docs/MVP.md` with the user-approved regional specialization direction and its exact first-slice boundary. Update `Docs/EditorArchitecture.md` with the new definitions and parity requirements. Add a development report that records:

- final schemas and stable IDs;
- global chain inventory;
- five regional portfolios;
- city-by-stage subset matrix;
- runtime execution and regional exchange rules;
- catalog/save decision;
- validation rules;
- tests and exact results;
- remaining limitations and deferred goods/assets/UI.

Preserve a machine-readable compiled/diff artifact and the promoted catalog golden file according to existing economic-catalog conventions.

No screenshots or visual captures are required because this prompt makes no visual or player-facing GUI change. If implementation unexpectedly changes visible UI, stop and follow the repository's GUI and ImageGen requirements before proceeding.

## Acceptance criteria

The task is complete only when all of the following are true:

- global chains are defined once and reused by regions/cities;
- all five regions contain five or six member cities and a larger permitted portfolio;
- every city owns a meaningful smaller subset rather than the full regional set;
- current Day Laborer and Craftsmen chains are represented using the promoted catalog recipes;
- market-only cities produce through real aggregated recipes instead of migrated magic background supply;
- regional intermediate exchange is bounded, delayed, observable and goods-conserving;
- Lübeck still requires explicit trade for external inputs;
- Rostock can source hides and tanning bark and convert them to leather through authoritative state;
- all goods remain deliverable/tradeable even where they are not natively produced;
- editor/schema/migration/save/query/test parity is complete;
- no new GUI artwork, image, 3D model, workshop presentation, generated media or live provider call was introduced;
- normal and Shipping builds retain the required module and staging exclusions;
- the final report distinguishes implemented runtime behavior from data prepared for cities not yet exposed through the player-facing map.

## Deliberate follow-ups, not part of this prompt

- player-facing regional economy and origin/supply-chain UI;
- economic-map visualization and additional route authoring;
- rendered or buildable Wismar, Stralsund and later-region cities;
- workshop meshes and city presentation for remote industries;
- textile, candle, rope, fur, wax, amber, copper, wool, cloth, wine, pitch and tar chains;
- contracts, privileges, tariffs, embargoes and regional politics;
- deeper transport modes and player control over intra-region wholesale exchange.
