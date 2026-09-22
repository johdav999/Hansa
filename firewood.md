# Implement firewood for household heating and workshops

Implement a complete firewood economy in Hansa's Lübeck scenario, including production, workshop fuel, seasonal household heating, protected household reserves, authoring support, and verification. Work through implementation and testing; do not stop at a plan. Preserve unrelated changes and player saves.

## Scope and project contracts

Read the applicable AGENTS.md and the complete Docs/MVP.md and Docs/EditorArchitecture.md before planning. Read Docs/EditorMVP.md for authoring work. Before any UI or visual work, read Docs/UIDesignBrief.md and Docs/UIAssetWorkflow.md completely and follow the required ImageGen/component workflow. Use applicable asset-generation skills for new artwork and models.

This task explicitly authorizes expanding the MVP from thirteen to fourteen goods and adding a fifth locally buildable chain: firewood. Update the MVP and related active design documentation consistently, including stale goods/chain counts. Preserve the two-population-tier and Lübeck–Rostock scope. Do not add charcoal, disease, individual temperature simulation, forest depletion, smoked fish, or additional cities in this task.

Inspect current code, accepted Data Assets, catalog manifests, and runtime initialization before editing. Historical proposals and old fixtures are not the current balance authority. Useful starting references include Docs/Development/StarterEconomyBalance.md, BeerProductionChain.md, LumberTreeProximity.md, and EconomicCatalogVersioning.md. Revalidate all numbers against the current accepted catalog.

Maintain game/editor/automation parity. Gameplay changes must include schema metadata, validation, migration classification, impact analysis, applicable generation/import support, and meaningful tests. Runtime must remain independent of editor/provider code. Generated content enters staging with provenance and requires the existing explicit promotion approval; do not interpret this prompt as approval of unseen generated assets. Finish all independently authorized implementation and verification before presenting any concrete approval request.

## Intended player experience

A woodcutter's yard turns timber into firewood. Households use firewood for heating in cold seasons. Bakeries, malt houses, and breweries consume it for process heat throughout the year. Winter makes the shared supply more valuable. Players can protect household fuel, accepting workshop pauses or reduced exports instead of losing heating.

The flow must work through ordinary construction, roads, logistics, markets, inspectors, save/load, and trade. Do not implement a parallel inventory, debug-only production path, or cosmetic fuel meter.

## 1. Firewood production

Add stable definitions following the repository's actual conventions, provisionally:

- Good.Firewood
- Recipe.SplitFirewood
- Building.WoodcutterYard
- Need.Heating

The production graph is:

```text
Lumber Camp -> Timber -> Sawmill -> Planks
                     -> Cooperage -> Barrels
                     -> Woodcutter's Yard -> Firewood
```

Use an explicit timber input and firewood output. Choose and document a coherent unit and conversion ratio. Require laborers only and modest construction materials/currency so heating is accessible before artisans. The yard processes delivered timber; it does not harvest standing trees or require its own forest proximity. Preserve the lumber camp's current tree-access rules.

Integrate the good and building into the accepted catalog, construction navigation, placement, workforce, inventory, local deliveries, market price/history, trade, and production inspection. Reuse the shared timber dependency rather than creating another lumber camp definition. Ensure sufficient early workforce and construction supplies to bootstrap the complete supply path without an artisan dependency cycle.

## 2. Workshop process heat

Add firewood to the existing recipes for:

| Workshop | Recipe inputs after this change |
| --- | --- |
| Bakery | Flour + firewood |
| Malt House | Grain + firewood |
| Brewery | Malt + hops + empty barrel + firewood |

Preserve existing outputs and unrelated economics initially; change them only when measured balance results justify a documented adjustment. Do not add firewood consumption to fisheries, mills, sawmills, or cooperages.

Use normal multi-input reservation and batch execution. A batch cannot start without its complete inputs, including fuel. Preserve existing in-progress batch semantics and account for fuel exactly once. Paused or blocked workshops do not continuously burn firewood. Cancellation, demolition, and refunds must follow existing conservation rules.

Fuel demand is independent of household season multipliers. Provide causal feedback distinguishing missing fuel from fuel protected for households, including an actionable remedy.

## 3. Seasonal household heating

Implement Need.Heating as a good-backed need fulfilled by firewood using existing cohort consumption and market/service access. Apply it to both occupied laborer and artisan residences, including compound variants; an artisan upgrade must not remove the heating requirement. Cosmetic child houses must not multiply demand.

Use resident-based consumption for this first version. Author the base rate and season multipliers as validated, editable data using existing season infrastructure:

| Season | Initial multiplier proposal |
| --- | ---: |
| Summer | 0% |
| Spring/autumn | 40% |
| Winter | 100% |

These are initial balance proposals, not immutable final values. Use deterministic fixed-point arithmetic and avoid systematic truncation of small consumption amounts. Do not infer season from wall-clock time. Define predictable behavior at season transitions and for scenarios with a fixed season.

When heating demand is zero, exclude its weight from satisfaction normalization; do not grant free satisfaction or divide by zero. In cold seasons, unmet heating reduces satisfaction through the existing sustained growth/decline system, with no immediate deaths or separate health model.

Rebalance need weights and thresholds together. Preserve a viable summer opening with bread and basic services, and growth with adequate staple food. Reliable heating should matter in winter. Beer should remain optional for laborer growth when other relevant needs are satisfied. Preserve applicable access, affordability, evaluation-window, and food-reserve gates.

Empty residences consume no fuel. Ensure founding residents and population increases do not bypass any deliberately introduced heating requirements or create an impossible bootstrap. Verify both tiers and all relevant residence variants.

## 4. Protected household fuel reserve

Add a configurable per-city household heating reserve expressed in days, plus an explicit player override to release protection. Use existing inventory, ownership, command-validation, and reservation systems; never duplicate stock in a new reserve inventory.

Compute the protected quantity from household heating demand only. Exclude workshop demand. Define and document the treatment of current stock, warehouse/building stock, existing batch reservations, household allocations, export reservations, incoming cargo, and inaccessible inventory. Stock must never be counted or promised twice.

Workshops and exports may claim only the physically available, unreserved surplus above the protected household quantity. Household consumption can use protected stock. Apply protection to every relevant outbound path, including manual sales, route loading, and merchant actions where those can access the protected owner inventory. Do not seize another owner's goods or let the policy override ownership permissions.

Preserve valid reservations already committed to an in-progress batch or departure; policy changes cannot silently confiscate them. Explain committed stock and any reserve deficit. Ensure the policy also prevents new industrial deliveries from draining protected stock into workshop buffers before batch reservation.

Author a modest default reserve and an appropriate configurable range. Handle summer's zero demand explicitly. Show an upcoming-winter target or warning separately so a zero summer target does not conceal the need to stockpile. Do not count hypothetical future production or empty planned routes as available fuel.

Player reserve changes and overrides must use normal authoritative commands, persist in saves, respect multiplayer ownership, and be available through semantic automation. Specify whether the override persists and show its active state clearly.

## 5. Feedback and presentation

Extend existing market, city overview, residence, and production inspectors with:

- Firewood stock and available surplus.
- Household demand versus workshop demand, using clearly labeled time units.
- Seasonal demand multiplier and heating fulfillment.
- Protected target, committed quantities, and household reserve days.
- Approaching-winter shortage warning with a concrete remedy.
- Missing fuel versus protected-fuel workshop blockers.
- Reserve control and explicit override state.

At zero heating demand, show an appropriate not-applicable state instead of a misleading zero-day shortage. Derive all values from authoritative projections and expose stable semantic identifiers. Preserve controller/keyboard access and localization room. Follow the approved UI design system and validate assembled screens at supported native reference resolutions.

Provide the firewood good/building artwork and yard presentation needed for a finished player-facing chain under repository asset rules. Preserve generated masters, prompt records, provenance, and inspections. Reuse approved assets where appropriate. Do not present an Engine primitive, full-screen mockup, or unapproved staging asset as completed Shipping presentation. If promotion needs approval, deliver the exact reviewable asset/diff and clearly separate that remaining gate from completed runtime work.

## 6. Balance and scenario integration

Initial capacity target: one fully staffed woodcutter's yard should approximately supply winter heating for 50 laborers plus the bakery in one current bread chain. Treat this as a simulation target, accounting for conversion losses, workforce, lumber-camp throughput, and real logistics. Beer/malting should create additional fuel demand.

Measure the combined load from construction, planks, barrels, and firewood on timber production. Do not tune the yard in isolation while leaving its upstream supply impossible. Check early workforce participation and the artisan requirements of existing downstream industries.

Use a dedicated balance scenario with modest explicit starting reserves and no construction-testing grants or hidden recurring supply. Preserve existing development-grant behavior unless its removal is separately requested; do not confuse that opening with balance evidence. Provide a recoverable firewood opening for ordinary New Game and explicit bounded Rostock supply/import behavior as needed. Do not allow background imports to mask a local-production test.

Record actual quantities, cycle times, workforce, household rates, weights, season timing, reserve policy, and operating costs. Prove viable summer startup, winter survival, and recovery; do not declare balance from recipe arithmetic alone.

## 7. Authoring, persistence, and release

Expose all new tuning and policy defaults in the normal authoring system. Validate references, positive recipe quantities/cycles, legal multipliers/weights, reserve ranges, and population requirements. Extend economic dependency/impact analysis to show timber competition and effects on bread, malt, beer, household demand, trade, and satisfaction.

Update canonical seeds, accepted Data Assets through the normal reviewed workflow, full catalog manifests/hash pins, fixtures, and tests together. Verify deterministic compilation independent of discovery order. Document and implement an explicit save decision: a tested named migration or clear version incompatibility, never silent acceptance or player-save deletion. Preserve current-catalog save/load determinism, including season, reserve settings, overrides, and outstanding reservations.

Keep provider credentials, generation workers, staged media, editor modules, automation, and development-only data out of Shipping. New content must respect the existing approval and release gates.

## 8. Required verification

Add meaningful targeted tests and run relevant existing suites. Cover at least:

1. Timber-to-firewood production conserves inputs/outputs and uses normal workforce/logistics.
2. Each affected workshop consumes the authored fuel once per completed batch; missing fuel blocks correctly; pause/resume/cancel follow established accounting.
3. Summer consumes no household fuel and excludes heating from satisfaction normalization. Shoulder-season and winter demand are deterministic, including fractional accumulation and transitions.
4. Cold shortages cause sustained satisfaction/population effects and recover when fuel returns; laborer growth does not require beer when other relevant needs are met.
5. Residence compounds do not multiply household demand; upgrades retain heating.
6. Reserve protection holds under competing workshops, industrial delivery requests, route loading, manual sales, and applicable merchant actions. No stock is double-reserved or created.
7. Changing the reserve or override respects ownership and previously committed stock, produces clear feedback, and survives save/load.
8. Normal startup is viable without artisan-dependent firewood production, free recurring fuel, or test grants. Actual road/delivery distances are included.
9. A full seasonal balance run supports the target population, enters winter with planned reserves, survives, and recovers from a bounded supply interruption.
10. Increasing brewing demand measurably drains fuel surplus. Household protection pauses fuel-consuming workshops before new industrial allocations consume protected household fuel.
11. Editor schema/validation, impact analysis, catalog reload, save compatibility, and deterministic replay remain valid.
12. Ordinary player controls and semantic automation can build the yard, inspect heating, set reserves, observe a blocker, override it, and restore protection. Capture correlated native viewport and simulation evidence.

Use mocked/offline tests in ordinary CI. Run no live generation provider calls from CI. Clearly distinguish fresh test results, historical evidence, theoretical capacities, and unverified limitations.

## Delivery

Deliver implementation, authored data, scope/design updates, a concise balance report, and reproducible verification evidence. Report changed paths, final production graph and tuning, measured population support, reserve behavior, test results, save compatibility, and remaining approval or release gates. Do not claim completion of a visual, live-play, or Shipping gate from headless checks alone.
