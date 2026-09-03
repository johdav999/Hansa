# Population, residences and the city loop (S04-P01 through S06-P03)

## Authoritative definitions

The MVP population content is authored as ordinary reflected definition assets and compiled into the same immutable `FHansaEconomicRegistry` used by production:

- five needs: bread, fish, beer, tools and basic services;
- `PopulationTier.Laborer`: bread, fish, beer and basic services, with 60% workforce supply;
- `PopulationTier.Artisan`: stronger bread/beer expectations plus tools and basic services, with 70% skilled workforce supply.

Per-tier requirements own consumption rates and importance weights. A good need must reference an existing `Good.*` definition and consume a positive number of milli-units per resident per tick. A service need has no inventory good and must consume zero. Tier progression has exactly one base tier and an acyclic chain. The compiler fails closed with stable `HSA-REGISTRY-011` through `016` diagnostics for missing goods/needs and invalid consumption/progression.

The Authoring Studio discovers these types through the generic reflected schema and Details workflows. It does not have a population-specific form or a second validation model.

## Deterministic cohort system

Each authoritative residence cohort has stable cohort, residence, city, inventory and tier identities plus current residents and residence capacity. Completed constructed residences now provide the authoritative tier and capacity: laborer residences host `PopulationTier.Laborer`, artisan residences host `PopulationTier.Artisan`, and unfinished residences provide neither needs consumption nor workforce. A completed placed residence creates its missing cohort against the city's canonical inventory. The fixed simulation phases process cohorts and tier needs in canonical stable-ID order.

For each good need:

- required consumption = residents × authored per-resident rate;
- access requires a city-owned consumption inventory, a city market, and available stock;
- affordability is the cohort's bounded purchasing-power factor;
- reliability is fulfilled affordable demand divided by affordable demand;
- consumption is the lesser of available stock and affordable demand, recorded as an explicit `PopulationConsumption` inventory sink;
- reserve days use integer milli-days derived from pre-consumption available stock and the simulation clock's minutes per tick.

Service needs use separately authored access and reliability inputs and consume no inventory. Need satisfaction is the minimum of access, affordability and reliability. Cohort factors and satisfaction are importance-weighted integer averages in `0..10000`; UI and automation code must display these projections rather than recompute formulas.

Workforce is `residents × tier workforce basis points / 10000`. Building-recipe production may opt into city workforce assignment; opted-in rows compete in stable production-ID order, receive laborer and artisan allocations from their city, and expose the existing typed workforce blocker when supply is short. Because production precedes needs in the fixed pipeline, migration produced by a needs evaluation becomes allocatable on the following tick.

Consecutive satisfaction above/below the tier thresholds produces an authored growth/decline step after the evaluation interval, clamped to `0..residence capacity`. The projection exposes both the last resident change and a derived `Declining`, `Stable`, or `Growing` trend so migration remains explainable without clients interpreting the sign themselves. A satisfied laborer residence can be advanced explicitly through `UpgradeResidence` to its authored same-footprint artisan residence. The command preserves building identity, changes the building definition and cohort tier/capacity together, and emits `ResidenceUpgraded`; invalid ownership, construction state, progression, satisfaction, or capacity fails atomically.

`FHansaCityPopulationProjection` is the sole city-loop read model for UI and automation. It reports population, typed trend and last-tick change, operational housing capacity, residents and workforce by tier, assigned and available workforce, resident-weighted satisfaction, market access, and bread reserve in deterministic milli-days. The `city.population` automation query returns that projection without recomputing simulation rules. The `population.cohort` query exposes one canonical cohort's operational and market state, workforce, satisfaction factors, and per-need required/consumed quantities and reserve evidence. Both queries are read-only and use stable IDs.

Population cohorts, residence/market status and need state are included in snapshots and the version-13 determinism fingerprint as the `Population` subsystem. Automatic-workforce participation is included in the `Productions` subsystem. Discovery/container order is canonicalized at state creation. S04-P02 consumes the already-published required/consumed quantities as separate citizen and unmet-demand inputs to [Market.md](Market.md); the market never reimplements need formulas. S04-P03 also consumes each matching need's affordability and reserve days for typed consumer queries and affordability alerts.

## Verification

`Hansa.Simulation.Population` covers consumption, factor separation, reserve days, constructed and unfinished residences, missing market access, manual residence progression, workforce shortage and recovery, city projections, bounded growth/decline, and a 1,000-tick repeatability run. `Hansa.Content.Definitions.PopulationValidation` covers missing goods/needs, impossible service consumption, cyclic tier progression, missing residence-tier links, and invalid residence upgrade chains. The generic economic schema and on-disk asset reload tests include both population definition classes and the residence linkage.
