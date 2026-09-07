from pathlib import Path
import json,hashlib,shutil
from PIL import Image
P=Path(__file__).resolve().parents[1];repo=P.parents[2];dest=repo/'SourceArt/Generated/Buildings/HansaWindmillAnimated_20260906_04'
prior=json.loads((P/'previous_master_hash.json').read_text(encoding='utf-8-sig'));assert hashlib.sha256(Path(prior['Path']).read_bytes()).hexdigest().upper()==prior['Hash']
rec=json.loads((P/'unreal_animation.json').read_text());saved=json.loads((P/'unreal_saved_verification.json').read_text());geo=json.loads((P/'geometry.json').read_text());master=json.loads((P/'master_verification.json').read_text());assert master['surface_collision_checks_passed'];assert saved['rotor']['path']==rec['meshes']['Rotor']['refPath'];assert max(abs(a-b) for a,b in zip(saved['pivot_cm'],rec['pivot_cm']))<.001
for part in ['Body','Rotor']:
 actual=saved[part.lower()];bounds=geo['parts'][part]['bounds_m'];expected_origin=[(bounds[0][i]+bounds[1][i])*50*(-1 if i==1 else 1) for i in range(3)];expected_extent=[(bounds[1][i]-bounds[0][i])*50 for i in range(3)];assert max(abs(a-b) for a,b in zip(actual['bounds_origin'],expected_origin))<.01;assert max(abs(a-b) for a,b in zip(actual['bounds_extent'],expected_extent))<.01
for folder in ['exports','scripts']:
 shutil.copytree(P/folder,dest/folder,dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__','*.blend1'))
shutil.copytree(repo/'SourceArt/Generated/Buildings/HansaTowerMill_CapFit_20260906_03/textures',dest/'textures',dirs_exist_ok=True)
(dest/'renders').mkdir(exist_ok=True)
for name in ['unreal_animated_preview.png','windmill_rotation.mp4','unreal_rotation.mp4','glb_frame_1.png','glb_frame_31.png','glb_frame_61.png']:
 shutil.copy2(P/'renders'/name,dest/'renders'/name)
for name in ['000.png','030.png','060.png','090.png']:
 shutil.copy2(P/'renders/animation'/name,dest/'renders'/('rotation_'+name))
for f in P.glob('*.json'):
 if f.name not in ['clearance_offset_test.json']:shutil.copy2(f,dest/f.name)
readme=f'''# Animated Hansa windmill

Four sails, windshaft, hub plate, rivets and bindings now rotate as one assembly. The tower and corrected roof stay stationary. The pivot is at the shaft axis, not at the building origin. Default speed is **6 rpm / 36 degrees per second**, one complete turn every 10 seconds.

## Open and play

Open `/Game/Hansa/Developer/GenerationPreview/HansaWindmillAnimated_20260906_04/L_AnimatedWindmill` and click **Simulate** or **Play**. The isolated preview uses GameModeBase to prevent Hansa scenario scenery spawning around it.

Placeable actor: `/Game/Hansa/Generated/Staging/HansaWindmillAnimated_20260906_04/BP_HansaWindmill_Animated`.

The building contains a stationary body mesh and a `TurningSails` ChildActorComponent. Its child class is `BP_HansaWindmill_Rotor_Clearance`; the movable rotor uses the native `SailRotation` RotatingMovementComponent. It activates automatically during play and rotates in local space, with zero pivot translation. No Python, Blender or generation worker runs in the game.

## Speed, pause and reverse

On `TurningSails`, use **Get Child Actor → Get Component by Class (RotatingMovementComponent)**. Set that component's **Rotation Rate** to `(Pitch, Yaw, Roll)`:

| Behavior | Pitch | Yaw | Roll |
| --- | ---: | ---: | ---: |
| Default 6 rpm | 36 | 0 | 0 |
| Slow 3 rpm | 18 | 0 | 0 |
| Pause at current angle | 0 | 0 | 0 |
| Reverse 6 rpm | -36 | 0 | 0 |

Pitch in degrees per second equals rpm × 6. For reusable defaults, edit `SailRotation` on the rotor Blueprint and review any child-actor-template overrides in the parent Blueprint. The live component's rate can be set by gameplay or Sequencer. This asset does not yet connect rotation to Hansa production, weather or game-speed data.

## Files and evidence

- [Packed editable animated Blender source](exports/HansaWindmill_Animated.blend): separate original members parented to a named rotor pivot, linear looping Y rotation, 24 fps / 240-frame playback cycle.
- [Animated GLB](exports/HansaWindmill_Animated.glb): rigid object-transform animation, 10-second cycle. Select the rotor action when a viewer/importer lists separate clips; Blender's glTF importer places clips in NLA tracks.
- [Body FBX](exports/SM_Windmill_Body.fbx) and [rotor FBX](exports/SM_Windmill_Rotor.fbx): separate static parts with deliberate pivots, used by the native Unreal animation. FBX files do not claim a baked skeletal animation.
- [Rendered animation](renders/windmill_rotation.mp4): 800 × 800, 12 fps, 10 seconds.
- [Actual Unreal playback recording](renders/unreal_rotation.mp4): timed captures from the running simulation. MCP capture cadence is sparse; this is runtime evidence, not the animation's frame-rate limit. Native captures are 1116 × 905; the MP4 adds one bottom padding row for encoding and does not resample.
- [Unreal still](renders/unreal_animated_preview.png) and [clean GLB reimport](renders/glb_frame_31.png).

## Verification

The initial rotation exposed a sail/tower intersection when a wing pointed down. Moving the rotor forward **0.60 m** and extending its shaft from 1.25 m to 1.85 m preserved the rear bearing endpoint. The corrected shaft pivot is **(0, 399, 1082) cm** relative to the building. The preview's 1.5 cm ground offset applies to the whole actor.

Geometry was checked at 72 angles (5-degree steps), with no surface intersections between moving sails/hub/fittings and the stationary building. The shaft is intentionally excluded from that check because it enters the bearing. Minimum swept sail height is **2.567 m**. A separate 241-frame check confirms a fixed pivot and stationary body.

Both FBX parts were imported into clean Blender scenes; bounds, pivots, UVs and weathering colors passed. GLB checks confirm a quarter-turn, fixed pivot, stationary building and matching loop endpoints. The editable master reopens with five packed source images. Unreal's saved/reopened actor uses the corrected rotor mesh, matching bounds and all material assignments.

Live Unreal checks passed forward rotation, zero-speed pause, reverse and resumed forward rotation. Measured quaternion changes match rate × elapsed simulation time; the pivot and body remain fixed. See [runtime checks](runtime_verification.json), [source clearance](master_verification.json), [export checks](export_verification.json) and [saved Unreal readback](unreal_saved_verification.json).

## Materials and status

The existing eight material families and five 1254 × 1254 ImageGen worn surface originals are retained. Exact original prompts are in [textures](textures/). The 24 portable 1024 × 1024 PBR maps are reused; no image generation or resampling was needed for this motion revision. Vertex-color weathering is preserved.

This is a verified animated staging asset. Production promotion remains pending the repository's required approval. It is a visual rotation, with no physical sail collision or machinery simulation, no network phase synchronization, and no certification of LOD, performance, navigation or Shipping cook. Prior source masters and preview levels remain preserved.

Native component reference: [Epic RotatingMovementComponent documentation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/URotatingMovementComponent).
'''
(dest/'README.md').write_text(readme,encoding='utf-8');(P/'README.md').write_text(readme,encoding='utf-8')
(dest/'PROVENANCE.md').write_text('Animation and pivot/shaft revision authored locally for the user request. No provider calls, new textures, downloaded rigs or animation services. Original ImageGen inputs and prompt records are preserved in textures/. Source geometry is derived from the preceding cap-fit master, whose SHA-256 remains unchanged. Runtime assets contain native Engine components and staged mesh/material references.\n',encoding='utf-8')
(dest/'EVALUATION.md').write_text('Verified: separate stationary and rotating meshes, aligned pivot, full source sweep, clean exported animation, native runtime speed/pause/reverse, corrected child actor template, saved/reopened level, material readback and actual Unreal captures. Runtime limitations are listed in README.md.\n',encoding='utf-8')
(dest/'ITERATIONS.md').write_text('1. Split the cap-fit model into body and rotor, including hub fasteners. Corrected deferred Blender transform evaluation before export; verified origin-centred rotor.\n2. Added native Unreal rotating movement and tested forward/pause/reverse. Corrected the isolated preview game mode to suppress unrelated scenario actors.\n3. Full surface sweep detected blade intersections with the lower taper. Moved rotor forward 0.6 m and extended the shaft back into the cap; reran 72-angle intersection checks successfully.\n4. Corrected an inherited child-actor mesh override exposed by saved-instance readback, respawned the preview actor, and repeated runtime and saved-state checks.\n',encoding='utf-8')
manifest=[{'path':f.relative_to(dest).as_posix(),'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'bytes':f.stat().st_size} for f in sorted(dest.rglob('*')) if f.is_file() and f.name!='manifest.json'];(dest/'manifest.json').write_text(json.dumps(manifest,indent=2));print(dest)
