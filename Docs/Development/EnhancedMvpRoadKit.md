# EMVP-P18 — production road kit

Completed and promoted with Johan's explicit approval, 2026-09-08. Acceptance is for the current MVP road-drawing terrain and gameplay contract, not arbitrary Landscape conformance or a full enhanced-MVP release.

## Delivered

Six native 4 m-cell pieces (isolated, end, straight, corner, T, cross), a shared three-channel earth material with dry/wet support and feathered shoulders, and a native presentation actor bound through `Building.Road`. Eleven packages live under `/Game/Mesh/hansa-dirt-road`; this preserves the established DirtRoad family name instead of the manifest's former proposed `hansa-road` root. Source, packed Blender master, clean FBX/GLB exports, prompt, original-size inspection views and material/provenance reports are retained in [the source archive](../../SourceArt/Generated/Roads/HansaRoad_P18_20260908/README.md).

HansaModels drove the original-source audit, research constraints, native reconstruction, independent hybrid PBR bakes, repeated render/inspect/correct passes, clean reimports and native Unreal verification. Existing ImageGen source was reused unchanged at 1254 x 1254; final three PBR maps are independent native 1024 x 1024 bakes, not resized artwork. No new live provider call was made. Visual references are non-shipping; the eleven imported packages are production assets.

## Runtime and authoring parity

`AHansaRoadPresentation` maps all 16 cardinal-neighbor masks to six meshes and quarter-turn yaw. Final projection rebuilds neighbor state from authoritative roads, including adjacent roads outside a stroke, after placement/removal/load. Live ghost pieces use the same topology and authored unit scale. Invalid water cells do not add phantom connections. Construction retains the road instead of showing an Engine cube. Serialized gameplay data, route logic, costs and placement validation are unchanged.

The native actor exposes six editable mesh fields with metadata and deterministic asset validation. Production references cannot point into staging. Review imports preserve provenance and require explicit promotion. Test-only reimport/map creation requires `-P18Authoring`; normal test runs are read-only with respect to those assets.

Only `Building.Road` advances to revision 2. Catalog v7 has 72 definitions and registry hash `22248A11101B32B0`; v6 was `483D86D8C5549199`. Road content hash is `64FB950BBE107411`. The seeder, golden catalog, scenario pins, compatibility lineage tests and visible-content manifest were updated together. Economics remain unchanged. [Final approval receipt](Evidence/P18ApprovedPromotion-20260908.json) pins the saved packages and post-promotion ground-material amendment.

## Geometry and terrain acceptance

Native 400 cm source cell, centered pivot, +/-200 cm maximum XY. The original P18 no-runtime-fitting restriction is superseded by the 2026-09-11 terrain implementation in [RoadTerrainSplines.md](RoadTerrainSplines.md). All 12 connector profiles agree at 0.00001 m tolerance. Each piece has one material and three conventional LODs; authored simple convex collision is retained but runtime road collision/overlap/navigation is disabled. Simulation remains authoritative; selection uses an invisible non-navigation proxy.

Actual native MVP land is 75 cm high, shore 85 cm. The material smoothly approaches each of the three shore boxes over 1 m, with 12 cm conservative displacement bounds. Saved/reopened overhead and low-angle views verify the old 25 cm floating offset is gone. No gate-specific MVP road rule exists. Arbitrary terrain gradients and a future native Landscape require their own conformance implementation and acceptance. Existing cosmetic world-foundation road strips/quay cubes are not this production kit and were preserved.

## Verification

- Latest DebugGame and Development Editor builds pass; Shipping runtime build and binary exclusion audit pass.
- Final targeted suite: 18 passed, 0 failed. Covers all 16 preview/final masks, ground contract, authoritative external neighbors, command/cancel/remove/save/rebuild, catalog reload, construction validation, manifest completeness, staging-path guards, and actual viewport drag. One drag test reports existing font fallback/render-thread warnings; no failures are suppressed.
- Separate native ground-detail capture passes at 1920 x 1080. Kit captures are native 1920 x 1080 and 1280 x 720. Every module and wet/dry row was inspected; source FBX/GLB were independently reopened and inspected. See archive `ITERATIONS.md` for accepted images and corrections.
- Production hard/soft reference audit and expanded Lubeck cooked-package audit pass. Audit now recognizes World Partition staging external actors/objects without exempting production external actors. Actual cooked packages are scanned; exact cooker-only metadata descriptors are recorded separately following Unreal's staging behavior. This is not a final IoStore/container or whole-game release audit.
- Original DirtRoad packed master hash remains `9F4569CE84B3A430CCF9DF0EB1A0F9F939519E6F7F6CB693B4D4A61F6931A781`.

Shipping verification also exposed an editor-only drag helper in runtime Slate code. It was replaced with the runtime drag-operation base while retaining Hansa styles and behavior; actual drag regression passes. Cook uses process-only authoring-plugin/brush exclusions and `-SkipZenStore` for auditable loose packages; no project packaging configuration was changed.

## Remaining limits outside P18

No new weather simulation, gate framework, terrain generator, city-wide historic paving replacement or vehicle navigation system. Wetness is a cosmetic input, not a new gameplay field. Low-angle dither grain should be reassessed if temporal AA or terrain material changes. Central historic Lübeck paving must not be inferred from the peripheral earth-road reference. P16/P17 and unrelated visible-content statuses are not promoted to complete by this task.


## Terrain conformance revision — 2026-09-11

The user authorized spline deformation and RVT shoulders. [Implementation and acceptance](RoadTerrainSplines.md) retain the approved source kit and authoritative road identity while replacing rigid terrain placement. Original P18 approval evidence above remains historical; it is not reissued for the new material variant.
