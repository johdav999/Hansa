# Provenance

Job: `hansa-malthouse_20260914`. Authoring: Codex, headless Blender 3.5.1, built-in ImageGen, Unreal 5.8 through Unreal MCP. No purchased models, provider API purchase, or third-party photograph used as a texture.

This is a functional reconstruction informed by malt-house history and Lüneburg brewery architecture. Low ventilation openings and a separate kiln follow malting functions; footprint, roof proportions, kiln arrangement and workyard are inferred for Hansa's plot contract. The Lüneburg photograph shows a different, stepped-gable building. It supports material and regional architectural language, not exact geometric fidelity. Modern industrial brewery equipment and the museum's 1902 building are not modeled.

## Color sources

| Family | Source | Actual native dimensions | Treatment |
|---|---|---|---|
| Brick | Built-in ImageGen, new generation | 1254 × 1254 | Continuous clay face; joints are geometry |
| Roof clay | Built-in ImageGen, new generation | 1254 × 1254 | Replaced cloudy earlier roof input; tile overlap is geometry |
| Oak | Reused brewery ImageGen v2 source | 1254 × 1254 measured | Original filename/record claimed 1536; actual decoded dimensions take precedence |
| Mortar, recess, iron, sacking | Procedural | Baked to 1024 × 1024 | Simple utility surfaces and independently authored cloth weave/roughness/relief |

New generation requests targeted a square 1536 image; tool returned native 1254 squares. Selected originals are preserved unresampled. Sibling prompt records contain the full prompts. Oak came from `SourceArt/Buildings/hansa-brewery-huexstrasse128/TextureSources/brewery--oak--basecolor--1536x1536--v2.png`; its prior source record remains unchanged.

All seven material families have separately baked BaseColor, Roughness and tangent NormalGL maps at native bake resolution 1024². These are shader bakes, not resized ImageGen originals. Base color uses sRGB; roughness and normal use data color space. No AO or lighting is baked into base color. Normals are OpenGL +Y; Unreal texture import flips green. Iron metallic is a constant 0.78, restored explicitly in Unreal. Image generation does not provide measured PBR values; physical channels are authored approximations.

Generator model/version and commercial attribution terms were not exposed by the built-in tool. Generated imagery is synthetic, not evidence. Research-photo creator and redistribution license are unknown; the downloaded reference stays only in the Saved job and is not part of shipping textures. See reference_manifest.csv for URLs and use restrictions.
