# Empty New Game and session menu — 2026-09-11

Normal New Game now starts Lübeck with no simulated buildings, roads, building production or resident cohorts. Starting house money, city stockpiles, markets, research, ships and trading opportunities remain available. Rostock remains the prebuilt trading destination. The explicit shortage fixture keeps its seeded setup for existing deterministic tests. Existing saves keep their own state; New Game is the way to begin from scratch.

The reset also forces world-projection reconciliation, removing the previous city's actors immediately. Save restoration forces the same reconciliation so the visible city follows the loaded state. Legacy RoadHarbor/RoadSpine foundation geometry was removed. Two static bakery/residence showcase actor packages were removed from the Lübeck World Partition map; reusable building mesh assets are retained. Their original packages are backed up under Saved/Backups/EmptyCityStarterShowcases, with identities and hashes in Saved/P28/removed-starter-showcases.json.

## Menu controls

Click Menu at top right, or press Escape when no active tool/panel consumes it. The existing pause menu offers Resume, Settings, Return to title, Review prosperity and Save / load. Save / load opens manual/autosave slots and save/load actions; loading and returning to title use existing confirmation. No new screen, style, raster asset, prompt or image generation was required.

## Verification

- New Game, deterministic restart, retained supplies, empty simulation and save compatibility: Hansa.Integration.Save.FrontendNewGameAndRejectedRestore passed.
- Pause-menu Save/load and Return-to-title action wiring: Hansa.UI.Session.LifecycleAndPersistentHelp passed.
- Frontend confirmation/session state: Hansa.UI.Frontend.SessionAndSettingsContract passed.
- Foundation contract without baked starter roads: Hansa.Content.World.LubeckMapContract passed.
- Native controller flow through New Game, pause, save, confirmed load and confirmed return to title passed at 1280x720 and 1920x1080.
- Captures: Saved/P28/session-<width>x<height>-scale100-{camera,pause,saved,load-confirmation,restored-paused,return-confirmation,returned-title}.png and matching semantic TSV files. These are real-viewport evidence, not production assets.
- Empty-city capture also asserts zero live building projections and no static bakery/residence showcase meshes. Saved/P28/empty-city-visible-meshes.tsv records the visible mesh inventory.
- Development and DebugGame editor builds succeeded. No gameplay schema, provider workflow, or save format changed.

Restart the editor/game to load the rebuilt binaries and map, then choose New game. Loading an older save intentionally restores that save's constructed city.
