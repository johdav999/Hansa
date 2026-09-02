# ADR-0003 — Deterministic units and simulation time

- Status: Accepted
- Date: 2026-09-01
- Decision owners: Hansa project

## Context

Market prices, inventories, production, routes, research, saves, AI, multiplayer and fixtures must reproduce from the same definitions, state, seed and commands. Frame-rate-dependent time, floating-point authority and unstable container order would make long simulations and cross-machine evidence diverge.

## Decision

### Authoritative numeric units

- Money uses a typed signed `int64` count of the smallest gameplay currency quantum. The initial display quantum is one pfennig; display/localization may group it into larger historical denominations without changing stored value.
- Goods quantity uses a typed signed `int64` count of milli-units (`1/1000` of an authored stock unit).
- Rates, efficiencies, percentages and normalized progress use explicit fixed-point types. The default ratio scale is parts per million (`1_000_000 == 100%`) unless a domain type documents a different scale.
- Authoritative multiplication/division uses checked, deterministic helpers with a declared rounding mode. Economy defaults to round-half-away-from-zero for final conversions; allocation code may use floor plus deterministic remainder distribution where conservation requires it.
- Overflow, invalid division and conversion loss return structured failure or checked assertions appropriate to the boundary; silent wraparound is forbidden.
- Floating point is allowed for rendering, camera movement, visual interpolation and nonauthoritative UI layout. It is not used to decide accepted price, stock, recipe completion, route arrival, research completion, command order or victory.

### Simulation time

- Authoritative time is a signed 64-bit simulation tick plus calendar projection; zero is scenario start and negative runtime ticks are invalid.
- The initial economic tick duration is one simulated hour. Durations and deadlines are stored as integer ticks, not real seconds.
- Tick duration is a versioned simulation/scenario constant accessed through the clock, never copied as a magic number into systems.
- Pause advances zero ticks. Speed changes how many fixed ticks the authority attempts within a bounded real frame budget; it never changes the meaning of one tick.
- Commands carry a requested/scheduled tick and deterministic sequence. System execution order is explicit and versioned.

### Determinism rules

- Result-affecting iteration is explicitly sorted by the stable identifiers from ADR-0002.
- Random outcomes use named deterministic streams with seed and current state serialized. Systems do not use global, frame-seeded or platform-random generators for authority.
- Equal simulation version, definition hash, initial state, RNG state and accepted command stream must produce equal per-tick checksums.
- Changes to tick meaning, rounding, system order, RNG algorithm or checksum normalization are simulation-version changes with migration/replay consequences.

## Consequences

Positive:

- Headless, rendered, server and fixture execution can compare exact results.
- Saves and multiplayer diagnostics can locate the first divergent tick.
- Conservation and market-bound invariants are testable without epsilon comparisons.

Costs:

- Fixed-point math and remainder allocation require deliberate helpers.
- Designers need unit-aware editor fields and conversion previews.
- Changing economic granularity later requires versioned migration and balance review.

## Compliance

`HansaSimulation` must provide typed values and checked conversions rather than exposing raw arithmetic throughout systems. Tests must cover boundaries, negative values, rounding, overflow, serialization, stable ordering, named RNG replay and equal state hashes over long runs. Editor fields must display unit and range metadata.

Implementation clarification, 2026-09-02 (`S01-P01`): authoritative scaling uses a portable unsigned 128-bit intermediate with explicit `TowardZero`, `Floor`, `Ceiling`, and `HalfAwayFromZero` modes. The current clock version is `1` with a default 60 simulated minutes per tick. Named streams use `SplitMix64V1`; their initial state is the campaign seed XOR an FNV-1a 64-bit hash of the strict ASCII dot-separated stream name. Stream name, algorithm, state, and draw count are serialized in the versioned `HPR1` primitive format. Any incompatible change to these rules requires a version change.

## Deferred

- Maximum supported campaign duration and formal numeric range budget per subsystem.
- Calendar epoch, historical calendar rules and seasonal representation beyond the current fixed tick projection.
- Checksum algorithm and normalized state layout.
