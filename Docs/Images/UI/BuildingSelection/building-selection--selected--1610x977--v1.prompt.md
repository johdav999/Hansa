# Building selection — selected state

- Status: reference
- Generator mode: built-in ImageGen
- Model: not exposed
- Native size: 1610 × 977
- Transparency: no (composed gameplay reference)
- Style anchor: `Docs/UIDesignBrief.md`; input screenshot `building-selection--current-reference--459x280.png`
- Intended Unreal asset: reference-only; reconstructed with native mesh-hugging selection shells, custom-depth tagging, and footprint components
- Revision: v1; refined from an unsaved first pass to reduce glow and ornament
- Resampling/crop: none

## Component inventory

| Component | Role | Implementation classification |
| --- | --- | --- |
| World viewport and building | Context retained from the supplied screenshot | Existing 3D scene; unchanged in shipping UI |
| Silhouette contour | Persistent selected-object boundary | Native transient inverted-hull mesh shell plus custom-depth tag |
| Separation halo | Keeps the contour legible over bright terrain and dark shadow | Slightly larger Baltic-blue inverted-hull shell |
| Footprint corner ticks | Grounds the selection and adds a non-color shape cue | Eight native mesh segments arranged as four L brackets |
| Front diamond notch | Gives the selected state a consistent facing/anchor cue | Native mesh component at the front footprint edge |
| Footprint wash | Optional, near-transparent spatial grounding | Native projected decal/material |
| Panels, text, icons, charts, navigation | Not required for this selection-only reference | None |

## State guidance

| State | Treatment |
| --- | --- |
| Default | No selection overlay |
| Hover | Baltic Blue contour only; no corner ticks or diamond |
| Pressed | Brief 80–100 ms brass confirmation, then resolve to selected or default |
| Selected | Brass contour, narrow Baltic Blue separation halo, footprint corner ticks, and front diamond as shown |
| Disabled/unselectable | No selected treatment; explain through the relevant cursor/tooltip system |
| Keyboard/controller focus | Selected treatment plus the shared high-contrast focus treatment; must remain distinct from pointer hover |
| Loading | Preserve selected geometry; any activity indication belongs in the inspector, not the world overlay |
| Warning | Preserve selection treatment and add the shared amber warning shape/icon at the causal UI surface |
| Error/critical | Preserve selection treatment and add the shared oxblood critical shape/icon at the causal UI surface |

## Final prompt

```text
Use case: precise-object-edit
Asset type: Hansa in-game world-space building-selection visual reference, refined pass
Input images: Image 1 is the original pulled-back gameplay screenshot and the authoritative scene/composition target. Image 2 is the first generated selection concept and is reference only for the brass contour + footprint-corner idea.
Primary request: Apply a much subtler version of Image 2's selection language to Image 1. Remove Image 1's yellow floating teardrop and pale opaque ground strip. Change nothing else.
Keep unchanged from Image 1: exact pulled-back isometric camera, wide 459:280 landscape composition, generous terrain margin, building size and position (roughly half the frame width), exact house geometry, terracotta roof tiles, chimney, windows, sandy terrain, warm daylight, and cast shadow.
Selected treatment: a very thin crisp brass #C19A52 contour hugging the visible building silhouette, visually 1–2 pixels at Image 1 scale, with only a narrow low-opacity Baltic Blue #397FA3 separation halo so it survives both bright ground and dark shadow. Add short fine L-shaped brass corner ticks at the projected ground-footprint corners, visually 10–14 pixels long at Image 1 scale and no thicker than the contour. Add a tiny solid brass diamond notch, visually 6–8 pixels, centered just beyond the front footprint edge as a non-color shape cue. Any Harbor Slate #29424D footprint wash must be nearly transparent, allowing terrain texture to remain clearly visible.
Style/medium: polished real-time Unreal Engine gameplay screenshot; native post-process outline plus decal/material reference; understated Hanseatic mercantile UI with modern clarity.
Lighting/mood: preserve Image 1 lighting exactly; no emitted scene light and no bloom.
Text: none.
Constraints: preserve Image 1, not Image 2, for camera, composition, scale, and all scene content. Selection is conveyed by contour plus corner/diamond shape, not color alone. Building remains dominant. No watermark.
Avoid: zoom, crop, camera change, altered building, thick outline, bloom, fuzzy aura, fantasy magic, vertical beam, floating arrow, pin, teardrop, opaque rectangle, selection circle, pulsing rings, particles, grid, text, labels, or new objects.
```

## QA

- [x] Inspected visually at generated resolution
- [x] Native dimensions recorded: 1610 × 977, 24-bit RGB
- [x] Correct landscape aspect for the intended gameplay reference
- [x] Brass/blue selection language matches the Hansa palette
- [x] Selection uses contour and corner/diamond geometry, not color alone
- [x] No unwanted text, logo, watermark, floating pin, beam, or opaque selection slab
- [x] State and safe margins verified
- [x] No resampling or crop used
- [ ] Exact source framing was not preserved by the generator; use this image for treatment and hierarchy, not pixel matching

## Implementation notes

- Keep the shipping effect native and resolution independent. This composed bitmap is not a production texture.
- Implemented in `AHansaBuildingWorldProjectionActor`: the visible authored static meshes receive nested brass/blue shells, while the authoritative footprint owns the corner brackets and front diamond.
- Selected primitives also receive custom-depth stencil value `1`, preserving compatibility with a future shared post-process renderer without making that renderer a dependency of this feature.
- Target approximately 1–2 px apparent contour width at 1080p, compensating for distance so the line does not become a thick halo on close views.
- Depth-test the contour where appropriate; do not reveal fully occluded buildings unless an explicit x-ray mode is active.
- Keep the footprint cue terrain-conforming and clipped to the selected actor's authoritative footprint.
- Motion is optional and should be limited to a single short confirmation settle. Reduced-motion mode should be static.
