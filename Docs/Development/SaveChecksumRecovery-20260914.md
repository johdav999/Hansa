# Save checksum recovery — 2026-09-14

## Root cause

`FHansaSimulationPipeline` invalidated the research checksum only when a research queue was still active **after** the tick. Completion clears that queue, so the final progress, completed technology and applied effects could retain the previous cached checksum. Saving serialized the correct completed research records but wrote the stale authoritative checksum. Decoding recomputed it from records and rejected the archive.

The player manual archive at tick 6069 reproduced the failure. House 2 (the AI merchant) had completed `Technology.Commerce.MarketReports` and `Technology.Logistics.WarehouseHandling`. This can occur without the player researching anything. The stored archive integrity and compressed-payload checks passed. Both original player slots decode successfully with the repair.

## Changes

- Research invalidation considers the queue before and after the tick, and explicitly covers research commands that queue and complete within one tick.
- Encoding recomputes authoritative hashes independently of runtime caches and verifies the complete archive through Decode before returning bytes. Failed verification leaves output bytes and existing save slots unchanged.
- A format-7/fingerprint-20 recovery reconstructs the precise pre-completion research state and requires its full authoritative hash to equal the saved hash. It changes no gameplay records. All integrity, content, topology, record and scenario checks remain enforced. Unrecognized mismatches still fail.
- Recovery records `Hansa.Save.7.RepairStaleResearchCompletionHash`; a subsequent save has the corrected checksum and requires no repeated repair.
- Payload parsing, clock mismatch and checksum mismatch now have separate diagnostics.

The recovery intentionally does not guess across definition migrations or multiple simultaneous research completions. Those unsupported mismatches remain rejected. It has been checked against both affected player slots.

## Regression evidence

- `Hansa.Integration.Save.RuntimeAuthorityContinuation` now saves at every research transition and compares the live cached fingerprint to the independently encoded checksum. Before the fix, it failed at tick 14 as the AI completed warehouse handling.
- `Tests/Fixtures/research_completion_stale_v7.hansa` is a 6,232-byte synthetic runtime fixture captured with the original bug, not the player city.
- `Hansa.Integration.Save.StaleResearchCompletionRecovery` verifies exact payload-hash preservation, repair provenance, idempotence, rejection of an unrelated re-signed checksum mismatch and atomic failure.
- `Hansa.Integration.Save.PlayerArchive` accepts an optional `-HansaInspectSave=<path>` for read-only reproduction against a player archive using surveyed Lübeck definitions.
- Isolated Development build passed. All 13 `Hansa.Integration.Save` tests passed, including the original autosave. The original manual save also passed separately.
- Logs: `Saved/Logs/SaveRegressionBefore.log`, `SaveRegressionAfter.log`, `SaveRegressionFinal.log`.
- Original slots backed up under `Saved/SaveGames/Hansa/Backups/ResearchChecksumFix-20260914/` before restarting.

## Scope

No gameplay definition schema, asset or save field order changed. The existing format remains compatible. The normal editor build also required renaming the generic anonymous-namespace `Root` constant in `HansaLubeckWorldArtAuthoringTests.cpp` to avoid unity-build shadowing errors in other project and engine code; its behavior is unchanged.

## Main-project verification and activation

The main `HansaEditor Win64 Development` build passed after the unity-build naming correction. Build report: `Saved/BuildArtifacts/20260914-192916323-build-HansaEditor-Win64-Development/`.

All 13 save tests passed against the main project, including the original manual archive (`Saved/Logs/SaveMainProjectVerification.log`). The save benchmark also passed: 100 canonical encode/decode round trips, 17,902 bytes, 12.195 ms per round trip (`Saved/Logs/SavePerformanceVerification.log`).

The editor was reopened with the rebuilt modules. Original manual/autosave SHA-256 hashes still match their backups; recovery occurs on load, and the next save persists the corrected checksum and migration record.
