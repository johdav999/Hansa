# EMVP-P32 — Real cargo and production world projections

Runtime implementation and technical acceptance are complete. Full release-art acceptance remains open for the inherited P11/P30/P31 content gaps. No staged map, draft model, texture, or reference was promoted in this task.

## Implementation

`AHansaCargoProjectionManager` now owns the non-replicated cargo actors in the world. The HUD no longer creates a second Rostock fleet. Simulation publications reconcile actors by complete vehicle/job identity, including generation. Game-mode frame updates sample only fractional simulation time. No transform, wheel angle, sail state, selection, or cosmetic stock cue can issue a gameplay command or alter inventory.

Sea observations join the vehicle, cargo inventory, assigned route and last transfer. A mismatched inventory, missing approved class or missing city berth produces an explicit failure and no substitute mesh. Sea actors are capped at eight. Local delivery actors are pooled, represent every active authoritative job up to an explicit 128-job safety guard, and use deterministic lane spacing when journeys overlap. Completed/cancelled/stale identities release actors to the pool. The default starter harbor now exposes a native berth datum outside the north pier. City-level routes have no simulated dock assignment, so unrelated or unfinished harbor actors cannot relocate vessels. The authored city-port datum stays stable across construction and reconstruction.

The sea journey uses compressed port lanes: the origin city presents departure/travel and the final quarter of the leg uses the destination approach. These two city presentations are not a geographically continuous navigable sea. Load/unload are recorded atomic simulation events; their visual state is held at the port for the corresponding displayed tick. The cargo mesh means occupied hold, never an exact count of sacks or a claim about commodity appearance. Exact quantities remain native text and typed query values.

Local deliveries follow the authoritative stored road cells from the source building anchor to the destination building anchor. Building inventories resolve directly through placement; city inventories resolve through the selected physical market. The reconstructed distance must match the dispatched job. A disconnected road pauses a loaded in-transit wagon in place with its exact cargo until the authoritative job resumes. Job pickup/delivery ticks determine progress; wheel rotation is an absolute function of sampled road distance. Cargo is absent before pickup and after delivery. Full per-job identity, good, quantity, endpoint, timing, status and failure details are available to the native inspector and automation.

Bread, fish and plank-chain production objects expose `FHansaProductionWorldObservation`. Verified bakery flour/bread and lumber/sawmill timber/plank roles reflect actual inventory presence. Farm work props and saw-work roles reflect active/unblocked production. Mill mechanisms are sampled from completed cycles plus current progress; autonomous rotating-component ticks are disabled. There are no new worker, particle, or ambient cargo actor spawners. Missing production role art is explicitly reported, and the generic building mesh is not presented as a working factory.

Pausing preserves the fractional tick instead of resetting visual phase backward. New game and save restoration explicitly reset the fractional adapter. Normal, 4× and 12× speed use the same simulation clock. Save/load re-publishes the read-only projection, reconstructs cargo and production roles, and keeps the stable cargo selection when the entity remains valid. Returning from Rostock hides both rendering and selection collision. World teardown destroys managed actors.

## Typed queries and controls

| Surface | Contract |
| --- | --- |
| `QueryCargo()` | Owning array of `FHansaCargoWorldObservation`: identity, city, route/job/inventory links, cargo milli-units, transfer quantity/tick, simulation tick, phase/progress, location, visibility and failure |
| `FindObservation` / `FindActor` | Exact semantic identity lookup; no value-only aliasing |
| `World.Cargo.Vehicle.<value>.<generation>` | Stable Cog semantic selection target |
| `World.Cargo.Delivery.<value>.<generation>` | Stable local delivery semantic selection target |
| `SelectCargo` / `AHansaRootHud::InspectCargo` | Reject unavailable/new offscreen targets; open the native inspector for a visible entity |
| `QueryProduction()` | Availability, work state, blocker, cycle progress/completions and presentation failure on each building actor |
| Inspector actions | Existing Close, Frame, related Trade map and Pin controls; ordinary native focus and scrolling; disabled offscreen Frame cannot report success |
| `CancelRoute` | Runtime forwarding to the existing authoritative cancellation command; presentation does not own cancellation rules |

Selecting another world object clears cargo focus. Selection uses a query-only box, with physics, navigation and overlaps disabled. Production meshes retain their existing selection/inspector contract. Cargo-specific headings replace factory terminology, and cargo inspection does not trigger the building-inspection tutorial.

These local actors intentionally operate only in standalone worlds. They are not a new multiplayer replication channel; the existing two-client authority proof continues to use its own filtered projection contract.

## Component inventory and visual references

| Component | Existing source / states |
| --- | --- |
| Cog body, rig, sails, cargo | Approved P19 native Blueprint and seven-role family: empty/occupied, set/furled, port/travel, hidden/error |
| Wagon body, cargo, four wheel instances | Approved P19 Blueprint: awaiting pickup, in transit, delivered/hidden; deterministic wheel phase |
| Production role components | Existing farm, bakery, mill, lumber and saw-yard assets: constructing, working, stopped/blocked, stock absent/present |
| Inspector shell / navigation | Approved P21/P23 Slate panel, typography and spacing; cargo identity/state, close and focus restoration |
| Lists / feedback | Existing flow/history rows, cause/evidence/remedy and unavailable-state treatment |
| Controls / icons | Existing shared action and information glyph; default, hover, pressed, focused, disabled and pinned states |
| Charts / decorative imagery | No new chart, decorative raster, screen background or asset family |

Production assets are reused without changes from `Content/Mesh/hansa-vehicles/`, including `BP_Cog_Review.uasset` and `BP_Wagon_Review.uasset`, through `Content/Hansa/Core/Vehicles/DA_Vehicle_Cog.uasset` and `DA_Vehicle_Wagon.uasset`. Despite their historical names, those are the P19-approved production packages, documented in [the approval report](EnhancedMvpVehicleFamily.md). No new asset import or gameplay-definition hash change was needed.

This is implementation of approved visual components, not a new GUI design or raster generation task. Generation mode for P32: **none**. No production raster was resampled. The existing built-in ImageGen inspector reference remains a **visual reference only**: [healthy inspector, 1024×1536](../Images/UI/GuiRepair/guirepair--inspector--healthy--1024x1536--v1.png), with its [original prompt](../Images/UI/GuiRepair/guirepair--inspector--healthy--1024x1536--v1.prompt.md). The [P19 source archive and prompt records](../../SourceArt/Generated/Vehicles/HansaVehicles_P19_20260908/README.md) remain the vehicle material provenance. P32 creates no additional prompt set or production imagery.

Final native game captures are in `Docs/Images/World/CargoP32/`, with the 1.4 UI-scale run in `Scale14/`. Each PNG has a sibling typed evidence `.txt`; `validation.json` records native dimensions, original file hashes and identity/state checks. Dimensions are 1280×720, 1920×1080, 2560×1440 and 3440×1440. These are test evidence, not imported game assets.

Original-size inspection of the 1280 cargo inspector against the generated reference confirmed the shared dark title surface, parchment panel, restrained borders, native controls and readable focus. It caught and corrected the building-specific headings/icon and inappropriate inspection tutorial. The 1.4-scale inspector remains scrollable and framing brings the focused action into view. The existing tutorial/alert overlays consume much of the smallest viewport; the responsive header now includes Menu and Return-to-Lübeck in its row calculation, fixing the local-game label overflow. A native semantic containment check guards every visible header control. All four resolutions receive automated native-dimension, opaque-alpha, cargo-identity and authoritative-state comparisons; this does not substitute for final art approval of every frame.

## Verification

- Development Editor build passes.
- Four P32 headless contracts pass: `RouteLifecycle`, `RealRoadDelivery`, `ProductionRoles`, `SpeedClock`.
- All 61 existing UI tests, two runtime-host integration tests and ten save integration tests pass.
- Five native cargo runs pass: the four reference resolutions plus 1280×720 at UI scale 1.4. Eight states per run give **40 native screenshots**.
- The existing P31 native visit/arrival/delivery/save/return flow also passes at 1280×720 after fleet ownership moved out of the HUD.
- Shipping binary exclusion passes; current production dependency and expanded cooked-package audits pass. These are not final packaged-IoStore or whole-game GPU-performance approval.
- `python Scripts/ValidateCargoProjection.py` validates the archived evidence without editing the images.

The cargo proof records the same `World.Cargo.Vehicle.1.0`, route `4.0`, and cargo inventory `1001.0` in every frame and display profile. **10,000 milli-units load at tick 1; 10,000 remain aboard at berth with the route at 100% completion; 10,000 unload at tick 12 and cargo becomes zero.** Selection, framing and save restoration preserve the unload fingerprint. Local wagon tests dispatch, pick up, move and deliver through the real logistics pipeline, check cargo/identity and road distance, and prove rendering samples cannot mutate inventory. The follow-up [visible local wagon transport implementation](VisibleLocalWagonTransport.md) adds complete native Grain Farm to Market and Market to Mill captures, concurrent job coverage, road-break recovery, pooling and save/load reconstruction using the approved P19 model.

Primary evidence runs:

- `Saved/BuildArtifacts/20260909-190244522-automation-Hansa.World.CargoProjection`
- `Saved/BuildArtifacts/20260909-184940885-automation-Hansa.UI`
- `Saved/BuildArtifacts/20260909-183346655-automation-Hansa.Integration.RuntimeSimulationHost`
- `Saved/BuildArtifacts/20260909-183346846-automation-Hansa.Integration.Save`
- `Saved/BuildArtifacts/20260909-185806360-gui-repair-1280-720`
- `Saved/BuildArtifacts/20260909-185845349-gui-repair-1920-1080`
- `Saved/BuildArtifacts/20260909-185929537-gui-repair-2560-1440`
- `Saved/BuildArtifacts/20260909-190010412-gui-repair-3440-1440`
- `Saved/BuildArtifacts/20260909-190052876-gui-repair-1280-720` (1.4 UI scale)
- `Saved/BuildArtifacts/20260909-184139171-gui-repair-1280-720` (P31 regression)
- `Saved/BuildArtifacts/20260909-183831555-media-shipping-audit/result.json`
- `Saved/BuildArtifacts/20260909-190259742-shipping-exclusion-Win64/result.json`

The successful cook's disposable `Cooked/` copy was removed after the disk filled; its audit reports and logs were retained. Production source content and all archived screenshots were preserved. The subsequent final Shipping exclusion recheck also passed.

## Editor/game/automation parity and limits

There is no new authoritative gameplay schema, stable gameplay ID, provider integration, recipe, cargo rule or save format. Existing vehicle definitions, validation, editor metadata and P19 catalog bindings remain unchanged. New observation structs/functions are runtime/Blueprint-readable and usable in editor inspection; automated tests invoke the same projection owner and native inspector as the game. Cancellation forwards the existing command gateway. Editor and generation-worker dependencies were not added to runtime. Development fixtures remain in test modules and are excluded from Shipping.

P11's verified Fishery role asset is still absent; its production state remains queryable, but a complete fish-chain art presentation cannot be certified. The default starter trade scenario has no active local logistics jobs, so no decorative wagons are invented. Real jobs appear when the logistics simulation dispatches them. P30's world-art gaps and P31's staged Rostock approval remain open. There are no new horses, drivers, harnesses, continuous geographic sea navigation, production smoke/particle systems or GPU-budget acceptance claims.

## Manual check

Restart into the rebuilt Development preview:

```powershell
./Scripts/LaunchGuiPreview.ps1 -P31Candidate -EngineRoot H:/Unreal/UE_5.8
```

Start New game. Open Trade map, choose New route and Cog, lower the first stop's minimum reserve by one step, review, and activate. Watch the Cog leave the Lübeck harbor; click it for cargo and journey details. Use the destination's Visit city action for the Rostock approach and unload. Pause or change speed, frame the selected vessel, save/load, then Return to Lübeck. `-P31Candidate` enables the staged destination for review and does not promote it to production.
