# Check GUI icon

- Generator: built-in ImageGen; model not exposed
- Intended use: shared GUI Check icon
- Requested size: 32x32 or closest supported native size
- Native size: 1254x1254
- Alpha crop: (368, 424, 888, 834); bounds at alpha >=16 plus four source pixels, retains antialiased edges
- Display variants: (16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 160) pixels square, aspect preserved
- Resampling: premultiplied-alpha Lanczos, authorized 2026-09-10
- State: default artwork; native focus, hover, selected, disabled surfaces
- Style anchor: Coin icon family; prompt matched, reference attachment produced rejected checkerboards
- Revision: v1

## Final prompt

Generate a transparent PNG icon, isolated on a genuinely empty alpha channel. Subject: one bold check mark. Hansa game GUI asset. Request 32x32 pixels, or the closest supported native size. At larger canvas size keep the icon small and centred. Restrained engraved brass #C19A52 with pale linen #F2E9D8 highlight and a dark ink #202628 outline. Large solid silhouette, broad stroke, very little texture, minimal shallow bevel. Designed to be legible at 20px. NOT a photograph. No background at all: no grey checkerboard, no grid, no cloth, no solid backdrop, no external shadow. No text, no watermark, no coin, no frame. Produce actual RGBA transparency, not a picture of transparency.

## QA

Original displayed by ImageGen; alpha validated. Accepted at16/20/24/32/48px on linen and navy. Menu integration reviewed. Broad silhouettes, no painted backgrounds or matte fringes.
