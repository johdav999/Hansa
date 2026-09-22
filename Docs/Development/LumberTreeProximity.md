# Lumber camp tree proximity — 2026-09-16

Lumber camps running Recipe.FellTimber require at least one uncovered standing-tree cell within a circular, inclusive 12-cell (48 m) radius of any occupied footprint cell. Distance uses the four-metre simulation grid, not rendered mesh bounds. Construction is rejected outside the forest: the dragged ghost uses the existing red invalid feedback and displays No nearby trees. The command gateway enforces the same rule. Existing camps also report NoNearbyTrees and neither advance their batch nor create timber. The test runs before reserving inputs and on every production tick. Staffing, storage, pauses and construction retain their existing rules.

Sawmills continue to consume timber and produce planks; they do not fell trees. Background supply and nonspatial economic fixtures retain their explicitly abstract source model. Every scenario with placement maps enforces tree access, including missing camp placements and empty surveys.

Trees covered by any building or road in the same city are unavailable. Forestry does not deplete trees in this change, and deleting a covering placement exposes the surveyed resource again. Live actor edits require regenerating the authored survey; streaming visibility never changes production.

## Authoring, deterministic data and impact

The existing placed-tree inventory is the authoring input:
SourceArt/Generated/Trees/LubeckSummer/v1/placement/placed_trees.json.
Run python Scripts/GenerateLubeckTreeResources.py after changing that inventory.
Run with --check to validate the checked-in result. The exporter validates finite positions, unique tree identities and map bounds, deduplicates grid cells, sorts them and records the source SHA-256. It introduces no new media or provider calls.

The generated Source/Hansa/Private/World/HansaLubeckTrees.generated.inl contains only grid coordinates. Survey initialization filters water cells against the authoritative terrain. FHansaPlacementTopology validates unique, present, dry/unblocked tree cells and includes the canonical resource survey in its hash and record count. No source-art, staging asset or generation-worker dependency is loaded by the runtime. Map authors use the same survey in game, tests and editor-created scenario initialization.

Changes affect all Recipe.FellTimber producers and their downstream planks/barrels chains. Changing tree positions changes map identity and can change their utilization. The existing recipe Declared source metadata explains the spatial rule in native Details and derived schema/prompt context. No new economic-definition property, provider contract or asset import is needed.

## Persistence

The tree survey is immutable definition topology, not per-save actor state. Format 7 retains its binary layout. Existing enum values are unchanged; NoNearbyTrees is appended.

For a pre-tree format-7 save, decoding reconstructs the exact terrain-only topology from the current definition, requires its hash to equal the saved topology hash, and validates the saved authoritative checksum against that original topology. Only then does it attach the current tree survey, invalidate hash caches and record Hansa.Save.7.AddStandingTreeSurvey. Changed terrain, ownership or nonempty tree surveys still fail compatibility checks. The current production blocker refreshes on the next simulation tick.

## Feedback and verification

The existing native production inspector gains a warning with cause, 48 m range and remedy. No widget layout, icon or raster artwork changes. Existing queries/events expose the NoNearbyTrees stable code.

Hansa.Simulation.Production.LumberTreeRange covers inclusive boundary, one cell beyond, diagonal outside, negative coordinates, footprint coverage, empty survey, road coverage/removal, malformed resource records, old-save migration and current-save round trip. Full production tests guard unaffected chains and deterministic replay. Live viewport appearance and a Shipping package are not claimed by these headless checks.

## Verification result

- Isolated Development Editor build succeeded; original Editor remained open.
- 7 production, 1 inspector, 1 surveyed-world, 6 placement and 12 save tests passed (27 total).
- One unrelated save fixture, Hansa.Integration.Save.StaleResearchCompletionRecovery, was rejected at the existing content/registry compatibility gate, before tree-topology migration. Its recorded archive does not match the current working catalogue; no compatibility checks were bypassed.
- All 320 authored tree cells are present and none fall on water in the terrain survey.
- Exporter --check and changed-file whitespace checks passed.
- Logs and machine-readable results: .codex-build/lumber-tree-verify/tree-proximity-results.json and sibling log files.
- Main project DLL linking was blocked by the running editor. Close the editor, build with Scripts/Build.ps1, and reopen to activate the change in that session. Isolated build/tests do not hot-reload the user's editor.

## Construction preview follow-up

Placement validation shares the production tree query, excluding both existing occupied cells and the proposed camp footprint. The current rotated footprint is checked on every target update. NoNearbyTrees is an appended placement-failure value with native cause/remedy text; invalid feedback drives the existing red overlay, footprint and world status label. Both drag release and direct construction confirmation reject invalid targets. No new artwork or widget styling is introduced. Regression coverage: Hansa.UI.BuildMenu.LumberGhostTreeFeedback and the placement cases in Hansa.Simulation.Production.LumberTreeRange.

Construction follow-up verification: isolated Development Editor build succeeded; all 14 ghost-drag, production and placement tests passed. Logs are build-ghost.log and *-ghost.log in .codex-build/lumber-tree-verify. The existing invalid material/text path is reused; no new live viewport screenshot was captured. The running editor must be closed and the main project rebuilt before that session uses these changes.
