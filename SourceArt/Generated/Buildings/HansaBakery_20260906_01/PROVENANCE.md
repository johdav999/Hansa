# Hansa bakery provenance

Created 2026-09-06 for the Hansa project using the user-requested hansamodels skill.

## Inputs and historical limits

R01 is a tourism photograph of Muehlenstrasse 1, Stralsund, credited TMV / Gaensicke. The official tourism page identifies a late-13th-century building remodeled in the 14th century and a surviving 17th-century oven. R02 supplies bakery-use context. URLs, photograph hash, observed features and limits are in reference_manifest.csv.

The model is an original exterior adaptation for Hansa, not a measured replica or a claim that the displayed shop existed in 1650. The main footprint (9 x 13 m), approximately 16.7 m maximum height, rear bakehouse, oven opening, chimney, trading hatches, shutters, sign, props, rear elevations and roof structure are inferred design decisions. The source photo depicts a restored modern condition, not a period survey. No unseen elevation was presented as photographed evidence.

## Creation mode and rights

Headless Blender 3.5.1, deterministic Python construction, procedural shaders and Cycles emission/normal baking. No ImageGen image, remote 3D provider, paid API, purchased asset or scanned texture was used. All shipping-candidate mesh geometry and texture pixels were constructed for this job. Photo pixels are not in the model or its maps. The reference photo remains in the internal job folder and is excluded from this selected asset package; no photo redistribution license is claimed.

Base color contains no baked lighting or AO. Roughness and tangent normals are separate. Portable glass is an opaque reflective approximation; original source shading remains editable. Bake samples are 2 x 2 m and all maps are natively 1024 x 1024. The geometry-only final revision reused shader-identical maps; shader and texture hashes are retained in evidence/shader_cache_validation.json.

## Approval boundary

Repository AGENTS.md requires generated drafts to enter staging and explicit approval before production promotion. This user-level instruction takes precedence over the skill's default /Game/Mesh destination. This package and the verified Unreal import are review drafts. No gameplay map or definition was changed. Intended post-approval destination: /Game/Mesh/hansa-bakery/.
