# oak texture

- Mode: built-in ImageGen, generation; model not exposed.
- Native dimensions: 1254 x 1254, opaque square; original retained without resampling.
- Intended use: synthetic sRGB base color, 1 x 1 metre coverage; not a scan.
- Revision: v1, first material-family anchor. Physical roughness and relief authored separately.
- Inspected generated output: restrained weathering, no text or joints; shader repetition QA pending.
- Original: C:/Users/Johan/.codex/generated_images/01a0770c-7c7e-7f90-934e-85eae2801654/exec-2f7cfa40-017c-4fc3-98e3-80c0763335a7.png

## Final prompt

Use case: photorealistic-natural. Asset type: synthetic base-color texture input for an editable Hansa Baltic post windmill shader. Generate one native 1024x1024 square, opaque, tileable surface of heavily weathered unpainted oak, physical coverage 1x1 metre. Straight-on flat frame-filling wood material, fine irregular vertical grain, narrow age checks, subtle silver grey fibres over desaturated warm brown Oak #795137 undertones, ingrained grey dirt and sparse dark pores. Realistic worn, dirty, decades outdoors but structurally sound. No boards or joints: individual plank geometry supplies those. Uniform diffuse flat illumination, seamless opposite edges, low broad contrast. No cast shadows, highlights, AO, vignette, perspective, lettering, watermark, border, knots larger than 3cm, green blanket moss or decorative scratches. Vertical grain must flow continuously across top/bottom. This is a material swatch, not a building photo.


## Final integration QA

Native source inspected and unchanged. Connected to the packed Blender source shader; repeated swatch and model inspected. Final portable color is a native 1024-square shader bake for mipmapping, with independent 1024 roughness/normal and geometry weathering. Actual Unreal material slot and map dimensions verified. This is a synthetic source-art master; production promotion remains pending review.
