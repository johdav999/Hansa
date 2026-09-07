# Hansa Bakery — ImageGen material revision

Updated the existing bakery's brick, roof clay, lime plaster/damp-lime and oak using four built-in ImageGen surface-color masters across nine material variants. Structural geometry is unchanged.

- [Packed editable Blender master](HansaBakery.blend)
- [GLB](HansaBakery.glb) and [FBX](HansaBakery.fbx); keep HansaBakery.fbm beside the FBX.
- [Turntable](HansaBakery_turntable.mp4)
- [Blender preview](renders/final_Hero.png) and [native Unreal preview](renders/unreal_Hero.png)
- [Native-size comparisons](COMPARISONS.html)
- [Evaluation](EVALUATION.md), [material assessment](MATERIAL_ASSESSMENT.md), [iteration log](ITERATIONS.md), [provenance](PROVENANCE.md)
- [Material inventory](evidence/material_inventory.json), [prompt set](PROMPTS.md), [reference manifest](reference_manifest.csv)

In Blender, Portable_Export is the delivered mesh. HYBRID_ImageGen_* materials preserve editable generated-image/tint graphs and separate physical channels. Original authored pieces/procedural shaders remain in hidden collections for historical editing; those original materials are not the revised portable appearance. Reveal the original collections and object render visibility deliberately when editing geometry.

Unreal project: C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject

Mesh: /Game/Hansa/Generated/Staging/HansaBakery_ImageGen_20260906_02/Meshes/SM_HansaBakery_ImageGen

Preview: /Game/Hansa/Developer/GenerationPreview/HansaBakery_ImageGen_20260906_02/L_BakeryPreview

Nine revised Unreal materials/color textures live under this revision's staging folder. Unchanged materials and physical-map dependencies retain references to the previous staging revision, which is preserved. No gameplay level, game definition or production asset was changed. Production promotion still requires explicit approval under repository AGENTS.md.

The original delivered package remains at ../HansaBakery_20260906_01/. Selected final exports were moved from Saved into this persistent folder to avoid duplicate disk use. Job scripts retain their execution-time Saved paths as evidence; final packaged model files are usable independently.
