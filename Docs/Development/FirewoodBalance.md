# Firewood balance evidence

This is a review candidate, not an accepted balance release. Source of truth is the reload-verified staged catalog in `FirewoodCandidate.json`; accepted catalog v24 remains unchanged.

## Authored tuning

Firewood inherits timber's kilogram unit. One raw inventory unit is one gram (a thousandth of the displayed kilogram); quantities below are game abstractions, not a physical heating-energy model.

| Item | Candidate setting |
| --- | --- |
| Yard batch | 6 kg timber → 5.4 kg firewood, 100 ticks; 10% splitting/handling loss |
| Yard workforce | 2 laborers, 0 artisans |
| Yard construction | 6 timber, 2 planks, 0.5 tools, 1,800 pf; 3 × 3 cells / 12 × 12 m plot |
| Yard storage | 120 kg-equivalent game units |
| Bakery fuel | 0.05 kg per batch, existing 20-tick cycle |
| Malt-house fuel | 0.10 kg per batch, existing 60-tick cycle |
| Brewery fuel | 0.20 kg per batch, existing 100-tick cycle |
| Household winter demand | 0.001 kg per resident per tick, both tiers |
| Calendar | 90-day summer, autumn, winter, spring; multipliers 0%, 40%, 100%, 40% |
| Reserve default / limits | 3 days / 0–90 days; explicit release persists until restored |
| Heating weight | 4,000, added to existing tier weights only when season multiplier is nonzero |
| Firewood opening profile | 60 kg in authored city profiles; the legacy shortage scenario uses desired reserve + 10, so its actual pool opens at 70 kg |
| Rostock background supply | 1.25 kg production − 0.75 kg citizen demand per market update, bounded by existing market rules |

At the normal 60-minute tick, theoretical full-utilization yard output is 1.296 kg/day. Fifty residents demand 1.2 kg/day in winter; a fully utilized bakery adds 0.06 kg/day. That leaves only 0.036 kg/day before delivery and staffing delays. Malt and brewing add 0.04 and 0.048 kg/day at nominal utilization. These are arithmetic ceilings, not measured support guarantees.

One lumber camp produces 6 timber per 100 ticks. A yard consumes that entire nominal output. Sawmill demand is 5 timber/75 ticks, and cooperage demand is 3 timber/75 ticks. Running one of each continuously requires 16.67 timber/100 ticks before construction: about 2.78 nominal camps, with further delivery headroom. Sawmills/cooperages keep their existing artisan requirements; the yard introduces none. No new upkeep charge is invented: the yard uses existing construction and workforce economics.

## Dedicated measurement

`Hansa.Integration.Firewood.SeasonalBalanceMeasurement` uses the actual compiled candidate definitions, five occupied laborer residences (50 people), a market, lumber camp, farm, mill, bakery, fishery and yard. It runs a full 360-day year with a seven-day yard shutdown on days 200–207. It uses physical road travel, normal four-job delivery concurrency and actual population workforce. No imports, merchant AI, background production, recurring grants or manually allocated workshop workers are supplied.

Explicit finite starting stocks: 60 bread, 40 fish, 30 grain, 20 flour, 12 timber, 20 firewood and 4 tools. The buildings are prebuilt to measure operating capacity; this fixture does not prove a paid New Game construction opening. Ordinary yard construction is tested separately through the real command gateway.

Daily CSV evidence is saved under `Saved/GenerationJobs/Firewood_20260916/`. The original long-road experiment is retained as `seasonal-long-road-failed.csv`: it fails the 50-person target. Bread deliveries become intermittent before winter, population falls, and staffing losses compound the shortage, despite accumulating firewood. At winter entry the household pool contained 152.68 firewood; lack of fuel was not the initial failure. This is evidence against claiming sustainable 50-person support from recipe arithmetic alone.

The revised central-market layout is measured separately in `seasonal-balance.csv`. Its result and any final tuning decision must be recorded before acceptance. No production recipe outputs have been changed to conceal the food/logistics issue.

## Final measured layout — passed

The final operating layout keeps the same definitions, worker costs, four-cart limit and finite reserves. Market anchor is (14,5); all workshops sit immediately south of the road at y=9: farm x=6, mill x=10, bakery x=13, lumber camp x=16, yard x=19, fishery x=22. Homes remain north of the road. Nearby surveyed tree cells support the lumber camp; the yard requires delivered timber only. This reduces timber shuttle distances without bypassing road travel.

`FirewoodBalance.csv` contains all 360 daily checkpoints. Population stayed at exactly 50 throughout the year. Winter began at tick 4,320 with 186.65 kg firewood, 1.2 kg/day household demand and a 3.6 kg protected target. The yard was paused for seven winter days, then resumed through the normal production command. Year end: 50 residents, 240 kg firewood in the household pool, 10.5 bread, 79 completed yard cycles, 95% current satisfaction and no active yard blocker. Four integration tests passed in `Saved/GenerationJobs/Firewood_20260916/tests/20260916-204545558-automation-Hansa.Integration.Firewood`.

This proves this explicit operating layout, including the interruption; it does not promise that any road layout supports 50. The intermediate central-market layout retained 50 through winter but ended spring at 40; both unsuccessful layouts are preserved under Saved evidence. No unrelated recipe output was retuned. The accumulated seasonal surplus also means this run does not establish a permanently sustainable winter-only capacity of 50 without summer stockpiling. Dedicated brewery-drain and paid no-grant New Game playthroughs remain unverified.
