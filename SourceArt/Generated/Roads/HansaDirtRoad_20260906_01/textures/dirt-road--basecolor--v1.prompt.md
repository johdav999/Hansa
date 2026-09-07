# Hansa dirt road — generated base color

- Status: synthetic material source, imported to draft staging.
- Generator mode: built-in ImageGen, generate.
- Model/version: not exposed by the tool.
- Intended resolution: 1024 x 1024 square; returned native size: 1254 x 1254. Adopted the returned supported native variant; no raster resizing.
- Transparency: opaque.
- Physical coverage: 2 x 2 metres, repeated on UV0; 627 source pixels per metre.
- Style anchor: user-supplied dirt road photograph, appearance reference only; no photograph pixels used as texture.
- Intended Unreal texture: T_DirtRoad_BaseColor.
- Revision: v1; neutral sandy-earth interpretation of the photograph, without its sunset cast.

## Final prompt

Use case: photorealistic-natural. Asset type: Hansa realistic rural dirt road tiling base-color texture source, one material only. Generate a native 1024x1024 square texture representing a 2 metre by 2 metre patch of compacted dry sandy earth with fine scattered rounded gravel, matching the supplied road reference's natural sandy ochre-brown soil, but under neutral daylight without the reference sunset's red color cast. Flat orthographic directly overhead frame-filling surface, uniform diffuse illumination, seamless opposite edges horizontally and vertically. Predominantly finely compacted sandy soil, occasional embedded 3-15 mm pebbles, sparse tiny pale stone grains, subtle irregular tan brown mineral mottling. Restrained natural colors: dusty beige-brown sand and muted warm grey pebbles, no saturated orange. This is an albedo material input, not a photograph of a road: no wheel tracks or road borders because those will be authored in mesh geometry and masks; no grass, plants, large rocks, cracks, puddles, mud clods, tread patterns, cast shadows, highlights, ambient occlusion, vignette, perspective, text, labels, frame, logos or watermark. Equal detail density and tone across all four boundaries. Preserve a believable fine grain scale.

## Inspection

Original generated output inspected at its native size. Fine gravel and compacted sand present; no text, watermark, border, vegetation or intentional directional light. Repeated source-shader renders inspected. Opposing edges are visually approximate, not pixel-identical. The non-power-of-two source retains noticeable distant aliasing in Unreal; mip/streaming optimization is not accepted. Source pixels preserved without resampling. Roughness and normals authored independently in Blender, not derived from image luminance.
