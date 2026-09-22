# Implement preserved fish in Hansa

Implement a complete preserved-fish gameplay feature for the Lübeck region, following the requirements below. Inspect the current implementation first, then carry the work through runtime, authoring, UI, automation, validation, and documentation. Treat this as an extension of the current MVP from thirteen to fourteen goods while retaining the four existing production-chain families.

## Repository contracts and initial inspection

- Follow the repository AGENTS.md instructions. Read Docs/MVP.md and Docs/EditorArchitecture.md completely; read Docs/EditorMVP.md for affected authoring-editor work.
- Before any UI or game-image work, read Docs/UIDesignBrief.md and Docs/UIAssetWorkflow.md completely. Use the imagegen skill where required and follow the existing asset approval and staging workflow.
- Inspect the current production Data Assets, compiled catalog, recipes, household consumption, inventory reservations, logistics, spoilage-related fields, trade, construction upgrades, and save compatibility. Accepted Data Assets remain authoritative; seed definitions and documentation must agree with them.
- Useful starting references include Docs/Development/StarterEconomyBalance.md, Docs/Development/BeerProductionChain.md, Docs/Development/EconomicCatalogVersioning.md, and Source/HansaEditor/Private/Definitions/HansaEconomicDefinitionSeedCommandlet.cpp. Determine the actual current catalog version rather than assuming a historical version.
- Reuse existing systems where possible. Do not introduce an independent economy model or a parallel inventory system.

## Intended player experience

A basic fishery continues supplying affordable fresh fish to local households. The player can construct an optional Salting Shed upgrade, import salt, obtain locally produced barrels, and switch that fishery to preserved-fish production. Preserved fish costs more to produce but loses much less stock during storage and transport, making it useful for winter reserves and exports.

Preservation is optional. Existing fresh fisheries must remain viable, and laborers must not need salt imports merely to obtain basic fish food.

## Goods and household demand

1. Preserve the stable identity Good.Fish and display it as Fresh fish where individual goods are listed. Audit localization and references when changing its displayed name.
2. Add Good.PreservedFish as a separate tradable, storable good with its own price, stock, production, consumption, incoming supply, reserve, and spoilage reporting.
3. Keep the existing household need identity and display name Fish. Both fresh fish and preserved fish fulfill this one need.
4. Use equal fulfillment per equivalent quantity of edible fish initially. Preservation must not create extra food value or an additional satisfaction bonus.
5. Extend authored need definitions to support alternative goods and explicit fulfillment factors. Existing single-good needs must retain their behavior through defaults or a validated migration.
6. Consumption must reduce one shared remaining requirement; supplying both products cannot double fulfillment. Handle partial fulfillment, access, affordability, competing households, rounding, and inventory reservations deterministically.
7. Prefer fresh fish when both alternatives are accessible, affordable, and available to households; use preserved fish for the remainder. Define deterministic allocation so iteration order does not favor particular households.
8. Leave bread's existing population-growth reserve requirement unchanged.

## Fishery upgrade and production modes

Add an optional Salting Shed upgrade to the existing fishery. It has authored material, tool, money, construction-time, and workforce requirements. Preserve the fishery's identity and existing operation wherever practical. Determine whether it fits the existing occupied parcel; do not silently enlarge footprints or overlap neighboring buildings.

After construction, the fishery offers:

| Mode | Recurring inputs | Output |
| --- | --- | --- |
| Fresh catch | Existing fishery workforce | Fresh fish |
| Salted catch | Fishery workforce, additional processing labor, salt, empty barrels | Preserved fish |

- The fishery catches its own fish in both modes. Salted production processes that catch internally and does not take fresh fish from the market or another fishery.
- Preserve source-production validation and existing fishery placement/resource restrictions. Explain the internal catch in recipe metadata so the lack of a fresh-fish input is intentional and validated.
- Unlock both modes permanently after upgrading. Allow the player to change modes without demolishing the upgrade.
- Finish the current batch before applying a mode change. Reserve salt and barrels before starting a salted batch, and consume them exactly once under the normal production contract.
- Missing preservation inputs produce explicit Missing salt or Missing barrels blockers.
- Add an optional Produce fresh fish when preservation supplies are unavailable setting. Fallback occurs only at batch boundaries and automatically retries the selected salted mode when it can start a valid batch. Distinguish selected mode from currently active mode.
- Switching, pausing, upgrading, demolishing, saving, or loading must not discard reserved goods incorrectly, duplicate outputs, bypass staffing, or grant free production.
- Initially use additional laborers for preservation rather than requiring artisans. Set exact costs, staffing, batch sizes, and cycle durations through balance testing.

## Spoilage and packaging

Preservation must have a real storage advantage, not an arbitrary output multiplier.

- Audit and reuse existing spoilage support where possible. Implement simple deterministic stock loss without individual fish, per-item ages, or detailed batch-aging simulation.
- Fresh fish has a meaningful loss rate; preserved fish has a substantially lower rate.
- Apply consistent time-based rules across city, warehouse, building, and vehicle inventories without applying losses twice to shared stock. Explicitly define how reserved inputs and committed cargo are handled, and update reservations and incoming-supply projections safely when relevant quantities change.
- Use fixed-point accounting with deterministic fractional handling so splitting stocks or transferring goods cannot evade spoilage through rounding.
- Report spoilage separately from citizen consumption, industrial consumption, and trade.
- Define food value by edible fish quantity, excluding salt and packaging. Specify a coherent quantity of fish per barrel and use it consistently in recipes, units, cargo accounting, and descriptions.
- Review the existing beer barrel ratio for consistency. Report any recommended beer rebalance separately; do not silently change unrelated beer economics.
- Defer empty-barrel returns, packaging reuse, fresh-fish species, and food quality tiers.
- Aim for fresh fish to be cheaper for immediate local use, while preserved fish can justify its added cost over longer storage or voyages. Winter benefits arise from reliable stored supply and existing seasonal constraints, not an unexplained preserved-fish bonus.

## Salt supply and regional scope

- Do not add unrestricted salt extraction to the Lübeck city map.
- For the existing Lübeck–Rostock game, use bounded salt availability through Rostock's market as a trade supply abstraction. Do not claim that Rostock locally produced that salt.
- Connect fish preservation to real salt purchases, cargo, delivery, and industrial demand. No recurring free player stock or unlimited supplier inventory.
- Ensure local cooperage can supply preservation as well as beer, creating competition for barrels and timber.
- Document Lüneburg as a future major salt-supplying trade partner and Oldesloe as a future brine-source location. A later Oldesloe saltworks would require a specific brine resource, fuel, and labor.
- Do not implement Oldesloe, Lüneburg transport expansion, firewood production, saltworks, or additional rendered cities in this feature.

## Household availability and trade reserves

Add a simple city-level control for preserved fish: Available to households or Reserved for trade.

- Reserved-for-trade stock must be excluded from household availability and usable household food-reserve reporting.
- Define this as a household-consumption policy, distinct from physical cargo/input reservations and trade-route minimum-stock settings.
- Show reserved and household-available quantities clearly. Existing route minimum-stock rules must continue to operate separately for each fish good.
- Household consumption and trade loading must never spend the same units.

## Player-facing GUI

Use native UMG/Slate widgets and the existing component system.

- Fishery inspector: upgrade cost and construction progress, selected/active production mode, fallback setting, input needs, output, workforce, and actionable blockers.
- Laborer and artisan needs views: retain Fish as the need label. Expanded details show accepted products, actual supplied quantities or mix for the reported period, and remaining unmet demand. Derive percentages from food fulfillment, not mismatched item counts.
- Market, warehouse, production, and route views: list Fresh fish and Preserved fish separately.
- City stock controls: expose household availability and distinguish it from route reserves.
- Explain storage losses and preservation's benefit using actual simulation values.
- Add semantic IDs and complete mouse, keyboard, and controller interactions, readable focus, loading/disabled/error states, localization space, and accessibility checks.
- Follow the required component-first ImageGen workflow for new visual appearance, icons, and references. Preserve prompt records and generated masters. Shipping screens remain native interactive widgets.
- Represent the salting shed within a valid authored footprint using approved assets. Generated media remains staged until explicit approval; finish concrete reviewable work before requesting promotion approval.

## Authoring, compatibility, and documentation

- Update runtime definitions, reflected authoring metadata, schemas, validation, migrations, dependency/impact analysis, import/export, seed reconstruction, and applicable AI-generation contracts in the same implementation stream.
- Validate need alternatives, fulfillment factors, production modes, upgrade references, resource sources, spoilage settings, and recipe quantities. Reject duplicate alternatives, invalid factors, missing references, and invalid transitions.
- Make active mode, requested mode, fallback policy, upgrade progress, inventory policies, and any fractional spoilage state persistent wherever required by the chosen implementation.
- Version the catalog and save contract explicitly. Preserve existing saves; implement and verify an intentional migration or reject incompatible saves with a clear explanation. Never bypass registry-hash checks.
- Update Docs/MVP.md consistently for fourteen goods, preservation, need alternatives, and the bounded spoilage extension. Resolve conflicting exclusions and stale counts without broadening unrelated scope.
- Update production-chain and authoring documentation with final measured recipes, costs, rates, limitations, and approval status.
- Keep HansaEditor editor-only. Prove Shipping excludes generation workers, staging assets, automation surfaces, provider configuration, and fallback/test presentations.

## Verification and balance

Use meaningful deterministic tests plus real gameplay evidence. Do not claim balance from formulas alone.

Required cases:

1. Existing unupgraded fisheries and single-good household needs retain correct behavior.
2. Upgrade construction charges the correct goods and money and unlocks modes only when complete.
3. Fresh-only, preserved-only, mixed, insufficient, inaccessible, unaffordable, and trade-reserved supplies produce correct Fish fulfillment without double consumption.
4. Salt/barrel shortages, workforce shortages, mode switches, automatic fallback, pause/resume, and demolition preserve inventory and reservations.
5. Spoilage is deterministic, recorded, bounded by stock, consistent in cargo and storage, and cannot be avoided by inventory splitting or transfers.
6. Salt imports and fish exports use normal Lübeck–Rostock route commands and authoritative cargo. Incoming-supply figures track actual deliverable quantities.
7. Fresh production remains an economically viable local strategy. Preservation becomes worthwhile in at least one measured storage/export scenario without dominating every strategy.
8. Population growth and decline remain understandable; bread's existing growth guard is preserved.
9. Save/load during construction, each production mode, pending mode changes, fallback, transport, and spoilage preserves authoritative state and determinism.
10. Editor changes compile into the same runtime behavior and pass schema, migration, catalog, and impact-analysis checks.
11. The ordinary player GUI supports the complete flow, with correlated semantic/state evidence and native 1280×720 and 1920×1080 captures.

Run balance playthroughs using an explicit modest-starting-stock test profile. Separate construction-testing grants from balance evidence without silently changing the user's normal New Game settings. Include fresh-food, reserve-building, and export strategies; test shortage recovery and competition with beer for barrels.

## Delivery order and completion report

1. Inspect the current systems and record a concise implementation plan, exact schema changes, and initial balance hypotheses.
2. Implement simulation and data contracts with editor support and targeted tests.
3. Implement native GUI controls and reviewable salting-shed/icon assets under the required approval workflow.
4. Verify integrated gameplay, trade, save/load, balance, and Shipping boundaries; fix defects found.
5. Report changed files and assets, final recipe/cost/rate tables, measured results, visual evidence, save compatibility, and remaining limitations or approvals.

Deliver this as one coherent preservation feature. Do not mark it complete with only a new good, an upgrade button, or salt/barrel costs: alternative-food fulfillment, meaningful storage benefits, usable controls, authoring parity, and verified gameplay are all required.
