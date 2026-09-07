# Animated Hansa windmill

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
