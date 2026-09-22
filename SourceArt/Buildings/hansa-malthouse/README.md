# Hansa malt house

Created 2026-09-14 using the hansamodels workflow. Original late-medieval north German reconstruction: a low ventilated malting hall, grain loft and hoist, attached malt kiln with a louvred cowl, service yard, tub, sorting table, shovel and sacks. It is not a measured replica of a particular surviving building.

## Deliverables

- [Packed editable Blender master](Model/HansaMaltHouse.blend)
- [FBX](Model/SM_HansaMaltHouse.fbx), [GLB](Model/SM_HansaMaltHouse.glb), [portable texture maps](Model/Textures/)
- [Unreal preview](Evidence/unreal-courtyard.png), [turntable](Evidence/malthouse-turntable.mp4)
- [Evaluation](EVALUATION.md), [provenance and texture sources](PROVENANCE.md), [reference manifest](reference_manifest.csv)
- [Iteration record](ITERATIONS.md), [material gap ledger](MATERIAL_ASSESSMENT.md), [native-size comparisons](comparison.html)
- [Material inventory](Evidence/material-inventory.json), [geometry audit](Evidence/geometry-audit.json), [source reopen audit](Evidence/source-reopen-audit.json)

Unreal project: `Hansa.uproject` at the repository root. Mesh: `/Game/Mesh/hansa-malthouse/Meshes/SM_HansaMaltHouse`. Materials and textures occupy sibling folders. Isolated review level: `/Game/Hansa/Developer/GenerationPreview/L_MaltHouse_20260914`.

Verified import for review; gameplay replacement is pending approval. The existing malt-house seed still points at the bakery. No gameplay definition was changed. Mesh scale is 1, footprint approximately 9.695 by 9.824 m, height 9.194 m, 271,276 triangles and seven material slots. It fits the 11.6 m inset of a 3 by 3 plot using 4 m cells. Entry faces +X; ground pivot is at the origin.

Nanite is disabled after its conversion visibly damaged the small disjoint pieces. Two simple volume colliders are authored; runtime collision, navigation, LOD budgets, memory and Shipping acceptance remain untested. See evaluation for density and historical limitations.

The source shaders remain editable. `Scripts/` preserves the actual generation and correction scripts; their job-relative paths expect the working job directory `Saved/GenerationJobs/hansa-malthouse_20260914`. The packed master and portable exports are independently usable without that directory.
