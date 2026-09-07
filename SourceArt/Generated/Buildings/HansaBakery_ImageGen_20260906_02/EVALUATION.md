# Evaluation

Completed material revision, technically verified in source exports and Unreal staging.

- Four generated color masters; nine updated material variants; 17 total slots and 51 delivered PBR maps.
- Updated color maps: native 1254 x 1254. Physical channels and unchanged color maps: 1024 x 1024.
- Reopened Blender master: all 55 image datablocks packed (51 consumed maps plus four generated masters).
- Geometry remains 863,720 triangles / 496,234 vertices. Bounds match both clean reimports: approximately 10.007 x 19.485 x 16.645 m.
- Typical color density: plaster/oak 627 px/m; brick/clay 2508 px/m from the revised UV scale. Projection losses from the original mesh remain; these are nominal plane densities, not a claim that every sliver face achieves them.
- GLB and FBX each imported into separate clean Blender scenes with 51 images; hero, shop and roof renders inspected across formats.
- Final Unreal preview saved/reopened; all 17 slots and nine actual generated color connections verified. Source texture size/color space and mip settings read back.
- Native renders: Blender 1200 square; material swatches 1254 square; Unreal 2253 x 910. Turntable: 960 square, 96 frames, 24 fps.
- PNG signatures, dimensions, CRC and compressed data validated; selected deliverable hashes retained.

Visible limits: brick courses and roof tile layout remain regular; ImageGen improves surface color but cannot repair structural repetition. Oak knots repeat in a four-repeat flat swatch; coherent per-member offsets reduce matching phase on the actual model. Retained physical channels approximate the material rather than reconstruct the generated image's exact microscopic relief. Unreal daylight differs from Blender and some viewport/geometry aliasing remains. Glass, bread, sacks, metal and stone remain the original approximations.

Runtime gates are unchanged: no new LOD chain, custom collision, navigation, Shipping cook, budget acceptance or gameplay integration. This is a material revision in staging, not production approval or a claim of full photorealism.
