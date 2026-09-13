# Enhanced MVP world-scale and style-anchor contract

Status: **EMVP-P07 implemented, 2026-09-07**

Machine-readable contract: `Tests/Golden/enhanced_mvp_world_style_anchors_v1.json`

Audit package: `SourceArt/StyleAnchors/EnhancedMVP_20260907_01/`

## Decision

P07 approves a minimum set of **partial style anchors**, not a complete production asset. None of the five audited families currently passes every scale, source, PBR, LOD/Nanite, collision, canonical-path, and gameplay-camera gate. Four candidates require revision and the current harbor placeholder is rejected.

Later HansaModels work must reuse only the scopes named below. It must not copy a candidate's known defects, invent a second art direction, or compensate for bad authored dimensions with an Unreal scale transform.

## Shared world contract

### Units, axes, grid, and pivots

- Blender source uses metric units with `1 Blender unit = 1 metre`; Unreal uses `100 cm = 1 metre`.
- Authored geometry imports and renders at `(1,1,1)`. Runtime auto-fit, mesh squeezing, and per-axis scale compensation are forbidden.
- Hansa's authoritative placement grid is `400 cm` per cell. The visible/collision fit inset is `40 cm` total, so the grounded body has `360 cm` per occupied cell after clearance.
- `+Z` is up and `+X` is the deterministic forward/entrance direction unless an asset-family addendum records a necessary exception.
- The placement pivot is horizontally centered on the authoritative footprint and rests at ground contact within `+/-2 cm`. Windmill rotor, crane, rigging, eaves, signs, and sails use separate functional pivots.
- Grounded simple collision must fit the inset. Visual eaves, signs, sails, and rigging may overhang only when they do not claim placement cells, block navigation, or create hidden click/collision volumes.

### Human and architectural scale

- Reference adult: `180 cm`.
- Clear door opening: `85-120 cm` wide and `200-230 cm` high.
- Ordinary floor-to-floor rhythm: `280-340 cm`.
- A merchant Dielenhaus work hall may target `450-650 cm` clear height for readable combined work/residential use. This is a gameplay authoring range, not a claimed universal historical measurement; the specific merchant-house task must replace it with asset-specific evidence.
- Door, floor, stair, cart, barrel, quay, and ship relationships must be checked together before promotion. A correct bounding box does not excuse implausible openings or floor rhythm.

### Footprints

| Family | Grid | Inset available to grounded body | Current result |
|---|---:|---:|---|
| Merchant-house proxy | 2x2 | 7.6 x 7.6 m | 5.76 x 7.29 m fits, but is the wrong social/building type |
| Bakery | 3x2 | 11.6 x 7.6 m | 10.01 x 19.49 m fails |
| Mill | 3x3 | 11.6 x 11.6 m | 8.00 x 8.20 m body fits; 13.38 m sails are permitted visual overhang |
| Road | 1x1 | 3.6 x 3.6 m | 8.00 x 4.92 m straight piece fails |
| Dock | 5x3 shore footprint | 19.6 x 11.6 m | No authored candidate; cube placeholder rejected |

An asset that fails must be rebuilt, modularized, assigned a corrected footprint through the schema/migration path, or given an explicit system contract such as a spline road. It must never be silently fitted at runtime.

### Materials and texel density

- Baseline environment density is `512 px/m`; gameplay minimum is `384 px/m`; intentionally close hero surfaces may use `768-1024 px/m`.
- Density variance within a single asset is limited to 25% unless a documented hero/detail allocation justifies it. Required scale or density variants are authored natively; raster resampling is forbidden.
- Every opaque material needs physically meaningful base color, normal, and roughness. Ambient occlusion, height, and metallic are optional where materially justified. Non-metals default to metallic 0.
- A color image from an unrelated surface may not be reused to fabricate normal, roughness, or displacement. Generated base color is accepted only with a coherent real PBR set.
- Textures must be packed in the `.blend` or referenced repository-relatively, then imported into the canonical Unreal family path. Missing external absolute paths fail the gate.
- Values must remain legible under both the current warm key and a neutral overcast check. Do not paint directional light or hard shadows into albedo.

### LOD, Nanite, collision, and performance

- A non-Nanite static building or road needs at least three LODs and gameplay-camera transition proof.
- Animated assemblies need at least three authored LODs for each relevant component. Nanite is not a substitute for validating animation, material cost, silhouette, or screen-size transitions.
- Static meshes above 100k triangles are explicit Nanite candidates, not automatically approved Nanite assets.
- Use simple role collision. Complex-as-simple is rejected. Doors and work access remain open; rotor/sails do not drive the building's grounded collision.
- Promotion captures must cover 25 m inspection, the 65 m default strategy camera, and 120 m overview. At each distance, footprint, entrance, function, and selected-state silhouette must remain readable without noisy unique detail.

### Weather, season, lighting, and camera

- Baseline season is neutral late summer, dry-to-damp: no snow or heavy ice. Later seasons require native material/asset variants and the same scale/footprint contract.
- Weathering is restrained working-city use: ground-contact damp, rain runoff, handled-edge wear, and functionally justified roof/oven soot. Uniform grime, random damage, fantasy exaggeration, and painted lighting are rejected.
- The audited Lübeck prototype uses a directional light at `(-50,-35,0)`, intensity `5.0`, linear color `(1.0,0.84,0.68)`, plus skylight intensity `0.6`. This is validation context, not final lighting approval.
- The source-verified gameplay camera uses a `6500 cm` spring arm, `-55 degrees` pitch, and default `35 degrees` yaw. P30 must repeat native-resolution captures after final lighting and all anchors are co-located.

## Selected anchors and dispositions

### Merchant house: Laborer Residence proxy — requires revision

Current reference: `/Game/Mesh/LaborerResidence/Materials_R02/Meshes/SM_LaborerResidence`

Use only its believable human scale, grounded pivot, readable timber/plaster contrast, and broad city-camera value grouping. It is visibly a modest laborer residence, not evidence for the form or status of a Lübeck merchant Dielenhaus. No editable source or provenance was found; the mesh has 141,260 triangles, one LOD, no Nanite, ten material slots, and no accepted collision proof. P14 must create or evidence the actual merchant-house family.

### Bakery — requires revision

Current reference: `/Game/Mesh/hansa-bakery/Meshes/SM_HansaBakery`

Source: `SourceArt/Generated/Buildings/HansaBakery_ImageGen_20260906_02/HansaBakery.blend`

Use its material family, functional soot placement, restrained wear, and recognisable bakery massing as references. Headless Blender reported 10.0067 x 19.4850 x 16.6450 m, 863,720 evaluated source triangles, 17 materials, 55 packed images, UV0, and no missing external images. Unreal reported 42,952 triangles, one LOD, Nanite enabled, and 17 slots. The depth is 11.885 m over the 3x2 inset and the Unreal thumbnail showed gray checkerboard/fallback presentation. P10 must rebuild or remodule the form at authored scale, repair material presentation, and complete collision/camera proof.

### Mill — requires revision

Current reference: `/Game/Hansa/Core/Buildings/BP_HansaWindmill_Animated`

Source: `SourceArt/Generated/Buildings/HansaWindmillAnimated_20260906_04/exports/HansaWindmill_Animated.blend`

Use the functional silhouette, tower/material restraint, rotor motion pivot, and the explicit distinction between grounded body and sail overhang. Unreal reported an 8.00 x 8.195 x 13.043 m, 259,298-triangle body and a 13.377 m sail envelope; both have one LOD and Nanite disabled. Blender reported the complete 13.3799 x 8.7390 x 17.5249 m assembly, UV0 and packed sources, but 1,555 render objects retain non-unit source transforms. P09 must apply/normalize source transforms without breaking animation, add LOD/collision/performance proof, and retain identity-scale presentation.

### Road — requires revision

Current candidate: `/Game/Hansa/Generated/Staging/HansaDirtRoad_20260906_01/Meshes/SM_DirtRoad_Straight_8m`

Source: `SourceArt/Generated/Roads/HansaDirtRoad_20260906_01/exports/Hansa_DirtRoad_Kit.blend`

Use its compacted-earth material, crown/rut profile, and connector language. Do not promote the candidate: its 8.0 x 4.922 m straight piece does not fit one 4 m grid cell, it has one LOD, and its path is staging-only. Lübeck's later urban streets also need evidence-based irregular granite/fieldstone paving rather than treating a dirt kit as universal. P18 must deliver native 4 m neighbor-selected straight, corner, T, cross, and end pieces, or explicitly migrate the road system to a non-grid spline contract.

### Harbor — rejected

Current reference: `/Engine/BasicShapes/Cube.Cube`

The current dock plus world-foundation quay/piers are useful topology placeholders only. They establish no approved geometry, material, weathering, collision, or historical language. P17 must create the first harbor anchor from Lübeck/Trave evidence and validate it against merchant-house, road, cargo, and cog scale.

## Historical basis and art-direction guardrail

The existing Hansa direction remains evidence-led, restrained, legible, and grounded in late-medieval Lübeck. UNESCO records the surviving medieval plan and important fifteenth/sixteenth-century patrician residences. Lübeck's preservation material describes rows of gabled houses and the social distinction between western merchant residences and eastern craftspeople; the European Hansemuseum's Dielenhaus work documents a research-based combined merchant work/residence plan. Lübeck sources also document historic granite/fieldstone paving and the Trave salt-store/harbor relationship. These sources constrain later research; they are not permission to invent a generalized fantasy-Hanse style.

URLs, access date, and exact decisions are preserved in the audit package's `reference_manifest.csv`.

## Verification performed

- Headless Blender 3.5.1 opened Bakery, Mill, and road sources and emitted deterministic JSON for units, evaluated bounds/triangles, UV0, material lists, packed/external images, and non-unit transforms.
- Live Unreal inspection measured the selected Static Meshes and recorded bounds, triangles, LOD count, Nanite, and material-slot observations.
- `/Game/Hansa/World/Cities/Lubeck/L_Lubeck_MVP` was loaded and verified as the current level. Editor and PIE captures were inspected under its actual prototype lighting; the PIE capture included the real HUD/scenario modal.
- The four non-rejected candidates were also placed together transiently at identity scale, captured from `(-10000,-10000,10000)` at the contract's `-45/45` overview angle, and removed without saving. The view made the Bakery's excessive scale and the 8 m road mismatch unmistakable while preserving the Mill's functional distance silhouette. Zero audit actors remained afterward.
- The level proves the Laborer Residence proxy's city-camera legibility and exposes the placeholder character of the harbor. No final harbor art exists to co-locate, so the harbor remains rejected rather than converted into a false approval.
- `Hansa.Content.WorldStyleAnchors.Contract` validates the manifest, grid parity, identity-scale rule, family coverage, and canonical targets. Existing world-presentation tests verify Bakery and Mill authored presentations and placement/road ghosts at identity scale.

## Promotion checklist for later HansaModels tasks

1. Re-read this contract, the JSON manifest, the source family prompt records, and the candidate's evidence file.
2. State whether the grounded body fits its authoritative inset at scale 1. If not, revise geometry or the authoritative schema; do not scale it in Unreal.
3. Render required Blender review views at source resolution, then inspect in Unreal at 25/65/120 m under warm and neutral lighting.
4. Prove pivot, doors/floors, texel density, PBR inputs, material slots, LOD/Nanite, simple collision, weathering, season variants, and canonical paths.
5. Update the machine-readable contract/evidence with exact measured values and show the real asset diff before explicit promotion.

## Remaining limitations

- No checked-in screenshot was created from the live Unreal capture; its dimensions, camera, lighting, observations, and transcript retention are recorded in `evidence/unreal-lubeck-inspection.json`.
- Final environment lighting, co-located production anchors, native-resolution gameplay captures, and Shipping reference audit remain downstream P17/P18/P30 acceptance work.
- P07 intentionally generated and imported no new raster or 3D asset, so there are no new prompt records or production-ready imported outputs to list.
