# Final authoring prompt and build specification

User request: "use the HansaModel skill and create a realistic 3d model of a Hansa Bakery"

Generation mode: deterministic headless Blender Python. ImageGen mode: not used. Provider model: none. Blender version: 3.5.1. Seeds: 1701 for base construction, 78 for later masonry variation.

Final specification: create an editable, full exterior Hanseatic bakery inspired by the documented brick-gabled Muehlenstrasse 1 in Stralsund. Retain the tall paired gable openings, concave wings, projecting piers, pale lower plaster, muted brick and clay palette, and dark timber. Model structural depth, joints, overlapping clay tiles and solid ridge caps. Add an explicitly inferred early-modern bakery frontage, bread displays, a carved bread sign, rear bakehouse, oven opening, chimney and handling props. Use real metres, front -Y and up +Z. Preserve source collections, physically scaled procedural materials and portable maps. Avoid fantasy ornament, modern branding, copied game assets, baked dynamic text, photographic texture reuse, and provider dependencies.

Native image outputs: material maps 1024 x 1024; Blender review renders 1200 x 1200; turntable 960 x 960, 96 frames at 24 fps; Unreal viewport captures retain their native 2253 x 910 size. No raster resize was used to create delivery variants.

Final scripts: build_bakery.py with BAKERY_REV=3, refine_r4.py, refine_r5.py, refine_r6.py, then export_r6.py. The initial bake is bake_export.py against r5; export_r6.py consumes its shader-validated map cache. scripts were retained as executed evidence, with original job-relative paths. They are not an automated production importer.
