# Preserved fish — isolated implementation

The user approved main-project integration after reviewing the separate candidate. Preserved fish is now integrated in the normal `Hansa.uproject`: runtime, accepted Core assets, native inspector controls and `/Game/Mesh/hansa-fish-preservation/` model are installed. No candidate launch flag is needed. The original isolated project and delivery archive remain historical reproduction artifacts. See `PreservedFishMainIntegration.md` for the main build and real-map GUI evidence. Firewood's separately approved promotion is being sequenced additively after preservation catalog v25.

## Gameplay

An ordinary fishery remains useful without salt. Pay once for a Salting Shed on its existing 3 x 2 parcel. Building and production IDs remain stable. An in-progress batch finishes before the upgrade starts; construction then unlocks both modes. Selected and active mode are separate, with changes only between batches. Optional fresh fallback retries salted production when its complete input reservation can succeed. Inputs are reserved atomically and consumed once under the existing production contract.

| Value | Implemented setting |
| --- | --- |
| Fresh catch | 20 edible kg / 60 ticks; 2 laborers |
| Salted catch | 20 edible kg / 90 ticks; 4 laborers; 4 kg salt + 0.2 barrel |
| Barrel interpretation | One aggregate barrel packages 100 edible kg of fish |
| Upgrade | 8 planks + 4 timber + 2 tools; 2500 raw money units; 72 construction ticks |
| Fresh daily spoilage | 500 basis points (5%) |
| Preserved daily spoilage | 10 basis points (0.1%) |
| Food value | Equal edible mass; fresh first; one shared Fish requirement |

`Good.Fish` retains its identity and appears as **Fresh fish** in individual-good views. `Good.PreservedFish` is separate stock and trade cargo. `Need.Fish` remains **Fish**. Alternative fulfillment never grants an extra satisfaction bonus. Access, purchasing power, physical reservations and a shared remaining demand bound consumption. Cohorts share scarcity deterministically with rotating fractional allocation. Actual supplied goods and edible fulfillment are recorded; bread's population-growth reserve guard is unchanged.

The city policy **Available to households / Reserved for trade** excludes preserved stock from household consumption and usable food reserve reporting. It is distinct from physical cargo/input reservations and each route action's minimum-source reserve. Per-good production totals survive mode changes. Salted catch is a validated internal-catch recipe, not a purchaser of fresh fish.

Spoilage applies to physical inventories and local-hauler cargo exactly once, using simulation minutes and a global fixed-point carry per good. Splitting or transferring stock cannot eliminate fractional loss. Physical reservations, pending incoming quantities and vehicle cargo projections follow damaged stock. A batch with damaged reserved perishables must restart rather than receiving full output. Loss has its own ledger sink and cumulative report.

## Salt, trade and units

Lübeck has no saltworks or unrestricted salt extraction. Rostock redistributes salt through its bounded market: 2 kg background supply per market update, against the existing 1 kg combined background demand, capped by the existing three-times-desired-reserve rule (36 kg). This is an imported-supply abstraction, not a claim of local salt production. No recurring preserved-fish supply is granted to the player.

Foreign market-only route loads now buy actual transferred goods at the authoritative current market price; unloads receive sale proceeds. Purchases round up and sales round down at the smallest money unit. Affordability, cargo capacity, supplier availability and minimum source reserve limit quantity before transfer. Local port transfers remain internal movements. Prices and settled money are retained in transfer records, events, hashes and saves. This applies consistently to all goods on the existing foreign market route; it closes the previous free-transfer behavior needed for real salt purchases.

Money uses the existing runtime raw-unit/display convention (the HUD divides raw money and milli-mark prices by 1000). Legacy field names ending in `Pfennig` are not a newly asserted historical conversion. The upgrade therefore shows 2.5 in the existing money display for 2500 raw units. Cargo uses aggregate good quantities; packaging tare weight is not separately simulated. Salt and barrels add no edible food value.

Beer is unchanged: 3 malt + 1 hops + 1 barrel -> 5 beer / 100 ticks, 4 laborers and 2 artisans. Its barrel ratio is not consistent with the new 100-edible-kg fish container interpretation. A later beer change should first define liquid-volume/container units, then rebalance explicitly. Barrel returns, species, aging, quality tiers, firewood and new cities are outside this implementation. Lüneburg and Oldesloe remain future trade/brine-source directions.

## Measured balance

The deterministic modest-stock fixture uses the actual catalog, real route commands, physical ship cargo, current prices and ship upkeep. It starts with 100000 raw money, 2 barrels, 12 malt, 4 hops, 12 planks, 12 timber, no salt or fish, and prebuilt staffed fishery/cooperage/brewery. Cooperage opens at tick 300 to test shortage recovery and beer competition. It runs 1440 ten-minute ticks (10 game days). This is an explicit economic fixture, not the normal New Game grant or a complete spatial household campaign.

| Strategy | Ending fresh kg | Ending preserved kg in city | Salt purchases (raw money) | Fish sale proceeds | Ending cash |
| --- | ---: | ---: | ---: | ---: | ---: |
| Fresh only | 381.552 | 0 | 0 | 0 | 100000 |
| Build reserve | 121.201 | 199.435 | 86920 | 0 | -2640 |
| Export with 10 kg route reserve | 121.207 | 29.995 | 105502 | 306455 | 285233 |

The route buys at most 1 kg salt per visit; an earlier 8 kg setting overbought salt and exhausted cash. All strategies produced 20 beer. Export ended with another 7.228 kg fish aboard. The reserve route continued sailing for all ten days: its negative ending balance comes from ongoing ship upkeep after cash-limited purchases, an existing debt behavior. Stop/reduce imports once a sufficient reserve is built. This candidate does not add a route stock-target rule. The low-cash case starts with 1000 raw money, purchases only 879, delivers 0.410 kg salt, and cannot start a salted batch; upkeep can still create debt.

A separate 30-day stock test starts with 100 kg of each fish good: fresh ends at 22.307 kg, preserved at 97.045 kg. One inventory and 100 split inventories produce identical totals. Preservation therefore has a measured storage benefit and a profitable export case, while fresh-only avoids imports and yields more immediate food. Wages, construction amortization, household sales and full-city logistics are not included in these strategy profit figures; do not treat them as complete campaign profitability.

## Authoring and compatibility

Catalog v25: **D73BFD73C23C2D03**, 102 definitions, predecessor v24 **C1BDF313543BF44A**. Native save/reopen and full reverse lineage are verified. Reapplying the content commandlet changes zero assets. The temporary pre-save hashes are not valid catalog pins.

Additive reflected fields: need alternatives with ordered fulfillment factors, explicit spoilage enablement, and internal-catch recipe reference. Metadata, JSON schema and deterministic content hashes cover these fields. Empty alternatives and disabled spoilage retain old default hashes. The compiler validates duplicate/missing alternatives, factors, source recipe/output/cycle, required salt/barrel inputs, shoreline source and same-footprint upgrade transitions. The editor's validation list exposes sorted reverse dependencies for needs, recipes, upgrade targets, construction costs, tiers and city markets; source-recipe changes identify their internal-catch dependants. Native asset serialization/import and schema-constrained generation use the same reflected contract; no provider integration is introduced.

Save envelope 9, fingerprint 22, command schema 7 persist requested/active recipes, fallback, paid upgrade state, per-good output totals, city household policy, actual need source mix, spoilage carry/loss and priced route settlement. Fingerprint 21 remains reserved for the separate firewood work. Historical incompatible catalogs receive a clear rejection requiring original content or an explicit migration. No registry check is bypassed and no old file is overwritten. Generic seasonal code inherited at isolation is dormant: this catalog contains no Good.Firewood or firewood recipe.

## Native UI and images

Component inventory: existing HUD/navigation, production inspector, need details, market/warehouse/route lists, shared buttons and focus, upgrade progress, fresh/salted mode controls, fallback, household policy, source mix and blocker/loss tooltips. States include selected, focused, disabled with cause, pending construction, missing inputs and rejected-command feedback. Native text carries changing values; no full-screen generated artwork ships as UI.

The composed reference and separate controls reference are in `Docs/Images/UI/PreservedFish/` with prompt records. The final composed reference is native 1536 x 1024, controls reference 1254 x 1254, both built-in ImageGen. The real good-icon master is `SourceArt/UI/PreservedFish/preservedfish--good--default--1254x1254--v1.png`, RGBA, built-in generation, original prompt beside it. Thirteen proportional display variants (16 through 160 pixels) use the explicit GUI-resizing exception, preserving the master. `display-size-review-final.png` verifies actual sizes on linen/navy; tiny versions rely on their native text label.

Real game captures and semantic TSVs are under `Docs/Images/UI/PreservedFish/Verification/`. Tests use ordinary New Game, valid shoreline/road construction, native inspector upgrade/mode/fallback actions and focus, at 1280 x 720 and 1920 x 1080, standard and large/high-contrast settings. Controls, selected/active status and missing inputs are readable; detail content scrolls at 720p. The isolated preview fixture contains template terrain and a multiple-directional-light warning; these captures prove UI/runtime interaction, not final city-art acceptance. They do not yet cover every household-policy and trade-screen interaction in one end-to-end campaign.

## Model delivery

`SourceArt/Generated/Buildings/HansaFishPreservation_20260916/` contains the packed editable R4 Blender master, scripts, GLB/FBX, native ImageGen oak/thatch inputs and prompt records, twelve turntable frames/video, source/reimport renders, reference manifest, provenance and material gap ledger. Three actual correction cycles, clean export/reimport, packed-master reopen and Unreal save/reopen were completed.

The actual mesh `/Game/Mesh/hansa-fish-preservation/SM_HansaFisheryPreservation` is bound to the upgraded fishery in the main project and the historical isolated candidate. Bounds: 11.11333 x 7.42 x 4.92 m in the 12 x 8 m parcel. Three LODs: 42724 / 21362 / 10680 triangles. Six convex hulls fit the XY parcel; ten assigned material slots; three imported 1254-square texture assets. Native Unreal hero and close-up are in `renders/`. City-scale readability is verified. Strong tiled grain, simplified opaque glass, inherited coarse worktable fish and incomplete transferred source microrelief remain visible limitations; no photorealistic-close-up claim is made.

## Validation evidence and limits

Logs live in `Saved/GenerationJobs/preservedfish_20260916/`. `final-tests-1.log` passed 26 preservation, population, trade, catalog, schema and scenario tests. `balance-final.log` passed the final four-strategy/low-cash test with save/load during transport. UI logs `ui-1280-7.log` and `ui-1920-final.log` pass the native flow. `authoring-save-final-2.log` passes the final schema, reverse-impact and save suite. Across final runs, 45 distinct tests pass; the delivery manifest lists every test.

Win64 Editor Development and standalone Shipping builds pass. `shipping-cook-final.log` passes an Engine Entry smoke cook/package. `shipping-boundary-audit-final.json` checks actual IoStore and staging manifests: production preservation assets and all 13 icons are present; generation workers, provider configuration, editor/test/automation modules, staging content, preservation preview and baseline fixtures are absent. This is not a full default-city release package or performance soak.

Remaining broader release acceptance: a full household/trade campaign and longer spatial economy balance. Main integration and the fishing-hut upgrade GUI on the configured Lübeck map are now verified separately. The main merge retained heating/firewood source changes; earlier snapshots and the main merge report document the boundary. No concurrent firewood work was reverted.
