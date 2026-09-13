# Approved P12–P15 promotion — 2026-09-08

The user explicitly approved P11/P12/P13, then P14/P15. P12–P15 are now **promoted and bound**, not fully playable/Shipping accepted. HansaModels' selected-revision and provenance checks governed the copies; no new artwork, resizing, economics, placement rules or capacities were introduced.

| Prompt | Selected source | Production root | Packages |
| --- | --- | --- | --- |
| P12 Lumber Camp | MeshesCM; rejected tiny meshes excluded | `/Game/Mesh/hansa-lumber-camp` | 26 |
| P13 Sawmill | Final meshes and referenced R5 end-grain texture | `/Game/Mesh/hansa-sawmill` | 26 |
| P14 Residences | R06 A/B meshes for both tiers; R05 excluded | `/Game/Mesh/hansa-residences` | 34 |
| P15 Market | R05 six-module assembly | `/Game/Mesh/hansa-market` | 39 |

The 125 packages comprise 20 meshes, 25 materials, 75 native 1024-square textures and five Blueprints. Source 1254-square generated images and exact prompts remain in the original source deliveries. Only selected Blueprint dependency closures were copied. Staging originals, old production residence meshes, review maps and unrelated work were preserved. No preview map, reference photograph or authoring script was promoted. Existing Blueprint names retain `_Review`, but their canonical copies are approved production bindings; package references, not names, establish this distinction.

## Binding and catalog evidence

Five definitions now use the canonical mesh and Blueprint: Lumber Camp, Sawmill, Laborer Residence, Artisan Residence and Market. Their authored revisions advanced from 1 to 2. Every other reflected field was compared before/after and remained unchanged. All copied mesh/material/Blueprint dependencies were checked for staging/developer paths; none remained. Per-package approval metadata records source paths and original package SHA-256 hashes.

- Catalog v5: `DAA463F7043FD66F`; previous v4: `DA77AC921DFB6DC0`.
- Fresh-process compiler and reversed input ordering produce the same hash. Exactly the five approved definition hashes changed; all other catalog rows match v4.
- `Tests/Golden/economic_catalog_v5.json`, seed defaults, runtime hash pin and catalog-lineage tests are updated together. v4/v3/v2 lineage remains tested.
- Existing save policy is **new-game-required** for changed catalogs. Old saves were not modified or deleted. Do not bypass definition-hash mismatch guards.
- The P02 manifest records these eight building/prop roles as `unverified-production`: canonical binding exists, but complete real-game and Shipping evidence is still required.

Receipts: [P12](Evidence/P12ApprovedPromotion-20260908.json), [P13](Evidence/P13ApprovedPromotion-20260908.json), [P14](Evidence/P14ApprovedPromotion-20260908.json), [P15](Evidence/P15ApprovedPromotion-20260908.json). They retain approved source hashes, destination hashes and exact definition diffs. The full transient preflight and dependency results are under `Saved/GenerationJobs/approved-p12-p15-20260908/`.

## Verification

- HansaEditor Win64 Development and DebugGame builds succeeded with normal unity settings.
- Fresh-process Development automation: **22 succeeded, including 2 with warnings; 0 failed**. Final DebugGame automation: **23 succeeded, including 2 with warnings; 0 failed**, adding visible-manifest completeness. This covers promoted actor spawning and assigned roles/materials/LODs/cosmetic authority, catalog reload and lineage, presentation states, placement, runtime construction, and authoritative save/load continuation. The two warning-bearing tests report transient-world cleanup without an engine world context; they are not warning-free passes.
- The final manifest check corrected misplaced Lumber Camp entries and stale Road/Grain Farm/Bakery references to match actual definitions. Warehouse remains an engine placeholder and Road Transition remains missing; neither was promoted.
- All four promotion receipt validators pass; P13/P14/P15 source/import validators also pass after accounting for the approved bindings.
- `Hansa.World.PromotedFamilies.P12P15` is a production-path test, independent of ignored staging packages.
- Retained reports: `Saved/Automation/P12P15-Promotion/index.json`, `Saved/Logs/P12P15-Promotion-Tests.log`, `Saved/P12P15-Development-Build.log`, `Saved/Automation/P12P15-DebugGame/index.json`, `Saved/Logs/P12P15-DebugGame-Tests.log`, `Saved/P12P15-DebugGame-Build.log`.
- The read-only catalog commandlet generated valid deterministic evidence, but its commandlet process reports unrelated GameFeatureData configuration and occupied MCP-port startup errors. That process is not reported as a clean pass.

## Remaining boundaries

P11 Fishery has only an audit: no model, staging candidate or reference-reviewed family exists to promote. The approval is recorded, but no future unseen Fishery revision is represented as reviewed or promoted.

The user closed the editor and DebugGame was rebuilt successfully. Both rebuilt configurations contain v5. Start a new game after reopening the editor.

Real playable-map visual/input acceptance, crowded-city performance, and a clean Shipping cook/package remain open. Existing staged native captures are historical visual evidence, not fresh playable captures from this promotion. Market still has no radius-based service model, and capacities remain 12 Laborers / 8 Artisans. This approval does not authorize inventing either gameplay rule.

The original source README/evaluation/provenance records describe the pre-approval review state. This dated report and the approval receipts supersede their pending-approval statements without rewriting archived evidence.
