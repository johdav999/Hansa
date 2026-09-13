# Staple production capacity rebalance — 2026-09-13

User-approved catalog **16**, registry **B65512A7BFAC9E0C**, raises the complete
bread chain and fishery to five times their catalog-15 batch capacity. It is intended
to make production-chain layout strategic rather than repetitive as cities scale.

## Catalog 16 authored settings

| Producer | Input | Output | Batch ticks | Laborers | Cash cost |
| --- | --- | --- | --- | --- | --- |
| Farm | none | 30 grain | 60 | 2 | 3,000 pf |
| Mill | 20 grain | 15 flour | 45 | 2 | 4,000 pf |
| Bakery | 10 flour | 15 bread | 20 | 2 | 3,500 pf |
| Fishery | none | 20 fish | 60 | 2 | 3,750 pf |

The conversion ratios and cycle times are unchanged from catalog 15; multiplying
batch inputs and outputs avoids five times as many production and delivery events.
At full staffing, the mill-limited bread chain yields 12 bread per game day, enough
for 50 laborers at 0.24 bread per resident per day. A Fishery yields 8 fish per day,
enough for about 55 laborers at 0.144 fish per resident per day.

Each building now requests two laborers instead of one. Construction materials are
doubled and currency costs are 2.5 times the prior values:

| Building | Construction materials |
| --- | --- |
| Farm | 6 timber, 1 tool |
| Mill | 8 timber, 6 planks, 2 tools |
| Bakery | 8 planks, 2 tools |
| Fishery | 10 timber, 4 planks, 1 tool |

This means a mature 400-resident laborer city needs about eight bread chains and
eight fisheries for fully local staple supply, rather than roughly forty of each.
Imports, stored reserves, and future production upgrades can substitute for local
chains.

## Catalog 16 acceptance evidence

`Hansa.Integration.RuntimeSimulationHost.StarterEconomyLongRun` pays for and builds
roads, nine houses, a Market, one Farm, one Mill, and one Bakery in normal New Game.
Merchant AI is disabled, so no imports or free recurring supply mask the result.

| Checkpoint | Residents | Bread stock | Stock 30 days earlier | Bread fulfilled |
| --- | --- | --- | --- | --- |
| Day 90 | 18 | 157.6 | 107.2 | 129.6 / 129.6 |
| Day 210 | 18 | 354.2 | 303.8 | 129.6 / 129.6 |

The city retains all 18 founding residents with 100% bread fulfillment. At this
population the Market and staple buildings compete for the ten available workers,
so the bread chain runs below its 50-resident fully staffed ceiling but still grows
reserves. The test also passes a five-day Bakery shutdown, 30-day recovery, and
authoritative save/load hash equality.

The on-disk catalog reloads in either discovery order, reconstructs catalog 15 by
reversing only these eight balance changes, and passes all nine
`Hansa.Content.Definitions` tests. `Hansa.Integration.Save.StarterBalanceCompatibility`
also confirms current saves round-trip and older incompatible balances fail closed.
The New Game city inventory is explicitly sized to 20,000 units so the construction
stockpile and tuned costs validate without overflow.

The eight original assets are backed up under
`Saved/Backups/ProductionCapacity5x-20260913`. Preview, apply, catalog, and test logs
are under `Saved/Logs/ProductionCapacity5x*.log`. Catalog 16 is recorded in
`Tests/Golden/economic_catalog_v16.json` and requires a new game.

# Historical starter economy rebalance — 2026-09-12

User-approved starter balance, catalog **11**, registry **4170F53E6E9BC675**.
This targeted laborer bread sustainability, not a complete artisan/trade economy rebalance.

## Catalog 11 authored settings

| Producer | Input | Output | Batch ticks | Laborers | Artisans |
| --- | --- | --- | --- | --- | --- |
| Farm | none | 6 grain | 60 | 1 | 0 |
| Mill | 4 grain | 3 flour | 45 | 1 | 0 |
| Bakery | 2 flour | 3 bread | 20 | 1 | 0 |
| Fishery | none | 4 fish | 60 | 1 | 0 |

Recipe and building staffing agree. Eight laborer residents provide four pooled workers
at the existing 60% participation rate: three for bread and one spare.
Laborer consumption per resident per tick is 0.010 bread, 0.006 fish, and 0.004 beer.
Need weights are bread 60%, basic services 20%, fish 15%, beer 5%.
Bread plus services gives 80% satisfaction and maintains population; adding fish permits
growth without beer. Decline threshold is 75%, growth threshold 90%.

Migration uses actual consumption over the last 72 ticks and requires 72 consecutive
qualifying ticks before changing a home's population by one. At the normal 60-minute
tick, each window is three game days. Growth also requires at least 30 game days of
the most important food good in available city stock, calculated after the proposed
growth and across all homes sharing that inventory (or a longer evaluation window).
This is a reserve guard, not a prediction of future production capacity.

Empty homes receive two founding residents only once connected basic services are
available. Ordinary growth cannot bootstrap a zero-resident home or bypass market
access. This fixes zero demand being interpreted as fully satisfied immigration while
the market was still under construction.

## Delivery-inclusive tuning

The proposed 30-tick bakery was insufficient once normal logistics were included:
eight residents consumed 57.6 bread over 30 days while reserves fell from 23.2 to 19.6.
The final 20-tick bakery produces a measured surplus with the same three-bread batch.
No recurring free supplies, stock injections, merchant purchases, or extra bakeries
are used in the acceptance test. Opening bread remains 34.

The initial three-day growth reserve also allowed a boom to 21 residents followed by
decline. The 30-day reserve prevents starting supplies alone from causing that boom.

## Acceptance evidence

`Hansa.Integration.RuntimeSimulationHost.StarterEconomyLongRun`:
normal New Game, paid construction of roads, four houses, market, farm, mill and one
bakery; merchant AI disabled; normal production, delivery and consumption.

| Checkpoint | Residents | Bread stock | Stock 30 days earlier | Bread fulfilled |
| --- | --- | --- | --- | --- |
| Day 90 | 8 | 64.6 | 53.2 | 57.6 / 57.6 |
| Day 210 | 8 | 95.2 | 89.8 | 57.6 / 57.6 |

The test then pauses the bakery for five days, runs 30 days after restarting,
checks new completed batches and at least 95% recovered bread fulfillment,
and checks complete authoritative save/load hash equality (245 days total).
It also asserts all four homes have market access and are operational.

Development evidence:
`Saved/BuildArtifacts/20260912-115450641-automation-Hansa.Integration.RuntimeSimulationHost/UnrealEditor.log`.
The long-run test passed; the wider host suite has a separate
`ConstructionProjectionCompletes` ready-visual assertion failure, not suppressed here.
Population regression suite: 12 tests passed.
Content definition suite: all 9 tests passed, including final seed-revision parity.
Save compatibility: `Hansa.Integration.Save.StarterBalanceCompatibility` passed.

Final DebugGame build passed. The 245-day acceptance (including construction/bootstrap
assertions), catalog reload/reverse-order/historical-reconstruction gate, and all 9
definition tests passed in DebugGame:
`Saved/BuildArtifacts/20260912-115829063-automation-Hansa.Integration.RuntimeSimulationHost.StarterEconomyLongRun`,
`20260912-115842097-automation-Hansa.Integration.Authoring.EconomicAssetReload`, and
`20260912-115908236-automation-Hansa.Content.Definitions`.

The automated scenario does not yet validate a fishery-led growth path, expanded
industrial chains, or all possible road distances. Long routes can still reduce delivery
throughput; the demonstrated layout is recorded in the test.

## Authoring, provenance and compatibility

Nine assets under `Content/Hansa/Core/{Buildings,Recipes,PopulationTiers}` changed.
The canonical seed builder applies the same values and authored revisions.
Editor tooltips describe the migration window and food-reserve gate. No new schema
fields or serialized simulation state were introduced.

Use the seed commandlet with `-RebalanceStarterEconomy` to validate a preview and
add `-Apply` to save changed assets. The compiler validates the full catalog before
save. Original assets were copied to `Saved/Backups/StarterEconomy-20260912`;
application logs are `Saved/Logs/StarterBalanceApply.log` and
`Saved/Logs/StarterBalanceBakeryTune.log`.
The commandlet process also reports an existing GameFeatureData asset-manager setup
error; its rebalance-specific log confirms nine saved assets, then one bakery tuning save.

`Tests/Golden/economic_catalog_v11.json` records all 72 fingerprints.
The reload gate checks reversed discovery order and reconstructs the exact prior v10
hash by reversing only the nine reviewed balance changes, then checks earlier lineage.

**Restart the editor and start a new game.** Catalogs v10 and earlier are not silently
migrated into the changed economy; their files are preserved. The compatibility test
checks rejection leaves the current city unchanged and current-catalog saves round-trip.
The earlier v7-v9 to v10 migration registrations do not apply to v11.
