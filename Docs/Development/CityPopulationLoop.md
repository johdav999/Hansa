# S06-P03 city population loop

S06-P03 connects the previously separate construction, population, market, and production slices without introducing post-MVP civic systems.

## Authoritative flow

1. Construction completes a placed residence.
2. Residence synchronization applies the building definition's hosted population tier and capacity and creates a missing cohort against the city's canonical inventory.
3. An empty operational home with physical market access and fully satisfied `Need.BasicServices` immediately receives an initial two residents (bounded by capacity); goods demand begins on the following tick.
4. Operational cohorts publish tier workforce. Active building-recipe productions receive one worker each in stable production-ID order where possible, then remaining labor fills authored slots in that same order.
5. A staffed production runs at `assigned workforce / required workforce` speed. One worker is sufficient to start; the effective batch duration is `ceil(base batch ticks * required / assigned)`. A unit blocks for workforce only when it has no assigned worker.
6. Needs evaluation requires both the city market and stocked city consumption inventory, consumes goods through the ledger, and updates satisfaction.
7. Sustained satisfaction drives bounded migration. New workforce participates on the next simulation tick.

The flow is integer-only, stable-ID ordered, and uses the existing fixed simulation phases. UI, automation, and editor code consume projections and diagnostics instead of reproducing these rules.

## Residence progression

`UHansaBuildingDefinition.ResidentPopulationTierId` is required on positive-capacity residences and forbidden on non-residences. Compiler diagnostic `HSA-REGISTRY-019` enforces that link. `HSA-REGISTRY-020` requires an authored residence upgrade to preserve footprint and advance to the target tier whose `PreviousTierId` names the source tier.

`UpgradeResidence` is an explicit atomic gameplay command. It requires ownership, completed construction, a valid direct authored upgrade, qualifying source-tier satisfaction, and resident count within target capacity. Success updates the building, placement, cohort tier and capacity together and emits `ResidenceUpgraded`.

## Read model and automation

`FHansaCityPopulationProjection` reports:

- total residents, a typed `Declining`/`Stable`/`Growing` trend, last-tick change, and operational housing capacity;
- laborer/artisan residents and workforce supply;
- assigned and available laborer/artisan workforce;
- resident-weighted satisfaction and market access;
- the bread staple reserve in milli-days.

Automation exposes the same object through the allowlisted `city.population` query keyed by canonical `City.*` identity. The allowlisted `population.cohort` query returns one cohort's residence and tier identities, capacity, workforce, access/affordability/reliability/satisfaction factors, and per-need consumption and reserve evidence. Controlled automation can submit the same upgrade through `residence.upgrade`; it reaches the normal typed gameplay gateway and does not mutate fixture state directly.

## Acceptance coverage

`Hansa.Simulation.Population` covers operational versus unfinished residences, unsatisfied needs without market access, explicit residence progression, deterministic workforce shortage and next-tick recovery, city projections, growth/decline bounds, and long-run repeatability. Economic definition tests cover invalid residence tier links and progression chains. The state-hash contract is version 13.
