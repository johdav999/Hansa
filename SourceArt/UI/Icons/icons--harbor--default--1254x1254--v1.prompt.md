# Harbor GUI icon

- Generator: built-in ImageGen; model not exposed
- Intended use: shared GUI Harbor icon
- Requested size: 32x32 or closest supported native size
- Native size: 1254x1254
- Alpha crop: (158, 75, 1095, 1178); bounds at alpha >=16 plus four source pixels, retains antialiased edges
- Display variants: (16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 160) pixels square, aspect preserved
- Resampling: premultiplied-alpha Lanczos, authorized 2026-09-10
- State: default artwork; native focus, hover, selected, disabled surfaces
- Style anchor: Coin icon family; prompt matched, reference attachment produced rejected checkerboards
- Revision: v1

## Final prompt

Generate a single transparent PNG GUI anchor icon for Hansa. SQUARE canvas, requested32x32 or closest supported native square size. Match engraved brass family style anchor: bright Brass #C19A52 and broad Linen #F2E9D8 highlights, shallow Oak #795137 bevel, slim Ink #202628 outline. The entire anchor must be light brass, highly visible on both navy #152A35 and linen #F2E9D8 at20px. Broad shank, ring and flukes, simple flat frontal silhouette. Real transparent alpha outside silhouette, empty surroundings, no shadow, glow, backdrop, words, numbers. Generous padding. Replace earlier anchor that was too dark. Preserve aspect when resizing for GUI.

## QA

Original displayed by ImageGen; alpha validated. Accepted at16/20/24/32/48px on linen and navy. Menu integration reviewed. Broad silhouettes, no painted backgrounds or matte fringes.
