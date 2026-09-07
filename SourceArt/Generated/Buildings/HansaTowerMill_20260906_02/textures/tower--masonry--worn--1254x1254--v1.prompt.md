# ImageGen source: masonry

Mode: built-in generate, no reference image uploaded. Target square 1024 if supported; actual native output 1254 x 1254, retained unchanged. Intended use: base-color input to tower mill shader. No raster resampling.

## Exact prompt

Generate a single seamless square photorealistic base-color surface texture for a 3D Hansa historical windmill masonry tower, 1024 x 1024 native pixels if supported. The image represents a 3 x 3 metre area, straight-on orthographic, edge-to-edge flat surface. Old Baltic rubble masonry under badly weathered warm grey-beige lime plaster: about 55 percent irregular eroded plaster islands, 45 percent exposed irregular small fieldstones of chalk cream, muted brown, slate grey and a few faded reddish fragments embedded in granular old mortar. Individual stones about 10–35 centimetres, irregular uncoursed construction, NOT neat rows, not cobbles. Plausible dirty mineral staining, granular lime, crumbling plaster edges, subtle hairline cracks and restrained dark age marks. Desaturated grey-brown historical realism. Uniform flat diffuse albedo lighting with no directional light, no shadows, no ambient occlusion, no highlights, no perspective. No windows, no doors, no roof, no vegetation, no ground, no text, no watermark, no borders. Seamlessly tileable across both axes. This is one material texture, not a photograph of a building and not a collage.

## Initial inspection

Native generated output inspected. Subject, opacity, square aspect, absence of text and directional illumination accepted. Physical channels authored independently. Generated detail is synthetic, not a scan. Masonry coverage adjusted in shader to 2.5 m to match reference stone scale; timber and paint 1 m. Model and repeated-swatch QA follows.


Final integration: native source retained and packed, actual shader and portable map consumers verified through clean reimport and Unreal. See material inventory and assessment.
