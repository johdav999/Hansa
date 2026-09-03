# Lübeck grain-shortage fixture

`lubeck_grain_shortage_v1` is the versioned, actor-free S04-P04 integration fixture. It is available only when `WITH_HANSA_AUTOMATION` is enabled and runs through the same fixed-tick pipeline and gameplay command gateway as the game.

## Reviewed phases

The deterministic baseline is Lübeck at tick 0 with one city inventory, 20 laborers, grain consumers, grain-using mill and brewery production, a 30,000 milli-unit desired reserve, and a 1,000 milli-mark grain price. The fixture begins with 16,000 milli-units of grain. The normal population and production systems deplete it; the cadence-five market report publishes the shortage at tick 5 with separate citizen demand, industrial demand, unmet demand, causal price factors, alerts, and consumer identities.

Recovery production `10` is a predeclared, initially inactive background grain supply. Controlled automation may issue `production.set_active` through `FHansaGameplayCommandGateway`. Activating it advances one tick and creates grain through `Source.BackgroundSupply` and the inventory transaction ledger. Deactivating it is a second normal command. The tick-10 report must show stock at or above reserve, no shortage alert, zero unmet demand, and a price below the shortage price. Neither the fixture service nor MCP surface exposes a price or stock setter.

## Automation workflow

Use a `FixtureControl` session with `gameplay.query`, `gameplay.command`, `fixture.control`, and `wait-assertions`:

1. Load `lubeck_grain_shortage_v1`.
2. Run until `market.alert_active` for `City.Lubeck`, `Good.Grain`, `Shortage`.
3. Inspect `market.price`, `market.components`, `market.reserve`, `market.explanation`, `market.consumers`, `market.producers`, `market.history`, and `market.alerts`.
4. Issue `production.set_active` for production `10` with `active: true`, then `active: false`.
5. Run to the next market cadence and assert `market.reserve_recovered`; `market.stock_at_least` and `market.price_at_most` are available for explicit thresholds.

`gameplay_assert` evaluates the same allowlisted predicates without advancing. `simulation_run_until` advances by at most the caller's bounded tick budget.

## Golden evidence

`Tests/Golden/lubeck_grain_shortage_v1.json` is the compact reviewed oracle. It pins fixture and registry versions plus baseline, shortage, and recovery ticks, hashes, stock, reserve, separate demand, unmet demand, price, and alert state. `Hansa.Integration.Fixtures.LubeckGrainShortageV1` also writes the complete ordered event bundle to ignored `Saved/TestEvidence/Market/lubeck_grain_shortage_v1.json`.

## Migration rules

- The stable fixture ID is part of the automation API. Never change `lubeck_grain_shortage_v1` initial state, phase boundaries, recovery command identity, or metric semantics in place.
- Any intentional behavioral change creates a new immutable suffix, such as `lubeck_grain_shortage_v2`, with a new factory, fixture version, golden oracle, documentation entry, and MCP allowlist entry. Keep older fixtures while supported clients or evidence refer to them.
- A definition-catalogue change requires a reviewed registry hash. If it changes fixture behavior or evidence, create a new fixture version rather than merely accepting new golden values.
- A determinism-fingerprint implementation version may update state hashes in a same-behavior golden only when the phase metrics and event semantics are unchanged. Record the reason in the change review and regenerate the full evidence bundle.
- Golden updates are never produced by normal gameplay or CI. A reviewer must compare baseline/shortage/recovery metrics and ordered events before checking in the compact oracle.
- Provider IDs, filenames, asset paths, wall-clock time, localized display strings, and generated draft content may not enter fixture identity or assertions.

Authoring Studio's **Run shortage fixture** toolbar action executes this exact contract and displays localized baseline, shortage, and recovery metrics without changing authored assets.
