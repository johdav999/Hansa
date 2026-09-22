# Render–inspect–correct record

| Cycle | Inspected defect | Correction | Evidence / result |
|---|---|---|---|
| r0 → r1 | Roof needed physical tile overlap and closed edges | Rebuilt roof as separate overlapping closed clay tiles and ridge caps; adjusted smoothing | r0-hero / r1-hero: overlap and silhouette improved |
| r1 → r2 | Cloudy oversized color mottles and indistinct vertical tile seams; plain gable | Regenerated continuous clay face, opened 8 mm tile joints, varied UV offsets; added gable courses and loft hatch/hoist beam | r2-hero: clearer individual tiles and gable depth |
| r2 → r3 | Workyard/function cues incomplete; sacks too simple | Folded sacks and ties, independent weave shader, pulley/rope, kiln fire door, sorting table/tub/shovel | r3 hero, courtyard, rear and detail renders inspected; courtyard exposed backing problem |
| r3 → r4 | Kiln backing exposed and service doorway incomplete; some closed pieces had inward winding | Moved backing, completed doorway, recalculated outward faces | r4 courtyard/rear/detail: masonry coverage and door restored |
| Export correction | A prop crossed ground; export packing and portable shader issues | Raised the affected prop; rebuilt portable materials with freshly decoded maps; widened jambs; rebaked native 1024 maps for mip support | Final clean FBX and GLB render/audit passed |
| Unreal correction | Optional capture parameters required explicit values; normal sizes transiently zero while compiling | Supplied capture pose; waited for texture compilation and verified all 21 dimensions | Saved/reopened native viewport captures |
| Unreal geometry correction | Nanite conversion visibly stripped bricks/tiles and distorted small pieces | Disabled Nanite, saved mesh, recaptured original geometry | Final courtyard capture restores components; optimization remains open |

Rejected trials stay in Saved/GenerationJobs, not shipping mesh folders. Earlier renders copied into Evidence are deliberate labeled comparisons. Source shaders are preserved in the packed editable master.
