# Construction progression: tray

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

Use case: ui-mockup.
Create a high-fidelity construction-menu reference for Hansa, based on this exact description of the user's existing screenshot: a wide navy tray on sandy terrain, fine brass framing, top category row and a lower row of illustrated goods cards. No Windows taskbar. Target native canvas 1536 x 640, wide landscape. Show ONE composed construction tray, enlarged enough to read, centered on a quiet muted sandy game-terrain backdrop with modest margins. No desktop, no taskbar, no title heading, no explanatory arrows, no contact sheet.
Preserve the screenshot's compact horizontal dark navy rectangular shell, fine double brass border, tiny corner fittings, slate rectangular buttons, illustrated colorful three-quarter goods icons and modern readable sans-serif labels. Exact palette: Baltic Navy #152A35, Harbor Slate #29424D, Brass #C19A52, Chalk #FAF7EF, Linen #F2E9D8, Oxblood #762F32. Thin restrained brass linework, no new ornamental system, no fantasy styling, no glossy mobile styling. Typography direction Atkinson Hyperlegible; no blackletter.
TOP ROW: existing category buttons in exactly this order: Roads, Residences, Production, Storage, Harbor, then the compact oxblood demolition button with a brass minus. Use the same road, brick residence, hammer, wooden crates, anchor imagery as screenshot. Production selected with a clear double brass outline.
NEW SECOND ROW directly under categories: a small plain label 'Tier' followed by three broad compact TEXT-ONLY tabs 'Day Laborers', 'Craftsmen', 'Merchants'. No portraits or new tier icons. Day Laborers is selected: slate fill, brass outline and short brass underline. Craftsmen and Merchants are unselected but clearly readable. Keep these three tabs as one coherent secondary navigation row. Exactly three tiers, no fourth tier, no Merchant House tab, no political tab.
THIRD ROW: preserve the five existing illustrated product cards in the same order 'Beer', 'Bread', 'Firewood', 'Fresh fish', 'Planks'. Each has one clean colorful object illustration above the readable label: foaming wooden beer mug, golden loaf, split logs, silver fish, stacked planks. Match screenshot illustrations in spirit and silhouette. Uniform card sizes, 8-pixel-style gaps and generous clean negative space to right. This is a reference illustrating tier navigation; existing product choices are retained for comparison, not a claim about final unlock balancing.
Keep the panel compact and practical: category row about 64px, tier row about 52px, goods cards about 104px at the intended design scale, with simple breathing room and no giant unused internal space. Flat frontal orthographic UI view, crisp readable text, no perspective distortion. All text and controls are mockup references to be implemented natively; no extra content or fake metrics.

## Inspection

Inspected the full native output displayed by ImageGen. Labels are readable and correctly spell Day Laborers, Craftsmen, Merchants. Day Laborers selection is visible. Main reference preserves the existing category order and five labeled goods. Tier selector has no additional fourth tier, new icon family or portrait. Panel contents have safe margins and no unintended watermark. Palette and proportions visually follow the supplied tray, but this is a generated interpretation, not a pixel-exact edit.

The isolated component is a geometry/typography study only; its background/edge treatment is not suitable as a production raster. Native UMG/Slate must supply opaque surfaces, clean edges, focus, hover and disabled states. No alpha certification, measured contrast audit, live interaction test or multi-resolution acceptance is claimed. Goods-to-tier balancing remains undecided.

See README.md for component inventory, state matrix and implementation proposal.
