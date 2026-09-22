# Local-only development assets

## Current scope update — whole generated-source archive

The user subsequently requested exclusion of **all `SourceArt/Generated/`**.
This supersedes the selective retention rules below for that directory: its
masters, exports, textures, generation scripts, provenance and evidence are now
local-only. Production `Content`, main `Source`, main `Scripts`, `Tests` and
other `SourceArt` directories remain tracked. Files on disk are preserved.

Fresh clones will not contain generated source masters or their embedded tools.
Reimporting or regenerating those assets requires restoring this directory from
a separate archive. Make an off-machine backup before deleting any local copy.
The recovery branch for this additional change is
`backup/before-generated-source-exclusion-9890969f`; do not push it.

The historical description below records the earlier, narrower cleanup.

On 2026-09-22, the user requested a smaller Git LFS upload. The cleanup keeps
production `Content`, Python/build scripts, final editable masters, final export
files, authored textures and test golden images in Git. It is not a blanket
exclusion of Blender, PNG, FBX or GLB files.

The following selected files remain on the original workstation but are excluded
from Git. Matching historical versions are removed from the three unpublished
commits only; published GitHub ancestry is preserved:

- `.codex-build/`: generated verification project copies.
- The PreservedFish delivery ZIP: regenerate from the delivery workflow.
- The downloaded HansaWorld ETOPO TIFF: retrieve the exact URL and verify SHA256
  from `SourceArt/Terrain/HansaWorld/Prototype_20260918/sources/sources.json`.
- Intermediate Blender checkpoints, except the fish-preservation R4 editable
  master explicitly identified by its README.
- Rabbit r03 Blender/FBX revisions superseded by the r05 deliverables, the r04
  diagnostic scene and the r05 review scene. Keep the corrected rig source,
  r05 editable master, final animation FBXs, metadata and validation reports.
- Grain-farm v0-v4 and clean-reimport scenes; keep its final master.
- Hop-farm clean-reimport scenes; keep its final source and exports.
- Woodcutter review-r01 through review-r04 scenes; keep the delivery master.
- Tree-family/grove preview scenes; keep individual source/runtime masters.
- PNG screenshots under `Docs/Images/UI/CogSelection/legacy/`.

The exact rules are in `.gitignore`. Historical scripts and evidence documents
may still reference archived revisions. To reproduce those old runs, restore the
local archived files first. The final production content is not changed by this
Git-only cleanup. No fresh-clone regeneration claim is made for old review runs.

## Recovery and storage boundaries

The local backup branch `backup/before-asset-cleanup-f6be5940` preserves the
pre-cleanup trees. Do not push backup branches: they intentionally retain the
excluded history. The earlier `backup/before-large-file-repair-6d67f752` branch
also retains the original large ordinary blobs. No Git/LFS pruning was done.

Excluded working files are **not an off-machine backup**. Copy desired archives
to a separate backed-up disk or storage service before deleting local files or
pruning Git/LFS objects. Old documents' archive links can be unavailable in a
fresh clone; retain this note with the project.

Removing these paths from unpublished history reduces future uploads, but does
not automatically reclaim any objects already uploaded to GitHub or reset its
billing. Size comparisons must deduplicate object IDs; folder sizes overlap.
