# Hansa bakery — review package

An editable Hanseatic bakery exterior inspired by Muehlenstrasse 1 in Stralsund, with a reconstructed shop frontage and rear bakehouse. This is a technically verified staging draft awaiting appearance review and production approval.

## Open the model

- [Packed editable Blender master](HansaBakery.blend)
- [GLB](HansaBakery.glb)
- [FBX](HansaBakery.fbx) — keep the HansaBakery.fbm folder beside it
- [Turntable](HansaBakery_turntable.mp4)
- [Blender preview](renders/portable_Hero.png)
- [Native Unreal preview](renders/unreal_Hero.png)
- [Native material close-up](renders/unreal_Roof.png)

The master opens with Portable_Export visible. Original editable pieces and procedural shaders are retained in Structure, Masonry, Roof, Openings, Timber, Hardware and Bakery collections, hidden for the portable-material comparison. To edit the original, hide Portable_Export and reveal those source collections, including their object render visibility.

## Component inventory

- Hollow main structural shell and floor slabs; foundation and corner quoins.
- Brick gable, concave coping, projecting piers, metal-tipped pinnacles and paired lancets.
- Individually modelled overlapping clay roof tiles and solid ridge caps.
- Stone reveals, sills, glazed panes, mullions, door planks, shop shutters and supported canopies.
- Forged hinges, straps, door pull, rivets, projecting bread sign and chains.
- Rear bakehouse, brick chimney with open flue, oven opening, hearth, firewood and peel.
- Bread displays, carved bread emblem and flour sacks.
- Separate neutral lighting, cameras and review ground; not part of exported building geometry.

## Unreal locations

Project: C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject, Unreal 5.8.

Final staging mesh: /Game/Hansa/Generated/Staging/HansaBakery_20260906_01/Meshes/SM_HansaBakery_R2

Materials and textures: /Game/Hansa/Generated/Staging/HansaBakery_20260906_01/Materials and /Textures.

Preview level: /Game/Hansa/Developer/GenerationPreview/HansaBakery_20260906_01/L_BakeryPreview.

The earlier SM_HansaBakery is a superseded staging comparison. No gameplay map or game definition was changed. Intended post-approval destination is /Game/Mesh/hansa-bakery/. Do not promote until explicit approval, per AGENTS.md.

## Evidence

- [Evaluation and limits](EVALUATION.md)
- [Provenance](PROVENANCE.md)
- [Reference manifest](reference_manifest.csv)
- [Inspection and correction history](ITERATIONS.md)
- [Material gap ledger](MATERIAL_ASSESSMENT.md)
- [Native-size side-by-side comparisons](COMPARISONS.html)
- [Material inventory](evidence/material_inventory.json)
- [Measured texel density](evidence/measured_density.json)
- [Authoring prompt and generation mode](PROMPTS.md)
- [File hashes](MANIFEST.sha256.json)

Detailed intermediate checkpoints, process logs and native original photograph remain in Saved/GenerationJobs/hansa-bakery_20260906_01/. The scripts in this package retain that original job-relative structure and are execution evidence. The final model files and their packed/sidecar textures are independently usable.
