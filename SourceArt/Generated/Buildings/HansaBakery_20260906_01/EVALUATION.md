# Evaluation

Status: editable exterior model and technically verified Unreal staging import, awaiting human appearance review and production approval.

- Model: 863,720 triangles and 496,234 vertices after export preparation, 17 material slots.
- Measured overall bounds including props: 10.007 x 19.485 x 16.645 metres. Main house footprint 9 x 13 m. Scale is inferred, not surveyed.
- Textures: 51 original procedural PNG maps, 1024 x 1024 each. CRC, compressed image streams and dimensions checked. All 51 consumed maps packed in the reopened master.
- UV coverage: one canonical UV0_MetreTiling channel. Median density about 480-512 px/m. Lowest measured values occur on small curved/boolean/sliver faces (approximately 252 px/m); detailed per-family measurements are in evidence/measured_density.json. No zero-density material minimum remains.
- GLB and FBX reimport: actual clean-scene imports, matching bounds to within floating-point tolerance, 51 maps present, rendered hero/roof/shop checks. Source unit is metres, front -Y, up +Z. Unreal correctly imports centimetres and reflects Y through its coordinate conversion; no extra manual scale factor was applied.
- Unreal: final SM_HansaBakery_R2 saved with all 17 named slots assigned. Every texture read back at 1024 x 1024. Base color is sRGB; roughness and normals are data; OpenGL +Y normals use Unreal green-channel inversion. Preview saved and reopened before captures.
- Preview: native 2253 x 910 engine screenshots, Blender stills 1200 x 1200. Turntable 960 x 960, 96 frames, 24 fps, four seconds. No raster resizing was used to create variants.

## Limits and remaining gates

The unseen rear, bakery shop arrangement and working props are reconstructions. The gable simplifies the photographed building's moulding and masonry irregularity. Materials remain procedurally regular and are intended for exterior game inspection around 5 m or farther, not macro photography. Glass is a documented opaque reflective approximation. Interior floors are a visual shell; no accessible interior or working oven/fire simulation is delivered.

This is a high-detail source asset. It has no authored LOD chain, custom collision, navigation test, production budget acceptance, gameplay integration, or Shipping cook proof. The substantial mesh/texture size needs a separate runtime optimization pass before widespread city placement. The preview and staging packages do not constitute production acceptance.

Windows sandbox process creation failed at the start; the approved shell fallback was used. Native image pixels were read through that shell because the image viewer and browser runtimes could not launch. This did not require changing the repository's render or application settings.

## Final Unreal surface adaptation

Baked normal response was too strong under direct engine daylight. Nine masonry/clay/lime material families now blend 25% sampled tangent normal with 75% flat tangent normal. This is an engine-specific material adaptation; original maps and Blender source shaders are preserved. Before capture: renders/unreal_Hero_before_normal_fix.png. Final capture: renders/unreal_Hero.png. Some regularized brick and tile repetition remains visible; the result is still an exterior review draft. The sun source angle is 6.9 degrees to match the soft source-review lighting, with 12000 lux and fixed EV100 11.5. All 17 slot assignments and 51 native texture sizes were read back again after saving/reopening; see evidence/final_unreal_verification.json.
