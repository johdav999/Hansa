from pathlib import Path
import json,hashlib,shutil
P=Path(__file__).resolve().parents[1];D=P.parents[2]/'SourceArt/Generated/Buildings/HansaMill_20260906_01'
docs={
'README.md':'''# Hansa — weathered Baltic windmill

Created an editable, textured post windmill and verified its staging import in Hansa. The material direction is worn and dirty, with desaturated weathered timber, aged shingles, sheltered grime, stone discoloration, and restrained oxidized iron. This is an original game interpretation informed by the Vilidu mill at Angla, not a measured replica or a proven medieval reconstruction.

## Deliverables

- [Packed editable Blender master](exports/HansaMill.blend) — named structure, cladding, roof, sails, access, stonework, hardware and review collections; original ImageGen inputs and procedural shaders retained.
- [GLB](exports/HansaMill.glb) — portable materials and vertex-color weathering.
- [FBX](exports/HansaMill.fbx) — standard PBR map references and vertex colors; receiving shaders must multiply vertex color into base color. Unreal reconstruction is verified.
- [Turntable](exports/HansaMill_turntable.mp4) — 48 freshly rendered 800 × 800 frames, 12 fps, 4 seconds; complete silhouette.
- [Native Unreal preview](renders/unreal_Hero.png) and [rear view](renders/unreal_Rear.png).
- [Native comparison evidence](COMPARISONS.html), [evaluation](EVALUATION.md), [iteration log](ITERATIONS.md), [material assessment](MATERIAL_ASSESSMENT.md), [provenance](PROVENANCE.md), [reference manifest](reference_manifest.csv), [material inventory](material_inventory.json), [file hashes](manifest.json).

## Component inventory

| Component | Construction / state |
|---|---|
| Foundation | Individually modeled fieldstones and recessed infill |
| Post and trestle | Central timber post, crossbeams and quarter bars |
| Mill body | Individual vertical planks, corner posts and floor/girts |
| Roof | Separate overlapping split shingles, bargeboards and ridge cap |
| Sail assembly | Four open lattice sails, leading boards, stocks, shaft and bindings; parked with cloth removed |
| Access | Framed rear door and upper hatch, open shutters, stair treads, landing and rails |
| Tailpole | Tailpole and lower support beam |
| Hardware | Timber pegs, iron hinges, shaft band and stock bindings |

All components are actual mesh geometry. No GUI is part of this asset; GUI state matrices are inapplicable. Sails remain separately editable in the source master. The delivered combined mesh is static.

## ImageGen and materials

Built-in ImageGen generated three original color masters, each **1254 × 1254**, opaque square, nominal 1 × 1 metre surface coverage. No API/CLI fallback or external paid model was used. The originals were not resized. Each has its full prompt and inspection record beside it:

- [Oak prompt](textures/mill--oak--worn--1254x1254--v1.prompt.md)
- [Roof timber prompt](textures/mill--roof--worn--1254x1254--v1.prompt.md)
- [Limestone prompt](textures/mill--stone--worn--1254x1254--v1.prompt.md)

These images feed actual source shaders. Roughness and submillimetre/millimetre relief are authored separately from procedural structure; stains are not converted into bumps. Broad weathering uses geometry-bound vertex colors. Sheltered oak is a darker material variant. Small forged-iron fittings use an independent procedural material because another color image adds little useful information there.

Final portable maps are native **1024 × 1024 shader bakes** (base color, roughness and OpenGL tangent normal for each of five materials). Color was rebaked natively from the original shaders to support Unreal mipmapping, replacing aliased 1254-square delivery bakes. This was not a resize of the generated masters. Color maps use sRGB; normal and roughness use linear/data settings. Unreal normal green-channel adaptation was explicitly configured and checked in close-ups.

## Scale and technical verification

- Inferred mill body: 3.6 × 4.2 m; eaves 6.5 m; roof ridge 8.2 m.
- Complete mesh envelope, including sails and tailpole: approximately **9.44 × 10.93 × 11.01 m**.
- Pivot is nominal ground centre. The irregular lowest stone extends 5.33 cm below nominal zero; preview placement compensates by 5.34 cm.
- Blender source/export geometry: 171,940 triangles; Unreal imported mesh readback: 161,764 triangles, five assigned slots, one LOD. The importer has remove-degenerates enabled and changes the triangle count; visual silhouette/material checks passed, but this is not a topology-equivalence claim.
- Area-weighted median delivery density: approximately 1024 px/m for wood/iron and 987 px/m for stone. Measured important-surface minima range from roughly 782–1024 px/m. See [density measurements](exports/texel_density.json); tiny slivers below 0.0001 m² are excluded from that summary.
- Packed master reopened and all three generated images verified packed: [master verification](exports/master_verification.json).
- GLB and FBX each imported into a clean Blender scene and rendered after the final bake update: [GLB](exports/reimport_glb.json), [FBX](exports/reimport_fbx.json).
- Image files decoded and dimensions/checksums checked: [image integrity](image_integrity.json).
- Unreal packages saved; preview level reopened; five material assignments, all final map dimensions, unit conversion and handedness verified: [engine verification](unreal_final_verification.json).

## Hansa paths and approval status

Project: `C:/Users/Johan/source/repos/Unreal/Hansa/Hansa/Hansa.uproject`

Staging mesh: `/Game/Hansa/Generated/Staging/HansaMill_20260906_01/Meshes/SM_HansaMill_Weathered`

Staging materials/textures: `/Game/Hansa/Generated/Staging/HansaMill_20260906_01/Materials/` and `/Textures/`.

Saved preview level: `/Game/Hansa/Developer/GenerationPreview/HansaMill_20260906_01/L_MillPreview`

Requested workflow destination after approval: `/Game/Mesh/hansa-mill/`. Repository AGENTS.md requires explicit approval before production promotion, so this deliverable remains in staging for review. No gameplay map reference was added.

## Remaining limits

This is a visually reviewed static asset, not runtime/Shipping acceptance. Interior milling machinery, cloth sails and rotation animation are not authored. Collision, navigation, distance LODs, memory/performance budgets, and a Shipping cook have not been validated. Architecture and proportions are inferred from later references; the shingle roof and hardware are artistic reconstructions. Stone shapes are somewhat smoother and more regularly coursed than the photographic reference. Extreme macro shots reveal simplified end grain, nails and wear geometry. Unreal daylight and neutral Blender lighting differ; compare each material in the supplied close-ups rather than treating color differences as a calibration measurement.
''',
'ITERATIONS.md':'''# Inspected correction history

1. **r0 → r1, gable closure and broad condition.** Inspected `r0_Hero.png` and `r0_Rear.png` beside the native Angla reference. Rectangular plank ends created stepped roof gaps; stones were too regular/clean. Cut gable plank tops to the roof slope, add roof underboarding, vary stone positions and add geometry-bound variation/splash dirt. `r1_Hero.png` confirms the gap closure and less uniform stone coloration. Neutral cameras/exposure retained.
2. **r1 → r2, access and foundation.** The front opening lacked a believable frame and foundation blocks still looked machined. Added jambs/sills, stair railings, pegs; increased rock asymmetry and reduced bevel radii. `r2_Base_Detail.png` confirms visible stair structure and grain alignment, but the rocks still read as angular blocks. `r2_Rear.png` retained for comparison.
3. **r2 → r3, fieldstone correction and bindings.** Replaced distorted blocks with irregular fieldstone geometry, recessed infill behind joints, and added stock bindings and small plank-end checks. `r3_Hero.png` and `r3_Base_Detail.png` confirm the corrected silhouette, stones, wood and hardware. Roof/iron close-ups also inspected. This completes three actual render–inspect–correct cycles; intermediate failures are not counted.
4. **Export checks.** Packed the source, baked independent PBR channels, exported real FBX/GLB, and rendered clean reimports. GLB carries vertex-color modulation. FBX retains the data but requires receiving-shader multiplication; reconstructed explicitly in Unreal.
5. **Engine correction.** Initial native 1254-square color bakes showed heavy fine aliasing in `unreal_before_exposure.png`. Kept generated masters unchanged, rebaked native 1024 shader outputs, imported new sibling textures, and reconnected the actual material inputs. `unreal_Hero.png`, `unreal_Base.png` and `unreal_Roof.png` confirm substantially cleaner grain and roof sampling. Locked daylight exposure and used capture warm-up frames; no postprocessing of screenshot pixels.
6. **Vertex weathering import correction.** The standard Unreal mesh importer defaulted to Ignore for FBX vertex colors. A bounded job-local ToolsetDefinition imported a new staging mesh with Replace through MCP. All five materials were assigned and the isolated preview was switched to this mesh. Save/reopen and import-data readback confirmed Replace; fresh engine captures verify the result. The original staging import remains a superseded comparison. No production asset was overwritten.
7. **Turntable framing correction.** The initial rear turntable frame clipped a sail tip. Changed to a generous orthographic orbit, tested every world-bounds corner against every frame with a 3% safety margin, then rerendered all 48 native 800-square frames and replaced the encoded turntable. Cardinal frames retained for inspection.

Every source checkpoint and execution log remains in `Saved/GenerationJobs/hansa-mill_20260906_01/`. Selected evidence and final artifacts are preserved in the durable source-art package. Failed runs, temporary node-discovery work and superseded delivery textures are not counted as acceptance evidence.
''',
'MATERIAL_ASSESSMENT.md':'''# Material assessment and gap ledger

Reference: Rutake's native 1600 × 1276 rear-oblique photo of the Vilidu mill. It supports structure and broad timber/stone observations; it does not resolve all hardware, precise species, mineral type, roughness or measurements. The Hansa model is an original adaptation. [Native comparisons](COMPARISONS.html) retain different camera/frame sizes without resampling.

| Family / view | Reference observation | Render observation / gap | Correction | Verified evidence / limits |
|---|---|---|---|---|
| Timber cladding | Vertical weathered boards, strong real joints, uneven condition | r0 had stepped gable gaps and uniform condition | Slope-cut ends, underboarding, restrained per-piece and splash variation, pegs | r1/r3 Hero, r3 Base, clean GLB Base and final Unreal Timber/Iron: real joints and longitudinal grain retained; macro end grain remains simplified |
| Roof | Ribbed roof surface in photo; substrate uncertain | A literal copy would not establish period accuracy; shingles are an artistic adaptation | Actual individually overlapping shingles with thickness and fine generated split-wood color | r3 Roof, swatch RoofTimber, clean GLB Roof, Unreal Roof: overlaps/readable silhouette; some regularity remains |
| Stone | Rough irregular rubble support | r0/r2 looked like regular or distorted manufactured blocks | Fieldstone geometry with varied shapes, inset infill, material variation and localized lower dirt | r3 Base and final engine Base show irregular stones; still smoother/more coursed than reference |
| Sail wood | Lattice construction with different member sizes | Exposed lattice must read from game distance | Modeled stocks, longitudinal spars, cross laths, leading boards, pegs and bindings | r3 Hero/Roof, engine Iron, turntable cardinal views: continuous credible framing; cloth removed / parked state |
| Iron | Fine details not fully resolved in reference | Plain bands can look too smooth close up | Independent high-roughness oxidized iron shader with mild relief; actual strap geometry | r3 Iron and engine Iron: restrained dark fittings; corrosion microstructure remains an authored approximation |
| Source inputs | No reusable albedo in the research photo | Generated color inputs are synthetic, not measured PBR scans | Three separate ImageGen swatches; independent physical channels | Native generated outputs and repeated Oak/Limestone/Roof swatches inspected; no prominent seam or joint-grid conflicts at intended viewing scale |
| Unreal sampling | N/A, engine-specific | Fine no-mip aliasing on first 1254 bakes overwhelmed grain | New native 1024 shader bakes; intact 1254 ImageGen masters retained | Before/after engine frames: visibly improved, final grain not swamped by high-frequency aliasing |
| FBX transfer | N/A | Standard material translation omits the vertex-color multiplication | Explicitly rebuild Image × VertexColor in Unreal; prefer GLB for portable automatic appearance | Engine graph readbacks and assigned materials saved; raw FBX in other software needs this documented reconstruction |

Plaster, glass, terracotta and painted surfaces are inapplicable to this chosen timber mill. Neutral source and raking daylight renders were inspected; no photometric equivalence between Blender and Unreal is claimed.
''',
'PROVENANCE.md':'''# Provenance

## Authorship and method

Original Hansa mesh geometry and procedural shader/UV construction authored in task-local Python and executed in Blender 3.5.1. No external mesh provider was used. The model is a later-reference-informed original Baltic post windmill, with inferred dimensions and period adaptations.

Three material color images were generated with built-in ImageGen in this conversation. Model version/seed and monetary usage were not exposed by the tool; none are invented here. The user explicitly requested ImageGen textures. No paid API/CLI fallback, provider credential access, purchased asset or third-party photographic texture was used. Full exact prompts are preserved beside each 1254-square native original.

Derived maps were baked from editable shaders in Blender. Final native map dimensions are 1024 square, chosen for portable texture filtering. Generated originals retain their original bytes and dimensions; no raster resize was used. Roughness and normal detail originate from authored physical shader structure, not luminance conversion of stains. Vertex colors encode broad object/location condition. JPEG files are same-dimension inspection encodings of rendered PNGs; PNGs remain authoritative. Turntable frames were rendered directly at 800 square, not resized.

## Architectural reference

Rutake, 2014, *Angla Vilidu talu pukktuulik Saaremaal august 2014 2.jpg*, Wikimedia Commons, CC BY-SA 3.0 Estonia. Unmodified local research reference, original 1600 × 1276. Attribution/license/source/checksum are in [reference_manifest.csv](reference_manifest.csv). Its pixels are not in the model's shaders or maps. The photograph is not represented as a survey or proof of medieval appearance. Visit Estonia supplies later construction/restoration context; the Buckinghamshire heritage portal supplies generic historical post-mill context, not proof of Baltic details.

Generated source imagery is synthetic artwork, not a scan or a photograph of the actual reference building. Source hashes are in [manifest.json](manifest.json); channel sources and consumers are in [material_inventory.json](material_inventory.json).

## Approval and production boundary

The user authorized generation and selected a windmill. Repository instructions require explicit approval for promotion to production content. Assets are saved in the new staging root and a separate Developer preview level; no existing approved asset was replaced and no gameplay map was edited. Production destination, if approved, is `/Game/Mesh/hansa-mill/`. No runtime or Shipping validation is implied by the staging import.
''',
'EVALUATION.md':'''# Evaluation

## Result

The delivered mill is an editable, materially weathered static 3D asset, with three built-in ImageGen color inputs actually integrated into source shaders and portable material maps. Whole-building renders, material close-ups, repeated swatches, clean format reimports, and actual reopened Unreal captures were inspected. Three explicit geometry/material correction cycles plus engine filtering and turntable-framing corrections are documented in [ITERATIONS.md](ITERATIONS.md).

The model achieves the requested worn/dirty Baltic windmill direction as an original game asset. It is not a scan-quality replica or a fully researched medieval reconstruction. Final visual review and production promotion remain the user's decision.

## Verified

- Real separate planks, roof shingles, framing, sail lattice, stones, stairs and iron details; no full-screen/generated image used as a 3D substitute.
- All three original color inputs are packed in the reopened Blender master.
- Native ImageGen dimensions 1254 × 1254 retained; each has an exact prompt record.
- Five portable PBR material sets; final engine maps all read back at 1024 × 1024 with data-map color-space settings.
- UV density measured from actual mesh triangles and physical dimensions, not inferred from texture size alone.
- Actual GLB and FBX exported and reimported into clean Blender scenes with scale checks and rendered evidence after the final bake change.
- Unreal import saved at the intended Hansa project's staging root, five exact material slots verified, bounds verified in centimetres including Y-axis handedness conversion, and the saved preview level reloaded before final captures.
- Alias-prone initial engine color maps replaced with new native shader bakes; generated originals unchanged.
- Turntable: corrected framing, 48 frames at 800 square, 12 fps, 4 seconds; cardinal views inspected.

## Limitations and production gates

Reference coverage is one useful native rear-oblique photograph plus historical context. Front, hidden joints, precise dimensions and period roof/hardware details are inferred. The mill is a parked static model with cloth removed. Functional machinery, animated sails, and interiors are not supplied.

Stonework remains smoother/more regularly coursed than the real reference. Very close inspection reveals simplified timber end grain and hardware/corrosion detail. This was designed for approximately 8–25 m game viewing, with material review around 2 m; it is not a macro architectural visualization asset.

Unreal reports 161,764 triangles and one LOD, versus 171,940 source/export triangles. The importer's triangle-count change is recorded rather than claimed lossless. Silhouette, bounds and visible component/material checks passed; per-triangle identity was not validated. Collision, navigation, distance LODs, draw-call/texture-memory budgets and Shipping exclusion/cook are not certified. A staging/developer path is not proof of Shipping exclusion.

Raw FBX retains maps and vertex-color data but requires a receiving material to multiply vertex color into base color; the delivered Unreal material does this. GLB supplies automatic portable color multiplication. See the [material gap ledger](MATERIAL_ASSESSMENT.md) for remaining visual approximations.
'''
}
for name,content in docs.items():
 (D/name).write_text(content,encoding='utf8');(P/name).write_text(content,encoding='utf8')
# Update prompt records with completed inspection and final consumers, keeping verbatim prompts.
for f in (D/'textures').glob('*.prompt.md'):
 text=f.read_text();text += '\n\n## Final integration QA\n\nNative source inspected and unchanged. Connected to the packed Blender source shader; repeated swatch and model inspected. Final portable color is a native 1024-square shader bake for mipmapping, with independent 1024 roughness/normal and geometry weathering. Actual Unreal material slot and map dimensions verified. This is a synthetic source-art master; production promotion remains pending review.\n';f.write_text(text,encoding='utf8')
manifest=[]
for f in D.rglob('*'):
 if f.is_file() and f.name!='manifest.json':manifest.append({'path':str(f.relative_to(D)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(D/'manifest.json').write_text(json.dumps(manifest,indent=2));print('REPORTS_WRITTEN',len(manifest))
