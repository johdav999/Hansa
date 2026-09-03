# Hansa — Deterministic Primitive Contract

- Implemented prompt: `S01-P01`
- Contract date: 2026-09-02
- Owning module: `HansaSimulation`
- Current primitive format: `HPR1`, version `1`
- Current simulation version: `1`
- Current RNG algorithm: `SplitMix64V1`

## Purpose

This note records the concrete primitive contracts introduced by `S01-P01`. The accepted policy remains in ADR-0002 and ADR-0003. This document is the implementation handoff for later simulation, save, registry, editor and automation work.

## Source ownership

| Concern | Public contract | Implementation |
| --- | --- | --- |
| Structured value failures | `Source/HansaSimulation/Public/Model/HansaValueResult.h` | `Private/Model/HansaValueResult.cpp` |
| Definition and entity IDs | `Public/Model/HansaIds.h` | `Private/Model/HansaIds.cpp` |
| Money, quantity, rate and checked arithmetic | `Public/Math/HansaFixedPoint.h` | `Private/Math/HansaFixedPoint.cpp` |
| Tick, duration, version and calendar projection | `Public/Model/HansaSimulationTime.h` | `Private/Model/HansaSimulationTime.cpp` |
| Named deterministic RNG | `Public/Math/HansaDeterministicRandom.h` | `Private/Math/HansaDeterministicRandom.cpp` |
| Explicit primitive byte serialization | `Public/Save/HansaPrimitiveSerialization.h` | `Private/Save/HansaPrimitiveSerialization.cpp` |
| Headless tests | `Source/HansaTests/Private/Simulation/HansaPrimitiveTests.cpp` | Five `Hansa.Simulation.Primitives.*` tests |

`HansaSimulation` still depends only on Unreal `Core`; these types own no Actor, UObject, World, UI, editor, automation or provider reference.

## Identity contract

- `FHansaDefinitionId` accepts only registered ASCII dot-separated canonical IDs with at least two PascalCase alphanumeric segments.
- Typed aliases enforce the registered definition domains from `Docs/Development/RepositoryConventions.md` and cannot implicitly convert across domains.
- Runtime aliases store `(uint64 value, uint32 generation)`. Value zero is invalid. Ordering compares numeric value, then generation.
- Inventory and reservation identities introduced by `S03-P02` use the same typed runtime-ID contract and cannot be interchanged with other entity domains.
- Production identities introduced by `S03-P03` use the same typed runtime-ID contract and cannot be interchanged with buildings, inventories or reservations.
- Definition ordering is case-sensitive canonical-text order. Runtime ordering is typed numeric order.
- Definition `ToString()` and entity `ToDebugString()` are stable diagnostic evidence for the current implementation, not save compatibility contracts.
- Global uniqueness, redirects, tombstones and compact registry handles remain registry/migration responsibilities in later prompts.

## Numeric and time contract

- `FHansaMoney` stores signed 64-bit pfennig.
- `FHansaQuantity` stores signed 64-bit milli-units.
- `FHansaRate` stores signed parts per million; `1,000,000` means one or 100 percent.
- Authoritative add, subtract and multiply/divide return `THansaValueResult<T>` and never silently wrap.
- Multiply/divide uses a portable unsigned 128-bit intermediate. Its rounding mode is explicit; economic conversion defaults to half away from zero.
- `FHansaSimulationTick` and `FHansaSimulationDuration` reject negative values. `FHansaSimulationVersion` rejects zero.
- Clock version `1` supports an explicit positive `MinutesPerTick` up to one day and defaults to 60. Calendar projection currently reports elapsed day plus hour/minute from scenario start; historical epoch and seasonal rules remain deferred.

## Random-stream contract

- A stream is created from a campaign seed and strict ASCII dot-separated name.
- The initial state is `campaign seed XOR FNV-1a-64(stream name)`.
- `SplitMix64V1` owns the exact output sequence. Bounded 32-bit draws use rejection sampling rather than biased modulo-only mapping.
- Name, algorithm, state and draw count are authoritative serialized state. Systems must own named streams rather than use a global or frame-seeded generator.
- Changing the algorithm, derivation, name normalization or bounded-draw behavior is a simulation-version change.

## Primitive serialization contract

`FHansaPrimitiveWriter` and `FHansaPrimitiveReader` write and read one explicit byte envelope:

```text
magic "HPR1"
uint16 primitive format version (little endian)
zero or more type-tagged primitive values
exact end of envelope
```

Integers are two's-complement little-endian fields. Strings are length-prefixed strict ASCII. Type tags prevent accidental cross-domain or cross-unit reads. Readers reject bad magic, unsupported versions, unexpected types, invalid reconstructed values, truncation and trailing bytes with structured error codes.

The format is suitable for embedding into later save, fixture, command and evidence envelopes, but it is not yet the complete save format. Incompatible field or tag changes require a new primitive-format version and migration coverage. Debug strings never substitute for this format.

## Verification

The focused filter is:

```powershell
pwsh -File Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Simulation.Primitives
```

Coverage includes canonical validation, typed comparison and ordering, zero/negative/range failures, arithmetic overflow, large intermediate values, all rounding directions, clock projection and overflow, identical named sequences, the locked first `SplitMix64V1` value, bounded draws, exact round trips and malformed binary envelopes.

`S01-P02 — Simulation state and ordered pipeline` consumes these types through [SimulationKernel.md](SimulationKernel.md), `S01-P03` uses them through [CommandGateway.md](CommandGateway.md), and `S01-P04` locks their normalized diagnostic representation in [DeterminismDiagnostics.md](DeterminismDiagnostics.md). Sprint 1 is complete.
