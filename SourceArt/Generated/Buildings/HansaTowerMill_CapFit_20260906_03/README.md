# Tower windmill — corrected cap fit

The cap was centred, but its 2.15 m front/back half-depth was smaller than the 2.55 m tower crown radius. Its lower edge also sat 6 cm above the tower. This produced the apparent misalignment reported in the attached screenshot.

The corrected cap has a 2.90 m half-depth and unchanged 2.82 m half-width, remains centred at XY (0, 0), and sits 10 cm lower. A closed timber seating curb bridges the junction. Cap windows, sails and attached ironwork follow the revised front plane. Shingle UVs were adjusted to preserve their physical density.

## Deliverables

- Editable packed source: [HansaTowerMill_CapFit.blend](exports/HansaTowerMill_CapFit.blend).
- [FBX](exports/HansaTowerMill_CapFit.fbx) and [GLB](exports/HansaTowerMill_CapFit.glb).
- [Before/after comparison](comparison.html), [Unreal cap view](renders/unreal_CapFit.png), [Unreal side view](renders/unreal_CapSide.png), and [turntable](renders/turntable.mp4).
- New staging mesh: `/Game/Hansa/Generated/Staging/HansaTowerMill_20260906_02/Meshes/SM_HansaTowerMill_CapFit`.
- Updated saved preview: `/Game/Hansa/Developer/GenerationPreview/HansaTowerMill_20260906_02/L_TowerPreview`.

## Inventory and materials

Tapered masonry tower, brick-framed openings, sage doors, shingle cap and new seating curb, four lattice sails, shaft and iron fittings. Eight material families: masonry, recessed timber, fieldstone, weathered timber, brick, sage paint, glass and iron.

This geometry correction reuses the five original built-in ImageGen worn surface masters and their hybrid shaders. No new raster generation or resampling was needed. All five 1254 × 1254 originals and exact sibling prompt records are in [textures](textures/), and are packed in the source. The 24 portable shader-baked maps remain 1024 × 1024. Generation mode and exact prompt sets remain recorded beside each original. Glass, iron and hidden timber use the existing authored procedural exceptions.

## Verification

Clean FBX and GLB reimports passed bounds checks and were rendered. Source/export: 284,416 triangles; Unreal: 284,010, one LOD. Import removes degenerate triangles. Unreal bounds, handedness, metre-to-centimetre conversion, eight material assignments, all texture dimensions and colour settings passed readback. Vertex colors use Replace, preserving weathering. The preview was saved and reopened successfully.

Native Blender cap comparisons and reimport renders are 1100 × 1100; Unreal captures are 1116 × 905. Side and overhead views were visually inspected: the crown is covered and the junction is seated. The 48-frame 800 × 800 turntable passed full-envelope framing checks. JPEG inspection copies retain their PNG dimensions.

The previous delivered source master was verified unchanged by SHA-256. See [fit measurements](fit_measurements.json), [Unreal checks](unreal_final_verification.json), and [import options](unreal_import_options.json).

## Scope

Imported assets remain review-stage content. Production promotion remains pending the explicit approval required by repository AGENTS.md. The preview images are evidence, not shipping textures. The original photo supports the front appearance; exact historical dimensions and rear construction remain inferred. This remains a static asset with opaque glass, without certified collision, navigation, animation, distance LODs, performance or Shipping-cook acceptance.
