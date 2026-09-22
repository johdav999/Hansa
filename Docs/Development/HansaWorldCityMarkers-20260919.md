# Campaign city reference markers: Play/Simulate visibility repair

Target: `/Game/Hansa/Generated/Staging/HansaWorld_20260918/L_HansaWorld_WP`.

## Root cause

The live editor was running Play/Simulate. All 31 cube markers and 31 text labels
still existed in the source level's `CityMarkers` folder, but the world generator
had set `bIsEditorOnlyActor=true` on both actor types. They were excluded from the
play-world copy. The earlier review asserted that flag and counted source actors,
so it did not cover the user's preview workflow. This was not actor deletion or
a river/terrain import removing the city inventory.

## Repair

- Set `bIsEditorOnlyActor=false`, `bHidden=false`, and `bIsSpatiallyLoaded=false`
  on the exact 62 manifest-matched reference actors. Preserve their positions,
  meshes, labels, materials, data-layer membership, and non-colliding behavior.
- Save the staged level and its external actor packages with Unreal's native
  Save Current Level action. The generic MCP `save_actor` helper incorrectly
  relies on asset-registry discovery of these external actor packages; merely
  saving the map asset through `save_assets([map])` did not save the actor edits.
- Update creation and finalization in `HansaWorldMapCommandlet.cpp` so future
  authoring does not restore the editor-only flag.
- Strengthen `HansaWorldMapReview.cpp` to check both cube and text actor flags,
  component visibility and always-loaded state, and use game-view capture flags.
- Add `Scripts/HansaWorld/markers.py`: exact-map/manifest-gated repair, source
  visibility audit, and `--preview` regression against actual `UEDPIE_` actor
  paths, all 31 city identities, and all 62 non-hidden/non-editor-only actors.

These are temporary reference aids in a NeverCook staging map, not production
gameplay objects. `Config/DefaultGame.ini` still excludes
`/Game/Hansa/Generated/Staging` from cooking. Do not promote these debug references
into production without an explicit authoring decision.

## Validation

Source inventory: 31 cubes plus 31 text labels. A new simulation contained all
62 actors with actual `UEDPIE_0_L_HansaWorld_WP` paths. The live simulation view
visibly showed the Lübeck cube and name. The map was then reloaded from disk:
62 source actors remained, with saved editor-only, hidden and spatial flags false.
Map check after reload reported zero errors and warnings.
The final disk-reloaded simulation passed `python Scripts/HansaWorld/markers.py
--preview`: `{"preview_actors":62,"all_preview_visible":true}`. The editor was
left in simulation near Lübeck so the restored reference is immediately visible.

The C++ generator/review changes are source updates; the open editor was not
rebuilt or restarted for this property-only repair. Verification uses the running
editor's property APIs and real simulation, not a claim that the updated C++ test
binary has already run. Terrain, water, lighting and tree assets are untouched.
