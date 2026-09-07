# Worn lime plaster — v1

- Status: source texture master, not imported or assigned to a game material.
- Component inventory: one opaque plaster base-color surface; state: lightly dirty/worn. No interactive states.
- Generator mode: built-in ImageGen edit using the attached image.
- Model: not exposed.
- Native target and output: 1254 × 1254, square (1:1); no resampling.
- Style anchor: references/lime-plaster--clean--1254x1254.png (user attachment).
- Intended use: Hansa building plaster shader, sRGB base color.
- Intended Unreal name: T_LimePlaster_Worn_BaseColor.
- Revision: v1 adds subdued dirt, ochre staining, abrasion, shallow flaking and fine cracks while retaining pale warm cream plaster.

## Final prompt

Use case: precise-object-edit. Image 1 is the edit target and style anchor: a warm cream lime-plaster surface texture. Remake this same material with a modest but clearly visible increase in dirt and age. Preserve the pale warm ivory/linen overall color, fine granular lime aggregate, matte plaster identity and square face-on full-frame surface. Add irregular restrained gray-beige dust and embedded grime in pits, soft faded ochre water stains, lightly abraded patches, small shallow flaking areas and a few very fine hairline cracks. Natural uneven wear across the whole surface, without large dominant marks or obvious repeating motifs. Still mostly pale plaster, not ruined, burnt, mold-covered or dark brown. Intended use: Hansa building shader base-color/albedo texture. Photorealistic material scan appearance, orthographic front view, neutral flat diffuse illumination, no directional shadows, no specular highlights, no ambient occlusion baked around imaginary objects, no vignette, border, text, logos, bricks, timber or other objects. Aim for a seamless tile with consistent edge tone. Output a single opaque square texture, target native 1254x1254 matching the supplied image; if unavailable use supported native square resolution without resizing.

## Inspection

Inspected the generated output at its native 1254 × 1254 display: pale cream palette retained, visible wear and light dirt, no text, logo, border or foreign objects. Opaque full-frame surface. No resizing, cropping or postprocessing performed. Seamless tiling was requested but has not been verified on a repeated surface or in an Unreal shader; this is a saved source master, not a fully validated PBR material set.

