# Construction progression: tier-selector

- Status: visual reference only; not an imported or production-ready UI asset.
- Generator mode: built-in ImageGen, new generation from a textual description of the user screenshot.
- Model: not exposed.
- Native dimensions: 1942 x 809.
- Requested target: 1536 x 640; actual tool-native output retained without resizing.
- Pixel format: Format32bppArgb.
- Style anchor: user screenshot codex-clipboard-fd71cdc7-67ea-4353-8f1c-ba6bd55d9484.png and Docs/UIDesignBrief.md.
- Revision: v1, three-tier navigation proposal.
- Intended Unreal asset: reference only.
- Reference-file loading failed due to the Windows sandbox helper; generation used a detailed textual description instead.
- Crop/resampling: none.

## Final prompt

Use case: ui-mockup. Create ONE reusable component reference for Hansa's construction tier selector, NOT a screen or a contact sheet. Native target 1536x640 wide canvas, accept closest native wide size. Flat Baltic Navy #152A35 background. Center one compact horizontal navigation strip: the small label 'Tier', then three equal-height text-only rectangular tabs 'Day Laborers', 'Craftsmen', 'Merchants'. Day Laborers selected with Harbor Slate #29424D fill, thin Brass #C19A52 outline and short brass underline. Other two unselected with restrained slate fills and thin understated borders. Chalk #FAF7EF readable Atkinson Hyperlegible-style sans serif labels, roomy padding, 8px-style gaps, 2–4px-style corner rounding. The row is about 80px high in this canvas; leave plain navy negative space above and below. Subtle painted slate surface, no glossy effects, no icons, no portraits, no extra text, no outer decorative panel, no watermark. Front orthographic UI, restrained Hanseatic mercantile character, matching a dark navy and fine brass construction tray. Reference only; native UI will render labels, geometry and states. No resizing of the generated output.

## Inspection

Inspected the full native output displayed by ImageGen. Labels are readable and correctly spell Day Laborers, Craftsmen, Merchants. Day Laborers selection is visible. Main reference preserves the existing category order and five labeled goods. Tier selector has no additional fourth tier, new icon family or portrait. Panel contents have safe margins and no unintended watermark. Palette and proportions visually follow the supplied tray, but this is a generated interpretation, not a pixel-exact edit.

The isolated component is a geometry/typography study only; its background/edge treatment is not suitable as a production raster. Native UMG/Slate must supply opaque surfaces, clean edges, focus, hover and disabled states. No alpha certification, measured contrast audit, live interaction test or multi-resolution acceptance is claimed. Goods-to-tier balancing remains undecided.

See README.md for component inventory, state matrix and implementation proposal.
