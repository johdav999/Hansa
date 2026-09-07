# S09-P01 — Simulated city markets and information age

Hamburg, Lüneburg, and Rostock are market-only cities. Each uses the same ten-good market, inventory, demand, price-factor, bounded-price, history, checksum, and typed-query contracts as buildable Lübeck. Their city-market definitions author initial stock plus fixed background production, citizen demand, and industrial demand per market update. Background flows enter and leave the city inventory through explicit source/sink inventory transactions; they do not write stock directly.

## Report knowledge contract

The authoritative simulation evolves every city continuously at the configured market cadence. Player-facing remote knowledge changes only when the city's report cadence publishes an immutable snapshot. A report is classified by age as:

- `Current`: age is at or below `CurrentReportMaxAgeTicks`;
- `Recent`: age is at or below `RecentReportMaxAgeTicks`;
- `Stale`: age is at or below `StaleReportMaxAgeTicks`;
- `Estimated`: age is at or below `EstimatedReportMaxAgeTicks`; the MVP uses the last published value as an explicit hold-last-value estimate;
- `Unknown`: no report exists or the estimate horizon has expired.

The age limits are inclusive and must be ordered. Authored report cadence must be a positive multiple of the market update cadence. Lübeck publishes with the local cadence; the three remote cities publish every 20 ticks and traverse current, recent, stale, and estimated states between reports.

## Typed read-only queries

- `QueryMarketReportAge` returns report provenance and optional timestamps/age.
- `QueryKnownMarketPrice` returns an optional price and recent average plus information state.
- `QueryKnownMarketSupplyDemand` returns optional stock, reserve, production, incoming supply, demand, and unmet demand.
- `CompareMarketOpportunity` compares two reported markets and returns optional reported prices, gross margin, exportable stock above reserve, and destination demand gap.

The automation equivalents are `market.report_age`, `market.known_price`, `market.known_components`, and `market.opportunity`. Unknown numeric facts are omitted and accompanied by `known: false` or `comparable: false`; zero is reserved for a known measured zero.

## MVP content

- Hamburg has extra fish and bread production.
- Lüneburg has a strong salt surplus.
- Rostock has grain and fish surpluses.
- All other remote rows use balanced fixed background production and demand so the complete ten-good contract evolves deterministically.

The normal Lübeck shortage scenario initializes all four cities and 40 city/good market records. Only Lübeck is buildable in the MVP.

Existing reviewed city-market assets are upgraded explicitly with:

```text
UnrealEditor-Cmd.exe Hansa.uproject -run=HansaEconomicDefinitionSeed -MigrateIntercityMarketsS09P01 -unattended -nop4
```

The migration updates exactly the four MVP city-market assets and fails closed on missing or mismatched stable identities.
