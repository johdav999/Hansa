# Coin sizing trial 1

- Mode: built-in ImageGen
- Status: sizing experiment only; not accepted for production
- Target: native 32x32 icon, or 28x28 artwork on transparent 1024x1024 canvas
- Actual native size: 1254x1254
- Alpha bounds: (97, 51, 1196, 1182)
- Visible alpha >=128 bounds: (388, 402, 881, 872)
- Inspection: original inline output inspected; artwork exceeds required native size
- No resizing or cropping performed

## Final prompt

Use case: stylized-concept. Production GUI icon for Hansa, not a mockup or reference sheet. Generate ONE small stack of three medieval silver and muted-brass merchant coins, readable simple hand-painted silhouette, restrained Baltic mercantile engraved style. Exact intended output dimensions: 32x32 pixels with genuine transparent alpha background; render native small pixel artwork with crisp edges, no lettering, no numerals, no drop shadow, no outline frame, no watermark. Colors Brass #C19A52, Chalk #FAF7EF highlights, Oak #795137 dark edges, with contrast on Baltic Navy #152A35 and Linen #F2E9D8. The entire object must occupy no more than 28x28 source pixels. If the renderer requires a larger canvas, retain the 28x28-pixel icon at the exact center of a transparent 1024x1024 canvas, with the entire remainder fully transparent; do not enlarge the icon to fill the canvas. We will only remove transparent margins without resizing the artwork. Single coin-stack icon only, no contact sheet, no text, no other objects.
