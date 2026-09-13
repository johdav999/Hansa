# Enhanced MVP Grain Farm asset family — EMVP-P08

Implemented 2026-09-07. `Building.GrainFarm` revision 3 binds `/Script/Hansa.HansaGrainFarmPresentation` and `/Game/Mesh/hansa-grain-farm/Final/SM_HansaGrainFarm`. No visible Engine cube or staging asset is used by this presentation.

## Delivered family

Seven imported meshes: completed farmstead, construction foundation/scaffold, mature center/edge/corner 4 m field modules, 4 m bare furrows, and a compact sheaf/sack/rake bundle. Nine native components compose the runtime family at identity scale. The complete assembly is 1432.5 × 1523 × 766 cm and fits the 1560 cm inset of its 4×4 footprint. The gate faces +X; ground error is within 2 cm.

All seven meshes have three conventional LODs (55% and 25% reduction targets; thresholds 1/.32/.12), not Nanite. The completed mesh has four convex hulls; auxiliary assets have no collision. Runtime art components disable collision and navigation, leaving existing placement/selection ownership unchanged. Building LOD0 is 12,112 triangles; field center/edge/corner/furrow counts are 12,800/9,040/6,960/1,200. Field visual bounds stay within 400×400 cm.

Construction displays scaffolding and furrows. Ready displays the building, mature crops and work props. Blocked/idle hides work props but preserves mature crops: pausing production does not change the season. State derives from the existing simulation projection and reconstructs after load; there is no new gameplay identity or save format.

## Evidence and materials

The northern hall-house form is a conservative game interpretation, not a reconstruction of a named 1400 building. Official regional museum, heritage and archaeological sources are recorded in the source package. Surviving museum examples are mostly sixteenth century or later; exact superstructure, dimensions and details are explicitly inferred. The Bakery and grain-field P07 anchors supply reused synthetic oak, lime, masonry, soil and straw color masters.

HansaModels inspection corrected folded roof slopes, embedded windows, missing gable infill, horizontal frame posts, buried props, oversize grain cards, incorrect FBX unit conversion and lost generated-coordinate materials. Final source is `SourceArt/Generated/Buildings/HansaGrainFarm_20260907_01/exports/HansaGrainFarm_final.blend`, with seven FBX/GLB exports and native 900×700 review renders/turntable.

The new thatch master is native 1254×1254, built-in ImageGen generate mode; its exact prompt is saved beside it. No raster was resampled. Explicit physical-coverage UV0 carries color and independent synthetic normal/roughness maps into portable exports. Six image-backed Unreal materials connect all three channels; normal maps invert green for Unreal, data maps are linear, and color is sRGB. Iron/linen retain simple physical constants. Physical maps are authored procedural approximations, not measured scans or luminance-as-height claims.

## Verification

- Development editor build succeeded using installed MSVC 14.44.
- Four automation tests passed: `Hansa.World.GrainFarm.PresentationRoles`, `Hansa.UI.World.DataDrivenPresentation`, `Hansa.Simulation.Placement.CanonicalRestoreOrder`, `Hansa.Integration.Save.RoundTripContinuation`. Report: `Saved/Automation/P08`; log: `Saved/Logs/P08Tests.log`.
- `Scripts/ValidateEnhancedMvpGrainFarmAssets.ps1` passes for all seven promoted assets. The visible-content manifest points at the same final root.
- Clean FBX and GLB reimports agree: 114 source mesh objects, 12,112 triangles, five building materials, UV0 throughout, near-zero ground error. Source conservative bounds differ from tight Unreal geometry bounds by less than 3.1 cm on Y.
- The isolated preview `/Game/Hansa/Generated/Staging/GrainFarm_P08/L_GrainFarm_Preview` was saved and reopened; `evidence/unreal_reopen.json` records measured assembly bounds. It is excluded from cooking by the existing staging exclusion. Runtime references point only at canonical content.
- The final Blender master reopens with all 22 file-image dependencies packed. Seven superseded root-level mesh imports were deleted only after reference checks returned empty; final assets and editable exports remain intact. `evidence/intermediate_cleanup.json` records the exact targets.
- Native Unreal working/idle/construction viewport captures are retained as `evidence/unreal_final_*.png` at 2253×772. They are inspection evidence, not shipping images. The field-facing view confirms the combined silhouette, tiled ground and state changes; the scaffold replaces the completed building during construction. Blender seam review is `renders/final_field_joins.png` at native 900×700. These inspection lights are not a claim of P30 final gameplay lighting acceptance.

## Limitations

No crop wind animation or hero-close botanical detail is claimed. Whole-project Shipping audit and P30 co-located gameplay-lighting acceptance remain downstream. The existing DebugGame editor must be rebuilt/restarted to load the new native class; this work compiled and tested a separate Development editor without changing the user's open terrain scene. Existing project startup GameFeatureData configuration errors remain outside P08; they did not fail the four selected automation tests.
