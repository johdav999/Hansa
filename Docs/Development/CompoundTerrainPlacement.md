# Terrain-aware labour-house construction

## Scope and behavior

The labour-court compound families now use sampled terrain rather than rendering their authored Surface slots as flat sheets. Ordinary labour-house selection, explicit family construction, preview, completed-building reconstruction and the native compound Details preview share the fitter.

- Sample the plot and each dwelling/workshop at intervals no greater than 100 cm.
- Keep every structure upright at the highest sampled point under its authored envelope.
- Extend individual fieldstone foundation skirts to ground, with a buried lower edge. No parcel-wide retaining slab.
- Generate one non-colliding, non-shadow-casting dirt mesh at 50 cm spacing, 1.5 cm above sampled ground. World-space noise, feathered plot edges, local wear and alpha holes expose original ground.
- Draw worn paths along the authored, clearance-validated access graph, including asymmetric link declarations. Extend a road entrance into the adjacent road only when the road-front mask confirms a road.
- Suppress Landscape Grass with engine exclusion boxes under structures and narrow path segments. Ghosts do not change grass. Demolition, cancellation, rebuild and world teardown unregister exclusions; the Landscape grass system repopulates asynchronously. Unused yard corners remain untouched.
- Reject slopes above 15 degrees, foundation relief above 120 cm per structure, and incomplete terrain coverage. No excavation, terrain deformation or surcharge is introduced.
- Use the saved building identity's existing parcel seed. Live previews receive the next building seed. Reload rebuilds the same coverage and structure transforms; it adds no serialized terrain field.
- Cache fitted geometry by composition, transform and road-front state. Streamed level changes invalidate nearby fits. Terrain edits in authoring use Rebuild Preview.

## Authority and limitations

UHansaRuntimeSimulationHost applies the physical terrain check to both preview validation and the full PlaceBuildingsForAuthority batch before command IDs, funds, inventory, events or simulation ticks change. The domain simulation remains independent of Unreal collision and retains its existing topology validation. Headless worlds with no terrain retain the legacy planar datum; a world containing terrain does not treat missing samples as valid.

All eligible current-stage road contexts are checked to prevent an adjacent road from choosing an unsupported structure layout. Existing saved placements are reconstructed rather than deleted by the new placement policy. The change applies to compound residences; legacy single-mesh buildings keep their existing presentation.

Grass suppression targets Unreal Landscape Grass. Hand-painted foliage, trees and other independently authored actors are not removed. Ground fitting is a presentation projection, not authoritative landscape deformation. Landscape collision must be streamed in for construction.

## Native component and asset inventory

- Existing HISM batches: authored dwelling, workshop, prop, boundary and vegetation meshes.
- Foundations: procedural fieldstone skirts beneath structures.
- GroundCoverage: procedural terrain-conforming dirt and paths, with vertex opacity.
- Existing authored access nodes: projected to the ground.
- Transient scene-component identities: own reversible Landscape Grass exclusions.
- Existing native placement feedback: FoundationTooSteep cause and remedy.
- Existing read-only QueryCompound: sample count, terrain completeness, maximum foundation depth and grass exclusion count.

Native material: Content/Hansa/World/Ground/M_CompoundDirt.uasset.
Reproducible authoring: HansaCompoundGroundMaterials -Apply; dry run is the default and existing material assets are not overwritten.
Source color: existing Content/Mesh/hansa-dirt-road/Textures/T_Road_BaseColor.uasset.
Foundation material: existing Content/Mesh/labour-housing-kit/Materials/M_LabourKit_Fieldstone.uasset.

No new raster imagery, model generation, resampling, provider calls or asset promotion is involved. This is native procedural geometry and material work reusing existing artwork. The ground material has an explicit cook directory; the editor commandlet remains in HansaEditor.

## Schema, migration and impact

No definition property, save version, gameplay identity or economic catalog hash changes. The existing Surface group's tooltip describes its terrain-following semantics; reflected schema export adopts the new help text automatically. Compound impact analysis includes terrain support, paths and grass exclusions. Existing flat-sheet authoring validation still constrains Surface envelopes and architecture stays at unit scale.

## Verification

- Hansa.World.CompoundGround.Fitting: gentle/steep/partial terrain, per-vertex conformity, upright instances, independent foundations, soft coverage, stable reconstruction, cached refits, preview grass isolation, road removal and demolition cleanup.
- Hansa.World.CompoundGround.Authority: preview diagnostic and direct authority batch rejection with unchanged fingerprint and no events.
- Existing compound authoring, projection, save and selection tests remain applicable.
- Hansa.Compound.PlayerFlow: native game placement, upgrades, save/load, demolition, replacement and a district of six adjacent houses.

## Verification results — 2026-09-15

Development Editor build passed. The 2 ground tests, 11 existing compound tests and 1 existing legacy terrain test passed. The real-game PlayerFlow passed at native 1920x1080 and 1280x720, including exact ground vertices/opacity and house transforms after save/load, both upgrades, demolition and replacement. District captures show the raised parcel surface removed and dirt coverage blending into the existing ground; a close-up was inspected for ground contact.

Evidence and SHA-256 hashes: [evidence manifest](CompoundTerrainPlacement/evidence.json).
Native district: [1080p](CompoundTerrainPlacement/1920x1080/district.png), [720p](CompoundTerrainPlacement/1280x720/district.png).
Native close-up: [1080p](CompoundTerrainPlacement/1920x1080/close.png).

Road approaches resolve actual adjacent road cells and follow the inside of the frontage when the entrance is offset. Terrain validation also covers component-tagged terrain and missing-world/headless operation.

The commandlet saved the material, but the engine startup emitted existing GameFeatureData asset-manager errors and returned a failure status; subsequent material loading, shader rendering and gameplay tests succeeded. Game-mode startup also emits existing experimental Toolset Python warnings/errors. These are not shader errors and were not changed here. No full Shipping package was built; this report is not whole-MVP release approval.
