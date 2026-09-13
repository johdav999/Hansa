# Economic catalog versioning

## Catalog v15 — approved Fishery presentation

Catalog v15 promotes the accepted `Building.Fishery` presentation from the Engine cube
fallback to `/Game/Mesh/hansa-fishery/SM_HansaFishery.SM_HansaFishery` and advances its
authored revision from 3 to 4. The economic definition, construction cost, workforce,
footprint, recipes and placement rules are unchanged.

Forward and reverse discovery compile to `F1A0A054CB2DFCD9`. Reverting only those two
Fishery presentation fields reproduces catalog v14 `73EC37D013D49BA0`; the full reviewed
manifest is `Tests/Golden/economic_catalog_v15.json`. Exact-hash compatibility remains
strict, so catalog-v14 saves stay on disk but require a new game. The runtime also caches
a failed host-initialization attempt, preventing an invalid catalog from being reloaded
and logged every frame while the frontend displays its unavailable state.

## Catalog v12 — player-buildable beer

Catalog v12 is the user-directed promotion of the existing `Building.Brewery` and
`Recipe.BrewBeer` runtime content into the Lübeck construction catalog. Only the Brewery
fingerprint changes: authored revision 1 to 2, construction visibility enabled, Production
category, and one-stage `Good.Beer` chain membership. Its established economics remain
`3 Good.Grain → 5 Good.Beer` per 100-tick cycle with four Laborers and two Artisans.

The targeted `-EnableBeerProductionChain` migration changes only the Brewery Data Asset.
Forward and reverse discovery compile to `C31520B6DB3A6E09`; reverting only those Brewery
fields reproduces catalog v11 `4170F53E6E9BC675`. The full reviewed manifest is
`Tests/Golden/economic_catalog_v12.json`. Because the exact definition hash changes and no
state migration is declared, catalog-v11 saves remain on disk but require a new game.

This promotion does not claim a production-ready Brewery 3D presentation. The definition still
references the prior Engine cube fallback, so visual Shipping acceptance remains blocked until a
reviewed Brewery asset is created and promoted through the HansaModels workflow.

## EMVP-P01 decision

The `Hansa.Integration.Authoring.EconomicAssetReload` failure was reproduced from the checked-in assets before changing the runtime pin. All 72 definitions loaded and validated, but the compiled registry was `97F691C37A6A4BFF` while `FHansaLubeckScenarioInitializer` still selected `724BD5DE8DB9C292`.

This is a legitimate reviewed content change plus a stale seed-builder defect, not asset-discovery ordering or canonical serialization drift. The approved `Building.Mill` definition advances `AuthoredRevision` from 1 to 2 and assigns `/Game/Hansa/Core/Buildings/BP_HansaWindmill_Animated.BP_HansaWindmill_Animated_C` to `PresentationActorClass`. Both reflected, serialized values deliberately participate in the building content hash. Reversing definition discovery order preserves the registry hash and every per-definition fingerprint. Reverting only the mill to authored revision 1 and clearing that actor assignment reproduces `724BD5DE8DB9C292`; restoring revision 2 and the actor produces `97F691C37A6A4BFF`, and the mismatch report names only `Building.Mill`.

The seed builder had already assigned the new mill actor but still inherited the default authored revision 1. The earlier approved Bakery mesh seed had the same stale-revision defect: its accepted asset was revision 2 while the seed remained revision 1. EMVP-P01 carries both approved revision increments into the seed builder so their semantic authoring inputs agree with the accepted assets. Transient seed objects do not share package-stable `FText` namespaces with saved Data Assets, so their per-definition and aggregate hashes are intentionally not compared to the accepted-asset manifest; order and mutation determinism are compared within the same object context instead. This parity repair is required alongside the runtime version update; accepting the new aggregate number alone would have masked the stale revisions.

The reviewed catalog lineage is therefore:

| Catalog | Registry hash | Meaning |
| --- | --- | --- |
| 1 | `724BD5DE8DB9C292` | Bakery presentation integrated; mill uses its prior mesh-only presentation. |
| 2 | `97F691C37A6A4BFF` | Approved animated windmill actor assigned to `Building.Mill`. |
| 3 | `D8DB2585B867453B` | EMVP-P03 construction categories, ordering, production chains, locks, and presentation metadata. |
| 4 | `DA77AC921DFB6DC0` | User-approved P10 Bakery role assembly and P08 Grain Farm presentation with repaired P03 card metadata. |
| 5 | `DAA463F7043FD66F` | User-approved P12-P15 Lumber Camp, Sawmill, both Residence tiers and Market presentations. |
| 6 | `483D86D8C5549199` | User-approved P17 modular Dock/harbor presentation. |

At the close of EMVP-P01, `FHansaLubeckScenarioInitializer::MvpCatalogVersion` selected version 6.
`Tests/Golden/economic_catalog_v6.json` is the reviewed manifest for its 72 canonical class-path,
stable-ID, and content-hash rows. Version 2 remains checked in as the prior reviewed baseline. This is
intentionally more evidence than a single aggregate number: the reload test reports changed, missing,
and unexpected definitions with expected and actual hashes. If every definition fingerprint agrees
but the aggregate differs, the diagnostic explicitly identifies registry row serialization or
aggregation as the suspect.

## EMVP-P03 decision

Catalog v3 is an intentional authoring-schema migration. Twelve of the fourteen building assets gain
visible construction-card data; Smithy and Brewery carry the schema revision while remaining hidden.
The canonical seeder and the explicit `-MigrateEnhancedConstructionCatalog` commandlet own the data
transition. Runtime and editor both consume the same compiled fields, and the construction UI no
longer owns a duplicate building list or cost/flow formulas.

The catalog hash changed only after compiling the migrated on-disk assets and reviewing all changed
building fingerprints. Canonical hashing omits the new fields for pre-v3 building objects, allowing the
reload gate to clear the construction presentation fields and reproduce the exact catalog-v2 hash.

## Save compatibility

### EMVP-P17 / catalog v6

Only Dock changes from v5: authored revision 1 to 2 and approved mesh/actor references.
All other 71 fingerprints, economics, stable IDs and placement rules are unchanged.
The reload test reconstructs v5 before the older lineage. Exact-hash saves from v5
and earlier require a new game; no existing save is deleted or silently migrated.
See `Evidence/P17ApprovedPromotion-20260908.json`. Promotion is not a claim that
the deferred Rostock placement or the P19 actual-ship berth acceptance has passed.

### EMVP-P12-P15 / catalog v5

The user explicitly approved these four families on 2026-09-08. Only five building
fingerprints change from v4: Lumber Camp, Sawmill, Laborer Residence, Artisan Residence
and Market. The other 67 fingerprints are unchanged. Authored revisions advance 1 to 2;
only presentation mesh/actor references change. All economics, capacities, placement
rules and stable IDs remain unchanged. The reload test reconstructs v4 and then the
older v3/v2 lineage. Source, approval, package hashes and validation are recorded in
`EnhancedMvpApprovedPromotionP12P15.md` and the four dated promotion receipts.

The exact-hash compatibility boundary is preserved: v4 and earlier saves require a
new game with v5. No save was deleted or silently converted. Fishery has no staged
candidate and was not promoted despite the user's P11 approval request.

### EMVP-P10 / catalog v4

The user explicitly approved the reviewed two-definition change on 2026-09-07.
Only Grain Farm and Bakery differ from v3; all other 70 fingerprints remain unchanged.
The applied commandlet compiled in forward and reverse order and reproduced the reviewed
hash. Runtime, full manifest and reconstruction regressions advance together. Stable IDs,
recipe economics, costs, workforce, footprints and simulation layout are unchanged.
The exact-hash save boundary remains: v1/v2/v3 saves require a new game with v4.
No implicit save conversion or acceptance of an old hash was introduced.

The first application repaired Grain Farm but encountered a Bakery file lock. After the
user closed the editor, the retry completed. The durable approval/application receipt is
`Docs/Development/Evidence/P10ApprovedPromotion-20260907.json`.

### Historical catalog-v3 boundary

Catalog v3 changes the authored building schema but does not change stable gameplay identity or the
simulation-state layout. The checked-in Data Assets were migrated explicitly; save envelopes still
embed the exact registry hash. The current save contract rejects a save whose definition or registry
hash differs and requires an explicit state migration rather than guessing that a presentation change
is harmless.

There is no reviewed version-1 or version-2 save migration in this pre-release catalog. Those saves
remain incompatible with catalog v3 and require a new game. This is a deliberate compatibility
boundary, not a bypass: adding a migration later requires a named migration, state replay/round-trip
evidence, and a decision about every catalog transition it accepts. The catalog-v2 hash remains in
source as lineage evidence and is never accepted silently by the version-3 runtime.

## Future catalog updates

For a legitimate change, update the authored revision as required, compile twice with reversed discovery order, review the per-definition diff, assign the next catalog version, update the runtime hash and full golden manifest together, and record the save-compatibility decision. A mismatch must not be fixed by replacing the aggregate hash alone. Ordering changes, canonical serialization changes, or unexplained multi-definition drift are defects until independently explained.

For an illegitimate change, restore or migrate the responsible definition and leave the reviewed catalog version untouched. The focused gates are `Hansa.Content.Definitions.EconomicRegistry` and `Hansa.Integration.Authoring.EconomicAssetReload`; runtime integration must also prove that the selected catalog loads through the normal `FHansaLubeckScenarioInitializer` path.

## Verification (2026-09-07)

Before the fix, `Hansa.Integration.Authoring.EconomicAssetReload` failed with actual decimal hash `10950099812756376575` (`97F691C37A6A4BFF`) versus expected `8235911495414301330` (`724BD5DE8DB9C292`). After the version, seed-parity, manifest, and diagnostic changes:

- the non-unity Development Editor build passed with `-DisableUnity -NoUBA -MaxParallelActions=8`;
- `Hansa.Content.Definitions` passed all eight tests, including discovery-order stability, seed revision parity, and per-definition mismatch reporting;
- `Hansa.Integration.Authoring.EconomicAssetReload` passed from all 72 checked-in assets and proved the version-1 reconstruction;
- `Hansa.Integration.RuntimeSimulationHost` passed both tests and `Hansa.UI.RuntimeScenario.PlayableShortageProjection` passed through the normal runtime initializer;
- `Hansa.UI.World.MillBlueprintPresentation` passed; and
- `Hansa.Integration.Save.CorruptionCompatibilityAtomicity` passed, retaining explicit incompatible-content rejection.

The repository's ordinary unity build route still encounters pre-existing anonymous-namespace symbol collisions in unrelated UI translation units. EMVP-P01 was verified non-unity without changing those unrelated files.
