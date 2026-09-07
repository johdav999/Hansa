# Inspected iterations

All building comparison renders use fixed cameras, Filmic exposure and neutral daylight unless explicitly noted. Images are linked at native pixel dimensions.

| Cycle | Inspected defect | Implemented correction | Evidence and result |
|---|---|---|---|
| r0 to r1 | Roof was a solid sheet; front roof edge crossed upper openings; custom mesh winding inconsistent | Individual overlapping tiles, roof set back behind gable, recalculate custom winding | renders/r0_hero.png vs r1_hero.png: sheet and facade intersections corrected |
| r1 to r2 | Flat shop canopies, weak roof supports, overly coarse plaster and bright metal | Added canopy tile detail, corbels, sill drip courses; restrained plaster relief and metal response | renders/r1_hero.png vs r2_hero.png: added construction depth verified |
| r2 to r3 | Corners and doorway lacked close-range joint/hardware detail | Quoins, local plaster repairs and rivets | r3_Detail_Shop.png: details visible; exposed faceted bread and insufficient timber grain remained |
| r3 to r4/r5 | Rear windows faced inward; rear masonry/chimney and annex roof too plain; bread/sacks faceted | Reflected rear shutter assemblies, actual brick courses, annex tiles, organic smoothing, grain alignment, reduced plaster mottling, visible bread emblem and scoring | r3_Rear.png vs turntable_049.png; r3_Detail_Shop.png vs r5_Detail_Shop.png: construction and prop corrections visible |
| r5 to r6 | Engine roof close-up revealed ridge hoops; chimney cap obstructed flue | Closed, overlapping half-cylinder ridge shells and recessed open chimney throat | r6_Detail_Roof.png, reimport_glb_Detail_Roof.png, unreal_Roof.png and turntable_049.png: continuous ridge and open flue verified |
| Export UV correction | Measured median density was zero on several joined material families | Unified UV0_MetreTiling name, triangulation before per-face projection, fresh export | evidence/measured_density.json: no material has zero minimum density; median approximately 480-512 px/m |
| Unreal lighting | Native capture displayed cached-exposure warning and overly bright surfaces | Preview-local 12000-lux daylight, fixed EV100 11.5, neutral ground; save and reopen | unreal_Hero_before_exposure.png vs unreal_Hero.png: warning removed; no project-wide rendering configuration edited |

Final GLB and FBX were each reimported in clean Blender scenes after the UV and ridge changes. The final Blender master was reopened and all 51 consumed images were packed. The 96-frame turntable was regenerated from the final master. Native Unreal final mesh is the R2 staging revision; the earlier mesh remains only as a superseded staging comparison.

## Final Unreal surface adaptation

Baked normal response was too strong under direct engine daylight. Nine masonry/clay/lime material families now blend 25% sampled tangent normal with 75% flat tangent normal. This is an engine-specific material adaptation; original maps and Blender source shaders are preserved. Before capture: renders/unreal_Hero_before_normal_fix.png. Final capture: renders/unreal_Hero.png. Some regularized brick and tile repetition remains visible; the result is still an exterior review draft. The sun source angle is 6.9 degrees to match the soft source-review lighting, with 12000 lux and fixed EV100 11.5. All 17 slot assignments and 51 native texture sizes were read back again after saving/reopening; see evidence/final_unreal_verification.json.
