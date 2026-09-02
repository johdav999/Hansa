# ADR-0006 — Normalized state hashes and first-divergence diagnostics

- Status: Accepted
- Date: 2026-09-02
- Decision owners: Hansa project

## Context

A single final checksum proves that two runs differ but does not identify when or where they first diverged. Saves, fixtures, multiplayer diagnostics, editor previews and automated evidence need one normalized definition of authoritative state plus bounded subsystem evidence that remains independent of discovery order, pointers, transient caches and presentation.

## Decision

- Authoritative state hashing uses hash-format version `1`, normalization version `1` and determinism fingerprint version `3`.
- State is normalized into this fixed subsystem order: `Contract`, `SimulationMetadata`, `RandomStreams`, `Houses`, `Cities`, `Buildings`, `Vehicles`, `Routes`, `TestEntities`.
- `Contract` includes fingerprint/pipeline versions, scenario ID and compiled-definition hash. `SimulationMetadata` includes clock, campaign seed and command/event sequencing history. Every entity subsystem includes its canonical record count, typed stable IDs/references and authoritative fixed-point/raw fields.
- Arrays are hashed only after state initialization has sorted and validated them by their typed stable keys. Integers use explicit little-endian bytes; canonical strings use explicit length plus strict ASCII bytes. Enum values and record counts are explicit.
- Each subsystem is domain-separated and hashed independently with FNV-1a 64. The overall fingerprint hashes the versioned ordered list of subsystem IDs, values and record counts. This diagnostic checksum is not cryptographic authentication.
- Transient caches, rebuild counters, returned domain-event payload batches, snapshots/projections, projection diffs, debug strings, logs, UI/world/presentation state, object pointers and container capacity are excluded. Published event count and command history remain included because they are authoritative sequencing state.
- Incompatible field inclusion, field order, canonicalization, byte encoding, subsystem order or algorithm changes require a normalization/hash/fingerprint version change and updated fixture evidence.
- Deterministic traces record the initial subsystem report plus one record per processed tick containing state-after-tick hashes, pipeline-order hash and domain-event-order hash. Comparison stops at the first mismatch and reports tick plus pipeline, events or authoritative state subsystem.
- Projection diffs are read-only diagnostic copies keyed by stable semantic fields. They never become mutation input or part of authoritative hashes.
- Evidence schema version `1` is JSON intended for test correlation under ignored `Saved/TestEvidence/`; it is not a save-game or network compatibility envelope.

## Consequences

Positive:

- Failures identify the first divergent tick and relevant subsystem rather than only a final mismatch.
- The global fingerprint and diagnostic component hashes cannot silently drift as separate implementations.
- UI, editor previews, automation and networking can compare bounded read-only projections without mutable state access.

Costs:

- Hash reports add linear diagnostic work over authoritative records.
- Fixture traces consume memory proportional to requested tick count and should remain bounded.
- FNV-1a collision resistance is suitable for deterministic diagnostics, not adversarial security decisions.

## Compliance

Tests must prove canonical equal-state hashes, cache exclusion, global/component correlation, empty and changed projection diffs, exact headless tick advancement, reviewed fixture checksum, valid JSON evidence, first-divergent tick/subsystem reporting and intentional pipeline-order drift detection. Development Editor, full automation, Shipping and exclusion gates remain required.

## Deferred

- Save-game envelope and migration integration.
- Cross-process/network trace exchange and mismatch sampling policy.
- Large-campaign incremental hashing if profiling shows full diagnostic reports exceed budget.
- Rich record-level diffs inside the first divergent subsystem.
