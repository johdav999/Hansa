# Market panel reference

- Status: visual reference only; not imported or shipped
- Generator mode: built-in ImageGen
- Model: not exposed
- Native size: 1024 x 1536
- Transparency: opaque linen
- Style anchor: market--panel--partial--1024x1536--v1.png
- Revision: v1, initial requested market-demand design
- Intended use: native Slate reconstruction; no raster used in runtime

## Final prompt

Use case: ui-mockup. Create an original Hansa market building detail panel visual reference, target native 1024x1536 portrait. Flat front-on UI, no perspective. Composition: narrow compact navy header 'Market' and close control, linen working surface, heading 'Citizen demand', small subtitle 'City-wide · last consumption tick'. Four generously spaced rows with engraved native-shape-like product icons paired with labels Bread, Fish, Beer, Tools. Each row displays a horizontal pill bar with genuinely rounded end corners, numeric fulfillment and plain status: Bread 75% supplied, Fish 100% supplied, Beer 25% supplied, Tools 0% supplied. Beneath bars show small supplied / required amounts. Footer has restrained Details and Frame controls. This is a reference only; all text, bars, icons, and controls will be native Slate widgets. Palette exactly Baltic Navy #152A35, Harbor Slate #29424D, Ink #202628, Linen #F2E9D8, Parchment #DFCFAF, Brass #C19A52, Prosperity Teal #35766F, Oxblood #762F32. Use Source Serif 4 style header and Atkinson Hyperlegible style body. Restrained linen texture away from data, thin brass rules, 8px spacing system, no fantasy ornament, no copying existing games, no purple palette, no glossy buttons, no portraits or decorative artwork. Bars use teal filled portion, dark ink outline for contrast, parchment unfilled portion. Show explicit percent and supplied label so status is not color-only. Safe margins 32px. Components/states: reuse Hansa inspector shell/header/close/Details/Frame and product glyph system; new reusable demand row states measured full/partial/zero, no demand, pending and unavailable; bars read-only, no hover/pressed/selected behavior. Controls use existing brass focus ring, hover and pressed token states. No logos or watermarks.

## Inspection

Native pixel dimensions verified from PNG header. Inspected through original-detail image output.
Labels, percentages, bar proportions, silhouette, safe margins and Hansa material/palette direction checked.
No clipping, unwanted logos or watermark. Opaque alpha is intentional.
No resampling or cropping. Reference texture is more pronounced than the flat native operational surface.
The panel is a hierarchy reference; existing Hansa shell, navigation and controls are reused.
