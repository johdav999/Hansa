# Definition browser selected/focus state

- Status: reference
- Generator mode: built-in ImageGen edit
- Model: not exposed
- Native size: 1254 × 1254
- Transparency: no
- Style anchor: `authoring-studio--composed--valid-warning--1536x1024--v1.png`
- Intended Unreal asset: reference-only; `SSearchBox` and `SListView`
- Revision: v2 removes a contradictory simultaneous empty state and corrects the visible count

## Initial prompt

Use case: ui-mockup
Asset type: Hansa Authoring Studio definition browser component reference, reference-only raster
Primary request: One isolated close-up of a professional desktop editor definition-browser component: section header, native search field, compact filter button, and a short virtualized list with one selected definition row. Focus on this single reusable component only, not a full application or contact sheet.
Intended use: native Slate SSearchBox plus SListView implementation reference
Native dimensions/aspect: square native generator output; do not resize
State: selected row with visible keyboard focus; one hover affordance; no-match empty-state hint visible but secondary
Style/medium: restrained Hanseatic mercantile professional editor UI with modern data clarity
Color palette: Baltic Navy #152A35, Harbor Slate #29424D, Ink #202628, Brass #C19A52, Chalk #FAF7EF, Prosperity Teal #35766F
Materials/textures: nearly flat dark Slate panels, thin ledger dividers, subtle brass focus outline
Composition/framing: centered single browser panel with generous safe margin, readable 40 px-equivalent rows, no clipped edges
Text (verbatim where visible): "Definitions", "Search definitions", "Sample.Foundation", "1 definition"
Constraints: icon plus text identity, focus distinct from hover, accessible contrast, original design, reference only, no watermark
Avoid: full-screen UI, multiple unrelated components, glossy mobile cards, fantasy ornament, logos, watermark, illegible microtext, baked illustration, clipped shadows

## Final correction prompt

Use case: precise-object-edit
Asset type: Hansa Authoring Studio definition browser component reference, reference-only raster
Primary request: Correct only the contradictory list state in the existing definition-browser reference.
Change only: remove the empty/no-match illustration and its text from the lower half; extend the normal list surface naturally into that area; change the count label from "1 definition" to "2 definitions" because two rows are visible.
Keep unchanged: square native dimensions, panel silhouette, header, search control, filter button, the two visible definition rows, selected/focus treatment, palette, materials, spacing, typography direction, lighting, safe margins, and every unrelated component detail.
Constraints: one coherent normal list state only; no resampling; accessible contrast; no watermark.
Avoid: adding rows, adding unrelated components, changing selection, altering style, logos, watermark, clipped edges.

## QA

- [x] Inspected at original resolution
- [x] Correct square native size
- [x] Coherent selected-list state and correct count
- [x] Focus is distinct from selection/hover
- [x] No unwanted logo/watermark
- [x] No resampling used

