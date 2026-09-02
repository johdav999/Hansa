# Hansa — Command Gateway and Domain Event Contract

- Implemented prompt: `S01-P03`
- Contract date: 2026-09-02
- Owning module: `HansaSimulation`
- Command schema version: `1`
- Determinism fingerprint version: `3`
- Governing decision: [ADR-0005](../Architecture/Decisions/0005-command-gateway-and-domain-events.md)

## Source ownership

| Concern | Public contract | Implementation |
| --- | --- | --- |
| Typed command headers and payloads | `Source/HansaSimulation/Public/Commands/HansaGameplayCommand.h` | `Private/Commands/HansaGameplayCommand.cpp` |
| Sole command gateway and structured results | `Public/Commands/HansaGameplayCommandGateway.h` | `Private/Commands/HansaGameplayCommandGateway.cpp` |
| Immutable ordered domain events | `Public/Events/HansaDomainEvent.h` | `Private/Events/HansaDomainEvent.cpp` |
| Transactional fixed-step application | `Public/Systems/HansaSimulationPipeline.h` | `Private/Systems/HansaSimulationPipeline.cpp` |
| Headless contract tests | `Source/HansaTests/Private/Simulation/HansaCommandGatewayTests.cpp` | Four `Hansa.Simulation.Commands.*` tests |

## Command contract

`FHansaCommandHeader` carries command ID, issuing house, principal, origin, requested tick, global sequence and schema version. The four origins—player input, AI, multiplayer RPC and controlled automation—are correlation context, not privilege levels. Every origin enters `FHansaGameplayCommandGateway::ExecuteTick` and receives the same validation and application rules.

Command IDs and global sequences must both be valid and strictly increasing across accepted history. The complete header and active typed payload produce a deterministic fingerprint inside `HansaSimulation`; external callers cannot claim a trusted payload fingerprint.

The representative version-1 payload family is:

- `FHansaCreateTestEntityCommand` — create a plain lifecycle record owned by the issuing house;
- `FHansaCancelTestEntityCommand` — remove an existing record only when the issuing house owns it;
- `FHansaNoOpTestCommand` — accept and evidence a command without feature state mutation.

These payloads prove the infrastructure without implementing placement, routes, markets or other city features. Later payloads extend the same closed typed family and stable rejection model.

## Validation and transaction contract

The gateway returns `FHansaCommandGatewayResult` with a stable `EHansaCommandGatewayError`, failed command index/ID, unchanged or committed tick/fingerprint, and an owning read-only event batch. Stable causes distinguish schema, identity, authority, tick, order, capacity, payload, existence, ownership and clock failures.

After the separate validated initialization/restore boundary, the fixed-step pipeline is private to the gateway. It validates headers, capacity and the clock before application, then applies payloads sequentially to a working state copy. A failure at any index discards the working state and pending events. The live state, time, fingerprint and transient cache remain unchanged. Only a fully successful batch is committed.

## Domain-event contract

Each accepted representative command emits one `FHansaDomainEvent`. Events are returned in application order and contain type, globally increasing event sequence, tick, source command ID, issuing house and typed representative data. Consumers receive `TConstArrayView`; no mutable or partial event batch escapes.

Only `PublishedDomainEventCount` is authoritative and fingerprinted. Returned event batches are presentation/replication inputs and are not retained as an unbounded campaign log. Later audit/timeline work must opt into explicit retention.

## Authoritative state and fingerprint changes

State now records the last accepted command ID, published-event count and canonically ordered representative lifecycle records. Snapshot and projection APIs expose these through read-only copies/counts. `S01-P04` fingerprint version `3` includes those fields through the normalized metadata/test-entity components. Pipeline phase order remains version `1` because its eleven phases did not change.

No reflected definition, authoring asset or designer field was introduced, so there is no editor schema, migration or OpenAI proposal surface to update in this prompt.

## Verification

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Commands
```

Coverage proves typed lifecycle behavior, identical rules across caller origins, immutable command-correlated event order, globally monotonic event sequences, late-command rollback, structured cause/index/identity results, invalid schema/authority/order/payload rejection, 200-tick replay equality and divergence from a one-field payload change.

## Deliberate next boundary

`S01-P04 — State hashes, projections and diagnostic evidence` now provides normalized subsystem hashes, projection diffs, named deterministic fixture execution and first-divergence evidence around this typed command/event boundary. Sprint 1 is complete.
