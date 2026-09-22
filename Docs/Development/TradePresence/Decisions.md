# Trade presence decisions

Status date: 2026-09-20. These decisions describe the executable baseline inspected for TR-01 and constrain TR-02 onward. They do not add gameplay state.

## TP-ADR-001 — Inventory transfer and market settlement are distinct contracts

**Status:** Accepted and implemented for explicit spot trade; legacy-route compatibility work remains assigned to TR-06.

The authoritative inventory operation remains `FHansaInventoryLedger::TryTransfer`: it moves a good between explicit inventory endpoints and records quantity movement without implying ownership exchange or price. A purchase or sale additionally requires a sampled authoritative market price, a signed money effect, and a typed settlement event/record.

The current executable baseline contains one legacy hybrid: `FHansaTradeExecutor::ExecuteAction` treats route load/unload at a compiled city whose `bMarketOnly` flag is true as a priced purchase/sale. The same action at Lübeck remains an unpriced inventory transfer. This behavior was introduced for preserved-fish salt imports and is saved and hashed. TR-01 preserves and characterizes it. TR-03 adds an explicit `FHansaSpotTradeCommand` and receipt/event contract without altering that legacy path. TR-06 owns how legacy route settlement is retired, versioned, or represented by an explicit commerce action.

## TP-ADR-002 — Station inventory ownership

**Status:** Accepted for TR-04 implementation.

A station inventory is a finite physical inventory owned by one house, located in one city, and referenced by a stable station identity. The host city owns neither the stock nor its reservations. City-market inventory remains city-owned. Transfers and commerce identify both endpoints; closing or suspension cannot delete station stock.

## TP-ADR-003 — Foreign-presence identity

**Status:** Accepted for TR-02 implementation.

One authoritative presence record is keyed by `(FHansaHouseId, FHansaCityDefinitionId)`. Display names, player indices, asset paths, actor paths, provider IDs, stations, and plots are not identity. A record may reference stable station and leased-plot entities but survives presentation replacement.

## TP-ADR-004 — Historical MVP Rostock access

**Status:** Accepted for TR-02 migration.

The historical Lübeck-shortage profile receives an explicit authored/migrated minimum Rostock commercial-access state sufficient to preserve its accepted journey. It does not receive a station, plot, construction right, merchant office, or city ownership. Existing saves receive the same deterministic state through a versioned migration; new successor scenarios may author a different opening.

## TP-ADR-005 — Capabilities and permissions

**Status:** Accepted for TR-02 implementation.

Capabilities are stable, typed, versioned identifiers compiled into an immutable canonical set. They have explicit consumers and causal query text. Stage is not a bag of booleans and is not itself permission to perform every later action. Unknown capabilities fail compilation/load; policy and stage contradictions fail validation.

## TP-ADR-006 — Leased plots do not convey city ownership

**Status:** Accepted for TR-04/TR-09 implementation.

A lease is a separate stable allocation containing city, house, bounds/site, permitted categories, occupancy, and status. It grants only the named placement rights. It does not change city identity, city-market stock ownership, existing building ownership, roads, workforce, taxes, or general buildability. Rostock remains autonomous outside the allocated site.

## TP-ADR-007 — Price timing and rounding

**Status:** Accepted as the compatibility baseline; authored policy extensions belong to later prompts.

The current legacy foreign-route settlement samples `FHansaCityMarketState::CurrentPriceMilliMarks` during `VehicleMovementAndTransfers`, immediately before the physical action. Current transaction friction is 500 basis points minus an applicable research reduction: buys multiply by `10000 + friction`, sales by `10000 - friction`, rounded half away from zero. Total purchase cost rounds up; total sale proceeds round toward zero. Affordability is bounded before transfer. Market price recalculation occurs later in the same tick, so it observes the changed inventory but does not retroactively reprice that settlement.

## TP-ADR-008 — Report age and information access

**Status:** Accepted for reuse.

Foreign-presence queries must reuse the existing market report policy and knowledge projections. A report retains its authoritative observation tick and age classification. Unknown is never converted to zero; stale and estimated reports remain labeled. Capabilities may alter lawful cadence/coverage only through authored policy and query logic, not UI-side freshness claims.

## TP-ADR-009 — Save, command, catalog, and fingerprint strategy

**Status:** Accepted.

The TR-03 baseline is save format 13, determinism fingerprint 26, command schema 9, multiplayer client-intent schema 3, simulation version 1, and system-pipeline version 1. TR-02 introduced format 12 / fingerprint 25 for presence state. TR-03 adds the spot-trade command and receipt, deterministic prior-save migration, and safe rejection of impossible pending legacy intent while preserving stable IDs. Catalog hash changes remain definition-driven and must pass reversed-discovery compilation. A version is never repinned merely to hide a behavior change.

## Named baseline gaps and owners

| ID | Gap | Owner / resolution prompt |
| --- | --- | --- |
| TP-BLOCK-001 | `trade.md` and older route/UI notes called route actions transfer-only, while executable market-only routes settle purchases and sales. | Resolved for TR-03 documentation and explicit spot trade. The legacy hybrid is preserved and characterized; TR-06 still owns its action-kind migration or retirement. |
| TP-BLOCK-002 | Money is stored on `FHansaHouseState` and foreign route settlement mutates it after inventory transfer; there is no standalone atomic money transaction ledger or rollback object parallel to the inventory ledger. | Resolved for TR-03 with candidate-state inventory-ledger movement, money settlement, validation, receipt, and commit-or-discard rollback. A separate money-ledger object is not required for this contract. |
| TP-BLOCK-003 | `EHansaRouteCargoActionKind` still names only `Load`/`Unload`, so saved plans do not state whether a foreign action is commerce or transfer; behavior is inferred from `bMarketOnly`. | Save/commands owner in TR-03/TR-06. |
| TP-BLOCK-004 | Current route preview documentation promised no arbitrage while runtime can settle foreign sales. | Resolved in TR-03 documentation: legacy market-only route settlement is stated explicitly and remains covered by its characterization test. |


## TR-06 resolution — explicit transfer targets

TP-BLOCK-001/003 are resolved by retaining saved action bytes 0/1 as the characterized legacy city operations and appending StationLoad/StationUnload (2/3) and OwnedCityLoad/OwnedCityUnload (4/5). This avoids silently rewriting old economies or promoting station capabilities into the historical MVP. Save 16 names this compatible payload migration. New station transfers consume RouteAccess and LocalStorage against the active house/city presence, never settle money, and leave factor commerce in its separate deterministic market phase. Native foreign legacy controls say Buy at market / Sell at market. No automated TR-03 spot-trade variant is introduced. See TR-06.md for evidence and remaining rendered/network gates.
