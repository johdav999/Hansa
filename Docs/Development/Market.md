# Local city markets and causal reports (S04-P02–P03)

## Authoritative inputs

Every market is keyed by `(City.*, Good.*)` and reads the available, unreserved stock in an explicit canonical inventory set. The authored `UHansaCityMarketProfileDefinition` supplies desired reserve, confirmed incoming supply per update, bounded seasonal/city modifiers, and minimum, maximum and initial prices. Its shared settings define cadence, history capacity, smoothing, maximum movement and stale threshold. The four MVP city profiles cover all ten goods and compile into the immutable economic registry; runtime code never looks up assets or package paths.

Citizen demand is the cohort requirement published by `WorkforceAndNeeds`; unmet citizen demand is required minus consumed. Industrial demand is the ceiling of each active recipe input divided by its cycle ticks. A missing-input production contributes that good to unmet industrial demand. Completed local output is accumulated on every tick until the next report. Confirmed incoming supply remains a separate authored forward-looking value and is never silently added to inventory.

## Deterministic price update

Reports update when `tick % UpdateCadenceTicks == 0`; the reviewed default cadence is five ticks. For each report, integer basis-point factors use desired reserve as the denominator (or one when reserve is zero):

- scarcity: `clamp((reserve - stock) / reserve, -50%, 100%) × 60%`;
- citizen demand: `clamp(citizen / reserve, 0%, 100%) × 25%`;
- industrial demand: `clamp(industrial / reserve, 0%, 100%) × 20%`;
- incoming supply: `-clamp(incoming / reserve, 0%, 100%) × 25%`;
- unmet demand: `clamp(unmet / reserve, 0%, 100%) × 20%`;
- authored season and city modifiers: each independently bounded to ±50%.

Ratios and weights round half away from zero. Their sum plus `10000` is clamped to `2500..40000`, multiplied by the good's base value with half-away-from-zero rounding, then clamped to the authored price bounds. The default 25% target smoothing also rounds half away from zero. Movement is capped to 10% of the current price per update using ceiling rounding, with a one-milli-mark minimum step while target and current price differ. The final price is clamped again to the authored minimum and maximum. All arithmetic uses checked integer helpers; floating point and wall-clock time are absent.

Markets are canonicalized by city, good and inventory identity before simulation. One report reads only the pre-existing inventory, population and production results for that tick; it cannot mutate data consumed by another market. Reversing market or inventory discovery order therefore produces the same state fingerprint and prevents iteration-order oscillation.

## Reports, history and stale behavior

`FHansaCityMarketProjection` is the shared owning report for game UI, diagnostics and future allowlisted automation. It exposes stock, reserve, separate citizen/industrial demand, local and incoming supply, unmet demand, current/recent-average price, every typed factor, last/next update tick, age, stale state and bounded typed history. History retains the most recent 64 reports by default.

An unreported market (`LastUpdateTick == -1`) is stale. A published report becomes stale only when `current tick - last update tick > StaleAfterTicks`; equality is still current. Stale reports retain their last values and history so consumers can label age without inventing a fresh price. The reviewed stale threshold is ten ticks.

## Shared causal explanation

`QueryMarketExplanation` converts the seven authoritative factor fields into a fixed reviewed order: scarcity, citizen demand, industrial demand, incoming supply, unmet demand, season and city. Each entry carries a typed factor, exact signed basis-point contribution, stable localization key and formatted `FText`. When the raw multiplier exceeds the price-policy range, a final typed clamp entry accounts for the exact difference. Starting at the `10000` base and summing every explanation entry therefore equals the authoritative target multiplier exactly; UI code never reimplements the formula.

Typed read-only queries separately expose current/recent prices, bounded history, supply/demand components, aggregate reserve milli-days, citizen and industrial consumers, local/background producers, one market's alerts and all active alerts. Consumers retain cohort/production/building/recipe identities, demand, fulfillment, affordability, reserve days and production blocker. Producers retain production/building/recipe identities, cycle, nominal/actual output, active state and blocker. `FHansaSimulationProjection` owns the active alert list for normal UI and automation snapshots.

## Alerts and age

Alerts are evaluated only from authoritative market and population reports at the market cadence:

- `Shortage` is active while unmet demand is positive; zero stock is critical, otherwise warning.
- `LowReserve` is active while stock is below desired reserve; at or below 25% of reserve is critical, otherwise warning.
- `Affordability` is active while any citizen consumer is below 100% affordability; below 50% is critical, otherwise warning.

Each alert carries typed severity, city/good IDs, affected cohort/production IDs, a stable cause-message key, localized cause text, exact onset tick and age, plus typed localization-ready player actions. Onset ticks persist while the condition remains active and reset when it clears; age therefore advances with simulation ticks even between report updates. Alert derivation is read-only and does not affect prices.

Market configuration, mutable report/history state, minimum consumer affordability and alert-onset ticks form the dedicated `Market` hash subsystem. S04-P04 advances the determinism fingerprint contract to version 9 because controlled production activation joins the authoritative command/event fingerprint.

## Lübeck integration fixture

`lubeck_grain_shortage_v1` connects population consumption, industrial grain demand, inventory depletion, price pressure, alerts, controlled production recovery, and reserve/price verification in one deterministic headless path. Its complete workflow, golden evidence, MCP predicates, and migration rules are documented in [GrainShortageFixture.md](GrainShortageFixture.md).

## Validation and verification

Stable `HSA-MARKET-001..004` diagnostics reject non-city identity, invalid cadence/history/smoothing/movement/stale settings, empty profiles, duplicate or missing goods, invalid modifiers/reserves and inconsistent price bounds. `HSA-REGISTRY-017` rejects missing compiled good references.

`Hansa.Simulation.Market` covers scarcity, surplus, no demand, zero stock, citizen and industrial demand, recovering incoming supply, cadence, bounded history, stale reports, min/max convergence, repeatability, reversed discovery order, exact causal totals, every typed query, localization-ready messages and deterministic shortage/reserve/affordability alert aging. `Hansa.Content.Definitions.MarketValidation` covers reflected and cross-registry invalid content.

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Market
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Content.Definitions.MarketValidation -SkipBuild
```
