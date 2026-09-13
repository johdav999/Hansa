# Coin sizing trial 2

- Mode: built-in ImageGen
- Status: sizing experiment only; not accepted for production
- Target: native 32x32 icon, or 28x28 artwork on transparent 1024x1024 canvas
- Actual native size: 1254x1254
- Alpha bounds: (210, 21, 1079, 695)
- Visible alpha >=128 bounds: (564, 568, 687, 691)
- Inspection: original inline output inspected; artwork exceeds required native size
- No resizing or cropping performed

## Final prompt

Create a single production UI icon, native 32x32 pixels. It is a tiny flat 2D coin stack for the historical merchant game Hansa. Simple large silver and muted brass shapes with dark outline, three stacked coins, no intricate surface decoration, no lettering, no numerals, no ship, no logo, no frame or background. Genuine transparent alpha. Critically render the icon at only 28 pixels wide and 28 pixels high in the source image. If forced to output a 1024x1024 canvas, place this tiny 28x28 pixel icon at canvas center with 498 pixels of completely transparent margin each side. Do not fill the canvas with a large icon. The icon will be used at 1:1 pixels and cannot be resized. Chalk #FAF7EF, Brass #C19A52, Oak #795137 edge accents. Crisp anti-aliased silhouette and deliberately simple shading suitable for a 32-pixel GUI icon. One icon only.
