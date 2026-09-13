# EMVP-P31 — Rendered Rostock trade quarter

Status: implemented as a staged, playable review candidate. Production promotion and final art/performance acceptance remain open. The release map is not silently redirected to staging.

## Player flow

Run `.\Scripts\LaunchGuiPreview.ps1 -P31Candidate` after a Development build. Choose New game, open the city breadcrumb, select Rostock, then **Visit city**. Inspect the prebuilt market, residences, bakery, mill, quay or dock in the world. The existing city report retains its authoritative market state and report age. Use **Return to Lübeck** in the top HUD; the previous camera focus, zoom and rotation return. Existing route editors also expose **Visit selected stop**. Without the candidate switch, unavailable production content reports a recoverable error and preserves the current view.

A real own-house Cog appears during the terminal approach to Rostock and while berthed. Its stable vehicle/route identity and cargo come from the read-only simulation projection. Unloading records the applied quantity, good and tick. Return travel outside this bounded quarter is P32 work; this is not a complete voyage animation. No cosmetic vessels or simulated buildings are invented.

## Implementation and parity

- One asynchronous conventional level instance at a local 600 m presentation offset. The offset is not geographic distance. One cached quarter, 30-second failure recovery, local camera bounds, repeated-visit support and construction cancellation. No second simulation host or authoritative level travel.
- `AHansaRostockQuarter` uses 11 native HISM components: residences, market, bakery, mill body, mill rotor, quay, dock, hoist, handling skids, mooring and streets. Twelve residences, 22 quay segments and 34 street segments reuse the approved shared kit at identity scale. No remote Engine basic shapes. Art blocks selection traces only; it does not affect navigation or create economic entities.
- Native Landscape: 253 × 253 vertices, sixteen 63-quad components, 250 cm horizontal spacing, 25 cm Z scale, 630 m square extent. Locked `Interpreted_TradeQuarter` edit layer. Land +100 cm; inferred riverbed −450 cm; shoreline transition is explicit gameplay grading.
- Native `AWaterBodyCustom`/WaterZone: −125 cm surface, project-authored two-triangle clockwise mesh, 630 m extent, identity scale, no automatic carving or buoyancy. Native SingleLayerWater volume absorption/scattering and restrained procedural normal motion; no new raster textures. Original reversed mesh winding made the surface invisible from above; a native capture caught this and the corrected render was inspected.
- Construction is disabled at the presentation-intent boundary as well as hidden from HUD semantics/controller focus. Card selection/drag, placement confirmation and road drawing reject remote input. Returning restores local civic values even while paused.
- City and route-stop Visit actions, Return action, status and keyboard/controller focus use existing Slate components. Loading and missing-content messages preserve the market report. Inspection does not request knowledge refresh or present unknown remote quantities as zero.
- Visible cargo projections are bounded to eight own-house vehicles. Stable vehicle IDs, actual cargo, berth/approach state and positive unload receipts come from existing route projections. A non-rendered query-only box makes the native Cog selectable. Missing Cog presentation reports a failure instead of substituting a cube.
- No gameplay schema, definition identity, simulation/save format or editor schema changed. These are reconstructible local presentation state and existing definition references. Runtime depends on no editor/provider code. Authoring remains editor-only and requires explicit `-P31Authoring`; dirty editors/PIE are refused. No live provider worker calls.

## Historical grounding and provenance

The [official Rostock city history](https://www.rostock.de/entdecken/stadt) describes merchant/craft settlement on the left Warnow bank, 1218 town rights, the 1265 union of three towns, and the harbor/market/storage relationship. The city’s [2014 Stadthafen competition brief](https://rathaus.rostock.de/media/4984/Auslobung_Rostock_Stadthafen_klein.pdf), section 2.2.2, distinguishes historical harbor development from later alterations. Both were accessed 2026-09-09. Their text informed the compact merchant-lanes/market/waterfront arrangement; no source imagery is redistributed.

This is an interpreted late-medieval trade quarter, not a cadastral or georeferenced reconstruction. No exact street plan, measured bathymetry, surveyed elevation, religious landmark or specific building date is claimed. Existing Rostock survey files were preserved and not consumed. Local coordinates have no projected CRS or sea-level datum. The generated reference’s north-bank wording is not treated as geographical evidence; the historical grounding uses the official left-bank account.

Provenance and all dimensions are in `SourceArt/Terrain/Rostock/Gameplay_P31/manifest.json`. Ground material/textures reuse P18 through the existing P30 native authoring source. P31 owns independent ground/water packages. Shared kit master materials used by the new HISM components have their InstancedStaticMeshes usage flag enabled; no texture pixels or PBR values were altered by that compatibility fix. The current three-LOD residence family replaces an accidentally selected older high-poly residence in the candidate.

## References, inventory and visual inspection

Selected reference: `Docs/Images/UI/RostockP31/rostock--quarter-reference--default--1536x1024--v1.png`, **1536 × 1024**, built-in ImageGen, original unchanged. The exact final prompt, generation mode and inspection notes are in the sibling `.prompt.md`. It is a visual reference, not imported or shipping artwork. There are no new production raster assets and no resampling.

Component inventory: existing navy HUD shell and city breadcrumb; existing linen city report/market tables and report-age captions; shared Visit, Market, Trade map and Return actions; existing status/error text and focus outline; native world terrain, water and shared kit. Default, hover, pressed, focus and disabled controls reuse `SHansaAction`; loading/unavailable and remote construction states are native text/visibility. No second palette or typography system was introduced. Existing P21 component references remain the component anchors.

The native waterfront, berth, 1920×1080 market and 1280×720 market at 1.4× UI scale were inspected at original resolution. The corrected water, pier supports, cargo vessel and Return control read clearly; there is no checkerboard terrain or Engine placeholder in the remote quarter. The visual reference is more densely dressed: varied civic massing, vegetation, people and extra vessels are aspirational and have not been fabricated as delivered assets. The candidate uses an interim bare-loam ground, repeated residence family, handling yard in place of the unavailable P16 warehouse building, and inherited bright Lübeck lighting. This is reviewable functional assembly, not a claim of final AAA art parity.

## Reproduction and evidence

1. Build: `.\Scripts\Build.ps1 -EngineRoot H:/Unreal/UE_5.8`.
2. Author a fresh candidate with `.\Scripts\StageRostock.ps1 -Create`; existing destinations are refused. Without `-Create`, revise only the pinned staging map. Production packages are not promoted by this script.
3. Capture: `.\Scripts\CaptureGuiRepair.ps1 -P31Candidate -TestFilter Hansa.World.Rostock.RealViewport -Width 1920 -Height 1080`. Repeat at 1280×720, 2560×1440 and 3440×1440. `-UiScale` is applied to native widgets.
4. Validate and preserve originals: `python Scripts/ValidateRostock.py`.
5. Focused tests: `Scripts/RunAutomationTests.ps1 -SkipBuild -TestFilter Hansa.World.Rostock`, then `Hansa.UI` and `Hansa.Integration.Save`.

The nine native states are home, visited, waterfront, market, approaching, berthed, delivered, restored and returned. Normal Slate focus/Enter creates the route and visits the city. Deterministic tick advancement then observes actual approach/berth/unload; it does not synthesize cargo. Typed inspection exercises the same HUD handler as world selection. Native traces verify ground/riverbed elevations and that the rendered Cog is selectable. The save round trip compares full authoritative fingerprints. Construction denial and restoration, unchanged travel/inspection state and matching state at every resolution are asserted.

Each native screenshot preserves original RGB pixels and dimensions; alpha is explicitly set opaque at export because the viewport buffer does not define it. There is no resizing or post-render artwork. Each PNG has a mesh inventory TSV and synchronized text evidence. The inventory is loaded visible mesh components and LOD0 upper-bound triangles; it excludes Landscape and is not GPU draw calls or a frustum count. Short game/render-thread means are preliminary CPU observations, not a GPU benchmark. Full GPU/overdraw/texture-memory and final cooked Rostock performance remain acceptance work.

## Production boundary

Staged packages: `Content/Hansa/Generated/Staging/Rostock_P31/` (`L_Rostock_Quarter`, `M_Rostock_Ground`, `M_Rostock_Warnow`, `MI_Rostock_Warnow`, `SM_Rostock_Warnow`). Intended approved map: `/Game/Hansa/World/Cities/Rostock/L_Rostock_Quarter`, currently absent. The staging switch is compiled out of Shipping. Existing production Lübeck content is unchanged by P31.

The [cityterrain skill](C:/Users/Johan/.codex/skills/cityterrain/SKILL.md) requires: “Stage new or revised terrain, material, and water content first.” This candidate follows that staging rule. The additional approval requirement comes from the repository [AGENTS.md](../../AGENTS.md), which requires generated content to be staged and to “require explicit approval before promotion to production content.” That is why the candidate is not promoted automatically.

Final promotion must follow the repository’s explicit review/approval rule for generated content, after art limitations and target-platform performance are accepted. A current-production Shipping audit cannot prove that the unpromoted Rostock candidate is ready to cook. No claim of final Rostock package acceptance is made.

## Verified results — 2026-09-09

- Final Development build: `20260909-174249277-build-HansaEditor-Win64-Development`.
- Final focused regressions: **73 passed** — 2 `Hansa.World.Rostock` (`20260909-174716545`), 61 `Hansa.UI` (`20260909-174731359`), 10 `Hansa.Integration.Save` (`20260909-174748065`). The initially mistyped `Hansa.Save` filter matched no tests; the correct integration suite was subsequently run successfully.
- **Five native flows, 45 captures**: four resolutions at 1× scale and 1280×720 at 1.4×. Final native runs: `20260909-174255650`, `20260909-174337068`, `20260909-174418489`, `20260909-174501444`, `20260909-174547222`. See `Docs/Images/World/RostockP31/validation.json`; all RGB pixels/dimensions preserved, all output alpha opaque, and authoritative fingerprints match across display profiles. The separate `Scale14` folder preserves nine maximum-scale captures.
- Native corner traces cover all tested camera views after extending the background; sampled ultrawide corner RGB values also confirm the previous black exterior is gone. Selected screenshots received original-resolution visual inspection; automated resolution/corner checks do not stand in for final art sign-off at every possible camera setting.
- At tick 11, route 4’s real Cog is berthed with 10 units aboard. Tick 12 records a 10,000-milliunit Rostock unload. Delivery, save restoration and return share fingerprint `6164082863810064148`; the first four travel/inspection states are unchanged at `6519455959296703690`.
- Shipping target receipt/executable exclusion: passed (`20260909-174041240-shipping-exclusion-Win64`). Current production Lübeck cook: passed (`20260909-174627905-media-shipping-cook`); expanded cooked-package and dependency audit passed (`20260909-174647306-media-shipping-audit`). No staging/editor/worker leakage was found in that scope. These results do not constitute a cooked Rostock acceptance, because the candidate has not been promoted.
- Audit startup was repaired to keep linked editor ToolsetRegistry/Water dependencies available. A process-only, editor-only `GameFeatureData` NeverCook scan rule avoids unrelated plugin startup errors. Persistent project configuration and production asset rules were not changed. Shipping exclusion continues to be checked against real receipts and expanded cooked packages.
- Python/PowerShell validation scripts parse successfully; integration-file `git diff --check` passes.

Across these short remote-view samples, game-thread means ranged 1.238–2.558 ms and render-thread means 4.689–8.293 ms. At most six remote actors and 902,640 mesh LOD0 upper-bound triangles were inventoried in the exercised one-Cog flow. Landscape cost is excluded from that triangle count. These are bounded CPU/inventory observations, not GPU, draw-call, memory or eight-vessel stress acceptance.
