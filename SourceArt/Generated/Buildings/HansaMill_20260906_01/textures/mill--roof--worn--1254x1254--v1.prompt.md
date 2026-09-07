# roof texture

- Mode: built-in ImageGen, generation; model not exposed.
- Native dimensions: 1254 x 1254, opaque square; original retained without resampling.
- Intended use: synthetic sRGB base color, 1 x 1 metre coverage; not a scan.
- Revision: v1, first material-family anchor. Physical roughness and relief authored separately.
- Inspected generated output: restrained weathering, no text or joints; shader repetition QA pending.
- Original: C:/Users/Johan/.codex/generated_images/01a0770c-7c7e-7f90-934e-85eae2801654/exec-8d76e75f-6913-41ef-94b4-b068c962593c.png

## Final prompt

Use case: photorealistic-natural. Asset type: synthetic base-color shader texture for a realistic weathered Baltic wooden windmill. One native square 1254x1254 opaque surface, covering 1x1m. Aged grey split pine shingle surface material, fine vertical fibrous grain, sun-bleached silvery tan charcoal grey, weathered uneven soft streaks of ingrained dirt, scarce small lichen flecks. Closely related restrained desaturated brown/grey family to old oak with Oak #795137 undertones. Frame filling straight on flat surface with uniform diffuse illumination and seamless edges. No shingle or board grid or joints: modeled overlapping shingles supply those. No cast shadows, highlights, ambient occlusion, vignette, perspective, lettering, watermark, borders, green moss blanket, strong repeated damage. Physical fibre detail, worn out but structurally sound. This is one continuous wood surface color input, not a building photo.


## Final integration QA

Native source inspected and unchanged. Connected to the packed Blender source shader; repeated swatch and model inspected. Final portable color is a native 1024-square shader bake for mipmapping, with independent 1024 roughness/normal and geometry weathering. Actual Unreal material slot and map dimensions verified. This is a synthetic source-art master; production promotion remains pending review.
