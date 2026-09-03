# Hansa — Inventory and Reservation Contract

- Implemented prompt: `S03-P02`
- Contract date: 2026-09-02
- Owning module: `HansaSimulation`
- Determinism fingerprint version: `4`

## Scope and ownership

`FHansaInventoryLedger` is the Actor-independent authoritative store for city, building and warehouse inventories. Its public contract lives in `Source/HansaSimulation/Public/Inventory/HansaInventory.h`; its implementation lives in `Private/Inventory/HansaInventory.cpp`. It depends only on deterministic simulation primitives and owns no Actor, UObject, World, UI, editor, provider or transport reference.

Each inventory has a typed runtime ID, exactly one typed owner, positive fixed-point capacity, a canonical accepted-goods set and canonical stock records. Initialization rejects invalid or duplicate identities, duplicate owners, duplicate accepted goods or stock lines, unaccepted initial goods, negative stock and aggregate stock above capacity. Simulation-state initialization additionally requires city and building owners to exist.

## Atomic transaction contract

Transfers use explicit endpoints:

- inventory-to-inventory moves stock and preserves total quantity;
- explicit source-to-inventory creates only the reported `ExplicitlyCreatedQuantity`;
- inventory-to-explicit sink destroys only the reported `ExplicitlyDestroyedQuantity`;
- source-to-sink and same-inventory bypasses are invalid.

Every operation preflights identity, endpoint, good acceptance, aggregate capacity, unreserved availability, reservation match, arithmetic and strictly increasing sequence before commit. A failure returns a typed `EHansaInventoryTransactionError`, reports zero applied quantity and leaves stock, reservations, sequence and movement history unchanged. There is no partial transfer path.

Reservations are keyed by `FHansaReservationId`, sorted canonically and reduce available stock without reducing physical stock. Competing reservations cannot claim more than the unreserved balance. A matching reservation can be consumed wholly or partially by a transfer; release restores availability without creating stock.

## Read-only and diagnostic contract

`FHansaInventoryReadOnlyAccess` provides direct typed `QueryCapacity`, `QueryReservedAmount` and `QueryStock` calls plus owning inventory projections for used/free capacity and per-good stock/reserved/available amounts. Querying an accepted but empty good returns a zero-valued stock projection; querying an unaccepted good or unknown inventory returns no result. Recent movement queries are bounded, newest-first owning copies.

Simulation snapshots own copies of inventory records, reservations and recent movements. Simulation projections include canonical inventory projections. Inventory state has a dedicated `Inventories` subsystem hash covering movement capacity/sequence, owners, capacity, accepted goods, stock, reservations and retained movements. Any result-affecting inventory change therefore changes the global fingerprint.

## Verification

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Inventory
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Integration.Inventory
```

Coverage includes invalid initialization, capacity and accepted-goods enforcement, failure atomicity, explicit source/sink accounting, competing and partially consumed reservations, typed queries, bounded movement ordering, canonical discovery ordering, a 300-operation deterministic property run, conservation invariants, snapshots, projections and subsystem/global hashes.

## Deliberate next boundary

`S03-P03 — Production system and causal output` now consumes this ledger through production-owned reservations and working-copy atomic transactions. Inputs leave through `Sink.ProductionInput`; outputs and background supply enter through explicit sources, preserving causal accounting. See [Production.md](Production.md). `S03-P04` is the next boundary.
