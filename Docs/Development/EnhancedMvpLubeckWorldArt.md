# EMVP-P30 — Lübeck world-art assembly candidate

Status 2026-09-09: **implemented staging workflow; P30 incomplete and not accepted for production**.

The required production assembly cannot truthfully pass while P11 Fishery has no model, P16 Warehouse remains unimported, P20 vegetation/shared dressing is incomplete, and the authoritative initial session projects Building.Brewery as an Engine cube. P30 does not substitute unrelated buildings, hide authoritative entities, or promote unseen assets. The production `L_Lubeck_MVP` map is unchanged by this task.

## Implemented candidate

`/Game/Hansa/Generated/Staging/LubeckWorldArt_P30/L_Lubeck_WorldArt_Candidate` is a separate World Partition map with the normal Hansa game mode, simulation host, start transform and native HUD. Its native Landscape uses the existing placement topology; this is an abstract gameplay slice, **not surveyed medieval Lübeck terrain**. The 4 km survey and its license/datum/historical acceptance work remain untouched.

- Landscape: 505 × 505 vertices at 100 cm spacing, 64 components, four streaming proxies, 504 × 504 m extent. Land is 75 cm, shore 85 cm, inferred river bed −450 cm; only non-buildable banks slope down. `Gameplay_Grading` is a locked edit layer. The exact 60 × 40 placement grid and terrain classification remain authoritative.
- Native WaterBodyCustom at −125 cm, a project-authored two-triangle surface at identity scale, native Water material instance and WaterZone. No Engine plane, automatic terrain carving, custom water actor, or measured bathymetry claim.
- `AHansaLubeckWorldArt`: native sun, realtime skylight, atmosphere, restrained fog, fixed exposure and day/evening presets. Eight approved P17 quay segments and four mooring posts use two HISM components, identity mesh scale, no collision/navigation and 180 m distance culling.
- `bUseAuthoredWorld` is an opt-in foundation property, false by default. The candidate suppresses all legacy foundation geometry/collision and duplicate foundation lights. Normal production foundation behavior remains unchanged.
- Ground uses the existing approved P18 dirt-road base color, normal and roughness at 4 m world tiling, with native macro/moisture shading. This is interim bare ground, not a finished vegetation or land-cover treatment.
- Stable environment roles: `Presentation.Environment.Lubeck`, `Presentation.Environment.Harbor`, `Presentation.Terrain.Lubeck.GameplayGrading`, `Water.Lubeck.Trave.Gameplay`. Gameplay buildings remain owned by the existing stable world-projection resolver.

There are no new GUI components, generated images, raster assets, resampled images or ImageGen prompts in this task. Existing imported P17/P18 assets are reused unchanged. All PNGs below are native engine evidence, not visual references or shipping imagery.

## Paths and reproducibility

- Candidate map/material/mesh packages: `Content/Hansa/Generated/Staging/LubeckWorldArt_P30/`, with its World Partition external actor/object packages under the corresponding `Content/__ExternalActors__/` and `Content/__ExternalObjects__/` paths.
- Source provenance and dimensions: `SourceArt/Terrain/Lubeck/Gameplay_P30/manifest.json`.
- Runtime: `Source/Hansa/Public/World/HansaLubeckWorldArt.h` and `Source/Hansa/Private/World/HansaLubeckWorldArt.cpp`; opt-in foundation changes beside them.
- Editor-only authoring: `Source/HansaEditor/Private/Tests/HansaLubeckWorldArtAuthoringTests.cpp`.
- Native tests: `Source/HansaTests/Private/World/HansaLubeckWorldArtTests.cpp` and `HansaLubeckWorldArtCaptureTests.cpp`.

After a Development build, `Scripts/StageLubeckWorldArt.ps1 -Create` creates a fresh candidate and refuses an existing destination. Without `-Create`, it revises/bakes only the pinned existing candidate. Explicit `-P30Authoring` guards prevent normal tests from writing assets; dirty editor state and PIE are refused. Rendering is required. The bake retains loaded World Partition actors, completes Landscape edit layers, verifies encoded heights, refreshes cached render bounds, and saves external packages. Omitting the bounds refresh previously produced correct collision but invisible terrain; native inspection caught and corrected that defect.

Preview the current staged result:

```powershell
.\Scripts\LaunchGuiPreview.ps1 -P30Candidate
```

Choose New game. Production preview remains the default without this switch.

Reproduce each native comparison with `Scripts/CaptureGuiRepair.ps1 -World Production` or `-World P30Candidate`, `-TestFilter Hansa.World.LubeckArt.RealViewport`, and dimensions 1280 × 720 or 1920 × 1080. Run sequentially for timing comparison. Then run `python Scripts/ValidateLubeckWorldArt.py` to validate and preserve the original files.

## Evidence and limits

The capture workflow exercises six day/evening camera views at 25/65/120 m, ordinary road and residence construction, and save restoration. The baseline's evening-labelled views retain its original lighting; only the candidate has the new presentation-only evening preset. The 120 m test framing is an explicit review override; it does not change the production camera's 95 m maximum.

The per-capture TSV inventories visible loaded mesh components, instances, LOD counts, LOD0 triangles, material slots, shadows, Engine shapes and staging references. LOD0 triangle totals are an upper bound for those mesh components, excluding Landscape; they are **not actual draw calls or frustum-visible triangle counts**. Short game/render-thread counters are preliminary CPU observations, not GPU performance acceptance. Removing duplicate legacy display buildings accounts for much of the mesh-count reduction; it is not evidence of an equivalent GPU speedup.

## Remaining acceptance gates

1. Complete and review P11 Fishery, P16 Warehouse, P20 vegetation/dressing and the missing Brewery presentation, then explicitly approve promotion of actual staged deliveries.
2. Replace native Engine-shape status markers and all golden-path placeholder bindings; validate before/after construction across the complete economy, not just the exercised residence.
3. Finish terrain/cover, shoreline/harbor joins, water appearance, controlled vegetation, camera-boundary views and final art direction. Current bare ground and limited quay dressing are not an intentional finished Hanseatic city.
4. Profile actual GPU time, overdraw, shadow cost, material cost, LOD/Nanite behavior and representative maximum population at the target hardware/resolutions. Current component inventories and short CPU samples are insufficient.
5. Review the actual candidate, promote approved packages out of staging and assemble the production map. Run the full game/editor/automation parity and clean cook/package exclusion gates. Enabling Water and passing a Shipping binary audit alone does not prove a clean packaged map.

No required acceptance item is waived. P30 remains open.

## Verified evidence — 2026-09-09

Native capture matrix: **32 original-size screenshots**, 1280 × 720 and 1920 × 1080, eight states in both production baseline and candidate. `Docs/Images/World/LubeckP30/validation.json` validates dimensions, source PNG SHA256, exact tick/fingerprint parity and construction/save restoration. No PNG was resized. Visual inspection at original size confirmed the corrected terrain/building grounding at 720p and the constructed residence at 1080p. It also confirms the remaining Brewery cube, bare ground, unfinished water/shore composition and limited dressing; **visual production acceptance failed**.

| Day / 65 m view | Actors | Mesh instances | LOD0 upper bound | Material interfaces | Engine-shape components | Game thread ms | Render thread ms |
|---|---:|---:|---:|---:|---:|---:|---:|
| baseline-1280x720 | 44 | 37 | 12028792 | 87 | 13 | 1.360 | 8.880 |
| candidate-1280x720 | 40 | 26 | 304820 | 45 | 1 | 1.548 | 5.145 |
| baseline-1920x1080 | 44 | 37 | 12028792 | 87 | 13 | 1.409 | 9.917 |
| candidate-1920x1080 | 40 | 26 | 304820 | 45 | 1 | 1.445 | 5.871 |

These are short samples on the current machine; no framerate/target-hardware claim is made. Construction raises candidate Engine-shape count to three: the Brewery cube, GrainFarm sphere status marker and Residence cone status marker. The candidate water surface remains one staging mesh reference. Neither gate is treated as passing.

All initial views share fingerprint `6519455959296703690` at tick 0. Both variants reach fingerprint `16982245970769070759` at tick 124 after normal road/residence commands; save restoration returns that same fingerprint.

Verification records:

- Rendered Landscape bake, height readback and corrected bounds: `Saved/P30/BakeBounds.log`.
- Broad `Hansa.World` run: **21 passed, 10 failed**. Every failure is an older P17/P18/P19 screenshot test requiring its isolated review map; this is not recorded as a successful suite. Evidence: `Saved/BuildArtifacts/20260909-162331349-automation-Hansa.World/`.
- `Hansa.Content.World`: **3 passed**, `Saved/BuildArtifacts/20260909-162751948-automation-Hansa.Content.World/`.
- `Hansa.Integration.Save`: **10 passed**, `Saved/BuildArtifacts/20260909-162808787-automation-Hansa.Integration.Save/`.
- Projection regression initially reproduced a Mill sail collision defect. An inherited external mesh collision profile could override the component enable flag. Presentation components now receive the explicit NoCollision profile, clearing default inheritance; this applies to nested actors and placement ghosts while preserving the projection-owned selection proxy.

- Final Development build: `Saved/BuildArtifacts/20260909-163024586-build-HansaEditor-Win64-Development/`.
- Final `Hansa.UI.World`: **7 passed**, including the Mill collision regression, `Saved/BuildArtifacts/20260909-163030203-automation-Hansa.UI.World/`.
- Final Shipping build/exclusion: **passed**, `Saved/BuildArtifacts/20260909-163047036-shipping-exclusion-Win64/result.json`. This audits the executable/receipt for forbidden modules/provider tokens; it is not a clean content cook/package audit.

Post-fix native construction/save rerun: **passed**, `Saved/BuildArtifacts/20260909-163129068-gui-repair-1280-720/`. Its eight candidate captures replace the prior 720p files; the unchanged 1080p visual comparison predates the collision-only fix. The evidence validator was rerun successfully after replacement.
