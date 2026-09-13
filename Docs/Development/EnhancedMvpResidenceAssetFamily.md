# EMVP-P14 residence family

Status: R06 assets **promoted and bound following user approval on 2026-09-08**.
See [promotion and verification report](EnhancedMvpApprovedPromotionP12P15.md).
Playable and Shipping acceptance remain open. The original staged handoff below is
historical; its pending-approval and unchanged-binding statements are superseded.

The review package is [HansaResidences_P14_20260908](../../SourceArt/Generated/Buildings/HansaResidences_P14_20260908/README.md). It contains four original meshes, editable packed Blender source, per-variant FBX/GLB, native textures, exact generation prompts, provenance, revision history, renders and machine-readable checks.

## Presentation contract

`Presentation.Building.Residence.Laborer` and `Presentation.Building.Residence.Artisan` retain their existing stable IDs. Their proposed presentation class is `AHansaResidencePresentation`, with two mesh references per tier. A deterministic unsigned hash of the authoritative parcel anchor chooses A or B. Rotation, refresh and tier replacement preserve the variant index. The existing world projection and placement ghost apply that anchor after creating the presentation child. No simulation capacity, footprint, upgrade rule, save schema or provider dependency changes.

Both tiers use the same 2x2 / 400cm-cell parcel and fit its 760cm inset at scale one. +X is the entrance/street side, +Z up, ground origin within 2cm. Artisan adds one actual storey rather than scaling the Laborer mesh. A/B vary roof pitch and chimney location only. Seven shared materials, three conventional LODs and generated simple collision per mesh; runtime cosmetic components have collision/navigation disabled because the existing projection owns those authorities. No per-frame actor work.

Staged Blueprint defaults bind four R06 meshes under `/Game/Hansa/Generated/Staging/Residences_P14/Meshes_R06`. The earlier R05 folder is a deliberate comparison only. Neither tier definition is changed: Laborer still uses its existing R02 mesh; Artisan still uses its existing placeholder. Native class defaults do not load staged assets. The production loader's staging rejection remains intact.

## Capacity discrepancy

P14 requests increased prosperity/capacity. The current model has **12 Laborers versus 8 Artisans**. This visual implementation preserves those gameplay values and communicates a larger, more prosperous upper-tier house through a second storey. Increased resident count is not implemented or claimed; a balance change requires an explicit decision and the associated definition/editor/validation evidence.

## Verification and promotion gate

Run `Scripts/ValidateEnhancedMvpResidenceAssets.ps1` for archived source/import checks. `Hansa.World.Residence.ParcelVariants` is CI-safe and verifies deterministic selection, refresh, missing-asset clearing and cosmetic authority. `Hansa.World.Residence.StagedFamily` is an explicitly invoked, RequiresUser local test of the actual staged Blueprint defaults, imported mesh contracts, rotation and preserved definitions. It does not bypass production-loader validation.

Approval must precede canonical import and production definition binding. After approval: promote only R06 plus its seven materials/21 maps, set both tier presentation classes through the normal editor workflow, validate actual definition diffs and references, update the P02 visible-content evidence, then demonstrate placement, in-place upgrade with unchanged anchor/rotation and variant index, save/load, demolition, crowded-city gameplay cameras and Shipping exclusion/cook. These final gameplay and Shipping gates are still open. A staged screenshot or Blueprint spawn is not proof of a live upgrade.
