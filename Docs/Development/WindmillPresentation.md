# Animated mill presentation

`Building.Mill` uses `/Game/Hansa/Core/Buildings/BP_HansaWindmill_Animated` through the building definition's **Presentation actor** field. P09 meshes are under `/Game/Mesh/hansa-mill/P09/Meshes/`; the existing rotor Blueprint and materials remain under `/Game/Hansa/Core/Buildings/Windmill/`.

The projection retains stable building identity, placement rotation, authored unit scale (no automatic footprint fitting), construction placeholder, selection and teardown. The Blueprint is a child visual; its nested rotor and native movement component remain live across production/status updates. A hidden visibility-query proxy keeps clicks associated with the managed building instead of its animated children.

## Authoring and compatibility

`UHansaBuildingDefinition.PresentationActorClass` is an optional soft Actor class reference. It takes precedence over PresentationMesh. Empty fields preserve the existing mesh behavior and deterministic hash, so old assets require no destructive migration. Existing schema-driven details, JSON Schema export, reference inspection and diff/impact mechanisms include the reflected field. Metadata declares ActorClass reference, compatible migration, included serialization and AI access Never.

Validation rejects missing, abstract, deprecated, staging and developer actor classes. Assigned references participate in the deterministic definition hash. The mill seed and accepted mill data asset reference the generated `_C` class at the user-requested path. No gameplay identity or save format is derived from that path.

The Blueprint's body and rotor roots are movable for attachment to the placement projection. Its sails turn at 36 degrees/second (6 rpm) using local Y/Pitch. Blocked and constructing states stop the existing rotor without resetting phase; ready restores the authored rate. Construction hides the complete child hierarchy. Physics and navigation remain owned by the existing placement model, including after nested Blueprint refreshes.

## Verification

`Hansa.UI.World.MillBlueprintPresentation` covers the requested class, nested animation, phase preservation across projection refreshes, construction hide/reveal, stable selection, teardown, hash compatibility and rejection of staging classes. Existing projection tests cover mesh-only behavior and manager rebuilding. Production dependency inspection must show no staging/developer packages reachable from the mill Blueprint. Native game evidence and the import/assignment record are saved under `Saved/IntegrationJobs/windmill_game_20260906/`.

The underlying source and visual animation evidence are preserved in `SourceArt/Generated/Buildings/HansaWindmillAnimated_20260906_04/`. The user's request to use this Blueprint for in-game mills authorizes this production integration.

P09's revised packed sources, portable exports, full sail-sweep evidence, three-LOD import and native distance/material captures are in `SourceArt/Generated/Buildings/HansaMill_P09_20260907/`. See [the P09 asset contract](EnhancedMvpMillAsset.md). The current wider catalog gate fails because GrainFarm revision 3 has lost its construction-menu/chain metadata; the mill remains correctly authored as bread-chain stage 2/3. This separate gate is not hidden by the focused mill test results.

### Integration status (2026-09-06)

The production assignment was successfully saved and reloaded from disk. Development Editor builds successfully; all six `Hansa.UI.World` tests and `Hansa.Architecture.Authoring.EconomicSchemaCoverage` pass. The production dependency closure contains no staging/developer assets. Durable promotion and assignment records are under `Docs/Development/Evidence/WindmillPresentation_20260906/`.

`EconomicAssetReload` originally confirmed all 72 production definitions compile but found that the approved mill revision produced `97F691C37A6A4BFF`, while catalog version 1 expected `724BD5DE8DB9C292`. EMVP-P01 reproduced and resolved that blocker by proving that reverting only `Building.Mill` from authored revision 2 to revision 1 and clearing its new `PresentationActorClass` returns the version-1 hash, while accepted-asset discovery reversal leaves both the registry hash and every per-definition fingerprint unchanged. It also repaired the stale seed builder, which had assigned the approved Bakery mesh and mill actor without carrying either accepted asset's revision 2. The reviewed runtime catalog is now version 2 at `97F691C37A6A4BFF`; `Tests/Golden/economic_catalog_v2.json` pins all 72 accepted-asset fingerprints. Existing version-1 saves remain subject to the strict compatibility rejection and require a new game. See `Docs/Development/EconomicCatalogVersioning.md` for the decision and update procedure.
