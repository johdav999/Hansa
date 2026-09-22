# Historical isolated delivery — feature now integrated into main

Open the normal `C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject` for the current implementation. See `Docs/Development/PreservedFishMainIntegration.md`. The archived candidate below remains an immutable record of the earlier separate delivery.

# Preserved fish — separate Hansa candidate

Open the existing verified project:

`C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/.codex-build/preservedfish-verify/Hansa.uproject`

This is deliberately separate from the concurrent firewood work. It has its own source, runtime binaries, catalog and preserved-fish mesh. Other existing presentation assets are read through workspace junctions. Do not overwrite the main Source or Content folders with this delivery.

## Files

- `preservedfish-candidate.zip`: complete candidate Source, Config and Tests; authoritative Core Data Assets; new Unreal mesh/materials/textures; icon variants; preview/baseline fixtures; feature docs; model source/export/provenance/render set; final evidence.
- `preservedfish-source.patch`: reviewable source diff against retained pre-feature/isolation snapshots. It is not an unattended patch for the current firewood source.
- `source-manifest.json`: baseline and candidate SHA-256 for every changed source file and schema/catalog versions.
- `archive-manifest.json`: SHA-256 of each archived file and archive hash.
- `verification.json`: final test, build and packaging evidence summary.

The archive is an overlay requiring the unchanged Hansa project presentation assets from its v24 baseline; it is not a standalone game download and contains no engine or compiled binaries. Restore into a separate copy of that baseline with an empty Source folder before extraction; never extract over an actively edited firewood checkout. The complete source snapshot is authoritative if inherited source files differ. The already-created candidate above is ready to open in Unreal 5.8 on this workspace.

## Build and review

Build HansaEditor Win64 Development with the candidate .uproject. For the isolated UI/model review, open `/Game/PreservedFishPreview/L_PreservedFish`; ordinary New Game and inspector controls are functional. The native mesh is `/Game/Mesh/hansa-fish-preservation/SM_HansaFisheryPreservation`, assigned to Building.Fishery.SaltingShed. The preview is excluded from Shipping.

The feature guide is `Docs/Development/PreservedFish.md`. Model evidence begins at `SourceArt/Generated/Buildings/HansaFishPreservation_20260916/README.md`. The packed master is `checkpoints/HansaFishPreservation-r4.blend`; exports, turntable, Unreal hero/close-up, evaluation, reference manifest, material inventory/gap ledger and provenance are beside it. Native GUI screenshots and semantic TSVs are in `Docs/Images/UI/PreservedFish/Verification/`.

Gameplay: upgrade the shoreline fishery, choose Salted catch, supply imported salt and cooperage barrels, and optionally enable fresh fallback. Household demand remains Fish. Both goods have equal edible value; preservation improves storage life. The city household policy is separate from route minimum stock. Foreign route loads buy and unloads sell at current market price.

Catalog v25 D73BFD73C23C2D03; save9/fingerprint22/command7. Old incompatible catalogs reject explicitly and remain untouched. The fixture demonstrates storage benefit and a profitable export strategy; it is not complete campaign profit accounting. A full default-city household/trade GUI campaign remains a release acceptance step. See the feature guide for the exact limitations and the early shared-source isolation boundary.
