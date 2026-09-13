# EMVP-P33 — economy loop candidate

Status: runtime corrections and the catalog v9 review candidate are implemented. Production balance promotion awaits review of the exact change below. Catalog v8 remains the default; use the P33 candidate switch to test the new starting economy. This is not an assertion that P36 session balance or the world-art release gates are complete.

## Root causes and changes

The v8 starter grants fixed workforce to its production rows, includes an operating Brewery, substitutes reserve-plus-stock values for the authored local market opening, and consumes 1,200 milli-units of bread per tick with one Bakery producing only 3,000 per 45 ticks. Rostock replaces its own Tools/Beer consumption but cannot sustain exports. A full 12-resident Laborer home cannot upgrade into an 8-capacity Artisan home.

The candidate starts with three Laborer homes of ten residents and one Artisan home of eight residents. All three starter productions use the actual city workforce; the farm starts paused, while the Mill and Bakery start active. There is no starter Brewery. Starting buildings remain prebuilt city-inventory producers, as in the prior scenario; newly constructed buildings continue using the existing road/storage logistics pipeline. The additional homes use the existing approved presentation assets.

The existing satisfaction, access, affordability, reliability and population-trend projections are the approved wellbeing measures. No disease or separate health model has been added. Purchasing power remains the existing bounded cohort input; prices are not a new household cash simulation. Satisfaction drives migration, which changes workforce and citizen demand. Manual upgrades charge real construction goods/currency and require satisfaction. An early upgrade can remove the Laborers needed by the food chain, so upgrading is not an unconditional improvement.

Runtime corrections also apply to the existing economy: remote production stops at three times each good's desired reserve (including reserved stock), and still respects shared physical capacity and its per-update production quota. This prevents unused exports filling the warehouse indefinitely and starving other goods of capacity. Incoming supply at market updates is derived from the real vehicle inventory committed to unloading at the next stop, capped by cargo/action limits and counted once per vehicle. Inactive/cancelled routes and empty planned loads do not discount prices. Legacy initial reports remain byte-compatible; candidate authored incoming values are zero. Empty homes evaluate enough stock for a prospective resident while reporting zero actual demand/consumption until migration.

## Catalog v8 → candidate v9

Base hash: `6FAA28CD24E2C69E`. Candidate hash: `92658D0E14439F91`.

Eight definitions change; stable IDs, schema fields, recipe cycle times, workforce shares and the ten-good/two-tier scope remain intact. Authored revisions and compiled hashes distinguish the change. No live provider or ImageGen call is involved in this data-only revision.

| Definition | Before | Candidate |
| --- | --- | --- |
| Laborer needs, milli-units/resident/tick | Bread 100, Fish 60, Beer 40 | Bread 3, Fish 2, Beer 1 |
| Artisan needs, milli-units/resident/tick | Bread 140, Fish 60, Beer 70, Tools 20 | Bread 5, Fish 2, Beer 2, Tools 1 |
| Bakery bread output / 45 ticks | 3,000 | 6,000 |
| Artisan residence capacity | 8 | 12; existing residents are not evicted during upgrade |
| Lübeck market opening | Ignored zero authored fields; runtime overrides | Grain 16,000; Bread 12,000; other goods reserve + 10,000 |
| Rostock production / market update | 1,000 each for Bread, Beer, Tools, Salt, Iron | Bread 1,500; Beer 2,000; Tools 1,400; Salt/Iron 1,250 |
| Four city profiles' static incoming promises | Several nonzero values | Zero; runtime uses loaded route cargo |

Rostock's background consumption remains 1,000 per good per market update. Renewable exports therefore have a finite rate as well as a finite stock ceiling. The modest Bread export supports recovery when a city has lost its workforce; it does not create a local Brewery or Smithy chain.

[Exact authored-field diff and per-definition hashes](EconomyP33/catalog_v9_candidate.json) are saved alongside [deterministic playthrough results](EconomyP33/playthroughs.json). Selected draft Data Assets are under `Content/Hansa/Generated/Staging/EconomyP33/` (72 self-contained definitions; only eight differ from production).

## All ten goods have bounded sources

| Good | Opening milli-units | Source and use |
| --- | ---: | --- |
| Grain | 16,000 | Farm; Rostock exports; Mill input |
| Flour | 30,000 | Mill; Bakery input |
| Bread | 12,000 | Bakery; bounded Rostock recovery imports; both population tiers |
| Fish | 28,000 | Buildable Fishery; Rostock exports; both population tiers |
| Timber | 34,000 | Buildable Lumber Camp; construction and Sawmill input |
| Planks | 28,000 | Buildable Sawmill; construction/upgrades |
| Salt | 22,000 | Starting reserve and bounded Rostock/Lüneburg supply; retained trade commodity |
| Iron | 20,000 | Starting reserve and bounded Rostock supply; retained trade commodity, no mandatory local Smithy |
| Tools | 18,000 | Rostock imports; Artisan consumption and construction/upgrades |
| Beer | 26,000 | Rostock imports; both population tiers; no mandatory local Brewery |

Only Bread, Fish and Planks have buildable local chain cards. Salt and Iron are deliberately bounded holdings/trade commodities in this slice; neither requires an invented local chain or sink.

## Verification

`Hansa.Integration.EconomyP33.Playthroughs` runs the real runtime initializer with the candidate compiled through the production definition compiler. Commands use the ordinary gateway; it does not inject stock during a run. Eight scenarios run twice and compare exact authoritative hashes: growth, upgrade, neglect, shortage, recovery, surplus, trade interruption, and recovery after complete population collapse. Every terminal state saves and reloads with an identical hash. Candidate saves are rejected under the old catalog rather than silently rebalanced.

Measured examples: healthy growth reaches 46 residents after 120 ticks; prolonged neglect reaches zero. Reopening production plus real imports recovers the collapsed city to 48 residents and 29 workers. Ongoing trade reaches full satisfaction; stopping it eventually lowers satisfaction to 6,125 basis points. Delivered surplus lowers bread prices relative to the shortage case. These are deterministic balance probes, not a claim that every strategy or a 30–60-minute session is balanced.

The native Cog workflow also runs against the candidate at 1280×720 and 1920×1080: ordinary frontend/route controls, loading, departure, travel, arrival, unloading, selection and save reconstruction. Original PNGs and synchronized state text are archived under `Docs/Images/World/EconomyP33/`. These are in-game evidence, not generated references or new production artwork. The existing world-art limitations recorded by P30–P32 remain.

Targeted gates cover market, population, definition metadata/validation, on-disk production catalog reload, UI, save compatibility and Shipping exclusion. The broader simulation run also exposed the existing `foundation_determinism_v1` golden mismatch (`C827BB245C79D327` versus `F50841E8464CF363`): that fixture has no economic registry, markets or population and does not execute the P33 changes. Its golden was not rewritten to hide the discrepancy. See `EconomyP33/verification.json` for exact successful runs and this outstanding baseline issue.

## Test the candidate

From the project directory:

```powershell
.\Scripts\StageEconomyP33.ps1 -EngineRoot H:/Unreal/UE_5.8
.\Scripts\LaunchGuiPreview.ps1 -P33Candidate -P31Candidate -ReadOnlyZenDdc -EngineRoot H:/Unreal/UE_5.8
```

Start a new game. The HUD should show 38 residents initially. Enable the farm, watch population/workforce and market stocks, build further local production, or create a Cog import route. Compare an early home upgrade with retaining enough Laborers. The P33 switch is unavailable in Shipping. The optional P31 switch enables the existing staged Rostock visual quarter.

```powershell
.\Scripts\RunAutomationTests.ps1 -SkipBuild -NoZenDdc -TestFilter Hansa.Integration.EconomyP33 -EngineRoot H:/Unreal/UE_5.8
```

`NoZenDdc` uses Unreal's filesystem cache fallback for headless tests; `ReadOnlyZenDdc` reuses existing rendering data without writes to the low-space Zen cache. Neither clears user caches.

## Review and promotion boundary

The editor schema remains reflected from the existing runtime definition fields. Updated market metadata explains the stock ceiling and legacy incoming field; validation rejects background production without a positive reserve. The same compiler validates staged definitions and editor proposals. The staging commandlet refuses a baseline other than the exact reviewed v8 hash, preserves all unchanged definitions/presentation references, increments changed authored revisions, and records before/after fields and hashes. Runtime modules have no dependency on the staging commandlet or generation worker.

Catalog promotion is intentionally pending the actual balance review required by `Docs/EditorArchitecture.md` §9.4: **“require approval before saving.”** Repository instructions also require explicit approval before generated data moves from staging to production. This approval is for the eight-definition diff above, not a provider call or asset-art approval.

After approval, promote those exact fields through the editor's reviewed definition workflow, verify the v9 compiled hash, update the reviewed catalog golden and default runtime pin together, and rerun asset reload/save/native/Shipping gates. Existing v8 saves remain tied to their original definition hash; v9 requires a new game unless a separately reviewed state migration is added. No production Data Assets or reviewed v8 catalog manifest were overwritten by this task.
