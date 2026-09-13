# Runtime performance overlay

## Purpose

Normal gameplay displays a compact frames-per-second count in the existing top-right HUD panel. The value is a half-second sample of rendered Slate frames, making sustained slowdowns visible without requiring a console command or developer instructions.

## Component inventory

- Screen shell: existing native `SHansaRootHud`.
- Navigation: unchanged.
- Panels: existing top-right Baltic Navy/Brass status panel.
- Status/feedback: native, read-only `FPS {count}` text using the shared dark Data typography.
- Tables, controls, charts, overlays, icons, and decorative imagery: unchanged; no new raster or generated image is used.

## Component specification

- Component ID: `HUD.TopStatus.FPS`.
- Deliverable class: native status widget.
- Implementation: Slate `STextBlock` inside the existing root HUD.
- Content: localized `FPS` label plus a rounded integer frame rate.
- Update cadence: accumulated every frame and published twice per second.
- Default state: `FPS —` before the first complete sample.
- Running state: `FPS {count}`.
- Hover: native explanatory tooltip.
- Pressed, selected, disabled, focus, loading, warning, and error: not applicable because the status is read-only and non-interactive.
- Accessibility: explicit text communicates the metric without color; Chalk-on-Navy shared styling meets the existing dark-surface contrast contract.
- Localization: the label shares the responsive connection-status column and uses localized numeric formatting without widening the compact top-right panel.

## Initial root-cause evidence

This overlay is diagnostic and does not claim to fix performance. The first investigation found these plausible contributors, in descending order of concern:

1. The project defaults to maximum-quality DX12 rendering with Lumen global illumination/reflections, virtual shadow maps, hardware ray tracing, and Substrate enabled together.
2. Lübeck currently boots from a generated staging World Partition map with a 2017x2017 Landscape and numerous Water Body River meshes. Recent logs contain 34 waits for generated WaterInfo meshes before play.
3. The same logs contain repeated virtual-texture pool recreation events, which can produce visible hitches while terrain/material state settles.
4. The Lübeck world-art actor uses a movable Sky Light with real-time capture enabled, adding recurring GPU work even though the lighting preset is otherwise static.
5. Each game frame scans every building-projection actor and samples mill components. Each simulation tick also rebuilds projections and refreshes broad HUD models; 4x/12x game speed amplifies that CPU work.
6. Economy diagnostics are enabled by default every ten seconds and build/log a full projection. This is more likely to cause periodic hitches than persistently low FPS.

Use `stat unit`, `stat gpu`, Unreal Insights, and a standalone Development build to separate Game, Draw, GPU, and editor-only overhead before changing quality or content settings.
