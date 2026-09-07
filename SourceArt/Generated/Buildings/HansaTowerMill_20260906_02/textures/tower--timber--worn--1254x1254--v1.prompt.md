# ImageGen source: timber

Mode: built-in generate, no reference image uploaded. Target square 1024 if supported; actual native output 1254 x 1254, retained unchanged. Intended use: base-color input to tower mill shader. No raster resampling.

## Exact prompt

Use case: photorealistic-natural. Asset type: seamless base-color texture for historical windmill roof shingles and sail timber. Generate one square native 1024 x 1024 if supported, representing 1 x 1 metre of continuous very old weathered oak surface. Straight-on orthographic flat diffuse albedo, silver grey and charcoal grey desaturated timber with slightly warm fibres, irregular fine grain running vertically, worn raised fibres, hairline longitudinal checks, restrained sheltered dirty grey deposits, subtle bleached grain. Realistic old outdoor wood, not brown new lumber, not striped zebra wood. NO plank seams or shingle edges because those are modeled in 3D, no nails, no board layout, no pronounced knots, no lighting gradient, no shadows, no specular highlights, no ambient occlusion, no text, no watermark, no border, no building. Tile seamlessly in both directions. Single homogeneous material surface.

## Initial inspection

Native generated output inspected. Subject, opacity, square aspect, absence of text and directional illumination accepted. Physical channels authored independently. Generated detail is synthetic, not a scan. Masonry coverage adjusted in shader to 2.5 m to match reference stone scale; timber and paint 1 m. Model and repeated-swatch QA follows.


Final integration: native source retained and packed, actual shader and portable map consumers verified through clean reimport and Unreal. See material inventory and assessment.
