# Firewood review candidate

Status: implementation and verification in progress; generated assets and economic catalog are staged, not accepted production content.

## Concrete generated assets

- Editable packed Blender master: `SourceArt/Generated/Buildings/HansaWoodcutterYard_20260916/delivery/woodcutter-yard.blend`.
- Verified FBX / GLB: the same folder, `SM_WoodcutterYard.fbx` and `SM_WoodcutterYard.glb`.
- Native Unreal render: `SourceArt/Generated/Buildings/HansaWoodcutterYard_20260916/delivery/unreal-preview.png`.
- Staged mesh: `/Game/Hansa/Generated/Staging/FirewoodModel/Meshes/SM_WoodcutterYard`.
- Saved review scene: `/Game/Hansa/Generated/Staging/FirewoodModel/L_FirewoodReview`.
- Six explicitly assigned materials, eighteen texture maps; 8.447 × 5.900 × 4.977 m, 60,432 triangles. Includes shelter, timber input, split-log stacks, racks, bench, chopping block and axe. No additional functioning workshop or kiln is implied.
- Model research, four rendered revisions, clean FBX/GLB reimports, twelve turntable frames and material limitations are recorded beside the source in `README.md`, `PROVENANCE.md`, `ITERATIONS.md`, `EVALUATION.md`, and `reference_manifest.csv`.
- Two independent built-in ImageGen GUI masters are under `SourceArt/UI/Firewood/`, native 1254 × 1254 RGBA, each with its exact prompt record. Thirteen proportional density variants per icon are in `review-variants/`; the actual-size inspection and variant hash manifest are alongside them.

## Proposed promotion after review

1. Copy the staged yard mesh, six materials and eighteen textures into the new `/Game/Mesh/hansa-woodcutter-yard/` folder, fixing internal references to those production copies. Keep the preview scene and generation provenance outside Shipping.
2. Bind `Building.WoodcutterYard` to that mesh through the accepted definition workflow. Its current economy-test draft deliberately retains an Engine primitive, which is **not** finished presentation.
3. Promote the two reviewed GUI icon sets to `Content/Hansa/UI/Icons/Firewood--<density>.png` and `WoodcutterYard--<density>.png`; keep generated masters in SourceArt.
4. Promote the reviewed economic change, update canonical seed generation, generate the next accepted catalog manifest and hash pin, and verify reload and player-facing placement/inspection with the actual model.

This approval is for the listed model, materials, texture/icon variants and associated firewood definitions only. It does not approve unrelated dirty assets or change existing accepted presentation elsewhere.

## Economic diff

`FirewoodCandidateDiff.json` compares the actual staged assets against accepted catalog v24, hash `C1BDF313543BF44A`.

- Adds `Good.Firewood`, `Recipe.SplitFirewood`, `Building.WoodcutterYard`, `Need.Heating`.
- Changes the three heat-consuming recipes, both population tiers and all four existing market profiles so every already-supported market recognizes the new good. This does not add a city or expand the rendered-city scope.
- Ninety existing definitions retain their exact content hashes; none are removed.
- Draft registry hash: `8BB8ACD607E70FDB`. Model binding will change it; this is not an accepted-production pin.
- Development-only `-FirewoodCandidate` loads this staged catalog. The flag is excluded from Shipping and cannot silently replace the default accepted catalog.

## Approval boundary

`firewood.md` explicitly says generated content “requires the existing explicit promotion approval” and “do not interpret this prompt as approval of unseen generated assets.” Automatic approval review rejected the attempted direct production import for this reason. The safe staging import succeeded. No generated asset or catalog has been promoted to accepted production content.

See `FirewoodImplementation.md` for runtime behavior, verification results, measured balance and remaining gates. Approval should be requested against this packet once independent verification is complete.
