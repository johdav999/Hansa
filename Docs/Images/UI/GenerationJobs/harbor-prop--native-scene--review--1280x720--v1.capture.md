# HarborProp native review capture

- File: harbor-prop--native-scene--review--1280x720--v1.png
- Native dimensions: 1280 × 720, 16:9; no resizing or resampling.
- Mode: UE 5.8 FPreviewScene + SceneCapture2D rendering; not ImageGen.
- Source geometry: Tests/Golden/Media/harbor-prop.glb, original mathematical cube fixture.
- Purpose: S13-P02 deterministic preview implementation evidence; non-shipping reference, not a generated production prop.
- Components: static mesh, native floor, axis lines, fixed key/fill lights, manual exposure and fixed perspective camera; shown through existing native asset/texture editors.
- Scene: key (-40,120), fill (-25,-30), FOV 45°, camera direction (1.6,-2.4,1.5) at six bounding-sphere radii.
- Inspection: original pixels inspected. All visible faces readable, silhouette within frame, floor and grounding visible. Fine renderer dithering remains; deterministic scene settings do not promise bit-identical output across GPUs/drivers.
- Revisions: corrected initial dark side lighting; increased camera distance to retain clear margins.
- Prompt set: none for this renderer-produced evidence. The unused live request template is Tools/HansaGenerationWorker/examples/tripo-harbor-prop.json.
