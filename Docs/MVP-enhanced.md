# Hansa — Enhanced Playable MVP Prompt Plan

## 1. Purpose

This plan closes the gap between the current integrated simulation MVP and a game slice that a player can start, understand, play, and enjoy without operating a prescribed automation scenario.

The target is a polished 30–60 minute vertical slice:

- Lübeck is the fully buildable player city.
- Rostock is a second fully rendered coastal Hanseatic city. It is prebuilt, inspectable, and tradeable in the MVP, but not player-buildable.
- The locally buildable production chains are bread, fish, planks, and beer.
- The full thirteen-good market, population needs, satisfaction, workforce, growth, decline, and tier progression remain active.
- The player constructs buildings by dragging building cards into the world, sees the real 3D model as a placement ghost, and receives unambiguous valid/invalid feedback.
- The player creates roads with a direct world interaction inspired by the usability of Anno games while retaining an original Hansa visual identity.
- The player can establish a Lübeck–Rostock sea route and see a cog carrying actual simulated cargo.
- Every visible building, vehicle, road piece, and recurring prop has an intentional production asset. Engine cubes and test placeholders are not accepted in the playable path.
- Every player-facing screen uses one coherent, professional, high-fidelity Hansa GUI system.
- The session supports player choice, recovery, and several viable approaches. It is not a disguised test script with one required sequence.

Rostock becoming a second fully buildable city, more local production chains, additional cities, and production multiplayer remain post-MVP unless this scope is explicitly changed.

## 2. Why the current MVP still feels like a simulation scenario

The implemented foundation is substantial: the deterministic economy, thirteen goods, recipes, buildings, population needs, city markets, research, routes, objectives, save/load, automation, and functional screens largely exist. The remaining gap is concentrated in the player-facing layer:

- only Lübeck has a playable world map;
- Rostock, Hamburg, and Lüneburg currently behave primarily as market records;
- missing building presentation falls back to blockout geometry;
- vehicles are simulation/UI records rather than visible world actors;
- the construction menu is hard-coded and text-led;
- building placement is click-oriented and exposes test-helper affordances instead of a complete drag-to-world interaction;
- the simulation has road-drag logic, but the player-facing input and world preview do not expose a finished road tool;
- several generated assets are source or staging work rather than verified production content;
- the UI uses functional flat brushes and default Unreal typography instead of the production component system specified by the UI brief;
- research is not fully wired into the root HUD;
- the trade screen does not yet provide a complete route-creation journey;
- the current golden screenshot proof can pass without proving a real rendered game viewport;
- the focused economic authoring reload test currently reports a catalog hash mismatch.

The enhanced MVP therefore does not require replacing the simulation. It requires converting the simulation into visible, tactile, coherent play and proving it through real user flows.

## 3. How to use these prompts

Copy one prompt at a time into a Codex task opened on this repository. Each prompt should normally become one reviewable change set. Complete its acceptance gate before starting a dependent prompt.

Every prompt inherits this execution contract:

~~~text
Read AGENTS.md completely, then read Docs/MVP.md, Docs/MVP-enhanced.md, and every architecture, design, workflow, or evidence document named by the prompt. Inspect the repository and git status before changing anything. Treat the current implementation as the baseline: extend or repair it instead of building duplicate systems. Preserve all unrelated and pre-existing user changes.

Keep authoritative gameplay deterministic and route state changes through the normal gameplay command gateway. When a gameplay data model changes, update its runtime model, editor schema and metadata, validation, migration/version handling, impact analysis, automation/query surface, fixtures, and tests in the same task. Maintain game/editor/automation parity and keep Editor-only and provider code out of runtime and Shipping.

For every UI, GUI, icon, map treatment, or raster-image task, read Docs/UIDesignBrief.md and Docs/UIAssetWorkflow.md completely. Follow the component-first workflow and use the imagegen skill when the task creates or materially revises visual design. Generated full-screen images are references, not shippable interactive screens. Build the shipping UI from native UMG/Slate, materials, vector/SDF art, and separate native-size raster assets. Do not bake dynamic text into images, resample raster assets, copy Anno assets, or invent a second style system.

For every new or revised 3D building, vehicle, road piece, or prop, use the HansaModels skill end to end. Follow its research, evidence, ImageGen surface-texture, headless Blender, hybrid-PBR, three-cycle render/inspect/correct, export/re-import, Unreal import, reopen/inspect, provenance, and delivery-record workflow. Do not substitute a generic model-generation workflow. Import verified assets to their canonical /Game/Mesh/<asset-slug>/ folder, then connect them through stable Hansa presentation references. Keep gameplay identity independent of filenames and provider metadata.

Add proportionate automated tests and run the narrowest relevant build, validation, and Unreal automation commands. For visual work, also inspect the real assembled result at the required native resolutions. Do not use synthetic flat-color buffers or source-art thumbnails as proof of the game. Do not commit, push, discard unrelated changes, or call paid providers unless explicitly authorized.

Finish with a concise report listing changed files, assets and their native dimensions, generation mode and prompt records where relevant, tests and inspections run, evidence paths, remaining limitations, and the next prompt IDs now unblocked.
~~~

## 4. Delivery map

| Wave | Outcome | Prompts |
| --- | --- | --- |
| 0 | Scope, current blocker, and measurable completeness contract | EMVP-P00–P02 |
| 1 | Complete construction and road interaction | EMVP-P03–P06 |
| 2 | Production-ready 3D asset set | EMVP-P07–P20 |
| 3 | Coherent high-fidelity GUI | EMVP-P21–P29 |
| 4 | Two rendered cities, visible logistics, and playable economy | EMVP-P30–P36 |
| 5 | UAT, optimization, and clean release proof | EMVP-P37–P39 |

After EMVP-P02, independent 3D asset prompts may overlap with interaction work. UI implementation may overlap only after EMVP-P03 fixes the construction presentation contract and EMVP-P21 fixes the shared UI component system. Avoid parallel changes to the same style tokens, presentation registry, catalog-version contract, or map.

## 5. Wave 0 — Lock the enhanced playable MVP

### EMVP-P00 — Reconcile the enhanced scope and acceptance contract

~~~text
Execute EMVP-P00 from Docs/MVP-enhanced.md. Compare Docs/MVP.md, Docs/MVPSprintPlan.md, the current implementation, and the enhanced playable-MVP definition. Update Docs/MVP.md and related acceptance documentation only where required to make the new target explicit: buildable Lübeck; rendered, prebuilt, inspectable, tradeable Rostock; bread, fish, planks, and expanded beer as the local production chains; the full thirteen-good market and population loop; card-drag building placement with real 3D ghosts; player-drawn roads; visible cargo vehicles; production UI; and a 30–60 minute non-prescriptive session. Preserve earlier architecture and editor-parity requirements. Mark a second buildable city, additional local chains beyond beer, and wider content breadth as post-MVP. Add a requirement-to-evidence matrix whose entries cannot pass on headless state alone when the requirement is visual or interactive. Done when the target cannot reasonably be interpreted as only an automated simulation scenario.
~~~

### EMVP-P01 — Repair the economic catalog version/hash release blocker

~~~text
Execute EMVP-P01 from Docs/MVP-enhanced.md. Reproduce the failure in Hansa.Integration.Authoring.EconomicAssetReload and investigate why the economic definition registry hash differs from the pinned value in HansaLubeckScenarioInitializer. Determine whether the content changed legitimately, registry ordering or serialization drifted, or a stale version pin is masking a defect. Do not merely replace the expected number. Apply the correct migration/version update or deterministic fix, document the decision, and add a regression test that explains future mismatches with useful per-definition evidence. Run the focused reload test, related catalog/version tests, and the narrowest affected integration suite. Done when the registry reload is deterministic and green from the checked-in economic content.
~~~

### EMVP-P02 — Create the visible-content and screen completeness manifest

~~~text
Execute EMVP-P02 from Docs/MVP-enhanced.md. Audit every object and screen that can appear during the enhanced golden session. Create a checked-in manifest covering buildings, construction stages, roads, ships, carts, citizens if shown, harbor equipment, production props, cargo props, vegetation, street dressing, water/shore elements, effects, icons, cursors, overlays, frontend screens, HUD panels, modal screens, empty/loading/error states, and required native resolutions. For each entry record stable ID, owning definition, required presentation role, current asset/reference, status, intended canonical path, LOD/collision/pivot/footprint requirements, and proof needed. Add automated validation that fails when a golden-path definition has no production presentation, points to staging/developer content, uses an Engine basic-shape fallback, or lacks a required UI state. Done when all remaining visual work has an explicit inventory and no visible placeholder can disappear into an unspecified backlog.
~~~

## 6. Wave 1 — Construction that feels like a game

### EMVP-P03 — Make the construction catalog and production chains data-driven

~~~text
Execute EMVP-P03 from Docs/MVP-enhanced.md. Replace the hard-coded build-menu card list with a data-driven construction presentation model derived from stable building, recipe, unlock, category, and presentation definitions. Implement the MVP categories and the chain-expansion model: selecting bread exposes separate Grain Farm, Mill, and Bakery cards above the bottom panel; selecting fish exposes Fishery; selecting planks exposes Lumber Camp and Sawmill. Cards must report cost, footprint, workforce, inputs/outputs, lock state, availability, and causal invalid reasons without duplicating simulation formulas. Add editor schema metadata, validation, migration handling, semantic IDs, and tests for catalog ordering, missing chain members, locked content, and hot reload. Done when adding or revising an MVP building definition does not require editing a hard-coded Slate list.
~~~

### EMVP-P04 — Implement card drag-to-world building placement

~~~text
Execute EMVP-P04 from Docs/MVP-enhanced.md. Implement the complete mouse drag journey from a construction card into the world. On drag start, create a placement session for the selected stable building definition. While the pointer moves across the viewport, render the authored 3D building presentation as a world-space ghost aligned to the placement grid, footprint, rotation, terrain, roads, shore rules, costs, and obstruction validation. Communicate valid, warning, and invalid states through material, outline/shape, footprint cells, cursor, and concise reason text rather than color alone. On release, submit the normal construction command or return to the menu if released outside a valid site. Preserve click-to-place, keyboard, and controller alternatives. Add cancellation, rotation, repeated placement, focus restoration, semantic coverage, and real viewport automation. Done when no test target buttons are needed to place a building and the player always understands where and why placement is allowed.
~~~

### EMVP-P05 — Complete direct road drawing

~~~text
Execute EMVP-P05 from Docs/MVP-enhanced.md. Connect the existing deterministic road-drag/path logic to real viewport pointer input and the construction UI. The player selects Roads, presses or clicks a start cell, drags across the world, sees a live connected road preview with total cost and per-cell validity, and releases to submit the ordinary road construction command. Support straight and Manhattan corner paths, clear tie-breaking, collision and ownership validation, cancellation, controller/keyboard alternatives, and predictable continuation from existing roads. Render final roads through the production road presentation contract rather than cubes. Add automation for drag, invalid spans, insufficient funds, intersection, replacement, cancellation, and save/load. Done when a player can connect production buildings to a warehouse without debug controls.
~~~

### EMVP-P06 — Finish selection, upgrade, move-state, and demolition feedback

~~~text
Execute EMVP-P06 from Docs/MVP-enhanced.md. Complete the post-placement interaction loop for residences and production buildings. Selection must frame the authored model, open the contextual inspector, expose construction progress and operating state, and provide valid actions through the normal command gateway. Implement the laborer-to-artisan residence upgrade required by the MVP, with costs, prerequisites, population movement, needs, visual swap, and failure explanations. Implement bounded demolition/refund behavior if already supported by the architecture; otherwise document and implement the smallest safe cancellation/removal contract required for player recovery. Add semantic and viewport tests for selection, construction completion, upgrade success/failure, visual replacement, and save/load. Done when placement mistakes and population progression can be handled through player-facing UI.
~~~

## 7. Wave 2 — Production-ready 3D asset set

All prompts in this wave create or revise models through the HansaModels skill. Existing Bakery, Mill/Windmill, GrainFieldPatch, DirtRoad, and other generated work must be audited before replacement. Reuse and promote an existing asset when it passes the same evidence and Unreal verification gates; do not regenerate merely for novelty. Each task must verify world scale, grid footprint, pivot, collision, material assignment, LOD/Nanite choice, and an in-game camera read. Production buildings should be authored at intended world scale; arbitrary runtime auto-fitting is not an acceptable final solution.

### EMVP-P07 — Establish the playable-slice 3D style anchors and scale contract

~~~text
Execute EMVP-P07 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to audit the strongest existing merchant-house, Bakery, Mill, road, and harbor assets and select or revise the minimum style anchors for the enhanced MVP. Establish a documented world-scale, doorway/floor-height, grid-footprint, pivot, texel-density, PBR, LOD/Nanite, collision, weathering, season, lighting, and camera-read contract shared by all later assets. Verify anchors in headless Blender and in the actual Lübeck map under gameplay lighting. Do not create a conflicting art direction. Record which existing assets are production-ready, which require revision, and which are rejected. Add presentation validation for scale and canonical paths where feasible. Done when later HansaModels tasks have evidence-backed anchors and no longer rely on runtime mesh squeezing to fit footprints.
~~~

### EMVP-P08 — Grain Farm and cultivated-field set

~~~text
Execute EMVP-P08 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create or revise the production Grain Farm asset family: the farm building, cultivated grain field modules, field-edge transitions, and only the small agricultural props needed for its operating/readable states. Ground the design in northern German late-medieval evidence and the approved Hansa anchors. Make the silhouette and fields legible from the city-builder camera without exaggerating into fantasy. Verify footprints, modular joins, seasonal consistency, collision, LOD/Nanite, hybrid PBR materials, and construction/idle/working visual roles. Import and inspect all accepted assets in Unreal, connect them to the stable Grain Farm presentation definition, and test placement plus save/load. No engine cube or unverified staging reference may remain.
~~~

### EMVP-P09 — Mill production building

~~~text
Execute EMVP-P09 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to audit the existing Mill, TowerMill, and Windmill work and deliver one coherent production Mill presentation for the bread chain. Select the historically and spatially appropriate MVP variant, revise rather than duplicate when possible, and ensure the authored scale, footprint, pivot, materials, sails/blades, collision, and distance readability match the shared anchors. Provide only animation-ready separation that the existing runtime can use; do not invent a new animation system. Import, reopen, inspect, and connect the verified asset to the stable Mill definition. Validate placement, operational-state readability, output selection, and absence of auto-fit distortion.
~~~

### EMVP-P10 — Bakery production building

~~~text
Execute EMVP-P10 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to audit the existing Bakery variants and promote or revise one final production Bakery. Preserve the strongest historically credible form while improving the city-builder silhouette, oven/chimney cues, authored world scale, footprint fit, materials, collision, and LOD/Nanite behavior. Create only the separate flour sack, bread crate, oven smoke, or shop-sign props actually needed by the presentation-state manifest. Import and inspect the accepted assets in Unreal, assign stable presentation roles, and prove Grain Farm to Mill to Bakery can be read visually in the Lübeck scene. Remove golden-path fallback references without deleting unrelated source work.
~~~

### EMVP-P11 — Coastal Fishery asset family

~~~text
Execute EMVP-P11 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create the production Fishery asset family for a Baltic coastal Hanseatic city: shore-facing work building, small landing or working platform where required, nets/racks/barrels, fish cargo props, and a small non-route fishing craft only if it is visible in the approved presentation. Research Lübeck/Rostock-region analogues and keep the result original. Make shore-placement direction and water access visually obvious. Verify land/water pivot rules, footprint, collision, modular contact with quay/shore, material response near water, LOD/Nanite, and gameplay-camera silhouette. Import, inspect, connect to the Fishery definition, and test valid and invalid shore placement.
~~~

### EMVP-P12 — Lumber Camp asset family

~~~text
Execute EMVP-P12 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create the production Lumber Camp, log piles, cut timber, tool shelter, and the minimum forest-edge props needed to communicate input gathering. Match northern European medieval construction and the approved Hansa anchors. Keep trees and repeated props modular and performance-conscious; do not bake a whole forest into one opaque asset. Verify footprint, pivot, collisions, instancing suitability, PBR materials, LOD/Nanite, and read at gameplay zoom. Import and inspect the accepted set, connect stable presentation roles, and validate placement plus visible production state without engine primitives.
~~~

### EMVP-P13 — Sawmill asset family

~~~text
Execute EMVP-P13 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create the production Sawmill and only its required log-input, cut-plank-output, rack, and saw-work props. Research a credible late-medieval water-, wind-, or manual-power solution consistent with the selected site rules; document the choice and do not introduce a water-wheel requirement unless the gameplay placement contract supports it. Prioritize a readable transformation from logs to planks. Verify world scale, footprint, pivot, collision, material consistency, animation-ready separation if used, LOD/Nanite, and in-game silhouette. Import, inspect, connect to the Sawmill definition, and prove the Lumber Camp to Sawmill chain visually.
~~~

### EMVP-P14 — Laborer and Artisan residence family

~~~text
Execute EMVP-P14 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to audit the existing Laborer Residence and deliver a coordinated Laborer-to-Artisan residence family. The upgrade must read as increased prosperity and capacity while retaining the same city, period, parcel logic, and shared material language. Provide sufficient controlled variation to prevent obvious repetition without creating a combinatorial asset set. Verify footprint compatibility for in-place upgrade, pivots, door/street orientation, collision, LOD/Nanite, roofline variation, night/window treatment if used, and gameplay-camera readability. Import and inspect each accepted variant, connect it to stable residence-tier presentation roles, and prove the runtime upgrade swaps visuals correctly.
~~~

### EMVP-P15 — Market building and trading props

~~~text
Execute EMVP-P15 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create or revise the civic Market presentation: a recognizable market building or square treatment plus modular stalls, awnings, scales, baskets, and representative cargo props required by the manifest. Keep the layout compatible with native UI selection and city traffic; do not encode changing prices or text in textures. Establish a clear public-service silhouette distinct from production buildings. Verify footprint, pivots, collisions, reusable prop performance, PBR material consistency, LOD/Nanite, and camera read. Import and inspect the accepted assets, connect the Market definition and presentation roles, and validate its service/coverage feedback in the playable map.
~~~

### EMVP-P16 — Warehouse asset family

~~~text
Execute EMVP-P16 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create the production Warehouse, loading doors, hoist or crane where evidenced, stacked cargo modules, pallets/skids appropriate to the period, and only the props needed to show storage and transfer. The building must read as the logistics hub connecting roads, local production, and the harbor. Verify authored capacity scale without making the footprint misleading, street/harbor orientation, pivot, collision, LOD/Nanite, hybrid PBR materials, and repeated-cargo performance. Import, reopen, inspect, connect the stable Warehouse presentation, and validate road connectivity, selection, cargo state, and save/load in Lübeck.
~~~

### EMVP-P17 — Dock, quay, and harbor-working set

~~~text
Execute EMVP-P17 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create a modular production harbor set for Lübeck and Rostock: dock/landing, quay edges and corners, mooring elements, steps or ramps, compact crane/hoist if supported by evidence, and the required harbor cargo props. Match the waterline and shoreline contracts used by the levels. Ensure modular seams, pivots, collision, wet/dry material transitions, LOD/Nanite, and camera-distance readability are verified. Import and inspect the set in both city lighting contexts, connect the Dock definition and stable environment roles, and test ship berth alignment and shore-placement validation. Do not merge the complete harbor into one inflexible mesh.
~~~

### EMVP-P18 — Production road kit

Status 2026-09-08: completed and explicitly approved/promoted for the current MVP road contract. Six native-cell variants, live/final topology, wet/dry material, actual shore-grade blending, catalog v7 and release-boundary evidence are recorded in `Docs/Development/EnhancedMvpRoadKit.md`. Arbitrary Landscape conformance and unrelated world dressing remain outside this acceptance.

~~~text
Execute EMVP-P18 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to audit the existing DirtRoad work and deliver the production road kit required by the road-drawing tool: straight, corner, T-junction, crossroads, end/transition, and any shore or gate transition actually exercised by the MVP. Use modular geometry/material techniques that avoid visible seams and do not stretch painted detail. Verify cell dimensions, pivots, path orientation, grade tolerance, collision/navigation behavior, decals or material blending, LOD/Nanite choice, and wet-weather consistency. Import and inspect every accepted piece, connect deterministic neighbor-to-variant selection, and test live previews and final roads across all junction types.
~~~

### EMVP-P19 — Cog and local cargo vehicle family

Status 2026-09-08: completed and explicitly approved/promoted for the asset and read-only entity-presentation contract. The Cog/wagon family, stable bindings, corrected berth, catalog v8, 27 passing targeted tests and Shipping/cook evidence are recorded in `Docs/Development/EnhancedMvpVehicleFamily.md`. Continuous two-city movement and full world-projection lifecycle remain P32/P34; no animal/vehicle framework was added.

~~~text
Execute EMVP-P19 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create the MVP cargo Cog and the one local cart or wagon family that is actually projected into the world. Research credible Hanseatic hull, rig, cargo, wheel, harness, and loading details. Keep the cog readable at trade-route and city-harbor camera distances. Separate only the parts needed for supported movement, sail/wheel rotation, cargo-state variants, or simple rigging; do not expand into a full character/vehicle framework. Verify scale, pivots, collision, LOD/Nanite, materials, waterline, berth alignment, route orientation, and cargo sockets. Import and inspect both assets in Unreal, connect stable vehicle presentations, and verify they can represent real simulation entities rather than ambient decoration.
~~~

### EMVP-P20 — Shared city-life and environmental prop set

~~~text
Execute EMVP-P20 from Docs/MVP-enhanced.md. Use the HansaModels skill end to end to create only the remaining recurring props identified by EMVP-P02 that are necessary to make Lübeck and Rostock feel inhabited: selected barrels/crates/sacks, benches, wells, signs without baked dynamic text, carts at rest, fences, lamps or braziers if period-appropriate, shoreline debris, and controlled vegetation. Treat this as a manifest-driven set, not an invitation to make miscellaneous assets. Reuse materials and atlases where quality permits. Verify silhouette, scale, collision policy, pivot, instancing, LOD/Nanite, draw-call/material cost, and consistency against the style anchors. Import and inspect each accepted prop, add provenance records, and prove no golden-path scene still depends on Engine basic shapes.
~~~

## 8. Wave 3 — Professional Hansa GUI

The UI may use Anno as an interaction reference, but not as an asset, exact-layout, iconography, or branding source. Hansa's design brief remains the visual source of truth. “AAA level” here means a coherent component language, excellent hierarchy, complete states, polished motion and feedback, accessibility, responsive native-resolution composition, and no developer/test affordances in the player path.

GUI acceptance update (2026-09-09): the cross-screen repair and actual reference comparisons are recorded in [GuiRepairSession.md](Development/GuiRepairSession.md). P21–P25 implementation statuses describe functional delivery; they do not grant AAA visual or usability approval. Remaining reference-conformance, accessibility, performance and human UAT gates are explicit in that report.

### EMVP-P21 — Production UI design system and reusable component library

~~~text
Execute EMVP-P21 from Docs/MVP-enhanced.md. Read the UI brief and asset workflow, use the imagegen skill, and establish the final Hansa UI style anchor plus a named component inventory and state matrix before implementation. Cover screen shell, top bar, bottom tray, tabs, category buttons, building cards, chain connectors, panels, list/table rows, tooltips, modal dialogs, notifications, progress, charts/overlays, cursors, focus, icons, and decorative layers. Generate a composed reference for hierarchy and a separate native-size reference or asset for each distinct reusable visual component or variant; preserve prompt records and inspect originals. Then implement centralized native Slate/UMG tokens and reusable components for palette, typography, spacing, materials, nine-slice/tiled treatment, focus, disabled, warning, error, loading, and motion. Done when all later screens can be assembled without inventing local styles.
~~~

Implementation status (2026-09-08): P21 implemented. The shared native library,
component/state inventory, 21 selected ImageGen references and prompt records,
licensed fonts, accessibility/input tests and actual viewport evidence are documented
in [EnhancedMvpUiSystem.md](Development/EnhancedMvpUiSystem.md). This completes the
component foundation; P22–P29 and the broader EMVP-UI screen acceptance remain separate.

### EMVP-P22 — Anno-inspired bottom construction panel and chain expansion

~~~text
Execute EMVP-P22 from Docs/MVP-enhanced.md. Use the imagegen skill and the approved UI component system to design and implement the production bottom construction panel. It must provide clear category navigation, a compact default tray, and chain expansion above the tray: selecting the Bread icon reveals Grain Farm, Mill, and Bakery as separate draggable building cards connected as a readable chain; Fish reveals Fishery; Planks reveals Lumber Camp and Sawmill. Show cost, lock/availability, footprint, workforce, and input/output summaries through native UI. Implement default, hover, pressed, selected, disabled, focus, dragging, warning, and error states plus localization space. Connect card drag to EMVP-P04 and retain accessible non-drag controls. Remove test helper buttons from the player path. Validate at 1280x720 and 1920x1080 without raster resampling.
~~~

Implementation status (2026-09-08): P22 implemented. The persistent native category
tray, Bread/Fish/Planks chain cards, P04 drag and non-drag controls, accessibility
states, five ImageGen references and 16 native viewport captures are documented in
[EnhancedMvpConstructionPanel.md](Development/EnhancedMvpConstructionPanel.md).
Rendered build-menu, shared-style, HUD and native input/layout checks passed at the
two required resolutions. P23 and broader screen acceptance remain separate.

### EMVP-P23 — Root HUD, time controls, alerts, and contextual inspector

~~~text
Execute EMVP-P23 from Docs/MVP-enhanced.md. Use the imagegen skill only for unresolved component visuals, then assemble the root HUD from the approved native component library. Polish the top resource/status bar, city breadcrumb, time/speed controls, alert stack, selection layer, notification layer, bottom construction host, and right contextual inspector while keeping the 3D city dominant. The inspector must explain identity, state, result, inputs/outputs or needs, current problem and cause, actions, and recent history. All numbers and text remain dynamic native widgets. Complete mouse, keyboard, and controller focus behavior, color-redundant status, tooltips, loading/error/empty states, and reduced-motion behavior. Add semantic automation and true viewport captures at both reference resolutions.
~~~

Implementation status (2026-09-09): P23 implemented. The native status and time
controls, stable alert actions and snooze recovery, notification placement, and
contextual inspector now share the P21 component system and preserve P22 construction.
Mouse, keyboard and controller automation, 26 regression tests, and 26 native viewport
captures at both reference resolutions passed. Component inventory, evidence and test
limits are documented in
[EnhancedMvpHudInspector.md](Development/EnhancedMvpHudInspector.md).

### EMVP-P24 — City Overview and population progression screen

~~~text
Execute EMVP-P24 from Docs/MVP-enhanced.md. Use the imagegen skill and shared component library to deliver the production City Overview for Lübeck and the inspectable Rostock summary. Implement Population, Production, and Market tabs with clear population trend, tier counts, satisfaction, workforce, employment, needs fulfillment, growth/decline causes, staple reserves, bottlenecks, and causal navigation to affected buildings or goods. Rostock must clearly communicate which data is known, stale, estimated, or unavailable and must not expose construction controls. Preserve selection across refresh, support long localization labels and native scrolling, and include default/loading/empty/warning/error/focus states. Validate semantic navigation and native screenshots at both reference resolutions.
~~~

Implementation status (2026-09-09): P24 implemented. The native Population,
Production and Market summary tabs expose complete needs, tier/employment context,
causal navigation and report-aware Rostock data. Selection survives refresh, the
modal blocks construction input, and shared accessibility/state controls are wired.
Six current City Overview regression tests and 26 native captures at 1280×720 and 1920×1080
passed. Three ImageGen references, component inventory and verification limits are
recorded in [EnhancedMvpCityOverview.md](Development/EnhancedMvpCityOverview.md).
Follow-up verification also fixes critical-state text contrast, shared Retry targets/focus,
and player-facing recovery copy; native Enter-to-retry recovery passes.
The full Market redesign is recorded under P25 below.

### EMVP-P25 — Full thirteen-good Market screen

~~~text
Execute EMVP-P25 from Docs/MVP-enhanced.md. Use the imagegen skill and shared UI system to turn the existing market table into the signature production screen for all thirteen MVP goods. Implement search, sorting, filters, stable selection, stock, reserve, production, consumption, citizen and industrial demand, price, recent trend, incoming cargo, report age, and status. The selected-good panel must explain causes, producers, consumers, shortages, surplus, and route opportunities from authoritative projections and provide relevant actions. Use native charts and text; never bake data into raster art. Treat unknown as unknown rather than zero. Add responsive table behavior, semantic data summaries, focus, accessibility, loading/error states, and deterministic market-to-world navigation tests.
~~~

Implementation status (2026-09-09): P25 implemented; it is now extended to a native thirteen-good ledger. The ledger
uses proportional columns, wrapped report details, shared accessible controls and
stable selection. Selected-good details include authoritative production,
consumption, causal balance, native price history and stable producer/consumer
building navigation. Unknown values remain unknown and sort after reported values.
Eight current market/selected-good/performance regressions and native viewport flows at 1280×720 and 1920×1080
passed. Three ImageGen references and 20 native captures with semantic evidence are
recorded in [EnhancedMvpMarket.md](Development/EnhancedMvpMarket.md).
Follow-up verification fixes repeated native activation of unchanged selections/filters
and uses actual building names in producer/consumer links while preserving stable world IDs.

### EMVP-P26 — Trade map and complete Lübeck–Rostock route creator

~~~text
Execute EMVP-P26 from Docs/MVP-enhanced.md. Use the imagegen skill and shared UI components to deliver a polished Baltic trade map focused on Lübeck and Rostock, while preserving other market-only cities only if they remain in the approved scope. Implement a complete route-creation flow rather than only editing existing routes: choose or begin from a city/market good, add ordered stops, choose the Cog, configure load/unload and minimum reserves, review capacity, round-trip time, upkeep, expected profit range and risk, validate, name, and activate. Render dynamic routes, city labels, cargo, reports, and values natively. Provide controller/non-drag alternatives, stale-information treatment, error recovery, semantic IDs, and tests that create a delivering route without debug commands.
~~~

**Status (2026-09-09): implemented.** Native Baltic route creation now covers market-good entry, ordered stops, Cog selection/reassignment, cargo/reserves, name, validated departure review and activation. Typed atomic commands create a new delivering route; names survive save/load through format-3 cosmetic metadata with prior-format migrations. 21 tests, 12 real-viewport delivery runs and 108 native captures across four resolutions / three UI scales passed. Four ImageGen references and comparisons, final source/build evidence and scoped Shipping exclusion results are recorded in [EnhancedMvpTradeCreator.md](Development/EnhancedMvpTradeCreator.md).

### EMVP-P27 — Research screen integration

~~~text
Execute EMVP-P27 from Docs/MVP-enhanced.md. Use the imagegen skill only where the approved system lacks a research-specific visual component. Finish the existing research screen and wire it into the root HUD and normal navigation. Present the three MVP branches, prerequisites, costs, progress, locked reasons, queue, and applied gameplay effects with a legible hierarchy at city-builder resolutions. Reuse stable research definitions and authoritative effects; do not recompute rules in widgets. Complete hover/selected/focus/locked/loading/error states, controller navigation, causal links to affected buildings or routes, semantic coverage, and save/load tests for progress and unlocks.
~~~

**Status (2026-09-09): implemented.** Research now presents the three MVP branches in prerequisite order, explicit costs/locked reasons, persistent queue/progress, authoritative applied effects, and normal market/building/route causal navigation. Shared executor eligibility, native disabled/focus states, semantic bounds and progress/unlock save-load tests are complete. 16 automated tests, 12 real-viewport workflows and 120 original-size captures across four resolutions / three UI scales passed. Existing approved ImageGen components were reused without a redundant generation pass. Details, original-size comparisons and scoped Shipping verification are recorded in [EnhancedMvpResearch.md](Development/EnhancedMvpResearch.md).

### EMVP-P28 — Scenario, onboarding, pause, and results presentation

~~~text
Execute EMVP-P28 from Docs/MVP-enhanced.md. Use the imagegen skill and shared component library to create the player-facing shell around the session: a concise opening context, optional contextual onboarding, objective/prosperity tracking, pause menu, failure/recovery messaging, and success/results presentation. The onboarding must teach camera, construction card drag, roads, inspection, market diagnosis, and route creation through contextual prompts that can be dismissed and do not prescribe one economic solution. Avoid a checklist that makes the game feel like an automation scenario. Implement complete focus, controller, accessibility, reduced-motion, localization, save/resume, and semantic states. Done when a first-time player can begin and recover without developer instructions.
~~~

**Status (2026-09-09): implemented.** The native session shell now provides concise opening context, six optional contextual help topics with persistent dismissal, pause/resume, authoritative prosperity/results and confirmed save recovery. Initial focus, controller navigation/scrolling, modal input isolation, shared accessibility preferences and semantic states are integrated. Three built-in ImageGen references and their exact prompt records are preserved. Development and Shipping exclusion checks, 60 UI tests, 9 save integration tests and 12 native display-profile flows passed; 168 original-size captures are preserved. See [EnhancedMvpSession.md](Development/EnhancedMvpSession.md) for component inventory, references, comparisons and test limitations.

### EMVP-P29 — Frontend, new game, save/load, settings, and credits shell

~~~text
Execute EMVP-P29 from Docs/MVP-enhanced.md. Use the imagegen skill and established component library to deliver the minimal production frontend and system screens required for a playable build: title/start screen, New Game for the enhanced slice, Continue when valid, save/load management, settings, confirmation dialogs, loading transitions, and a compact credits/legal placeholder where required. Settings must cover the existing supported display, audio, control, UI scale, contrast, text-size, and motion options without advertising unsupported features. Remove development fixtures and automation scenarios from the normal player journey. Add focus restoration, keyboard/controller support, corrupted/incompatible-save handling, and native-resolution screenshot tests.
~~~

**Status (2026-09-09): implemented.** The native frontend now provides title/New game, compatible-save Continue, named save/load management, real system/interface settings, confirmed destructive actions, loading/recovery feedback and the requested credits/legal placeholder. New game resets the production runtime; Continue and Load restore paused sessions. Display changes have cancellation and 15-second rollback. Five built-in ImageGen references and exact prompt records are preserved. Development/Shipping exclusion, 61 UI tests, 10 save integration tests, native session/display regressions and all 12 frontend profiles passed; 300 original-size captures are validated. See [EnhancedMvpFrontend.md](Development/EnhancedMvpFrontend.md) for the component inventory, original-size comparisons, evidence and release limitations.

## 9. Wave 4 — Assemble the playable two-city slice

### EMVP-P30 — Lübeck world-art and readability pass

**Status (2026-09-09): in progress; production acceptance blocked.** A separate native World Partition/Landscape/Water candidate, instanced approved quay dressing, lighting presets, reproducible authoring, and actual player-viewport construction/save comparison are implemented. The production map is unchanged. Missing P11/P16/P20 asset deliveries, the starting Brewery cube, primitive status markers, final art review and complete GPU/package evidence prevent the required zero-placeholder production assembly. See [EnhancedMvpLubeckWorldArt.md](Development/EnhancedMvpLubeckWorldArt.md).

~~~text
Execute EMVP-P30 from Docs/MVP-enhanced.md. Assemble the verified production assets in L_Lubeck_MVP and complete a restrained world-art pass without changing authoritative gameplay state. Replace golden-path blockout geometry and staging references, establish coherent terrain, shoreline, harbor, roads, vegetation, lighting, weather/atmosphere, water, camera boundaries, and controlled ambient dressing. Preserve buildable-space readability and do not decorate valid plots into visual ambiguity. Use stable presentation roles for spawned gameplay objects. Profile instancing, materials, shadows, LOD/Nanite, overdraw, and actor counts while iterating. Capture true player-camera evidence at representative zooms and times. Done when Lübeck reads as an intentional Hanseatic city before and after player construction and no visible Engine cube remains.
~~~

### EMVP-P31 — Build the rendered Rostock trade city

**Status (2026-09-09): playable staged implementation; production acceptance open.** Rostock now has a native streamed trade-quarter candidate, report/route-stop visits, inspection, construction guards, direct return, and bounded real-Cog approach/berth/unload projection. 73 focused tests, five native flows and 45 original-size captures pass, alongside current-production Shipping/cook exclusion checks. Final art review, target-platform performance and explicit content promotion remain open; the production map is not redirected to staging. See [EnhancedMvpRostock.md](Development/EnhancedMvpRostock.md).

~~~text
Execute EMVP-P31 from Docs/MVP-enhanced.md. Create the second coastal city experience as a fully rendered, prebuilt, inspectable, tradeable Rostock map or streamed city presentation using the same world and presentation contracts as Lübeck. Ground the city layout and harbor identity in credible late-medieval Rostock evidence while reusing the shared Hansa asset kit intelligently. Include a recognizable waterfront, market/warehouse/dock context, residences and production silhouettes sufficient to explain its economy, but do not expose player construction there. Connect the existing Rostock market state, report age, camera transition, selection/inspection, route stops, and cargo arrival to the rendered city. Add validation, semantic travel/inspection flow, performance checks, and native viewport evidence. Done when Rostock is visibly a place, not a row in a market table.
~~~

### EMVP-P32 — Project real vehicles, cargo, and production state into the world

**Status (2026-09-09): runtime implementation and technical acceptance complete; release-art acceptance remains open.** A bounded world manager now projects the real Cog and local delivery jobs with deterministic movement, cargo cues, generation-aware selection, streaming visibility, cancellation and save reconstruction. Production roles follow actual inventory/work state, and pause preserves visual phase. Four P32 contracts, 61 UI tests, two runtime-host tests, ten save tests, five native cargo profiles (40 captures), the existing Rostock native regression, Shipping exclusion and the current production cook audit pass. Missing P11 Fishery art and P30/P31 world-art/promotion gates remain explicit. See [EnhancedMvpCargoProjection.md](Development/EnhancedMvpCargoProjection.md) for contracts, component/reference inventory, evidence and limits.

~~~text
Execute EMVP-P32 from Docs/MVP-enhanced.md. Implement presentation actors/components for the real simulated Cog and local cart/wagon entities using the verified EMVP-P19 assets. Bind movement, route progress, berth/loading/unloading, cargo-state cues, and selection to read-only deterministic projections; do not make actor transforms or animations authoritative. Add simple production-state presentation for the three local chains using verified role assets and effects without spawning unbounded cosmetic actors. Handle streaming, save/load reconstruction, time speed changes, route cancellation, and missing-presentation failures. Add typed queries and semantic selection, then prove that cargo seen departing, traveling, arriving, and unloading corresponds to the same simulated route and inventory events.
~~~

### EMVP-P33 — Complete the full market, needs, and population-growth play loop

**Status:** Runtime corrections and a tested catalog v9 candidate are implemented; production balance promotion awaits review of the exact eight-definition diff. See [implementation and evidence](Development/EnhancedMvpEconomy.md). Catalog v8 remains the default.

~~~text
Execute EMVP-P33 from Docs/MVP-enhanced.md. Audit and tune the existing thirteen-good economy so Bread, Fish, Planks, and Beer require locally buildable chains while remaining goods have explicit starting stock, remote supply, imports, consumption, or other bounded MVP sources. Ensure population needs drive satisfaction, health or approved wellbeing measures, workforce, growth, decline, tier eligibility, and demand; ensure prices and inventories respond to actual supply, consumption, reserves, and deliveries. Prevent unwinnable depletion, infinite stock, zero-information masquerading as zero, and dominant no-brainer strategies. Update definitions through the approved version/migration path and maintain editor/schema/test parity. Add deterministic playthrough simulations for recovery, neglect, growth, upgrade, shortage, surplus, and trade dependence.
~~~

### EMVP-P34 — Complete two-city trade gameplay and visible delivery

**Implemented (2026-09-09):** Market shortages now seed return-leg imports; circular cargo validation, protected-reserve feedback, actionable edit recovery, safe UI cancellation, and repeated native city/tab activation are verified. The deterministic two-city fixture and normal-control viewport UAT cover real loading/delivery, both city inventories/prices, household need fulfillment, failed setup recovery, stale reports, and save/replay. 63 UI tests and a 72-capture display matrix pass, with synchronized state/domain-event evidence and Shipping exclusion. See [implementation and verification](Development/EnhancedMvpTradeJourney.md). Economy remains production v8; existing P31 visual and P33 balance promotion gates remain unchanged.


~~~text
Execute EMVP-P34 from Docs/MVP-enhanced.md. Join the route creator, Lübeck and Rostock markets, Cog simulation, visible world projection, docks, warehouses, and market feedback into one complete player journey. The player must be able to identify a price/supply opportunity, create the Lübeck–Rostock sea route through normal UI, protect a minimum reserve, load real stock, watch the Cog depart and arrive, observe unloading, and see both city inventories, needs, and prices respond. Support route pause/edit/cancel, insufficient cargo, capacity, stale reports, and recovery from a poor setup. Add a deterministic fixture and semantic UAT that uses ordinary UI actions, observable waits, typed queries, real viewport captures, and synchronized event/state evidence.
~~~

### EMVP-P35 — Add tactile feedback, effects, and audio to the golden path

~~~text
Execute EMVP-P35 from Docs/MVP-enhanced.md. Audit the golden player journey and add the bounded feedback layer required for a polished slice: construction confirm/reject, card pickup/drop, road drawing, building completion, production active/blocked, need shortage, population growth/decline, market movement, route creation, ship departure/arrival, research completion, alerts, and UI transitions. Use existing approved audio and effect pipelines and stable presentation roles; do not make live provider calls. Keep cues readable but restrained, support audio volume categories, captions or text equivalents where information-bearing, reduced motion, and time-speed behavior. Add presentation tests where deterministic and conduct an in-game mix/readability inspection at both reference resolutions.
~~~

### EMVP-P36 — Balance the non-prescriptive 30–60 minute session

~~~text
Execute EMVP-P36 from Docs/MVP-enhanced.md. Turn the enhanced slice into a replayable session rather than a single scripted scenario. Define a stable starting state with meaningful but recoverable pressure, enough space and capital for different openings, at least three viable strategic emphases across local production, population growth, and trade, and clear prosperity milestones. Keep explicit tutorial objectives optional and avoid requiring one exact build order. Run deterministic accelerated simulations plus several real user-flow playthroughs to tune construction costs/times, production rates, needs, prices, route times/capacity, research pace, alerts, and victory/prosperity thresholds. Record balance changes and causal evidence. Done when the intended session finishes in roughly 30–60 minutes, poor choices can be recovered from, and waiting is not the dominant activity.
~~~

## 10. Wave 5 — Prove it is a playable MVP

### EMVP-P37 — Real-player UAT and priority polish loop

~~~text
Execute EMVP-P37 from Docs/MVP-enhanced.md. Run an evidence-led UAT and polish loop over the complete enhanced golden journey: frontend to Lübeck, camera and selection, bread/fish/planks construction, card drag, invalid placement, roads, production diagnosis, population growth and residence upgrade, full market use, Rostock inspection, route creation, visible Cog delivery, research, save/load, and session completion. Test mouse/keyboard and the controller golden path at native 1280x720 and 1920x1080. Capture real viewport video/screenshots, semantic trees, logs, projections, and user-visible defects. Prioritize and implement verified blockers and high-impact polish defects without broadening scope. Re-run the journey after each bounded fix and leave an explicit residual-defect list.
~~~

### EMVP-P38 — Performance, accessibility, determinism, and resilience hardening

~~~text
Execute EMVP-P38 from Docs/MVP-enhanced.md. Profile and harden the assembled slice against the repository budgets. Measure game/render thread time, memory, streaming, shader/material count, actor count, instancing, LOD/Nanite transitions, water/foliage cost, UI invalidation and list updates, save size/time, and long deterministic simulation. Validate focus order, controller access, contrast, color redundancy, UI scale, text expansion, high contrast, reduced motion, audio settings, error recovery, and all loading/empty/error states. Exercise route cancellation, missing cargo, blocked roads, failed production, population decline, incompatible saves, and map transitions. Fix measured MVP blockers, add regression coverage, and document deferred non-blockers with evidence.
~~~

### EMVP-P39 — Clean-checkout enhanced-MVP release gate

~~~text
Execute EMVP-P39 from Docs/MVP-enhanced.md. From a clean-checkout-equivalent state, build and package the enhanced playable MVP and run the complete automated and manual acceptance matrix. Prove the catalog/version blocker is resolved; all golden-path definitions have production presentations; no Engine basic-shape fallback, staging/developer reference, generated full-screen UI, provider credential, editor module, automation module, worker, or test fixture enters Shipping; and all required maps and assets cook. Run the real frontend-to-completion golden flow, save/load, deterministic replay, two-city trade delivery, UI accessibility/resolution suite, and performance checks. Replace any synthetic screenshot assertion with synchronized real viewport evidence. Produce a final acceptance report mapping every requirement in Docs/MVP.md and Docs/MVP-enhanced.md to exact build, test, screenshot/video, query, and package-audit evidence. Any unresolved required item blocks release.
~~~

## 11. Final enhanced-MVP acceptance gate

The enhanced MVP is playable only when all of the following are true:

- A player launches a packaged build, starts the slice through a production frontend, and receives no developer instructions.
- Lübeck is fully buildable and Rostock is visibly rendered, inspectable, and tradeable.
- Bread, fish, planks, and beer can be constructed and operated through their complete local chains.
- Building cards drag into the world with authored 3D ghosts, valid/invalid feedback, rotation, cancellation, and accessible alternatives.
- Roads are drawn directly in the world and visibly connect the logistics network.
- All thirteen goods participate in the active market and affect population or production through explicit rules.
- Needs, satisfaction, workforce, growth, decline, and residence upgrade are understandable and responsive.
- A player creates a Lübeck–Rostock route and sees the simulated Cog carry real cargo whose delivery changes inventories and prices.
- No building, vehicle, road, or recurring golden-path prop appears as an Engine cube or an unverified staging asset.
- All required screens share the approved Hansa component system and pass mouse/keyboard, controller, accessibility, localization-expansion, and native-resolution checks.
- The player has meaningful economic choices and recovery paths during a 30–60 minute session.
- Save/load restores the visible and authoritative state.
- Real viewport evidence, typed projections, events, logs, and deterministic hashes agree.
- The clean Shipping package passes content, module, provider, secret, staging, performance, and release-boundary audits.

Only after this gate passes should the implementation be described as a playable MVP rather than a simulated scenario with a UI.
