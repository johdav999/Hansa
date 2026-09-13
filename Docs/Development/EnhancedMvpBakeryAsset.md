# EMVP-P10 — Bakery candidate and promotion boundary

Status: user-approved production binding and catalog v4 applied on 2026-09-07;
full gameplay-resolution and Shipping acceptance remain open.

P10 preserves both earlier bakery variants and revises the ImageGen-material version in
`Saved/GenerationJobs/hansa-bakery-p10_20260907/`. Selected delivery artifacts are under
`SourceArt/Generated/Buildings/HansaBakery_P10_20260907/`.

## Component and state contract

`AHansaBakeryPresentation` uses the existing reflected `PresentationActorClass` field.
It adds no authoritative gameplay state, provider dependency, tick, inventory write, or animation system.

| Role | Mesh under `/Game/Mesh/hansa-bakery/P10/Meshes/` | Visibility |
|---|---|---|
| Bakery | `SM_Bakery_Body` | Completed, ready and blocked |
| Construction | `SM_Bakery_Construction` | Under construction only |
| Flour input | `SM_Bakery_Input` | Ready only; not an inventory quantity |
| Bread output | `SM_Bakery_Output` | Ready only; not an inventory quantity |
| Permanent symbol sign | `SM_Bakery_Sign` | Completed, ready and blocked |

There is no new smoke asset: it is not required by the P02 bakery manifest. Production
simulation still owns recipes, stocks, utilization, blockers and save reconstruction.
The projection actor owns selection/navigation; all child art has collision disabled.
Construction uses its own authored mesh, not an Engine primitive.

## Geometry and material decision

The original roughly 19.49 m depth cannot fit a 3x2 plot. The revision removes longitudinal
bays, shortens the rear bakehouse, lowers upper storage massing, and authors a narrower/lower
door opening. It does not apply runtime auto-fit. The grounded house/bakehouse envelope is
9.0 x 6.75 m; roof/sign/work-access visual overhang is documented separately. The family
retains source -Y / Unreal +Y entrance orientation, an explicit exception to the default +X.
Runtime scale stays `(1,1,1)`.

The original four ImageGen surface-color masters and independently authored physical maps
are reused without raster resizing or new image generation. There are 17 material families.
After a rejected aggressive mesh reduction damaged thin windows, the nearest-view body retains
111,512 source triangles. Three Unreal LODs, rather than blanket Nanite approval, are required.
This is a profiling candidate, not a claim that mass-city performance or Shipping cook passed.

## Approved promotion applied

After the user closed the editor, the retry saved both definitions successfully.
`Saved/Logs/P10ApprovedMigrationRetry.log` and
`Docs/Development/Evidence/P10ApprovedPromotion-20260907.json` record the applied
`DA77AC921DFB6DC0` registry, all 72 fingerprints, reverse-order verification, the
user's approval and the five promoted mesh roles. Their asset metadata now records approval.
Runtime and `Tests/Golden/economic_catalog_v4.json` select v4; v3 remains lineage evidence.
The earlier source-art review reports are retained as historical pre-approval evidence.

### First attempt (resolved)

On 2026-09-07 the user explicitly approved promoting the bakery and applying the reviewed
bread-chain catalog migration. No further promotion approval is needed for that reviewed diff.
The first approved application saved the Grain Farm metadata repair, then failed to save
`DA_Building_Bakery.uasset` with Windows sharing violation 32. Evidence:
`Saved/Logs/P10ApprovedMigration.log` (22:06 local time). Bakery remains on its old binding;
the runtime catalog pin and golden manifest remained v3 during that partial attempt.
No user editor was force-closed and no terrain work was discarded. The subsequent retry
resolved the partial application before the pin was advanced.

### Original review evidence

The automated approval check rejected saving the production migration because repository
instructions require explicit approval before promotion. At that review checkpoint no P10
production definition or catalog pin had been saved. The migration implementation defaults to dry-run and applies only
with the explicit `-MigrateBreadPresentationP10 -Apply` commandlet flags after approval.

The dry-run compiled all 72 definitions in forward/reverse discovery order. Compared with v3,
only `Building.GrainFarm` and `Building.Bakery` change. The resulting candidate registry hash is
`DA77AC921DFB6DC0`. This is evidence for review, not an accepted runtime pin.

- Grain Farm retains its P08 actor and mesh, authored revision 3, and recovers missing P03
  construction-card visibility/category/order/chain fields. Its missing chain metadata currently
  causes `HSA-REGISTRY-038` and prevents the integrated bread-chain scenario loading.
- Bakery advances revision 2 to 3 and uses the new mesh plus native role actor.
- Stable IDs, recipes, costs, workforce, footprint and simulation layout are unchanged.

After approval, apply the migration, review the on-disk fingerprints again, create catalog v4
and update runtime pin, prior-version evidence and lineage regression tests together. Preserve v3.
Existing saves remain exact-hash incompatible: a new game is required until an explicit save
migration is separately approved and tested. Do not merely replace the expected hash.

The P02 visible-content manifest must remain unverified until production binding, real gameplay
captures and applicable Shipping reference checks pass. Isolated/transient Lübeck art comparison
does not prove an operating simulation chain or ordinary placement input.

## Approved-promotion verification (2026-09-07)

- Development Editor build passed: `Saved/P10ApprovedBuildVerified.log`, non-unity,
  MSVC 14.44, `/Od` functional verification (not a performance build).
- All 22 focused tests passed: `Saved/Automation/P10ApprovedVerified/index.json`.
  Coverage includes all nine `Hansa.Content.Definitions` tests, economic asset reload,
  both runtime-host tests, eight save tests, normal shortage scenario initialization,
  and the promoted data-driven Bakery presentation.
- Reload reconstructs exact catalog v3 by reversing only Farm/Bakery presentation
  changes, and exact v2 by removing the P03 schema. Production compilation agrees
  with every v4 fingerprint; historical Engine placeholders exist only in transient
  lineage/legacy tests, never written back to production.
- Runtime construction verifies the authored construction mesh and transition to the
  same completed Bakery actor, rather than requiring an Engine cube.
- `Scripts/ValidateEnhancedMvpBakeryAssets.ps1` passed source hashes, preservation,
  recorded LOD/material/collision/density and saved/reopened review evidence.
- The migration/metadata commandlets still report pre-existing GameFeatureData
  configuration errors. Their own save/application and verification markers succeed;
  this does not declare those unrelated configuration errors fixed.

## Remaining full P10 acceptance

Production completeness-manifest verification, normal scenario placement and visible
save/load reconstruction, 1280x720 and
1920x1080 correlated gameplay captures, and applicable cook/package proof remain open.
No downstream prompt is claimed unblocked by a completed P10 gate.
