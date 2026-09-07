# S11-P01 — Versioned authoritative saves

`FHansaSaveEnvelope` implements the runtime-only custom archive in TechnicalArchitecture §15. Format **2** supports the checked-in synthetic format **1** fixture. It does not serialize UObjects or asset paths.

## Capture and restore

`FHansaSaveSnapshot` owns a copy of the complete simulation plus stable principal/house bindings, ordered pending typed commands, optional runtime command/building allocator counters, and scenario observer state. Capture this copy on the authority thread between completed ticks; the standalone codec can then run on a worker against the immutable copy and definition context.

`UHansaRuntimeSimulationHost::CaptureSaveBytes` and `RestoreSaveBytes` provide the integrated runtime seam. They require an initialized matching scenario on the authority game thread. The current host executes commands synchronously and therefore captures an empty pending queue. It explicitly rejects a nonempty queue on restore; a future scheduler must consume such commands through the normal gateway. The generic envelope preserves all thirteen current command variants, including header, principal, origin, execution tick, sequence, and active payload.

Decoding builds a separate candidate, verifies compatibility, decompresses and validates its records and hashes, and only then replaces the caller's snapshot. Runtime restoration also checks ownership and allocator state before replacement. It clears transient simulation caches, domain-event display history and AI diagnostic history, resets frame-time debt, pauses, and republishes the world projection. The merchant's decisions depend on saved simulation time and campaign seed; its trace ordinal/history are diagnostic only.

S11-P02 adds a bounded slot subsystem, save discovery UI, and MCP slot actions above this codec. The codec itself still exposes no filesystem access.

## Wire contract

All integers use fixed-width little-endian encoding. Strings use bounded UTF-16 code units. Typed definition IDs store canonical stable names; entity IDs store their 64-bit value and 32-bit generation. Clock and RNG records reuse the validated primitive archive. Arrays have checked counts and a cumulative allocation budget.

The envelope contains, in order:

1. `HNSV` magic and save-format version.
2. Simulation, pipeline and determinism-fingerprint versions.
3. Content hash, compiled registry hash and stable scenario ID.
4. Build identifier, ISO-8601 UTC timestamp, display name and migration lineage.
5. Authoritative subsystem hash and complete campaign-payload hash.
6. Uncompressed length and zlib-compressed payload.
7. SHA-1 integrity digest over the complete preceding envelope.

The payload includes clock/tick, campaign seed, RNG algorithm/state/draw counts, command/event counters, houses and ownership, cities, buildings/construction, placement maps/entitlements/occupancy, vehicles/routes/cargo, inventories/reservations/movement history, production progress/input reservations and ID counters, population/needs, market cadence/reports/history, research queues/effects, and logistics requests/jobs/counters. Market reports and histories remain authoritative because they affect knowledge, AI and deterministic hashes.

`HansaSaveFields.inl` is the explicit field-order contract. `HansaSaveValidation.inl` shares initialization validators and additionally checks reservation balances, production reservation references, logistics records/counters and history bounds. UI widgets, presentation actors, labels/briefing text, frame-time debt, screenshots and rebuildable caches are absent.

The scenario payload stores stable scenario/house/victory IDs, outcome, winner, last evaluation tick, failure streak and victory streaks. Restore rebuilds targets and presentation from the matching registry and then restores streaks without counting another tick.

Both archive and decompressed payload are limited to 64 MiB; individual strings to 65,536 UTF-16 units. The digest detects corruption, not sender authenticity. Public authoritative sessions must not accept client-provided save state.

## Hashes, migrations and authoring impact

`AuthoritativeHash` is the existing versioned subsystem report hash. `CampaignHash` additionally covers the complete serialized payload, including allocator counters, ownership, pending commands and scenario streaks. Display metadata and migration lineage do not alter campaign identity. Reencoding an unchanged current-version snapshot produces identical bytes.

The explicit `Hansa.Save.1To2.AddDisplayNameFromScenario` migration fills v1's missing display name from its stable scenario ID and records persistent migration lineage. Format 1 otherwise uses the same state contract; its header lacks display name and lineage. Loading the migrated format-2 save does not reapply the migration. `Tests/Fixtures/save_envelope_v1.hansa` is an immutable prior-schema golden using the production fixture's initial state.

Unknown formats, simulation/pipeline/fingerprint changes, content/registry mismatches, and scenario mismatches produce distinct errors. Corrupt/truncated archives, invalid records, compression failures and hash mismatches leave the destination untouched. Asset renames do not require migration while stable IDs and compiled content remain identical. Stable-ID or definition-content changes require an explicit reviewed migration; the loader never guesses redirects from names or paths.

This change adds no authorable definition properties, provider integration or generation workflow. Existing schema exports and authoring UI remain applicable. A changed authoritative field must update its explicit codec, validation, format/migration policy and golden tests in the same change. A definition edit that changes the compiled hash makes existing saves incompatible unless a migration is supplied, as required by the existing authoring impact-analysis contract.

## Verification

Run `Scripts/RunAutomationTests.ps1 -TestFilter Hansa.Integration.Save`:

- 36 round trips across production, grain-shortage and route-delivery checkpoints, followed by 15 deterministic ticks each;
- lossless encoding of all thirteen command variants and out-of-order queue rejection;
- corruption, truncation, trailing data, future format, simulation/content mismatch, duplicate principal and atomic failure checks;
- checked-in v1 migration, current-version reencode and persistent lineage;
- nonzero victory/failure sustain progress and terminal-state restoration;
- runtime save/restore after research and AI activity, followed by 80 matching ticks, scenario streak/outcome comparisons and failed-restore atomicity.

`Scripts/VerifyShippingExclusion.ps1` builds Shipping and audits its executable and target receipt. This proves the module boundary for this increment; the full cooked/package/depot audit remains the integrated release gate.



Validation on 2026-09-06: all six `Hansa.Integration.Save` tests passed. The final source files matched the isolated validation copy byte-for-byte. Isolation under `Saved/S11P01Validation/Hansa` avoided replacing DLLs loaded by the open editor; UDP automation transport was disabled for that test process. Evidence is in `Saved/S11P01Validation/Hansa/Saved/BuildArtifacts/20260906-092409561-automation-Hansa.Integration.Save/result.json`.

Shipping compilation and the executable/receipt exclusion audit passed: `Saved/BuildArtifacts/20260906-091909309-shipping-exclusion-Win64/result.json`. The repository-wide convention audit still reports five existing test-name violations in unrelated UAT, trade and runtime-host tests; the new save tests meet the convention.
