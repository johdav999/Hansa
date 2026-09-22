# Campaign map startup-menu repair — 2026-09-19

Target map: `/Game/Hansa/Generated/Staging/HansaWorld_20260918/L_HansaWorld_WP`.

## Root cause

The campaign-map finalization path explicitly assigned
`AGameModeBase::StaticClass()` to `AWorldSettings::DefaultGameMode`. This map-level
override took precedence over `GlobalDefaultGameMode=/Script/Hansa.HansaGameMode`
from `Config/DefaultEngine.ini`. A normal Play session therefore instantiated
`GameModeBase_0`, not `AHansaGameMode`, so Unreal never created the Hansa strategy
player controller, root HUD, simulation host, or production frontend.

This was independent of the terrain, water, city-marker, and World Partition work.

## Repair

- The saved staged map now uses `/Script/Hansa.HansaGameMode` in World Settings.
- `HansaWorldMapCommandlet.cpp` now assigns `AHansaGameMode::StaticClass()` during
  finalization, preventing future terrain-map rebuilds from restoring the bug.
- `HansaWorldMapReview.cpp` now fails if this campaign map does not use the
  production Hansa GameMode.
- No project-wide default map, terrain, water, trees, lighting, city markers,
  simulation data, or frontend implementation was changed.

## Verification

The level was saved, reloaded from disk, and its `defaultGameMode` property read
back as `/Script/Hansa.HansaGameMode`. A normal in-viewport Play session then
created `HansaGameMode_0`, `HansaStrategyPlayerController_0`, and
`HansaRootHud_0` in the `UEDPIE_0_L_HansaWorld_WP` world. Native editor capture
showed the production Hansa title page with Continue, New game, Load game,
Settings, Credits, and Quit.

The permanent code change compiled successfully in
`Saved/BuildArtifacts/20260919-122426714-build-HansaEditor-Win64-Development`.
The independent compiled campaign-map review, including the new GameMode
assertion, passed at
`Saved/BuildArtifacts/20260919-122526457-hansa-world-review`. The editor was then
reopened on the saved map and a second normal Play session again instantiated the
production GameMode, HUD, and controller and displayed the title page.

The full map remains under `/Game/Hansa/Generated/Staging`, which is NeverCook.
This repairs development play of the staged campaign map; it is not production
promotion or packaged-build acceptance.
